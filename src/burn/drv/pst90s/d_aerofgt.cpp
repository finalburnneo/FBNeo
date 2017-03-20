/*
 * Aero Fighters driver for FB Alpha 0.2.96.71
 * based on MAME driver by Nicola Salmoria
 * Port by OopsWare. 2007
 * http://oopsware.googlepages.com
 * http://oopsware.ys168.com
 *
 *
 * 12.08.2014, 8.13.2016 - revisited.
 *   Add sprite priority bitmap for Turbo Force - see notes in turbofrcDraw();
 *
 * 6.04.2014
 *   Overhaul graphics routines, now supports multiple color depths and sprite zooming
 *   Clean and merge routines
 *
 * 6.13.2007
 *   Add driver Spinal Breakers (spinlbrk)
 *
 * 6.12.2007
 *   Add driver Karate Blazers (karatblz)
 *
 * 6.11.2007
 *   Add driver Turbo Force (turbofrc)
 *
 * 6.10.2007
 *   Add BurnHighCol support, and add BDF_16BIT_ONLY into driver.   thanks to KEV
 *
 *
 *  Priorities and row scroll are not implemented
 *
 */

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2610.h"

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy3[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy4[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static UINT8 DrvInput[10] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvReset = 0;

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Rom01;
static UINT8 *RomZ80;
static UINT8 *RomBg;
static UINT8 *RomSpr1;
static UINT8 *RomSpr2;
static UINT8 *RomSnd1;
static UINT8 *RomSnd2;

static UINT8 *RamPal;
static UINT16 *RamRaster;
static UINT16 *RamBg1V, *RamBg2V;
static UINT16 *RamSpr1;
static UINT16 *RamSpr2;
static UINT16 *RamSpr3;
static UINT8 *Ram01;
static UINT8 *RamZ80;

static INT32 nCyclesTotal[2];

static UINT8 RamGfxBank[8];

static UINT32 *RamCurPal;
static UINT8 DrvRecalc;

static UINT8 *DeRomBg;
static UINT8 *DeRomSpr1;
static UINT8 *DeRomSpr2;
static UINT8 *RamPrioBitmap;

static UINT32 RamSpr1SizeMask;
static UINT32 RamSpr2SizeMask;
static UINT32 RomSpr1SizeMask;
static UINT32 RomSpr2SizeMask;

static void (*pAssembleInputs)();

static INT32 pending_command = 0;
static INT32 RomSndSize1, RomSndSize2;

static UINT16 bg1scrollx, bg2scrollx;
static UINT16 bg1scrolly, bg2scrolly;

static INT32 nAerofgtZ80Bank;
static UINT8 nSoundlatch;

static UINT8 spritepalettebank;
static UINT8 charpalettebank;

static struct BurnInputInfo aerofgtInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvButton + 2,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvButton + 3,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	
	{"P3 Coin",		BIT_DIGITAL,	DrvButton + 6,	"p3 coin"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"},
};

STDINPUTINFO(aerofgt)

static struct BurnDIPInfo aerofgtDIPList[] = {
	// Defaults
	{0x12,	0xFF, 0xFF,	0x00, NULL},
	{0x13,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin Slot"},
	{0x12,	0x01, 0x01,	0x00, "Same"},
	{0x12,	0x01, 0x01,	0x01, "Individual"},
	{0,		0xFE, 0,	8,	  "Coin 1"},
	{0x12,	0x01, 0x0E, 0x00, "1 coin 1 credits"},
	{0x12,	0x01, 0x0E, 0x02, "2 coin 1 credits"},
	{0x12,	0x01, 0x0E, 0x04, "3 coin 1 credit"},
	{0x12,	0x01, 0x0E, 0x06, "1 coin 2 credits"},
	{0x12,	0x01, 0x0E, 0x08, "1 coins 3 credit"},
	{0x12,	0x01, 0x0E, 0x0A, "1 coins 4 credit"},
	{0x12,	0x01, 0x0E, 0x0C, "1 coins 5 credit"},
	{0x12,	0x01, 0x0E, 0x0E, "1 coins 6 credit"},
	{0,		0xFE, 0,	8,	  "Coin 2"},
	{0x12,	0x01, 0x70, 0x00, "1 coin 1 credits"},
	{0x12,	0x01, 0x70, 0x10, "2 coin 1 credits"},
	{0x12,	0x01, 0x70, 0x20, "3 coin 1 credit"},
	{0x12,	0x01, 0x70, 0x30, "1 coin 2 credits"},
	{0x12,	0x01, 0x70, 0x40, "1 coins 3 credit"},
	{0x12,	0x01, 0x70, 0x50, "1 coins 4 credit"},
	{0x12,	0x01, 0x70, 0x60, "1 coins 5 credit"},
	{0x12,	0x01, 0x70, 0x70, "1 coins 6 credit"},
	{0,		0xFE, 0,	2,	  "2 Coins to Start, 1 to Continue"},
	{0x12,	0x01, 0x80,	0x00, "Off"},
	{0x12,	0x01, 0x80,	0x80, "On"},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen flip"},
	{0x13,	0x01, 0x01, 0x00, "Off"},
	{0x13,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x13,	0x01, 0x02, 0x00, "Off"},
	{0x13,	0x01, 0x02, 0x02, "On"},
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x13,	0x01, 0x0C, 0x00, "Normal"},
	{0x13,	0x01, 0x0C, 0x04, "Easy"},
	{0x13,	0x01, 0x0C, 0x08, "Hard"},
	{0x13,	0x01, 0x0C, 0x0C, "Hardest"},
	{0,		0xFE, 0,	4,	  "Lives"},
	{0x13,	0x01, 0x30, 0x00, "3"},
	{0x13,	0x01, 0x30, 0x10, "1"},
	{0x13,	0x01, 0x30, 0x20, "2"},
	{0x13,	0x01, 0x30, 0x30, "4"},
	{0,		0xFE, 0,	2,	  "Bonus Life"},
	{0x13,	0x01, 0x40, 0x00, "200000"},
	{0x13,	0x01, 0x40, 0x40, "300000"},
	{0,		0xFE, 0,	2,	  "Service"},
	{0x13,	0x01, 0x80, 0x00, "Off"}, 
	{0x13,	0x01, 0x80, 0x80, "On"},
	
};

static struct BurnDIPInfo aerofgt_DIPList[] = {
	{0x14,	0xFF, 0xFF,	0x0F, NULL},

	// DIP 3
	{0,		0xFE, 0,	5,	  "Region"},
	{0x14,	0x01, 0x0F, 0x00, "USA"},
	{0x14,	0x01, 0x0F, 0x01, "Korea"},
	{0x14,	0x01, 0x0F, 0x02, "Hong Kong"},
	{0x14,	0x01, 0x0F, 0x04, "Taiwan"},
	{0x14,	0x01, 0x0F, 0x0F, "Any"},	
};

STDDIPINFOEXT(aerofgt, aerofgt, aerofgt_)

static struct BurnInputInfo turbofrcInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvButton + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvButton + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	
	{"P3 Coin",		BIT_DIGITAL,	DrvButton + 7,	"p3 start"},
	{"P3 Start",	BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"},
	{"P3 Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"},
	{"P3 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"},
	
	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"},
};

STDINPUTINFO(turbofrc)

static struct BurnDIPInfo turbofrcDIPList[] = {

	// Defaults
	{0x16,	0xFF, 0xFF,	0x00, NULL},
	{0x17,	0xFF, 0xFF,	0x06, NULL},

	// DIP 1
	{0,		0xFE, 0,	8,	  "Coin 1"},
	{0x16,	0x01, 0x07, 0x00, "1 coins 1 credit"},
	{0x16,	0x01, 0x07, 0x01, "2 coins 1 credit"},
	{0x16,	0x01, 0x07, 0x02, "3 coins 1 credit"},
	{0x16,	0x01, 0x07, 0x03, "4 coins 1 credit"},
	{0x16,	0x01, 0x07, 0x04, "1 coin 2 credits"},
	{0x16,	0x01, 0x07, 0x05, "1 coin 3 credits"},
	{0x16,	0x01, 0x07, 0x06, "1 coin 5 credits"},
	{0x16,	0x01, 0x07, 0x07, "1 coin 6 credits"},
	{0,		0xFE, 0,	2,	  "2 Coins to Start, 1 to Continue"},
	{0x16,	0x01, 0x08,	0x00, "Off"},
	{0x16,	0x01, 0x08,	0x08, "On"},
	{0,		0xFE, 0,	2,	  "Coin Slot"},
	{0x16,	0x01, 0x10,	0x00, "Same"},
	{0x16,	0x01, 0x10,	0x10, "Individual"},
	{0,		0xFE, 0,	2,	  "Play Mode"},
	{0x16,	0x01, 0x20,	0x00, "2 Players"},
	{0x16,	0x01, 0x20,	0x20, "3 Players"},
	{0,		0xFE, 0,	2,	  "Demo_Sounds"},
	{0x16,	0x01, 0x40,	0x00, "Off"},
	{0x16,	0x01, 0x40,	0x40, "On"},
	{0,		0xFE, 0,	2,	  "Service"},
	{0x16,	0x01, 0x80,	0x00, "Off"},
	{0x16,	0x01, 0x80,	0x80, "On"},
	
	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen flip"},
	{0x17,	0x01, 0x01, 0x00, "Off"},
	{0x17,	0x01, 0x01, 0x01, "On"},
	{0,		0xFE, 0,	8,	  "Difficulty"},
	{0x17,	0x01, 0x0E, 0x00, "1 (Easiest)"},
	{0x17,	0x01, 0x0E, 0x02, "2"},
	{0x17,	0x01, 0x0E, 0x04, "3"},
	{0x17,	0x01, 0x0E, 0x06, "4 (Normal)"},
	{0x17,	0x01, 0x0E, 0x08, "5"},
	{0x17,	0x01, 0x0E, 0x0A, "6"},
	{0x17,	0x01, 0x0E, 0x0C, "7"},
	{0x17,	0x01, 0x0E, 0x0E, "8 (Hardest)"},
	{0,		0xFE, 0,	2,	  "Lives"},
	{0x17,	0x01, 0x10, 0x00, "3"},
	{0x17,	0x01, 0x10, 0x10, "2"},
	{0,		0xFE, 0,	2,	  "Bonus Life"},
	{0x17,	0x01, 0x20, 0x00, "300000"},
	{0x17,	0x01, 0x20, 0x20, "200000"},

};

STDDIPINFO(turbofrc)

static struct BurnInputInfo karatblzInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvButton + 2,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},
	{"P1 Button 4",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvButton + 3,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},
	{"P2 Button 4",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"},
	
	{"P3 Coin",		BIT_DIGITAL,	DrvButton + 4,	"p3 coin"},
	{"P3 Start",	BIT_DIGITAL,	DrvButton + 6,	"p3 start"},

	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"},
	{"P3 Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"},
	{"P3 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"},
	{"P3 Button 2",	BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"},
	{"P3 Button 3",	BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"},
	{"P3 Button 4",	BIT_DIGITAL,	DrvJoy3 + 7,	"p3 fire 4"},

	{"P4 Coin",		BIT_DIGITAL,	DrvButton + 5,	"p4 coin"},
	{"P4 Start",	BIT_DIGITAL,	DrvButton + 7,	"p4 start"},

	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 left"},
	{"P4 Right",	BIT_DIGITAL,	DrvJoy4 + 3,	"p4 right"},
	{"P4 Button 1",	BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"},
	{"P4 Button 2",	BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"},
	{"P4 Button 3",	BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"},
	{"P4 Button 4",	BIT_DIGITAL,	DrvJoy4 + 7,	"p4 fire 4"},
	
	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"},
};

STDINPUTINFO(karatblz)

static struct BurnDIPInfo karatblzDIPList[] = {

	// Defaults
	{0x29,	0xFF, 0xFF,	0x00, NULL},
	{0x2A,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	8,	  "Coinage"},
	{0x29,	0x01, 0x07,	0x00, "1 coin 1 credit"},
	{0x29,	0x01, 0x07,	0x01, "2 coins 1 credit"},
	{0x29,	0x01, 0x07,	0x02, "3 coins 1 credit"},
	{0x29,	0x01, 0x07,	0x03, "4 coins 1 credit"},
	{0x29,	0x01, 0x07,	0x04, "1 coin 2 credit"},
	{0x29,	0x01, 0x07,	0x05, "1 coin 3 credit"},
	{0x29,	0x01, 0x07,	0x06, "1 coin 5 credit"},
	{0x29,	0x01, 0x07,	0x07, "1 coin 6 credit"},
	{0,		0xFE, 0,	2,	  "2 Coins to Start, 1 to Continue"},
	{0x29,	0x01, 0x08,	0x00, "Off"},
	{0x29,	0x01, 0x08,	0x08, "On"},
	{0,		0xFE, 0,	2,	  "Lives"},
	{0x29,	0x01, 0x10,	0x00, "2"},
	{0x29,	0x01, 0x10,	0x10, "1"},
	{0,		0xFE, 0,	4,	  "Cabinet"},
	{0x29,	0x01, 0x60,	0x00, "2 Players"},
	{0x29,	0x01, 0x60,	0x20, "3 Players"},
	{0x29,	0x01, 0x60,	0x40, "4 Players"},
	{0x29,	0x01, 0x60,	0x60, "4 Players (Team)"},
	{0,		0xFE, 0,	2,	  "Coin Slot"},
	{0x29,	0x01, 0x80,	0x00, "Same"},
	{0x29,	0x01, 0x80,	0x80, "Individual"},
	
	// DIP 2
	{0,		0xFE, 0,	2,	  "Unknown"},
	{0x2A,	0x01, 0x01,	0x00, "Off"},
	{0x2A,	0x01, 0x01,	0x01, "On"},
	{0,		0xFE, 0,	4,	  "Number of Enemies"},
	{0x2A,	0x01, 0x06,	0x00, "Normal"},
	{0x2A,	0x01, 0x06,	0x02, "Easy"},
	{0x2A,	0x01, 0x06,	0x04, "Hard"},
	{0x2A,	0x01, 0x06,	0x06, "Hardest"},
	{0,		0xFE, 0,	4,	  "Strength of Enemies"},
	{0x2A,	0x01, 0x18,	0x00, "Normal"},
	{0x2A,	0x01, 0x18,	0x08, "Easy"},
	{0x2A,	0x01, 0x18,	0x10, "Hard"},
	{0x2A,	0x01, 0x18,	0x18, "Hardest"},
	{0,		0xFE, 0,	2,	  "Freeze"},
	{0x2A,	0x01, 0x20, 0x00, "Off"},
	{0x2A,	0x01, 0x20, 0x20, "On"},
	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x2A,	0x01, 0x40, 0x00, "Off"},
	{0x2A,	0x01, 0x40, 0x40, "On"},
	{0,		0xFE, 0,	2,	  "Flip Screen"},
	{0x2A,	0x01, 0x80, 0x00, "Off"},
	{0x2A,	0x01, 0x80, 0x80, "On"},
};

STDDIPINFO(karatblz)

static struct BurnInputInfo spinlbrkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvButton + 2,	"p1 start"},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvButton + 3,	"p2 start"},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},
	
	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 3,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
};

STDINPUTINFO(spinlbrk)

static struct BurnDIPInfo spinlbrkDIPList[] = {

	// Defaults
	{0x13,	0xFF, 0xFF,	0x00, NULL},
	{0x14,	0xFF, 0xFF,	0x00, NULL},

	// DIP 1
	{0,		0xFE, 0,	16,	  "Coin A"},
	{0x13,	0x01, 0x0F, 0x00, "1-1C 1-2 HPs"},
	{0x13,	0x01, 0x0F, 0x01, "1-1-1-1C 1-1-1-2 HPs"},
	{0x13,	0x01, 0x0F, 0x02, "1-1-1-1-1C 1-1-1-1-2 HPs"},
	{0x13,	0x01, 0x0F, 0x03, "2-2C 1-2 HPs"},
	{0x13,	0x01, 0x0F, 0x04, "2-1-1C  1-1-1 HPs"},
	{0x13,	0x01, 0x0F, 0x05, "2 Credits 2 Health Packs"},
	{0x13,	0x01, 0x0F, 0x06, "5 Credits 1 Health Pack"},
	{0x13,	0x01, 0x0F, 0x07, "4 Credits 1 Health Pack"},
	{0x13,	0x01, 0x0F, 0x08, "3 Credits 1 Health Pack"},
	{0x13,	0x01, 0x0F, 0x09, "2 Credits 1 Health Pack"},
	{0x13,	0x01, 0x0F, 0x0A, "1 Credit 6 Health Packs"},
	{0x13,	0x01, 0x0F, 0x0B, "1 Credit 5 Health Packs"},
	{0x13,	0x01, 0x0F, 0x0C, "1 Credit 4 Health Packs"},
	{0x13,	0x01, 0x0F, 0x0D, "1 Credit 3 Health Packs"},
	{0x13,	0x01, 0x0F, 0x0E, "1 Credit 2 Health Packs"},
	{0x13,	0x01, 0x0F, 0x0F, "1 Credit 1 Health Pack"},
	{0,		0xFE, 0,	16,	  "Coin A"},
	{0x13,	0x01, 0xF0, 0x00, "1-1C 1-2 HPs"},
	{0x13,	0x01, 0xF0, 0x10, "1-1-1-1C 1-1-1-2 HPs"},
	{0x13,	0x01, 0xF0, 0x20, "1-1-1-1-1C 1-1-1-1-2 HPs"},
	{0x13,	0x01, 0xF0, 0x30, "2-2C 1-2 HPs"},
	{0x13,	0x01, 0xF0, 0x40, "2-1-1C  1-1-1 HPs"},
	{0x13,	0x01, 0xF0, 0x50, "2 Credits 2 Health Packs"},
	{0x13,	0x01, 0xF0, 0x60, "5 Credits 1 Health Pack"},
	{0x13,	0x01, 0xF0, 0x70, "4 Credits 1 Health Pack"},
	{0x13,	0x01, 0xF0, 0x80, "3 Credits 1 Health Pack"},
	{0x13,	0x01, 0xF0, 0x90, "2 Credits 1 Health Pack"},
	{0x13,	0x01, 0xF0, 0xA0, "1 Credit 6 Health Packs"},
	{0x13,	0x01, 0xF0, 0xB0, "1 Credit 5 Health Packs"},
	{0x13,	0x01, 0xF0, 0xC0, "1 Credit 4 Health Packs"},
	{0x13,	0x01, 0xF0, 0xD0, "1 Credit 3 Health Packs"},
	{0x13,	0x01, 0xF0, 0xE0, "1 Credit 2 Health Packs"},
	{0x13,	0x01, 0xF0, 0xF0, "1 Credit 1 Health Pack"},		

	// DIP 2
	{0,		0xFE, 0,	4,	  "Difficulty"},
	{0x14,	0x01, 0x03, 0x00, "Normal"},
	{0x14,	0x01, 0x03, 0x01, "Easy"},
	{0x14,	0x01, 0x03, 0x02, "Hard"},
	{0x14,	0x01, 0x03, 0x03, "Hardest"},
};

static struct BurnDIPInfo spinlbrk_DIPList[] = {

	{0,		0xFE, 0,	2,	  "Coin Slot"},	
	{0x14,	0x01, 0x04, 0x00, "Individuala"},
	{0x14,	0x01, 0x04, 0x04, "Same"},
	{0,		0xFE, 0,	2,	  "Flip Screen"},
	{0x14,	0x01, 0x08, 0x00, "Off"},
	{0x14,	0x01, 0x08, 0x08, "On"},

	{0,		0xFE, 0,	2,	  "Lever Type"},
	{0x14,	0x01, 0x10, 0x00, "Digital"},
	{0x14,	0x01, 0x10, 0x10, "Analog"},
	{0,		0xFE, 0,	2,	  "Service"},
	{0x14,	0x01, 0x20, 0x00, "Off"},
	{0x14,	0x01, 0x20, 0x20, "On"},
	{0,		0xFE, 0,	2,	  "Health Pack"},
	{0x14,	0x01, 0x40, 0x00, "32 Hitpoints"},
	{0x14,	0x01, 0x40, 0x40, "40 Hitpoints"},
	{0,		0xFE, 0,	2,	  "Life Restoration"},
	{0x14,	0x01, 0x80, 0x00, "10 Points"},
	{0x14,	0x01, 0x80, 0x80, "5 Points"},
};

STDDIPINFOEXT(spinlbrk, spinlbrk, spinlbrk_)

static struct BurnDIPInfo spinlbru_DIPList[] = {

	{0,		0xFE, 0,	2,	  "Coin Slot"},	
	{0x14,	0x01, 0x04, 0x00, "Individuala"},
	{0x14,	0x01, 0x04, 0x04, "Same"},
	{0,		0xFE, 0,	2,	  "Flip Screen"},
	{0x14,	0x01, 0x08, 0x00, "Off"},
	{0x14,	0x01, 0x08, 0x08, "On"},

	{0,		0xFE, 0,	2,	  "Lever Type"},
	{0x14,	0x01, 0x10, 0x00, "Digital"},
	{0x14,	0x01, 0x10, 0x10, "Analog"},
	{0,		0xFE, 0,	2,	  "Service"},
	{0x14,	0x01, 0x20, 0x00, "Off"},
	{0x14,	0x01, 0x20, 0x20, "On"},
	{0,		0xFE, 0,	2,	  "Health Pack"},
	{0x14,	0x01, 0x40, 0x00, "20 Hitpoints"},
	{0x14,	0x01, 0x40, 0x40, "32 Hitpoints"},
	{0,		0xFE, 0,	2,	  "Life Restoration"},
	{0x14,	0x01, 0x80, 0x00, "10 Points"},
	{0x14,	0x01, 0x80, 0x80, "5 Points"},
};

STDDIPINFOEXT(spinlbru, spinlbrk, spinlbru_)

static struct BurnDIPInfo spinlbrj_DIPList[] = {

	{0,		0xFE, 0,	2,	  "Continue"},	
	{0x14,	0x01, 0x04, 0x00, "Unlimited"},
	{0x14,	0x01, 0x04, 0x04, "6 Times"},
	{0,		0xFE, 0,	2,	  "Flip Screen"},
	{0x14,	0x01, 0x08, 0x00, "Off"},
	{0x14,	0x01, 0x08, 0x08, "On"},

	{0,		0xFE, 0,	2,	  "Lever Type"},
	{0x14,	0x01, 0x10, 0x00, "Digital"},
	{0x14,	0x01, 0x10, 0x10, "Analog"},
	{0,		0xFE, 0,	2,	  "Service"},
	{0x14,	0x01, 0x20, 0x00, "Off"},
	{0x14,	0x01, 0x20, 0x20, "On"},
	{0,		0xFE, 0,	2,	  "Health Pack"},
	{0x14,	0x01, 0x40, 0x00, "32 Hitpoints"},
	{0x14,	0x01, 0x40, 0x40, "40 Hitpoints"},
	{0,		0xFE, 0,	2,	  "Life Restoration"},
	{0x14,	0x01, 0x80, 0x00, "10 Points"},
	{0x14,	0x01, 0x80, 0x80, "5 Points"},
};

STDDIPINFOEXT(spinlbrj, spinlbrk, spinlbrj_)

static struct BurnInputInfo PspikesInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 6,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 4,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"},
};

