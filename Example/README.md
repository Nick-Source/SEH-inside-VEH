# SEH inside VEH - Example

This is a project showing how the library can be used. This project has `/SAFESEH` set, enabling `SafeSEH` and contains an unsafe handler. Trying to use the handler without this library will result in an exception. You can try that by commenting out `SEH::EnableSEH` in `main`. Alongside, there is an optional demonstration of C++ exceptions; however, that requires patching the final image. To do so, compile the program and then patch it using the instructions found in the folder [Patching RtlUnwind](/Unwinding%20Problem/Patching%20RtlUnwind).

## Custom Handler

This example creates a structured exception handler, `CustomHandler`, to handle a division by 0. When the handler is called, it changes the second parameter from 0 to 1, allowing the division to work. After changing it, execution is transferred back to the throwing line, re-executing it.
