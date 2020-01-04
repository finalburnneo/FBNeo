/***************************************************************
 Power Instinct
 (C) 1993 Atlus
 driver by Luca Elia (l.elia@tin.it)
 ***************************************************************
 port to Finalburn Alpha by OopsWare. 2007
 ***************************************************************/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "nmk112.h"
#include "burn_ym2203.h"

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;

static UINT8 *Rom68K;
static UINT8 *RomZ80;
static UINT8 *RomBg;
static UINT8 *RomFg;
static UINT8 *RomSpr;

static UINT8 *RamZ80;
static UINT16 *RamPal;
static UINT16 *RamBg;
static UINT16 *RamFg;
static UINT16 *RamSpr;
static UINT16 *RamVReg;

static UINT32 *RamCurPal;

static UINT8 DrvButton[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvJoy1[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvJoy2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInput[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static UINT8 bRecalcPalette = 0;
static UINT8 DrvReset = 0;

static UINT32 tile_bank;
static INT32 m6295size;
static UINT16 soundlatch = 0;
static INT32 oki_bank = -1;

#define	GAME_POWERINS	1
#define	GAME_POWERINA	2
#define	GAME_POWERINB	3
#define GAME_POWERINC	4

static INT32 game_drv = 0;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

inline static void CalcCol(INT32 idx)
{
	/* RRRR GGGG BBBB RGBx */
	UINT16 wordValue = RamPal[idx];
	INT32 r = ((wordValue >> 8) & 0xF0 ) | ((wordValue << 0) & 0x08) | ((wordValue >> 13) & 0x07 );
	INT32 g = ((wordValue >> 4) & 0xF0 ) | ((wordValue << 1) & 0x08) | ((wordValue >>  9) & 0x07 );
	INT32 b = ((wordValue >> 0) & 0xF0 ) | ((wordValue << 2) & 0x08) | ((wordValue >>  5) & 0x07 );
	RamCurPal[idx] = BurnHighCol(r, g, b, 0);
}

static struct BurnInputInfo powerinsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvButton + 3,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},
	{"P1 Button 4",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvButton + 4,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},
	{"P2 Button 4",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Diagnostics",	BIT_DIGITAL,    DrvButton + 5,	"diag"},
	{"Service",		BIT_DIGITAL,    DrvButton + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"},
};

STDINPUTINFO(powerins)

static struct BurnDIPInfo powerinsDIPList[] = {

	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Free play"},
	{0x17,	0x01, 0x01,	0x00, "Off"},
	{0x17,	0x01, 0x01,	0x01, "On"},
	{0,		0xFE, 0,	8,	  "Coin 1"},
	{0x17,	0x01, 0x70, 0x30, "4 coins 1 credit"},
	{0x17,	0x01, 0x70, 0x50, "3 coins 1 credit"},
	{0x17,	0x01, 0x70, 0x10, "2 coins 1 credit"},
	{0x17,	0x01, 0x70, 0x00, "1 coin 1 credit"},
	{0x17,	0x01, 0x70, 0x40, "1 coin 2 credits"},
	{0x17,	0x01, 0x70, 0x20, "1 coin 3 credits"},
	{0x17,	0x01, 0x70, 0x60, "1 coin 4 credits"},
	{0x17,	0x01, 0x70, 0x70, "2 coins to start, 1 to continue"},
	{0,		0xFE, 0,	8,	  "Coin 2"},
	{0x17,	0x01, 0x0E, 0x06, "4 coins 1 credit"},
	{0x17,	0x01, 0x0E, 0x0A, "3 coins 1 credit"},
	{0x17,	0x01, 0x0E, 0x02, "2 coins 1 credit"},
	{0x17,	0x01, 0x0E, 0x00, "1 coin 1 credit"},
	{0x17,	0x01, 0x0E, 0x08, "1 coin 2 credits"},
	{0x17,	0x01, 0x0E, 0x04, "1 coin 3 credits"},
	{0x17,	0x01, 0x0E, 0x0C, "1 coin 4 credits"},
	{0x17,	0x01, 0x0E, 0x0E, "2 coins to start, 1 to continue"},
	{0,		0xFE, 0,	2,	  "Flip screen"},
	{0x17,	0x01, 0x80,	0x00, "Off"},
	{0x17,	0x01, 0x80,	0x80, "On"},

};

static struct BurnDIPInfo powerinaDIPList[] = {

	// Defaults
	{0x18,	0xFF, 0xFF,	0x04, NULL},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Coin chutes"},
	{0x18,	0x01, 0x01, 0x00, "1 chute"},
	{0x18,	0x01, 0x01, 0x01, "2 chutes"},
	{0,		0xFE, 0,	2,	  "Join In mode"},
	{0x18,	0x01, 0x02, 0x00, "Off"},
	{0x18,	0x01, 0x02, 0x02, "On"},
	{0,		0xFE, 0,	2,	  "Demo sounds"},
	{0x18,	0x01, 0x04, 0x00, "Off"},
	{0x18,	0x01, 0x04, 0x04, "On"},
	{0,		0xFE, 0,	2,	  "Allow continue"},
	{0x18,	0x01, 0x08, 0x08, "Off"},
	{0x18,	0x01, 0x08, 0x00, "On"},
	{0,		0xFE, 0,	2,	  "Blood color"},
	{0x18,	0x01, 0x10, 0x00, "Red"},
	{0x18,	0x01, 0x10, 0x10, "Blue"},
	{0,		0xFE, 0,	2,	  "Game time"},
	{0x18,	0x01, 0x20, 0x00, "Normal"},
	{0x18,	0x01, 0x20, 0x20, "Short"},
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x18,	0x01, 0xC0, 0x80, "Easy"},
	{0x18,	0x01, 0xC0, 0x00, "Normal"},
	{0x18,	0x01, 0xC0, 0x40, "Hard"},
	{0x18,	0x01, 0xC0, 0xC0, "Hardest"},

};

static struct BurnDIPInfo powerinjDIPList[] = {

	// Defaults
	{0x18,	0xFF, 0xFF,	0x04, NULL},

	// DIP 2
//	{0,		0xFE, 0,	2,	  "Unknown"},
//	{0x18,	0x01, 0x01, 0x00, "Off"},
//	{0x18,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	2,	  "Join In mode"},
	{0x18,	0x01, 0x02, 0x00, "Off"},
	{0x18,	0x01, 0x02, 0x02, "On"},
	{0,		0xFE, 0,	2,	  "Demo sounds"},
	{0x18,	0x01, 0x04, 0x00, "Off"},
	{0x18,	0x01, 0x04, 0x04, "On"},
	{0,		0xFE, 0,	2,	  "Allow continue"},
	{0x18,	0x01, 0x08, 0x08, "Off"},
	{0x18,	0x01, 0x08, 0x00, "On"},
	{0,		0xFE, 0,	2,	  "Blood color"},
	{0x18,	0x01, 0x10, 0x00, "Red"},
	{0x18,	0x01, 0x10, 0x10, "Blue"},
	{0,		0xFE, 0,	2,	  "Game time"},
	{0x18,	0x01, 0x20, 0x00, "Normal"},
	{0x18,	0x01, 0x20, 0x20, "Short"},
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x18,	0x01, 0xC0, 0x80, "Easy"},
	{0x18,	0x01, 0xC0, 0x00, "Normal"},
	{0x18,	0x01, 0xC0, 0x40, "Hard"},
	{0x18,	0x01, 0xC0, 0xC0, "Hardest"},

};

STDDIPINFOEXT(powerins, powerins, powerina)
STDDIPINFOEXT(powerinj, powerins, powerinj)

// Rom information

static struct BurnRomInfo powerinsRomDesc[] = {
	{ "93095-3a.u108",	0x080000, 0x9825ea3d, BRF_ESS | BRF_PRG },	//  0 68k code
	{ "93095-4.u109", 	0x080000, 0xd3d7a782, BRF_ESS | BRF_PRG },	//  1

	{ "93095-2.u90",	0x020000, 0x4b123cc6, BRF_ESS | BRF_PRG },	//  2 Z80 code

	{ "93095-5.u16",	0x100000, 0xb1371808, BRF_GRA }, 			//  3 layer 0
	{ "93095-6.u17",	0x100000, 0x29c85d80, BRF_GRA },			//  4
	{ "93095-7.u18",	0x080000, 0x2dd76149, BRF_GRA },			//  5

	{ "93095-1.u15",	0x020000, 0x6a579ee0, BRF_GRA }, 			//  6 layer 1

	{ "93095-12.u116",	0x100000, 0x35f3c2a3, BRF_GRA },			//  7 sprites
	{ "93095-13.u117",	0x100000, 0x1ebd45da, BRF_GRA },			//  8
	{ "93095-14.u118",	0x100000, 0x760d871b, BRF_GRA },			//  9
	{ "93095-15.u119",	0x100000, 0xd011be88, BRF_GRA },			// 10
	{ "93095-16.u120",	0x100000, 0xa9c16c9c, BRF_GRA },			// 11
	{ "93095-17.u121",	0x100000, 0x51b57288, BRF_GRA },			// 12
	{ "93095-18.u122",	0x100000, 0xb135e3f2, BRF_GRA },			// 13
	{ "93095-19.u123",	0x100000, 0x67695537, BRF_GRA },			// 14

	{ "93095-10.u48",	0x100000, 0x329ac6c5, BRF_SND }, 			// 15 sound 1
	{ "93095-11.u49",	0x100000, 0x75d6097c, BRF_SND },			// 16

	{ "93095-8.u46",	0x100000, 0xf019bedb, BRF_SND }, 			// 17 sound 2
	{ "93095-9.u47",	0x100000, 0xadc83765, BRF_SND },			// 18

	{ "22.u81",			0x000020, 0x67d5ec4b, BRF_OPT },			// 19 unknown
	{ "21.u71",			0x000100, 0x182cd81f, BRF_OPT },			// 20
	{ "20.u54",			0x000100, 0x38bd0e2f, BRF_OPT },			// 21

};

STD_ROM_PICK(powerins)
STD_ROM_FN(powerins)

static struct BurnRomInfo powerinjRomDesc[] = {
	{ "93095-3j.u108",	0x080000, 0x3050a3fb, BRF_ESS | BRF_PRG },	//  0 68k code
	{ "93095-4.u109", 	0x080000, 0xd3d7a782, BRF_ESS | BRF_PRG },	//  1

	{ "93095-2.u90",	0x020000, 0x4b123cc6, BRF_ESS | BRF_PRG },	//  2 Z80 code

	{ "93095-5.u16",	0x100000, 0xb1371808, BRF_GRA }, 			//  3 layer 0
	{ "93095-6.u17",	0x100000, 0x29c85d80, BRF_GRA },			//  4
	{ "93095-7.u18",	0x080000, 0x2dd76149, BRF_GRA },			//  5

	{ "93095-1.u15",	0x020000, 0x6a579ee0, BRF_GRA }, 			//  6 layer 1

	{ "93095-12.u116",	0x100000, 0x35f3c2a3, BRF_GRA },			//  7 sprites
	{ "93095-13.u117",	0x100000, 0x1ebd45da, BRF_GRA },			//  8
	{ "93095-14.u118",	0x100000, 0x760d871b, BRF_GRA },			//  9
	{ "93095-15.u119",	0x100000, 0xd011be88, BRF_GRA },			// 10
	{ "93095-16.u120",	0x100000, 0xa9c16c9c, BRF_GRA },			// 11
	{ "93095-17.u121",	0x100000, 0x51b57288, BRF_GRA },			// 12
	{ "93095-18.u122",	0x100000, 0xb135e3f2, BRF_GRA },			// 13
	{ "93095-19.u123",	0x100000, 0x67695537, BRF_GRA },			// 14

	{ "93095-10.u48",	0x100000, 0x329ac6c5, BRF_SND }, 			// 15 sound 1
	{ "93095-11.u49",	0x100000, 0x75d6097c, BRF_SND },			// 16

	{ "93095-8.u46",	0x100000, 0xf019bedb, BRF_SND }, 			// 17 sound 2
	{ "93095-9.u47",	0x100000, 0xadc83765, BRF_SND },			// 18

	{ "22.u81",			0x000020, 0x67d5ec4b, BRF_OPT },			// 19 unknown
	{ "21.u71",			0x000100, 0x182cd81f, BRF_OPT },			// 20
	{ "20.u54",			0x000100, 0x38bd0e2f, BRF_OPT },			// 21

};

STD_ROM_PICK(powerinj)
STD_ROM_FN(powerinj)

static struct BurnRomInfo powerinspuRomDesc[] = {
	{ "3.p000.v4.0a.u116.27c240",	0x80000, 0xd1dd5a3f, 1 | BRF_PRG | BRF_ESS }, //  0 68000 code
	{ "4.p000.v3.8.u117.27c4096",	0x80000, 0x9c0f23cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.sound 9.20.u74.27c1001",	0x20000, 0x4b123cc6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ba0.s0.27c040",		0x80000, 0x1975b4b8, 3 | BRF_GRA },           		  //  3 layer 0
	{ "ba1.s1.27c040",		0x80000, 0x376e4919, 3 | BRF_GRA },           		  //  4
	{ "ba2.s2.27c040",		0x80000, 0x0d5ff532, 3 | BRF_GRA },           		  //  5
	{ "ba3.s3.27c040",		0x80000, 0x99b25791, 3 | BRF_GRA },           		  //  6
	{ "ba4.s4.27c040",		0x80000, 0x2dd76149, 3 | BRF_GRA },           		  //  7

	{ "1.text 1080.u16.27c010",	0x20000, 0x6a579ee0, 4 | BRF_GRA },          	  //  8 layer 1

	{ "fo0.mo0.27c040",		0x80000, 0x8b9b89c9, 5 | BRF_GRA },           		  //  9 sprites
	{ "fe0.me0.27c040",		0x80000, 0x4d127bdf, 5 | BRF_GRA },           		  // 10
	{ "fo1.mo1.27c040",		0x80000, 0x298eb50e, 5 | BRF_GRA },           		  // 11
	{ "fe1.me1.27c040",		0x80000, 0x57e6d283, 5 | BRF_GRA },           		  // 12
	{ "fo2.mo2.27c040",		0x80000, 0xfb184167, 5 | BRF_GRA },           		  // 13
	{ "fe2.me2.27c040",		0x80000, 0x1b752a4d, 5 | BRF_GRA },           		  // 14
	{ "fo3.mo3.27c040",		0x80000, 0x2f26ba7b, 5 | BRF_GRA },           		  // 15
	{ "fe3.me3.27c040",		0x80000, 0x0263d89b, 5 | BRF_GRA },           		  // 16
	{ "fo4.mo4.27c040",		0x80000, 0xc4633294, 5 | BRF_GRA },           		  // 17
	{ "fe4.me4.27c040",		0x80000, 0x5e4b5655, 5 | BRF_GRA },           		  // 18
	{ "fo5.mo5.27c040",		0x80000, 0x4d4b0e4e, 5 | BRF_GRA },           		  // 19
	{ "fe5.me5.27c040",		0x80000, 0x7e9f2d2b, 5 | BRF_GRA },           		  // 20
	{ "fo6.mo6.27c040",		0x80000, 0x0e7671f2, 5 | BRF_GRA },           		  // 21
	{ "fe6.me6.27c040",		0x80000, 0xee59b1ec, 5 | BRF_GRA },           		  // 22
	{ "fo7.mo7.27c040",		0x80000, 0x9ab1998c, 5 | BRF_GRA },           		  // 23
	{ "fe7.me7.27c040",		0x80000, 0x1ab0c88a, 5 | BRF_GRA },           		  // 24

	{ "ao0.ad00.27c040",	0x80000, 0x8cd6824e, 6 | BRF_SND },           		  // 25 sound 1
	{ "ao1.ad01.27c040",	0x80000, 0xe31ae04d, 6 | BRF_SND },           		  // 26
	{ "ao2.ad02.27c040",	0x80000, 0xc4c9f599, 6 | BRF_SND },           		  // 27
	{ "ao3.ad03.27c040",	0x80000, 0xf0a9f0e1, 6 | BRF_SND },           		  // 28

	{ "ad10.ad10.27c040",	0x80000, 0x62557502, 7 | BRF_SND },           		  // 29 sound 2
	{ "ad11.ad11.27c040",	0x80000, 0xdbc86bd7, 7 | BRF_SND },           		  // 30
	{ "ad12.ad12.27c040",	0x80000, 0x5839a2bd, 7 | BRF_SND },           		  // 31
	{ "ad13.ad13.27c040",	0x80000, 0x446f9dc3, 7 | BRF_SND },           		  // 32

	{ "22.u81",		0x00020, 0x67d5ec4b, 8 | BRF_OPT },           				  // 33 proms
	{ "21.u71",		0x00100, 0x182cd81f, 8 | BRF_OPT },           				  // 34
	{ "20.u54",		0x00100, 0x38bd0e2f, 8 | BRF_OPT },           				  // 35
};

STD_ROM_PICK(powerinspu)
STD_ROM_FN(powerinspu)

static struct BurnRomInfo powerinspjRomDesc[] = {
	{ "3.p000.pc_j_12-1_155e.u116",	0x80000, 0x4ea18490, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "4.p000.f_p4.u117",			0x80000, 0x9c0f23cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.sound 9.20.u74.27c1001",	0x20000, 0x4b123cc6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ba0.s0.27c040",		0x80000, 0x1975b4b8, 3 | BRF_GRA },           		  //  3 layer 0
	{ "ba1.s1.27c040",		0x80000, 0x376e4919, 3 | BRF_GRA },           		  //  4
	{ "ba2.s2.27c040",		0x80000, 0x0d5ff532, 3 | BRF_GRA },           		  //  5
	{ "ba3.s3.27c040",		0x80000, 0x99b25791, 3 | BRF_GRA },           		  //  6
	{ "ba4.s4.27c040",		0x80000, 0x2dd76149, 3 | BRF_GRA },           		  //  7

	{ "1.text 1080.u16.27c010",		0x20000, 0x6a579ee0, 4 | BRF_GRA },           //  8 layer 1

	{ "fo0.mo0.27c040",		0x80000, 0x8b9b89c9, 5 | BRF_GRA },           		  //  9 sprites
	{ "fe0.me0.27c040",		0x80000, 0x4d127bdf, 5 | BRF_GRA },           		  // 10
	{ "fo1.mo1.27c040",		0x80000, 0x298eb50e, 5 | BRF_GRA },           		  // 11
	{ "fe1.me1.27c040",		0x80000, 0x57e6d283, 5 | BRF_GRA },           		  // 12
	{ "fo2.mo2.27c040",		0x80000, 0xfb184167, 5 | BRF_GRA },           		  // 13
	{ "fe2.me2.27c040",		0x80000, 0x1b752a4d, 5 | BRF_GRA },           		  // 14
	{ "fo3.mo3.27c040",		0x80000, 0x2f26ba7b, 5 | BRF_GRA },           		  // 15
	{ "fe3.me3.27c040",		0x80000, 0x0263d89b, 5 | BRF_GRA },           		  // 16
	{ "fo4.mo4.27c040",		0x80000, 0xc4633294, 5 | BRF_GRA },           		  // 17
	{ "fe4.me4.27c040",		0x80000, 0x5e4b5655, 5 | BRF_GRA },           		  // 18
	{ "fo5.mo5.27c040",		0x80000, 0x4d4b0e4e, 5 | BRF_GRA },           		  // 19
	{ "fe5.me5.27c040",		0x80000, 0x7e9f2d2b, 5 | BRF_GRA },           		  // 20
	{ "fo6.mo6.27c040",		0x80000, 0x0e7671f2, 5 | BRF_GRA },           		  // 21
	{ "fe6.me6.27c040",		0x80000, 0xee59b1ec, 5 | BRF_GRA },           		  // 22
	{ "fo7.mo7.27c040",		0x80000, 0x9ab1998c, 5 | BRF_GRA },           		  // 23
	{ "fe7.me7.27c040",		0x80000, 0x1ab0c88a, 5 | BRF_GRA },           		  // 24

	{ "ao0.ad00.27c040",	0x80000, 0x8cd6824e, 6 | BRF_SND },           		  // 25 sound 1
	{ "ao1.ad01.27c040",	0x80000, 0xe31ae04d, 6 | BRF_SND },           		  // 26
	{ "ao2.ad02.27c040",	0x80000, 0xc4c9f599, 6 | BRF_SND },           		  // 27
	{ "ao3.ad03.27c040",	0x80000, 0xf0a9f0e1, 6 | BRF_SND },           		  // 28

	{ "ad10.ad10.27c040",	0x80000, 0x62557502, 7 | BRF_SND },           		  // 29 sound 2
	{ "ad11.ad11.27c040",	0x80000, 0xdbc86bd7, 7 | BRF_SND },           		  // 30
	{ "ad12.ad12.27c040",	0x80000, 0x5839a2bd, 7 | BRF_SND },           		  // 31
	{ "ad13.ad13.27c040",	0x80000, 0x446f9dc3, 7 | BRF_SND },           		  // 32

	{ "22.u81",		0x00020, 0x67d5ec4b, 8 | BRF_OPT },           			  	  // 33 proms
	{ "21.u71",		0x00100, 0x182cd81f, 8 | BRF_OPT },           				  // 34
	{ "20.u54",		0x00100, 0x38bd0e2f, 8 | BRF_OPT },           				  // 35
};

STD_ROM_PICK(powerinspj)
STD_ROM_FN(powerinspj)

static struct BurnRomInfo powerinaRomDesc[] = {
	{ "rom1",		0x080000, 0xb86c84d6, BRF_ESS | BRF_PRG },	//  0 68k code
	{ "rom2",		0x080000, 0xd3d7a782, BRF_ESS | BRF_PRG },	//  1

	{ "rom6",		0x200000, 0xb6c10f80, BRF_GRA }, 			//  2 layer 0
	{ "rom4",		0x080000, 0x2dd76149, BRF_GRA },			//	3

	{ "rom3",		0x020000, 0x6a579ee0, BRF_GRA }, 			//  4 layer 1

	{ "rom10",		0x200000, 0xefad50e8, BRF_GRA }, 			//  5 sprites
	{ "rom9",		0x200000, 0x08229592, BRF_GRA },			//  6
	{ "rom8",		0x200000, 0xb02fdd6d, BRF_GRA },			//  7
	{ "rom7",		0x200000, 0x92ab9996, BRF_GRA },			//  8

	{ "rom5",		0x080000, 0x88579c8f, BRF_SND }, 			//  9 sound 1
};

STD_ROM_PICK(powerina)
STD_ROM_FN(powerina)

static struct BurnRomInfo powerinbRomDesc[] = {
	{ "2q.bin",		  	0x080000, 0x11bf3f2a, BRF_ESS | BRF_PRG },	//  0 68k code
	{ "2r.bin", 	  	0x080000, 0xd8d621be, BRF_ESS | BRF_PRG },	//  1

	{ "1f.bin",		  	0x020000, 0x4b123cc6, BRF_ESS | BRF_PRG },	//  2 Z80 code

	{ "13k.bin",	  	0x080000, 0x1975b4b8, BRF_GRA }, 			//  3 layer 0
	{ "13l.bin",	  	0x080000, 0x376e4919, BRF_GRA },			//  4
	{ "13o.bin",	  	0x080000, 0x0d5ff532, BRF_GRA },			//  5
	{ "13q.bin",	  	0x080000, 0x99b25791, BRF_GRA },			//  6
	{ "13r.bin",	  	0x080000, 0x2dd76149, BRF_GRA },			//  7

	{ "6n.bin",		  	0x020000, 0x6a579ee0, BRF_GRA }, 			//  8 layer 1

	{ "14g.bin",	  	0x080000, 0x8b9b89c9, BRF_GRA },			//  9 sprites
	{ "11g.bin",	  	0x080000, 0x4d127bdf, BRF_GRA },			// 10 
	{ "13g.bin",	  	0x080000, 0x298eb50e, BRF_GRA },			// 11
	{ "11i.bin",	  	0x080000, 0x57e6d283, BRF_GRA },			// 12
	{ "12g.bin",	  	0x080000, 0xfb184167, BRF_GRA },			// 13
	{ "11j.bin",	  	0x080000, 0x1b752a4d, BRF_GRA },			// 14
	{ "14m.bin",	  	0x080000, 0x2f26ba7b, BRF_GRA },			// 15
	{ "11k.bin",	  	0x080000, 0x0263d89b, BRF_GRA },			// 16
	{ "14n.bin",	  	0x080000, 0xc4633294, BRF_GRA },			// 17
	{ "11l.bin",	  	0x080000, 0x5e4b5655, BRF_GRA },			// 18
	{ "14p.bin",	  	0x080000, 0x4d4b0e4e, BRF_GRA },			// 19
	{ "11o.bin",	  	0x080000, 0x7e9f2d2b, BRF_GRA },			// 20
	{ "13p.bin",	  	0x080000, 0x0e7671f2, BRF_GRA },			// 21
	{ "11p.bin",	  	0x080000, 0xee59b1ec, BRF_GRA },			// 22
	{ "12p.bin",	  	0x080000, 0x9ab1998c, BRF_GRA },			// 23
	{ "11q.bin",	  	0x080000, 0x1ab0c88a, BRF_GRA },			// 24

	{ "4a.bin",		  	0x080000, 0x8cd6824e, BRF_SND }, 			// 25 sound 1
	{ "4b.bin",		  	0x080000, 0xe31ae04d, BRF_SND },			// 26
	{ "4c.bin",		  	0x080000, 0xc4c9f599, BRF_SND },			// 27
	{ "4d.bin",		  	0x080000, 0xf0a9f0e1, BRF_SND },			// 28

	{ "5a.bin",		  	0x080000, 0x62557502, BRF_SND }, 			// 29 sound 2
	{ "5b.bin",		  	0x080000, 0xdbc86bd7, BRF_SND },			// 30
	{ "5c.bin",		  	0x080000, 0x5839a2bd, BRF_SND },			// 31
	{ "5d.bin",		  	0x080000, 0x446f9dc3, BRF_SND },			// 32

	{ "82s123.bin",	  	0x000020, 0x67d5ec4b, BRF_OPT },			// 33 unknown
	{ "82s147.bin",	  	0x000200, 0xd7818542, BRF_OPT },			// 34

};

STD_ROM_PICK(powerinb)
STD_ROM_FN(powerinb)

static struct BurnRomInfo powerinscRomDesc[] = {
	{ "10.040.u41",		0x80000, 0x88e1244b, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "11.040.u39",		0x80000, 0x46cd506f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.010.u2",		0x20000, 0x4b123cc6, 2 | BRF_PRG | BRF_ESS }, //  2 z80 code

	{ "33.040.u99",		0x80000, 0x9b56a394, 3 | BRF_GRA },           //  3 layer 0
	{ "22.040.u97",		0x80000, 0x1e693f05, 3 | BRF_GRA },           //  4
	{ "32.040.u100",	0x80000, 0x7749bc80, 3 | BRF_GRA },           //  5
	{ "21.040.u98",		0x80000, 0xe1586a71, 3 | BRF_GRA },           //  6
	{ "31.040.u102",	0x80000, 0xac5a2952, 3 | BRF_GRA },           //  7
	{ "20.040.u101",	0x80000, 0xe4b2823c, 3 | BRF_GRA },           //  8

#if 1
	{ "30.040.u82",		0x80000, 0x6668d29d, 4 | BRF_GRA },           //  9 Sprites
	{ "19.040.u91",		0x80000, 0x17659d0c, 4 | BRF_GRA },           // 10
	{ "29.040.u85",		0x80000, 0xc349e556, 4 | BRF_GRA },           // 11
	{ "18.040.u84",		0x80000, 0x8716f8d3, 4 | BRF_GRA },           // 12
	{ "28.040.u88",		0x80000, 0xf93f10ce, 4 | BRF_GRA },           // 13
	{ "17.040.u87",		0x80000, 0xfa1ef844, 4 | BRF_GRA },           // 14
	{ "27.040.u91",		0x80000, 0x962f3455, 4 | BRF_GRA },           // 15
	{ "16.040.u90",		0x80000, 0xe1a37b42, 4 | BRF_GRA },           // 16
	{ "26.040.u93",		0x80000, 0x6e65099c, 4 | BRF_GRA },           // 17
	{ "15.040.u81",		0x80000, 0x035316d3, 4 | BRF_GRA },           // 18
	{ "25.040.u94",		0x80000, 0xa250dea8, 4 | BRF_GRA },           // 19
	{ "14.040.u96",		0x80000, 0xdd976689, 4 | BRF_GRA },           // 20
	{ "24.040.u95",		0x80000, 0xdd976689, 4 | BRF_GRA },           // 21
	{ "13.040.u89",		0x80000, 0x867262d6, 4 | BRF_GRA },           // 22
	{ "23.040.u96",		0x80000, 0x625c5b7b, 4 | BRF_GRA },           // 23
	{ "12.040.u92",		0x80000, 0x08c4e478, 4 | BRF_GRA },           // 24
#else
	{ "93095-12.u116",	0x100000, 0x35f3c2a3, BRF_GRA },			  // 25 sprites
	{ "93095-13.u117",	0x100000, 0x1ebd45da, BRF_GRA },			  // 26
	{ "93095-14.u118",	0x100000, 0x760d871b, BRF_GRA },			  // 27
	{ "93095-15.u119",	0x100000, 0xd011be88, BRF_GRA },			  // 28
	{ "93095-16.u120",	0x100000, 0xa9c16c9c, BRF_GRA },			  // 29
	{ "93095-17.u121",	0x100000, 0x51b57288, BRF_GRA },			  // 30
	{ "93095-18.u122",	0x100000, 0xb135e3f2, BRF_GRA },			  // 31
	{ "93095-19.u123",	0x100000, 0x67695537, BRF_GRA },			  // 33
#endif

	{ "9.040.u32",		0x80000, 0x8cd6824e, 5 | BRF_SND },           // 25 sound 1
	{ "8.040.u30",		0x80000, 0xe31ae04d, 5 | BRF_SND },           // 26
	{ "7.040.u33",		0x80000, 0xc4c9f599, 5 | BRF_SND },           // 27
	{ "6.040.u31",		0x80000, 0xf0a9f0e1, 5 | BRF_SND },           // 28

	{ "5.040.u36",		0x80000, 0x62557502, 6 | BRF_SND },           // 29 sound 2
	{ "4.040.u34",		0x80000, 0xdbc86bd7, 6 | BRF_SND },           // 30
	{ "3.040.u37",		0x80000, 0x5839a2bd, 6 | BRF_SND },           // 31
	{ "2.040.u35",		0x80000, 0x446f9dc3, 6 | BRF_SND },           // 32
};

STD_ROM_PICK(powerinsc)
STD_ROM_FN(powerinsc)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom68K 		= Next; Next += 0x0100000;			// 68000 ROM
	RomZ80		= Next; Next += 0x0020000;			// Z80 ROM
	RomBg		= Next; Next += 0x0500000;
	RomFg		= Next; Next += 0x0100000;
	RomSpr		= Next; Next += 0x1000000;
	MSM6295ROM	= Next; Next += m6295size;

	RamStart	= Next;

	RamZ80		= Next; Next += 0x002000;

	RamPal		= (UINT16 *) Next; Next += 0x001000;
	RamBg		= (UINT16 *) Next; Next += 0x004000;
	RamFg		= (UINT16 *) Next; Next += 0x001000;
	RamSpr		= (UINT16 *) Next; Next += 0x010000;
	RamVReg		= (UINT16 *) Next; Next += 0x000008;

	RamEnd		= Next;

	RamCurPal	= (UINT32 *) Next; Next += 0x000800 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static UINT8 __fastcall powerinsReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		case 0x10003f:
			return MSM6295Read(0);
	}
	return 0;
}