STDINPUTINFO(Pspikes)

static struct BurnDIPInfo aerofgtb_DIPList[] = {
	
	{0x14,	0xFF, 0xFF,	0x01, NULL},

	// DIP 3
	{0,		0xFE, 0,	2,	  "Region"},
	{0x14,	0x01, 0x01, 0x00, "Taiwan"},
	{0x14,	0x01, 0x01, 0x01, "Japan"},
};

STDDIPINFOEXT(aerofgtb, aerofgt, aerofgtb_)

static struct BurnDIPInfo PspikesDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbf, NULL		},
	{0x15, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x03, 0x01, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x40, 0x40, "Off"		},
	{0x14, 0x01, 0x40, 0x00, "On"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x14, 0x01, 0x80, 0x80, "Off"		},
//	{0x14, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x01, 0x01, "Off"		},
	{0x15, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "1 Player Starting Score"	},
	{0x15, 0x01, 0x06, 0x06, "12-12"		},
	{0x15, 0x01, 0x06, 0x04, "11-11"		},
	{0x15, 0x01, 0x06, 0x02, "11-12"		},
	{0x15, 0x01, 0x06, 0x00, "10-12"		},

	{0   , 0xfe, 0   ,    4, "2 Players Starting Score"	},
	{0x15, 0x01, 0x18, 0x18, "9-9"		},
	{0x15, 0x01, 0x18, 0x10, "7-7"		},
	{0x15, 0x01, 0x18, 0x08, "5-5"		},
	{0x15, 0x01, 0x18, 0x00, "0-0"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x20, 0x20, "Normal"		},
	{0x15, 0x01, 0x20, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "2 Players Time Per Credit"		},
	{0x15, 0x01, 0x40, 0x40, "3 min"		},
	{0x15, 0x01, 0x40, 0x00, "2 min"		},

	{0   , 0xfe, 0   ,    2, "Debug"		},
	{0x15, 0x01, 0x80, 0x80, "Off"		},
	{0x15, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Pspikes)

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = (nColour & 0x001F) << 3;	// Red
	r |= r >> 5;
	g = (nColour & 0x03E0) >> 2;  	// Green
	g |= g >> 5;
	b = (nColour & 0x7C00) >> 7;	// Blue
	b |= b >> 5;

	return BurnHighCol(b, g, r, 0);
}

static UINT8 __fastcall aerofgtReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xFFFFA1:
			return ~DrvInput[0];
		case 0xFFFFA3:
			return ~DrvInput[1];
		case 0xFFFFA5:
			return ~DrvInput[2];
		case 0xFFFFA7:
			return ~DrvInput[3];
		case 0xFFFFA9:
			return ~DrvInput[4];
		case 0xFFFFAD:
			//printf("read pending_command %d addr %08x\n", pending_command, sekAddress);
			return pending_command;
		case 0xFFFFAF:
			return ~DrvInput[5];
//		default:
//			printf("Attempt to read byte value of location %x\n", sekAddress);
	}
	return 0;
}

static void SoundCommand(UINT8 nCommand)
{
//	bprintf(PRINT_NORMAL, _T("  - Sound command sent (0x%02X).\n"), nCommand);
	INT32 nCycles = ((INT64)SekTotalCycles() * nCyclesTotal[1] / nCyclesTotal[0]);
	if (nCycles <= ZetTotalCycles()) return;
	
	BurnTimerUpdate(nCycles);

	nSoundlatch = nCommand;
	ZetNmi();
}

static void __fastcall aerofgtWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	if (( sekAddress & 0xFF0000 ) == 0x1A0000) {
		sekAddress &= 0xFFFF;
		if (sekAddress < 0x800) {
			RamPal[sekAddress^1] = byteValue;
			// palette byte write at boot self-test only ?!
			// if (sekAddress & 1)
			//	RamCurPal[sekAddress>>1] = CalcCol( *((UINT16 *)&RamPal[sekAddress]) );
		}
		return;	
	}

	switch (sekAddress) {
		case 0xFFFFC1:
			pending_command = 1;
			SoundCommand(byteValue);
			break;
/*			
		case 0xFFFFB9:
		case 0xFFFFBB:
		case 0xFFFFBD:
		case 0xFFFFBF:
			break;
		case 0xFFFFAD:
			// NOP
			break;
		default:
			printf("Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
*/
	}
}

static void __fastcall aerofgtWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0xFF0000 ) == 0x1A0000) {
		sekAddress &= 0xFFFF;
		if (sekAddress < 0x800) {
			*((UINT16 *)&RamPal[sekAddress]) = BURN_ENDIAN_SWAP_INT16(wordValue);
			RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		}
		return;	
	}

	switch (sekAddress) {
		case 0xFFFF80:
			RamGfxBank[0] = wordValue >> 0x08;
			RamGfxBank[1] = wordValue & 0xFF;
			break;
		case 0xFFFF82:
			RamGfxBank[2] = wordValue >> 0x08;
			RamGfxBank[3] = wordValue & 0xFF;
			break;
		case 0xFFFF84:
			RamGfxBank[4] = wordValue >> 0x08;
			RamGfxBank[5] = wordValue & 0xFF;
			break;
		case 0xFFFF86:
			RamGfxBank[6] = wordValue >> 0x08;
			RamGfxBank[7] = wordValue & 0xFF;
			break;
		case 0xFFFF88:
			bg1scrolly = wordValue;
			break;
		case 0xFFFF90:
			bg2scrolly = wordValue;
			break;
/*
		case 0xFFFF40:
		case 0xFFFF42:
		case 0xFFFF44:
		case 0xFFFF48:
		case 0xFFFF4A:
		case 0xFFFF4C:
		case 0xFFFF50:
		case 0xFFFF52:
		case 0xFFFF60:
			break;
		default:
			printf("Attempt to write word value %x to location %x\n", wordValue, sekAddress);
*/
	}
}

static UINT8 __fastcall turbofrcReadByte(UINT32 sekAddress)
{
	sekAddress &= 0x0FFFFF;
	switch (sekAddress) {
		case 0x0FF000:
			return ~DrvInput[3];
		case 0x0FF001:
			return ~DrvInput[0];
		case 0x0FF002:
			return 0xFF;
		case 0x0FF003:
			return ~DrvInput[1];
		case 0x0FF004:
			return ~DrvInput[5];
		case 0x0FF005:
			return ~DrvInput[4];
		case 0x0FF007:
			return pending_command;
		case 0x0FF009:
			return ~DrvInput[2];
//		default:
//			printf("Attempt to read byte value of location %x\n", sekAddress);
	}
	return 0;
}

static void __fastcall turbofrcWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	if (( sekAddress & 0x0FF000 ) == 0x0FE000) {
		sekAddress &= 0x07FF;
		RamPal[sekAddress^1] = byteValue;
		// palette byte write at boot self-test only ?!
		//if (sekAddress & 1)
		//	RamCurPal[sekAddress>>1] = CalcCol( *((UINT16 *)&RamPal[sekAddress & 0xFFE]) );
		return;	
	}
	
	sekAddress &= 0x0FFFFF;
	
	switch (sekAddress) {
	
		case 0x0FF00E:
			pending_command = 1;
			SoundCommand(byteValue);
			break;	
//		default:
//			printf("Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
	}
}

