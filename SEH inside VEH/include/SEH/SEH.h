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
#include <winnt.h>

namespace SEH
{
    //Adds a custom SEH handler to the bottom of VEH only once
    void EnableSEH();

    //Removes the SEH handler assigned from EnableSEH
    void DisableSEH();
    
    //An unwind implementation without SafeSEH
    void NTAPI Unwind(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD pException, PVOID ReturnValue);
}

/*
    Required to stop linker from optimizing Unwind out
    https://devblogs.microsoft.com/oldnewthing/20150102-00/?p=43233

    Replace "REPLACE_ME" with symbol for Unwind (the decorated name)
    To get the symbol, build the library and then use dumpbin

    This is only necessary when RtlUnwind needs to be patched and
    SEH::Unwind is never referenced. Not necessary when using
    dynamic linakge.
*/
//#pragma comment(linker, "/include:REPLACE_ME")