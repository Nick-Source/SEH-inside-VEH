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

#include <iostream>
#include <Windows.h>
#include <SEH/SEH.h>

/*
    NOTE: Linker option /SAFESEH is specified for this project
    NOTE: C++ try/catch exceptions require patching RtlUnwind, due to the NestedExceptionHandler, therefore this examples requires statically linking the runtime
*/

//C++ Exception Example - Requires patched RtlUnwind

void testCPPException()
{
    try
    {
        throw "EXCEPTION";
    }
    catch (...)
    {
        std::cout << "try/catch - Caught the C++ exception" << std::endl;
    }
}

//Helper Functions

__declspec(naked) EXCEPTION_REGISTRATION_RECORD* __cdecl getRegistrationHead()
{
    __asm
    {
        mov eax, dword ptr fs:[0]
        ret
    }
}

__declspec(naked) void __cdecl assignRegistrationHead(PEXCEPTION_REGISTRATION_RECORD newHead)
{
    __asm
    {
        mov eax, [esp + 4]
        mov dword ptr fs:[0], eax
        ret
    }
}

//Custom non-SafeSEH Exception Handler

EXCEPTION_DISPOSITION NTAPI _Function_class_(EXCEPTION_ROUTINE) CustomHandler(EXCEPTION_RECORD* ExceptionRecord, PVOID EstablisherFrame, CONTEXT* ContextRecord, PVOID DispatcherContext)
{
    if (ExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
    {
        std::cout << "CustomHandler - Unwinding" << std::endl;
        return ExceptionContinueSearch;
    }

    std::cout << std::hex << std::uppercase;
    std::cout << "CustomHandler - Exception thrown at 0x" << ExceptionRecord->ExceptionAddress << ": 0x" << ExceptionRecord->ExceptionCode << std::endl;

    int* divisor = (int*)(ContextRecord->Ebp + 12); //ptr to second parameter
    *divisor = 1;

    std::cout << "CustomHandler - Parameter \"divisor\" changed to 1" << std::endl;
    std::cout << std::nouppercase << std::dec;

    return ExceptionContinueExecution;
}

//Problematic Code

#pragma optimize( "", off )

int __cdecl divide(int dividend, int divisor)
{
    //Assign EH
    
    EXCEPTION_REGISTRATION_RECORD Registration;
    Registration.Next = getRegistrationHead();
    Registration.Handler = &CustomHandler;
    
    assignRegistrationHead(&Registration);
    
    //Divide
    int val = dividend / divisor;

    //Remove EH
    SEH::Unwind(Registration.Next, NULL, NULL, 0); //NOTE: RtlUnwind would create an exception because CustomHandler is a non-SafeSEH handler

    return val;
}

#pragma optimize( "", on )

int main()
{
    SEH::EnableSEH(); //Try commenting this line out

    //testCPPException(); //Requires patched RtlUnwind

    int val = divide(1, 0); //divide by 0
    std::cout << "1 / 0 = " << val << std::endl;

    SEH::DisableSEH();

    system("pause");
    return 0;
}