
#include <windows.h>
#include <stdio.h>

#include <cstdint>

#define IOCTL_PM_INITIALIZE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PM_WHITELIST_THREAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DEVICE_PATH "\\\\.\\privmem"

void SendIoctl(HANDLE device, DWORD ioctlCode, const char* message, ULONG* OutputBuffer) {
    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        device,
        ioctlCode,
        NULL, 0,                
        OutputBuffer, sizeof(ULONG),
        &bytesReturned,
        NULL
    );

    if (success) {
        printf("[+] %s: IOCTL succeeded.\n", message);
    }
    else {
        printf("[-] %s: IOCTL failed. Error code: %lu\n", message, GetLastError());
    }
}

int main() {
    printf("[*] Opening handle to driver: %s\n", DEVICE_PATH);

    HANDLE hDevice = CreateFileA(
        DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open device. Error code: %lu\n", GetLastError());
        return EXIT_FAILURE;
    }

    ULONG64 ShadowPageBase = 0;
    ULONG Pml4Index = 0;

    SendIoctl(hDevice, IOCTL_PM_INITIALIZE, "Initialize Shadow Page", &Pml4Index);
    if (!Pml4Index) {
        printf("[-] Failed to initialize shadow page.\n");
        return EXIT_FAILURE;
    }

    ShadowPageBase = uint64_t(Pml4Index) << 39;

    printf("[+] Shadow base 0x%llx\n", ShadowPageBase);
    printf("[*] Attempt to access shadow page before whitlisting: %s\n",
        IsBadReadPtr((void*)ShadowPageBase, 1) ? "Failed" : "Success");

    ULONG Status = 0;
    SendIoctl(hDevice, IOCTL_PM_WHITELIST_THREAD, "Whitelist Thread", &Status);
    if (Status != 0xDEADBEEF) {
        printf("[-] Failed add current thread to whitelist.\n");
        return EXIT_FAILURE;
    }

    printf("[+] Whitelist thread status: 0x%llx\n", Status);

    printf("[*] Attempt to access shadow page after whitlisting: %s\n",
        IsBadReadPtr((void*)ShadowPageBase, 1) ? "Failed" : "Success");

    printf("[*] Attempting to manipulate shadow page...\n");

    __try {
        volatile int* testPtr = (int*)ShadowPageBase;
        *testPtr = 0x13371337;
        printf("[+] Successfully wrote 0x13371337 to shadow page.\n");

        int value = *testPtr;
        printf("[+] Successfully read back value: 0x%x\n", value);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[-] Exception occurred while accessing shadow page.\n");
    }

    CloseHandle(hDevice);
    printf("[*] Done.\n");
    return 0;
}