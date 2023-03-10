# Unwinding Problem

SEH is emulated in VEH with this library. As a result, we will intercept every exception and handle it ourselves. However, just using the library alone won't work for C++/`try-except` exceptions as they rely on unwinding the frame based handlers through `RtlUnwind`. Unfortunately, `RtlUnwind` has valid handler checks for any handlers that are to be unwound. As a result, this presents a problem as the actual C++ exception handler `__InternalCxxFrameHandler` (this isn't the one registered but it's used eventually) will unwind the frames using `RtlUnwind` and return execution itself. The microsoft-specific extension `try-except` will also unwind through `RtlUnwind`.  Whilst unwinding, `NestedExceptionHandler`, from this library, will be in the list of frames to unwind. Granted this library is statically linked to a bad module with SafeSEH broken, that handler will be an invalid handler to `RtlUnwind`.

## Solution

A way to combat this problem was created through manually patching any calls/jmps to `RtlUnwind` with calls/jmps to our custom `Unwind` function, which does not have a valid handler check (besides stack bound checking). As you may be able to tell, this particular solution would require having your module statically linking the runtime library (`/MT` or `/MTd`).  Otherwise, we would have to patch the actual visual runtime DLLs which requires a lot more work and isn't sustainable; essentially, it's unrealistic and not a smart decision. This is why your faulty module including this library should link the runtime library statically (`/MT` or `/MTd`).  However, if you don't plan on using C++/`try-except` exceptions or `RtlUnwind` then you may dynamically link the runtime library and not have to worry about patching `RtlUnwind`. How to patch `RtlUnwind` can be found in the folder [Patching RtlUnwind](Patching%20RtlUnwind).

## What about any RtlUnwind calls we can't patch?

The above solution will only work for modules that have the runtime library statically linked. What if our main module isn't statically linking the runtime library? Well there are a few solutions:

----
### Keep `SEH inside VEH` enabled only for your patched module

This one is a pretty straight forward solution which involves calling `EnableSEH` for your code and then subsequently disabling it through `DisableSEH` before returning to code not in your faulty module. But this solution is a bit restricted, with more intertwined code this solution won't be as practical to implement.

----
### Assign `EXCEPTION_CHECKING` to `VALID_TOP_HANDLER_CHECK` in `stdafx.h`

This solution will check if the top handler being passed to our mock up of `RtlDispatchException` can pass as a valid handler if passed to `RtlIsValidHandler`, the handler validation function in Windows. This may seem odd but it allows us to let the real SEH deal with the exception if the top handler is valid. That way any calls to `RtlUnwind` won't generate an exception from our invalid handler `NestedExceptionHandler`. The patching of `RtlUnwind` is still required for any faulty modules. **This relies on an assumption that the top handler is supposed to handle the exception.**

----
### Assign `EXCEPTION_CHECKING` to `BOUND_CHECK` in `stdafx.h`

Bound checking is an alternative to checking if the top handler is valid. This solution will check if the exception originated in the module containing `SEH inside VEH`. If so, it assumes we have responsibility to deal with it. This has the advantage that the expected exception handler does not have to be at the top of the list. Once again, the patching of `RtlUnwind` is still required for any faulty modules. **This relies on an assumption that exceptions shouldn't be dealt with across modules; however, that assumption can be broken in specific scenarios. It also relies on the assumption that our invalid handlers will not be called for exceptions occurring outside of our faulty module. Also read the top of [bound_check.cpp](/SEH%20inside%20VEH/src/bound_check.cpp) before using this method.** 

**IMPORTANT:** `SEH inside VEH` **must be statically linked to the faulty module for this to work.**

----
### Patch calls/jmps to RtlUnwind in memory

Manually patch calls/jmps to `RtlUnwind` with the custom `Unwind` function once everything is in memory instead. It's possible but I just didn't like this solution so it never got created.

----
### Dynamically link `SEH inside VEH` properly

If for some reason, you can dynamically link `SEH inside VEH` with the SafeSEH functionality correctly then this will work. `/SAFESEH:NO` is set for the DLL configuration of `SEH inside VEH`, that way `RtlUnwind` won't freak out about `NestedExceptionHandler`. The thing is if your able to load this properly, I'd assume you'd be able to load your faulty module just fine? That would just make this whole library useless. However, I'm not sure, maybe there exists a use for this. **NOTE:** This will only solve the issue of `NestedExceptionHandler` coming up as an invlaid handler, not any other unsafe handlers. Maybe this could be used in conjuction with the other solutions?

----
### Give up

Just give up. These solutions aren't designed to combat unpatched `RtlUnwind` functions completely. They are just interesting work arounds I came up with while designing this. They actually ended up being completely useless for me due to my tunnel vision; however, I kept them here because they were already finished and interesting. There may even be other work arounds.
