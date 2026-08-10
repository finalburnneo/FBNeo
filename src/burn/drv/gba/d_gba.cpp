// FBNeo Game Boy Advance driver

#include "burnint.h"
#include "gba.h"

static GbaCore*	Gba;
static UINT8*	DrvRom;
static UINT8*	DrvBios;
static UINT8*	DrvState;
static UINT8*	DrvBattery;
static INT32	DrvStateSize;
static INT32	DrvBatterySize;

static UINT8	DrvJoy[10];
static UINT8	DrvReset;
static UINT8	DrvRecalc;
static UINT16	DrvSolar;
static UINT8	DrvSolarUp;
static UINT8	DrvSolarDown;
static UINT8	DrvSolarUpLast;
static UINT8	DrvSolarDownLast;
static UINT16	DrvGyroZ;
static UINT16	DrvTiltX;
static UINT16	DrvTiltY;

#define GBA_BUTTON_INPUTS																	\
	{ "P1 Up",			BIT_DIGITAL,	DrvJoy + GBA_BUTTON_UP,		"p1 up"				},	\
	{ "P1 Down",		BIT_DIGITAL,	DrvJoy + GBA_BUTTON_DOWN,	"p1 down"			},	\
	{ "P1 Left",		BIT_DIGITAL,	DrvJoy + GBA_BUTTON_LEFT,	"p1 left"			},	\
	{ "P1 Right",		BIT_DIGITAL,	DrvJoy + GBA_BUTTON_RIGHT,	"p1 right"			},	\
	{ "P1 Button A",	BIT_DIGITAL,	DrvJoy + GBA_BUTTON_A,		"p1 fire 1"			},	\
	{ "P1 Button B",	BIT_DIGITAL,	DrvJoy + GBA_BUTTON_B,		"p1 fire 2"			},	\
	{ "P1 Button L",	BIT_DIGITAL,	DrvJoy + GBA_BUTTON_L,		"p1 fire 3"			},	\
	{ "P1 Button R",	BIT_DIGITAL,	DrvJoy + GBA_BUTTON_R,		"p1 fire 4"			},

#define GBA_RESET_INPUTS																	\
	{ "P1 Select",		BIT_DIGITAL,	DrvJoy + GBA_BUTTON_SELECT,	"p1 select"			},	\
	{ "P1 Start",		BIT_DIGITAL,	DrvJoy + GBA_BUTTON_START,	"p1 start"			},	\
	{ "Reset",			BIT_DIGITAL,	&DrvReset,					"reset"				},

#define GBA_SOLAR_INPUTS																	\
	{ "Solar Brighter",	BIT_DIGITAL,	&DrvSolarUp,				"p1 fire 5"			},	\
	{ "Solar Darker",	BIT_DIGITAL,	&DrvSolarDown,				"p1 fire 6"			},

#define GBA_GYRO_INPUTS																		\
	{ "Gyro Z",			BIT_ANALOG_ABS,	(UINT8*)&DrvGyroZ,			"p1 gyro z-axis"	},

#define GBA_TILT_INPUTS																		\
	{ "Tilt X",			BIT_ANALOG_ABS,	(UINT8*)&DrvTiltX,			"p1 tilt x-axis"	},	\
	{ "Tilt Y",			BIT_ANALOG_ABS,	(UINT8*)&DrvTiltY,			"p1 tilt y-axis"	},

static struct BurnInputInfo GbaInputList[] = {
	GBA_BUTTON_INPUTS
	GBA_RESET_INPUTS
};

STDINPUTINFO(Gba)

static struct BurnInputInfo SolarInputList[] = {	// Solar sensor
	GBA_BUTTON_INPUTS
	GBA_SOLAR_INPUTS
	GBA_RESET_INPUTS
};

STDINPUTINFO(Solar)

static struct BurnInputInfo GyroInputList[] = {		// Gyroscope
	GBA_BUTTON_INPUTS
	GBA_GYRO_INPUTS
	GBA_RESET_INPUTS
};

STDINPUTINFO(Gyro)

static struct BurnInputInfo TiltInputList[] = {		// Tilt sensor
	GBA_BUTTON_INPUTS
	GBA_TILT_INPUTS
	GBA_RESET_INPUTS
};

STDINPUTINFO(Tilt)

static struct BurnInputInfo AioInputList[] = {		// All-in-one
	GBA_BUTTON_INPUTS
	GBA_SOLAR_INPUTS
	GBA_GYRO_INPUTS
	GBA_TILT_INPUTS
	GBA_RESET_INPUTS
};

STDINPUTINFO(Aio)

#undef GBA_BUTTON_INPUTS
#undef GBA_SOLAR_INPUTS
#undef GBA_GYRO_INPUTS
#undef GBA_TILT_INPUTS
#undef GBA_RESET_INPUTS

