
#include "driver.h"
#include "burnint.h"
#include "midwunit.h"

static struct BurnInputInfo mk3InputList[] = {
    {"P1 Coin",		   BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"	},
    {"P1 Start",	   BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"	},
    {"P1 Up",		   BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"		},
    {"P1 Down",		   BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"	},
    {"P1 Left",		   BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"	},
    {"P1 Right",	   BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"	},
    {"P1 High Punch",  BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 1"	},
    {"P1 High Kick",   BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 2"	},
    {"P1 Low Punch",   BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p1 fire 3"	},
    {"P1 Low Kick",	   BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p1 fire 4"	},
    {"P1 Run",	       BIT_DIGITAL,	nWolfUnitJoy2 + 2,	"p1 fire 5"	},
    {"P1 Block",	   BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 6"	},

    {"P2 Coin",		   BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"	},
    {"P2 Start",	   BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"	},
    {"P2 Up",		   BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"		},
    {"P2 Down",	   	   BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"	},
    {"P2 Left",		   BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"	},
    {"P2 Right",	   BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"	},
    {"P2 High Punch",  BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 1"	},
    {"P2 High Kick",   BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 2"	},
    {"P2 Low Punch",   BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p2 fire 3"	},
    {"P2 Low Kick",	   BIT_DIGITAL,	nWolfUnitJoy2 + 6,	"p2 fire 4"	},
    {"P2 Run",	       BIT_DIGITAL,	nWolfUnitJoy2 + 7,	"p2 fire 5"	},
    {"P2 Block",	   BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 6"	},
    {"Test",           BIT_DIGITAL, nWolfUnitDSW + 0,   "diag"      },
};

STDINPUTINFO(mk3)

static struct BurnDIPInfo mk3DIPList[1]=
{
};

STDDIPINFO(mk3)


static struct BurnRomInfo umk3RomDesc[] = {
    { "um312u54.bin",		0x80000, 0x712b4db6, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
    { "um312u63.bin",		0x80000, 0x6d301faf, 1 | BRF_PRG | BRF_ESS }, //  1 TMS34010

    { "umk3-u2.bin",	   0x100000, 0x3838cfe5, 2 | BRF_SND | BRF_ESS }, //  2 DCS sound banks
    { "mk3-u3.bin",		   0x100000, 0x856fe411, 2 | BRF_SND | BRF_ESS }, //  3
    { "mk3-u4.bin",		   0x100000, 0x428a406f, 2 | BRF_SND | BRF_ESS }, //  4
    { "mk3-u5.bin",		   0x100000, 0x3b98a09f, 2 | BRF_SND | BRF_ESS }, //  5

    { "mk3-u133.bin",      0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX 
    { "mk3-u132.bin",      0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
    { "mk3-u131.bin",      0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
    { "mk3-u130.bin",      0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

    { "mk3-u129.bin",      0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, //  10
    { "mk3-u128.bin",      0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, //  11
    { "mk3-u127.bin",      0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, //  12
    { "mk3-u126.bin",      0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, //  13

    { "mk3-u125.bin",      0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, //  14
    { "mk3-u124.bin",      0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, //  15
    { "mk3-u123.bin",      0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, //  16
    { "mk3-u122.bin",      0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, //  17

    { "umk-u121.bin",      0x100000, 0xcc4b95db, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, //  18
    { "umk-u120.bin",      0x100000, 0x1c8144cd, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, //  19
    { "umk-u119.bin",      0x100000, 0x5f10c543, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, //  20
    { "umk-u118.bin",      0x100000, 0xde0c4488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, //  21
   
    { "umk-u113.bin",      0x100000, 0x99d74a1e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, //  22
    { "umk-u112.bin",      0x100000, 0xb5a46488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, //  23
    { "umk-u111.bin",      0x100000, 0xa87523c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, //  24
    { "umk-u110.bin",      0x100000, 0x0038f205, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, //  25
   
};

STD_ROM_PICK(umk3)
STD_ROM_FN(umk3)

struct BurnDriver BurnDrvUmk3 = {
    "umk3", NULL, NULL, NULL, "1994",
    "Ultimate Mortal Kombat 3\0", NULL, "Midway", "MIDWAY Wolf-Unit",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
    NULL, umk3RomInfo, umk3RomName, NULL, NULL, mk3InputInfo, mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};