static UINT16 __fastcall powerinsReadWord(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0x100000:
			return ~DrvInput[0];

		case 0x100002:
			return ~(DrvInput[2] + (DrvInput[3]<<8));

		case 0x100008:
			return ~DrvInput[4];

		case 0x10000A:
			return ~DrvInput[6];
	}
	return 0;
}

static void __fastcall powerinsWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	if (game_drv != GAME_POWERINA) return; // for powerina

	switch (sekAddress)
	{
		case 0x100031: // powerina oki msm6295 bank
			if (oki_bank != (byteValue & 7)) {
				oki_bank = byteValue & 7;
				MSM6295SetBank(0, MSM6295ROM + 0x30000 + 0x10000*oki_bank, 0x30000, 0x3ffff);
			}
			break;

		case 0x10003e:
			// powerina only!
			break;

		case 0x10003f:
			// powerina only!
			MSM6295Write(0, byteValue);
			break;
	}
}

static void __fastcall powerinsWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress)
	{
		case 0x100014:
			// flipscreen
			break;

		case 0x100018:
			tile_bank = wordValue * 0x800;
			break;

		case 0x10001e:
			soundlatch = wordValue & 0xff;
			break;

		case 0x10003e:
			if (game_drv == GAME_POWERINA) {
				MSM6295Write(0, wordValue & 0xff);
			}
			break;

		case 0x130000:	RamVReg[0] = wordValue; break;
		case 0x130002:	RamVReg[1] = wordValue; break;
		case 0x130004:	RamVReg[2] = wordValue; break;
		case 0x130006:	RamVReg[3] = wordValue; break;

		case 0x100016:	// NOP
			break;
	}
}

