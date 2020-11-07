#include "burnint.h"
#include "megadrive.h"

static struct BurnInputInfo SbubsmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	MegadriveJoy3 +  1, "p1 coin" 	},

	{"P1 Start",		BIT_DIGITAL,	MegadriveJoy1 +  7, "p1 start"  },
	{"P1 Up",			BIT_DIGITAL,	MegadriveJoy1 +  2, "p1 up"     },
	{"P1 Down",			BIT_DIGITAL,	MegadriveJoy2 +  6, "p1 down"   },
	{"P1 Left",			BIT_DIGITAL,	MegadriveJoy1 +  0, "p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	MegadriveJoy1 +  1, "p1 right"  },
	{"P1 Button A",		BIT_DIGITAL,	MegadriveJoy2 +  5, "p1 fire 1" },
	{"P1 Button B",		BIT_DIGITAL,	MegadriveJoy2 +  4, "p1 fire 2" },

	{"P2 Start",		BIT_DIGITAL,	MegadriveJoy1 +  5, "p2 start"  },
	{"P2 Up",			BIT_DIGITAL,	MegadriveJoy1 +  6, "p2 up"     },
	{"P2 Down",			BIT_DIGITAL,	MegadriveJoy2 +  2, "p2 down"   },
	{"P2 Left",			BIT_DIGITAL,	MegadriveJoy5 +  3, "p2 left"   },
	{"P2 Right",		BIT_DIGITAL,	MegadriveJoy5 +  4, "p2 right"  },
	{"P2 Button A",		BIT_DIGITAL,	MegadriveJoy1 +  4, "p2 fire 1" },
	{"P2 Button B",		BIT_DIGITAL,	MegadriveJoy1 +  3, "p2 fire 2" },

	{"Reset",			BIT_DIGITAL,	&MegadriveReset,     "reset"    },
};

STDINPUTINFO(Sbubsm)

static struct BurnInputInfo TopshootInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	MegadriveJoy3 +  1, "p1 coin" 	},
	{"P2 Coin",			BIT_DIGITAL,	MegadriveJoy4 +  1, "p1 coin" 	},

	{"P1 Start",		BIT_DIGITAL,	MegadriveJoy1 +  5, "p1 fire 2" },
	{"P1 Bet",			BIT_DIGITAL,	MegadriveJoy1 +  4, "p1 fire 1" },
	{"P1 Fire",			BIT_DIGITAL,	MegadriveJoy1 +  6, "p1 fire 2" },

	{"Service Mode",	BIT_DIGITAL,	MegadriveJoy2 +  3, "diag" 		},
	{"Test Mode Down",	BIT_DIGITAL,	MegadriveJoy2 +  4, "service 1" },

	{"Reset",			BIT_DIGITAL,	&MegadriveReset,     "reset"    },
};

STDINPUTINFO(Topshoot)

static INT32 mdarcblInit()
{
	INT32 rc = MegadriveInit();
	MegadriveDIP[0] = 0x00;
	MegadriveDIP[1] = 0x03;

	return rc;
}

// Super Bubble Bobble (Sun Mixing, Megadrive clone hardware)

static struct BurnRomInfo sbubsmRomDesc[] = {
	{ "u11.bin", 				0x080000, 0x4f9337ea, BRF_PRG | SEGA_MD_ROM_LOAD16_BYTE | SEGA_MD_ROM_OFFS_000001 },
	{ "u12.bin", 				0x080000, 0xf5374835, BRF_PRG | SEGA_MD_ROM_LOAD16_BYTE | SEGA_MD_ROM_OFFS_000000 },

	{ "89c51.bin", 				0x001000, 0x00000000, BRF_PRG | BRF_NODUMP },
};

STD_ROM_PICK(sbubsm)
STD_ROM_FN(sbubsm)

struct BurnDriver BurnDrvSbubsm = {
	"sbubsm", NULL, NULL, NULL, "199?",
	"Super Bubble Bobble (Sun Mixing, Megadrive clone hardware)\0", NULL, "Sun Mixing", "Sega Megadrive",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S | HARDWARE_SEGA_MEGADRIVE_PCB_SBUBBOB | HARDWARE_SEGA_MEGADRIVE_SRAM_04000 | SEGA_MD_ARCADE_SUNMIXING, GBF_MISC, 0,
	NULL, sbubsmRomInfo, sbubsmRomName, NULL, NULL, NULL, NULL, SbubsmInputInfo, NULL,
	mdarcblInit, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};


// Top Shooter

static struct BurnRomInfo topshootRomDesc[] = {
	{ "tc574000ad_u11_2.bin", 0x080000, 0xb235c4d9, BRF_PRG | SEGA_MD_ROM_LOAD16_BYTE | SEGA_MD_ROM_OFFS_000001  },
	{ "tc574000ad_u12_1.bin", 0x080000, 0xe826f6ad, BRF_PRG | SEGA_MD_ROM_LOAD16_BYTE | SEGA_MD_ROM_OFFS_000000  },

	{ "89c51.bin", 			  0x001000, 0x7e41c8fe, BRF_PRG | BRF_NODUMP  },
};

STD_ROM_PICK(topshoot)
STD_ROM_FN(topshoot)

struct BurnDriver BurnDrvTopshoot = {
	"topshoot", NULL, NULL, NULL, "1993",
	"Top Shooter\0", NULL, "Sun Mixing", "Sega Megadrive",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S | HARDWARE_SEGA_MEGADRIVE_SRAM_04000 | SEGA_MD_ARCADE_SUNMIXING, GBF_MISC, 0,
	NULL, topshootRomInfo, topshootRomName, NULL, NULL, NULL, NULL, TopshootInputInfo, NULL,
	mdarcblInit, MegadriveExit, MegadriveFrame, MegadriveDraw, MegadriveScan,
	&bMegadriveRecalcPalette, 0x100, 320, 224, 4, 3
};
