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

// Famicom Mini Vol.08: Mappy (Japan)

static struct BurnRomInfo gba_f_mappyRomDesc[] = {
	{ "Famicom Mini Vol.08 - Mappy (J)(2004)(Nintendo).gba",	0x0100000,	0xc29e4a08,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_mappy, gba_f_mappy, gba_gba)
STD_ROM_FN(gba_f_mappy)

struct BurnDriver BurnDrvgba_f_mappy = {
	"gba_f_mappy", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.08: Mappy (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_mappyRomInfo, gba_f_mappyRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Bomberman (USA, Europe)

static struct BurnRomInfo gba_n_bombmnRomDesc[] = {
	{ "Classic NES Series - Bomberman (U, E)(2004)(Nintendo).gba",	0x0100000,	0xc9ebc17d,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_bombmn, gba_n_bombmn, gba_gba)
STD_ROM_FN(gba_n_bombmn)

struct BurnDriver BurnDrvgba_n_bombmn = {
	"gba_n_bombmn", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Bomberman (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_MAZE, 0,
	GbaGetZipName, gba_n_bombmnRomInfo, gba_n_bombmnRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.09: Bomberman (Japan)

static struct BurnRomInfo gba_f_bombmnRomDesc[] = {
	{ "Famicom Mini Vol.09 - Bomberman (J)(2004)(Nintendo).gba",	0x0100000,	0xc1116e40,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_bombmn, gba_f_bombmn, gba_gba)
STD_ROM_FN(gba_f_bombmn)

struct BurnDriver BurnDrvgba_f_bombmn = {
	"gba_f_bombmn", "gba_n_bombmn", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.09: Bomberman (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_MAZE, 0,
	GbaGetZipName, gba_f_bombmnRomInfo, gba_f_bombmnRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.10: Star Soldier (Japan)

static struct BurnRomInfo gba_f_ssoldrRomDesc[] = {
	{ "Famicom Mini Vol.10 - Star Soldier (J)(2004)(Nintendo).gba",	0x0100000,	0x1e23bad4,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_ssoldr, gba_f_ssoldr, gba_gba)
STD_ROM_FN(gba_f_ssoldr)

struct BurnDriver BurnDrvgba_f_ssoldr = {
	"gba_f_ssoldr", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.10: Star Soldier (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VERSHOOT, 0,
	GbaGetZipName, gba_f_ssoldrRomInfo, gba_f_ssoldrRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.11: Mario Bros. (Japan)

static struct BurnRomInfo gba_f_marioRomDesc[] = {
	{ "Famicom Mini Vol.11 - Mario Bros. (J)(2004)(Nintendo).gba",	0x0100000,	0x42a027ab,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_mario, gba_f_mario, gba_gba)
STD_ROM_FN(gba_f_mario)

struct BurnDriver BurnDrvgba_f_mario = {
	"gba_f_mario", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.11: Mario Bros. (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_marioRomInfo, gba_f_marioRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.12: Clu Clu Land (Japan)

static struct BurnRomInfo gba_f_clucluRomDesc[] = {
	{ "Famicom Mini Vol.12 - Clu Clu Land (J)(2004)(Nintendo).gba",	0x0100000,	0x8b3219fe,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_cluclu, gba_f_cluclu, gba_gba)
STD_ROM_FN(gba_f_cluclu)

struct BurnDriver BurnDrvgba_f_cluclu = {
	"gba_f_cluclu", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.12: Clu Clu Land (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_MAZE, 0,
	GbaGetZipName, gba_f_clucluRomInfo, gba_f_clucluRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.13: Balloon Fight (Japan)

static struct BurnRomInfo gba_f_ballnfRomDesc[] = {
	{ "Famicom Mini Vol.13 - Balloon Fight (J)(2004)(Nintendo).gba",	0x0100000,	0xa01f014a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_ballnf, gba_f_ballnf, gba_gba)
STD_ROM_FN(gba_f_ballnf)

struct BurnDriver BurnDrvgba_f_ballnf = {
	"gba_f_ballnf", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.13: Balloon Fight (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_ballnfRomInfo, gba_f_ballnfRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.14: Wrecking Crew (Japan)

static struct BurnRomInfo gba_f_wreckRomDesc[] = {
	{ "Famicom Mini Vol.14 - Wrecking Crew (J)(2004)(Nintendo).gba",	0x0100000,	0xadac99bd,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_wreck, gba_f_wreck, gba_gba)
STD_ROM_FN(gba_f_wreck)

struct BurnDriver BurnDrvgba_f_wreck = {
	"gba_f_wreck", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.14: Wrecking Crew (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_wreckRomInfo, gba_f_wreckRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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

// Famicom Mini Vol.16: Dig Dug (Japan)

static struct BurnRomInfo gba_f_digdugRomDesc[] = {
	{ "Famicom Mini Vol.16 - Dig Dug (J)(2004)(Nintendo).gba",	0x0100000,	0xee4c3504,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_digdug, gba_f_digdug, gba_gba)
STD_ROM_FN(gba_f_digdug)

struct BurnDriver BurnDrvgba_f_digdug = {
	"gba_f_digdug", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.16: Dig Dug (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_digdugRomInfo, gba_f_digdugRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.17: Takahashi Meijin no Bouken-jima (Japan)

static struct BurnRomInfo gba_f_takambRomDesc[] = {
	{ "Famicom Mini Vol.17 - Takahashi Meijin no Bouken-jima (J)(2004)(Nintendo).gba",	0x0100000,	0x53286f51,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_takamb, gba_f_takamb, gba_gba)
STD_ROM_FN(gba_f_takamb)

struct BurnDriver BurnDrvgba_f_takamb = {
	"gba_f_takamb", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.17: Takahashi Meijin no Bouken-jima (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.17: Takahashi Meijin no Bouken-jima (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.17 \u9ad8\u6a4b\u540d\u4eba\u306e\u5192\u967a\u5cf6\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_takambRomInfo, gba_f_takambRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.18: Makaimura (Japan)

static struct BurnRomInfo gba_f_makaimRomDesc[] = {
	{ "Famicom Mini Vol.18 - Makaimura (J)(2004)(Nintendo).gba",	0x0100000,	0x8a7964ca,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_makaim, gba_f_makaim, gba_gba)
STD_ROM_FN(gba_f_makaim)

struct BurnDriver BurnDrvgba_f_makaim = {
	"gba_f_makaim", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.18: Makaimura (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.18: Makaimura (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.18 \u9b54\u754c\u6751\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM | GBF_RUNGUN, 0,
	GbaGetZipName, gba_f_makaimRomInfo, gba_f_makaimRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.19: Twin Bee (Japan)

static struct BurnRomInfo gba_f_twinbeRomDesc[] = {
	{ "Famicom Mini Vol.19 - Twin Bee (J)(2004)(Nintendo).gba",	0x0100000,	0x2f390212,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_twinbe, gba_f_twinbe, gba_gba)
STD_ROM_FN(gba_f_twinbe)

struct BurnDriver BurnDrvgba_f_twinbe = {
	"gba_f_twinbe", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.19: Twin Bee (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.19: Twin Bee (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.19 \u30c4\u30a4\u30f3\u30d3\u30fc\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VERSHOOT, 0,
	GbaGetZipName, gba_f_twinbeRomInfo, gba_f_twinbeRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.20: Ganbare Goemon!: Karakuri Douchuu (Japan)

static struct BurnRomInfo gba_f_goemonRomDesc[] = {
	{ "Famicom Mini Vol.20 - Ganbare Goemon! - Karakuri Douchuu (J)(2004)(Nintendo).gba",	0x0100000,	0x33196b58,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_goemon, gba_f_goemon, gba_gba)
STD_ROM_FN(gba_f_goemon)

struct BurnDriver BurnDrvgba_f_goemon = {
	"gba_f_goemon", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.20: Ganbare Goemon!: Karakuri Douchuu (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.20: Ganbare Goemon!: Karakuri Douchuu (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.20 \u304c\u3093\u3070\u308c\u30b4\u30a8\u30e2\u30f3! \u304b\u3089\u304f\u308a\u9053\u4e2d\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ACTION | GBF_ADV, 0,
	GbaGetZipName, gba_f_goemonRomInfo, gba_f_goemonRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.21: Super Mario Bros. 2 (Japan)

static struct BurnRomInfo gba_f_smb2RomDesc[] = {
	{ "Famicom Mini Vol.21 - Super Mario Bros. 2 (J)(2004)(Nintendo).gba",	0x0100000,	0xef18f7b2,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_smb2, gba_f_smb2, gba_gba)
STD_ROM_FN(gba_f_smb2)

struct BurnDriver BurnDrvgba_f_smb2 = {
	"gba_f_smb2", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.21: Super Mario Bros. 2 (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_smb2RomInfo, gba_f_smb2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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

// Famicom Mini Vol.24: Hikari Shinwa: Palthena no Kagami (Japan)

static struct BurnRomInfo gba_f_hikariRomDesc[] = {
	{ "Famicom Mini Vol.24 - Hikari Shinwa - Palthena no Kagami (J)(2004)(Nintendo).gba",	0x0400000,	0xf311edac,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_hikari, gba_f_hikari, gba_gba)
STD_ROM_FN(gba_f_hikari)

struct BurnDriver BurnDrvgba_f_hikari = {
	"gba_f_hikari", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.24: Hikari Shinwa: Palthena no Kagami (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.24: Hikari Shinwa: Palthena no Kagami (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.24 \u5149\u795e\u8a71 \u30d1\u30eb\u30c6\u30ca\u306e\u93e1\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_hikariRomInfo, gba_f_hikariRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Classic NES Series: Zelda II: The Adventure of Link (USA, Europe)

static struct BurnRomInfo gba_n_zelda2RomDesc[] = {
	{ "Classic NES Series - Zelda II - The Adventure of Link (U, E)(2004)(Nintendo).gba",	0x0100000,	0x0b6ca48a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_n_zelda2, gba_n_zelda2, gba_gba)
STD_ROM_FN(gba_n_zelda2)

struct BurnDriver BurnDrvgba_n_zelda2 = {
	"gba_n_zelda2", NULL, "gba_gba", NULL, "2004",
	"Classic NES Series: Zelda II: The Adventure of Link (USA, Europe)\0", NULL, "Nintendo", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ADV | GBF_PLATFORM, 0,
	GbaGetZipName, gba_n_zelda2RomInfo, gba_n_zelda2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.25: The Legend of Zelda 2: Link no Bouken (Japan)

static struct BurnRomInfo gba_f_zelda2RomDesc[] = {
	{ "Famicom Mini Vol.25 - The Legend of Zelda 2 - Link no Bouken (J)(2004)(Nintendo).gba",	0x0400000,	0x1cbe712a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_zelda2, gba_f_zelda2, gba_gba)
STD_ROM_FN(gba_f_zelda2)

struct BurnDriver BurnDrvgba_f_zelda2 = {
	"gba_f_zelda2", "gba_n_zelda2", "gba_gba", NULL, "2004",
	"Famicom Mini Vol.25: The Legend of Zelda 2: Link no Bouken (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.25: The Legend of Zelda 2: Link no Bouken (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.25 \u30bc\u30eb\u30c0\u306e\u4f1d\u8aac 2 \u30ea\u30f3\u30af\u306e\u5192\u967a\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ADV | GBF_PLATFORM, 0,
	GbaGetZipName, gba_f_zelda2RomInfo, gba_f_zelda2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini 26: Famicom Mukashibanashi: Shin Onigashima: Zen, Kouhen (Japan)

static struct BurnRomInfo gba_f_mukshiRomDesc[] = {
	{ "Famicom Mini Vol.26 - Famicom Mukashibanashi - Shin Onigashima - Zen, Kouhen (J)(2004)(Nintendo).gba",	0x0400000,	0x63b51337,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_mukshi, gba_f_mukshi, gba_gba)
STD_ROM_FN(gba_f_mukshi)

struct BurnDriver BurnDrvgba_f_mukshi = {
	"gba_f_mukshi", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.26: Famicom Mukashibanashi: Shin Onigashima: Zen, Kouhen (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.26: Famicom Mukashibanashi: Shin Onigashima: Zen, Kouhen (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.26 \u3075\u3041\u307f\u3053\u3093\u3080\u304b\u3057\u8a71 \u65b0\u30fb\u9b3c\u30f6\u5cf6 \u524d\u5f8c\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_MISC, 0,
	GbaGetZipName, gba_f_mukshiRomInfo, gba_f_mukshiRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.27: Famicom Tantei Club: Kieta Koukeisha: Zen, Kouhen (Japan)

static struct BurnRomInfo gba_f_tanteiRomDesc[] = {
	{ "Famicom Mini Vol.27 - Famicom Tantei Club - Kieta Koukeisha - Zen, Kouhen (J)(2004)(Nintendo).gba",	0x0400000,	0x3cf43405,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_tantei, gba_f_tantei, gba_gba)
STD_ROM_FN(gba_f_tantei)

struct BurnDriver BurnDrvgba_f_tantei = {
	"gba_f_tantei", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.27: Famicom Tantei Club: Kieta Koukeisha: Zen, Kouhen (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.27: Famicom Tantei Club: Kieta Koukeisha: Zen, Kouhen (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.27 \u30d5\u30a1\u30df\u30b3\u30f3\u63a2\u5075\u5036\u697d\u90e8 \u6d88\u3048\u305f\u5f8c\u7d99\u8005 \u524d\u5f8c\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_MISC, 0,
	GbaGetZipName, gba_f_tanteiRomInfo, gba_f_tanteiRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini Vol.28: Famicom Tantei Club Part II: Ushiro ni Tatsu Shoujo: Zen, Kouhen (Japan)

static struct BurnRomInfo gba_f_tante2RomDesc[] = {
	{ "Famicom Mini Vol.28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen, Kouhen (J)(2004)(Nintendo).gba",	0x0400000,	0x75e1b220,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_tante2, gba_f_tante2, gba_gba)
STD_ROM_FN(gba_f_tante2)

struct BurnDriver BurnDrvgba_f_tante2 = {
	"gba_f_tante2", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.28: Famicom Tantei Club Part II: Ushiro ni Tatsu Shoujo: Zen, Kouhen (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.28: Famicom Tantei Club Part II: Ushiro ni Tatsu Shoujo: Zen, Kouhen (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.28 \u30d5\u30a1\u30df\u30b3\u30f3\u63a2\u5075\u5036\u697d\u90e8 PART\u2161 \u3046\u3057\u308d\u306b\u7acb\u3064\u5c11\u5973 \u524d\u5f8c\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_MISC, 0,
	GbaGetZipName, gba_f_tante2RomInfo, gba_f_tante2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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

// Famicom Mini Vol.30: SD Gundam World: Gachapon Senshi Scramble Wars (Japan)

static struct BurnRomInfo gba_f_sdgundRomDesc[] = {
	{ "Famicom Mini Vol.30 - SD Gundam World - Gachapon Senshi Scramble Wars (J)(2004)(Nintendo).gba",	0x0100000,	0xba78d1ee,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_sdgund, gba_f_sdgund, gba_gba)
STD_ROM_FN(gba_f_sdgund)

struct BurnDriver BurnDrvgba_f_sdgund = {
	"gba_f_sdgund", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini Vol.30: SD Gundam World: Gachapon Senshi Scramble Wars (Japan)\0", NULL, "Nintendo", "Game Boy Advance",
	L"Famicom Mini Vol.30: SD Gundam World: Gachapon Senshi Scramble Wars (Japan)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb Vol.30 SD \u30ac\u30f3\u30c0\u30e0\u30ef\u30fc\u30eb\u30c9 \u30ac\u30c1\u30e3\u30dd\u30f3\u6226\u58eb \u30b9\u30af\u30e9\u30f3\u30d6\u30eb\u30a6\u30a9\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_STRATEGY, 0,
	GbaGetZipName, gba_f_sdgundRomInfo, gba_f_sdgundRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini: Dai-2-ji Super Robot Taisen (Japan, Promo)

static struct BurnRomInfo gba_f_srobo2RomDesc[] = {
	{ "Famicom Mini - Dai-2-ji Super Robot Taisen (J, Promo)(2004)(Banpresto).gba",	0x0100000,	0x3ebb082a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_srobo2, gba_f_srobo2, gba_gba)
STD_ROM_FN(gba_f_srobo2)

struct BurnDriver BurnDrvgba_f_srobo2 = {
	"gba_f_srobo2", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini: Dai-2-ji Super Robot Taisen (Japan, Promo)\0", NULL, "Banpresto", "Game Boy Advance",
	L"Famicom Mini: Dai-2-ji Super Robot Taisen (Japan, Promo)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb \u7b2c 2 \u6b21\u30b9\u30fc\u30d1\u30fc\u30ed\u30dc\u30c3\u30c8\u5927\u6226 (\u30d7\u30ed\u30e2\u975e\u58f2\u54c1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_STRATEGY, 0,
	GbaGetZipName, gba_f_srobo2RomInfo, gba_f_srobo2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Famicom Mini: Kidou Senshi Z Gundam: Hot Scramble (Japan, Promo)

static struct BurnRomInfo gba_f_zgundmRomDesc[] = {
	{ "Famicom Mini - Kidou Senshi Z Gundam - Hot Scramble (J, Promo)(2004)(Bandai).gba",	0x0100000,	0x07a73d0a,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_f_zgundm, gba_f_zgundm, gba_gba)
STD_ROM_FN(gba_f_zgundm)

struct BurnDriver BurnDrvgba_f_zgundm = {
	"gba_f_zgundm", NULL, "gba_gba", NULL, "2004",
	"Famicom Mini: Kidou Senshi Z Gundam: Hot Scramble (Japan, Promo)\0", NULL, "Bandai", "Game Boy Advance",
	L"Famicom Mini: Kidou Senshi Z Gundam: Hot Scramble (Japan, Promo)\0\u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb \u30d5\u30a1\u30df\u30b3\u30f3\u30df\u30cb \u6a5f\u52d5\u6226\u58eb Z \u30ac\u30f3\u30c0\u30e0 \u30db\u30c3\u30c8\u30b9\u30af\u30e9\u30f3\u30d6\u30eb (\u30d7\u30ed\u30e2\u975e\u58f2\u54c1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VERSHOOT, 0,
	GbaGetZipName, gba_f_zgundmRomInfo, gba_f_zgundmRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Chou Makaimura R (Japan)
static struct BurnRomInfo gba_chomakaiRomDesc[] = {
	{ "Chou Makaimura R (J)(2002)(Capcom).gba",	0x0400000,	0xa4f8b4b4,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_chomakai, gba_chomakai, gba_gba)
STD_ROM_FN(gba_chomakai)

struct BurnDriver BurnDrvgba_chomakai = {
	"gba_chomakai", "gba_sgng", "gba_gba", NULL, "2002",
	"Chou Makaimura R (Japan)\0", NULL, "Capcom", "Game Boy Advance",
	L"Chou Makaimura R (Japan)\0\u8d85\u9b54\u754c\u6751R\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM | GBF_RUNGUN, 0,
	GbaGetZipName, gba_chomakaiRomInfo, gba_chomakaiRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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
	L"Double Dragon Advance (Japan)\0\u53cc\u8f09\u9f8d\0", NULL, NULL, NULL,
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

// Sonic Advance (Europe)

static struct BurnRomInfo gba_sonicRomDesc[] = {
	{ "Sonic Advance (E)(2002)(Infogrames).gba",	0x800000,	0x6232839b,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic, gba_sonic, gba_gba)
STD_ROM_FN(gba_sonic)

struct BurnDriver BurnDrvgba_sonic = {
	"gba_sonic", NULL, "gba_gba", NULL, "2002",
	"Sonic Advance (Europe)\0", NULL, "Infogrames", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonicRomInfo, gba_sonicRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance (Japan, Rev 1)

static struct BurnRomInfo gba_sonicjRomDesc[] = {
	{ "Sonic Advance (J, Rev 1)(2001)(Sega).gba",	0x800000,	0x85957a24,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicj, gba_sonicj, gba_gba)
STD_ROM_FN(gba_sonicj)

struct BurnDriver BurnDrvgba_sonicj = {
	"gba_sonicj", "gba_sonic", "gba_gba", NULL, "2001",
	"Sonic Advance (Japan, Rev 1)\0", NULL, "Sega", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonicjRomInfo, gba_sonicjRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance (USA)

static struct BurnRomInfo gba_sonicuRomDesc[] = {
	{ "Sonic Advance (U)(2002)(THQ).gba",	0x800000,	0x63f70fd8,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicu, gba_sonicu, gba_gba)
STD_ROM_FN(gba_sonicu)

struct BurnDriver BurnDrvgba_sonicu = {
	"gba_sonicu", "gba_sonic", "gba_gba", NULL, "2002",
	"Sonic Advance (USA)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonicuRomInfo, gba_sonicuRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 2 (Europe)

static struct BurnRomInfo gba_sonic2RomDesc[] = {
	{ "Sonic Advance 2 (E)(2003)(Infogrames).gba",	0x1000000,	0x89509891,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic2, gba_sonic2, gba_gba)
STD_ROM_FN(gba_sonic2)

struct BurnDriver BurnDrvgba_sonic2 = {
	"gba_sonic2", NULL, "gba_gba", NULL, "2003",
	"Sonic Advance 2 (Europe)\0", NULL, "Infogrames", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic2RomInfo, gba_sonic2RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 2 (Japan)

static struct BurnRomInfo gba_sonic2jRomDesc[] = {
	{ "Sonic Advance 2 (J)(2002)(Sega).gba",	0x1000000,	0x513804ff,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic2j, gba_sonic2j, gba_gba)
STD_ROM_FN(gba_sonic2j)

struct BurnDriver BurnDrvgba_sonic2j = {
	"gba_sonic2j", "gba_sonic2", "gba_gba", NULL, "2002",
	"Sonic Advance 2 (Japan)\0", NULL, "Sega", "Game Boy Advance",
	L"Sonic Advance 2 (Japan)\0\u30bd\u30cb\u30c3\u30af\u30a2\u30c9\u30d0\u30f3\u30b92\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic2jRomInfo, gba_sonic2jRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 2 (USA)

static struct BurnRomInfo gba_sonic2uRomDesc[] = {
	{ "Sonic Advance 2 (U)(2003)(THQ).gba",	0x1000000,	0x7efee7f7,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic2u, gba_sonic2u, gba_gba)
STD_ROM_FN(gba_sonic2u)

struct BurnDriver BurnDrvgba_sonic2u = {
	"gba_sonic2u", "gba_sonic2", "gba_gba", NULL, "2003",
	"Sonic Advance 2 (USA)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic2uRomInfo, gba_sonic2uRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 3 (Europe)

static struct BurnRomInfo gba_sonic3RomDesc[] = {
	{ "Sonic Advance 3 (E)(2004)(THQ).gba",	0x1000000,	0x5bf83456,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic3, gba_sonic3, gba_gba)
STD_ROM_FN(gba_sonic3)

struct BurnDriver BurnDrvgba_sonic3 = {
	"gba_sonic3", NULL, "gba_gba", NULL, "2004",
	"Sonic Advance 3 (Europe)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic3RomInfo, gba_sonic3RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 3 (Japan)

static struct BurnRomInfo gba_sonic3jRomDesc[] = {
	{ "Sonic Advance 3 (J)(2004)(Sega).gba",	0x1000000,	0x4375f1d6,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic3j, gba_sonic3j, gba_gba)
STD_ROM_FN(gba_sonic3j)

struct BurnDriver BurnDrvgba_sonic3j = {
	"gba_sonic3j", "gba_sonic3", "gba_gba", NULL, "2004",
	"Sonic Advance 3 (Japan)\0", NULL, "Sega", "Game Boy Advance",
	L"Sonic Advance 3 (Japan)\0\u30bd\u30cb\u30c3\u30af\u30a2\u30c9\u30d0\u30f3\u30b93\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic3jRomInfo, gba_sonic3jRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Advance 3 (USA)

static struct BurnRomInfo gba_sonic3uRomDesc[] = {
	{ "Sonic Advance 3 (U)(2004)(THQ).gba",	0x1000000,	0x49dda5e6,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonic3u, gba_sonic3u, gba_gba)
STD_ROM_FN(gba_sonic3u)

struct BurnDriver BurnDrvgba_sonic3u = {
	"gba_sonic3u", "gba_sonic3", "gba_gba", NULL, "2004",
	"Sonic Advance 3 (USA)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonic3uRomInfo, gba_sonic3uRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Battle (Europe)

static struct BurnRomInfo gba_sonicbtlRomDesc[] = {
	{ "Sonic Battle (E)(2004)(THQ).gba",	0x1000000,	0xd0f65125,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicbtl, gba_sonicbtl, gba_gba)
STD_ROM_FN(gba_sonicbtl)

struct BurnDriver BurnDrvgba_sonicbtl = {
	"gba_sonicbtl", NULL, "gba_gba", NULL, "2004",
	"Sonic Battle (Europe)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, FBF_SONIC,
	GbaGetZipName, gba_sonicbtlRomInfo, gba_sonicbtlRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Battle (Japan)

static struct BurnRomInfo gba_sonicbtljRomDesc[] = {
	{ "Sonic Battle (J)(2003)(Sega).gba",	0x1000000,	0x7305ac30,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicbtlj, gba_sonicbtlj, gba_gba)
STD_ROM_FN(gba_sonicbtlj)

struct BurnDriver BurnDrvgba_sonicbtlj = {
	"gba_sonicbtlj", "gba_sonicbtl", "gba_gba", NULL, "2003",
	"Sonic Battle (Japan)\0", NULL, "Sega", "Game Boy Advance",
	L"Sonic Battle (Japan)\0\u30bd\u30cb\u30c3\u30af\u30d0\u30c8\u30eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, FBF_SONIC,
	GbaGetZipName, gba_sonicbtljRomInfo, gba_sonicbtljRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Battle (USA)

static struct BurnRomInfo gba_sonicbtluRomDesc[] = {
	{ "Sonic Battle (U)(2004)(THQ).gba",	0x1000000,	0x9ec9d86f,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicbtlu, gba_sonicbtlu, gba_gba)
STD_ROM_FN(gba_sonicbtlu)

struct BurnDriver BurnDrvgba_sonicbtlu = {
	"gba_sonicbtlu", "gba_sonicbtl", "gba_gba", NULL, "2004",
	"Sonic Battle (USA)\0", NULL, "THQ", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, FBF_SONIC,
	GbaGetZipName, gba_sonicbtluRomInfo, gba_sonicbtluRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Pinball Party (Europe)

static struct BurnRomInfo gba_sonicpinRomDesc[] = {
	{ "Sonic Pinball Party (E)(2003)(Sega).gba",	0x800000,	0x4435917e,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicpin, gba_sonicpin, gba_gba)
STD_ROM_FN(gba_sonicpin)

struct BurnDriver BurnDrvgba_sonicpin = {
	"gba_sonicpin", NULL, "gba_gba", NULL, "2003",
	"Sonic Pinball Party (Europe)\0", NULL, "Sega", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PINBALL, FBF_SONIC,
	GbaGetZipName, gba_sonicpinRomInfo, gba_sonicpinRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Pinball Party (Japan)

static struct BurnRomInfo gba_sonicpinjRomDesc[] = {
	{ "Sonic Pinball Party (J)(2003)(Sega).gba",	0x800000,	0x43b5f167,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicpinj, gba_sonicpinj, gba_gba)
STD_ROM_FN(gba_sonicpinj)

struct BurnDriver BurnDrvgba_sonicpinj = {
	"gba_sonicpinj", "gba_sonicpin", "gba_gba", NULL, "2003",
	"Sonic Pinball Party (Japan)\0", NULL, "Sega", "Game Boy Advance",
	L"Sonic Pinball Party (Japan)\0\u30bd\u30cb\u30c3\u30af\u30d4\u30f3\u30dc\u30fc\u30eb\u30d1\u30fc\u30c6\u30a3\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PINBALL, FBF_SONIC,
	GbaGetZipName, gba_sonicpinjRomInfo, gba_sonicpinjRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic Pinball Party (USA)

static struct BurnRomInfo gba_sonicpinuRomDesc[] = {
	{ "Sonic Pinball Party (U)(2003)(Sega).gba",	0x800000,	0x08794743,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicpinu, gba_sonicpinu, gba_gba)
STD_ROM_FN(gba_sonicpinu)

struct BurnDriver BurnDrvgba_sonicpinu = {
	"gba_sonicpinu", "gba_sonicpin", "gba_gba", NULL, "2003",
	"Sonic Pinball Party (USA)\0", NULL, "Sega", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PINBALL, FBF_SONIC,
	GbaGetZipName, gba_sonicpinuRomInfo, gba_sonicpinuRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Sonic the Hedgehog - Genesis (USA)

static struct BurnRomInfo gba_sonicgenRomDesc[] = {
	{ "Sonic the Hedgehog - Genesis (U)(2006)(Sega).gba",	0x400000,	0x027bc70d,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sonicgen, gba_sonicgen, gba_gba)
STD_ROM_FN(gba_sonicgen)

struct BurnDriver BurnDrvgba_sonicgen = {
	"gba_sonicgen", NULL, "gba_gba", NULL, "2006",
	"Sonic the Hedgehog - Genesis (USA)\0", NULL, "Sega", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, FBF_SONIC,
	GbaGetZipName, gba_sonicgenRomInfo, gba_sonicgenRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Street Fighter Alpha 3 (Europe)

static struct BurnRomInfo gba_sfa3RomDesc[] = {
	{ "Street Fighter Alpha 3 (E)(2002)(Capcom).gba",	0x0800000,	0x93c5cf69,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sfa3, gba_sfa3, gba_gba)
STD_ROM_FN(gba_sfa3)

struct BurnDriver BurnDrvgba_sfa3 = {
	"gba_sfa3", NULL, "gba_gba", NULL, "2002",
	"Street Fighter Alpha 3 (Europe)\0", NULL, "Capcom", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_sfa3RomInfo, gba_sfa3RomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Street Fighter Alpha 3 (USA)

static struct BurnRomInfo gba_sfa3uRomDesc[] = {
	{ "Street Fighter Alpha 3 (U)(2002)(Capcom).gba",	0x0800000,	0x80b707c2,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sfa3u, gba_sfa3u, gba_gba)
STD_ROM_FN(gba_sfa3u)

struct BurnDriver BurnDrvgba_sfa3u = {
	"gba_sfa3u", "gba_sfa3", "gba_gba", NULL, "2002",
	"Street Fighter Alpha 3 (USA)\0", NULL, "Capcom", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_sfa3uRomInfo, gba_sfa3uRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Street Fighter Zero 3 Upper (Japan)

static struct BurnRomInfo gba_sfa3jRomDesc[] = {
	{ "Street Fighter Zero 3 Upper (J)(2002)(Capcom).gba",	0x0800000,	0x8d5d0eab,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_sfa3j, gba_sfa3j, gba_gba)
STD_ROM_FN(gba_sfa3j)

struct BurnDriver BurnDrvgba_sfa3j = {
	"gba_sfa3j", "gba_sfa3", "gba_gba", NULL, "2002",
	"Street Fighter Zero 3 Upper (Japan)\0", NULL, "Capcom", "Game Boy Advance",
	L"Street Fighter Zero 3 Upper (Japan)\0\u30b9\u30c8\u30ea\u30fc\u30c8\u30d5\u30a1\u30a4\u30bf\u30fc\u30bc\u30ed3\u30a2\u30c3\u30d1\u30fc\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_sfa3jRomInfo, gba_sfa3jRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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

// Super Street Fighter II Turbo - Revival (Europe, Rev 1)

static struct BurnRomInfo gba_ssf2tRomDesc[] = {
	{ "Super Street Fighter II Turbo - Revival (E, Rev 1)(2001)(Capcom).gba",	0x0800000,	0x459600a9,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_ssf2t, gba_ssf2t, gba_gba)
STD_ROM_FN(gba_ssf2t)

struct BurnDriver BurnDrvgba_ssf2t = {
	"gba_ssf2t", NULL, "gba_gba", NULL, "2001",
	"Super Street Fighter II Turbo - Revival (Europe, Rev 1)\0", NULL, "Capcom", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_ssf2tRomInfo, gba_ssf2tRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Super Street Fighter II Turbo - Revival (USA)

static struct BurnRomInfo gba_ssf2tuRomDesc[] = {
	{ "Super Street Fighter II Turbo - Revival (U)(2001)(Capcom).gba",	0x0800000,	0x063045aa,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_ssf2tu, gba_ssf2tu, gba_gba)
STD_ROM_FN(gba_ssf2tu)

struct BurnDriver BurnDrvgba_ssf2tu = {
	"gba_ssf2tu", "gba_ssf2t", "gba_gba", NULL, "2001",
	"Super Street Fighter II Turbo - Revival (USA)\0", NULL, "Capcom", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_ssf2tuRomInfo, gba_ssf2tuRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Super Street Fighter II X - Revival (Japan)

static struct BurnRomInfo gba_ssf2tjRomDesc[] = {
	{ "Super Street Fighter II X - Revival (J)(2001)(Capcom).gba",	0x0800000,	0x7a2c0d61,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_ssf2tj, gba_ssf2tj, gba_gba)
STD_ROM_FN(gba_ssf2tj)

struct BurnDriver BurnDrvgba_ssf2tj = {
	"gba_ssf2tj", "gba_ssf2t", "gba_gba", NULL, "2001",
	"Super Street Fighter II X - Revival (Japan)\0", NULL, "Capcom", "Game Boy Advance",
	L"Super Street Fighter II X - Revival (Japan)\0\u30b9\u30fc\u30d1\u30fc\u30b9\u30c8\u30ea\u30fc\u30c8\u30d5\u30a1\u30a4\u30bf\u30fcII X \u30ea\u30d0\u30a4\u30d0\u30eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_VSFIGHT, 0,
	GbaGetZipName, gba_ssf2tjRomInfo, gba_ssf2tjRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
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


// ----------------------------------------
// Aftermarkets/Homebrews/Improvement Hacks
// ----------------------------------------


// Alice Sisters (HB)

static struct BurnRomInfo gba_alicesistersRomDesc[] = {
	{ "Alice Sisters (2024)(OrionSoft).gba",	16777216,	0x1ebddded,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_alicesisters, gba_alicesisters, gba_gba)
STD_ROM_FN(gba_alicesisters)

struct BurnDriver BurnDrvgba_alicesisters = {
	"gba_alicesisters", NULL, "gba_gba", NULL, "2024",
	"Alice Sisters (HB)\0", NULL, "OrionSoft", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PLATFORM, 0,
	GbaGetZipName, gba_alicesistersRomInfo, gba_alicesistersRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Goodboy Galaxy (HB, v1.3)

static struct BurnRomInfo gba_goodboyRomDesc[] = {
	{ "Goodboy Galaxy v1.3 (2023-24)(Goodboy Galaxy).gba",	33554432,	0x159ff629,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_goodboy, gba_goodboy, gba_gba)
STD_ROM_FN(gba_goodboy)

struct BurnDriver BurnDrvgba_goodboy = {
	"gba_goodboy", NULL, "gba_gba", NULL, "2023-24",
	"Goodboy Galaxy (HB, v1.3)\0", NULL, "Goodboy Galaxy", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_ADV | GBF_PLATFORM, 0,
	GbaGetZipName, gba_goodboyRomInfo, gba_goodboyRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Inky and the Alien Aquarium (HB)

static struct BurnRomInfo gba_inkyalienRomDesc[] = {
	{ "Inky and the Alien Aquarium (2023)(Pocket Pulp).gba",	4194304,	0x402d4ce6,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_inkyalien, gba_inkyalien, gba_gba)
STD_ROM_FN(gba_inkyalien)

struct BurnDriver BurnDrvgba_inkyalien = {
	"gba_inkyalien", NULL, "gba_gba", NULL, "2023",
	"Inky and the Alien Aquarium (HB)\0", NULL, "Pocket Pulp", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_PUZZLE, 0,
	GbaGetZipName, gba_inkyalienRomInfo, gba_inkyalienRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

// Xeno Crisis (HB)

static struct BurnRomInfo gba_xenocrisisRomDesc[] = {
	{ "Xeno Crisis (2024)(Bitmap Bureau).gba",	33554432,	0x9693aabd,	BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(gba_xenocrisis, gba_xenocrisis, gba_gba)
STD_ROM_FN(gba_xenocrisis)

struct BurnDriver BurnDrvgba_xenocrisis = {
	"gba_xenocrisis", NULL, "gba_gba", NULL, "2024",
	"Xeno Crisis (HB)\0", NULL, "Bitmap Bureau", "Game Boy Advance",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_GBA, GBF_RUNGUN, 0,
	GbaGetZipName, gba_xenocrisisRomInfo, gba_xenocrisisRomName, NULL, NULL, NULL, NULL, GbaInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	GBA_WIDTH, GBA_HEIGHT, 3, 2
};

