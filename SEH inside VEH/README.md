# SEH inside VEH

This directory contains the actual library project files and explanations. Also, this project was heavily commented in hopes to help understanding.

## IMPORTANT

This library may not work as you expect right off the bat. Please check the folder [Unwinding Problem](/Unwinding%20Problem) before using this library and see if any of the problems will affect you. This library was also made for **user mode only**, there is no DPC stack validation for handlers in `DispatchException` and `Unwind`.

## How to use the library?

Compile the project `SEH inside VEH` and use the library files produced. Also, use the header file `#include <SEH/SEH.h>` in your project. There are 3 available functions:

| Function           | Description                                                       |
|--------------------|-------------------------------------------------------------------|
| `SEH::EnableSEH`   | Adds a custom SEH handler to the bottom of VEH only once          |
| `SEH::DisableSEH`  | Removes the SEH handler assigned from EnableSEH                   |
| `SEH::Unwind`      | An unwind implementation without SafeSEH (`RtlUnwind` replacement)|

`EnableSEH` can be called multiple times after being enabled; however, nothing will happen. The handler will only be readded to VEH once `DisableSEH` is called. The opposite is also true. 

## Linking the library

This library may be statically linked or dynamically linked; however, the default is a static library. If you wish to dynamically link, you must export the three functions listed above with `__declspec(dllexport)` and switch `Configuration Type` to dynamic DLL. Those three functions can be found in `src/SEH.cpp`. Don't forget to change their linkage in `include/SEH/SEH.h` accordingly. **Warning:** You should not dynamically link the library if using `BOUND_CHECK`. More info is explained in the folder [Unwinding Problem](/Unwinding%20Problem).


# SEH components

### FS:[0]

`FS:[0]` is the linked list containing the frames of exception handlers. [Win32 Thread Information Block](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block)

### Exception Disposition Values

Understanding handler return values is a crucial part to understanding SEH. Below is a list of `Exception Disposition Values` that an exception handler can return.

| Disposition        | Description |
|--------------------|-------------|
| `ExceptionContinueExecution` | The execution should continue from the saved `CONTEXT` |
| `ExceptionContinueSearch`    | Handler did not handle the exception. Let the next handler try. |
| `ExceptionNestedException`   | LMAOOOOO yea this is confusing. Essentially, it means a handler had an exception while trying to deal with another exception. The way this is implemented is through `NestedExceptionHandler`. It is a handler that is registered before running any handler. That way if it is ever called in the list of handlers, we know an exception occurred. `ExecuteHandler`, the thunk function that calls every handler, will have saved the throwing handler's frame (its registration) in the stack so we can know what frame threw and notify all frames that this is a nested exception up to and including the one that threw. It relies on the fact that frames created on higher values in the stack are older and lower values are newer, that way if we encounter multiple nested exceptions the `EXCEPTION_NESTED_CALL` flag can be enabled up to the last throwing frame. Therefore all frames should be created on the stack. Here is a visual demonstrating how multiple nested exceptions can override each other and why we need to verify the oldest throwing frame; thus, explaining why frames need to be in order on the stack: ![Gif of nested exception handlers overriding each other](NEHs%20overriding.gif) (***NOTE:** Each `NestedExceptionHandler` will get the address of their corresponding throwing frame from `ExecuteHandler` saving it on the stack in relation to the `NestedExceptionHandler` registration. This is a confusing idea that is best explained by the code itself.*) Imagine if the addresses of these frames weren't in order of their age; we would not be able to tell which one is older on the next iteration. Essentially, `Nested Exception Handler 1` would've overrided `Nested Exception Handler 2` on the next iteration, *not shown in this gif*, if we just simply took their saved throwing frame instead of trying to verify it as the oldest one we received. Without knowing which one is the last one, we are left in confusion on how long we should leave the flag `EXCEPTION_NESTED_CALL` on. |
| `ExceptionCollidedUnwind`    | Similar to `ExceptionNestedException` but for unwinding instead (`RtlUnwind` or `SEH::Unwind`). This disposition is retrieved the same way through `NestedExceptionhHandler` but doesn't require notifying every frame that it is a nested exception. This is because unwinding will actually remove the frames; instead, it just picks up where the old unwind left off. |                         

### Dispatch Exception

This is the function that is responsible for dispatching the exception through frame based handlers. In the remake this library has, there are no valid handler checks besides the one solution in the root folder [Unwinding Problem](/Unwinding%20Problem). But that isn't necessarily a "valid handler check." It's a unique solution. Besides that, the only form of validation is through checking that every handler called is on the stack and aligned. This is required as described by `ExceptionNestedException`'s description. 

### Unwind

Unwind will call handlers up to a specific one and notify them they are being unwound. Essentially, that means they are being removed. This function also has no validation except the stack validation for every handler called. That check isn't required but it was kept for consistency with `SEH::DispatchException`.

### Apart from these, the code is heavily commented so that should help understanding as well.