static INT32 DrvDoReset()
{
	return GbaCoreReset(Gba);
}

static INT32 DrvInit()
{
	struct BurnRomInfo ri;
	memset(&ri, 0, sizeof(ri));
	if (BurnDrvGetRomInfo(&ri, 0) || ri.nLen == 0 || ri.nLen > 32 * 1024 * 1024)
		return 1;

	BurnSetRefreshRate(59.72750057);

	UINT32 romSize = ri.nLen;
	DrvRom = (UINT8 *)BurnMalloc((INT32)romSize);
	if (DrvRom == NULL)
		return 1;
	if (BurnLoadRom(DrvRom, 0, 1))
		return 1;
	if (GbaCoreInit(&Gba))
		return 1;

	memset(&ri, 0, sizeof(ri));
	if (BurnDrvGetRomInfo(&ri, 0x80) == 0 && ri.nType && ri.nLen == 0x4000) {
		DrvBios = (UINT8 *)BurnMalloc(0x4000);
		if (DrvBios && BurnLoadRom(DrvBios, 0x80, 1) == 0) {
			if (GbaCoreLoadBios(Gba, DrvBios, 0x4000))
				return 1;
		} else {
			BurnFree(DrvBios);
		}
	}

	tm localTime;
	memset(&localTime, 0, sizeof(localTime));
	BurnGetLocalTime(&localTime);
	GbaRtcSeed rtcSeed;
	rtcSeed.year    = (UINT16)(localTime.tm_year + 1900);
	rtcSeed.month   = (UINT8) (localTime.tm_mon + 1);
	rtcSeed.day     = (UINT8)  localTime.tm_mday;
	rtcSeed.weekday = (UINT8)  localTime.tm_wday;
	rtcSeed.hour    = (UINT8)  localTime.tm_hour;
	rtcSeed.minute  = (UINT8)  localTime.tm_min;
	rtcSeed.second  = (UINT8)  localTime.tm_sec;
	bprintf(PRINT_NORMAL, _T("GBA RTC seed: %04d-%02d-%02d %02d:%02d:%02d (wday %d)\n"),
		rtcSeed.year, rtcSeed.month, rtcSeed.day, rtcSeed.hour, rtcSeed.minute, rtcSeed.second, rtcSeed.weekday);
	if (GbaCoreLoadRom(Gba, DrvRom, romSize, &rtcSeed)) return 1;

	DrvStateSize = (INT32)GbaCoreStateSize();
	DrvState   = (UINT8*)BurnMalloc(DrvStateSize);
	DrvBattery = (UINT8*)BurnMalloc((INT32)GbaCoreGetBatteryCapacity());
	if (DrvState == NULL || DrvBattery == NULL)
		return 1;

	DrvBatterySize = (INT32)GbaCoreGetBatterySize(Gba);
	bprintf(PRINT_NORMAL, _T("GBA cartridge features: %08x, battery: %d\n"), GbaCoreGetCartridgeFeatures(Gba), DrvBatterySize);
	if (DrvBatterySize > 0) {
		memcpy(DrvBattery, GbaCoreGetBatteryDataConst(Gba), DrvBatterySize);
	}

	GbaCoreClearAudio(Gba);
	DrvRecalc        = 1;
	DrvSolar         = 0;
	DrvSolarUpLast   = 0;
	DrvSolarDownLast = 0;
	DrvGyroZ         = 0x8000;
	DrvTiltX         = 0x8000;
	DrvTiltY         = 0x8000;

	return 0;
}

static INT32 DrvExit()
{
	GbaCoreExit(&Gba);
	BurnFree(DrvRom);
	BurnFree(DrvBios);
	BurnFree(DrvState);
	BurnFree(DrvBattery);

	DrvStateSize     = 0;
	DrvBatterySize   = 0;
	DrvSolar         = 0;
	DrvSolarUpLast   = 0;
	DrvSolarDownLast = 0;
	DrvGyroZ         = 0x8000;
	DrvTiltX         = 0x8000;
	DrvTiltY         = 0x8000;

	return 0;
}

static double DrvAudioRate()
{
	return nBurnSoundLen * (GBA_MASTER_CLOCK / 280896.0);
}