static void __fastcall turbofrcWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0x0FF000 ) == 0x0FE000) {
		sekAddress &= 0x07FE;
		*((UINT16 *)&RamPal[sekAddress]) = BURN_ENDIAN_SWAP_INT16(wordValue);
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}

	sekAddress &= 0x0FFFFF;

	switch (sekAddress) {
		case 0x0FF002:
			bg1scrolly = wordValue;
			break;
		case 0x0FF004:
			bg2scrollx = wordValue;
			break;
		case 0x0FF006:
			bg2scrolly = wordValue;
			break;
			
		case 0x0FF008:
			RamGfxBank[0] = (wordValue >>  0) & 0x0f;
			RamGfxBank[1] = (wordValue >>  4) & 0x0f;
			RamGfxBank[2] = (wordValue >>  8) & 0x0f;
			RamGfxBank[3] = (wordValue >> 12) & 0x0f;
			break;
		case 0x0FF00A:
			RamGfxBank[4] = (wordValue >>  0) & 0x0f;
			RamGfxBank[5] = (wordValue >>  4) & 0x0f;
			RamGfxBank[6] = (wordValue >>  8) & 0x0f;
			RamGfxBank[7] = (wordValue >> 12) & 0x0f;
			break;
		case 0x0FF00C:
			// NOP
			break;
//		default:
//			printf("Attempt to write word value %x to location %x\n", wordValue, sekAddress);
	}
}

static UINT8 __fastcall karatblzReadByte(UINT32 sekAddress)
{
	sekAddress &= 0x0FFFFF;
	
	switch (sekAddress) {
		case 0x0FF000:
			return ~DrvInput[4];
		case 0x0FF001:
			return ~DrvInput[0];
		case 0x0FF002:
			return 0xFF;
		case 0x0FF003:
			return ~DrvInput[1];
		case 0x0FF004:
			return ~DrvInput[5];
		case 0x0FF005:
			return ~DrvInput[2];
		case 0x0FF006:
			return 0xFF;
		case 0x0FF007:
			return ~DrvInput[3];
		case 0x0FF008:
			return ~DrvInput[7];
		case 0x0FF009:
			return ~DrvInput[6];
		case 0x0FF00B:
			return pending_command;

//		default:
//			printf("Attempt to read byte value of location %x\n", sekAddress);
	}
	return 0;
}

static void __fastcall karatblzWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	sekAddress &= 0x0FFFFF;
	
	switch (sekAddress) {
		case 0x0FF000:
		case 0x0FF401:
		case 0x0FF403:
			
			break;
		
		case 0x0FF002:
			//if (ACCESSING_MSB) {
			//	setbank(bg1_tilemap,0,(data & 0x0100) >> 8);
			//	setbank(bg2_tilemap,1,(data & 0x0800) >> 11);	
			//}
			RamGfxBank[0] = (byteValue & 0x1);
			RamGfxBank[1] = (byteValue & 0x8) >> 3;
			break;
		case 0x0FF007:
			pending_command = 1;
			SoundCommand(byteValue);
			break;	
//		default:
//			printf("Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
	}
}

static void __fastcall karatblzWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0x0FF000 ) == 0x0FE000) {
		sekAddress &= 0x07FF;
		*((UINT16 *)&RamPal[sekAddress]) = wordValue;
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}

	sekAddress &= 0x0FFFFF;

	switch (sekAddress) {
		case 0x0ff008:
			bg1scrollx = wordValue;
			break;
		case 0x0ff00A:
			bg1scrolly = wordValue;
			break;	
		case 0x0ff00C:
			bg2scrollx = wordValue;
			break;
		case 0x0ff00E:
			bg2scrolly = wordValue;
			break;	
//		default:
//			printf("Attempt to write word value %x to location %x\n", wordValue, sekAddress);
	}
}

static UINT8 __fastcall aerofgtbReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0x0FE000:
			return ~DrvInput[2];
		case 0x0FE001:
			return ~DrvInput[0];
		case 0x0FE002:
			return 0xFF;
		case 0x0FE003:
			return ~DrvInput[1];
		case 0x0FE004:
			return ~DrvInput[4];			
		case 0x0FE005:
			return ~DrvInput[3];
		case 0x0FE007:
			return pending_command;
		case 0x0FE009:
			return ~DrvInput[5];
			
		default:
			printf("Attempt to read byte value of location %x\n", sekAddress);
	}
	return 0;
}

static UINT16 __fastcall aerofgtbReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {


		default:
			printf("Attempt to read word value of location %x\n", sekAddress);
	}
	return 0;
}

static void __fastcall aerofgtbWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	if (( sekAddress & 0x0FF000 ) == 0x0FD000) {
		sekAddress &= 0x07FF;
		RamPal[sekAddress^1] = byteValue;
		// palette byte write at boot self-test only ?!
		//if (sekAddress & 1)
		//	RamCurPal[sekAddress>>1] = CalcCol( *((UINT16 *)&RamPal[sekAddress & 0xFFE]) );
		return;	
	}
	
	switch (sekAddress) {
		case 0x0FE001:
		case 0x0FE401:
		case 0x0FE403:
			// NOP
			break;
			
		case 0x0FE00E:
			pending_command = 1;
			SoundCommand(byteValue);
			break;	

		default:
			printf("Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
	}
}

static void __fastcall aerofgtbWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0x0FF000 ) == 0x0FD000) {
		sekAddress &= 0x07FE;
		*((UINT16 *)&RamPal[sekAddress]) = BURN_ENDIAN_SWAP_INT16(wordValue);
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}

	switch (sekAddress) {
		case 0x0FE002:
			bg1scrolly = wordValue;
			break;
		case 0x0FE004:
			bg2scrollx = wordValue;
			break;
		case 0x0FE006:
			bg2scrolly = wordValue;
			break;
		case 0x0FE008:
			RamGfxBank[0] = (wordValue >>  0) & 0x0f;
			RamGfxBank[1] = (wordValue >>  4) & 0x0f;
			RamGfxBank[2] = (wordValue >>  8) & 0x0f;
			RamGfxBank[3] = (wordValue >> 12) & 0x0f;
			break;
		case 0x0FE00A:
			RamGfxBank[4] = (wordValue >>  0) & 0x0f;
			RamGfxBank[5] = (wordValue >>  4) & 0x0f;
			RamGfxBank[6] = (wordValue >>  8) & 0x0f;
			RamGfxBank[7] = (wordValue >> 12) & 0x0f;
			break;
		case 0x0FE00C:
			// NOP
			break;

		default:
			printf("Attempt to write word value %x to location %x\n", wordValue, sekAddress);
	}
}

static UINT16 __fastcall spinlbrkReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xFFF000:
			return ~(DrvInput[0] | (DrvInput[2] << 8));
		case 0xFFF002:
			return ~(DrvInput[1]);
		case 0xFFF004:
			return ~(DrvInput[3] | (DrvInput[4] << 8));

//		default:
//			printf("Attempt to read word value of location %x\n", sekAddress);
	}
	return 0;
}


static void __fastcall spinlbrkWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {
		case 0xFFF401:
		case 0xFFF403:
			// NOP
			break;
			
		case 0xFFF007:
			pending_command = 1;
			SoundCommand(byteValue);
			break;	

//		default:
//			printf("Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
	}
}

static void __fastcall spinlbrkWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0xFFF000 ) == 0xFFE000) {
		sekAddress &= 0x07FF;
		*((UINT16 *)&RamPal[sekAddress]) = BURN_ENDIAN_SWAP_INT16(wordValue);
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}

	switch (sekAddress) {
		case 0xFFF000:
			RamGfxBank[0] = (wordValue & 0x07);
			RamGfxBank[1] = (wordValue & 0x38) >> 3;
			break;
		case 0xFFF002:
			bg2scrollx = wordValue;
			break;
		case 0xFFF008:
			// NOP
			break;

//		default:
//			printf("Attempt to write word value %x to location %x\n", wordValue, sekAddress);
	}
}

static void __fastcall pspikesWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (( sekAddress & 0xFFF000 ) == 0xFFE000) {
		sekAddress &= 0x0FFE;
		*((UINT16 *)&RamPal[sekAddress]) = BURN_ENDIAN_SWAP_INT16(wordValue);
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}
}

static void __fastcall pspikesWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	if (( sekAddress & 0xFFF000 ) == 0xFFE000) {
		sekAddress &= 0x0FFF;
		RamPal[sekAddress^1] = byteValue;

		UINT16 wordValue = *((UINT16 *)&RamPal[sekAddress & ~1]);
		RamCurPal[sekAddress>>1] = CalcCol( wordValue );
		return;	
	}

	switch (sekAddress)
	{
		case 0xfff001:
			spritepalettebank = byteValue & 0x03;
			charpalettebank = (byteValue & 0x1c) >> 2;
		return;

		case 0xfff003:
			RamGfxBank[0] = (byteValue >> 4) & 0x0f;
			RamGfxBank[1] = byteValue & 0x0f;
		return;

		case 0xfff005:
			bg1scrolly = byteValue;
		return;

		case 0xfff007:
			pending_command = 1;
			SoundCommand(byteValue);
		return;
	}
}


static UINT8 __fastcall pspikesReadByte(UINT32 sekAddress)
{
	bprintf (0, _T("RB: %5.5x\n"), sekAddress);

	switch (sekAddress)
	{
		case 0xFFF001:
			return ~DrvInput[0];

		case 0xFFF000:
			return ~DrvInput[1];

		case 0xFFF003:
			return ~DrvInput[2];

		case 0xFFF005:
			return DrvInput[4];

		case 0xFFF004:
			return DrvInput[5];


		case 0xFFF007:
			return pending_command;
	}

	return 0;
}

static void aerofgtFMIRQHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;
//	bprintf(PRINT_NORMAL, _T("  - IRQ -> %i.\n"), nStatus);
	if (nStatus) {
		ZetSetIRQLine(0xFF, CPU_IRQSTATUS_ACK);
	} else {
		ZetSetIRQLine(0,    CPU_IRQSTATUS_NONE);
	}
}

static INT32 aerofgtSynchroniseStream(INT32 nSoundRate)
{
	if (ZetGetActive() == -1) return 0;
	return (INT64)ZetTotalCycles() * nSoundRate / 5000000;
}

static double aerofgtGetTime()
{
	if (ZetGetActive() == -1) return 0;
	return (double)ZetTotalCycles() / 5000000.0;
}

static void aerofgtSndBankSwitch(UINT8 v)
{
	v &= 0x03;

	if (v != nAerofgtZ80Bank) {
		UINT8* nStartAddress = RomZ80 + 0x10000 + (v << 15);
		ZetMapArea(0x8000, 0xFFFF, 0, nStartAddress);
		ZetMapArea(0x8000, 0xFFFF, 2, nStartAddress);
		nAerofgtZ80Bank = v;
	}
}

static UINT8 __fastcall aerofgtZ80PortRead(UINT16 p)
{
	switch (p & 0xFF) {
		case 0x00:
			return BurnYM2610Read(0);
		case 0x02:
			return BurnYM2610Read(2);
		case 0x0C:
			return nSoundlatch;
//		default:
//			printf("Z80 Attempt to read port %04x\n", p);
	}
	return 0;
}

static void __fastcall aerofgtZ80PortWrite(UINT16 p, UINT8 v)
{
	switch (p & 0x0FF) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			BurnYM2610Write(p & 3, v);
			break;
		case 0x04:
			aerofgtSndBankSwitch(v);
			break;
		case 0x08:
			pending_command = 0;
			break;
//		default:
//			printf("Z80 Attempt to write %02x to port %04x\n", v, p);
	}
}

static UINT8 __fastcall turbofrcZ80PortRead(UINT16 p)
{
	switch (p & 0xFF) {
		case 0x14:
			return nSoundlatch;
		case 0x18:
			return BurnYM2610Read(0);
		case 0x1A:
			return BurnYM2610Read(2);
//		default:
//			printf("Z80 Attempt to read port %04x\n", p);
	}
	return 0;
}

static void __fastcall turbofrcZ80PortWrite(UINT16 p, UINT8 v)
{
	switch (p & 0x0FF) {
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
			BurnYM2610Write((p - 0x18) & 3, v);
			break;
		case 0x00:
			aerofgtSndBankSwitch(v);
			break;
		case 0x14:
			pending_command = 0;
			break;
//		default:
//			printf("Z80 Attempt to write %02x to port %04x\n", v, p);
	}
}


static void aerofgtAssembleInputs()
{
	DrvInput[0] = 0x00;
	DrvInput[1] = 0x00;
	DrvInput[2] = 0x00;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvButton[i] & 1) << i;
	}
}

static void karatblzAssembleInputs()
{
	DrvInput[0] = 0x00;
	DrvInput[1] = 0x00;
	DrvInput[2] = 0x00;
	DrvInput[3] = 0x00;
	DrvInput[4] = 0x00;
	DrvInput[5] = 0x00;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvJoy3[i] & 1) << i;
		DrvInput[3] |= (DrvJoy4[i] & 1) << i;
	}
	for (INT32 i = 0; i < 4; i++) {
		DrvInput[4] |= (DrvButton[i] & 1) << i;
		DrvInput[5] |= (DrvButton[i+4] & 1) << i;
	}
}

static void turbofrcAssembleInputs()
{
	DrvInput[0] = 0x00;
	DrvInput[1] = 0x00;
	DrvInput[2] = 0x00;
	DrvInput[3] = 0x00;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvJoy3[i] & 1) << i;
		DrvInput[3] |= (DrvButton[i] & 1) << i;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x080000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	RomBg		= Next; Next += 0x200040;			// Background, 1M 8x8x4bit decode to 2M + 64Byte safe 
	DeRomBg		= 	   RomBg +  0x000040;
	RomSpr1		= Next; Next += 0x200000;			// Sprite 1	 , 1M 16x16x4bit decode to 2M + 256Byte safe 
	RomSpr2		= Next; Next += 0x100100;			// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    += 0x000100;
	
	RomSnd1		= Next; Next += 0x040000;			// ADPCM data
	RomSndSize1 = 0x040000;
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSndSize2 = 0x100000;
	RamStart	= Next;
	
	RamPal		= Next; Next += 0x000800;			// 1024 of X1R5G5B5 Palette
	RamRaster	= (UINT16 *)Next; Next += 0x000800 * sizeof(UINT16);	// 0x1b0000~0x1b07ff, Raster
															// 0x1b0800~0x1b0801, NOP
															// 0x1b0ff0~0x1b0fff, stack area during boot
	RamBg1V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG1 Video Ram
	RamBg2V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG2 Video Ram
	RamSpr1		= (UINT16 *)Next; Next += 0x004000 * sizeof(UINT16);	// Sprite 1 Ram
	RamSpr2		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// Sprite 2 Ram
	Ram01		= Next; Next += 0x010000;					// Work Ram
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamEnd		= Next;

	RamCurPal	= (UINT32 *)Next; Next += 0x000400 * sizeof(UINT32);	// 1024 colors

	MemEnd		= Next;
	return 0;
}

