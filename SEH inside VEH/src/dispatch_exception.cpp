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
#include "bound_check.h"
#include "dispatch_exception.h"
#include "exception_registration.h"

namespace SEH
{
#pragma warning( push )
#pragma warning( disable : 4715 ) //Not all control paths return a value

    //Iterate through SEH handlers
    LONG NTAPI DispatchException(EXCEPTION_POINTERS* ExceptionInfo)
    {
        CONTEXT* Context = ExceptionInfo->ContextRecord;
        EXCEPTION_RECORD* Exception = ExceptionInfo->ExceptionRecord;
        PEXCEPTION_REGISTRATION_RECORD DispatcherContext = NULL, NestedFrame = NULL;

    #if EXCEPTION_CHECKING == VALID_TOP_HANDLER_CHECK
        if (Handler::isTopHandlerValid())
        {
            /*
                Check if the first handler is in the SafeSEH table. If so, let
                original SEH deal with this exception.

                ASSUMPTION: Top handler handles exceptions
            */

            return EXCEPTION_CONTINUE_SEARCH;
        }
    #elif EXCEPTION_CHECKING == BOUND_CHECK
        if (!Bound_Check::exceptionInBounds(ExceptionInfo))
        {
            /*
                Check if the exception originated in our module. If so, we have
                responsibility to deal with it.

                ASSUMPTION: Exceptions shouldn't be dealt with across modules;
                however, they can be in specific scenarios.
            */

            return EXCEPTION_CONTINUE_SEARCH;
        }
    #endif

        //Stack limits
        DWORD stackLow;
        DWORD stackHigh;
        GetCurrentThreadStackLimits(&stackLow, &stackHigh);

        for (EXCEPTION_REGISTRATION_RECORD* Registration = Registration::getRegistrationHead(); Registration != EXCEPTION_CHAIN_END; Registration = Registration->Next)
        {
            if ((DWORD)Registration < stackLow || ((DWORD)Registration + sizeof(EXCEPTION_REGISTRATION_RECORD)) > stackHigh || ((DWORD)Registration & 0x3) != 0)
            {
                /*
                    Frame outside of stack limits or unaligned on stack

                    0x1 in binary is  01
                    0x2 in binary is  10
                    0x3 in binary is  11
                    0x4 in binary is 100

                    You can see how the bitwise AND is used to identify a 4 byte alignment.
                    
                    I think a flag is used instead of a new exception here because the exception can't be 
                    handled due to the bad frame. This way the exception at the end will appear as an unhandled 
                    exception. Personally, I think a new exception would be less ambiguous. A new exception could 
                    simply be passed to NtRaiseException with FirstChance/HandleException to FALSE.
                */

                Exception->ExceptionFlags |= EXCEPTION_STACK_INVALID;
                goto error; //Can't RtlRaiseException otherwise we'd end up in an infinite loop
            }

            EXCEPTION_DISPOSITION Disposition = Handler::ExecuteHandler(Exception, Registration, Context, DispatcherContext, Registration->Handler, &Handler::NestedExceptionHandler<false>);

            if (Registration == NestedFrame)
            {
                /*
                    Currently in the throwing frame. We passed EXCEPTION_NESTED_CALL to
                    it so now the flag can be removed and NestedFrame reset.
                */
                Exception->ExceptionFlags &= ~EXCEPTION_NESTED_CALL;
                NestedFrame = NULL;
            }

            switch (Disposition)
            {
            case ExceptionContinueExecution:

                if (Exception->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
                {
                    EXCEPTION_RECORD NewException = {};
                    NewException.ExceptionCode = STATUS_NONCONTINUABLE_EXCEPTION;
                    NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                    NewException.ExceptionRecord = Exception;

                    RtlRaiseException(&NewException);
                }
                else
                    return EXCEPTION_CONTINUE_EXECUTION;

                break;

            case ExceptionContinueSearch:
                
                if (Exception->ExceptionFlags & EXCEPTION_STACK_INVALID)
                {
                    goto error;
                }
                
                break;

            case ExceptionNestedException:
                //Assign EXCEPTION_NESTED_CALL to flags for all upcoming frames
                Exception->ExceptionFlags |= EXCEPTION_NESTED_CALL;

                /*
                    Is DispatcherContext (the EstablisherFrame of the nested exception) > NestedFrame?

                    NOTE: EstablisherFrame is a pointer to a EXCEPTION_REGISTRATION_RECORD

                    This identifies the oldest throwing frame; greater value means older in stack. 
                    Getting the oldest throwing frame allows EXCEPTION_NESTED_CALL to be enabled 
                    up to the last throwing frame.

                    REMEMBER: The Nested Exception Handlers aren't removed off the SEH list
                    therefore it is necessary to find the oldest throwing frame. Otherwise,
                    if a newer frame throws and then an old one throws later, the new frame's
                    nested handler would override the old frame's nested handler. Don't forget
                    the nested handlers get put at the top of the list. Overall, this requires
                    the frames to be on the stack in order for addresses to correspond to age.
                    There is a visual attached in the "SEH inside VEH" project folder.
                */
                if (DispatcherContext > NestedFrame)
                {
                    //Store the frame that threw to identify it later
                    NestedFrame = DispatcherContext;
                }

                break;

            default:
                EXCEPTION_RECORD NewException = {};
                NewException.ExceptionCode = STATUS_INVALID_DISPOSITION;
                NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                NewException.ExceptionRecord = Exception;

                RtlRaiseException(&NewException);
                break;
            }
        }

    error:
        //No appropriate handler found or bad conditions encountered
        NtRaiseException(Exception, Context, FALSE);
    }

#pragma warning( pop )
}