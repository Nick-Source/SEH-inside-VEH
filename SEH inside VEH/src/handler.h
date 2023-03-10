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

namespace SEH
{
    namespace Handler
    {
        //Check if top handler is a valid SafeSEH handler
        bool isTopHandlerValid();

        //Used to catch exceptions inside other SEH handlers
        template <bool unwind, EXCEPTION_DISPOSITION disposition = unwind ? ExceptionCollidedUnwind : ExceptionNestedException>
        EXCEPTION_DISPOSITION NTAPI _Function_class_(EXCEPTION_ROUTINE) NestedExceptionHandler(EXCEPTION_RECORD*, PEXCEPTION_REGISTRATION_RECORD, PCONTEXT, PEXCEPTION_REGISTRATION_RECORD&);

        //Execute handlers with an exception handler for identifying nested exceptions
        typedef EXCEPTION_DISPOSITION(NTAPI _Function_class_(EXCEPTION_ROUTINE)* PNESTED_EXCEPTION_HANDLER)(EXCEPTION_RECORD*, PEXCEPTION_REGISTRATION_RECORD, PCONTEXT, PEXCEPTION_REGISTRATION_RECORD&);
        EXCEPTION_DISPOSITION __cdecl ExecuteHandler(PEXCEPTION_RECORD ExceptionRecord, PEXCEPTION_REGISTRATION_RECORD EstablisherFrame, PCONTEXT ContextRecord, PEXCEPTION_REGISTRATION_RECORD& DispatcherContext, PEXCEPTION_ROUTINE Handler, PNESTED_EXCEPTION_HANDLER NestedExceptionHandler);
    }
}