static INT32 DrvDraw()
{
	const UINT32 *framebuffer = GbaCoreGetFramebuffer(Gba);
	if (framebuffer == NULL || pBurnDraw == NULL)
		return 0;

	for (INT32 y = 0; y < GBA_HEIGHT; y++) {
		UINT8 *dest = pBurnDraw + y * nBurnPitch;
		const UINT32 *source = framebuffer + y * GBA_WIDTH;
		for (INT32 x = 0; x < GBA_WIDTH; x++) {
			UINT32 pixel = source[x];
			PutPix(dest + x * nBurnBpp, BurnHighCol(pixel & 0xff, (pixel >> 8) & 0xff, (pixel >> 16) & 0xff, 0));
		}
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset && !bBurnRunAheadFrame && DrvDoReset()) return 1;

	GbaInput input;
	memset(&input, 0, sizeof(input));
	input.gyroZ = GbaMotionAxisToInput(DrvGyroZ);
	input.tiltX = GbaMotionAxisToInput(DrvTiltX);
	input.tiltY = GbaMotionAxisToInput(DrvTiltY);
	if (DrvSolarUp && !DrvSolarDown && !DrvSolarUpLast && DrvSolar < GBA_SOLAR_LEVEL_MAX) {
		DrvSolar++;
	} else if (DrvSolarDown && !DrvSolarUp && !DrvSolarDownLast && DrvSolar > 0) {
		DrvSolar--;
	}
	DrvSolarUpLast   = DrvSolarUp;
	DrvSolarDownLast = DrvSolarDown;
	input.solar = GbaSolarLevelToInput((UINT8)DrvSolar);
	for (INT32 i = 0; i < 10; i++) {
		input.buttons |= (DrvJoy[i] & 1) << i;
	}
	GbaCoreSetInput(Gba, &input);

	INT32 runAhead   = bBurnRunAheadFrame != 0;
	double audioRate = nBurnSoundLen > 0 ? DrvAudioRate() : 0.0;
	if (GbaCoreConfigureAudio(Gba, audioRate, nBurnSoundLen, !runAhead && nBurnSoundLen > 0))
		return 1;
	if (GbaCoreRunFrame(Gba))
		return 1;
	if (!runAhead && GbaCoreRenderAudio(Gba, pBurnSoundOut, nBurnSoundLen) != nBurnSoundLen)
		return 1;

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029698;
	}

	if ((nAction & ACB_VOLATILE) && Gba && DrvState) {
		SCAN_VAR(DrvSolar);
		if (nAction & ACB_WRITE) {
			if (DrvSolar > GBA_SOLAR_LEVEL_MAX)
				DrvSolar = GbaSolarLegacyToLevel(DrvSolar);
			DrvSolarUpLast   = DrvSolarUp;
			DrvSolarDownLast = DrvSolarDown;
			ScanVar(DrvState, DrvStateSize, "gba_machine");
			if (GbaCoreLoadState(Gba, DrvState, DrvStateSize, (nAction & ACB_RUNAHEAD) != 0))
				return 1;
		} else {
			if (GbaCoreSaveState(Gba, DrvState, DrvStateSize))
				return 1;
			ScanVar(DrvState, DrvStateSize, "gba_machine");
		}
	}

	if ((nAction & ACB_NVRAM) && Gba && DrvBattery && DrvBatterySize > 0) {
		if (nAction & ACB_WRITE) {
			ScanVar(DrvBattery, DrvBatterySize, "gba_battery");
			if (GbaCoreLoadBattery(Gba, DrvBattery, DrvBatterySize))
				return 1;
		} else {
			memcpy(DrvBattery, GbaCoreGetBatteryDataConst(Gba), DrvBatterySize);
			ScanVar(DrvBattery, DrvBatterySize, "gba_battery");
			if (!(nAction & ACB_RUNAHEAD))
				GbaCoreClearBatteryDirty(Gba);
		}
	}

	return 0;
}

static INT32 GbaGetZipName(char **pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char *pszGameName = NULL;

	if (pszName == NULL)
		return 1;

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
		pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	strncpy(szFilename, pszGameName, MAX_PATH - 1);
	szFilename[MAX_PATH - 1] = 0;
	if (strncmp(szFilename, "gba_", 4) == 0) {
		memmove(szFilename, szFilename + 4, strlen(szFilename + 4) + 1);
	}
	*pszName = szFilename;

	return 0;
}

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

static struct BurnRomInfo gba_gbaRomDesc[] = {
	{ "gba_bios.bin", 0x4000, 0x81977335, BRF_PRG | BRF_BIOS | BRF_OPT },
};

STD_ROM_PICK(gba_gba)
STD_ROM_FN(gba_gba)

struct BurnDriver BurnDrvgba_gba = {
	"gba_gba", NULL, NULL, NULL, "2001",
	"Game Boy Advance BIOS\0", "Optional BIOS, bundled replacement is used when absent", "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_GBA, GBF_BIOS, 0,
	GbaGetZipName, gba_gbaRomInfo, gba_gbaRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};


// =========================================================================================
//  Please use short names from the MAME softwarelist/gba set wherever possible.
//  Avoid excessively‑long titles that contradict the intent of the short‑name identifiers.
//  [Example] Fire Emblem : The Sacred Stones (USA, Australia)
//  ShortName: gba_firembssu
// =========================================================================================


