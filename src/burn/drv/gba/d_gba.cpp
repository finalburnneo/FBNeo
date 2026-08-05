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
	{ "gba_gba.bin", 0x4000, 0x81977335, BRF_PRG | BRF_BIOS | BRF_OPT },
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
//  [Example] Fire Emblem : The Sacred Stones(USA, Australia)
//  ShortName: gba_firembssu
// =========================================================================================


// Fire Emblem: The Sacred Stones (USA, Australia)

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

// WarioWare: Twisted! (USA, Australia)
// Gyro sensor

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
// Gyro sensor

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
// Tilt sensor

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
// Solar sensor + RTC clock

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
// Solar light‑sensor + RTC clock

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
