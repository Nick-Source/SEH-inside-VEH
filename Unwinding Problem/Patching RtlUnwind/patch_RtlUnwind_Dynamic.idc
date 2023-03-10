#include <idc.idc>

/*----------------------------------------------Helpers-----------------------------------------------*/

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

static getUnwindImport()
{
    auto DllName = tolower(ask_str("SEH.dll", HIST_IDENT, "What is the name of the DLL containing the replacement unwind function?"));
    auto unwindSymbol = ask_str("", HIST_IDENT, "What is the mangled symbol for the replacement unwind function?");
    
    return searchIAT(&DllName, &unwindSymbol);
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

    auto unwind = getUnwindImport();
    
    if (isBadAddr(unwind, "No replacement unwind function found. Did you enter the DLL name and unwind symbol correctly?"))
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
        patch_dword(rtlUnwind, unwind); //Replace the address of the RtlUnwind import with the replacement Unwind import
        msg("\nPatched RtlUnwind reference at: 0x%X\n", rtlUnwind);
        
        //Check for next reference of RtlUnwind
        rtlUnwind = find_binary(rtlUnwind, SEARCH_DOWN, rtlUnwindImportStr);
    }
    
    msg("\n--------------------------End patch--------------------------\n\n");
}