#pragma once

#include "utils.hpp"

namespace Page {
	extern MMPTE_HARDWARE ShadowPML4;
	extern MMPTE_HARDWARE* ClonePML4Virt;
	extern MMPTE_HARDWARE* ClientPML4Virt;
	extern UINT64 CloneCR3Phys;
	extern int FreePML4Index;

	void CreateShadowRegion();

	void ConstructPML4(_In_ MMPTE_HARDWARE* PML4E, _In_ UINT64 Phys, _In_ bool Usermode);
	void ConstructPDPT(_In_ MMPTE_HARDWARE* PDPTE, _In_ UINT64 Phys, _In_ bool Usermode, _In_ bool LargePage = false);
	void ConstructPD(_In_ MMPTE_HARDWARE* PDE, _In_ UINT64 Phys, _In_ bool Usermode, _In_ bool LargePage = false);

	constexpr SIZE_T Size = 0x1000;
}