static void __fastcall powerinsWriteWordPalette(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress -= 0x120000;
	sekAddress >>= 1;
	RamPal[sekAddress] = wordValue;
	CalcCol( sekAddress );
}

static UINT8 __fastcall powerinsZ80Read(UINT16 a)
{
	switch (a)
	{
		case 0xE000:
			return soundlatch;
	}

	return 0;
}

static UINT8 __fastcall powerinsZ80In(UINT16 p)
{
	switch (p & 0xFF)
	{
		case 0x00:
			if (game_drv == GAME_POWERINS)
				return BurnYM2203Read(0, 0);
			else
				return 0x01;

		case 0x01:
			if (game_drv == GAME_POWERINS)
				return BurnYM2203Read(0, 1);
			else
				return 0;

		case 0x80:
			return MSM6295Read(0);

		case 0x88:
			return MSM6295Read(1);
	}

	return 0;
}

static void __fastcall powerinsZ80Out(UINT16 p, UINT8 v)
{
	switch (p & 0x0FF) {
		case 0x00:
			if (game_drv == GAME_POWERINS) {
				BurnYM2203Write(0, 0, v);
			}
			break;

		case 0x01:
			if (game_drv == GAME_POWERINS) {
				BurnYM2203Write(0, 1, v);
			}
			break;

		case 0x80:
			MSM6295Write(0, v);
			break;

		case 0x88:
			MSM6295Write(1, v);
			break;

		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			NMK112_okibank_write(p & 7, v);
			break;
	}
}

