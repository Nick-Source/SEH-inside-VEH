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

#include "SEH.h"
#include "handler.h"
#include "bound_check.h"
#include "dispatch_exception.h"
#include "exception_registration.h"

namespace SEH
{
    static PVOID VEH = NULL;

    void EnableSEH()
    {
        if (!VEH)
        {
        #if EXCEPTION_CHECKING == BOUND_CHECK
            InitializeCriticalSection(&Bound_Check::stackTrace);
            Bound_Check::captureThrowStackTrace();
        #endif

            VEH = AddVectoredExceptionHandler(0, &DispatchException);
        }
    }

    void DisableSEH()
    {
        if (VEH)
        {
            RemoveVectoredExceptionHandler(VEH);
            VEH = NULL;

        #if EXCEPTION_CHECKING == BOUND_CHECK
            EnterCriticalSection(&Bound_Check::stackTrace);
            DeleteCriticalSection(&Bound_Check::stackTrace);
        #endif
        }
    }

    /*
        For some reason x86 RtlUnwind doesn't actually change the instruction pointer,
        so parameter "TargetIp" ends up being completely useless. This is weird because 
        the documentation specifically defines TargetIp as "The continuation address 
        of the unwind."

        https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlunwind
    */
    void NTAPI Unwind(PVOID TargetFrame, PVOID TargetIp, EXCEPTION_RECORD* pException, PVOID ReturnValue)
    {
        CONTEXT Context = {};
        EXCEPTION_RECORD Exception = {};
        PEXCEPTION_REGISTRATION_RECORD DispatcherContext = NULL;

        //Capture context of caller
        RtlCaptureContext(&Context);

        //Pop the current arguments
        Context.Esp += sizeof(TargetFrame) + sizeof(TargetIp) + sizeof(pException) + sizeof(ReturnValue);

        //Assign EAX (return value)
        Context.Eax = (DWORD)ReturnValue;

        if (pException == NULL)
        {
            Exception.ExceptionCode = STATUS_UNWIND;
            Exception.ExceptionAddress = (PVOID)Context.Eip;
            pException = &Exception;
        }

        if (TargetFrame == NULL)
        {
            //No target set, therefore exit after unwinding.
            pException->ExceptionFlags |= EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND;
        }
        else
            pException->ExceptionFlags |= EXCEPTION_UNWINDING;

        //Stack limits
        DWORD stackLow;
        DWORD stackHigh;
        GetCurrentThreadStackLimits(&stackLow, &stackHigh);

        for (PEXCEPTION_REGISTRATION_RECORD Registration = Registration::getRegistrationHead(); Registration != EXCEPTION_CHAIN_END; Registration = Registration->Next)
        {
            if (Registration == TargetFrame)
            {
                //Unwind up to but not including the target frame
                NtContinue(&Context, FALSE);

                return; //Should be unreachable
            }

            if (TargetFrame != NULL && (DWORD)TargetFrame < (DWORD)Registration)
            {
                /*
                    Target frame is less than Registration indicating it won't show up. This
                    is because Registration should only contain frames located in ascending 
                    order on the stack (frames point to older frames).

                    REMEMBER: Lower values indicate newer on stack
                */

                EXCEPTION_RECORD NewException = {};
                NewException.ExceptionCode = STATUS_INVALID_UNWIND_TARGET;
                NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                NewException.ExceptionRecord = pException;

                RtlRaiseException(&NewException);
            }

            if ((DWORD)Registration < stackLow || ((DWORD)Registration + sizeof(EXCEPTION_REGISTRATION_RECORD)) > stackHigh || ((DWORD)Registration & 0x3) != 0)
            {
                /*
                    Frame outside of stack limits or unaligned on stack

                    0x1 in binary is  01
                    0x2 in binary is  10
                    0x3 in binary is  11
                    0x4 in binary is 100

                    You can see how the bitwise AND is used to identify a 4 byte alignment.
                */

                EXCEPTION_RECORD NewException = {};
                NewException.ExceptionCode = STATUS_BAD_STACK;
                NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                NewException.ExceptionRecord = pException;

                RtlRaiseException(&NewException);
            }

            EXCEPTION_DISPOSITION Disposition = Handler::ExecuteHandler(pException, Registration, &Context, DispatcherContext, Registration->Handler, &Handler::NestedExceptionHandler<true>);

            switch (Disposition)
            {
            case ExceptionContinueSearch:
                break;

            case ExceptionCollidedUnwind:
                /*
                    An exception was thrown during an unwind handler. A new call to unwind is
                    made during the process of handling the exception. Therefore, in the process
                    of that new unwind we collide here from the NestedExceptionHandler. To pick
                    up on the old unwind, we assign Registration to the frame of the exception
                    thrown during the first unwind.

                    This is different from ExceptionNestedException because there are no flags
                    to be applied to the frames. The unwind should just return to where it was
                    before to prevent the unwind of frames that are supposed to stay.
                */
                Registration = DispatcherContext;
                break;

            default:
                EXCEPTION_RECORD NewException = {};

                NewException.ExceptionCode = STATUS_INVALID_DISPOSITION;
                NewException.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                NewException.ExceptionRecord = pException;

                RtlRaiseException(&NewException);
                break;
            }

            Registration::popRegistrationHead();
        }

        if (TargetFrame == EXCEPTION_CHAIN_END)
        {
            //Caller wanted all frames to be unwound
            NtContinue(&Context, FALSE);

            return; //Should be unreachable
        }

        //EXCEPTION_EXIT_UNWIND from NULL TargetFrame or nonexistent TargetFrame
        NtRaiseException(pException, &Context, FALSE);
    }
}