#include "burnint.h"

// A hiscore.dat support module for FB Neo - written by Barry Manilow, Feb 2009
// Updates & fixes by dink and iq_132

#define MAX_CONFIG_LINE_SIZE 		48

#define HISCORE_MAX_RANGES		64

UINT32 nHiscoreNumRanges;

#define APPLIED_STATE_NONE		0
#define APPLIED_STATE_ATTEMPTED		1
#define APPLIED_STATE_CONFIRMED		2

struct _HiscoreMemRange
{
	UINT32 Loaded, nCpu, Address, NumBytes, StartValue, EndValue, ApplyNextFrame, Applied;
	UINT32 NoConfirm; // avoid confirm check / re-apply area if it fails check
	UINT8 *Data;
};

_HiscoreMemRange HiscoreMemRange[HISCORE_MAX_RANGES];

INT32 EnableHiscores;
static INT32 HiscoresInUse;
static INT32 WriteCheck1;
static INT32 LetsTryToApply = 0;
static INT32 nLoadingFrameDelay = 0;

static cheat_core *cheat_ptr;
static cpu_core_config *cheat_subptr;
extern cheat_core *GetCpuCheatRegister(INT32 nCPU);

static inline void cpu_open(INT32 nCpu)
{
	cheat_ptr = GetCpuCheatRegister(nCpu);
	cheat_subptr = cheat_ptr->cpuconfig;
	cheat_subptr->open(cheat_ptr->nCPU);
}

static UINT32 hexstr2num (const char **pString)
{
	const char *string = *pString;
	UINT32 result = 0;
	if (string)
	{
		for(;;)
		{
			char c = *string++;
			INT32 digit;

			if (c>='0' && c<='9')
			{
				digit = c-'0';
			}
			else if (c>='a' && c<='f')
			{
				digit = 10+c-'a';
			}
			else if (c>='A' && c<='F')
			{
				digit = 10+c-'A';
			}
			else
			{
				if (!c) string = NULL;
				break;
			}
			result = result*16 + digit;
		}
		*pString = string;
	}
	return result;
}

static INT32 is_mem_range (const char *pBuf)
{
	char c;
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ':') break;
	}
	c = *pBuf; /* character following first ':' */

	return	(c>='0' && c<='9') ||
			(c>='a' && c<='f') ||
			(c>='A' && c<='F');
}

static INT32 is_mem_range_new (const char *pBuf)
{
	char c;
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ':') break;
	}
	c = *pBuf; /* character following first ':' */
	
	// cpu id - ignore for now
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ',') break;
	}
	c = *pBuf; /* character following first ',' */
	
	// address space - ignore for now
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ',') break;
	}
	c = *pBuf; /* character following second ',' */
	
	return	(c>='0' && c<='9') ||
			(c>='a' && c<='f') ||
			(c>='A' && c<='F');
}

static INT32 cpustr2num(char *pCpu)
{ // all known versions of the first cpu as of May 15, 2017
	return (strstr(pCpu, "maincpu") ||
		    strstr(pCpu, "cpu1") ||
		    strstr(pCpu, "alpha") ||
			strstr(pCpu, "master") ||
			strstr(pCpu, "fgcpu") ||
			strstr(pCpu, "cpua") ||
			strstr(pCpu, "master_cpu")) ? 0 : 1;
}

static INT32 matching_game_name (const char *pBuf, const char *name)
{
	while (*name)
	{
		if (*name++ != *pBuf++) return 0;
	}
	return (*pBuf == ':');
}

static INT32 CheckHiscoreAllowed()
{
	INT32 Allowed = 1;
	
	if (!EnableHiscores) Allowed = 0;
	if (!(BurnDrvGetFlags() & BDF_HISCORE_SUPPORTED)) Allowed = 0;
	
	return Allowed;
}