static void powerinsIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);
	
	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset();

	if (game_drv != GAME_POWERINA) {
		ZetOpen(0);
		ZetReset();
		ZetClose();

		if (game_drv == GAME_POWERINS) BurnYM2203Reset();

		NMK112Reset();
	}

	tile_bank = 0;
	soundlatch = 0;

	return 0;
}

static void LoadDecodeBgRom(UINT8 *tmp, UINT8 * dest, INT32 size)
{
	for (INT32 z=0;z<(size/128);z++) {
		for (INT32 y=0;y<16;y++) {
			dest[ 0] = tmp[ 0] >> 4;
			dest[ 1] = tmp[ 0] & 15;
			dest[ 2] = tmp[ 1] >> 4;
			dest[ 3] = tmp[ 1] & 15;

			dest[ 4] = tmp[ 2] >> 4;
			dest[ 5] = tmp[ 2] & 15;
			dest[ 6] = tmp[ 3] >> 4;
			dest[ 7] = tmp[ 3] & 15;

			dest[ 8] = tmp[64] >> 4;
			dest[ 9] = tmp[64] & 15;
			dest[10] = tmp[65] >> 4;
			dest[11] = tmp[65] & 15;

			dest[12] = tmp[66] >> 4;
			dest[13] = tmp[66] & 15;
			dest[14] = tmp[67] >> 4;
			dest[15] = tmp[67] & 15;

			dest += 16;
			tmp += 4;
		}
		tmp += 64;
	}
}

