// Cheevos support
#include "retro_memory.h"

void* MainRamData = NULL;
size_t MainRamSize = 0;
bool bMainRamFound = false;

int StateGetMainRamAcb(BurnArea *pba)
{
	int nHardwareCode = BurnDrvGetHardwareCode();

	if(!pba->szName)
		return 0;

	// Neogeo / PGM
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO
	 || (nHardwareCode & HARDWARE_PUBLIC_MASK) & HARDWARE_PREFIX_IGS_PGM) {
		if (strcmp(pba->szName, "68K RAM") == 0) {
			MainRamData = pba->Data;
			MainRamSize = pba->nLen;
			bMainRamFound = true;
			return 0;
		}
	}

	// Psikyo (psikyosh and psikyo4 uses "All RAM")
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) & HARDWARE_PREFIX_PSIKYO) {
		if ((strcmp(pba->szName, "All RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
			MainRamData = pba->Data;
			MainRamSize = pba->nLen;
			bMainRamFound = true;
			return 0;
		}
	}

	// Cave (gaia driver uses "68K RAM")
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) & HARDWARE_PREFIX_CAVE) {
		if ((strcmp(pba->szName, "RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
			MainRamData = pba->Data;
			MainRamSize = pba->nLen;
			bMainRamFound = true;
			return 0;
		}
	}

	// CPS1 / CPS2
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) & HARDWARE_PREFIX_CAPCOM) {
		if (strcmp(pba->szName, "CpsRamFF") == 0) {
			MainRamData = pba->Data;
			MainRamSize = pba->nLen;
			bMainRamFound = true;
			return 0;
		}
	}

	// CPS3
	if ((nHardwareCode & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS3) {
		if (strcmp(pba->szName, "Main RAM") == 0) {
			MainRamData = pba->Data;
			MainRamSize = pba->nLen;
			bMainRamFound = true;
			return 0;
		}
	}

	// For all other systems (?), main ram seems to be identified by either "All Ram" or "All RAM"
	if ((strcmp(pba->szName, "All Ram") == 0) || (strcmp(pba->szName, "All RAM") == 0)) {
		MainRamData = pba->Data;
		MainRamSize = pba->nLen;
		bMainRamFound = true;
		return 0;
	}

	return 0;
}

void *retro_get_memory_data(unsigned id)
{
	return id == RETRO_MEMORY_SYSTEM_RAM ? MainRamData : NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	return id == RETRO_MEMORY_SYSTEM_RAM ? MainRamSize : 0;
}
