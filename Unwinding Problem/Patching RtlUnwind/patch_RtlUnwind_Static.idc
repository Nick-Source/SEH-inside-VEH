#include <idc.idc>

/*----------------------------------------------Helpers-----------------------------------------------*/

static getReloc()
{
    auto reloc = get_first_seg();

    while (reloc != BADADDR)
    {
        if (get_segm_name(reloc) == ".reloc")
        {
            break;
        }
        
        reloc = get_next_seg(reloc);
    }
    
    return reloc;
}

//Expects TargetDLLNameStr in lowercase
static searchIAT(TargetDLLNameStr, TargetFunctionStr)
{
    auto imageBase = get_imagebase();
    
    /*
        Explanation behind 0x80 offset in ImportDescriptor:
        
        auto ntHeader = imageBase + dword(imageBase + 0x3c);
        auto OptionalHeader = ntHeader + 0x18;
        auto DataDirectory = OptionalHeader + 0x60;
        auto ImportDirectory = DataDirectory + 8;
        
        auto ImportDescriptor = imageBase + dword(ImportDirectory);
    */
    
    auto ntHeader = imageBase + dword(imageBase + 0x3c);
    auto ImportDescriptor = imageBase + dword(ntHeader + 0x80);
    auto DLLName = imageBase + dword(ImportDescriptor + 0xC);
    auto DLLNameStr = get_strlit_contents(DLLName, -1, get_str_type(DLLName));
    
    while (DLLName != imageBase) //dword(ImportDescriptor + 0xC) <-- would be 0 when finished
    {
        DLLNameStr = tolower(DLLNameStr);
        
        if (DLLNameStr != TargetDLLNameStr)
        {
            ImportDescriptor = ImportDescriptor + 0x14; //Get next ImportDescriptor
            DLLName = imageBase + dword(ImportDescriptor + 0xC);
            DLLNameStr = get_strlit_contents(DLLName, -1, get_str_type(DLLName));
            
            continue;
        }
        
        auto pImportByName = imageBase + dword(ImportDescriptor);
        auto FunctionName = imageBase + dword(pImportByName) + 0x2;
        auto FunctionNameStr;
        auto IAT_index = 0;
        
        while (FunctionName != (imageBase + 0x2)) //dword(pImportByName) <-- would be 0 when finished
        {
            FunctionNameStr = get_strlit_contents(FunctionName, -1, get_str_type(FunctionName));
            
            if (FunctionNameStr == TargetFunctionStr)
            {
                auto IAT_Entry = imageBase + dword(ImportDescriptor + 0x10);
                IAT_Entry = IAT_Entry + (IAT_index * 4);
                return IAT_Entry;
            }
            
            pImportByName = pImportByName + 0x4;
            FunctionName = imageBase + dword(pImportByName) + 0x2;
            ++IAT_index;
        }
        
        ImportDescriptor = ImportDescriptor + 0x14; //Get next ImportDescriptor
        DLLName = imageBase + dword(ImportDescriptor + 0xC);
        DLLNameStr = get_strlit_contents(DLLName, -1, get_str_type(DLLName));
    }
    
    return BADADDR;
}

static getRtlUnwindImport()
{    
    auto IAT_Entry = searchIAT("ntdll.dll", "RtlUnwind");

    if (IAT_Entry == BADADDR)
    {
        IAT_Entry = searchIAT("kernel32.dll", "RtlUnwind"); //Kernel32 has a stub function for RtlUnwind
    }
    
    return IAT_Entry;
}

/*----------------------------------------------Patching----------------------------------------------*/

static patchCall(rtlUnwind, unwind)
{
    patch_byte(rtlUnwind, 0xE8);
    patch_dword(rtlUnwind + 1, unwind - rtlUnwind - 5);
    patch_byte(rtlUnwind + 5, 0x90);
    msg("\nPatched RtlUnwind call at: 0x%X\n", rtlUnwind);
}

static patchJmp(rtlUnwind, unwind)
{
    patch_byte(rtlUnwind, 0xE9);
    patch_dword(rtlUnwind + 1, unwind - rtlUnwind - 5);
    patch_byte(rtlUnwind + 5, 0x90);
    msg("\nPatched RtlUnwind jmp at: 0x%X\n", rtlUnwind);
}

