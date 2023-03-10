# Patching RtlUnwind

Patching RtlUnwind will look slightly different depending on how you link `SEH inside VEH`. Below are instructions for both static and dynamic linkage.

## Statically linking `SEH inside VEH`

1. Compile the library
2. Run `dumpbin` with option `/SYMBOLS` on `SEH inside VEH.lib` (the library file produced)
3. Find the decorated symbol for `SEH::Unwind` and copy it
    - You can `Ctrl` + `F` in `cmd` for `SEH::Unwind`
4. Uncomment the pragma include line from `SEH.h` and paste the symbol in
    - This step is only necessary if `SEH::Unwind` is never called and certain optimizations are enabled
5. Link the library to your faulty module and compile
6. Open the PE in `IDA`
7. Check Manual Load
8. Answer the questions but ensure `.reloc` is loaded
    - Any base is fine
    - I think it's best to not load every section manually by choosing `No` and instead choosing yes for loading the `File Header`. Loading the `File Header` will load `.reloc` granted it is created properly.
    - Make sure you load PDB information to locate the `Unwind` function later
9. Go to `File` ---> `Script File` and choose `patch_RtlUnwind_Static.idc`
10. Choose `SEH::Unwind` from the list of functions
11. It should output every patch made in `Output`
12. Go to `Edit` ---> `Patch Program` and choose `Apply patches to input file...` and click `Ok`
13. Profit

## Dynamically linking `SEH inside VEH`

1. Compile the library
2. Run `dumpbin` with option `/SYMBOLS` on `SEH.dll` (the DLL file produced)
3. Find the decorated symbol for `SEH::Unwind` and copy it
    - You can `Ctrl` + `F` in `cmd` for `SEH::Unwind`
4. Compile your faulty module with `SEH inside VEH` dynamically linked (requires a few extra steps, check the folder [SEH inside VEH](/SEH%20inside%20VEH))
5. Open your final PE in `IDA`
6. Check Manual Load
7. Answer the questions but ensure `.reloc` is loaded
    - Any base is fine
    - I think it's best to not load every section manually by choosing `No` and instead choosing yes for loading the `File Header`. Loading the `File Header` will load `.reloc` granted it is created properly.
    - Make sure you load PDB information to locate the `Unwind` function later
8. Go to `File` ---> `Script File` and choose `patch_RtlUnwind_Dynamic.idc`
9. Enter the name of the DLL file produced for `SEH inside VEH` (`SEH.dll` by default)
10. Paste the decorated symbol for `SEH::Unwind`
11. It should output every patch made in `Output`
12. Go to `Edit` ---> `Patch Program` and choose `Apply patches to input file...`
13. Profit