static void LoadDecodeSprRom(UINT8 *tmp, UINT8 * dest, INT32 size)
{
	for (INT32 z=0;z<(size/128);z++) {
		for (INT32 y=0;y<16;y++) {
			dest[ 0] = tmp[ 1] >> 4;
			dest[ 1] = tmp[ 1] & 15;
			dest[ 2] = tmp[ 0] >> 4;
			dest[ 3] = tmp[ 0] & 15;

			dest[ 4] = tmp[ 3] >> 4;
			dest[ 5] = tmp[ 3] & 15;
			dest[ 6] = tmp[ 2] >> 4;
			dest[ 7] = tmp[ 2] & 15;

			dest[ 8] = tmp[65] >> 4;
			dest[ 9] = tmp[65] & 15;
			dest[10] = tmp[64] >> 4;
			dest[11] = tmp[64] & 15;

			dest[12] = tmp[67] >> 4;
			dest[13] = tmp[67] & 15;
			dest[14] = tmp[66] >> 4;
			dest[15] = tmp[66] & 15;

			dest += 16;
			tmp += 4;
		}
		tmp += 64;
	}
}

static INT32 powerinsInit()
{
	INT32 nRet;
	UINT8 * tmp;

	m6295size = 0x80000 * 4 * 2;

	if ( strcmp(BurnDrvGetTextA(DRV_NAME), "powerins") == 0 || strcmp(BurnDrvGetTextA(DRV_NAME), "powerinsj") == 0) {
		game_drv = GAME_POWERINS;
	} else
	if ( strcmp(BurnDrvGetTextA(DRV_NAME), "powerinsa") == 0 ) {
		game_drv = GAME_POWERINA;
		m6295size = 0x90000;
	} else
	if ( strcmp(BurnDrvGetTextA(DRV_NAME), "powerinsb") == 0 ) {
		game_drv = GAME_POWERINB;
	} else
	if ( strcmp(BurnDrvGetTextA(DRV_NAME), "powerinsc") == 0 ) {
		game_drv = GAME_POWERINC;
	} else
	if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
		// below...
	} else
		return 1;

	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex();

	// load roms

	tmp = (UINT8 *)BurnMalloc(0x200000);
	if (tmp==0) return 1;

	if ( game_drv == GAME_POWERINS ) {
		nRet = BurnLoadRom(Rom68K + 0x000000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x080000, 1, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomZ80 + 0x000000, 2, 1); if (nRet != 0) return 1;

		// load background tile roms
		BurnLoadRom(tmp, 3, 1);
		LoadDecodeBgRom(tmp, RomBg+0x000000, 0x100000);
		BurnLoadRom(tmp, 4, 1);
		LoadDecodeBgRom(tmp, RomBg+0x200000, 0x100000);
		BurnLoadRom(tmp, 5, 1);
		LoadDecodeBgRom(tmp, RomBg+0x400000, 0x080000);

		BurnLoadRom(RomFg + 0x000000,  6, 1);

		// load sprite roms
		for (INT32 i=0;i<8;i++) {
			BurnLoadRom(tmp, i+7, 1);
			LoadDecodeSprRom(tmp, RomSpr+0x200000*i,  0x100000);
		}
		
		BurnLoadRom(MSM6295ROM + 0x000000, 15, 1);
		BurnLoadRom(MSM6295ROM + 0x100000, 16, 1);
		BurnLoadRom(MSM6295ROM + 0x200000, 17, 1);
		BurnLoadRom(MSM6295ROM + 0x300000, 18, 1);

	} else
	if ( game_drv == GAME_POWERINA ) {
		nRet = BurnLoadRom(Rom68K + 0x000000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x080000, 1, 1); if (nRet != 0) return 1;

		// load background tile roms
		BurnLoadRom(tmp, 2, 1);
		LoadDecodeBgRom(tmp, RomBg+0x000000, 0x200000);
		BurnLoadRom(tmp, 3, 1);
		LoadDecodeBgRom(tmp, RomBg+0x400000, 0x080000);

		// load foreground tile roms
		BurnLoadRom(RomFg + 0x000000,  4, 1);

		// load sprite roms
		for (INT32 i=0;i<4;i++) {
			BurnLoadRom(tmp, i+5, 1);
			LoadDecodeSprRom(tmp, RomSpr+0x400000*i, 0x200000);
		}
		
		BurnLoadRom(MSM6295ROM + 0x00000, 9, 1);

	} else
	if ( game_drv == GAME_POWERINB ) {

		nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomZ80 + 0x000000, 2, 1); if (nRet != 0) return 1;

		// load background tile roms
		for (INT32 i=0;i<5;i++) {
			BurnLoadRom(tmp, 3+i, 1);
			LoadDecodeBgRom(tmp, RomBg+0x100000*i, 0x80000);
		}
		
		// load foreground tile roms
		BurnLoadRom(RomFg + 0x000000,  8, 1);

		// load sprite roms
		for (INT32 i=0;i<8;i++) {
			BurnLoadRom(tmp + 0,  9 + i*2, 2);
			BurnLoadRom(tmp + 1, 10 + i*2, 2);
		
			LoadDecodeSprRom(tmp, RomSpr+0x200000*i, 0x100000);
		}
		
		BurnLoadRom(MSM6295ROM + 0x000000, 25, 1);
		BurnLoadRom(MSM6295ROM + 0x080000, 26, 1);
		BurnLoadRom(MSM6295ROM + 0x100000, 27, 1);
		BurnLoadRom(MSM6295ROM + 0x180000, 28, 1);
		
		BurnLoadRom(MSM6295ROM + 0x200000, 29, 1);
		BurnLoadRom(MSM6295ROM + 0x280000, 30, 1);
		BurnLoadRom(MSM6295ROM + 0x300000, 31, 1);
		BurnLoadRom(MSM6295ROM + 0x380000, 32, 1);
	} else
	if ( game_drv == GAME_POWERINC ) {

		nRet = BurnLoadRom(Rom68K + 0x000001, 0, 2); if (nRet != 0) return 1;
		nRet = BurnLoadRom(Rom68K + 0x000000, 1, 2); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomZ80 + 0x000000, 2, 1); if (nRet != 0) return 1;

		// load background tile roms
		BurnLoadRom(tmp + 0, 3, 2);
		BurnLoadRom(tmp + 1, 4, 2);
		LoadDecodeBgRom(tmp, RomBg+0x000000, 0x100000);
		BurnLoadRom(tmp + 0, 5, 2);
		BurnLoadRom(tmp + 1, 6, 2);
		LoadDecodeBgRom(tmp, RomBg+0x200000, 0x100000);
		BurnLoadRom(tmp + 0, 7, 2);
		BurnLoadRom(tmp + 1, 8, 2);
	
		for (INT32 i = 0; i < 0x20000; i++) { // copy fg rom from bg roms and swap nibbles
			RomFg[i] = (tmp[0x80000 + i] << 4) | (tmp[0x80000 + i] >>  4);
		}

		LoadDecodeBgRom(tmp, RomBg+0x400000, 0x080000);
		BurnByteswap(RomBg, 0x500000);

		// load sprite roms
		for (INT32 i=0;i<8;i++) {
			BurnLoadRom(tmp + 0,  9 + i*2, 2);
			BurnLoadRom(tmp + 1, 10 + i*2, 2);
		
			LoadDecodeSprRom(tmp, RomSpr+0x200000*i, 0x100000);
		}
		
		BurnLoadRom(MSM6295ROM + 0x000000, 25, 1);
		BurnLoadRom(MSM6295ROM + 0x080000, 26, 1);
		BurnLoadRom(MSM6295ROM + 0x100000, 27, 1);
		BurnLoadRom(MSM6295ROM + 0x180000, 28, 1);
		
		BurnLoadRom(MSM6295ROM + 0x200000, 29, 1);
		BurnLoadRom(MSM6295ROM + 0x280000, 30, 1);
		BurnLoadRom(MSM6295ROM + 0x300000, 31, 1);
		BurnLoadRom(MSM6295ROM + 0x380000, 32, 1);

