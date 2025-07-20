#pragma once

#include <ntddk.h>

typedef struct _HAL_PRIVATE_DISPATCH_TABLE {
	UCHAR  Pad[0x400];
	UINT64 ContextSwapHook;
} HAL_PRIVATE_DISPATCH_TABLE;

typedef struct _SYSTEM_MODULE_ENTRY
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;

typedef struct _SYSTEM_MODULE_INFORMATION
{
	ULONG Count;
	SYSTEM_MODULE_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

union MMPTE_HARDWARE {
	struct {
		UINT64 Present : 1;
		UINT64 Write : 1;
		UINT64 UserAccessible : 1;
		UINT64 WriteThrough : 1;
		UINT64 CacheDisable : 1;
		UINT64 Accessed : 1;
		UINT64 Dirty : 1;
		UINT64 LargePage : 1;
		UINT64 Available : 4;
		UINT64 PageFrameNumber : 36;
		UINT64 ReservedForHardware : 4;
		UINT64 ReservedForSoftware : 11;
		UINT64 NoExecute : 1;
	} u;

	UINT64 Flags;
	MMPTE_HARDWARE() = default;

	constexpr MMPTE_HARDWARE(const UINT64 Value) : Flags(Value) {}
};

// 

union CR3 {
	CR3() { Flags = 0; }

	CR3(const UINT64 Value) { Flags = Value; }

	bool operator==(const CR3& B) const { return Flags == B.Flags; }

	struct {
		UINT64 Reserved1 : 3;
		UINT64 Pwt : 1;
		UINT64 Pcd : 1;
		UINT64 Reserved2 : 7;
		UINT64 PageDirectoryBase : 52;
	} u;

	UINT64 Flags;
};

union CR4 {
	CR4() { Flags = 0; }

	CR4(const UINT64 Value) { Flags = Value; }

	bool operator==(const CR4& B) const { return Flags == B.Flags; }

	struct {
		UINT64 VirtualModeExtensions : 1;
		UINT64 ProtectedModeVirtualInterrupts : 1;
		UINT64 TimestampDisable : 1;
		UINT64 DebuggingExtensions : 1;
		UINT64 PageSizeExtensions : 1;
		UINT64 PhysicalAddressExtension : 1;
		UINT64 MachineCheckEnable : 1;
		UINT64 PageGlobalEnable : 1;
		UINT64 PerformanceMonitoringCounterEnable : 1;
		UINT64 OsFxsaveFxrstorSupport : 1;
		UINT64 OsXmmExceptionSupport : 1;
		UINT64 UsermodeInstructionPrevention : 1;
		UINT64 LA57 : 1;
		UINT64 VmxEnable : 1;
		UINT64 SmxEnable : 1;
		UINT64 Reserved2 : 1;
		UINT64 FsgsbaseEnable : 1;
		UINT64 PcidEnable : 1;
		UINT64 OsXsave : 1;
		UINT64 Reserved3 : 1;
		UINT64 SmepEnable : 1;
		UINT64 SmapEnable : 1;
		UINT64 ProtectionKeyEnable : 1;
		UINT64 Reserved4 : 41;
	};

	UINT64 Flags;
};
