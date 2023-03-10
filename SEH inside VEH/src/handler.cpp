/*
    SEH inside VEH - Implements SEH, bypassing SafeSEH, inside VEH
    Copyright (C) 2023 Nick Daniel / https://github.com/Nick-Source

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "handler.h"
#include "exception_registration.h"

namespace SEH
{
    namespace Handler
    {
    #if EXCEPTION_CHECKING == VALID_TOP_HANDLER_CHECK
        /*
            This is not an emulation of RtlIsValidHandler. There is no DEP enforcement
            here. It can only accurately identify when a valid SafeSEH handler can be
            passed to the real SEH. When the handler is in a SafeSEH table AND can pass
            the RtlIsValidHandler test, we pass it off to real SEH. Otherwise, we dispatch
            the exception.
            
            Valid handlers = Real SEH
            Non-valid handlers = Custom SEH

            Pseudocode of RtlIsValidHandler
            https://repo.zenk-security.com/Techniques%20d.attaques%20%20.%20%20Failles/EN-Bypassing%20SEHOP.pdf#page=4
        */
        bool isTopHandlerValid()
        {
            PEXCEPTION_ROUTINE Handler = Registration::getRegistrationHead()->Handler;

            HMODULE module = NULL;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)Handler, &module);

            if (module == NULL)
            {
                return false; //Not in a registered module, our custom SEH allows handlers from anywhere
            }

            //Traverse PE structure to find the SafeSEH table
            IMAGE_DOS_HEADER* DosHeader = (IMAGE_DOS_HEADER*)module;
            IMAGE_NT_HEADERS* NTHeaders = (IMAGE_NT_HEADERS*)((DWORD)DosHeader + DosHeader->e_lfanew);
            IMAGE_DATA_DIRECTORY* ImageDirectory_LoadConfig = &NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
            IMAGE_LOAD_CONFIG_DIRECTORY* LoadConfigDirectory = (IMAGE_LOAD_CONFIG_DIRECTORY*)((DWORD)DosHeader + ImageDirectory_LoadConfig->VirtualAddress);

            if (NTHeaders->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NO_SEH)
            {
                EXCEPTION_RECORD Exception = {};

                Exception.ExceptionCode = STATUS_INVALID_EXCEPTION_HANDLER;
                Exception.ExceptionFlags = EXCEPTION_NONCONTINUABLE;

                RtlRaiseException(&Exception); //Why are we attempting SEH on a non-SEH image?
            }

            if (ImageDirectory_LoadConfig->VirtualAddress == 0)
            {
                return false; //SafeSEH isn't possible without IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG
            }

            //SafeSEH info
            DWORD SEHandlerCount = LoadConfigDirectory->SEHandlerCount; //Amount of safe handlers
            DWORD* SEHandlerTable = (DWORD*)LoadConfigDirectory->SEHandlerTable; //A sorted table of the RVAs of safe handlers
            DWORD HandlerRVA = (DWORD)Handler - (DWORD)DosHeader; //Get RVA of handler (relative virtual addresses)

            if (SEHandlerTable != NULL && SEHandlerCount != 0)
            {
                /*
                    Binary search to find the handler in SafeSEH table
                */

                DWORD lowerBound = 0;
                DWORD upperBound = SEHandlerCount - 1;

                while (lowerBound <= upperBound && upperBound != (DWORD)-1)
                {
                    DWORD middle = lowerBound + ((upperBound - lowerBound) / 2);

                    if (SEHandlerTable[middle] < HandlerRVA)
                    {
                        /*
                            Won't ever overflow to 0 because if lowerBound ever reaches MAXDWORD,
                            the loop will end from the condition lowerBound <= upperBound.
                        */
                        lowerBound = middle + 1;
                    }
                    else if (SEHandlerTable[middle] > HandlerRVA)
                    {
                        /*
                            UpperBound could overflow to MAXDWORD so a check for that is used in
                            the while conditions.
                        */
                        upperBound = middle - 1;
                    }
                    else
                        return true; //Found the handler in the SafeSEH table
                }
            }

            return false; //Handler is not in SafeSEH table or the table doesn't exist for the handler's module, we will dispatch the exception
        }
    #endif

        /*
            Used to catch exceptions inside other SEH handlers

            This handler could be registered as SafeSEH but that doesn't hold much value as 
            it is expected the image this exists in doesn't have valid SafeSEH handlers 
            anyways. Otherwise, there is no use to this entire library.
        */
        template <bool unwind, EXCEPTION_DISPOSITION disposition>
        EXCEPTION_DISPOSITION NTAPI _Function_class_(EXCEPTION_ROUTINE) NestedExceptionHandler(EXCEPTION_RECORD* ExceptionRecord, PEXCEPTION_REGISTRATION_RECORD EstablisherFrame, PCONTEXT ContextRecord, PEXCEPTION_REGISTRATION_RECORD& DispatcherContext)
        {
            if ((bool)(ExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) == unwind)
            {
                /*
                    Inside ExecuteHandler, the original EstablisherFrame was pushed right before the registration record for
                    NestedExceptionHandler was created; therefore, ExecuteHandler's EstablisherFrame (the throwing frame) is
                    a EXCEPTION_REGISTRATION_RECORD, the size of itself, ahead. Remember, NestedExceptionHandler's EstablisherFrame
                    (the parameter for this function) is a pointer to a location of the stack inside ExecuteHandler (mov dword ptr fs:[0], esp),
                    so this offset is relative to that. This is because the regisration inside DispatchException will be at NestedExceptionHandler.
                */

                DispatcherContext = *reinterpret_cast<PEXCEPTION_REGISTRATION_RECORD*>((DWORD)EstablisherFrame + sizeof(EXCEPTION_REGISTRATION_RECORD));
                return disposition;
            }
            else
            {
                /*
                    Make the opposing function blind to this function. The opposing types are
                    DispatchException and Unwind. Remember, this function gets added to the SEH
                    frame list and serves different purposes for DispatchException and Unwind.
                    It is important to know what function it will be useful for and only respond
                    to that one.
                */

                return ExceptionContinueSearch;
            }
        }

        template EXCEPTION_DISPOSITION NestedExceptionHandler<true>(EXCEPTION_RECORD*, PEXCEPTION_REGISTRATION_RECORD, PCONTEXT, PEXCEPTION_REGISTRATION_RECORD&);
        template EXCEPTION_DISPOSITION NestedExceptionHandler<false>(EXCEPTION_RECORD*, PEXCEPTION_REGISTRATION_RECORD, PCONTEXT, PEXCEPTION_REGISTRATION_RECORD&);

        /*
            Should be made in assembly because EstablisherFrame is saved
            on the stack in relation to a EXCEPTION_REGISTRATION_RECORD.
            This makes NestedExceptionHandler able to identify the throwing
            handler.
        */
        __declspec(naked) EXCEPTION_DISPOSITION __cdecl ExecuteHandler(PEXCEPTION_RECORD ExceptionRecord, PEXCEPTION_REGISTRATION_RECORD EstablisherFrame, PCONTEXT ContextRecord, PEXCEPTION_REGISTRATION_RECORD& DispatcherContext, PEXCEPTION_ROUTINE Handler, PNESTED_EXCEPTION_HANDLER NestedExceptionHandler)
        {
            __asm
            {
                //Prologue
                push ebp
                mov ebp, esp

                //Save EstablisherFrame in case Handler throws
                push EstablisherFrame

                //Add NestedExceptionHandler to the linked list in case Handler throws
                push NestedExceptionHandler     //Handler
                push dword ptr fs:[0]           //Next
                mov dword ptr fs:[0], esp

                //Execute Handler
                push DispatcherContext
                push ContextRecord
                push EstablisherFrame
                push ExceptionRecord
                call Handler //__stdcall if supported otherwise __cdecl

                //Remove top exception handler from the linked list
                mov esp, dword ptr fs:[0]
                pop dword ptr fs:[0]

                //Epilogue and Cleanup
                mov esp, ebp
                pop ebp
                ret
            }
        }
    }
}