void HiscoreSearch_internal(FILE *fp, const char *name)
{
	char buffer[MAX_CONFIG_LINE_SIZE];
	enum { FIND_NAME, FIND_DATA, FETCH_DATA } mode;
	mode = FIND_NAME;

	while (fgets(buffer, MAX_CONFIG_LINE_SIZE, fp)) {
		if (mode == FIND_NAME) {
			if (matching_game_name(buffer, name)) {
				mode = FIND_DATA;
			}
		} else {
			if (buffer[0] == '@' && buffer[1] == ':') {
				if (is_mem_range_new(buffer)) {
					if (nHiscoreNumRanges < HISCORE_MAX_RANGES) {
						const char *pBuf = buffer;
						char cCpu[80];
						char *pCpu = &cCpu[0];

						// increment pBuf to the address
						char c;
						
						for (;;) {
							c = *pBuf++;
							if (c == ':') break;
						}
						
						c = *pBuf; /* character following first ':' */

						// cpu id -> cCpu
						for(;;)
						{
							c = *pBuf++;
							if (c == ',') { *pCpu++ = '\0'; break; }
							else *pCpu++ = c;
						}
						c = *pBuf; /* character following first ',' */
						
						// address space - ignore for now
						for(;;)
						{
							c = *pBuf++;
							if (c == ',') break;
						}
						c = *pBuf; /* character following second ',' */

						// now set the high score data
						HiscoreMemRange[nHiscoreNumRanges].Loaded = 0;
						HiscoreMemRange[nHiscoreNumRanges].nCpu = cpustr2num(&cCpu[0]);
						HiscoreMemRange[nHiscoreNumRanges].Address = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].NumBytes = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].StartValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].EndValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].ApplyNextFrame = 0;
						HiscoreMemRange[nHiscoreNumRanges].Applied = 0;
						HiscoreMemRange[nHiscoreNumRanges].Data = (UINT8*)BurnMalloc(HiscoreMemRange[nHiscoreNumRanges].NumBytes);
						memset(HiscoreMemRange[nHiscoreNumRanges].Data, 0, HiscoreMemRange[nHiscoreNumRanges].NumBytes);

						// NoConfirm "feature"

						// TODO: If another game needs this (someday?), we'll make a callback from the driver-side
						// to keep kludges from piling up in here

						// dbreed @ 88959
						// the last address range in hiscore.dat is the game's animation timer/
						// this area will never write-confirm because it's always changing.  We'll only
						// confirm the StartValue/EndValue in memory @ bootup, but not confirm that the value has stuck
						// a frame after it has been written, since this is impossible.

						if (!strcmp(name, "dbreed") && HiscoreMemRange[nHiscoreNumRanges].Address == 0x88959) {
							bprintf(0, _T("-- dbreed noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}

						if (!strcmp(name, "quantum") && HiscoreMemRange[nHiscoreNumRanges].Address == 0x1b5aa) {
							bprintf(0, _T("-- dbreed noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}

						// same thing, with ledstorm, ledstorm 2011[u,p] & madgear[j]
						if ((!strcmp(name, "leds2011") || !strcmp(name, "leds2011u") || !strcmp(name, "leds2011p") || !strcmp(name, "ledstorm") || !strcmp(name, "madgear") || !strcmp(name, "madgearj")) && HiscoreMemRange[nHiscoreNumRanges].Address == 0xff87c9) {
							bprintf(0, _T("-- leds2011 noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}

						// gradius2/vulcan venture
						if ((!strcmp(name, "gradius2") || !strcmp(name, "gradius2a") || !strcmp(name, "gradius2b") || !strcmp(name, "vulcan") || !strcmp(name, "vulcana") || !strcmp(name, "vulcanb"))
							&& HiscoreMemRange[nHiscoreNumRanges].Address == 0x60008) {
							bprintf(0, _T("-- gradius2/vulcan noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}

						// mhavoc (same comment as above, 0x0095 is a timer)
						if (!strcmp(name, "mhavoc") && HiscoreMemRange[nHiscoreNumRanges].Address == 0x0095) {
							bprintf(0, _T("-- mhavoc noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}

						// amspdwy e3de block is suspect -dink
						if (!strcmp(name, "amspdwy") && HiscoreMemRange[nHiscoreNumRanges].Address == 0xe3de) {
							bprintf(0, _T("-- amspdwy noConfirm hack for address range %x\n"), HiscoreMemRange[nHiscoreNumRanges].Address);
							HiscoreMemRange[nHiscoreNumRanges].NoConfirm = 1;
						}
						// end NoConfirm "feature"

#if 1 && defined FBNEO_DEBUG
						bprintf(PRINT_IMPORTANT, _T("Hi Score Memory Range %i Loaded (New Format) - CPU %i (%S), Address %x, Bytes %02x, Start Val %x, End Val %x\n"), nHiscoreNumRanges, HiscoreMemRange[nHiscoreNumRanges].nCpu, cCpu, HiscoreMemRange[nHiscoreNumRanges].Address, HiscoreMemRange[nHiscoreNumRanges].NumBytes, HiscoreMemRange[nHiscoreNumRanges].StartValue, HiscoreMemRange[nHiscoreNumRanges].EndValue);
#endif
						
						nHiscoreNumRanges++;
						
						mode = FETCH_DATA;
					} else {
						break;
					}
				} else {
					if (mode == FETCH_DATA) break;
				}
			} else if (strncmp("@delay=", buffer, 7) == 0) {
				nLoadingFrameDelay = atof(buffer+7) * (nBurnFPS / 100.0);

#if 1 && defined FBNEO_DEBUG
				bprintf(PRINT_IMPORTANT, _T("Hi Score loading should be delayed by %d frames ?\n"), nLoadingFrameDelay);
#endif
			} else {
				if (is_mem_range(buffer)) {
					if (nHiscoreNumRanges < HISCORE_MAX_RANGES) {
						const char *pBuf = buffer;
					
						HiscoreMemRange[nHiscoreNumRanges].Loaded = 0;
						HiscoreMemRange[nHiscoreNumRanges].nCpu = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].Address = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].NumBytes = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].StartValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].EndValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].ApplyNextFrame = 0;
						HiscoreMemRange[nHiscoreNumRanges].Applied = 0;
						HiscoreMemRange[nHiscoreNumRanges].Data = (UINT8*)BurnMalloc(HiscoreMemRange[nHiscoreNumRanges].NumBytes);
						memset(HiscoreMemRange[nHiscoreNumRanges].Data, 0, HiscoreMemRange[nHiscoreNumRanges].NumBytes);
					
#if 1 && defined FBNEO_DEBUG
						bprintf(PRINT_IMPORTANT, _T("Hi Score Memory Range %i Loaded - CPU %i, Address %x, Bytes %02x, Start Val %x, End Val %x\n"), nHiscoreNumRanges, HiscoreMemRange[nHiscoreNumRanges].nCpu, HiscoreMemRange[nHiscoreNumRanges].Address, HiscoreMemRange[nHiscoreNumRanges].NumBytes, HiscoreMemRange[nHiscoreNumRanges].StartValue, HiscoreMemRange[nHiscoreNumRanges].EndValue);
#endif
					
						nHiscoreNumRanges++;
					
						mode = FETCH_DATA;
					} else {
						break;
					}
				} else {
					if (mode == FETCH_DATA) break;
				}
			}
		}
	}
}

// Sometimes we have different setnames than other emu's, the table below
// will translate our set to their set to keep hiscore.dat happy
struct game_replace_entry {
	char fb_name[80];
	char mame_name[80];
};

static game_replace_entry replace_table[] = {
	{ "vsraidbbay",		"bnglngby"		},
	{ "vsrbibbal",		"rbibb"			},
	{ "vsbattlecity",	"btlecity"		},
	{ "vscastlevania",	"cstlevna"		},
	{ "vsclucluland",	"cluclu"		},
	{ "vsdrmario",		"drmario"		},
	{ "vsduckhunt",		"duckhunt"		},
	{ "vsexcitebike",	"excitebk"		},
	{ "vsfreedomforce",	"vsfdf"			},
	{ "vsgoonies",		"goonies"		},
	{ "vsgradius",		"vsgradus"		},
	{ "vsgumshoe",		"vsgshoe"		},
	{ "vshogansalley",	"hogalley"		},
	{ "vsiceclimber",	"iceclimb"		},
	{ "vsmachrider",	"nvs_machrider"	},
	{ "vsmightybomjack","nvs_mightybj"	},
	{ "vsninjajkun",	"jajamaru"		},
	{ "vspinball",		"vspinbal"		},
	{ "vsplatoon",		"nvs_platoon"	},
	{ "vsslalom",		"vsslalom"		},
	{ "vssoccer",		"vssoccer"		},
	{ "vsstarluster",	"starlstr"		},
	{ "vssmgolf",		"smgolf"		},
	{ "vssmgolfla",		"ladygolf"		},
	{ "vssmb",			"suprmrio"		},
	{ "vssuperskykid",	"vsskykid"		},
	{ "vssuperxevious",	"supxevs"		},
	{ "vstetris",		"vstetris"		},
	{ "vstkoboxing",	"tkoboxng"		},
	{ "vstopgun",		"topgun"		},
	{ "\0", 			"\0"			}
};

void HiscoreSearch(FILE *fp, const char *name)
{
	const char *game = name; // default to passed name

	// Check the above table to see if we should use an alias
	for (INT32 i = 0; replace_table[i].fb_name[0] != '\0'; i++) {
		if (!strcmp(replace_table[i].fb_name, name)) {
			game = replace_table[i].mame_name;
			break;
		}
	}

	HiscoreSearch_internal(fp, game);
}

void HiscoreInit()
{
	Debug_HiscoreInitted = 1;

	if (!CheckHiscoreAllowed()) return;

	HiscoresInUse = 0;

	TCHAR szDatFilename[MAX_PATH];
	_stprintf(szDatFilename, _T("%shiscore.dat"), szAppHiscorePath);

	FILE *fp = _tfopen(szDatFilename, _T("r"));
	if (fp) {
		HiscoreSearch(fp, BurnDrvGetTextA(DRV_NAME));
		if (nHiscoreNumRanges) HiscoresInUse = 1;

		// no hiscore entry for this game in hiscore.dat, and the game is a clone (probably a hack)
		// let's try using parent entry as a fallback, the success rate seems reasonably good
		if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetTextA(DRV_PARENT) && HiscoresInUse == 0) {
			fseek(fp, 0, SEEK_SET);
			HiscoreSearch(fp, BurnDrvGetTextA(DRV_PARENT));
			if (nHiscoreNumRanges) HiscoresInUse = 1;
		}

		fclose(fp);
	}

	TCHAR szFilename[MAX_PATH];
#ifndef __LIBRETRO__
 	_stprintf(szFilename, _T("%s%s.hi"), szAppHiscorePath, BurnDrvGetText(DRV_NAME));
#else
	_stprintf(szFilename, _T("%s%s.hi"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
#endif

	fp = _tfopen(szFilename, _T("rb"));
	INT32 Offset = 0;
	if (fp) {
		UINT32 nSize = 0;
		
		while (!feof(fp)) {
			fgetc(fp);
			nSize++;
		}
		
		UINT8 *Buffer = (UINT8*)BurnMalloc(nSize);
		fseek(fp, 0, SEEK_SET);

		fread((char *)Buffer, 1, nSize, fp);

		for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
			for (UINT32 j = 0; j < HiscoreMemRange[i].NumBytes; j++) {
				HiscoreMemRange[i].Data[j] = Buffer[j + Offset];
			}
			Offset += HiscoreMemRange[i].NumBytes;
			
			HiscoreMemRange[i].Loaded = 1;
			
#if 1 && defined FBNEO_DEBUG
			bprintf(PRINT_IMPORTANT, _T("Hi Score Memory Range %i Loaded from file\n"), i);
#endif
		}
		
		BurnFree(Buffer);

		fclose(fp);
	}

	WriteCheck1 = 0;
}

void HiscoreReset(INT32 bDisableInversionWriteback)
{
#if defined FBNEO_DEBUG
	if (!Debug_HiscoreInitted) bprintf(PRINT_ERROR, _T("HiscoreReset called without init\n"));
#endif

	if (!CheckHiscoreAllowed() || !HiscoresInUse) return;

	WriteCheck1 = 0;

	LetsTryToApply = 0;

	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		HiscoreMemRange[i].ApplyNextFrame = 0;
		HiscoreMemRange[i].Applied = APPLIED_STATE_NONE;
		
		if (HiscoreMemRange[i].Loaded) {
			cpu_open(HiscoreMemRange[i].nCpu);
			// in some games, system16b (aliensyn, everything else) rom is mapped (for cheats)
			// on reset where the hiscore should be.  Writing here is bad.
			if (bDisableInversionWriteback == 0) {
				cheat_subptr->write(HiscoreMemRange[i].Address, (UINT8)~HiscoreMemRange[i].StartValue);
				if (HiscoreMemRange[i].NumBytes > 1) cheat_subptr->write(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1, (UINT8)~HiscoreMemRange[i].EndValue);
			}
			cheat_subptr->close();
			
#if 1 && defined FBNEO_DEBUG
			bprintf(PRINT_IMPORTANT, _T("Hi Score Memory Range %i Initted\n"), i);
#endif
		}
	}
}

INT32 HiscoreOkToWrite()
{ // Check if it is ok to write the hiscore data aka. did we apply it at least?
	INT32 Ok = 1;

	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		if (!(HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_CONFIRMED)) {
			Ok = 0;
		}
	}

#if 1 && defined FBNEO_DEBUG
	bprintf(0, _T("Hiscore Write-Check #1 - Applied data: %X\n"), Ok);
#endif

	if (Ok)
		return 1; // Passed check #1 - already applied hiscore?

	// Check #2 - didn't apply high score, but verified the memory locations
#if 1 && defined FBNEO_DEBUG
	bprintf(0, _T("Hiscore Write-Check #2 - Memory verified: %X\n"), WriteCheck1);
#endif

	return WriteCheck1;
}

INT32 HiscoreOkToApply(INT32 i)
{
	if (!(HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_NONE && HiscoreMemRange[i].ApplyNextFrame)) {
		return 0;
	}

	return 1;
}

INT32 HiscoreOkToApplyAll()
{ // All of the memory locations in the game's entry must be verfied, then applied when they _all_ match up
	INT32 Ok = 1;

	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		if (!HiscoreOkToApply(i)) {
			Ok = 0;
		}
	}

	return Ok;
}

void HiscoreApply()
{
#if defined FBNEO_DEBUG
	if (!Debug_HiscoreInitted) bprintf(PRINT_ERROR, _T("HiscoreApply called without init\n"));
#endif

	if (!CheckHiscoreAllowed() || !HiscoresInUse || bBurnRunAheadFrame) return;

	UINT8 WriteCheckOk = 0;
	
	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		if (HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_ATTEMPTED) {
			INT32 Confirmed = 1;

			cpu_open(HiscoreMemRange[i].nCpu);

			for (UINT32 j = 0; j < HiscoreMemRange[i].NumBytes; j++) {
				if (cheat_subptr->read(HiscoreMemRange[i].Address + j) != HiscoreMemRange[i].Data[j]) {
					Confirmed = 0;
				}
			}
			cheat_subptr->close();
			
			if (Confirmed == 1 || HiscoreMemRange[i].NoConfirm) {
				HiscoreMemRange[i].Applied = APPLIED_STATE_CONFIRMED;
#if 1 && defined FBNEO_DEBUG
				bprintf(PRINT_IMPORTANT, _T("Applied Hi Score Memory Range %i on frame number %i %s\n"), i, GetCurrentFrame(), (HiscoreMemRange[i].NoConfirm) ? _T("(no confirm)") : _T(""));
#endif
			} else {
				HiscoreMemRange[i].Applied = APPLIED_STATE_NONE;
				HiscoreMemRange[i].ApplyNextFrame = 1; // Can't apply yet (machine still booting?) - let's try again!
#if 1 && defined FBNEO_DEBUG
				bprintf(PRINT_IMPORTANT, _T("Failed attempt to apply Hi Score Memory Range %i on frame number %i\n"), i, GetCurrentFrame());
#endif
			}
		}

#if 0
		// Save for debugging hiscore.dat / driver problems.
		cpu_open(HiscoreMemRange[i].nCpu);
		bprintf(0, _T("start: addr %x   %x  %x\n"), HiscoreMemRange[i].Address, cheat_subptr->read(HiscoreMemRange[i].Address), HiscoreMemRange[i].StartValue);
		bprintf(0, _T(" end : addr %x   %x  %x\n"), HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1, cheat_subptr->read(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1), HiscoreMemRange[i].EndValue);
		cheat_subptr->close();
#endif
		if (HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_NONE) {
			cpu_open(HiscoreMemRange[i].nCpu);

			if (cheat_subptr->read(HiscoreMemRange[i].Address) == HiscoreMemRange[i].StartValue && cheat_subptr->read(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1) == HiscoreMemRange[i].EndValue) {
				HiscoreMemRange[i].ApplyNextFrame = 1;
			}
			cheat_subptr->close();
		}

		if (!HiscoreMemRange[i].Loaded && !WriteCheck1) {
			cpu_open(HiscoreMemRange[i].nCpu);
			if (cheat_subptr->read(HiscoreMemRange[i].Address) == HiscoreMemRange[i].StartValue && cheat_subptr->read(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1) == HiscoreMemRange[i].EndValue) {
				WriteCheckOk++;
			}
			cheat_subptr->close();
		}
	}

#if 0
	// Save for debugging hiscore.dat / driver problems.
	bprintf(0, _T("WriteCheckOK %d  nHiscoreNumRanges %d\n"), WriteCheckOk, nHiscoreNumRanges);
#endif

	if (WriteCheckOk == nHiscoreNumRanges) {
#if 1 && defined FBNEO_DEBUG
		bprintf(0, _T("Memory Verified - OK to write Hiscore data!\n"));
#endif
		WriteCheck1 = 1; // It's OK to write hi-score data for the first time.
	}

	if (HiscoreOkToApplyAll()) {
		// Game has booted - now we can attempt to apply the HS data
		LetsTryToApply = 1;
	}

	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		if (LetsTryToApply && HiscoreOkToApply(i)) {
			cpu_open(HiscoreMemRange[i].nCpu);
			for (UINT32 j = 0; j < HiscoreMemRange[i].NumBytes; j++) {
				cheat_subptr->write(HiscoreMemRange[i].Address + j, HiscoreMemRange[i].Data[j]);
			}
			cheat_subptr->close();

			HiscoreMemRange[i].Applied = APPLIED_STATE_ATTEMPTED;
			HiscoreMemRange[i].ApplyNextFrame = 0;
		}
	}
}

void HiscoreScan(INT32 nAction, INT32* pnMin)
{
	if (pnMin)
		*pnMin = 0x029743;

	for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
		SCAN_VAR(HiscoreMemRange[i]);
	}
}

