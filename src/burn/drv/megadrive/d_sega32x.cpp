// FB Neo Sega 32X driver module

#include "burnint.h"
#include "megadrive.h"

static struct BurnInputInfo S32XInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	MegadriveJoy1 +  7, "p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	MegadriveJoy1 +  0, "p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	MegadriveJoy1 +  1, "p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	MegadriveJoy1 +  2, "p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	MegadriveJoy1 +  3, "p1 right"  },
	{"P1 Button A",		BIT_DIGITAL,	MegadriveJoy1 +  6, "p1 fire 1" },
	{"P1 Button B",		BIT_DIGITAL,	MegadriveJoy1 +  4, "p1 fire 2" },
	{"P1 Button C",		BIT_DIGITAL,	MegadriveJoy1 +  5, "p1 fire 3" },
	{"P1 Button X",		BIT_DIGITAL,	MegadriveJoy1 + 10, "p1 fire 4" },
	{"P1 Button Y",		BIT_DIGITAL,	MegadriveJoy1 +  9, "p1 fire 5" },
	{"P1 Button Z",		BIT_DIGITAL,	MegadriveJoy1 +  8, "p1 fire 6" },
	{"P1 Mode",		BIT_DIGITAL,	MegadriveJoy1 + 11, "p1 select" },

	{"P2 Start",		BIT_DIGITAL,	MegadriveJoy2 +  7, "p2 start"  },
	{"P2 Up",		BIT_DIGITAL,	MegadriveJoy2 +  0, "p2 up"     },
	{"P2 Down",		BIT_DIGITAL,	MegadriveJoy2 +  1, "p2 down"   },
	{"P2 Left",		BIT_DIGITAL,	MegadriveJoy2 +  2, "p2 left"   },
	{"P2 Right",		BIT_DIGITAL,	MegadriveJoy2 +  3, "p2 right"  },
	{"P2 Button A",		BIT_DIGITAL,	MegadriveJoy2 +  6, "p2 fire 1" },
	{"P2 Button B",		BIT_DIGITAL,	MegadriveJoy2 +  4, "p2 fire 2" },
	{"P2 Button C",		BIT_DIGITAL,	MegadriveJoy2 +  5, "p2 fire 3" },
	{"P2 Button X",		BIT_DIGITAL,	MegadriveJoy2 + 10, "p2 fire 4" },
	{"P2 Button Y",		BIT_DIGITAL,	MegadriveJoy2 +  9, "p2 fire 5" },
	{"P2 Button Z",		BIT_DIGITAL,	MegadriveJoy2 +  8, "p2 fire 6" },
	{"P2 Mode",		BIT_DIGITAL,	MegadriveJoy2 + 11, "p2 select" },

	{"Reset",		BIT_DIGITAL,	&MegadriveReset,     "reset"    },
	{"Dip A",		BIT_DIPSWITCH,	MegadriveDIP  + 0,   "dip"      },
	{"Dip B",		BIT_DIPSWITCH,	MegadriveDIP  + 1,   "dip"      },
};

STDINPUTINFO(S32X)

static struct BurnDIPInfo S32XDIPList[] = {
	{0x19,	0xff, 0xff, 0x21, NULL				},

	{0,		0xfe, 0,       5, "Hardware"		},
	{0x19,	0x01, 0xc1, 0x01, "Auto Detect"		},
	{0x19,	0x01, 0xc1, 0x00, "Japan NTSC"		},
	{0x19,	0x01, 0xc1, 0x40, "Japan PAL"		},
	{0x19,	0x01, 0xc1, 0x80, "USA"				},
	{0x19,	0x01, 0xc1, 0xc0, "Europe"			},
};

STDDIPINFO(S32X)

static INT32 S32XGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else if (i == 1) {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "s32x_"
	memset(szFilename, 0, MAX_PATH);
	strncpy(szFilename, pszGameName + 5, MAX_PATH - 1);

	*pszName = szFilename;

	return 0;
}

// After Burner Complete ~ After Burner (Japan, USA)