static INT32 turbofrcMemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x0C0000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	
	RomBg		= Next; Next += 0x400040;			// 
	DeRomBg		= 	   RomBg +  0x000040;
	
	RomSpr1		= Next; Next += 0x400000;			// Sprite 1
	RomSpr2		= Next; Next += 0x200100;			// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    += 0x000100;
	
	RomSnd1		= Next; Next += 0x020000;			// ADPCM data
	RomSndSize1 = 0x020000;
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSndSize2 = 0x100000;
	
	RamStart	= Next;

	RamBg1V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG1 Video Ram
	RamBg2V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG2 Video Ram
	RamSpr1		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 1 Ram
	RamSpr2		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 2 Ram
	RamSpr3		= (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);	// Sprite 3 Ram
	RamRaster	= (UINT16 *)Next; Next += 0x000800 * sizeof(UINT16);	// Raster Ram
	
	RamSpr1SizeMask = 0x1FFF;
	RamSpr2SizeMask = 0x1FFF;
	RomSpr1SizeMask = 0x3FFF;
	RomSpr2SizeMask = 0x1FFF;
	
	Ram01		= Next; Next += 0x014000;					// Work Ram
	
	RamPal		= Next; Next += 0x000800;					// 1024 of X1R5G5B5 Palette
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamEnd		= Next;

	RamPrioBitmap	= Next; Next += 352 * 240 * 2; // For turbofrc
	RamCurPal	= (UINT32 *)Next; Next += 0x000400 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static INT32 karatblzMemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x080000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	
	RomBg		= Next; Next += 0x200040;			// Background, 1M 8x8x4bit decode to 2M + 64Byte safe 
	DeRomBg		= 	   RomBg +  0x000040;
	
	RomSpr1		= Next; Next += 0x800000;			// Sprite 1	 , 1M 16x16x4bit decode to 2M + 256Byte safe 
	RomSpr2		= Next; Next += 0x200100;			// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    += 0x000100;
	
	RomSnd1		= Next; Next += 0x080000;			// ADPCM data
	RomSndSize1 = 0x080000;
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSndSize2 = 0x100000;
	
	RamStart	= Next;

	RamBg1V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG1 Video Ram
	RamBg2V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG2 Video Ram
	RamSpr1		= (UINT16 *)Next; Next += 0x008000 * sizeof(UINT16);	// Sprite 1 Ram
	RamSpr2		= (UINT16 *)Next; Next += 0x008000 * sizeof(UINT16);	// Sprite 2 Ram
	RamSpr3		= (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);	// Sprite 3 Ram
	Ram01		= Next; Next += 0x014000;					// Work Ram 1 + Work Ram 1
	RamPal		= Next; Next += 0x000800;					// 1024 of X1R5G5B5 Palette
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamSpr1SizeMask = 0x7FFF;
	RamSpr2SizeMask = 0x7FFF;
	RomSpr1SizeMask = 0x7FFF;
	RomSpr2SizeMask = 0x1FFF;

	RamEnd		= Next;

	RamCurPal	= (UINT32 *)Next; Next += 0x000400 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static INT32 spinlbrkMemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x040000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	
	RomBg		= Next; Next += 0x500050;			// Background, 2.5M 8x8x4bit
	DeRomBg		=      RomBg +  0x000040;
	
	RomSpr1		= Next; Next += 0x200000;			// Sprite 1
	RomSpr2		= Next; Next += 0x400110;			// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    +  0x000100;
	
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSnd1		= RomSnd2;

	RomSndSize1 = 0x100000;
	RomSndSize2 = 0x100000;
	
	RamSpr2		= (UINT16 *)Next; Next += 0x010000 * sizeof(UINT16);	// Sprite 2 Ram
	RamSpr1		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 1 Ram
		
	RamStart	= Next;

	RamBg1V		= (UINT16 *)Next; Next += 0x000800 * sizeof(UINT16);	// BG1 Video Ram
	RamBg2V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG2 Video Ram
	Ram01		= Next; Next += 0x004000;					// Work Ram
	RamSpr3		= (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);	// Sprite 3 Ram
	RamRaster	= (UINT16 *)Next; Next += 0x000100 * sizeof(UINT16);	// Raster
	RamPal		= Next; Next += 0x000800;					// 1024 of X1R5G5B5 Palette
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamSpr1SizeMask = 0x1FFF;
	RamSpr2SizeMask = 0xFFFF;
	
	RomSpr1SizeMask = 0x1FFF;
	RomSpr2SizeMask = 0x3FFF;

	RamEnd		= Next;

	RamCurPal	= (UINT32 *)Next; Next += 0x000400 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static INT32 aerofgtbMemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x080000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	RomBg		= Next; Next += 0x400040;			// Background, 1M 8x8x4bit decode to 2M + 64Byte safe 
	DeRomBg		= 	   RomBg +  0x000040;
	RomSpr1		= Next; Next += 0x200000;			// Sprite 1	 , 1M 16x16x4bit decode to 2M + 256Byte safe 
	RomSpr2		= Next; Next += 0x100100;			// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    += 0x000100;
	
	RomSnd1		= Next; Next += 0x040000;			// ADPCM data
	RomSndSize1 = 0x040000;
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSndSize2 = 0x100000;

	RamStart	= Next;
	
	Ram01		= Next; Next += 0x014000;					// Work Ram 
	RamBg1V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG1 Video Ram
	RamBg2V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG2 Video Ram
	RamSpr1		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 1 Ram
	RamSpr2		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 2 Ram
	RamSpr3		= (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);	// Sprite 3 Ram
	RamPal		= Next; Next += 0x000800;					// 1024 of X1R5G5B5 Palette
	RamRaster	= (UINT16 *)Next; Next += 0x000800 * sizeof(UINT16);	// Raster

	RamSpr1SizeMask = 0x1FFF;
	RamSpr2SizeMask = 0x1FFF;
	RomSpr1SizeMask = 0x1FFF;
	RomSpr2SizeMask = 0x0FFF;
	
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamEnd		= Next;

	RamCurPal	= (UINT32 *)Next; Next += 0x000400 * sizeof(UINT32);	// 1024 colors

	MemEnd		= Next;
	return 0;
}

static INT32 pspikesMemIndex()
{
	UINT8 *Next; Next = Mem;
	Rom01 		= Next; Next += 0x040000;			// 68000 ROM
	RomZ80		= Next; Next += 0x030000;			// Z80 ROM
	RomBg		= Next; Next += 0x100040;			// Background, 1M 8x8x4bit decode to 2M + 64Byte safe 
	DeRomBg		= 	   RomBg +  0x000040;
	RomSpr1		= Next; Next += 0x200000;			// Sprite 1	 , 1M 16x16x4bit decode to 2M + 256Byte safe 
	RomSpr2		= RomSpr1;				// Sprite 2
	
	DeRomSpr1	= RomSpr1    +  0x000100;
	DeRomSpr2	= RomSpr2    += 0x000100;
	
	RomSnd1		= Next; Next += 0x040000;			// ADPCM data
	RomSndSize1 = 0x040000;
	RomSnd2		= Next; Next += 0x100000;			// ADPCM data
	RomSndSize2 = 0x100000;

	RamStart	= Next;
	
	Ram01		= Next; Next += 0x010000;					// Work Ram 
	RamBg1V		= (UINT16 *)Next; Next += 0x001000 * sizeof(UINT16);	// BG1 Video Ram

	RamSpr1		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 1 Ram
	RamSpr2		= (UINT16 *)Next; Next += 0x002000 * sizeof(UINT16);	// Sprite 2 Ram
	RamSpr3		= (UINT16 *)Next; Next += 0x000400 * sizeof(UINT16);	// Sprite 3 Ram
	RamPal		= Next; Next += 0x001000;					// 1024 of X1R5G5B5 Palette
	RamRaster	= (UINT16 *)Next; Next += 0x000800 * sizeof(UINT16);	// Raster

	RamSpr1SizeMask = 0x1FFF;
	RamSpr2SizeMask = 0x1FFF;
	RomSpr1SizeMask = 0x1FFF;
	RomSpr2SizeMask = 0x1FFF;
	
	RamZ80		= Next; Next += 0x000800;					// Z80 Ram 2K

	RamEnd		= Next;

	RamCurPal	= (UINT32 *)Next; Next += 0x000800 * sizeof(UINT32);	// 1024 colors

	MemEnd		= Next;
	return 0;
}

static void pspikesDecodeBg(INT32 cnt)
{
	for (INT32 c=cnt-1; c>=0; c--) {
		for (INT32 y=7; y>=0; y--) {
			DeRomBg[(c * 64) + (y * 8) + 7] = RomBg[0x00003 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 6] = RomBg[0x00003 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 5] = RomBg[0x00002 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 4] = RomBg[0x00002 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 3] = RomBg[0x00001 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 2] = RomBg[0x00001 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 1] = RomBg[0x00000 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 0] = RomBg[0x00000 + (y * 4) + (c * 32)] & 0x0f;
		}
	}
}

static void pspikesDecodeSpr(UINT8 *d, UINT8 *s, INT32 cnt)
{
	for (INT32 c=cnt-1; c>=0; c--)
		for (INT32 y=15; y>=0; y--) {
			d[(c * 256) + (y * 16) + 15] = s[0x00007 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 14] = s[0x00007 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 13] = s[0x00005 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 12] = s[0x00005 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 11] = s[0x00006 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 10] = s[0x00006 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  9] = s[0x00004 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  8] = s[0x00004 + (y * 8) + (c * 128)] & 0x0f;

			d[(c * 256) + (y * 16) +  7] = s[0x00003 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  6] = s[0x00003 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  5] = s[0x00001 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  4] = s[0x00001 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  3] = s[0x00002 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  2] = s[0x00002 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  1] = s[0x00000 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  0] = s[0x00000 + (y * 8) + (c * 128)] & 0x0f;
		}
}


static void DecodeBg()
{
	for (INT32 c=0x8000-1; c>=0; c--) {
		for (INT32 y=7; y>=0; y--) {
			DeRomBg[(c * 64) + (y * 8) + 7] = RomBg[0x00002 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 6] = RomBg[0x00002 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 5] = RomBg[0x00003 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 4] = RomBg[0x00003 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 3] = RomBg[0x00000 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 2] = RomBg[0x00000 + (y * 4) + (c * 32)] >> 4;
			DeRomBg[(c * 64) + (y * 8) + 1] = RomBg[0x00001 + (y * 4) + (c * 32)] & 0x0f;
			DeRomBg[(c * 64) + (y * 8) + 0] = RomBg[0x00001 + (y * 4) + (c * 32)] >> 4;
		}
	}
}

static void DecodeSpr(UINT8 *d, UINT8 *s, INT32 cnt)
{
	for (INT32 c=cnt-1; c>=0; c--)
		for (INT32 y=15; y>=0; y--) {
			d[(c * 256) + (y * 16) + 15] = s[0x00006 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 14] = s[0x00006 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 13] = s[0x00007 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 12] = s[0x00007 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 11] = s[0x00004 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 10] = s[0x00004 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  9] = s[0x00005 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  8] = s[0x00005 + (y * 8) + (c * 128)] >> 4;

			d[(c * 256) + (y * 16) +  7] = s[0x00002 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  6] = s[0x00002 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  5] = s[0x00003 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  4] = s[0x00003 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  3] = s[0x00000 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  2] = s[0x00000 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  1] = s[0x00001 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  0] = s[0x00001 + (y * 8) + (c * 128)] >> 4;
		}
}

static void aerofgtbDecodeSpr(UINT8 *d, UINT8 *s, INT32 cnt)
{
	for (INT32 c=cnt-1; c>=0; c--)
		for (INT32 y=15; y>=0; y--) {
			d[(c * 256) + (y * 16) + 15] = s[0x00005 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 14] = s[0x00005 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 13] = s[0x00007 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 12] = s[0x00007 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) + 11] = s[0x00004 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) + 10] = s[0x00004 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  9] = s[0x00006 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  8] = s[0x00006 + (y * 8) + (c * 128)] & 0x0f;

			d[(c * 256) + (y * 16) +  7] = s[0x00001 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  6] = s[0x00001 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  5] = s[0x00003 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  4] = s[0x00003 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  3] = s[0x00000 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  2] = s[0x00000 + (y * 8) + (c * 128)] & 0x0f;
			d[(c * 256) + (y * 16) +  1] = s[0x00002 + (y * 8) + (c * 128)] >> 4;
			d[(c * 256) + (y * 16) +  0] = s[0x00002 + (y * 8) + (c * 128)] & 0x0f;
		}
}

static INT32 DrvDoReset()
{
	nAerofgtZ80Bank = -1;
		
	SekOpen(0);
	//SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	aerofgtSndBankSwitch(0);
	ZetClose();
	BurnYM2610Reset();

	memset(RamGfxBank, 0 , sizeof(RamGfxBank));

	spritepalettebank = 0;
	charpalettebank = 0;

	nSoundlatch = 0;
	bg1scrollx = 0;
	bg2scrollx = 0;
	bg1scrolly = 0;
	bg2scrolly = 0;

	HiscoreReset();

	return 0;
}

static void aerofgt_sound_init()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(RomZ80, 0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(RamZ80, 0x7800, 0x7fff, MAP_RAM);
	ZetSetInHandler(aerofgtZ80PortRead);
	ZetSetOutHandler(aerofgtZ80PortWrite);
	ZetClose();
	
	BurnYM2610Init(8000000, RomSnd2, &RomSndSize2, RomSnd1, &RomSndSize1, &aerofgtFMIRQHandler, aerofgtSynchroniseStream, aerofgtGetTime, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);
}

static void turbofrc_sound_init()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(RomZ80, 0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(RamZ80, 0x7800, 0x7fff, MAP_RAM);
	ZetSetInHandler(turbofrcZ80PortRead);
	ZetSetOutHandler(turbofrcZ80PortWrite);
	ZetClose();

	BurnYM2610Init(8000000, RomSnd2, &RomSndSize2, RomSnd1, &RomSndSize1, &aerofgtFMIRQHandler, aerofgtSynchroniseStream, aerofgtGetTime, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);
}

static INT32 aerofgtInit()
{
	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)malloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);
	MemIndex();	
	
	// Load the roms into memory
	
	// Load 68000 ROM
	if (BurnLoadRom(Rom01, 0, 1)) {
		return 1;
	}
	
	// Load Graphic
	BurnLoadRom(RomBg,   1, 1);
	BurnLoadRom(RomBg+0x80000,  2, 1);
	DecodeBg();
	
	BurnLoadRom(RomSpr1+0x000000, 3, 1);
	BurnLoadRom(RomSpr1+0x100000, 4, 1);
	DecodeSpr(DeRomSpr1, RomSpr1, 8192+4096);
	
	// Load Z80 ROM
	if (BurnLoadRom(RomZ80+0x10000, 5, 1)) return 1;
	memcpy(RomZ80, RomZ80+0x10000, 0x10000);

	BurnLoadRom(RomSnd1,  6, 1);
	BurnLoadRom(RomSnd2,  7, 1);


	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(RamPal,			0x1A0000, 0x1A07FF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamRaster,	0x1B0000, 0x1B0FFF, MAP_RAM);	// Raster / MRA_NOP / MRA_BANK7
		SekMapMemory((UINT8 *)RamBg1V,		0x1B2000, 0x1B3FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamBg2V,		0x1B4000, 0x1B5FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamSpr1,		0x1C0000, 0x1C7FFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr2,		0x1D0000, 0x1D1FFF, MAP_RAM);
		SekMapMemory(Ram01,			0xFEF000, 0xFFEFFF, MAP_RAM);	// 64K Work RAM