// Classic NES Series: Super Mario Bros. (USA, Europe)

static struct BurnRomInfo gba_n_smbRomDesc[] = {
	{ "Classic NES Series - Super Mario Bros. (U, E)(2004)(Nintendo).gba",	0x0100000,	0xf7129225,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_smb, gba_n_smb, gba_gba)
STD_ROM_FN(gba_n_smb)

struct BurnDriver BurnDrvgba_n_smb = {
	"gba_n_smb", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Super Mario Bros. (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_smbRomInfo, gba_n_smbRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.01: Super Mario Bros. (Japan, Rev 1)

static struct BurnRomInfo gba_f_smbRomDesc[] = {
	{ "Famicom Mini Vol.01 - Super Mario Bros. (J, Rev 1)(2004)(Nintendo).gba",	0x0100000,	0xcd2604dd,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_smb, gba_f_smb, gba_gba)
STD_ROM_FN(gba_f_smb)

struct BurnDriver BurnDrvgba_f_smb = {
	"gba_f_smb", "gba_n_smb", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.01: Super Mario Bros. (Japan, Rev 1)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_smbRomInfo, gba_f_smbRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Donkey Kong (USA, Europe)

static struct BurnRomInfo gba_n_dkongRomDesc[] = {
	{ "Classic NES Series - Donkey Kong (U, E)(2004)(Nintendo).gba",	0x0100000,	0xf53d8b56,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_dkong, gba_n_dkong, gba_gba)
STD_ROM_FN(gba_n_dkong)

struct BurnDriver BurnDrvgba_n_dkong = {
	"gba_n_dkong", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Donkey Kong (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_dkongRomInfo, gba_n_dkongRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.02: Donkey Kong (Japan)

static struct BurnRomInfo gba_f_dkongRomDesc[] = {
	{ "Famicom Mini Vol.02 - Donkey Kong (J)(2004)(Nintendo).gba",	0x0100000,	0x071c3f2b,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_dkong, gba_f_dkong, gba_gba)
STD_ROM_FN(gba_f_dkong)

struct BurnDriver BurnDrvgba_f_dkong = {
	"gba_f_dkong", "gba_n_dkong", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.02: Donkey Kong (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_dkongRomInfo, gba_f_dkongRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Ice Climber (USA, Europe)

static struct BurnRomInfo gba_n_iceclmRomDesc[] = {
	{ "Classic NES Series - Ice Climber (U, E)(2004)(Nintendo).gba",	0x0100000,	0xb265538d,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_iceclm, gba_n_iceclm, gba_gba)
STD_ROM_FN(gba_n_iceclm)

struct BurnDriver BurnDrvgba_n_iceclm = {
	"gba_n_iceclm", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Ice Climber (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_iceclmRomInfo, gba_n_iceclmRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.03: Ice Climber (Japan)

static struct BurnRomInfo gba_f_iceclmRomDesc[] = {
	{ "Famicom Mini Vol.03 - Ice Climber (Japan)(2004)(Nintendo).gba",	0x0100000,	0xd0aef472,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_iceclm, gba_f_iceclm, gba_gba)
STD_ROM_FN(gba_f_iceclm)

struct BurnDriver BurnDrvgba_f_iceclm = {
	"gba_f_iceclm", "gba_n_iceclm", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.03: Ice Climber (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_iceclmRomInfo, gba_f_iceclmRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Excitebike (USA, Europe)

static struct BurnRomInfo gba_n_exbikeRomDesc[] = {
	{ "Classic NES Series - Excitebike (U, E)(2004)(Nintendo).gba",	0x0100000,	0x67d9a2a6,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_exbike, gba_n_exbike, gba_gba)
STD_ROM_FN(gba_n_exbike)

struct BurnDriver BurnDrvgba_n_exbike = {
	"gba_n_exbike", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Excitebike (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RACING, 0,
	GbaGetZipName, gba_n_exbikeRomInfo, gba_n_exbikeRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.04: Excitebike (Japan)

static struct BurnRomInfo gba_f_exbikeRomDesc[] = {
	{ "Famicom Mini Vol.04 - Excitebike (J)(2004)(Nintendo).gba",	0x0100000,	0x32604c95,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_exbike, gba_f_exbike, gba_gba)
STD_ROM_FN(gba_f_exbike)

struct BurnDriver BurnDrvgba_f_exbike = {
	"gba_f_exbike", "gba_n_exbike", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.04: Excitebike (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RACING, 0,
	GbaGetZipName, gba_f_exbikeRomInfo, gba_f_exbikeRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: The Legend of Zelda (USA, Europe)

static struct BurnRomInfo gba_n_zeldaRomDesc[] = {
	{ "Classic NES Series - The Legend of Zelda (U, E)(2004)(Nintendo).gba",	0x0100000,	0x6d49cabf,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_zelda, gba_n_zelda, gba_gba)
STD_ROM_FN(gba_n_zelda)

struct BurnDriver BurnDrvgba_n_zelda = {
	"gba_n_zelda", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: The Legend of Zelda (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_ADV, 0,
	GbaGetZipName, gba_n_zeldaRomInfo, gba_n_zeldaRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.05: Zelda no Densetsu 1: The Hyrule Fantasy (Japan)

static struct BurnRomInfo gba_f_zeldaRomDesc[] = {
	{ "Famicom Mini Vol.05 - Zelda no Densetsu 1 - The Hyrule Fantasy (J)(2004)(Nintendo).gba",	0x0100000,	0x712d76e8,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_zelda, gba_f_zelda, gba_gba)
STD_ROM_FN(gba_f_zelda)

struct BurnDriver BurnDrvgba_f_zelda = {
	"gba_f_zelda", "gba_n_zelda", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.05: Zelda no Densetsu 1: The Hyrule Fantasy (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.05: Zelda no Densetsu 1: The Hyrule Fantasy (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.05 \u30bc\u30eb\u30c0\u306e\u4f1d\u8aac 1 The Hyrule Fantasy\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_ADV, 0,
	GbaGetZipName, gba_f_zeldaRomInfo, gba_f_zeldaRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Pac-Man (USA, Europe)

static struct BurnRomInfo gba_n_pacmanRomDesc[] = {
	{ "Classic NES Series - Pac-Man (U, E)(2004)(Nintendo).gba",	0x0100000,	0xc28df82f,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_pacman, gba_n_pacman, gba_gba)
STD_ROM_FN(gba_n_pacman)

struct BurnDriver BurnDrvgba_n_pacman = {
	"gba_n_pacman", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Pac-Man (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_MAZE, 0,
	GbaGetZipName, gba_n_pacmanRomInfo, gba_n_pacmanRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.06: Pac-Man (Japan)

static struct BurnRomInfo gba_f_pacmanRomDesc[] = {
	{ "Famicom Mini Vol.06 - Pac-Man (J)(2004)(Nintendo).gba",	0x0100000,	0x91acf642,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_pacman, gba_f_pacman, gba_gba)
STD_ROM_FN(gba_f_pacman)

struct BurnDriver BurnDrvgba_f_pacman = {
	"gba_f_pacman", "gba_n_pacman", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.06: Pac-Man (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_MAZE, 0,
	GbaGetZipName, gba_f_pacmanRomInfo, gba_f_pacmanRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Xevious (USA, Europe)

static struct BurnRomInfo gba_n_xeviosRomDesc[] = {
	{ "Classic NES Series - Xevious (U, E)(2004)(Nintendo).gba",	0x0100000,	0x9cd2d5dd,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_xevios, gba_n_xevios, gba_gba)
STD_ROM_FN(gba_n_xevios)

struct BurnDriver BurnDrvgba_n_xevios = {
	"gba_n_xevios", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Xevious (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VERSHOOT, 0,
	GbaGetZipName, gba_n_xeviosRomInfo, gba_n_xeviosRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.07: Xevious (Japan)

static struct BurnRomInfo gba_f_xeviosRomDesc[] = {
	{ "Famicom Mini Vol.07 - Xevious (J)(2004)(Nintendo).gba",	0x0100000,	0xf54eeb0e,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_xevios, gba_f_xevios, gba_gba)
STD_ROM_FN(gba_f_xevios)

struct BurnDriver BurnDrvgba_f_xevios = {
	"gba_f_xevios", "gba_n_xevios", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.07: Xevious (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VERSHOOT, 0,
	GbaGetZipName, gba_f_xeviosRomInfo, gba_f_xeviosRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Dr. Mario (USA, Europe)

static struct BurnRomInfo gba_n_drmrioRomDesc[] = {
	{ "Classic NES Series - Dr. Mario (U)(2004)(Nintendo).gba",	0x0100000,	0x934e1f1d,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_drmrio, gba_n_drmrio, gba_gba)
STD_ROM_FN(gba_n_drmrio)

struct BurnDriver BurnDrvgba_n_drmrio = {
	"gba_n_drmrio", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Dr. Mario (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PUZZLE, 0,
	GbaGetZipName, gba_n_drmrioRomInfo, gba_n_drmrioRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.15: Dr. Mario (Japan)

static struct BurnRomInfo gba_f_drmrioRomDesc[] = {
	{ "Famicom Mini Vol.15 - Dr. Mario (J)(2004)(Nintendo).gba",	0x0100000,	0xc836f2e2,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_drmrio, gba_f_drmrio, gba_gba)
STD_ROM_FN(gba_f_drmrio)

struct BurnDriver BurnDrvgba_f_drmrio = {
	"gba_f_drmrio", "gba_n_drmrio", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.15: Dr. Mario (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.15: Dr. Mario (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.15 \u30c9\u30af\u30bf\u30fc\u30de\u30ea\u30aa\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PUZZLE, 0,
	GbaGetZipName, gba_f_drmrioRomInfo, gba_f_drmrioRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.22: Nazo no Murasame Jou (Japan)

static struct BurnRomInfo gba_f_murasaRomDesc[] = {
	{ "Famicom Mini Vol.22 - Nazo no Murasame Jou (J)(2004)(Nintendo).gba",	0x0400000,	0x8233349C,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_murasa, gba_f_murasa, gba_gba)
STD_ROM_FN(gba_f_murasa)

struct BurnDriver BurnDrvgba_f_murasa = {
	"gba_f_murasa", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.22: Nazo no Murasame Jou (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.22: Nazo no Murasame Jou (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.22 \u8b0e\u306e\u6751\u96e8\u57ce\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION, 0,
	GbaGetZipName, gba_f_murasaRomInfo, gba_f_murasaRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Metroid (USA, Europe)

static struct BurnRomInfo gba_n_metroiRomDesc[] = {
	{ "Classic NES Series - Metroid (U, E)(2004)(Nintendo).gba",	0x0100000,	0x9a243b9b,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_metroi, gba_n_metroi, gba_gba)
STD_ROM_FN(gba_n_metroi)

struct BurnDriver BurnDrvgba_n_metroi = {
	"gba_n_metroi", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Metroid (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ADV | GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_metroiRomInfo, gba_n_metroiRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.23: Metroid (Japan)

static struct BurnRomInfo gba_f_metroiRomDesc[] = {
	{ "Famicom Mini Vol.23 - Metroid (J)(2004)(Nintendo).gba",	0x0400000,	0xabeccdab,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_metroi, gba_f_metroi, gba_gba)
STD_ROM_FN(gba_f_metroi)

struct BurnDriver BurnDrvgba_f_metroi = {
	"gba_f_metroi", "gba_n_metroi", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.23: Metroid (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.23: Metroid (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.23 \u30e1\u30c8\u30ed\u30a4\u30c9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ADV | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_metroiRomInfo, gba_f_metroiRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Castlevania (USA)

static struct BurnRomInfo gba_n_cvaniauRomDesc[] = {
	{ "Classic NES Series - Castlevania (U)(2004)(Nintendo).gba",	0x0100000,	0x23e4082c,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_cvaniau, gba_n_cvaniau, gba_gba)
STD_ROM_FN(gba_n_cvaniau)

struct BurnDriver BurnDrvgba_n_cvaniau = {
	"gba_n_cvaniau", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Castlevania (USA)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_cvaniauRomInfo, gba_n_cvaniauRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.29: Akumajou Dracula (Japan)

static struct BurnRomInfo gba_f_akumajRomDesc[] = {
	{ "Famicom Mini Vol.29 - Akumajou Dracula (J)(2004)(Nintendo).gba",	0x0100000,	0x11419d8b,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_akumaj, gba_f_akumaj, gba_gba)
STD_ROM_FN(gba_f_akumaj)

struct BurnDriver BurnDrvgba_f_akumaj = {
	"gba_f_akumaj", "gba_n_cvaniau", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.29: Akumajou Dracula (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.29: Akumajou Dracula (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.29 \u60aa\u9b54\u57ce\u30c9\u30e9\u30ad\u30e5\u30e9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_akumajRomInfo, gba_f_akumajRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Double Dragon Advance (USA)

static struct BurnRomInfo gba_ddragonRomDesc[] = {
	{ "Double Dragon Advance (U)(2003)(Atlus).gba",	0x0400000,	0x764fafb5,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_ddragon, gba_ddragon, gba_gba)
STD_ROM_FN(gba_ddragon)

struct BurnDriver BurnDrvgba_ddragon = {
	"gba_ddragon", NULL, "gba_gba", NULL, "2003",
	"Double Dragon Advance (USA)\0", NULL, "Atlus", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_SCRFIGHT, 0,
	GbaGetZipName, gba_ddragonRomInfo, gba_ddragonRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Double Dragon Advance (Japan)

static struct BurnRomInfo gba_ddragonjRomDesc[] = {
	{ "Double Dragon Advance (J)(2004)(Atlus).gba",	0x0400000,	0xa3330e8f,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_ddragonj, gba_ddragonj, gba_gba)
STD_ROM_FN(gba_ddragonj)

struct BurnDriver BurnDrvgba_ddragonj = {
	"gba_ddragonj", "gba_ddragon", "gba_gba", NULL, "2004",
	"Double Dragon Advance (Japan)\0", NULL, "Atlus", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_SCRFIGHT, 0,
	GbaGetZipName, gba_ddragonjRomInfo, gba_ddragonjRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Fire Emblem: The Sacred Stones (USA, Australia)
// No cartridge‑mounted GPIO peripherals - GbaInputInfo

static struct BurnRomInfo gba_firembssuRomDesc[] = {
	{ "Fire Emblem - The Sacred Stones (U, A)(2005)(Nintendo).gba",	0x1000000,	0xa47246ae,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_firembssu, gba_firembssu, gba_gba)
STD_ROM_FN(gba_firembssu)

struct BurnDriver BurnDrvgba_firembssu = {
	"gba_firembssu", NULL, "gba_gba", NULL, "2005",
	"Fire Emblem: The Sacred Stones (USA, Australia)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RPG | GBF_STRATEGY, 0,
	GbaGetZipName, gba_firembssuRomInfo, gba_firembssuRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Fire Emblem: Seima no Kouseki (Japan)
// No cartridge‑mounted GPIO peripherals - GbaInputInfo

static struct BurnRomInfo gba_firembssjRomDesc[] = {
	{ "Fire Emblem - Seima no Kouseki (J)(2004)(Nintendo).gba",	0x1000000,	0x9d76826f,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_firembssj, gba_firembssj, gba_gba)
STD_ROM_FN(gba_firembssj)

struct BurnDriver BurnDrvgba_firembssj = {
	"gba_firembssj", "gba_firembssu", "gba_gba", NULL, "2004",
	"Fire Emblem: Seima no Kouseki (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Fire Emblem: Seima no Kouseki (Japan)\0\u30d5\u30a1\u30a4\u30a2\u30fc\u30a8\u30e0\u30d6\u30ec\u30e0 \u8056\u9b54\u306e\u5149\u77f3\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RPG | GBF_STRATEGY, 0,
	GbaGetZipName, gba_firembssjRomInfo, gba_firembssjRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Fire Emblem: Sheng Xie De Yi Zhi (Hack, v1.4)
// No cartridge‑mounted GPIO peripherals - GbaInputInfo

static struct BurnRomInfo gba_firembssxRomDesc[] = {
	{ "Fire Emblem - Sheng Xie De Yi Zhi (C)(2006)(ChinaFE & Wolf Group).gba",	0x1000000,	0x9cbb923e,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_firembssx, gba_firembssx, gba_gba)
STD_ROM_FN(gba_firembssx)

struct BurnDriver BurnDrvgba_firembssx = {
	"gba_firembssx", "gba_firembssu", "gba_gba", NULL, "2006",
	"Fire Emblem: Sheng Xie De Yi Zhi (Hack, v1.4)\0", NULL, "ChinaFE & Wolf Group", "Game Boy Advance",
	L"Fire Emblem: Sheng Xie De Yi Zhi (Hack, v1.4)\0\u706b\u7130\u4e4b\u7eb9\u7ae0: \u5723\u90aa\u7684\u610f\u5fd7\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RPG | GBF_STRATEGY, 0,
	GbaGetZipName, gba_firembssxRomInfo, gba_firembssxRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Super Ghouls'n Ghosts (Europe, USA)

static struct BurnRomInfo gba_sgngRomDesc[] = {
	{ "Super Ghouls'n Ghosts (E, U)(2002)(Capcom).gba",	0x0400000,	0x1ef2acf3,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sgng, gba_sgng, gba_gba)
STD_ROM_FN(gba_sgng)

struct BurnDriver BurnDrvgba_sgng = {
	"gba_sgng", NULL, "gba_gba", NULL, "2002",
	"Super Ghouls'n Ghosts (Europe, USA)\0", NULL, "Capcom", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM | GBF_RUNGUN, 0,
	GbaGetZipName, gba_sgngRomInfo, gba_sgngRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// WarioWare: Twisted! (USA, Australia)
// Gyro sensor - GyroInputInfo

static struct BurnRomInfo gba_wariotwsRomDesc[] = {
	{ "WarioWare ‑ Twisted! (U, A)(2005)(Nintendo).gba",	0x1000000,	0xcb4e844b,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_wariotws, gba_wariotws, gba_gba)
STD_ROM_FN(gba_wariotws)

struct BurnDriver BurnDrvgba_wariotws = {
	"gba_wariotws", NULL, "gba_gba", NULL, "2005",
	"WarioWare: Twisted! (USA, Australia)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_MISC, 0,
	GbaGetZipName, gba_wariotwsRomInfo, gba_wariotwsRomName, NULL, NULL, NULL, NULL, GyroInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Mawaru: Made in Wario (Japan)
// Gyro sensor - GyroInputInfo

static struct BurnRomInfo gba_wariotwjRomDesc[] = {
	{"Mawaru - Made in Wario (J)(2005)(Nintendo).gba", 0x1000000, 0xe69964f1, BRF_ESS | BRF_PRG},
};

STDROMPICKEXT(gba_wariotwj, gba_wariotwj, gba_gba)
STD_ROM_FN(gba_wariotwj)

struct BurnDriver BurnDrvGbaMawaruWario = {
	"gba_wariotwj", "gba_wariotws", "gba_gba", NULL, "2005",
	"Mawaru: Made in Wario (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Mawaru: Made in Wario (Japan)\0\u307e\u308f\u308b\u30e1\u30a4\u30c9\u30a4\u30f3\u30ef\u30ea\u30aa\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_MISC, 0,
	GbaGetZipName, gba_wariotwjRomInfo, gba_wariotwjRomName, NULL, NULL, NULL, NULL, GyroInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Yoshi Topsy: Turvy (USA)
// Tilt sensor - TiltInputInfo

static struct BurnRomInfo gba_yoshittRomDesc[] = {
	{ "Yoshi Topsy ‑ Turvy (U)(2005)(Nintendo).gba",	0x800000,	0xe64c265c,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_yoshitt, gba_yoshitt, gba_gba)
STD_ROM_FN(gba_yoshitt)

struct BurnDriver BurnDrvgba_yoshitt = {
	"gba_yoshitt", NULL, "gba_gba", NULL, "2005",
	"Yoshi Topsy: Turvy (USA)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_yoshittRomInfo, gba_yoshittRomName, NULL, NULL, NULL, NULL, TiltInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Yoshi no Banyuuinryoku (Japan)
// Tilt sensor - TiltInputInfo

static struct BurnRomInfo gba_yoshibanRomDesc[] = {
	{ "Yoshi no Banyuuinryoku (J)(2005)(Nintendo).gba",	0x800000,	0x31594b7a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_yoshiban, gba_yoshiban, gba_gba)
STD_ROM_FN(gba_yoshiban)

struct BurnDriver BurnDrvgba_yoshiban = {
	"gba_yoshiban", "gba_yoshitt", "gba_gba", NULL, "2005",
	"Yoshi no Banyuuinryoku (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Yoshi no Banyuuinryoku (Japan)\0\u30e8\u30c3\u30b7\u30fc\u306e\u4e07\u6709\u5f15\u529b\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_yoshibanRomInfo, gba_yoshibanRomName, NULL, NULL, NULL, NULL, TiltInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Boktai 2: Solar Boy Django (USA)
// Solar sensor + RTC clock - SolarInputInfo

static struct BurnRomInfo gba_boktai2uRomDesc[] = {
	{ "Boktai 2 ‑ Solar Boy Django (U)(2004)(Konami).gba",	0x1000000,	0xe1ffb2d1,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_boktai2u, gba_boktai2u, gba_gba)
STD_ROM_FN(gba_boktai2u)

struct BurnDriver BurnDrvgba_boktai2u = {
	"gba_boktai2u", NULL, "gba_gba", NULL, "2004",
	"Boktai 2: Solar Boy Django (USA)\0", NULL, "Konami", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_RPG, 0,
	GbaGetZipName, gba_boktai2uRomInfo, gba_boktai2uRomName, NULL, NULL, NULL, NULL, SolarInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Zoku Bokura no Taiyou: Taiyou Shounen Django (Japan, Rev 1)
// Solar sensor + RTC clock - SolarInputInfo

static struct BurnRomInfo gba_boktai2jRomDesc[] = {
	{"Zoku Bokura no Taiyou - Taiyou Shounen Django (J, Rev 1)(2004)(Konami).gba", 0x1000000, 0x71e2cd01, BRF_ESS | BRF_PRG},
};

STDROMPICKEXT(gba_boktai2j, gba_boktai2j, gba_gba)
STD_ROM_FN(gba_boktai2j)

struct BurnDriver BurnDrvGbaBoktai2j = {
	"gba_boktai2j", "gba_boktai2u", "gba_gba", NULL, "2004",
	"Zoku Bokura no Taiyou: Taiyou Shounen Django (Japan, Rev 1)\0", NULL, "Konami", "Game Boy Advance",
	L"Zoku Bokura no Taiyou: Taiyou Shounen Django (Japan, Rev 1)\0\u7d9a \u30dc\u30af\u3089\u306e\u592a\u967d \u592a\u967d\u5c11\u5e74\u30b8\u30e3\u30f3\u30b4\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_RPG, 0,
	GbaGetZipName, gba_boktai2jRomInfo, gba_boktai2jRomName, NULL, NULL, NULL, NULL, SolarInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};