static struct BurnRomInfo s32x_aburnerRomDesc[] = {
	{ "After Burner Complete ~ After Burner (Japan, USA).32x", 0x200000, 0x204044c4, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_aburner)
STD_ROM_FN(s32x_aburner)

struct BurnDriver BurnDrvs32x_aburner = {
	"s32x_aburner", NULL, NULL, NULL, "1994",
	"After Burner Complete ~ After Burner (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_aburnerRomInfo, s32x_aburnerRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// After Burner Complete (Europe)

static struct BurnRomInfo s32x_aburnereRomDesc[] = {
	{ "After Burner Complete (Europe).32x", 0x200000, 0x029106f5, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_aburnere)
STD_ROM_FN(s32x_aburnere)

struct BurnDriver BurnDrvs32x_aburnere = {
	"s32x_aburnere", "s32x_aburner", NULL, NULL, "1994",
	"After Burner Complete (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_aburnereRomInfo, s32x_aburnereRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// BC Racers (USA)

static struct BurnRomInfo s32x_bcracersRomDesc[] = {
	{ "BC Racers (USA).32x", 0x300000, 0x936c3d27, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_bcracers)
STD_ROM_FN(s32x_bcracers)

struct BurnDriver BurnDrvs32x_bcracers = {
	"s32x_bcracers", NULL, NULL, NULL, "1995",
	"BC Racers (USA)\0", NULL, "Core Design", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_bcracersRomInfo, s32x_bcracersRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Blackthorne (USA)

static struct BurnRomInfo s32x_blackthnRomDesc[] = {
	{ "Blackthorne (USA).32x", 0x300000, 0xd1a60a47, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_blackthn)
STD_ROM_FN(s32x_blackthn)

struct BurnDriver BurnDrvs32x_blackthn = {
	"s32x_blackthn", NULL, NULL, NULL, "1995",
	"Blackthorne (USA)\0", NULL, "Interplay", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_blackthnRomInfo, s32x_blackthnRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Brutal - Above the Claw (USA)

static struct BurnRomInfo s32x_brutalRomDesc[] = {
	{ "Brutal - Above the Claw (USA).32x", 0x300000, 0x7a72c939, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_brutal)
STD_ROM_FN(s32x_brutal)

struct BurnDriver BurnDrvs32x_brutal = {
	"s32x_brutal", NULL, NULL, NULL, "1995",
	"Brutal - Above the Claw (USA)\0", NULL, "GameTek", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_brutalRomInfo, s32x_brutalRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Knuckles' Chaotix (Japan, USA)

static struct BurnRomInfo s32x_chaotixRomDesc[] = {
	{ "Knuckles' Chaotix (Japan, USA) (En).32x", 0x300000, 0xd0b0b842, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_chaotix)
STD_ROM_FN(s32x_chaotix)

struct BurnDriver BurnDrvs32x_chaotix = {
	"s32x_chaotix", NULL, NULL, NULL, "1995",
	"Knuckles' Chaotix (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_chaotixRomInfo, s32x_chaotixRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Knuckles' Chaotix (Europe)

static struct BurnRomInfo s32x_chaotixeRomDesc[] = {
	{ "Knuckles' Chaotix (Europe).32x", 0x300000, 0x41d63572, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_chaotixe)
STD_ROM_FN(s32x_chaotixe)

struct BurnDriver BurnDrvs32x_chaotixe = {
	"s32x_chaotixe", "s32x_chaotix", NULL, NULL, "1995",
	"Knuckles' Chaotix (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_chaotixeRomInfo, s32x_chaotixeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// ClayFighter 2 (USA, Prototype 19950428)

static struct BurnRomInfo s32x_clayfgt2RomDesc[] = {
	{ "ClayFighter 2 (USA) (Proto) (1995-04-28).32x", 0x100000, 0x59701755, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_clayfgt2)
STD_ROM_FN(s32x_clayfgt2)

struct BurnDriver BurnDrvs32x_clayfgt2 = {
	"s32x_clayfgt2", NULL, NULL, NULL, "1995",
	"ClayFighter 2 (USA, Prototype 19950428)\0", NULL, "Interplay", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_PROTOTYPE, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_clayfgt2RomInfo, s32x_clayfgt2RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Cosmic Carnage (Europe)

static struct BurnRomInfo s32x_cosmiccrRomDesc[] = {
	{ "Cosmic Carnage (Europe).32x", 0x300000, 0x9f3fdbc2, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_cosmiccr)
STD_ROM_FN(s32x_cosmiccr)

struct BurnDriver BurnDrvs32x_cosmiccr = {
	"s32x_cosmiccr", "s32x_cybrawl", NULL, NULL, "1994",
	"Cosmic Carnage (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_cosmiccrRomInfo, s32x_cosmiccrRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Cyber Brawl ~ Cosmic Carnage (Japan, USA)

static struct BurnRomInfo s32x_cybrawlRomDesc[] = {
	{ "Cyber Brawl ~ Cosmic Carnage (Japan, USA).32x", 0x300000, 0x7c7be6a2, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_cybrawl)
STD_ROM_FN(s32x_cybrawl)

struct BurnDriver BurnDrvs32x_cybrawl = {
	"s32x_cybrawl", NULL, NULL, NULL, "1994",
	"Cyber Brawl ~ Cosmic Carnage (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_cybrawlRomInfo, s32x_cybrawlRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Darxide (Europe)

static struct BurnRomInfo s32x_darxideRomDesc[] = {
	{ "Darxide (Europe) (En,Fr,De,Es).32x", 0x200000, 0x22d7c906, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_darxide)
STD_ROM_FN(s32x_darxide)

struct BurnDriver BurnDrvs32x_darxide = {
	"s32x_darxide", NULL, NULL, NULL, "1996",
	"Darxide (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_darxideRomInfo, s32x_darxideRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Doom (Japan, USA)

static struct BurnRomInfo s32x_doomRomDesc[] = {
	{ "Doom (Japan, USA).32x", 0x300000, 0x208332fd, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_doom)
STD_ROM_FN(s32x_doom)

struct BurnDriver BurnDrvs32x_doom = {
	"s32x_doom", NULL, NULL, NULL, "1994",
	"Doom (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_doomRomInfo, s32x_doomRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Doom (Europe)

static struct BurnRomInfo s32x_doomeRomDesc[] = {
	{ "Doom (Europe).32x", 0x300000, 0x53734e3a, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_doome)
STD_ROM_FN(s32x_doome)

struct BurnDriver BurnDrvs32x_doome = {
	"s32x_doome", "s32x_doom", NULL, NULL, "1994",
	"Doom (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_doomeRomInfo, s32x_doomeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Ecco the Dolphin (USA, Demo)

static struct BurnRomInfo s32x_eccodRomDesc[] = {
	{ "Ecco the Dolphin (USA) (Demo) (Developer Cart).32x", 0x300000, 0xb06178df, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_eccod)
STD_ROM_FN(s32x_eccod)

struct BurnDriver BurnDrvs32x_eccod = {
	"s32x_eccod", NULL, NULL, NULL, "1995",
	"Ecco the Dolphin (USA, Demo)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_DEMO, 1, HARDWARE_SEGA_32X, GBF_ACTION, 0,
	S32XGetZipName, s32x_eccodRomInfo, s32x_eccodRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// FIFA Soccer 96 (Europe)

static struct BurnRomInfo s32x_fifa96RomDesc[] = {
	{ "FIFA Soccer 96 (Europe) (En,Fr,De,Es,It,Sv).32x", 0x300000, 0xfb14a7c8, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_fifa96)
STD_ROM_FN(s32x_fifa96)

struct BurnDriver BurnDrvs32x_fifa96 = {
	"s32x_fifa96", NULL, NULL, NULL, "1995",
	"FIFA Soccer 96 (Europe)\0", NULL, "EA Sports", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SPORTSFOOTBALL, 0,
	S32XGetZipName, s32x_fifa96RomInfo, s32x_fifa96RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Japan, USA)

static struct BurnRomInfo s32x_golfmagRomDesc[] = {
	{ "Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Japan, USA).32x", 0x300000, 0xd3d0a2fe, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_golfmag)
STD_ROM_FN(s32x_golfmag)

struct BurnDriver BurnDrvs32x_golfmag = {
	"s32x_golfmag", NULL, NULL, NULL, "1995",
	"Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_golfmagRomInfo, s32x_golfmagRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Europe)

static struct BurnRomInfo s32x_golfmageRomDesc[] = {
	{ "Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Europe).32x", 0x300000, 0x6ef99202, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_golfmage)
STD_ROM_FN(s32x_golfmage)

struct BurnDriver BurnDrvs32x_golfmage = {
	"s32x_golfmage", "s32x_golfmag", NULL, NULL, "1995",
	"Golf Magazine Presents - 36 Great Holes Starring Fred Couples (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_golfmageRomInfo, s32x_golfmageRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Kolibri (USA, Europe)

static struct BurnRomInfo s32x_kolibriRomDesc[] = {
	{ "Kolibri (USA, Europe).32x", 0x300000, 0x20ca53ef, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_kolibri)
STD_ROM_FN(s32x_kolibri)

struct BurnDriver BurnDrvs32x_kolibri = {
	"s32x_kolibri", NULL, NULL, NULL, "1995",
	"Kolibri (USA, Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_kolibriRomInfo, s32x_kolibriRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Mega Marble World 2 (World, Prototype)

static struct BurnRomInfo s32x_megamw2RomDesc[] = {
	{ "Mega Marble World 2 (World) (Proto) (Aftermarket) (Unl).32x", 0x07df0c, 0xc00c76e6, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_megamw2)
STD_ROM_FN(s32x_megamw2)

struct BurnDriver BurnDrvs32x_megamw2 = {
	"s32x_megamw2", NULL, NULL, NULL, "2020",
	"Mega Marble World 2 (World, Prototype)\0", NULL, "<unknown>", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_HOMEBREW | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_ACTION, 0,
	S32XGetZipName, s32x_megamw2RomInfo, s32x_megamw2RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Metal Head (Japan, USA)

static struct BurnRomInfo s32x_metalhedRomDesc[] = {
	{ "Metal Head (Japan, USA) (En,Ja).32x", 0x300000, 0xef5553ff, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_metalhed)
STD_ROM_FN(s32x_metalhed)

struct BurnDriver BurnDrvs32x_metalhed = {
	"s32x_metalhed", NULL, NULL, NULL, "1995",
	"Metal Head (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_metalhedRomInfo, s32x_metalhedRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Metal Head (Europe)

static struct BurnRomInfo s32x_metalhedeRomDesc[] = {
	{ "Metal Head (Europe) (En,Ja).32x", 0x300000, 0xbf9b8376, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_metalhede)
STD_ROM_FN(s32x_metalhede)

struct BurnDriver BurnDrvs32x_metalhede = {
	"s32x_metalhede", "s32x_metalhed", NULL, NULL, "1995",
	"Metal Head (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_metalhedeRomInfo, s32x_metalhedeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Mortal Kombat II (Japan, USA)

static struct BurnRomInfo s32x_mk2RomDesc[] = {
	{ "Mortal Kombat II (Japan, USA).32x", 0x400000, 0x773c0a85, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_mk2)
STD_ROM_FN(s32x_mk2)

struct BurnDriver BurnDrvs32x_mk2 = {
	"s32x_mk2", NULL, NULL, NULL, "1995",
	"Mortal Kombat II (Japan, USA)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_mk2RomInfo, s32x_mk2RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Mortal Kombat II (Europe)

static struct BurnRomInfo s32x_mk2eRomDesc[] = {
	{ "Mortal Kombat II (Europe).32x", 0x400000, 0x211085ce, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_mk2e)
STD_ROM_FN(s32x_mk2e)

struct BurnDriver BurnDrvs32x_mk2e = {
	"s32x_mk2e", "s32x_mk2", NULL, NULL, "1995",
	"Mortal Kombat II (Europe)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_mk2eRomInfo, s32x_mk2eRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Motocross Championship (USA)

static struct BurnRomInfo s32x_motoxchRomDesc[] = {
	{ "Motocross Championship (USA).32x", 0x200000, 0xa21c5761, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_motoxch)
STD_ROM_FN(s32x_motoxch)

struct BurnDriver BurnDrvs32x_motoxch = {
	"s32x_motoxch", NULL, NULL, NULL, "1994",
	"Motocross Championship (USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_motoxchRomInfo, s32x_motoxchRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Motocross Championship (Europe)

static struct BurnRomInfo s32x_motoxcheRomDesc[] = {
	{ "Motocross Championship (Europe).32x", 0x200000, 0xae3364e9, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_motoxche)
STD_ROM_FN(s32x_motoxche)

struct BurnDriver BurnDrvs32x_motoxche = {
	"s32x_motoxche", "s32x_motoxch", NULL, NULL, "1994",
	"Motocross Championship (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_motoxcheRomInfo, s32x_motoxcheRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// NBA Jam - Tournament Edition (World)

static struct BurnRomInfo s32x_nbajamteRomDesc[] = {
	{ "NBA Jam - Tournament Edition (World).32x", 0x400000, 0x6b7994aa, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_nbajamte)
STD_ROM_FN(s32x_nbajamte)

struct BurnDriver BurnDrvs32x_nbajamte = {
	"s32x_nbajamte", NULL, NULL, NULL, "1995",
	"NBA Jam - Tournament Edition (World)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X | HARDWARE_SEGA_MEGADRIVE_PCB_NBA_JAM_TE, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_nbajamteRomInfo, s32x_nbajamteRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// NFL Quarterback Club (World)

static struct BurnRomInfo s32x_nflqbcRomDesc[] = {
	{ "NFL Quarterback Club (World).32x", 0x300000, 0x0bc7018d, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_nflqbc)
STD_ROM_FN(s32x_nflqbc)

struct BurnDriver BurnDrvs32x_nflqbc = {
	"s32x_nflqbc", NULL, NULL, NULL, "1995",
	"NFL Quarterback Club (World)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X | HARDWARE_SEGA_MEGADRIVE_PCB_NBA_JAM_TE, GBF_SPORTSFOOTBALL, 0,
	S32XGetZipName, s32x_nflqbcRomInfo, s32x_nflqbcRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Pinocchio (Europe, Prototype 19950612)

static struct BurnRomInfo s32x_pinocchiRomDesc[] = {
	{ "Pinocchio (Europe) (Proto) (1995-06-12).32x", 0x400000, 0x390a90f9, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_pinocchi)
STD_ROM_FN(s32x_pinocchi)

struct BurnDriver BurnDrvs32x_pinocchi = {
	"s32x_pinocchi", NULL, NULL, NULL, "1995",
	"Pinocchio (Europe, Prototype 19950612)\0", NULL, "Virgin Interactive", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_pinocchiRomInfo, s32x_pinocchiRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Pitfall - The Mayan Adventure (USA)

static struct BurnRomInfo s32x_pitfallRomDesc[] = {
	{ "Pitfall - The Mayan Adventure (USA).32x", 0x300000, 0xf9126f15, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_pitfall)
STD_ROM_FN(s32x_pitfall)

struct BurnDriver BurnDrvs32x_pitfall = {
	"s32x_pitfall", NULL, NULL, NULL, "1995",
	"Pitfall - The Mayan Adventure (USA)\0", NULL, "Activision", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_pitfallRomInfo, s32x_pitfallRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Primal Rage (USA, Europe)

static struct BurnRomInfo s32x_primrageRomDesc[] = {
	{ "Primal Rage (USA, Europe).32x", 0x400000, 0xe78a4d28, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_primrage)
STD_ROM_FN(s32x_primrage)

struct BurnDriver BurnDrvs32x_primrage = {
	"s32x_primrage", NULL, NULL, NULL, "1995",
	"Primal Rage (USA, Europe)\0", NULL, "Time Warner Interactive", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_primrageRomInfo, s32x_primrageRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// RBI Baseball '95 (USA)

static struct BurnRomInfo s32x_rbi95RomDesc[] = {
	{ "RBI Baseball '95 (USA).32x", 0x200000, 0xff795fdc, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_rbi95)
STD_ROM_FN(s32x_rbi95)

struct BurnDriver BurnDrvs32x_rbi95 = {
	"s32x_rbi95", NULL, NULL, NULL, "1995",
	"RBI Baseball '95 (USA)\0", NULL, "Time Warner Interactive", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_rbi95RomInfo, s32x_rbi95RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Sangokushi IV (Japan)

static struct BurnRomInfo s32x_sangoku4RomDesc[] = {
	{ "Sangokushi IV (Japan).32x", 0x400000, 0xe4de7625, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_sangoku4)
STD_ROM_FN(s32x_sangoku4)

struct BurnDriver BurnDrvs32x_sangoku4 = {
	"s32x_sangoku4", NULL, NULL, NULL, "1995",
	"Sangokushi IV (Japan)\0", NULL, "Koei", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_STRATEGY, 0,
	S32XGetZipName, s32x_sangoku4RomInfo, s32x_sangoku4RomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Space Harrier (Japan, USA)

static struct BurnRomInfo s32x_sharrierRomDesc[] = {
	{ "Space Harrier (Japan, USA).32x", 0x200000, 0x86e7f989, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_sharrier)
STD_ROM_FN(s32x_sharrier)

struct BurnDriver BurnDrvs32x_sharrier = {
	"s32x_sharrier", NULL, NULL, NULL, "1994",
	"Space Harrier (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_sharrierRomInfo, s32x_sharrierRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Space Harrier (Europe)

static struct BurnRomInfo s32x_sharriereRomDesc[] = {
	{ "Space Harrier (Europe).32x", 0x200000, 0x5cac3587, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_sharriere)
STD_ROM_FN(s32x_sharriere)

struct BurnDriver BurnDrvs32x_sharriere = {
	"s32x_sharriere", "s32x_sharrier", NULL, NULL, "1994",
	"Space Harrier (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_sharriereRomInfo, s32x_sharriereRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Sonic the Hedgehog (World)

static struct BurnRomInfo s32x_sonicRomDesc[] = {
	{ "Sonic the Hedgehog (World) (Aftermarket) (Unl).32x", 0x0825c6, 0x2221c701, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_sonic)
STD_ROM_FN(s32x_sonic)

struct BurnDriver BurnDrvs32x_sonic = {
	"s32x_sonic", NULL, NULL, NULL, "2007",
	"Sonic the Hedgehog (World)\0", NULL, "<unknown>", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_HOMEBREW, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_sonicRomInfo, s32x_sonicRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Soulstar X (USA, Prototype)

static struct BurnRomInfo s32x_soulstrxRomDesc[] = {
	{ "Soulstar X (USA) (Proto).32x", 0x200000, 0x5ea52f22, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_soulstrx)
STD_ROM_FN(s32x_soulstrx)

struct BurnDriver BurnDrvs32x_soulstrx = {
	"s32x_soulstrx", NULL, NULL, NULL, "1995",
	"Soulstar X (USA, Prototype)\0", NULL, "Core Design", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_soulstrxRomInfo, s32x_soulstrxRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// The Amazing Spider-Man - Web of Fire (USA)

static struct BurnRomInfo s32x_spidwofRomDesc[] = {
	{ "Amazing Spider-Man, The - Web of Fire (USA).32x", 0x300000, 0x29dce257, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_spidwof)
STD_ROM_FN(s32x_spidwof)

struct BurnDriver BurnDrvs32x_spidwof = {
	"s32x_spidwof", NULL, NULL, NULL, "1996",
	"The Amazing Spider-Man - Web of Fire (USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	S32XGetZipName, s32x_spidwofRomInfo, s32x_spidwofRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Spot Goes to Hollywood (USA, Prototype)

static struct BurnRomInfo s32x_spotgohRomDesc[] = {
	{ "Spot Goes to Hollywood (USA) (Proto).32x", 0x300000, 0xce6c4b99, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_spotgoh)
STD_ROM_FN(s32x_spotgoh)

struct BurnDriver BurnDrvs32x_spotgoh = {
	"s32x_spotgoh", NULL, NULL, NULL, "1995",
	"Spot Goes to Hollywood (USA, Prototype)\0", NULL, "Virgin Interactive", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_spotgohRomInfo, s32x_spotgohRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Stellar Assault (USA, Europe)

static struct BurnRomInfo s32x_stelaslRomDesc[] = {
	{ "Stellar Assault (USA, Europe).32x", 0x200000, 0x60c49e4d, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_stelasl)
STD_ROM_FN(s32x_stelasl)

struct BurnDriver BurnDrvs32x_stelasl = {
	"s32x_stelasl", NULL, NULL, NULL, "1995",
	"Stellar Assault (USA, Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_stelaslRomInfo, s32x_stelaslRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Stellar Assault (Japan)

static struct BurnRomInfo s32x_stelasljRomDesc[] = {
	{ "Stellar Assault (Japan).32x", 0x200000, 0xfce4c8c7, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_stelaslj)
STD_ROM_FN(s32x_stelaslj)

struct BurnDriver BurnDrvs32x_stelaslj = {
	"s32x_stelaslj", "s32x_stelasl", NULL, NULL, "1995",
	"Stellar Assault (Japan)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_stelasljRomInfo, s32x_stelasljRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Star Trek - Starfleet Academy - Starship Bridge Simulator (USA)

static struct BurnRomInfo s32x_stfleetRomDesc[] = {
	{ "Star Trek - Starfleet Academy - Starship Bridge Simulator (USA).32x", 0x200000, 0xdd9708b9, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_stfleet)
STD_ROM_FN(s32x_stfleet)

struct BurnDriver BurnDrvs32x_stfleet = {
	"s32x_stfleet", NULL, NULL, NULL, "1995",
	"Star Trek - Starfleet Academy - Starship Bridge Simulator (USA)\0", NULL, "Interplay", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SIM, 0,
	S32XGetZipName, s32x_stfleetRomInfo, s32x_stfleetRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Star Wars Arcade (USA)

static struct BurnRomInfo s32x_swarcadeRomDesc[] = {
	{ "Star Wars Arcade (USA).32x", 0x280000, 0x2f16b44a, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_swarcade)
STD_ROM_FN(s32x_swarcade)

struct BurnDriver BurnDrvs32x_swarcade = {
	"s32x_swarcade", NULL, NULL, NULL, "1994",
	"Star Wars Arcade (USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_swarcadeRomInfo, s32x_swarcadeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Star Wars Arcade (Europe)

static struct BurnRomInfo s32x_swarcadeeRomDesc[] = {
	{ "Star Wars Arcade (Europe).32x", 0x280000, 0x82e82660, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_swarcadee)
STD_ROM_FN(s32x_swarcadee)

struct BurnDriver BurnDrvs32x_swarcadee = {
	"s32x_swarcadee", "s32x_swarcade", NULL, NULL, "1994",
	"Star Wars Arcade (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_swarcadeeRomInfo, s32x_swarcadeeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Star Wars Arcade (Japan)

static struct BurnRomInfo s32x_swarcadejRomDesc[] = {
	{ "Star Wars Arcade (Japan).32x", 0x280000, 0xf4e9b846, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_swarcadej)
STD_ROM_FN(s32x_swarcadej)

struct BurnDriver BurnDrvs32x_swarcadej = {
	"s32x_swarcadej", "s32x_swarcade", NULL, NULL, "1994",
	"Star Wars Arcade (Japan)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_swarcadejRomInfo, s32x_swarcadejRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Tempo (Japan, USA)

static struct BurnRomInfo s32x_tempoRomDesc[] = {
	{ "Tempo (Japan, USA).32x", 0x300000, 0x14e5c575, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_tempo)
STD_ROM_FN(s32x_tempo)

struct BurnDriver BurnDrvs32x_tempo = {
	"s32x_tempo", NULL, NULL, NULL, "1995",
	"Tempo (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_tempoRomInfo, s32x_tempoRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// T-MEK (USA, Europe)

static struct BurnRomInfo s32x_tmekRomDesc[] = {
	{ "T-MEK (USA, Europe).32x", 0x300000, 0x66d2c48f, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_tmek)
STD_ROM_FN(s32x_tmek)

struct BurnDriver BurnDrvs32x_tmek = {
	"s32x_tmek", NULL, NULL, NULL, "1995",
	"T-MEK (USA, Europe)\0", NULL, "Time Warner Interactive", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_tmekRomInfo, s32x_tmekRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Toughman Contest (USA, Europe)

static struct BurnRomInfo s32x_toughmanRomDesc[] = {
	{ "Toughman Contest (USA, Europe).32x", 0x400000, 0x14eac7a6, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_toughman)
STD_ROM_FN(s32x_toughman)

struct BurnDriver BurnDrvs32x_toughman = {
	"s32x_toughman", NULL, NULL, NULL, "1995",
	"Toughman Contest (USA, Europe)\0", NULL, "EA Sports", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_toughmanRomInfo, s32x_toughmanRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Fighter (Japan, USA)

static struct BurnRomInfo s32x_vfRomDesc[] = {
	{ "Virtua Fighter (Japan, USA).32x", 0x400000, 0xb5de9626, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vf)
STD_ROM_FN(s32x_vf)

struct BurnDriver BurnDrvs32x_vf = {
	"s32x_vf", NULL, NULL, NULL, "1995",
	"Virtua Fighter (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_vfRomInfo, s32x_vfRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Fighter (Europe)

static struct BurnRomInfo s32x_vfeRomDesc[] = {
	{ "Virtua Fighter (Europe).32x", 0x400000, 0x25aea73c, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vfe)
STD_ROM_FN(s32x_vfe)

struct BurnDriver BurnDrvs32x_vfe = {
	"s32x_vfe", "s32x_vf", NULL, NULL, "1995",
	"Virtua Fighter (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_vfeRomInfo, s32x_vfeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Hamster (USA, Prototype)

static struct BurnRomInfo s32x_vhamsterRomDesc[] = {
	{ "Virtua Hamster (USA) (Proto).32x", 0x200000, 0xcd54ac37, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vhamster)
STD_ROM_FN(s32x_vhamster)

struct BurnDriver BurnDrvs32x_vhamster = {
	"s32x_vhamster", NULL, NULL, NULL, "1995",
	"Virtua Hamster (USA, Prototype)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_ACTION, 0,
	S32XGetZipName, s32x_vhamsterRomInfo, s32x_vhamsterRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Racing Deluxe (USA)

static struct BurnRomInfo s32x_vrdxRomDesc[] = {
	{ "Virtua Racing Deluxe (USA).32x", 0x300000, 0x7896b62e, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vrdx)
STD_ROM_FN(s32x_vrdx)

struct BurnDriver BurnDrvs32x_vrdx = {
	"s32x_vrdx", NULL, NULL, NULL, "1994",
	"Virtua Racing Deluxe (USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_vrdxRomInfo, s32x_vrdxRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Racing Deluxe (Europe)

static struct BurnRomInfo s32x_vrdxeRomDesc[] = {
	{ "Virtua Racing Deluxe (Europe).32x", 0x300000, 0x27f14b5f, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vrdxe)
STD_ROM_FN(s32x_vrdxe)

struct BurnDriver BurnDrvs32x_vrdxe = {
	"s32x_vrdxe", "s32x_vrdx", NULL, NULL, "1994",
	"Virtua Racing Deluxe (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_vrdxeRomInfo, s32x_vrdxeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Virtua Racing Deluxe (Japan)

static struct BurnRomInfo s32x_vrdxjRomDesc[] = {
	{ "Virtua Racing Deluxe (Japan).32x", 0x300000, 0x0908e888, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_vrdxj)
STD_ROM_FN(s32x_vrdxj)

struct BurnDriver BurnDrvs32x_vrdxj = {
	"s32x_vrdxj", "s32x_vrdx", NULL, NULL, "1994",
	"Virtua Racing Deluxe (Japan)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 2, HARDWARE_SEGA_32X, GBF_RACING, 0,
	S32XGetZipName, s32x_vrdxjRomInfo, s32x_vrdxjRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// World Series Baseball Starring Deion Sanders (USA)

static struct BurnRomInfo s32x_wsbasebRomDesc[] = {
	{ "World Series Baseball Starring Deion Sanders (USA).32x", 0x300000, 0x6de1bc75, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_wsbaseb)
STD_ROM_FN(s32x_wsbaseb)

struct BurnDriver BurnDrvs32x_wsbaseb = {
	"s32x_wsbaseb", NULL, NULL, NULL, "1995",
	"World Series Baseball Starring Deion Sanders (USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_SPORTSMISC, 0,
	S32XGetZipName, s32x_wsbasebRomInfo, s32x_wsbasebRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// WWF Raw (World)

static struct BurnRomInfo s32x_wwfrawRomDesc[] = {
	{ "WWF Raw (World).32x", 0x400000, 0x8eb7cd2c, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_wwfraw)
STD_ROM_FN(s32x_wwfraw)

struct BurnDriver BurnDrvs32x_wwfraw = {
	"s32x_wwfraw", NULL, NULL, NULL, "1995",
	"WWF Raw (World)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_wwfrawRomInfo, s32x_wwfrawRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// WWF WrestleMania - The Arcade Game (USA)

static struct BurnRomInfo s32x_wwfwmaRomDesc[] = {
	{ "WWF WrestleMania - The Arcade Game (USA).32x", 0x400000, 0x61833503, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_wwfwma)
STD_ROM_FN(s32x_wwfwma)

struct BurnDriver BurnDrvs32x_wwfwma = {
	"s32x_wwfwma", NULL, NULL, NULL, "1995",
	"WWF WrestleMania - The Arcade Game (USA)\0", NULL, "Acclaim", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_SEGA_32X, GBF_VSFIGHT, 0,
	S32XGetZipName, s32x_wwfwmaRomInfo, s32x_wwfwmaRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// X-Men (USA, Prototype)
// early prototype: the slave sh-2 can loop forever on a sprite with a negative scale
// (object close to the camera), the game then freezes

static struct BurnRomInfo s32x_xmenRomDesc[] = {
	{ "X-Men (USA) (Proto).32x", 0x400000, 0xd61febc0, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_xmen)
STD_ROM_FN(s32x_xmen)

struct BurnDriverD BurnDrvs32x_xmen = {
	"s32x_xmen", NULL, NULL, NULL, "1995",
	"X-Men (USA, Prototype)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_16BIT_ONLY | BDF_PROTOTYPE, 1, HARDWARE_SEGA_32X, GBF_SCRFIGHT, 0,
	S32XGetZipName, s32x_xmenRomInfo, s32x_xmenRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// xRick (World, v1.1)

static struct BurnRomInfo s32x_xrickRomDesc[] = {
	{ "xRick (World) (v1.1) (Aftermarket) (Unl).32x", 0x400000, 0x79cb7f70, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_xrick)
STD_ROM_FN(s32x_xrick)

struct BurnDriver BurnDrvs32x_xrick = {
	"s32x_xrick", NULL, NULL, NULL, "2011",
	"xRick (World, v1.1)\0", NULL, "<unknown>", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_HOMEBREW, 1, HARDWARE_SEGA_32X, GBF_PLATFORM, 0,
	S32XGetZipName, s32x_xrickRomInfo, s32x_xrickRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Zaxxon's Motherbase 2000 (Japan, USA)

static struct BurnRomInfo s32x_zaxxonmbRomDesc[] = {
	{ "Zaxxon's Motherbase 2000 (Japan, USA) (En).32x", 0x200000, 0x447d44be, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_zaxxonmb)
STD_ROM_FN(s32x_zaxxonmb)

struct BurnDriver BurnDrvs32x_zaxxonmb = {
	"s32x_zaxxonmb", NULL, NULL, NULL, "1995",
	"Zaxxon's Motherbase 2000 (Japan, USA)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_zaxxonmbRomInfo, s32x_zaxxonmbRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};

// Motherbase (Europe)

static struct BurnRomInfo s32x_zaxxonmbeRomDesc[] = {
	{ "Motherbase (Europe).32x", 0x200000, 0x0d6523a3, BRF_PRG | SEGA_MD_ROM_LOAD16_WORD_SWAP | SEGA_MD_ROM_OFFS_000000 },
};

STD_ROM_PICK(s32x_zaxxonmbe)
STD_ROM_FN(s32x_zaxxonmbe)

struct BurnDriver BurnDrvs32x_zaxxonmbe = {
	"s32x_zaxxonmbe", "s32x_zaxxonmb", NULL, NULL, "1995",
	"Motherbase (Europe)\0", NULL, "Sega", "Sega 32X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY | BDF_CLONE, 1, HARDWARE_SEGA_32X, GBF_SHOOT, 0,
	S32XGetZipName, s32x_zaxxonmbeRomInfo, s32x_zaxxonmbeRomName, NULL, NULL, NULL, NULL, S32XInputInfo, S32XDIPInfo,
	MegadriveInit32X, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};