//		SekSetReadWordHandler(0, aerofgtReadWord);
		SekSetReadByteHandler(0, aerofgtReadByte);
		SekSetWriteWordHandler(0, aerofgtWriteWord);
		SekSetWriteByteHandler(0, aerofgtWriteByte);
		SekClose();
	}

	aerofgt_sound_init();

	pAssembleInputs = aerofgtAssembleInputs;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 turbofrcInit()
{
	Mem = NULL;
	turbofrcMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	turbofrcMemIndex();	
	
	// Load 68000 ROM
	if (BurnLoadRom(Rom01+0x00000, 0, 1)) return 1;
	if (BurnLoadRom(Rom01+0x40000, 1, 1)) return 1;
	if (BurnLoadRom(Rom01+0x80000, 2, 1)) return 1;
	
	// Load Graphic
	BurnLoadRom(RomBg+0x000000, 3, 1);
	BurnLoadRom(RomBg+0x080000, 4, 1);
	BurnLoadRom(RomBg+0x0A0000, 5, 1);
	BurnLoadRom(RomBg+0x120000, 6, 1);
	pspikesDecodeBg(0x14000);
		
	BurnLoadRom(RomSpr1+0x000000,  7, 2);
	BurnLoadRom(RomSpr1+0x000001,  9, 2);
	BurnLoadRom(RomSpr1+0x100000,  8, 2);
	BurnLoadRom(RomSpr1+0x100001, 10, 2);
	BurnLoadRom(RomSpr1+0x200000, 11, 2);
	BurnLoadRom(RomSpr1+0x200001, 12, 2);
	pspikesDecodeSpr(DeRomSpr1, RomSpr1, 0x6000);
	
	// Load Z80 ROM
	if (BurnLoadRom(RomZ80+0x10000, 13, 1)) return 1;
	memcpy(RomZ80, RomZ80+0x10000, 0x10000);

	BurnLoadRom(RomSnd1, 14, 1);
	BurnLoadRom(RomSnd2, 15, 1);
	
	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x0BFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,			0x0C0000, 0x0CFFFF, MAP_RAM);	// 64K Work RAM
		SekMapMemory((UINT8 *)RamBg1V,		0x0D0000, 0x0D1FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamBg2V,		0x0D2000, 0x0D3FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamSpr1,		0x0E0000, 0x0E3FFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr2,		0x0E4000, 0x0E7FFF, MAP_RAM);
		SekMapMemory(Ram01+0x10000,		0x0F8000, 0x0FBFFF, MAP_RAM);	// Work RAM
		SekMapMemory(Ram01+0x10000,		0xFF8000, 0xFFBFFF, MAP_RAM);	// Work RAM
		SekMapMemory((UINT8 *)RamSpr3,		0x0FC000, 0x0FC7FF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr3,		0xFFC000, 0xFFC7FF, MAP_RAM);
		SekMapMemory((UINT8 *)RamRaster,	0x0FD000, 0x0FDFFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamRaster,	0xFFD000, 0xFFDFFF, MAP_RAM);
		SekMapMemory(RamPal,			0x0FE000, 0x0FE7FF, MAP_ROM);	// Palette
//		SekSetReadWordHandler(0, turbofrcReadWord);
		SekSetReadByteHandler(0, turbofrcReadByte);
		SekSetWriteWordHandler(0, turbofrcWriteWord);
		SekSetWriteByteHandler(0, turbofrcWriteByte);
		SekClose();
	}
	
	turbofrc_sound_init();

	pAssembleInputs = turbofrcAssembleInputs;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 karatblzInit()
{
	Mem = NULL;
	karatblzMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	karatblzMemIndex();	
	
	// Load 68000 ROM
	if (BurnLoadRom(Rom01+0x00000, 0, 1)) return 1;
	if (BurnLoadRom(Rom01+0x40000, 1, 1)) return 1;
	
	// Load Graphic
	BurnLoadRom(RomBg+0x00000, 2, 1);
	BurnLoadRom(RomBg+0x80000, 3, 1);
	pspikesDecodeBg(0x10000);
	
	BurnLoadRom(RomSpr1+0x000000, 4, 2);
	BurnLoadRom(RomSpr1+0x000001, 6, 2);
	BurnLoadRom(RomSpr1+0x200000, 5, 2);
	BurnLoadRom(RomSpr1+0x200001, 7, 2);
	BurnLoadRom(RomSpr1+0x400000, 8, 2);
	BurnLoadRom(RomSpr1+0x400001, 9, 2);
	pspikesDecodeSpr(DeRomSpr1, RomSpr1, 0xA000);
	
	// Load Z80 ROM
	if (BurnLoadRom(RomZ80+0x10000, 10, 1)) return 1;
	memcpy(RomZ80, RomZ80+0x10000, 0x10000);

	BurnLoadRom(RomSnd1, 11, 1);
	BurnLoadRom(RomSnd2, 12, 1);
	
	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory((UINT8 *)RamBg1V,		0x080000, 0x081FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamBg2V,		0x082000, 0x083FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamSpr1,		0x0A0000, 0x0AFFFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr2,		0x0B0000, 0x0BFFFF, MAP_RAM);
		SekMapMemory(Ram01,			0x0C0000, 0x0CFFFF, MAP_RAM);	// 64K Work RAM
		SekMapMemory(Ram01+0x10000,		0x0F8000, 0x0FBFFF, MAP_RAM);	// Work RAM
		SekMapMemory(Ram01+0x10000,		0xFF8000, 0xFFBFFF, MAP_RAM);	// Work RAM
		SekMapMemory((UINT8 *)RamSpr3,		0x0FC000, 0x0FC7FF, MAP_RAM);
		SekMapMemory(RamPal,			0x0FE000, 0x0FE7FF, MAP_ROM);	// Palette
//		SekSetReadWordHandler(0, karatblzReadWord);
		SekSetReadByteHandler(0, karatblzReadByte);
		SekSetWriteWordHandler(0, karatblzWriteWord);
		SekSetWriteByteHandler(0, karatblzWriteByte);
		SekClose();
	}

	turbofrc_sound_init();

	pAssembleInputs = karatblzAssembleInputs;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 spinlbrkInit()
{
	Mem = NULL;
	spinlbrkMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	spinlbrkMemIndex();	
	
	// Load 68000 ROM
	if (BurnLoadRom(Rom01+0x00001, 0, 2)) return 1;
	if (BurnLoadRom(Rom01+0x00000, 1, 2)) return 1;
	if (BurnLoadRom(Rom01+0x20001, 2, 2)) return 1;
	if (BurnLoadRom(Rom01+0x20000, 3, 2)) return 1;
	
	// Load Graphic
	BurnLoadRom(RomBg+0x000000, 4, 1);
	BurnLoadRom(RomBg+0x080000, 5, 1);
	BurnLoadRom(RomBg+0x100000, 6, 1);
	BurnLoadRom(RomBg+0x180000, 7, 1);
	BurnLoadRom(RomBg+0x200000, 8, 1);
	pspikesDecodeBg(0x14000);
	
	BurnLoadRom(RomSpr1+0x000000,  9, 2);
	BurnLoadRom(RomSpr1+0x000001, 10, 2);
	BurnLoadRom(RomSpr1+0x100000, 11, 2);
	BurnLoadRom(RomSpr1+0x100001, 13, 2);
	BurnLoadRom(RomSpr1+0x200000, 12, 2);
	BurnLoadRom(RomSpr1+0x200001, 14, 2);
	pspikesDecodeSpr(DeRomSpr1, RomSpr1, 0x6000);
	
	BurnLoadRom((UINT8 *)RamSpr2+0x000001, 15, 2);
	BurnLoadRom((UINT8 *)RamSpr2+0x000000, 16, 2);
	
	// Load Z80 ROM
	if (BurnLoadRom(RomZ80+0x00000, 17, 1)) return 1;
	if (BurnLoadRom(RomZ80+0x10000, 18, 1)) return 1;
	
	BurnLoadRom(RomSnd2+0x00000, 19, 1);
	BurnLoadRom(RomSnd2+0x80000, 20, 1);
	
	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x04FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory((UINT8 *)RamBg1V,		0x080000, 0x080FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamBg2V,		0x082000, 0x083FFF, MAP_RAM);
		SekMapMemory(Ram01,			0xFF8000, 0xFFBFFF, MAP_RAM);	// Work RAM
		SekMapMemory((UINT8 *)RamSpr3,		0xFFC000, 0xFFC7FF, MAP_RAM);
		SekMapMemory((UINT8 *)RamRaster,	0xFFD000, 0xFFD1FF, MAP_RAM);
		SekMapMemory(RamPal,			0xFFE000, 0xFFE7FF, MAP_ROM);	// Palette
		SekSetReadWordHandler(0, spinlbrkReadWord);
//		SekSetReadByteHandler(0, spinlbrkReadByte);
		SekSetWriteWordHandler(0, spinlbrkWriteWord);
		SekSetWriteByteHandler(0, spinlbrkWriteByte);
		SekClose();
	}
	
	turbofrc_sound_init();

	pAssembleInputs = aerofgtAssembleInputs;

	// Fix sprite glitches...
	for (unsigned short i=0; i<0x2000;i++)
		RamSpr1[i] = i;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 aerofgtbInit()
{
	Mem = NULL;
	aerofgtbMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);
	aerofgtbMemIndex();	

	if (BurnLoadRom(Rom01 + 0x00001, 0, 2)) return 1;
	if (BurnLoadRom(Rom01 + 0x00000, 1, 2)) return 1;
	
	BurnLoadRom(RomBg+0x00000,  2, 1);
	BurnLoadRom(RomBg+0x80000,  3, 1);
	pspikesDecodeBg(0x8000);
		
	BurnLoadRom(RomSpr1+0x000000, 4, 2);
	BurnLoadRom(RomSpr1+0x000001, 5, 2);
	BurnLoadRom(RomSpr1+0x100000, 6, 2);
	BurnLoadRom(RomSpr1+0x100001, 7, 2);
	aerofgtbDecodeSpr(DeRomSpr1, RomSpr1, 0x3000);
	
	if (BurnLoadRom(RomZ80+0x10000, 8, 1)) return 1;
	memcpy(RomZ80, RomZ80+0x10000, 0x10000);

	BurnLoadRom(RomSnd1,  9, 1);
	BurnLoadRom(RomSnd2, 10, 1);

	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,			0x0C0000, 0x0CFFFF, MAP_RAM);	// 64K Work RAM
		SekMapMemory((UINT8 *)RamBg1V,		0x0D0000, 0x0D1FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamBg2V,		0x0D2000, 0x0D3FFF, MAP_RAM);	
		SekMapMemory((UINT8 *)RamSpr1,		0x0E0000, 0x0E3FFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr2,		0x0E4000, 0x0E7FFF, MAP_RAM);
		SekMapMemory(Ram01+0x10000,		0x0F8000, 0x0FBFFF, MAP_RAM);	// Work RAM
		SekMapMemory((UINT8 *)RamSpr3,		0x0FC000, 0x0FC7FF, MAP_RAM);
		SekMapMemory(RamPal,			0x0FD000, 0x0FD7FF, MAP_ROM);	// Palette
		SekMapMemory((UINT8 *)RamRaster,	0x0FF000, 0x0FFFFF, MAP_RAM);	// Raster 
		SekSetReadWordHandler(0, aerofgtbReadWord);
		SekSetReadByteHandler(0, aerofgtbReadByte);
		SekSetWriteWordHandler(0, aerofgtbWriteWord);
		SekSetWriteByteHandler(0, aerofgtbWriteByte);
		SekClose();
	}

	aerofgt_sound_init();

	pAssembleInputs = aerofgtAssembleInputs;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 pspikesInit()
{
	Mem = NULL;
	pspikesMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);
	pspikesMemIndex();	

	if (BurnLoadRom(Rom01 + 0x00000, 0, 1)) return 1;

	if (BurnLoadRom(RomZ80+0x10000, 1, 1)) return 1;
	memcpy(RomZ80, RomZ80+0x10000, 0x10000);

	BurnLoadRom(RomBg+0x00000,  2, 1);
	pspikesDecodeBg(0x4000);
		
	BurnLoadRom(RomSpr1+0x000000, 3, 2);
	BurnLoadRom(RomSpr1+0x000001, 4, 2);
	pspikesDecodeSpr(DeRomSpr1, RomSpr1, 0x2000);

	BurnLoadRom(RomSnd1,  5, 1);
	BurnLoadRom(RomSnd2,  6, 1);

	{
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Rom01,			0x000000, 0x03FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,			0x100000, 0x10FFFF, MAP_RAM);	// 64K Work RAM
		SekMapMemory((UINT8 *)RamSpr1,		0x200000, 0x203FFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamBg1V,		0xFF8000, 0xFF8FFF, MAP_RAM);
		SekMapMemory((UINT8 *)RamSpr3,		0xFFC000, 0xFFC7FF, MAP_RAM);
		SekMapMemory((UINT8 *)RamRaster,	0xFFD000, 0xFFDFFF, MAP_RAM);	// Raster 
		SekMapMemory(RamPal,			0xFFE000, 0xFFEFFF, MAP_ROM);	// Palette
	//	SekSetReadWordHandler(0, pspikesReadWord);
		SekSetReadByteHandler(0, pspikesReadByte);
		SekSetWriteWordHandler(0, pspikesWriteWord);
		SekSetWriteByteHandler(0, pspikesWriteByte);
		SekClose();
	}

	turbofrc_sound_init();

	pAssembleInputs = turbofrcAssembleInputs;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2610Exit();
	
	ZetExit();
	SekExit();
	
	BurnFree(Mem);
	
	return 0;
}



static void aerofgt_drawsprites(INT32 priority)
{
	priority <<= 12;
	INT32 offs = 0;
	
	while (offs < 0x0400 && (BURN_ENDIAN_SWAP_INT16(RamSpr2[offs]) & 0x8000) == 0) {
		INT32 attr_start = (BURN_ENDIAN_SWAP_INT16(RamSpr2[offs]) & 0x03ff) * 4;

		if ((BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 2]) & 0x3000) == priority) {
			INT32 map_start;
			INT32 ox,oy,x,y,xsize,ysize,zoomx,zoomy,flipx,flipy,color;

			ox = BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 1]) & 0x01ff;
			xsize = (BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 1]) & 0x0e00) >> 9;
			zoomx = (BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 1]) & 0xf000) >> 12;
			oy = BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 0]) & 0x01ff;
			ysize = (BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 0]) & 0x0e00) >> 9;
			zoomy = (BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 0]) & 0xf000) >> 12;
			flipx = BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 2]) & 0x4000;
			flipy = BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 2]) & 0x8000;
			color = (BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 2]) & 0x0f00) >> 4;
			map_start = BURN_ENDIAN_SWAP_INT16(RamSpr2[attr_start + 3]) & 0x3fff;
			INT32 bank = map_start & 0x2000;

			ox += (xsize*zoomx+2)/4;
			oy += (ysize*zoomy+2)/4;
			
			zoomx = 32 - zoomx;
			zoomy = 32 - zoomy;

			UINT8 *gfxbase;
			
			if (bank) {
				gfxbase = DeRomSpr2;
				color += 768;
			} else {
				gfxbase = DeRomSpr1;
				color += 512;
			}

			for (y = 0;y <= ysize;y++) {
				INT32 sx,sy;

				if (flipy) sy = ((oy + zoomy * (ysize - y)/2 + 16) & 0x1ff) - 16;
				else sy = ((oy + zoomy * y / 2 + 16) & 0x1ff) - 16;

				for (x = 0;x <= xsize;x++) {
					if (flipx) sx = ((ox + zoomx * (xsize - x) / 2 + 16) & 0x1ff) - 16;
					else sx = ((ox + zoomx * x / 2 + 16) & 0x1ff) - 16;

					INT32 code = BURN_ENDIAN_SWAP_INT16(RamSpr1[map_start]) & 0x1fff;

					RenderZoomedTile(pTransDraw, gfxbase, code, color, 0xf, sx, sy, flipx, flipy, 16, 16, zoomx<<11, zoomy<<11);

					map_start++;
				}
			}
		}
		offs++;
	}
}