#if 0
		// hack in old sprite configuration
		Rom68K[0x0370b] = 0x4d; // 0x4e
		Rom68K[0x0370c] = 0x18; // 0x0d
		Rom68K[0x0370e] = 0x00; // 0x80
		Rom68K[0x0370f] = 0x80; // 0x01
		Rom68K[0x03aa0] = 0x10; // 0x08
		Rom68K[0x03ac6] = 0x10; // 0x08
		Rom68K[0x03b88] = 0x06; // 0x04
		Rom68K[0x03b8c] = 0x0e; // 0x06
		Rom68K[0x03b94] = 0x10; // 0x08
		Rom68K[0x03bc4] = 0x06; // 0x04
		Rom68K[0x03bc8] = 0x0e; // 0x06
		Rom68K[0x03bd0] = 0x10; // 0x08
		Rom68K[0x03bf1] = 0x49; // 0x4e
		Rom68K[0x03bf2] = 0x0a; // 0x0d
		Rom68K[0x03bf5] = 0x80; // 0x00
		Rom68K[0x03c1a] = 0x2a; // 0xf9
		Rom68K[0x03c1b] = 0x3c; // 0x4e
		Rom68K[0x03c1c] = 0x2e; // 0x0d
		Rom68K[0x03c1e] = 0x46; // 0x28
		Rom68K[0x03c1f] = 0x53; // 0x00
		Rom68K[0x03c7a] = 0x01; // 0x08
		Rom68K[0x03c96] = 0x42; // 0x40
		Rom68K[0x03c98] = 0x82; // 0x01
		Rom68K[0x03c9a] = 0x14; // 0x6c
		Rom68K[0x03c9b] = 0x16; // 0xd6
		Rom68K[0x03c9c] = 0x43; // 0x06
		Rom68K[0x03c9d] = 0x02; // 0x00
		Rom68K[0x03c9e] = 0x0f; // 0x43
		Rom68K[0x03c9f] = 0x00; // 0x02
		Rom68K[0x03ca0] = 0x4b; // 0xff
		Rom68K[0x03ca1] = 0xe1; // 0x01
		Rom68K[0x03ca2] = 0x14; // 0x83
		Rom68K[0x03ca3] = 0x16; // 0x36
		Rom68K[0x03ca4] = 0x03; // 0x14
		Rom68K[0x03ca5] = 0x02; // 0x16
		Rom68K[0x03ca6] = 0xf0; // 0x03
		Rom68K[0x03ca7] = 0x00; // 0x02
		Rom68K[0x03ca8] = 0x43; // 0xf0
		Rom68K[0x03ca9] = 0x37; // 0x00
		Rom68K[0x03cab] = 0x00; // 0x86
		Rom68K[0x03cac] = 0x00; // 0x13
		Rom68K[0x03cad] = 0x36; // 0x87
		Rom68K[0x03cae] = 0x6c; // 0x00
		Rom68K[0x03caf] = 0xd6; // 0x36
		Rom68K[0x03cb0] = 0x04; // 0x6c
		Rom68K[0x03cb1] = 0x00; // 0xd6
		Rom68K[0x03cb2] = 0x43; // 0x04
		Rom68K[0x03cb3] = 0x37; // 0x00
		Rom68K[0x03cb4] = 0x08; // 0x43
		Rom68K[0x03cb5] = 0x00; // 0x37
		Rom68K[0x03cb6] = 0x01; // 0x02
		Rom68K[0x03cb7] = 0x36; // 0x00
		Rom68K[0x03cb8] = 0x6c; // 0x14
		Rom68K[0x03cb9] = 0xd6; // 0x16
		Rom68K[0x03cba] = 0x06; // 0x03
		Rom68K[0x03cbb] = 0x00; // 0x02
		Rom68K[0x03cbc] = 0x43; // 0x0f
		Rom68K[0x03cbd] = 0x37; // 0x00
		Rom68K[0x03cbe] = 0x0c; // 0x43
		Rom68K[0x03cbf] = 0x00; // 0x17
		Rom68K[0x03cc0] = 0x47; // 0x06
		Rom68K[0x03cc1] = 0x52; // 0x00
		Rom68K[0x03cc2] = 0xfc; // 0x47
		Rom68K[0x03cc3] = 0xd6; // 0x52
		Rom68K[0x03cc4] = 0x10; // 0xfc
		Rom68K[0x03cc5] = 0x00; // 0xd6
		Rom68K[0x03cc6] = 0x4c; // 0x08
		Rom68K[0x03cc7] = 0x50; // 0x00
		Rom68K[0x03cc8] = 0x47; // 0x4c
		Rom68K[0x03cc9] = 0x0c; // 0x50
		Rom68K[0x03cca] = 0x00; // 0x47
		Rom68K[0x03ccb] = 0x01; // 0x0c
		Rom68K[0x03ccd] = 0x65; // 0x01
		Rom68K[0x03ccf] = 0x00; // 0x6c
		Rom68K[0x03cd0] = 0x75; // 0xce
		Rom68K[0x03cd1] = 0x4e; // 0x51
		Rom68K[0x03cd2] = 0xce; // 0xc6
		Rom68K[0x03cd3] = 0x51; // 0xff
		Rom68K[0x03cd4] = 0xc4; // 0x75
		Rom68K[0x03cd5] = 0xff; // 0x4e
		Rom68K[0x03cd6] = 0x75; // 0x01
		Rom68K[0x03cd7] = 0x4e; // 0x36
		Rom68K[0x03cd8] = 0x82; // 0x6c
		Rom68K[0x03cd9] = 0x36; // 0xd6
		Rom68K[0x03cda] = 0x2c; // 0xfe
		Rom68K[0x03cdb] = 0x16; // 0xff
		Rom68K[0x03cdc] = 0xf8; // 0x43
		Rom68K[0x03cdd] = 0xff; // 0x02
		Rom68K[0x03cde] = 0x43; // 0xff
		Rom68K[0x03cdf] = 0x02; // 0x01
		Rom68K[0x03ce0] = 0x0f; // 0x83
		Rom68K[0x03ce1] = 0x00; // 0x36
		Rom68K[0x03ce2] = 0x4b; // 0x2c
		Rom68K[0x03ce3] = 0xe1; // 0x16
		Rom68K[0x03ce4] = 0x2c; // 0xf8
		Rom68K[0x03ce5] = 0x16; // 0xff
		Rom68K[0x03ce6] = 0xf8; // 0x03
		Rom68K[0x03ce7] = 0xff; // 0x02
		Rom68K[0x03ce8] = 0x03; // 0xf0
		Rom68K[0x03ce9] = 0x02; // 0x00
		Rom68K[0x03cea] = 0xf0; // 0x02
		Rom68K[0x03ceb] = 0x00; // 0x86
		Rom68K[0x03cec] = 0x43; // 0x13
		Rom68K[0x03ced] = 0x00; // 0x87
		Rom68K[0x03cef] = 0x10; // 0x36
		Rom68K[0x03cf0] = 0x43; // 0x6c
		Rom68K[0x03cf1] = 0x37; // 0x96
		Rom68K[0x03cf2] = 0x02; // 0xfc
		Rom68K[0x03cf3] = 0x00; // 0xff
		Rom68K[0x03cf4] = 0x00; // 0x43
		Rom68K[0x03cf5] = 0x36; // 0x04
		Rom68K[0x03cf6] = 0x6c; // 0x10
		Rom68K[0x03cf7] = 0x96; // 0x00
		Rom68K[0x03cf8] = 0xfc; // 0x43
		Rom68K[0x03cf9] = 0xff; // 0x37
		Rom68K[0x03cfa] = 0x43; // 0x02
		Rom68K[0x03cfb] = 0x04; // 0x00
		Rom68K[0x03cfc] = 0x10; // 0x2c
		Rom68K[0x03cfd] = 0x00; // 0x16
		Rom68K[0x03cfe] = 0x43; // 0xf8
		Rom68K[0x03cff] = 0x37; // 0xff
		Rom68K[0x03d00] = 0x08; // 0x03
		Rom68K[0x03d01] = 0x00; // 0x02
		Rom68K[0x03d02] = 0x01; // 0x0f
		Rom68K[0x03d03] = 0x36; // 0x00
		Rom68K[0x03d04] = 0x6c; // 0x03
		Rom68K[0x03d05] = 0xd6; // 0x00
		Rom68K[0x03d06] = 0xfe; // 0x80
		Rom68K[0x03d07] = 0xff; // 0x00
		Rom68K[0x03d09] = 0x37; // 0x17
		Rom68K[0x03d0a] = 0x0c; // 0x06
		Rom68K[0x03d10] = 0x10; // 0x08
		Rom68K[0x03d18] = 0x00; // 0x04
		Rom68K[0x03d19] = 0x65; // 0x6c
		Rom68K[0x03d1a] = 0x04; // 0xce
		Rom68K[0x03d1b] = 0x00; // 0x51
		Rom68K[0x03d1c] = 0x75; // 0xba
		Rom68K[0x03d1d] = 0x4e; // 0xff
		Rom68K[0x03d1e] = 0xce; // 0x75
		Rom68K[0x03d1f] = 0x51; // 0x4e
