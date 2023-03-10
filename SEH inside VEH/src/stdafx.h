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

#pragma once
#define UMDF_USING_NTSTATUS

#include <Windows.h>
#include <ntstatus.h>
#include <dbghelp.h>
#include <intrin.h>
#include <vector>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
EXTERN_C NTSYSAPI NTSTATUS NTAPI NtContinue(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);
EXTERN_C NTSYSAPI NTSTATUS NTAPI NtRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ThreadContext, BOOLEAN HandleException);

#define EXCEPTION_CHAIN_END (PEXCEPTION_REGISTRATION_RECORD)-1

/*
    Ways to determine if we should handle specific exceptions.

    This is useful for scenarios where our non-SafeSEH nested
    exception handler can present issues. One example is during
    a scenario where an unpatched RtlUnwind is called and needs
    to unwind with our non-SafeSEH handler in the way.
*/

#define NO_CHECK 0
#define BOUND_CHECK 1 //Compare exception's origin address to bounds of our module
#define VALID_TOP_HANDLER_CHECK 2 //If top handler is valid (in the SafeSEH table and can pass RtlIsValidHandler) pass it to real SEH

#define EXCEPTION_CHECKING NO_CHECK