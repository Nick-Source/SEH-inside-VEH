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
#include "bound_check.h"

#if EXCEPTION_CHECKING == BOUND_CHECK

#pragma comment(lib, "Dbghelp.lib")

/*
    Bound checking makes some compromises because we can't be sure that 
    the stack trace of an exception will always be the same. Visual C++ 
    can change that anytime as well as Windows functions. This code makes
    the assumption that exceptions from throw will start from _CxxThrowException
    and then all RaiseException. There is another assumption that the stack
    trace retrieved from the CONTEXT inside DispatchException ends at RaiseException.

    This is all necessary because in C++ exceptions, the real exception doesn't
    happen where throw is called.

    NOTE: This is designed for inclusion of C++ exceptions. You may need to 
    modify it to work for your case. It does support normal exceptions though, 
    where the CONTEXT given to DispatchException is directly related to the 
    exception.

    WARNING: Enabling Frame-Pointer Omission may break this method

    FOR THIS TO WORK (C++ Exceptions):
        1. Throw must directly call _CxxThrowException
        2. _CxxThrowException should not be inlined
        3. _CxxThrowException must call RaiseException
        4. The CONTEXT in the custom SEH inside VEH, after a throw, must be relative to RaiseException

    FOR THIS TO WORK (Manual call to RaiseException):
        1. The CONTEXT in the custom SEH inside VEH, after a throw, must be relative to RaiseException

    FOR THIS TO WORK (Other):
        1. Nothing needed unless the exception does not occur directly in its specific module
        (Imagine throw or RaiseException, the real exception for those don't occur where they're called at)

    These requirements could technically be extended more but I just kept it to a 
    general guideline that works as of creating this.
*/

namespace SEH
{
    namespace Bound_Check
    {
        CRITICAL_SECTION stackTrace = {};

        static DWORD RaiseException = 0;
        static DWORD _CxxThrowException = 0;

        static std::vector<DWORD> captureStackTrace(CONTEXT Context)
        {
            STACKFRAME stackFrame = {};

            stackFrame.AddrPC.Offset = Context.Eip;
            stackFrame.AddrPC.Mode = AddrModeFlat;

            stackFrame.AddrFrame.Offset = Context.Ebp;
            stackFrame.AddrFrame.Mode = AddrModeFlat;

            stackFrame.AddrStack.Offset = Context.Esp;
            stackFrame.AddrStack.Mode = AddrModeFlat;

            std::vector<DWORD> stackTrace;

            EnterCriticalSection(&Bound_Check::stackTrace);

            while (StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &stackFrame, &Context, NULL, NULL, NULL, NULL))
            {
                if (stackFrame.AddrPC.Offset != 0)
                {
                    stackTrace.push_back(stackFrame.AddrPC.Offset);
                }
                else
                {
                    break;
                }
            }

            LeaveCriticalSection(&Bound_Check::stackTrace);

            return stackTrace;
        }

        static LONG NTAPI emulateThrow(EXCEPTION_POINTERS* ExceptionInfo)
        {
            CONTEXT* Context = ExceptionInfo->ContextRecord;
            std::vector<DWORD> stackTrace = captureStackTrace(*Context);

            if (stackTrace.size() >= 2)
            {
                RaiseException = stackTrace.at(0);
                _CxxThrowException = stackTrace.at(1);
            }
            else
            {
                EXCEPTION_RECORD Exception = {};

                Exception.ExceptionCode = 0xE0000028; //Arbitrary exception code indicating failed stack trace capture
                Exception.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                Exception.ExceptionAddress = (PVOID)Context->Eip;

                NtRaiseException(&Exception, Context, FALSE);
            }

            return EXCEPTION_CONTINUE_EXECUTION;
        }

    #pragma optimize( "", off )

        void captureThrowStackTrace() //Only necessary for C++ exception support
        {
            /*
                emulateThrow will handle the throw in this function and view the stack trace.
                In doing that, we are able to find the return addresses of these functions
                and compare to them in real exceptions. This is useful because we can't figure
                out what function we are in when analyzing the stack without symbols. So by
                analyzing this beforehand, we can identify what the stack will look similar to
                when a real throw exception occurs.
            */

            PVOID VEH = AddVectoredExceptionHandler(1, &emulateThrow);

            throw;

            RemoveVectoredExceptionHandler(VEH);
        }

    #pragma optimize( "", on )
    
    #pragma warning( push )
    #pragma warning( disable : 4715 ) //Not all control paths return a value

        bool exceptionInBounds(EXCEPTION_POINTERS* ExceptionInfo)
        {
            CONTEXT* Context = ExceptionInfo->ContextRecord;
            IMAGE_NT_HEADERS* NTHeaders = (IMAGE_NT_HEADERS*)((DWORD)&__ImageBase + __ImageBase.e_lfanew);
            std::vector<DWORD> stackTrace = captureStackTrace(*Context);

            for (unsigned int i = 0; i < stackTrace.size(); ++i)
            {
                if (stackTrace.at(i) == RaiseException) { ++i; }
                if (stackTrace.at(i) == _CxxThrowException) { ++i; } //_CxxThrowException calls RaiseException

                return ((stackTrace.at(i) > (DWORD)&__ImageBase) && (stackTrace.at(i) < ((DWORD)&__ImageBase + NTHeaders->OptionalHeader.SizeOfImage)));
            }

            EXCEPTION_RECORD NewException = {};

            NewException.ExceptionCode = 0xE0000029; //Arbitrary exception code indicating failed exception bound checking
            NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            NewException.ExceptionRecord = ExceptionInfo->ExceptionRecord;
            NewException.ExceptionAddress = (PVOID)Context->Eip;

            NtRaiseException(&NewException, Context, FALSE);
        }

    #pragma warning( pop )
    }
}

#endif