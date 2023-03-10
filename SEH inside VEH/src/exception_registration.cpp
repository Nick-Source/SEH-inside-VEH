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
#include "exception_registration.h"

namespace SEH
{
    namespace Registration
    {
        __declspec(naked) EXCEPTION_REGISTRATION_RECORD* __cdecl getRegistrationHead()
        {
            __asm
            {
                mov eax, dword ptr fs:[0]
                ret
            }
        }

        __declspec(naked) void __cdecl popRegistrationHead()
        {
            __asm
            {
                mov eax, dword ptr fs:[0]
                mov eax, [eax] //Next is stored first in EXCEPTION_REGISTRATION_RECORD
                mov dword ptr fs:[0], eax
                ret
            }
        }
    }
}