void RenderZoomedTilePrio(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *pri, INT32 prio, INT32 /*turbofrc_layer*/)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight) 
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16];
						
						// new!
						if ((prio & (1 << pri[y * nScreenWidth + x])) == 0/* && pri[y * nScreenWidth + x] < 0x80 */) {
							if (pxl != t) {
								dst[x] = pxl + color;
							}
						}
						// !new
					}
	
					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

static void turbofrc_drawsprites(INT32 chip, INT32 turbofrc_layer, INT32 paloffset, INT32 chip_disabled_pri)
{
	INT32 attr_start,base,first;

	base = chip * 0x0200;
	first = 4 * BURN_ENDIAN_SWAP_INT16(RamSpr3[0x1fe + base]);

	//for (attr_start = base + 0x0200-8;attr_start >= first + base;attr_start -= 4) {
	for (attr_start = first + base; attr_start <= base + 0x0200-8; attr_start += 4) {
		INT32 map_start;
		INT32 ox,oy,x,y,xsize,ysize,zoomx,zoomy,flipx,flipy,color,pri;

		if (!(BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x0080)) continue;
		pri = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x0010;
		if ( chip_disabled_pri & !pri) continue;
		if ((!chip_disabled_pri) & (pri>>4)) continue;
		ox = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 1]) & 0x01ff;
		xsize = (BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x0700) >> 8;
		zoomx = (BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 1]) & 0xf000) >> 12;
		oy = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 0]) & 0x01ff;
		ysize = (BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x7000) >> 12;
		zoomy = (BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 0]) & 0xf000) >> 12;
		flipx = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x0800;
		flipy = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x8000;
		color = ((BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 2]) & 0x000f) + (spritepalettebank*16))  * 16;
		color += paloffset;

		map_start = BURN_ENDIAN_SWAP_INT16(RamSpr3[attr_start + 3]);

		UINT8 *gfxbase;
		if (chip) {
			gfxbase = DeRomSpr2;
		} else {
			gfxbase = DeRomSpr1;
		}

		zoomx = 32 - zoomx;
		zoomy = 32 - zoomy;

		for (y = 0;y <= ysize;y++) {
			INT32 sx,sy;

			if (flipy) sy = ((oy + zoomy * (ysize - y)/2 + 16) & 0x1ff) - 16;
			else sy = ((oy + zoomy * y / 2 + 16) & 0x1ff) - 16;

			for (x = 0;x <= xsize;x++) {
				INT32 code;

				if (flipx) sx = ((ox + zoomx * (xsize - x) / 2 + 16) & 0x1ff) - 16 - 8;
				else sx = ((ox + zoomx * x / 2 + 16) & 0x1ff) - 16 - 8;

				if (chip == 0)	code = BURN_ENDIAN_SWAP_INT16(RamSpr1[map_start & RamSpr1SizeMask]) & RomSpr1SizeMask;
				else			code = BURN_ENDIAN_SWAP_INT16(RamSpr2[map_start & RamSpr2SizeMask]) & RomSpr2SizeMask;

				if (turbofrc_layer)
					RenderZoomedTilePrio(pTransDraw, gfxbase, code, color, 0xf, sx, sy, flipx, flipy, 16, 16, zoomx<<11, zoomy<<11, RamPrioBitmap, (pri) ? 0 : 2, turbofrc_layer);
				else
					RenderZoomedTile(pTransDraw, gfxbase, code, color, 0xf, sx, sy, flipx, flipy, 16, 16, zoomx<<11, zoomy<<11);


				map_start++;
			}

			if (xsize == 2) map_start += 1;
			if (xsize == 4) map_start += 3;
			if (xsize == 5) map_start += 2;
			if (xsize == 6) map_start += 1;
		}
	}
}

static void RenderTilePrio(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *pribmp, UINT8 prio)
{
	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip];
			if (pxl == trans_col) continue;

			pribmp[sy * nScreenWidth + sx] = prio;
			dest[sy * nScreenWidth + sx] = pxl | color;
		}

		sx -= width;
	}
}

static void TileBackgroundPrio(UINT16 *bgram, UINT8 *gfx, INT32 transp, UINT16 pal, INT32 scrollx, INT32 scrolly, UINT8 *bankbase)
{
	scrollx &= 0x1ff;
	scrolly &= 0x1ff;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = BURN_ENDIAN_SWAP_INT16(bgram[offs]);
		INT32 code  = (attr & 0x07FF) + ((bankbase[((attr & 0x1800) >> 11)] & 0xf) << 11);
 		INT32 color = attr >> 13;

		if (transp) { // only this layer does prio
			RenderTilePrio(pTransDraw, gfx, code, (color << 4) | pal, 0x0f, sx, sy, 0, 0, 8, 8, RamPrioBitmap, 1);
		} else {
			if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
				Render8x8Tile(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			}
		}
	}
}

static void TileBackground(UINT16 *bgram, UINT8 *gfx, INT32 transp, UINT16 pal, INT32 scrollx, INT32 scrolly, UINT8 *bankbase)
{
	scrollx &= 0x1ff;
	scrolly &= 0x1ff;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = BURN_ENDIAN_SWAP_INT16(bgram[offs]);
		INT32 code  = (attr & 0x07FF) + ((bankbase[((attr & 0x1800) >> 11)] & 0xf) << 11);
 		INT32 color = attr >> 13;

		if (transp) {
			if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
				Render8x8Tile_Mask(pTransDraw, code, sx, sy, color, 4, 0x0f, pal, gfx);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, pal, gfx);
			}
		} else {
			if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
				Render8x8Tile(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			}
		}
	}
}

static void karatblzTileBackground(UINT16 *bgram, UINT8 *gfx, INT32 transp, UINT16 pal, INT32 scrollx, INT32 scrolly, UINT8 bankbase)
{
	scrollx &= 0x1ff;
	scrolly &= 0x1ff;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = BURN_ENDIAN_SWAP_INT16(bgram[offs]);
		INT32 code  = (attr & 0x1fff) + (bankbase << 13);
 		INT32 color = attr >> 13;

		if (transp) {
			if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
				Render8x8Tile_Mask(pTransDraw, code, sx, sy, color, 4, 0x0f, pal, gfx);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, pal, gfx);
			}
		} else {
			if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
				Render8x8Tile(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, pal, gfx);
			}
		}
	}
}

static void spinlbrkTileBackground()
{
	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= BURN_ENDIAN_SWAP_INT16(RamRaster[sy]) & 0x1ff;
		if (sx < -7) sx += 512;

		INT32 attr = BURN_ENDIAN_SWAP_INT16(RamBg1V[offs]);
		INT32 code = (attr & 0x0fff) + ((RamGfxBank[0] & 0x0f) << 12);
 		INT32 color = attr >> 12;

		if (sx >= nScreenWidth) continue;
		if (sy >= nScreenHeight) break;

		if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
			Render8x8Tile(pTransDraw, code, sx, sy, color, 4, 0, DeRomBg);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DeRomBg);
		}
	}
}

static void pspikesTileBackground()
{
	INT32 scrolly = bg1scrolly + 2;

	for (INT32 sy = 0; sy < nScreenHeight; sy++)
	{
		UINT16 *dst = pTransDraw + sy * nScreenWidth;

		for (INT32 sx = 0; sx < nScreenWidth + 8; sx++)
		{
			INT32 yy = (sy + scrolly) & 0xff;
			INT32 xx = (sx + BURN_ENDIAN_SWAP_INT16(RamRaster[yy])) & 0x1ff;

			INT32 offs = ((yy/8)*64) + (xx/8);

			INT32 attr  = BURN_ENDIAN_SWAP_INT16(RamBg1V[offs]);
			INT32 code  = (attr & 0x0FFF) + ((RamGfxBank[((attr & 0x1000) >> 12)] & 0xf) << 12);
	 		INT32 color = (((attr >> 13) + (charpalettebank * 8)) * 16);
			
			{
				xx = sx - (xx & 7);
				UINT8 *rom = DeRomBg + (code * 0x40) + ((yy&7) * 8);

				for (INT32 x = 0; x < 8; x++, xx++) {
					if (xx >= 0 && xx < nScreenWidth) {
						dst[xx] = rom[x] + color;
					}
				}
			}
		}
	}
}

static inline void DrvRecalcPalette()
{
	UINT16* ps = (UINT16*) RamPal;
	UINT32* pd = RamCurPal;
	for (INT32 i=0; i<BurnDrvGetPaletteEntries(); i++, ps++, pd++)
		*pd = CalcCol(*ps);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	INT32 scrollx0 = BURN_ENDIAN_SWAP_INT16(RamRaster[0x0000]) - 18;
	INT32 scrollx1 = BURN_ENDIAN_SWAP_INT16(RamRaster[0x0200]) - 20;

	BurnTransferClear();

 	if (nBurnLayer & 1) TileBackground(RamBg1V, DeRomBg, 0, 0x000, scrollx0, bg1scrolly, RamGfxBank + 0);
 	if (nSpriteEnable & 1) aerofgt_drawsprites(0);
	if (nSpriteEnable & 2) aerofgt_drawsprites(1);
 	if (nBurnLayer & 2) TileBackground(RamBg2V, DeRomBg, 1, 0x100, scrollx1, bg2scrolly, RamGfxBank + 4);
	if (nSpriteEnable & 4) aerofgt_drawsprites(2);
	if (nSpriteEnable & 8) aerofgt_drawsprites(3);

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 turbofrcDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	INT32 scrollx0 = BURN_ENDIAN_SWAP_INT16(RamRaster[7]) - 11;
	INT32 scrollx1 = bg2scrollx;

	if (nBurnLayer & 1) TileBackground(RamBg1V, DeRomBg + 0x000000, 0, 0x000, scrollx0, bg1scrolly, RamGfxBank + 0);
	memset(RamPrioBitmap, 0, 352 * 240); // clear priority
	if (nBurnLayer & 2) TileBackgroundPrio(RamBg2V, DeRomBg + 0x140000, 1, 0x100, scrollx1, bg2scrolly, RamGfxBank + 4);

	// sprite priority-bitmap is only used between the first 2 calls to turbofrc_drawsprites()
	// it probably could have been implemented better, but it works. -dink
	if (nBurnLayer & 4) turbofrc_drawsprites(0, 2, 512,  0); // big alien (level 3)
 	if (nBurnLayer & 8) turbofrc_drawsprites(0, 0, 512, -1); // enemies
	if (nSpriteEnable & 1) turbofrc_drawsprites(1, 0, 768,  0); // nothing?
	if (nSpriteEnable & 2) turbofrc_drawsprites(1, 0, 768, -1); // player

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 karatblzDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	karatblzTileBackground(RamBg1V, DeRomBg + 0x000000, 0, 0x000, bg1scrollx, bg1scrolly, RamGfxBank[0] & 1);
	karatblzTileBackground(RamBg2V, DeRomBg + 0x100000, 1, 0x100, bg2scrollx, bg2scrolly, RamGfxBank[1] & 1);

/* 	
	turbofrc_drawsprites(1,-1); 
	turbofrc_drawsprites(1, 0); 
 	turbofrc_drawsprites(0,-1); 
	turbofrc_drawsprites(0, 0); 
*/
	turbofrc_drawsprites(0, 0, 512,  0);
 	turbofrc_drawsprites(0, 0, 512, -1); 
	turbofrc_drawsprites(1, 0, 768,  0); 
	turbofrc_drawsprites(1, 0, 768, -1); 


	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 spinlbrkDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	spinlbrkTileBackground();
	karatblzTileBackground(RamBg2V, DeRomBg + 0x200000, 1, 0x100, bg2scrollx, bg2scrolly, RamGfxBank[1] & 0x07);

	turbofrc_drawsprites(1, 0, 768, -1);	// enemy(near far)
	turbofrc_drawsprites(1, 0, 768,  0);	// enemy(near) fense
 	turbofrc_drawsprites(0, 0, 512,  0); // avatar , post , bullet
	turbofrc_drawsprites(0, 0, 512, -1);

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 aerofgtbDraw()
{
	INT32 scrollx0 = BURN_ENDIAN_SWAP_INT16(RamRaster[7]);
	INT32 scrollx1 = bg2scrollx + 5;

	TileBackground(RamBg1V, DeRomBg + 0x000000, 0, 0x000, scrollx0, bg1scrolly + 2, RamGfxBank + 0);
	TileBackground(RamBg2V, DeRomBg + 0x100000, 1, 0x100, scrollx1, bg2scrolly + 2, RamGfxBank + 4);

	turbofrc_drawsprites(0, 0, 512,  0);
 	turbofrc_drawsprites(0, 0, 512, -1);
	turbofrc_drawsprites(1, 0, 768,  0);
	turbofrc_drawsprites(1, 0, 768, -1);

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 pspikesDraw()
{
	pspikesTileBackground();

	turbofrc_drawsprites(0, 0, 1024,  0);
 	turbofrc_drawsprites(0, 0, 1024, -1);

	BurnTransferCopy(RamCurPal);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (pAssembleInputs) {
		pAssembleInputs();
	}

	SekNewFrame();
	ZetNewFrame();
	
	nCyclesTotal[0] = 10000000 / 60;
	nCyclesTotal[1] = 5000000  / 60;
	
	SekOpen(0);
	ZetOpen(0);
	
	SekRun(nCyclesTotal[0]);
	SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}
	
	return 0;
}


static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029671;
	}

	if (nAction & ACB_MEMORY_ROM) {
		// 
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
    		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
		
		if (nAction & ACB_WRITE) {
			// update palette while loaded
			DrvRecalc = 1;
		}
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);
		ZetScan(nAction);

		SCAN_VAR(RamGfxBank);
		SCAN_VAR(DrvInput);

		ZetOpen(0);
		BurnYM2610Scan(nAction, pnMin);
		ZetClose();
		SCAN_VAR(nSoundlatch);
		SCAN_VAR(nAerofgtZ80Bank);

		SCAN_VAR(spritepalettebank);
		SCAN_VAR(charpalettebank);

		if (nAction & ACB_WRITE) {
			INT32 nBank = nAerofgtZ80Bank;
                        nAerofgtZ80Bank = -1;
                        ZetOpen(0);
                        aerofgtSndBankSwitch(nBank);
                        ZetClose();
		}
	}

	return 0;
}


// Aero Fighters

static struct BurnRomInfo aerofgtRomDesc[] = {
	{ "1.u4",    	  0x080000, 0x6fdff0a2, BRF_ESS | BRF_PRG }, // 68000 code swapped

	{ "538a54.124",   0x080000, 0x4d2c4df2, BRF_GRA },			 // graphics
	{ "1538a54.124",  0x080000, 0x286d109e, BRF_GRA },
	
	{ "538a53.u9",    0x100000, 0x630d8e0b, BRF_GRA },			 // 
	{ "534g8f.u18",   0x080000, 0x76ce0926, BRF_GRA },