#endif

		game_drv = GAME_POWERINB; // correct machine
	}
	else if (BurnDrvGetFlags() & BDF_PROTOTYPE)
	{
		nRet = BurnLoadRom(Rom68K + 0x000000, 0, 1); if (nRet != 0) return 1; // POWERINSB is interleaved!!
		nRet = BurnLoadRom(Rom68K + 0x080000, 1, 1); if (nRet != 0) return 1;

		nRet = BurnLoadRom(RomZ80 + 0x000000, 2, 1); if (nRet != 0) return 1;

		// load background tile roms
		for (INT32 i=0;i<5;i++) {
			BurnLoadRom(tmp, 3+i, 1);
			LoadDecodeBgRom(tmp, RomBg+0x100000*i, 0x80000);
		}
		
		// load foreground tile roms
		BurnLoadRom(RomFg + 0x000000,  8, 1);

		// load sprite roms
		for (INT32 i=0;i<8;i++) {
			BurnLoadRom(tmp + 0,  9 + i*2, 2);
			BurnLoadRom(tmp + 1, 10 + i*2, 2);
		
			LoadDecodeSprRom(tmp, RomSpr+0x200000*i, 0x100000);
		}
		
		BurnLoadRom(MSM6295ROM + 0x000000, 25, 1);
		BurnLoadRom(MSM6295ROM + 0x080000, 26, 1);
		BurnLoadRom(MSM6295ROM + 0x100000, 27, 1);
		BurnLoadRom(MSM6295ROM + 0x180000, 28, 1);
		
		BurnLoadRom(MSM6295ROM + 0x200000, 29, 1);
		BurnLoadRom(MSM6295ROM + 0x280000, 30, 1);
		BurnLoadRom(MSM6295ROM + 0x300000, 31, 1);
		BurnLoadRom(MSM6295ROM + 0x380000, 32, 1);

		game_drv = GAME_POWERINS; // correct machine
	}
	
	BurnFree(tmp);

	{
		SekInit(0, 0x68000);										// Allocate 68000
	    SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom68K,		0x000000, 0x0FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory((UINT8 *)RamPal,
									0x120000, 0x120FFF, MAP_ROM);	// palette
		SekMapMemory((UINT8 *)RamBg,
									0x140000, 0x143fff, MAP_RAM);	// b ground
		SekMapMemory((UINT8 *)RamFg,
									0x170000, 0x170fff, MAP_RAM);	// f ground
		SekMapMemory((UINT8 *)RamFg,
									0x171000, 0x171fff, MAP_RAM);	// f ground Mirror
		SekMapMemory((UINT8 *)RamSpr,
									0x180000, 0x18ffff, MAP_RAM);	// sprites

		SekMapHandler(1,			0x120000, 0x120FFF, MAP_WRITE);


		SekSetReadWordHandler(0, powerinsReadWord);
		SekSetReadByteHandler(0, powerinsReadByte);
		SekSetWriteWordHandler(0, powerinsWriteWord);
		SekSetWriteByteHandler(0, powerinsWriteByte);

		SekSetWriteWordHandler(1, powerinsWriteWordPalette);

		SekClose();
	}

	if (game_drv != GAME_POWERINA) {

		ZetInit(0);
		ZetOpen(0);

		ZetSetReadHandler(powerinsZ80Read);
		ZetSetInHandler(powerinsZ80In);
		ZetSetOutHandler(powerinsZ80Out);

		// ROM bank 1
		ZetMapArea(0x0000, 0xBFFF, 0, RomZ80);
		ZetMapArea(0x0000, 0xBFFF, 2, RomZ80);
		// RAM
		ZetMapArea(0xC000, 0xDFFF, 0, RamZ80);
		ZetMapArea(0xC000, 0xDFFF, 1, RamZ80);
		ZetMapArea(0xC000, 0xDFFF, 2, RamZ80);

		ZetClose();
	}

	if (game_drv == GAME_POWERINA) {
		MSM6295Init(0, 990000 / 165, 0);
		MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	}

	if (game_drv == GAME_POWERINS) {
		BurnSetRefreshRate(56.0);
		BurnYM2203Init(1, 12000000 / 8, &powerinsIRQHandler, 0);
		BurnTimerAttachZet(6000000);
		BurnYM2203SetAllRoutes(0, 2.00, BURN_SND_ROUTE_BOTH);
		
		MSM6295Init(0, 4000000 / 165, 1);
		MSM6295Init(1, 4000000 / 165, 1);
		MSM6295SetRoute(0, 0.15, BURN_SND_ROUTE_BOTH);
		MSM6295SetRoute(1, 0.15, BURN_SND_ROUTE_BOTH);

		NMK112_init(0, MSM6295ROM, MSM6295ROM + 0x200000, 0x200000, 0x200000);
	}

	if (game_drv == GAME_POWERINB) {
		MSM6295Init(0, 4000000 / 165, 1);
		MSM6295Init(1, 4000000 / 165, 1);
		MSM6295SetRoute(0, 0.15, BURN_SND_ROUTE_BOTH);
		MSM6295SetRoute(1, 0.15, BURN_SND_ROUTE_BOTH);

		NMK112_init(0, MSM6295ROM, MSM6295ROM + 0x200000, 0x200000, 0x200000);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 powerinsExit()
{
	GenericTilesExit();

	SekExit();
	MSM6295Exit();

	if (game_drv != GAME_POWERINA) {
		if (game_drv == GAME_POWERINS)
			BurnYM2203Exit();

		ZetExit();
	}

	BurnFree(Mem);
	return 0;
}

static void TileBackground()
{
	for (INT32 offs = 256*32-1; offs >=0; offs--) {

		INT32 page = offs >> 8;
		INT32 x = (offs >> 4) & 0xF;
		INT32 y = (offs >> 0) & 0xF;

		x = x * 16 + (page &  7) * 256 + 32 - ((RamVReg[1]&0xff) + (RamVReg[0]&0xff)*256);
		y = y * 16 + (page >> 4) * 256 - 16 - ((RamVReg[3]&0xff) + (RamVReg[2]&0xff)*256);

		if ( x<=-16 || x>=320 || y <=-16 || y>=224 ) continue;
		else {
			INT32 attr = RamBg[offs];
			INT32 code = ((attr & 0x7ff) + tile_bank);
			INT32 color = (attr >> 12) | ((attr >> 7) & 0x10);

			if ( x >=0 && x <= (320-16) && y>=0 && y <=(224-16) ) {
				Render16x16Tile(pTransDraw, code, x, y, color, 4, 0, RomBg);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, x, y, color, 4, 0, RomBg);
			}
		}
	}
}