void HiscoreExit()
{
#if defined FBNEO_DEBUG
	if (!Debug_HiscoreInitted) bprintf(PRINT_ERROR, _T("HiscoreExit called without init\n"));
#endif

	if (!CheckHiscoreAllowed() || !HiscoresInUse) {
		Debug_HiscoreInitted = 0;
		return;
	}

	if (HiscoreOkToWrite()) {
		TCHAR szFilename[MAX_PATH];
#ifndef __LIBRETRO__
 		_stprintf(szFilename, _T("%s%s.hi"), szAppHiscorePath, BurnDrvGetText(DRV_NAME));
#else
		_stprintf(szFilename, _T("%s%s.hi"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
#endif

		FILE *fp = _tfopen(szFilename, _T("wb"));
		if (fp) {
			for (UINT32 i = 0; i < nHiscoreNumRanges; i++) {
				UINT8 *Buffer = (UINT8*)BurnMalloc(HiscoreMemRange[i].NumBytes + 10);
				memset(Buffer, 0, HiscoreMemRange[i].NumBytes + 10);

				cpu_open(HiscoreMemRange[i].nCpu);
				for (UINT32 j = 0; j < HiscoreMemRange[i].NumBytes; j++) {
					Buffer[j] = cheat_subptr->read(HiscoreMemRange[i].Address + j);
				}
				cheat_subptr->close();

				fwrite(Buffer, 1, HiscoreMemRange[i].NumBytes, fp);

				BurnFree(Buffer);
			}
			fclose(fp);
		}
	} else {
#if 1 && defined FBNEO_DEBUG
		bprintf(0, _T("HiscoreExit(): -NOT- ok to write Hiscore data!\n"));
#endif
	}

	nHiscoreNumRanges = 0;
	WriteCheck1 = 0;

	for (UINT32 i = 0; i < HISCORE_MAX_RANGES; i++) {
		HiscoreMemRange[i].Loaded = 0;
		HiscoreMemRange[i].nCpu = 0;
		HiscoreMemRange[i].Address = 0;
		HiscoreMemRange[i].NumBytes = 0;
		HiscoreMemRange[i].StartValue = 0;
		HiscoreMemRange[i].EndValue = 0;
		HiscoreMemRange[i].ApplyNextFrame = 0;
		HiscoreMemRange[i].Applied = 0;

		BurnFree(HiscoreMemRange[i].Data);
	}

	Debug_HiscoreInitted = 0;
}
