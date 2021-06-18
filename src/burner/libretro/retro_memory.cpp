// Cheevos support
#include "retro_common.h"
#include "retro_memory.h"

static void* pMainRamData = NULL;
static size_t nMainRamSize = 0;
static bool bMainRamFound = false;
static int nMemoryCount = 0;
static struct retro_memory_descriptor sMemoryDescriptors[10] = {};
static bool bMemoryMapFound = false;

static int StateGetMainRamAcb(BurnArea *pba)
{
	if(!pba->szName)
		return 0;

	switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
	{
		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
		case HARDWARE_CAPCOM_CPS2:
			if (strcmp(pba->szName, "CpsRamFF") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_CAPCOM_CPS3:
			if (strcmp(pba->szName, "Main RAM") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_SNK_NEOGEO:
		case HARDWARE_IGS_PGM:
			if (strcmp(pba->szName, "68K RAM") == 0) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_PSIKYO:
			// Psikyo (psikyosh and psikyo4 uses "All RAM")
			if ((strcmp(pba->szName, "All RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_CAVE_68K_Z80:
		case HARDWARE_CAVE_68K_ONLY:
			// Cave (gaia driver uses "68K RAM")
			if ((strcmp(pba->szName, "RAM") == 0) || (strcmp(pba->szName, "68K RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_SEGA_MEGADRIVE:
			if ((strcmp(pba->szName, "RAM") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00FF0000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "NV RAM") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SAVE_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00000000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_SEGA_MASTER_SYSTEM:
		case HARDWARE_SEGA_GAME_GEAR:
			if ((strcmp(pba->szName, "sms") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
		case HARDWARE_NES:
		case HARDWARE_FDS:
			if ((strcmp(pba->szName, "CPU Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00000000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "Work Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00006000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		case HARDWARE_SNK_NGP:
		case HARDWARE_SNK_NGPC:
			if ((strcmp(pba->szName, "Main Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00004000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			if ((strcmp(pba->szName, "Shared Ram") == 0)) {
				sMemoryDescriptors[nMemoryCount].flags     = RETRO_MEMDESC_SYSTEM_RAM;
				sMemoryDescriptors[nMemoryCount].ptr       = pba->Data;
				sMemoryDescriptors[nMemoryCount].start     = 0x00007000;
				sMemoryDescriptors[nMemoryCount].len       = pba->nLen;
				sMemoryDescriptors[nMemoryCount].select    = 0;
				sMemoryDescriptors[nMemoryCount].addrspace = pba->szName;
				bMemoryMapFound = true;
				nMemoryCount++;
			}
			return 0;
		default:
			// For all other systems (?), main ram seems to be identified by either "All Ram" or "All RAM"
			if ((strcmp(pba->szName, "All Ram") == 0) || (strcmp(pba->szName, "All RAM") == 0)) {
				pMainRamData = pba->Data;
				nMainRamSize = pba->nLen;
				bMainRamFound = true;
			}
			return 0;
	}
}

void CheevosInit()
{
	INT32 nMin = 0;
	BurnAcb = StateGetMainRamAcb;
	BurnAreaScan(ACB_FULLSCAN, &nMin);
	if (bMainRamFound) {
		HandleMessage(RETRO_LOG_INFO, "[Cheevos] System RAM set to %p, size is %zu\n", pMainRamData, nMainRamSize);
	}
	if (bMemoryMapFound) {
		struct retro_memory_map sMemoryMap = {};
		sMemoryMap.descriptors = sMemoryDescriptors;
		sMemoryMap.num_descriptors = nMemoryCount;
		environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &sMemoryMap);
	}
}

void CheevosExit()
{
	pMainRamData = NULL;
	nMainRamSize = 0;
	bMainRamFound = false;
	nMemoryCount = 0;
	memset(sMemoryDescriptors, 0, sizeof(sMemoryDescriptors));
	bMemoryMapFound = false;
}

void *retro_get_memory_data(unsigned id)
{
	return id == RETRO_MEMORY_SYSTEM_RAM ? pMainRamData : NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	return id == RETRO_MEMORY_SYSTEM_RAM ? nMainRamSize : 0;
}
