#pragma once

#include <intrin.h>

#include "structs.hpp"

#define PM_IMPORT extern "C" __declspec(dllimport)
#define PM_EXPORT extern "C"

void kprintf(const char* format, ...);

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation = 0,
	SystemProcessorInformation = 1,             // obsolete...delete
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemPathInformation = 4,
	SystemProcessInformation = 5,
	SystemCallCountInformation = 6,
	SystemDeviceInformation = 7,
	SystemProcessorPerformanceInformation = 8,
	SystemFlagsInformation = 9,
	SystemCallTimeInformation = 10,
	SystemModuleInformation = 11,
	SystemLocksInformation = 12,
	SystemStackTraceInformation = 13,
	SystemPagedPoolInformation = 14,
	SystemNonPagedPoolInformation = 15,
	SystemHandleInformation = 16,
	SystemObjectInformation = 17,
	SystemPageFileInformation = 18,
	SystemVdmInstemulInformation = 19,
	SystemVdmBopInformation = 20,
	SystemFileCacheInformation = 21,
	SystemPoolTagInformation = 22,
	SystemInterruptInformation = 23,
	SystemDpcBehaviorInformation = 24,
	SystemFullMemoryInformation = 25,
	SystemLoadGdiDriverInformation = 26,
	SystemUnloadGdiDriverInformation = 27,
	SystemTimeAdjustmentInformation = 28,
	SystemSummaryMemoryInformation = 29,
	SystemMirrorMemoryInformation = 30,
	SystemPerformanceTraceInformation = 31,
	SystemObsolete0 = 32,
	SystemExceptionInformation = 33,
	SystemCrashDumpStateInformation = 34,
	SystemKernelDebuggerInformation = 35,
	SystemContextSwitchInformation = 36,
	SystemRegistryQuotaInformation = 37,
	SystemExtendServiceTableInformation = 38,
	SystemPrioritySeperation = 39,
	SystemVerifierAddDriverInformation = 40,
	SystemVerifierRemoveDriverInformation = 41,
	SystemProcessorIdleInformation = 42,
	SystemLegacyDriverInformation = 43,
	SystemCurrentTimeZoneInformation = 44,
	SystemLookasideInformation = 45,
	SystemTimeSlipNotification = 46,
	SystemSessionCreate = 47,
	SystemSessionDetach = 48,
	SystemSessionInformation = 49,
	SystemRangeStartInformation = 50,
	SystemVerifierInformation = 51,
	SystemVerifierThunkExtend = 52,
	SystemSessionProcessInformation = 53,
	SystemLoadGdiDriverInSystemSpace = 54,
	SystemNumaProcessorMap = 55,
	SystemPrefetcherInformation = 56,
	SystemExtendedProcessInformation = 57,
	SystemRecommendedSharedDataAlignment = 58,
	SystemComPlusPackage = 59,
	SystemNumaAvailableMemory = 60,
	SystemProcessorPowerInformation = 61,
	SystemEmulationBasicInformation = 62,
	SystemEmulationProcessorInformation = 63,
	SystemExtendedHandleInformation = 64,
	SystemLostDelayedWriteInformation = 65,
	SystemBigPoolInformation = 66,
	SystemSessionPoolTagInformation = 67,
	SystemSessionMappedViewInformation = 68,
	SystemHotpatchInformation = 69,
	SystemObjectSecurityMode = 70,
	SystemWatchdogTimerHandler = 71,
	SystemWatchdogTimerInformation = 72,
	SystemLogicalProcessorInformation = 73,
	SystemWow64SharedInformation = 74,
	SystemRegisterFirmwareTableInformationHandler = 75,
	SystemFirmwareTableInformation = 76,
	SystemModuleInformationEx = 77,
	SystemVerifierTriageInformation = 78,
	SystemSuperfetchInformation = 79,
	SystemMemoryListInformation = 80,
	SystemFileCacheInformationEx = 81,
	MaxSystemInfoClass = 82  // MaxSystemInfoClass should always be the last enum
} SYSTEM_INFORMATION_CLASS;

PM_IMPORT NTSTATUS ZwQuerySystemInformation(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

BOOLEAN FindKernelModule(
	const char* targetModuleName,
	PSYSTEM_MODULE_ENTRY Entry
);

void SwitchAddressSpace(_In_ UINT64 NewCR3);

//
// Exported from `ntoskrnl.exe`, so if we want to use it, just declare it.
//
PM_IMPORT HAL_PRIVATE_DISPATCH_TABLE HalPrivateDispatchTable;


template <typename T>
class Vector {
private:
	T* Objects_;
	UINT32 Count_;
	UINT32 Capacity_;
	KSPIN_LOCK Spinlock_;

	void Initialize() {
		KIRQL Irql = EnterLock();
		if (!Objects_) {
			Capacity_ = sizeof(T) * 64;
			Objects_ = (T*)ExAllocatePool(POOL_TYPE::NonPagedPoolBase, Capacity_);

			ASSERT(Objects_);

			memset(Objects_, 0, Capacity_);
		}

		ExitLock(Irql);
	}

public:
	KIRQL EnterLock() {
		return KeAcquireSpinLockRaiseToDpc(&Spinlock_);
	}

	void ExitLock(_In_ KIRQL Irql) {
		KeReleaseSpinLock(&Spinlock_, Irql);
	}

	void Clear() {
		KIRQL Irql = EnterLock();
		if (Objects_ && Count_) {
			memset(Objects_, 0, sizeof(T) * Count_);
			Count_ = 0;
		}
		ExitLock(Irql);
	}

	void Destory() {
		if (!Objects_) {
			return;
		}

		KIRQL Irql = EnterLock();

		memset(Objects_, 0, Count_ * sizeof(T));

		ExFreePool(Objects_);

		Count_ = 0;
		Capacity_ = 0;
		ExitLock(Irql);
	}

	void Insert(_In_ T Item) {
		if (!Objects_) {
			Initialize();
		}

		KIRQL Irql = EnterLock();

		//
		// Reallocate the buffer
		//
		if (sizeof(T) + Count_ * sizeof(T) > Capacity_) {
			Capacity_ *= 2;
			T* NewArray = (T*)ExAllocatePool(POOL_TYPE::NonPagedPoolNx, Capacity_);

			ASSERT(!NewArray);

			memset(&NewArray[Count_], 0, Capacity_ - (sizeof(T) * Count_));

			memcpy(NewArray, Objects_, Count_ * sizeof(T));

			ExFreePool(Objects_);
			Objects_ = NewArray;
		}

		Objects_[Count_++] = Item;
		ExitLock(Irql);
	}

	bool Contains(_In_ T Item)
	{
		if (!Objects_)
		{
			Initialize();
			return false;
		}

		KIRQL Irql = EnterLock();
		for (UINT32 i = 0; i < Count_; i++)
		{
			if (Objects_[i] == Item)
			{
				ExitLock(Irql);
				return true;
			}
		}

		ExitLock(Irql);
		return false;
	}

	T operator[](UINT32 i)
	{
		if (!Objects_)
		{
			Initialize();
			return {};
		}

		ASSERT(i > Count_);
		return Objects_[i];
	}

	UINT32 Size()
	{
		return Count_;
	}
};

extern Vector<void*> MemoryAllocations;