static void TileForeground()
{
	for (INT32 offs = 0; offs < 64*32; offs++) {
		INT32 x = ((offs & 0xFFE0) >> 2 ) + 32;
		INT32 y = ((offs & 0x001F) << 3 ) - 16;
		if (x > 320) x -= 512;
		if ( x<0 || x>(320-8) || y<0 || y>(224-8)) continue;
		else {
			if ((RamFg[offs] & 0x0FFF) == 0) continue;
			UINT8 *d = RomFg + (RamFg[offs] & 0x0FFF) * 32;
 			UINT16 c = ((RamFg[offs] & 0xF000) >> 8) | 0x200;
 			UINT16 *p = pTransDraw + y * 320 + x;
			for (INT32 k=0;k<8;k++) {
 				if ((d[0] >>  4) != 15) p[0] = (d[0] >>  4) | c;
 				if ((d[0] & 0xF) != 15) p[1] = (d[0] & 0xF) | c;
 				if ((d[1] >>  4) != 15) p[2] = (d[1] >>  4) | c;
 				if ((d[1] & 0xF) != 15) p[3] = (d[1] & 0xF) | c;
 				if ((d[2] >>  4) != 15) p[4] = (d[2] >>  4) | c;
 				if ((d[2] & 0xF) != 15) p[5] = (d[2] & 0xF) | c;
 				if ((d[3] >>  4) != 15) p[6] = (d[3] >>  4) | c;
 				if ((d[3] & 0xF) != 15) p[7] = (d[3] & 0xF) | c;
 				d += 4;
 				p += 320;
 			}
		}
	}
}

static inline void drawgfx(UINT32 code,UINT32 color,INT32 flipx,/*INT32 flipy,*/INT32 sx,INT32 sy)
{
	if (sx >= 0 && sx <= (320-16) && sy >= 0 && sy <= (224-16) ) {
		if ( flipx )
			Render16x16Tile_Mask_FlipX(pTransDraw, code, sx, sy, color, 4, 15, 0x400, RomSpr);
		else
			Render16x16Tile_Mask(pTransDraw, code, sx, sy, color, 4, 15, 0x400, RomSpr);
	} else {
		if ( flipx )
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x400, RomSpr);
		else
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x400, RomSpr);
	}
}

static void DrawSprites()
{
	UINT16 *source = RamSpr + 0x8000/2;
	UINT16 *finish = RamSpr + 0x9000/2;

	for ( ; source < finish; source += 8 ) {

		if (!(source[0]&1)) continue;

		INT32	size	=	source[1];
		INT32	code	=	(source[3] & 0x7fff) + ( (size & 0x0100) << 7 );
		INT32	sx		=	source[4];
		INT32	sy		=	source[6];
		INT32	color	=	source[7] & 0x3F;
		INT32	flipx	=	size & 0x1000;
		INT32	dimx	=	((size >> 0) & 0xf ) + 1;
		INT32	dimy	=	((size >> 4) & 0xf ) + 1;

		sx &= 0x3ff; if (sx > 0x1ff) sx -= 0x400; sx += 32;
		sy &= 0x3ff; if (sy > 0x1ff) sy -= 0x400; sy -= 16;

		for (INT32 x = 0 ; x < dimx ; x++)
			for (INT32 y = 0 ; y < dimy ; y++) {
				drawgfx(code, color, flipx, /*flipy, */sx + x*16, sy + y*16);
				code ++;
			}
	}
}

static INT32 DrvDraw()
{
	if (bRecalcPalette) {
		for (INT32 i = 0; i < 0x800; i++)
			CalcCol(i);
		bRecalcPalette = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) TileBackground();
	if (nSpriteEnable & 1) DrawSprites();
	if (nBurnLayer & 2) TileForeground();

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 powerinsFrame()
{
	INT32 nInterleave = 200;

	if (DrvReset) DrvDoReset();

	DrvInput[0] = 0x00;
	DrvInput[2] = 0x00;
	DrvInput[3] = 0x00;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[2] |= (DrvJoy1[i] & 1) << i;
		DrvInput[3] |= (DrvJoy2[i] & 1) << i;
		DrvInput[0] |= (DrvButton[i] & 1) << i;
	}

	nCyclesTotal[0] = (INT32)((INT64)12000000 * nBurnCPUSpeedAdjust / (0x0100 * 56));
	if (game_drv == GAME_POWERINB) nCyclesTotal[0] = (INT32)((INT64)12000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));
	nCyclesTotal[1] = 6000000 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	SekOpen(0);
	if (game_drv != GAME_POWERINA) {
		ZetNewFrame();
		ZetOpen(0);
	}

	if (game_drv == GAME_POWERINA) {
		nCyclesTotal[0] = (INT32)((INT64)12000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));
		SekRun(nCyclesTotal[0]);
		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		if (pBurnSoundOut) MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (game_drv == GAME_POWERINS) {
		for (INT32 i = 0; i < nInterleave; i++) {
			INT32 nCurrentCPU, nNext;

			nCurrentCPU = 0;
			nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
			nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
			nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);

			BurnTimerUpdate(i * (nCyclesTotal[1] / nInterleave));
		}

		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	}

	if (game_drv == GAME_POWERINB) {
		for (INT32 i = 0; i < nInterleave; i++) {
			INT32 nCurrentCPU, nNext;

			nCurrentCPU = 0;
			nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
			nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
			nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);

			nCurrentCPU = 1;
			nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
			nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
			nCyclesDone[nCurrentCPU] += ZetRun(nCyclesSegment);
			if ((i & 180) == 0) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			}
		}

		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	}

	SekClose();

	if (game_drv == GAME_POWERINS) {
		BurnTimerEndFrame(nCyclesTotal[1]);

		if (pBurnSoundOut) {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
			MSM6295Render(pBurnSoundOut, nBurnSoundLen);
		}

		ZetClose();
	}

	if (game_drv == GAME_POWERINB) {
		ZetRun(nCyclesTotal[1] - nCyclesDone[1]);

		if (pBurnSoundOut) {
			BurnSoundClear();
			MSM6295Render(pBurnSoundOut, nBurnSoundLen);
		}

		ZetClose();
	}

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 powerinsScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {						// Return minimum compatible version
		*pnMin =  0x029671;
	}

	if (nAction & ACB_MEMORY_RAM) {	    // Scan all memory, devices & variables
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		SekScan(nAction);			    // Scan 68000 state

		if (game_drv != GAME_POWERINA) ZetScan(nAction);										// Scan Z80 state

		if (game_drv == GAME_POWERINS) BurnYM2203Scan(nAction, pnMin);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		if (game_drv == GAME_POWERINA) SCAN_VAR(oki_bank);

		SCAN_VAR(tile_bank);

		if (nAction & ACB_WRITE) {
			if (game_drv == GAME_POWERINA)  MSM6295SetBank(0, MSM6295ROM + 0x30000 + 0x10000*oki_bank, 0x30000, 0x3ffff);
		}
	}

	return 0;
}

struct BurnDriver BurnDrvPowerins = {
	"powerins", NULL, NULL, NULL, "1993",
	"Power Instinct (USA)\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinsRomInfo, powerinsRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinsDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerinj = {
	"powerinsj", "powerins", NULL, NULL, "1993",
	"Gouketsuji Ichizoku (Japan)\0", NULL, "Atlus", "Miscellaneous",
	L"\u8C6A\u8840\u5BFA\u4E00\u65CF (Japan)\0Gouketsuji Ichizoku\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinjRomInfo, powerinjRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinjDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerinspu = {
	"powerinspu", "powerins", NULL, NULL, "1993",
	"Power Instinct (USA, prototype)\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinspuRomInfo, powerinspuRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinjDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerinspj = {
	"powerinspj", "powerins", NULL, NULL, "1993",
	"Gouketsuji Ichizoku (Japan, prototype)\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinspjRomInfo, powerinspjRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinjDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerina = {
	"powerinsa", "powerins", NULL, NULL, "1993",
	"Power Instinct (USA, bootleg set 1)\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinaRomInfo, powerinaRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinsDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerinb = {
	"powerinsb", "powerins", NULL, NULL, "1993",
	"Power Instinct (USA, bootleg set 2)\0", NULL, "Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinbRomInfo, powerinbRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinsDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPowerinsc = {
	"powerinsc", "powerins", NULL, NULL, "1993",
	"Power Instinct (USA, bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, FBF_PWRINST,
	NULL, powerinscRomInfo, powerinscRomName, NULL, NULL, NULL, NULL, powerinsInputInfo, powerinsDIPInfo,
	powerinsInit, powerinsExit, powerinsFrame, DrvDraw, powerinsScan, &bRecalcPalette, 0x800,
	320, 224, 4, 3
};
