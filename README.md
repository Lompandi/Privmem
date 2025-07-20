[Demo](./media/Privmem.mp4)

### Implementation details

Windows uses `nt!SwapContext` to handle context switching during task scheduling. To monitor which threads are being scheduled — and determine whether the current thread is whitelisted — we need a reliable way to observe these context switches.

One approach is to hook into one of the indirect mechanisms triggered during a context switch, such as `EtwTraceContextSwap` or `KiClearLastBranchRecordStack`. Reverse engineering the context switch logic reveals the following structure:

```c++
if (KiCpuTracingFlags) {
  if ((PerfGlobalGroupMask & 4) == 0)
    EtwTraceContextSwap(OldThread, NewThread);

  if ((KiCpuTracingFlags & 2) == 0) {
    KiClearLastBranchRecordStack();
  }

  if ((KiCpuTracingFlags & 4) == 0) {
    KiResetProcessorTraceBuffer();
  }
}
```

Focusing on KiClearLastBranchRecordStack, we find that it simply calls a function pointer:

```c++
__int64 KiClearLastBranchRecordStack()
{
  return off_140C01DA0[0]();
}
```

Here, off_140C01DA0 resides at `nt!HalPrivateDispatchTable + 0x400`. This structure is exported from `ntoskrnl.exe` and resides in the .data section, meaning it's writable in memory and — crucially — not protected by PatchGuard.

By overwriting this function pointer with our custom handler, we can safely intercept every context switch and check whether the scheduled thread is whitelisted. This technique provides a powerful, PatchGuard-safe method for selectively modifying execution environments on a per-thread basis.

During a context switch, if the thread being scheduled is whitelisted, we modify its CR3 register to point to a custom page table. In this table, we insert a crafted PML4 entry that maps to our shadow address space — enabling controlled access to the shadow memory region.
Therefor allowing only our permitted thread to be able to access the memory region.
