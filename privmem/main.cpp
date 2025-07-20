
#include <ntddk.h>

#include "utils.hpp"
#include "paging.hpp"

#define DEVICE_NAME L"\\Device\\privmem"
#define SYMBOLIC_NAME L"\\DosDevices\\privmem"

#define IOCTL_PM_INITIALIZE CTL_CODE(FILE_DEVICE_UNKNOWN,		0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PM_WHITELIST_THREAD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

Vector<PKTHREAD> WhitelistedThreads;
Vector<PVOID>    MemoryAllocations;

UINT64 OriginalHalEntry = 0;

static void SwapContextHook() {
	if (!WhitelistedThreads.Contains(KeGetCurrentThread())) {
		return;
	}

	memcpy(Page::ClonePML4Virt, Page::ClientPML4Virt, Page::Size);

	Page::ClonePML4Virt[Page::FreePML4Index] = Page::ShadowPML4;
	CR3 Cr3(__readcr3());
	Cr3.Flags = Page::CloneCR3Phys;
	SwitchAddressSpace(Cr3.Flags);

	kprintf("[Privmem] Handling context swap for thread %d | process %d\n", PsGetCurrentThreadId(), PsGetCurrentProcessId());
}

void PmDriverUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING SymLink = RTL_CONSTANT_STRING(SYMBOLIC_NAME);
	IoDeleteSymbolicLink(&SymLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	//
	// Free allocated pages (used when constructing shadow region)
	//
	KIRQL Irql = MemoryAllocations.EnterLock();
	for (UINT32 i = 0; i < MemoryAllocations.Size(); i++) {
		MmFreeContiguousMemory(MemoryAllocations[i]);
	}
	MemoryAllocations.ExitLock(Irql);

	//
	// Destory all structures
	//
	WhitelistedThreads.Destory();
	MemoryAllocations.Destory();
	
	//
	// Restore hook
	//
	HalPrivateDispatchTable.ContextSwapHook = OriginalHalEntry;
	kprintf("[Privmem] Unloaded\n");
}


NTSTATUS PmDriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS PmDriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG OutputBufferLength = Stack->Parameters.DeviceIoControl.OutputBufferLength;
	UINT32* SystemBuffer = (UINT32*)Irp->AssociatedIrp.SystemBuffer;

	if (Stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_PM_INITIALIZE) {

		WhitelistedThreads.Clear();
		CR3 Cr3(__readcr3());
		Page::ClientPML4Virt = (MMPTE_HARDWARE*)MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = LONGLONG(Cr3.Flags) });

		bool FoundFreeIndex = false;
		for (int i = 0; i < 256; i++) {
			kprintf("[Privmem] PML4E-%d:\t0x%llx\n", i, Page::ClientPML4Virt[i].Flags);

			if (!Page::ClientPML4Virt[i].Flags) {
				FoundFreeIndex = true;
				Page::FreePML4Index = i;
				break;
			}
		}

		if (!FoundFreeIndex) {
			kprintf("[Privmem] Cannot find a free PML4 entry\n");
		}

		kprintf("[Privmem] Initializing shadow page for index: %d\n", Page::FreePML4Index);
		
		*SystemBuffer = Page::FreePML4Index;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 4;
	}
	else if (Stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_PM_WHITELIST_THREAD) {

		kprintf("[Privmem] Whitelisting thread %d\n", PsGetCurrentThreadId());

		WhitelistedThreads.Insert(KeGetCurrentThread());

		memcpy(Page::ClonePML4Virt, Page::ClientPML4Virt, Page::Size);
		Page::ClonePML4Virt[Page::FreePML4Index] = Page::ShadowPML4;

		CR3 Cr3(__readcr3());
		Cr3.Flags = Page::CloneCR3Phys;
		SwitchAddressSpace(Cr3.Flags);

		kprintf("[Privmem] Whitelisted thread %d\n", PsGetCurrentThreadId());

		*SystemBuffer = 0xDEADBEEF;
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 4;
	}
	else {
		kprintf("[Privmem] Unknown IOCTL code being sent\n");
		Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Information = 0;
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}

PM_EXPORT NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	SYSTEM_MODULE_ENTRY Ntoskrnl = { 0 };
	if (!FindKernelModule("ntoskrnl.exe", &Ntoskrnl)) {
		kprintf("[Privmem] ntoskrnl.exe not found\n");
		return STATUS_FAILED_DRIVER_ENTRY;
	}

	UNICODE_STRING devName = RTL_CONSTANT_STRING(DEVICE_NAME);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYMBOLIC_NAME);
	PDEVICE_OBJECT deviceObject = NULL;

	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
	if (!NT_SUCCESS(status)) {
		kprintf("[Privmem] Failed to create device\n");
		return status; 
	}

	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		kprintf("[Privmem] Failed to create symbolic link\n");
		IoDeleteDevice(deviceObject);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE]	= PmDriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]	= PmDriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PmDriverDeviceControl;
	DriverObject->DriverUnload = PmDriverUnload;

	OriginalHalEntry = HalPrivateDispatchTable.ContextSwapHook;

	Page::CreateShadowRegion();
	HalPrivateDispatchTable.ContextSwapHook = (UINT64)SwapContextHook;

	kprintf("[Privmem] Kernel base address: 0x%llx\n", Ntoskrnl.ImageBase);
	kprintf("[Privmem] HalPrivateDispatchTable: 0x%llx\n", (UINT64)&HalPrivateDispatchTable);
	kprintf("[Privmem] Driver loaded\n");

	return STATUS_SUCCESS;
}