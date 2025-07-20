
#include <stdarg.h>
#include <intrin.h>

#include "utils.hpp"

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    vDbgPrintEx(0, 0, format, args);

    va_end(args);
}

void PmDbg(const char* format, ...) {
    va_list args;
    va_start(args, format);

    kprintf("[Privmem] ");   
    kprintf(format, args);

    va_end(args);
}

void FlushTB() {
    UINT64 CR4 = __readcr4();

    if (CR4 & ((1 << 7) | (1 << 17)))
    {
        __writecr4(CR4 ^ (1 << 7));
        __writecr4(CR4);
        return;
    }

    __writecr3(__readcr3());
}

void SwitchAddressSpace(_In_ UINT64 NewCR3) {
    __writecr3(NewCR3);
    FlushTB();
}

BOOLEAN FindKernelModule(
    const char* targetModuleName,
    PSYSTEM_MODULE_ENTRY Entry
) {
    ULONG Bytes = 0;
    NTSTATUS Status;

    Status = ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, &Bytes);

    if (Status != STATUS_INFO_LENGTH_MISMATCH) {
        kprintf("[VgCheck] : ZwQuerySystemInformation failed 1.\n", Status);
        return FALSE;
    }

    PSYSTEM_MODULE_INFORMATION pMods = (PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(
        NonPagedPoolNx, Bytes, 'tagM');
    if (!pMods) {
        kprintf("[VgCheck] : Failed to allocate buffer\n");
        return FALSE;
    }

    RtlZeroMemory(pMods, Bytes);

    Status = ZwQuerySystemInformation(SystemModuleInformation, pMods, Bytes, &Bytes);


    if (!NT_SUCCESS(Status)) {
        ExFreePool2(pMods, 'tagM', NULL, 0);
        return FALSE;
    }

    BOOLEAN found = FALSE;
    for (ULONG i = 0; i < pMods->Count; i++) {
        PSYSTEM_MODULE_ENTRY pMod = &pMods->Module[i];
        const char* fileName = (const char*)(pMod->FullPathName + pMod->OffsetToFileName);


        if (_stricmp(fileName, targetModuleName) == 0) {
            RtlCopyMemory(Entry, pMod, sizeof(SYSTEM_MODULE_ENTRY));
            found = TRUE;
            break;
        }
    }

    ExFreePool2(pMods, 'tagM', NULL, 0);
    return found;
}