//Remove relocation information for any RtlUnwind reference
static patchReloc(reloc, rtlUnwind)
{
    rtlUnwind = rtlUnwind - get_imagebase(); //Get relative address

    auto rtlUnwindOffset = 0x3000 + ((rtlUnwind + 0x2) & 0xFFF); //0x2 is the offset for the opcode and operand
    auto rtlUnwindOffsetStr = sprintf("%X", rtlUnwindOffset);
    
    auto RVA = dword(reloc);
    auto rtlUnwindRVA = (rtlUnwind & 0xFFFFF000); //Last 16 bits are ignored because they are the offsets provided in relocation block 
    auto relocTarget = rtlUnwind + get_imagebase() + 0x2; //Restore image base offset and add 0x2 to point to relocation target
    
    while (RVA > 0)
    {
        RVA = dword(reloc);
        auto blockSize = dword(reloc + 4);
        
        if (RVA == rtlUnwindRVA)
        {
            auto rtlUnwindEntry = find_binary(reloc, SEARCH_DOWN, rtlUnwindOffsetStr); 
            patch_word(rtlUnwindEntry, 0);
            del_fixup(relocTarget);
            
            msg("Patched relocation information for 0x%X at: 0x%X\n", relocTarget, rtlUnwindEntry);
            break;
        }
        
        reloc = reloc + blockSize; //Assign to next block
    }
}

/*------------------------------------------------Main------------------------------------------------*/

static isBadAddr(val, err)
{
    if (val == BADADDR)
    {
        warning(err + "\n");
        msg("\nERROR: " + err + "\n");
        msg("\n--------------------------End patch--------------------------\n\n");
        return 1; //true
    }
    
    return 0; //false
}

static main()
{
    msg("\n-------------------------Begin patch-------------------------\n");

    auto unwind = choose_func("Choose the replacement unwind function.");
    
    if (isBadAddr(unwind, "No replacement unwind function chosen."))
        return;
    
    auto reloc = getReloc();

    if (isBadAddr(reloc, "Can't find section \".reloc\". Did you forget to manual load?"))
        return;
    
    auto rtlUnwindImport = getRtlUnwindImport();
    
    if (isBadAddr(rtlUnwindImport, "Can't find RtlUnwind import in either kernel32.dll or ntdll.dll. Did you forget to statically link the runtime library (C++/try-except exceptions)? Maybe it is a stub function in a different import?"))
        return;
    
    auto rtlUnwindImportStr = sprintf("%X", rtlUnwindImport);
    auto rtlUnwind = find_binary(0, SEARCH_DOWN, rtlUnwindImportStr); 
    
    if (isBadAddr(rtlUnwind, "Can't find any RtlUnwind references."))
        return;
    
    while (rtlUnwind != BADADDR)
    {
        rtlUnwind = rtlUnwind - 0x2; //0x2 is the offset for the opcode and operand
    
        auto opcode = byte(rtlUnwind);
        auto operand = byte(rtlUnwind + 1);
    
        if (opcode == 0xFF)
        {
            if (operand == 0x15) //Call
            {
                //Patch the call of RtlUnwind to a relative call of our custom Unwind function
                patchCall(rtlUnwind, unwind);
            }
            else if (operand == 0x25) //Jmp
            {
                //Patch the jmp of RtlUnwind to a relative jmp of our custom Unwind function
                patchJmp(rtlUnwind, unwind);
            }
            else
            {
                msg("\nOnly near calls/jmps with EIP-relative addressing are supported.\n");
                msg("\nUnable to patch RtlUnwind reference at: 0x%X\n", rtlUnwind);
                continue;
            }
        }
        else
        {
            msg("\nOnly near, absolute indirect, calls/jmps are supported.\n");
            msg("\nUnable to patch RtlUnwind reference at: 0x%X\n", rtlUnwind);
            continue;
        }
        
        /*
            Patch the relocation information of the old calls to RtlUnwind
            
            NOTE: We are only allowed to pass reloc by reference because rtlUnwind is
            patched one at a time and in order. Therefore, the next iteration will be
            at a higher RVA. This skips re-searching part of the .reloc section we know
            won't contain the next rtlUnwind entry.
        */
        patchReloc(&reloc, rtlUnwind);
        
        //Check for next reference of RtlUnwind
        rtlUnwind = find_binary(rtlUnwind, SEARCH_DOWN, rtlUnwindImportStr);
    }
    
    msg("\n--------------------------End patch--------------------------\n\n");
}