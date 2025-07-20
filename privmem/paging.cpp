
#include "paging.hpp"

namespace Page {
	MMPTE_HARDWARE ShadowPML4;
	MMPTE_HARDWARE* ClonePML4Virt;
	MMPTE_HARDWARE* ClientPML4Virt;
	UINT64 CloneCR3Phys;
	int FreePML4Index;

	void CreateShadowRegion() {
		void* ShadowPage		= MmAllocateContiguousMemory(0x200000, PHYSICAL_ADDRESS{ .QuadPart = -1 });
		MMPTE_HARDWARE* PD		= (MMPTE_HARDWARE*)MmAllocateContiguousMemory(Page::Size, PHYSICAL_ADDRESS{ .QuadPart = -1 });
		MMPTE_HARDWARE* PDPT	= (MMPTE_HARDWARE*)MmAllocateContiguousMemory(Page::Size, PHYSICAL_ADDRESS{ .QuadPart = -1 });
		Page::ClonePML4Virt		= (MMPTE_HARDWARE*)MmAllocateContiguousMemory(Page::Size, PHYSICAL_ADDRESS{ .QuadPart = -1 });

		memset(ShadowPage, 0, 0x200000);
		memset(PD, 0, Page::Size);
		memset(PDPT, 0, Page::Size);
		memset(Page::ClonePML4Virt, 0, Page::Size);

		ConstructPD(PD, MmGetPhysicalAddress(ShadowPage).QuadPart, true, true);
		ConstructPDPT(PDPT, MmGetPhysicalAddress(PD).QuadPart, true);
		ConstructPML4(&ShadowPML4, MmGetPhysicalAddress(PDPT).QuadPart, true);

		Page::CloneCR3Phys = MmGetPhysicalAddress(Page::ClonePML4Virt).QuadPart;

		MemoryAllocations.Insert(ShadowPage);
		MemoryAllocations.Insert(PD);
		MemoryAllocations.Insert(PDPT);
		MemoryAllocations.Insert(Page::ClonePML4Virt);
	}

	void ConstructPML4(_In_ MMPTE_HARDWARE* PML4E, _In_ UINT64 Phys, _In_ bool Usermode) {
		//
		// First, reset all flags.
		//
		PML4E->Flags = 0;

		PML4E->u.UserAccessible = Usermode;
		PML4E->u.Write = true;
		PML4E->u.PageFrameNumber = Phys >> 12;
		PML4E->u.Present = true;
	}

	void ConstructPDPT(_In_ MMPTE_HARDWARE* PDPTE, _In_ UINT64 Phys, _In_ bool Usermode, _In_ bool LargePage) {
		PDPTE->Flags = 0;

		PDPTE->u.UserAccessible = Usermode;
		PDPTE->u.LargePage = LargePage;
		PDPTE->u.Write = true;
		LargePage ? PDPTE->u.PageFrameNumber = Phys >> 30 : PDPTE->u.PageFrameNumber = Phys >> 12;
		PDPTE->u.Present = true;
	}

	void ConstructPD(_In_ MMPTE_HARDWARE* PDE, _In_ UINT64 Phys, _In_ bool Usermode, _In_ bool LargePage) {
		PDE->Flags = 0;

		PDE->u.LargePage = LargePage;
		PDE->u.Write = true;
		PDE->u.UserAccessible = Usermode;
		LargePage ? PDE->u.PageFrameNumber = Phys >> 12 : PDE->u.PageFrameNumber = Phys >> 12;
		PDE->u.Present = true;
	}
}