	{ "2.153",    	  0x020000, 0xa1ef64ec, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "it-19-01",     0x040000, 0x6d42723d, BRF_SND },			 // samples
	{ "it-19-06",     0x100000, 0xcdbbdb1d, BRF_SND },	
};

STD_ROM_PICK(aerofgt)
STD_ROM_FN(aerofgt)


struct BurnDriver BurnDrvAerofgt = {
	"aerofgt", NULL, NULL, NULL, "1992",
	"Aero Fighters\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, FBF_SONICWI,
	NULL, aerofgtRomInfo, aerofgtRomName, NULL, NULL, aerofgtInputInfo, aerofgtDIPInfo,
	aerofgtInit,DrvExit,DrvFrame,DrvDraw,DrvScan,&DrvRecalc,0x400,
	224,320,3,4
};


// Turbo Force (World)
// World version with no copyright notice

static struct BurnRomInfo turbofrcRomDesc[] = {
	{ "4v2.subpcb.u2",	0x040000, 0x721300ee, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "4v1.subpcb.u1",  0x040000, 0x6cd5312b, BRF_ESS | BRF_PRG },
	{ "4v3.u14",    	0x040000, 0x63f50557, BRF_ESS | BRF_PRG },
	
	{ "lh534ggs.u94",  	0x080000, 0xbaa53978, BRF_GRA },		   // gfx1
	{ "7.u95",  	  	0x020000, 0x71a6c573, BRF_GRA },
	
	{ "lh534ggy.u105", 	0x080000, 0x4de4e59e, BRF_GRA },		   // gfx2
	{ "8.u106", 	  	0x020000, 0xc6479eb5, BRF_GRA },
	
	{ "lh534gh2.u116", 	0x080000, 0xdf210f3b, BRF_GRA },		   // gfx3
	{ "5.u118", 	  	0x040000, 0xf61d1d79, BRF_GRA },
	{ "lh534gh1.u117", 	0x080000, 0xf70812fd, BRF_GRA },
	{ "4.u119", 	  	0x040000, 0x474ea716, BRF_GRA },

	{ "lh532a52.u134", 	0x040000, 0x3c725a48, BRF_GRA },		   // gfx4
	{ "lh532a51.u135", 	0x040000, 0x95c63559, BRF_GRA },

	{ "6.u166", 	  	0x020000, 0x2ca14a65, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "lh532h74.u180", 	0x040000, 0xa3d43254, BRF_SND },		   // samples
	{ "lh538o7j.u179", 	0x100000, 0x60ca0333, BRF_SND },	
};

STD_ROM_PICK(turbofrc)
STD_ROM_FN(turbofrc)

struct BurnDriver BurnDrvTurbofrc = {
	"turbofrc", NULL, NULL, NULL, "1991",
	"Turbo Force (World)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcRomInfo, turbofrcRomName, NULL, NULL, turbofrcInputInfo, turbofrcDIPInfo,
	turbofrcInit,DrvExit,DrvFrame,turbofrcDraw,DrvScan,&DrvRecalc,0x400,
	240,352,3,4
};


// Turbo Force (US)

static struct BurnRomInfo turbofrcuRomDesc[] = {
	{ "8v2.subpcb.u2",  0x040000, 0x721300ee, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "8v1.subpcb.u1",  0x040000, 0xcc324da6, BRF_ESS | BRF_PRG },
	{ "8v3.u14",    	0x040000, 0xc0a15480, BRF_ESS | BRF_PRG },

	{ "lh534ggs.u94",  	0x080000, 0xbaa53978, BRF_GRA },		   // gfx1
	{ "7.u95",  	  	0x020000, 0x71a6c573, BRF_GRA },
	
	{ "lh534ggy.u105", 	0x080000, 0x4de4e59e, BRF_GRA },		   // gfx2
	{ "8.u106", 	  	0x020000, 0xc6479eb5, BRF_GRA },
	
	{ "lh534gh2.u116", 	0x080000, 0xdf210f3b, BRF_GRA },		   // gfx3
	{ "5.u118", 	  	0x040000, 0xf61d1d79, BRF_GRA },
	{ "lh534gh1.u117", 	0x080000, 0xf70812fd, BRF_GRA },
	{ "4.u119", 	  	0x040000, 0x474ea716, BRF_GRA },

	{ "lh532a52.u134", 	0x040000, 0x3c725a48, BRF_GRA },		   // gfx4
	{ "lh532a51.u135", 	0x040000, 0x95c63559, BRF_GRA },

	{ "6.u166", 	  	0x020000, 0x2ca14a65, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "lh532h74.u180", 	0x040000, 0xa3d43254, BRF_SND },		   // samples
	{ "lh538o7j.u179", 	0x100000, 0x60ca0333, BRF_SND },	
};

STD_ROM_PICK(turbofrcu)
STD_ROM_FN(turbofrcu)

struct BurnDriver BurnDrvTurbofrcu = {
	"turbofrcu", "turbofrc", NULL, NULL, "1991",
	"Turbo Force (US)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcuRomInfo, turbofrcuRomName, NULL, NULL, turbofrcInputInfo, turbofrcDIPInfo,
	turbofrcInit,DrvExit,DrvFrame,turbofrcDraw,DrvScan,&DrvRecalc,0x400,
	240,352,3,4
};


// Aero Fighters (Turbo Force hardware set 1)

static struct BurnRomInfo aerofgtbRomDesc[] = {
	{ "v2",    	      0x040000, 0x5c9de9f0, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "v1",    	      0x040000, 0x89c1dcf4, BRF_ESS | BRF_PRG }, // 68000 code swapped

	{ "it-19-03",     0x080000, 0x85eba1a4, BRF_GRA },			 // graphics
	{ "it-19-02",     0x080000, 0x4f57f8ba, BRF_GRA },
	
	{ "it-19-04",     0x080000, 0x3b329c1f, BRF_GRA },			 // 
	{ "it-19-05",     0x080000, 0x02b525af, BRF_GRA },			 //
	
	{ "g27",          0x040000, 0x4d89cbc8, BRF_GRA },
	{ "g26",          0x040000, 0x8072c1d2, BRF_GRA },

	{ "v3",    	      0x020000, 0xcbb18cf4, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "it-19-01",     0x040000, 0x6d42723d, BRF_SND },			 // samples
	{ "it-19-06",     0x100000, 0xcdbbdb1d, BRF_SND },	
};

STD_ROM_PICK(aerofgtb)
STD_ROM_FN(aerofgtb)

struct BurnDriver BurnDrvAerofgtb = {
	"aerofgtb", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (Turbo Force hardware set 1)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, FBF_SONICWI,
	NULL, aerofgtbRomInfo, aerofgtbRomName, NULL, NULL, aerofgtInputInfo, aerofgtbDIPInfo,
	aerofgtbInit,DrvExit,DrvFrame,aerofgtbDraw,DrvScan,&DrvRecalc,0x400,
	224,320,3,4
};


// Aero Fighters (Turbo Force hardware set 2)

static struct BurnRomInfo aerofgtcRomDesc[] = {
	{ "v2.149",       0x040000, 0xf187aec6, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "v1.111",       0x040000, 0x9e684b19, BRF_ESS | BRF_PRG }, // 68000 code swapped

	{ "it-19-03",     0x080000, 0x85eba1a4, BRF_GRA },			 // graphics
	{ "it-19-02",     0x080000, 0x4f57f8ba, BRF_GRA },
	
	{ "it-19-04",     0x080000, 0x3b329c1f, BRF_GRA },			 // 
	{ "it-19-05",     0x080000, 0x02b525af, BRF_GRA },			 //
	
	{ "g27",          0x040000, 0x4d89cbc8, BRF_GRA },
	{ "g26",          0x040000, 0x8072c1d2, BRF_GRA },

	{ "2.153",        0x020000, 0xa1ef64ec, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "it-19-01",     0x040000, 0x6d42723d, BRF_SND },			 // samples
	{ "it-19-06",     0x100000, 0xcdbbdb1d, BRF_SND },	
};

STD_ROM_PICK(aerofgtc)
STD_ROM_FN(aerofgtc)

struct BurnDriver BurnDrvAerofgtc = {
	"aerofgtc", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (Turbo Force hardware set 2)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, FBF_SONICWI,
	NULL, aerofgtcRomInfo, aerofgtcRomName, NULL, NULL, aerofgtInputInfo, aerofgtDIPInfo,
	aerofgtbInit,DrvExit,DrvFrame,aerofgtbDraw,DrvScan,&DrvRecalc,0x400,
	224,320,3,4
};


// Sonic Wings (Japan)

static struct BurnRomInfo sonicwiRomDesc[] = {
	{ "2.149",    	  0x040000, 0x3d1b96ba, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "1.111",    	  0x040000, 0xa3d09f94, BRF_ESS | BRF_PRG }, // 68000 code swapped

	{ "it-19-03",     0x080000, 0x85eba1a4, BRF_GRA },			 // graphics
	{ "it-19-02",     0x080000, 0x4f57f8ba, BRF_GRA },
	
	{ "it-19-04",     0x080000, 0x3b329c1f, BRF_GRA },			 // 
	{ "it-19-05",     0x080000, 0x02b525af, BRF_GRA },			 //
	
	{ "g27",          0x040000, 0x4d89cbc8, BRF_GRA },
	{ "g26",          0x040000, 0x8072c1d2, BRF_GRA },

	{ "2.153",        0x020000, 0xa1ef64ec, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "it-19-01",     0x040000, 0x6d42723d, BRF_SND },			 // samples
	{ "it-19-06",     0x100000, 0xcdbbdb1d, BRF_SND },	
};

STD_ROM_PICK(sonicwi)
STD_ROM_FN(sonicwi)

struct BurnDriver BurnDrvSonicwi = {
	"sonicwi", "aerofgt", NULL, NULL, "1992",
	"Sonic Wings (Japan)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, FBF_SONICWI,
	NULL, sonicwiRomInfo, sonicwiRomName, NULL, NULL, aerofgtInputInfo, aerofgtDIPInfo,
	aerofgtbInit,DrvExit,DrvFrame,aerofgtbDraw,DrvScan,&DrvRecalc,0x400,
	224,320,3,4
};


// Karate Blazers (World, set 1)

static struct BurnRomInfo karatblzRomDesc[] = {
	{ "rom2v3",    	  0x040000, 0x01f772e1, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "1.u15",    	  0x040000, 0xd16ee21b, BRF_ESS | BRF_PRG },

	{ "gha.u55",   	  0x080000, 0x3e0cea91, BRF_GRA },			 // gfx1
	{ "gh9.u61",  	  0x080000, 0x5d1676bd, BRF_GRA },			 // gfx2
	
	{ "u42",          0x100000, 0x65f0da84, BRF_GRA },			 // gfx3
	{ "3.u44",        0x020000, 0x34bdead2, BRF_GRA },
	{ "u43",          0x100000, 0x7b349e5d, BRF_GRA },			
	{ "4.u45",        0x020000, 0xbe4d487d, BRF_GRA },
	
	{ "u59.ghb",      0x080000, 0x158c9cde, BRF_GRA },			 // gfx4
	{ "ghd.u60",      0x080000, 0x73180ae3, BRF_GRA },

	{ "5.u92",    	  0x020000, 0x97d67510, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "u105.gh8",     0x080000, 0x7a68cb1b, BRF_SND },			 // samples
	{ "u104",         0x100000, 0x5795e884, BRF_SND },	
};

STD_ROM_PICK(karatblz)
STD_ROM_FN(karatblz)

struct BurnDriver BurnDrvKaratblz = {
	"karatblz", NULL, NULL, NULL, "1991",
	"Karate Blazers (World, set 1)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzRomInfo, karatblzRomName, NULL, NULL, karatblzInputInfo, karatblzDIPInfo,
	karatblzInit,DrvExit,DrvFrame,karatblzDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Karate Blazers (World, set 2)

static struct BurnRomInfo karatblaRomDesc[] = {
	{ "v2.u14",    	  0x040000, 0x7a78976e, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "v1.u15",    	  0x040000, 0x47e410fe, BRF_ESS | BRF_PRG },

	{ "gha.u55",   	  0x080000, 0x3e0cea91, BRF_GRA },			 // gfx1
	{ "gh9.u61",  	  0x080000, 0x5d1676bd, BRF_GRA },			 // gfx2
	
	{ "u42",          0x100000, 0x65f0da84, BRF_GRA },			 // gfx3
	{ "3.u44",        0x020000, 0x34bdead2, BRF_GRA },
	{ "u43",          0x100000, 0x7b349e5d, BRF_GRA },			
	{ "4.u45",        0x020000, 0xbe4d487d, BRF_GRA },
	
	{ "u59.ghb",      0x080000, 0x158c9cde, BRF_GRA },			 // gfx4
	{ "ghd.u60",      0x080000, 0x73180ae3, BRF_GRA },

	{ "5.u92",    	  0x020000, 0x97d67510, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "u105.gh8",     0x080000, 0x7a68cb1b, BRF_SND },			 // samples
	{ "u104",         0x100000, 0x5795e884, BRF_SND },	
};

STD_ROM_PICK(karatbla)
STD_ROM_FN(karatbla)

struct BurnDriver BurnDrvKaratbla = {
	"karatblza", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (World, set 2)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblaRomInfo, karatblaRomName, NULL, NULL, karatblzInputInfo, karatblzDIPInfo,
	karatblzInit,DrvExit,DrvFrame,karatblzDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


static struct BurnRomInfo karatbluRomDesc[] = {
	{ "2.u14",    	  0x040000, 0x202e6220, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "1.u15",    	  0x040000, 0xd16ee21b, BRF_ESS | BRF_PRG },

	{ "gha.u55",   	  0x080000, 0x3e0cea91, BRF_GRA },			 // gfx1
	{ "gh9.u61",  	  0x080000, 0x5d1676bd, BRF_GRA },			 // gfx2
	
	{ "u42",          0x100000, 0x65f0da84, BRF_GRA },			 // gfx3
	{ "3.u44",        0x020000, 0x34bdead2, BRF_GRA },
	{ "u43",          0x100000, 0x7b349e5d, BRF_GRA },			
	{ "4.u45",        0x020000, 0xbe4d487d, BRF_GRA },
	
	{ "u59.ghb",      0x080000, 0x158c9cde, BRF_GRA },			 // gfx4
	{ "ghd.u60",      0x080000, 0x73180ae3, BRF_GRA },

	{ "5.u92",    	  0x020000, 0x97d67510, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "u105.gh8",     0x080000, 0x7a68cb1b, BRF_SND },			 // samples
	{ "u104",         0x100000, 0x5795e884, BRF_SND },	
};

STD_ROM_PICK(karatblu)
STD_ROM_FN(karatblu)

struct BurnDriver BurnDrvKaratblu = {
	"karatblzu", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (US)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatbluRomInfo, karatbluRomName, NULL, NULL, karatblzInputInfo, karatblzDIPInfo,
	karatblzInit,DrvExit,DrvFrame,karatblzDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Karate Blazers (Japan)

static struct BurnRomInfo karatbljRomDesc[] = {
	{ "2tecmo.u14",   0x040000, 0x57e52654, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "1.u15",    	  0x040000, 0xd16ee21b, BRF_ESS | BRF_PRG },

