#include "retro_common.h"
#include "retro_memory.h"

// Cheevos support
static void* pMainRamData = NULL;
static size_t nMainRamSize = 0;
static bool bMainRamFound = false;
static int nMemoryCount = 0;
static struct retro_memory_descriptor sMemoryDescriptors[10] = {};
static bool bMemoryMapFound = false;

bool bLibretroSupportsSavestateContext = false;

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
static UINT8 *pStateBuffer;
static UINT32 nStateLen;
static UINT32 nStateTmpLen;

static int StateWriteAcb(BurnArea *pba)
{
	nStateTmpLen += pba->nLen;
	if (nStateTmpLen > nStateLen)
		return 1;
	memcpy(pStateBuffer, pba->Data, pba->nLen);
	pStateBuffer += pba->nLen;

	return 0;
}

static int StateReadAcb(BurnArea *pba)
{
	nStateTmpLen += pba->nLen;
	if (nStateTmpLen > nStateLen)
		return 1;
	memcpy(pba->Data, pStateBuffer, pba->nLen);
	pStateBuffer += pba->nLen;

	return 0;
}

static int StateLenAcb(BurnArea *pba)
{
	nStateLen += pba->nLen;
#ifdef FBNEO_DEBUG
	HandleMessage(RETRO_LOG_INFO, "state debug: name %s, len %d, total %d\n", pba->szName, pba->nLen, nStateLen);
#endif

	return 0;
}

static INT32 LibretroAreaScan(INT32 nAction)
{
	nStateTmpLen = 0;

	// The following value is sometimes used in game logic (xmen6p, ...),
	// and will lead to various issues if not handled properly.
	// On standalone, this value is stored in savestate files headers
	// (and has special logic in runahead feature ?).
	// Due to core's logic, this value is increased at each frame iteration,
	// including multiple iterations of the same frame through runahead,
	// but it needs to stay synced between multiple iterations of a given frame
	SCAN_VAR(nCurrentFrame);

	BurnAreaScan(nAction, 0);

	return 0;
}

static void TweakScanFlags(INT32 &nAction)
{
	// note: due to the fact all hosts would require the same version of hiscore.dat to properly scan the same memory ranges,
	//       hiscores can't work reliably in a netplay environment, and we should never enable them in that context
	if (bLibretroSupportsSavestateContext)
	{
		int nSavestateContext = RETRO_SAVESTATE_CONTEXT_NORMAL;
		environ_cb(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &nSavestateContext);
		// With RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, we can guess accurately
		switch (nSavestateContext)
		{
			case RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_INSTANCE:
				nAction |= ACB_RUNAHEAD;
				break;
			case RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_BINARY:
				nAction |= ACB_2RUNAHEAD;
				break;
			case RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY:
				EnableHiscores = false;
				kNetGame = 1;
				nAction |= ACB_NET_OPT;
				break;
		}
	}
	else
	{
		// With RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, we can't guess accurately, so let's use the safest tweaks (netplay)
		int nAudioVideoEnable = -1;
		environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &nAudioVideoEnable);
		kNetGame = nAudioVideoEnable & 4 ? 1 : 0;
		if (kNetGame == 1)
		{
			EnableHiscores = false;
			nAction |= ACB_NET_OPT;
		}
	}
}

size_t retro_serialize_size()
{
	if (nBurnDrvActive == ~0U)
		return 0;

	// This is the size for retro_serialize, so it wants ACB_FULLSCAN | ACB_READ
	// retro_unserialize doesn't call this function
	INT32 nAction = ACB_FULLSCAN | ACB_READ;

	// Tweaking from context
	TweakScanFlags(nAction);

	// Store previous size
	INT32 nStateLenPrev = nStateLen;

	// Compute size
	nStateLen = 0;
	BurnAcb = StateLenAcb;
	LibretroAreaScan(nAction);

	// cv1k and ngp/ngpc need overallocation
	switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)
	{
		case HARDWARE_CAVE_CV1000:
		case HARDWARE_SNK_NGP:
		case HARDWARE_SNK_NGPC:
			nStateLen += (128*1024);
			break;
	}

	// The frontend doesn't handle it well when different savestates
	// with different sizes are used concurrently for runahead & rewind,
	// so we always keep the largest computed size
	if (nStateLenPrev > nStateLen)
		nStateLen = nStateLenPrev;

	return nStateLen;
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
	TweakScanFlags(nAction);

	BurnAcb = StateWriteAcb;
	pStateBuffer = (UINT8*)data;

	LibretroAreaScan(nAction);

	// size is bigger than expected
	if (nStateTmpLen > size)
		return false;

	return true;
}

bool retro_unserialize(const void *data, size_t size)
{
	if (nBurnDrvActive == ~0U)
		return true;

	// We want ACB_FULLSCAN | ACB_WRITE for loading states
	INT32 nAction = ACB_FULLSCAN | ACB_WRITE;

	// Tweaking from context
	TweakScanFlags(nAction);

	// second instance runahead never calls retro_serialize_size(),
	// but to avoid overflows nStateLen is required in this core's savestate logic,
	// so we use "size" to update nStateLen
	if (size > nStateLen)
		nStateLen = size;

	BurnAcb = StateReadAcb;
	pStateBuffer = (UINT8*)data;

	LibretroAreaScan(nAction);

	// size is bigger than expected
	if (nStateTmpLen > size)
		return false;

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
