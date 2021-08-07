#include "retro_common.h"
#include "retro_memory.h"

// Cheevos support
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

// Savestates support
static uint8_t *pStateWrite;
static const uint8_t *pStateRead;
static UINT32 nStateSize;

static int StateWriteAcb(BurnArea *pba)
{
	memcpy(pStateWrite, pba->Data, pba->nLen);
	pStateWrite += pba->nLen;
	return 0;
}

static int StateReadAcb(BurnArea *pba)
{
	memcpy(pba->Data, pStateRead, pba->nLen);
	pStateRead += pba->nLen;
	return 0;
}

static int StateSizeAcb(BurnArea *pba)
{
#ifdef FBNEO_DEBUG
	HandleMessage(RETRO_LOG_INFO, "state debug: name %s, len %d\n", pba->szName, pba->nLen);
#endif
	nStateSize += pba->nLen;
	return 0;
}

static INT32 LibretroAreaScan(INT32 nAction)
{
	// The following value is sometimes used in game logic (xmen6p, ...),
	// and will lead to various issues if not handled properly.
	// On standalone, this value is stored in savestate files headers,
	// and has special logic in runahead feature.
	SCAN_VAR(nCurrentFrame);

	BurnAreaScan(nAction, 0);

	return 0;
}

size_t retro_serialize_size()
{
	if (nBurnDrvActive == ~0U)
		return 0;

	// This is the size for retro_serialize, so it wants ACB_FULLSCAN | ACB_READ
	// retro_unserialize doesn't call this function
	INT32 nAction = ACB_FULLSCAN | ACB_READ;

	// Tweaking from context
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	if (kNetGame == 1) {
		// Hiscores are causing desync in netplay
		EnableHiscores = false;
		// Some data isn't required for netplay
		nAction |= ACB_NET_OPT;
	}

	// Don't try to cache state size, it's causing more issues than it solves (ngp)
	nStateSize = 0;
	BurnAcb = StateSizeAcb;

	LibretroAreaScan(nAction);

	return nStateSize;
}

bool retro_serialize(void *data, size_t size)
{
	if (nBurnDrvActive == ~0U)
		return true;

#if 0
	// Used to convert a standalone savestate we are loading into something usable by the libretro port
	char convert_save_path[MAX_PATH];
	snprintf_nowarn (convert_save_path, sizeof(convert_save_path), "%s%cfbneo%c%s.save", g_save_dir, PATH_DEFAULT_SLASH_C(), PATH_DEFAULT_SLASH_C(), BurnDrvGetTextA(DRV_NAME));
	BurnStateLoad(convert_save_path, 1, NULL);
#endif

	// We want ACB_FULLSCAN | ACB_READ for saving states
	INT32 nAction = ACB_FULLSCAN | ACB_READ;

	// Tweaking from context
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	if (kNetGame == 1) {
		// Hiscores are causing desync in netplay
		EnableHiscores = false;
		// Some data isn't required for netplay
		nAction |= ACB_NET_OPT;
	}

	BurnAcb = StateWriteAcb;
	pStateWrite = (uint8_t*)data;

	LibretroAreaScan(nAction);

	return true;
}

bool retro_unserialize(const void *data, size_t size)
{
	if (nBurnDrvActive == ~0U)
		return true;

	// We want ACB_FULLSCAN | ACB_WRITE for loading states
	INT32 nAction = ACB_FULLSCAN | ACB_WRITE;

	// Tweaking from context
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	if (kNetGame == 1) {
		// Hiscores are causing desync in netplay
		EnableHiscores = false;
		// Some data isn't required for netplay
		nAction |= ACB_NET_OPT;
	}

	BurnAcb = StateReadAcb;
	pStateRead = (const uint8_t*)data;

	LibretroAreaScan(nAction);

	// Some driver require to recalc palette after loading savestates
	BurnRecalcPal();

#if 0
	// Used to convert a libretro savestate we are loading into something usable by standalone
	char convert_save_path[MAX_PATH];
	snprintf_nowarn (convert_save_path, sizeof(convert_save_path), "%s%cfbneo%c%s.save", g_save_dir, PATH_DEFAULT_SLASH_C(), PATH_DEFAULT_SLASH_C(), BurnDrvGetTextA(DRV_NAME));
	BurnStateSave(convert_save_path, 1);
#endif

	return true;
}