	{ "gha.u55",   	  0x080000, 0x3e0cea91, BRF_GRA },			 // gfx1
	{ "gh9.u61",  	  0x080000, 0x5d1676bd, BRF_GRA },			 // gfx2
	
	{ "u42",          0x100000, 0x65f0da84, BRF_GRA },			 // gfx3
	{ "3.u44",        0x020000, 0x34bdead2, BRF_GRA },
	{ "u43",          0x100000, 0x7b349e5d, BRF_GRA },			
	{ "4.u45",        0x020000, 0xbe4d487d, BRF_GRA },
	
	{ "u59.ghb",      0x080000, 0x158c9cde, BRF_GRA },			 // gfx4
	{ "ghd.u60",      0x080000, 0x73180ae3, BRF_GRA },

	{ "5.u92",    	  0x020000, 0x97d67510, BRF_ESS | BRF_PRG }, // Sound CPU
	
	{ "u105.gh8",     0x080000, 0x7a68cb1b, BRF_SND },			 // samples
	{ "u104",         0x100000, 0x5795e884, BRF_SND },	
};

STD_ROM_PICK(karatblj)
STD_ROM_FN(karatblj)

struct BurnDriver BurnDrvKaratblj = {
	"karatblzj", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (Japan)\0", NULL, "Video System Co.", "Video System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatbljRomInfo, karatbljRomName, NULL, NULL, karatblzInputInfo, karatblzDIPInfo,
	karatblzInit,DrvExit,DrvFrame,karatblzDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Spinal Breakers (World)

static struct BurnRomInfo spinlbrkRomDesc[] = {
	{ "ic98",    	  0x010000, 0x36c2bf70, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "ic104",    	  0x010000, 0x34a7e158, BRF_ESS | BRF_PRG },
	{ "ic93",    	  0x010000, 0x726f4683, BRF_ESS | BRF_PRG },
	{ "ic94",    	  0x010000, 0xc4385e03, BRF_ESS | BRF_PRG },

	{ "ic15",         0x080000, 0xe318cf3a, BRF_GRA },			 // gfx 1
	{ "ic9",          0x080000, 0xe071f674, BRF_GRA },

	{ "ic17",         0x080000, 0xa63d5a55, BRF_GRA },			 // gfx 2
	{ "ic11",         0x080000, 0x7dcc913d, BRF_GRA },
	{ "ic16",         0x080000, 0x0d84af7f, BRF_GRA },
	
	{ "ic12",         0x080000, 0xd63fac4e, BRF_GRA },			 // gfx 3
	{ "ic18",         0x080000, 0x5a60444b, BRF_GRA },
	
	{ "ic14",         0x080000, 0x1befd0f3, BRF_GRA },			 // gfx 4
	{ "ic20",         0x080000, 0xc2f84a61, BRF_GRA },
	{ "ic35",         0x080000, 0xeba8e1a3, BRF_GRA },
	{ "ic40",         0x080000, 0x5ef5aa7e, BRF_GRA },
	
	{ "ic19",         0x010000, 0xdb24eeaa, BRF_GRA },			 // gfx 5, hardcoded sprite maps
	{ "ic13",         0x010000, 0x97025bf4, BRF_GRA },

	{ "ic117",    	  0x008000, 0x625ada41, BRF_ESS | BRF_PRG }, // Sound CPU
	{ "ic118",    	  0x010000, 0x1025f024, BRF_ESS | BRF_PRG },
	
	{ "ic166",	      0x080000, 0x6e0d063a, BRF_SND },			 // samples
	{ "ic163",   	  0x080000, 0xe6621dfb, BRF_SND },	
	
	{ "epl16p8bp.ic100",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic127",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic133",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic99",    263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic114",    279, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic95",     279, 0x00000000, BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(spinlbrk)
STD_ROM_FN(spinlbrk)

struct BurnDriver BurnDrvSpinlbrk = {
	"spinlbrk", NULL, NULL, NULL, "1990",
	"Spinal Breakers (World)\0", NULL, "V-System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrkRomInfo, spinlbrkRomName, NULL, NULL, spinlbrkInputInfo, spinlbrkDIPInfo,
	spinlbrkInit,DrvExit,DrvFrame,spinlbrkDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Spinal Breakers (US)

static struct BurnRomInfo spinlbruRomDesc[] = {
	{ "ic98.u5",      0x010000, 0x3a0f7667, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "ic104.u6",     0x010000, 0xa0e0af31, BRF_ESS | BRF_PRG },
	{ "ic93.u4",      0x010000, 0x0cf73029, BRF_ESS | BRF_PRG },
	{ "ic94.u3",      0x010000, 0x5cf7c426, BRF_ESS | BRF_PRG },

	{ "ic15",         0x080000, 0xe318cf3a, BRF_GRA },			 // gfx 1
	{ "ic9",          0x080000, 0xe071f674, BRF_GRA },

	{ "ic17",         0x080000, 0xa63d5a55, BRF_GRA },			 // gfx 2
	{ "ic11",         0x080000, 0x7dcc913d, BRF_GRA },
	{ "ic16",         0x080000, 0x0d84af7f, BRF_GRA },
	
	{ "ic12",         0x080000, 0xd63fac4e, BRF_GRA },			 // gfx 3
	{ "ic18",         0x080000, 0x5a60444b, BRF_GRA },
	
	{ "ic14",         0x080000, 0x1befd0f3, BRF_GRA },			 // gfx 4
	{ "ic20",         0x080000, 0xc2f84a61, BRF_GRA },
	{ "ic35",         0x080000, 0xeba8e1a3, BRF_GRA },
	{ "ic40",         0x080000, 0x5ef5aa7e, BRF_GRA },
	
	{ "ic19",         0x010000, 0xdb24eeaa, BRF_GRA },			 // gfx 5, hardcoded sprite maps
	{ "ic13",         0x010000, 0x97025bf4, BRF_GRA },

	{ "ic117",    	  0x008000, 0x625ada41, BRF_ESS | BRF_PRG }, // Sound CPU
	{ "ic118",    	  0x010000, 0x1025f024, BRF_ESS | BRF_PRG },
	
	{ "ic166",	      0x080000, 0x6e0d063a, BRF_SND },			 // samples
	{ "ic163",   	  0x080000, 0xe6621dfb, BRF_SND },	
	
	{ "epl16p8bp.ic100",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic127",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic133",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic99",    263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic114",    279, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic95",     279, 0x00000000, BRF_OPT | BRF_NODUMP },		
};

STD_ROM_PICK(spinlbru)
STD_ROM_FN(spinlbru)

struct BurnDriver BurnDrvSpinlbru = {
	"spinlbrku", "spinlbrk", NULL, NULL, "1990",
	"Spinal Breakers (US)\0", NULL, "V-System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbruRomInfo, spinlbruRomName, NULL, NULL, spinlbrkInputInfo, spinlbruDIPInfo,
	spinlbrkInit,DrvExit,DrvFrame,spinlbrkDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Spinal Breakers (Japan)

static struct BurnRomInfo spinlbrjRomDesc[] = {
	{ "j5",			  0x010000, 0x6a3d690e, BRF_ESS | BRF_PRG }, // 68000 code swapped
	{ "j6",			  0x010000, 0x869593fa, BRF_ESS | BRF_PRG },
	{ "j4",			  0x010000, 0x33e33912, BRF_ESS | BRF_PRG },
	{ "j3",     	  0x010000, 0x16ca61d0, BRF_ESS | BRF_PRG },

	{ "ic15",         0x080000, 0xe318cf3a, BRF_GRA },			 // gfx 1
	{ "ic9",          0x080000, 0xe071f674, BRF_GRA },

	{ "ic17",         0x080000, 0xa63d5a55, BRF_GRA },			 // gfx 2
	{ "ic11",         0x080000, 0x7dcc913d, BRF_GRA },
	{ "ic16",         0x080000, 0x0d84af7f, BRF_GRA },
	
	{ "ic12",         0x080000, 0xd63fac4e, BRF_GRA },			 // gfx 3
	{ "ic18",         0x080000, 0x5a60444b, BRF_GRA },
	
	{ "ic14",         0x080000, 0x1befd0f3, BRF_GRA },			 // gfx 4
	{ "ic20",         0x080000, 0xc2f84a61, BRF_GRA },
	{ "ic35",         0x080000, 0xeba8e1a3, BRF_GRA },
	{ "ic40",         0x080000, 0x5ef5aa7e, BRF_GRA },
	
	{ "ic19",         0x010000, 0xdb24eeaa, BRF_GRA },			 // gfx 5, hardcoded sprite maps
	{ "ic13",         0x010000, 0x97025bf4, BRF_GRA },

	{ "ic117",    	  0x008000, 0x625ada41, BRF_ESS | BRF_PRG }, // Sound CPU
	{ "ic118",    	  0x010000, 0x1025f024, BRF_ESS | BRF_PRG },
	
	{ "ic166",	      0x080000, 0x6e0d063a, BRF_SND },			 // samples
	{ "ic163",   	  0x080000, 0xe6621dfb, BRF_SND },	
	
	{ "epl16p8bp.ic100",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic127",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic133",   263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "epl16p8bp.ic99",    263, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic114",    279, 0x00000000, BRF_OPT | BRF_NODUMP },	
	{ "gal16v8a.ic95",     279, 0x00000000, BRF_OPT | BRF_NODUMP },	
};

STD_ROM_PICK(spinlbrj)
STD_ROM_FN(spinlbrj)

struct BurnDriver BurnDrvSpinlbrj = {
	"spinlbrkj", "spinlbrk", NULL, NULL, "1990",
	"Spinal Breakers (Japan)\0", NULL, "V-System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrjRomInfo, spinlbrjRomName, NULL, NULL, spinlbrkInputInfo, spinlbrjDIPInfo,
	spinlbrkInit,DrvExit,DrvFrame,spinlbrkDraw,DrvScan,&DrvRecalc,0x400,
	352,240,4,3
};


// Power Spikes (World)

static struct BurnRomInfo pspikesRomDesc[] = {
	{ "pspikes2.bin",	0x040000, 0xec0c070e, BRF_ESS | BRF_PRG }, //  0 68000 code

	{ "19",			0x020000, 0x7e8ed6e5, BRF_ESS | BRF_PRG }, //  1 Sound CPU

	{ "g7h",		0x080000, 0x74c23c3d, BRF_GRA }, //  2 gfx 1

	{ "g7j",		0x080000, 0x0b9e4739, BRF_GRA }, //  3 gfx 2
	{ "g7l",		0x080000, 0x943139ff, BRF_GRA }, //  4

	{ "a47",		0x040000, 0xc6779dfa, BRF_SND }, //  5 samples
	{ "o5b",		0x100000, 0x07d6cbac, BRF_SND }, //  6

	{ "peel18cv8.bin",	0x000155, 0xaf5a83c9, BRF_OPT }, //  7 plds
};

STD_ROM_PICK(pspikes)
STD_ROM_FN(pspikes)

struct BurnDriver BurnDrvPspikes = {
	"pspikes", NULL, NULL, NULL, "1991",
	"Power Spikes (World)\0", NULL, "Video System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesRomInfo, pspikesRomName, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	pspikesInit,DrvExit,DrvFrame,pspikesDraw,DrvScan,&DrvRecalc,0x800,
	356,240,4,3
};


// Power Spikes (Korea)

static struct BurnRomInfo pspikeskRomDesc[] = {
	{ "20",			0x040000, 0x75cdcee2, BRF_ESS | BRF_PRG }, //  0 68000 code

	{ "19",			0x020000, 0x7e8ed6e5, BRF_ESS | BRF_PRG }, //  1 Sound CPU

	{ "g7h",		0x080000, 0x74c23c3d, BRF_GRA }, //  2 gfx 1

	{ "g7j",		0x080000, 0x0b9e4739, BRF_GRA }, //  3 gfx 2
	{ "g7l",		0x080000, 0x943139ff, BRF_GRA }, //  4

	{ "a47",		0x040000, 0xc6779dfa, BRF_SND }, //  5 samples
	{ "o5b",		0x100000, 0x07d6cbac, BRF_SND }, //  6


	{ "peel18cv8-1101a-u15.53",	0x000155, 0xc05e3bea, BRF_OPT }, //  7 plds
	{ "peel18cv8-1103-u112.76",	0x000155, 0x786da44c, BRF_OPT }, //  8
};

STD_ROM_PICK(pspikesk)
STD_ROM_FN(pspikesk)

struct BurnDriver BurnDrvPspikesk = {
	"pspikesk", "pspikes", NULL, NULL, "1991",
	"Power Spikes (Korea)\0", NULL, "Video System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikeskRomInfo, pspikeskRomName, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	pspikesInit,DrvExit,DrvFrame,pspikesDraw,DrvScan,&DrvRecalc,0x800,
	356,240,4,3
};


// Power Spikes (US)

static struct BurnRomInfo pspikesuRomDesc[] = {
	{ "svolly91.73",	0x040000, 0xbfbffcdb, BRF_ESS | BRF_PRG }, //  0 68000 code

	{ "19",			0x020000, 0x7e8ed6e5, BRF_ESS | BRF_PRG }, //  1 Sound CPU

	{ "g7h",		0x080000, 0x74c23c3d, BRF_GRA }, //  2 gfx 1

	{ "g7j",		0x080000, 0x0b9e4739, BRF_GRA }, //  3 gfx 2
	{ "g7l",		0x080000, 0x943139ff, BRF_GRA }, //  4

	{ "a47",		0x040000, 0xc6779dfa, BRF_SND }, //  5 samples
	{ "o5b",		0x100000, 0x07d6cbac, BRF_SND }, //  6

};

STD_ROM_PICK(pspikesu)
STD_ROM_FN(pspikesu)

struct BurnDriver BurnDrvPspikesu = {
	"pspikesu", "pspikes", NULL, NULL, "1991",
	"Power Spikes (US)\0", NULL, "Video System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesuRomInfo, pspikesuRomName, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	pspikesInit,DrvExit,DrvFrame,pspikesDraw,DrvScan,&DrvRecalc,0x800,
	356,240,4,3
};


// Super Volley '91 (Japan)

static struct BurnRomInfo svolly91RomDesc[] = {
	{ "u11.jpn",	0x040000, 0xea2e4c82, BRF_ESS | BRF_PRG }, //  0 68000 code

	{ "19",			0x020000, 0x7e8ed6e5, BRF_ESS | BRF_PRG }, //  1 Sound CPU

	{ "g7h",		0x080000, 0x74c23c3d, BRF_GRA }, //  2 gfx 1

	{ "g7j",		0x080000, 0x0b9e4739, BRF_GRA }, //  3 gfx 2
	{ "g7l",		0x080000, 0x943139ff, BRF_GRA }, //  4

	{ "a47",		0x040000, 0xc6779dfa, BRF_SND }, //  5 samples
	{ "o5b",		0x100000, 0x07d6cbac, BRF_SND }, //  6

};

STD_ROM_PICK(svolly91)
STD_ROM_FN(svolly91)

struct BurnDriver BurnDrvSvolly91 = {
	"svolly91", "pspikes", NULL, NULL, "1991",
	"Super Volley '91 (Japan)\0", NULL, "Video System Co.", "V-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, svolly91RomInfo, svolly91RomName, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	pspikesInit,DrvExit,DrvFrame,pspikesDraw,DrvScan,&DrvRecalc,0x800,
	356,240,4,3
};

