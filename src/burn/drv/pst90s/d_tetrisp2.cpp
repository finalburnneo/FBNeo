// FB Alpha Tetris Plus driver module
// Based on MAME driver by Luca Elia

/*
	To do:
		clean ups
		nndmseal screen upside-down (game doesn't work, so who cares?)
		Stepping Stage (also doesn't work)
		Rock'n MegaSession (Japan)
*/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "ymz280b.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVFgRAM;
static UINT8 *DrvVBgRAM;
static UINT8 *DrvPriRAM;
static UINT8 *DrvRotRAM;
static UINT8 *DrvNvRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgScr;
static UINT8 *DrvBgScr;
static UINT8 *DrvRotReg;
static UINT8 *DrvSysReg;

static UINT32  *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 watchdog;
static INT32 game;

static UINT8 nndmseal_bank_lo;
static UINT8 nndmseal_bank_hi;
static UINT8 rockn_adpcmbank;
static INT32 rockn_adpcmbank_max;
static UINT8 rockn_soundvolume;
static UINT8 rockn_protectdata;
static INT32 rockn_14_timer = -1;
static INT32 rockn_14_timer_countdown = 0;

static struct BurnInputInfo Tetrisp2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tetrisp2)

static struct BurnInputInfo RocknInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Rockn)

static struct BurnInputInfo NndmsealInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"Print 1 (Start)",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"Print 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P1 -",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 +",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},
	{"P1 OK",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"	},
	{"P1 Cancel",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Nndmseal)

static struct BurnDIPInfo Tetrisp2DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "5 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x01, "4 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x02, "3 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x03, "2 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "5 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x08, "4 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x10, "3 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x18, "2 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin 1 Credits "	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin 2 Credits "	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin 3 Credits "	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x00, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x02, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Vs Mode Rounds"	},
	{0x15, 0x01, 0x04, 0x00, "1"			},
	{0x15, 0x01, 0x04, 0x04, "3"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x15, 0x01, 0x08, 0x08, "Japanese"		},
	{0x15, 0x01, 0x08, 0x00, "English"		},

	{0   , 0xfe, 0   ,    2, "F.B.I Logo"		},
	{0x15, 0x01, 0x10, 0x10, "Off"			},
	{0x15, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Voice"		},
	{0x15, 0x01, 0x20, 0x00, "Off"			},
	{0x15, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Tetrisp2)

static struct BurnDIPInfo Tetrisp2jDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "5 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x01, "4 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x02, "3 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x03, "2 Coins 1 Credits "	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "5 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x08, "4 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x10, "3 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x18, "2 Coins 1 Credits "	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin 1 Credits "	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin 2 Credits "	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin 3 Credits "	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x00, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x02, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Vs Mode Rounds"	},
	{0x15, 0x01, 0x04, 0x00, "1"			},
	{0x15, 0x01, 0x04, 0x04, "3"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x15, 0x01, 0x08, 0x08, "Japanese"		},
	{0x15, 0x01, 0x08, 0x00, "English (buggy!)"	},

	{0   , 0xfe, 0   ,    2, "Voice"		},
	{0x15, 0x01, 0x20, 0x00, "Off"			},
	{0x15, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Tetrisp2j)

static struct BurnDIPInfo RocknDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL			},
	{0x0b, 0xff, 0xff, 0xff, NULL			},
	{0x0c, 0xff, 0xff, 0x10, NULL			},

	{0   , 0xfe, 0   ,    1, "Service Mode"		},
	{0x0c, 0x01, 0x10, 0x10, "Off"			},
	{0x0c, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFO(Rockn)

static struct BurnDIPInfo NndmsealDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL			},
	{0x0b, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Time?"		},
	{0x0a, 0x01, 0x40, 0x00, "35"			},
	{0x0a, 0x01, 0x40, 0x40, "45"			},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x0b, 0x01, 0x0f, 0x0f, "0"			},
	{0x0b, 0x01, 0x0f, 0x0e, "1"			},
	{0x0b, 0x01, 0x0f, 0x0d, "2"			},
	{0x0b, 0x01, 0x0f, 0x0c, "3"			},
	{0x0b, 0x01, 0x0f, 0x0b, "4"			},
	{0x0b, 0x01, 0x0f, 0x0a, "5"			},
	{0x0a, 0x01, 0x0f, 0x09, "6"			},
	{0x0b, 0x01, 0x0f, 0x08, "7"			},
	{0x0b, 0x01, 0x0f, 0x07, "8"			},
	{0x0b, 0x01, 0x0f, 0x06, "9"			},
	{0x0b, 0x01, 0x0f, 0x05, "a"			},
	{0x0b, 0x01, 0x0f, 0x04, "b"			},
	{0x0b, 0x01, 0x0f, 0x03, "c"			},
	{0x0b, 0x01, 0x0f, 0x02, "d"			},
	{0x0b, 0x01, 0x0f, 0x01, "e"			},
	{0x0b, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0b, 0x01, 0x10, 0x00, "Off"			},
	{0x0b, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x80, 0x80, "Off"			},
	{0x0b, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Nndmseal)

static inline void palette_update(INT32 offset)
{
	if (offset & 2) return; // every other word

	offset &= 0x1fffc;

	INT32 p = *((UINT16*)(DrvPalRAM + offset)) >> 1;

	UINT8 r,g,b;

	r =  p & 0x1f;
	g = (p >>  5) & 0x1f;
	b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/4] = BurnHighCol(r, g, b, 0);
}

static void rockn_adpcmbankswitch(INT32 data)
{
	rockn_adpcmbank = data;

	INT32 bank = ((data & 0x001c) >> 2);

	if (bank > 7)
	{
		bank = 0;
	}

	memcpy (DrvSndROM + 0x0400000, DrvSndROM + 0x1000000 + (0x0c00000 * bank), 0x0c00000);
}

static void rockn2_adpcmbankswitch(INT32 data)
{
	INT32 bank;

	char banktable[9][3]=
	{
		{  0,  1,  2 },     // bank $00
		{  3,  4,  5 },     // bank $04
		{  6,  7,  8 },     // bank $08
		{  9, 10, 11 },     // bank $0c
		{ 12, 13, 14 },     // bank $10
		{ 15, 16, 17 },     // bank $14
		{ 18, 19, 20 },     // bank $18
		{  0,  0,  0 },     // bank $1c
		{  0,  5, 14 },     // bank $20
	};

	rockn_adpcmbank = data;

	bank = ((data & 0x003f) >> 2);

	if (bank > 8)
	{
		bank = 0;
	}

	memcpy (DrvSndROM + 0x0400000, DrvSndROM + 0x1000000 + (0x0400000 * banktable[bank][0]), 0x0400000);
	memcpy (DrvSndROM + 0x0800000, DrvSndROM + 0x1000000 + (0x0400000 * banktable[bank][1]), 0x0400000);
	memcpy (DrvSndROM + 0x0c00000, DrvSndROM + 0x1000000 + (0x0400000 * banktable[bank][2]), 0x0400000);
}

static void nndmseal_sound_bankswitch(INT32 data)
{
	if (data & 0x04)
	{
		nndmseal_bank_lo = data & 0x03;

		memcpy(DrvSndROM, DrvSndROM + 0x40000 + (nndmseal_bank_lo * 0x80000), 0x20000);
	}
	else
	{
		nndmseal_bank_hi = data & 0x03;

		memcpy(DrvSndROM + 0x20000, DrvSndROM + 0x40000 + (nndmseal_bank_lo * 0x80000) + (nndmseal_bank_hi * 0x20000), 0x20000);
	}
}

static void __fastcall tetrisp2_write_word(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);

	if ((address & 0xfc0000) == 0x200000) {
		DrvPriRAM[(address & 0x3fffe)/2] = data;
		return;
	}

	if ((address & 0xfe0000) == 0x300000) {
		*((UINT16*)(DrvPalRAM + (address & 0x1fffe))) = data;
		palette_update(address & 0x1fffe);
		return;
	}

	if ((address & 0xfffff0) == 0xb40000) {
		*((UINT16*)(DrvFgScr + (address & 0x0e))) = data;
		return;
	}

	if ((address & 0xfffff0) == 0xb40010) {
		*((UINT16*)(DrvBgScr + (address & 0x0e))) = data;
		return;
	}

	if (address >= 0xb60000 && address <= 0xb6002f) {
		*((UINT16*)(DrvRotReg + (address & 0x3e))) = data;
		return;
	}

	if ((address & 0xffffe0) == 0xba0000)
	{
		if (address == 0xba0018)
		{
			rockn_14_timer = (0x1000 - data) * 6000;
			rockn_14_timer_countdown = rockn_14_timer / 6000;
		}

		*((UINT16*)(DrvSysReg + (address & 0x1e))) = data;
		return;
	}

	switch (address)
	{
		case 0x800000:
			if (game == 3) {
				MSM6295Command(0, data);
			} else {
				YMZ280BSelectRegister(data);
			}
		return;

		case 0x800002:
			if (game != 3) {
				YMZ280BWriteRegister(data);
			}
		return;

		case 0xa30000:
			rockn_soundvolume = data;
		return;

		case 0xa40000: // rockn1
			YMZ280BSelectRegister(data);
		return;

		case 0xa40002: // rockn1
			YMZ280BWriteRegister(data);
		return;

		case 0xa44000:
			if (game == 2) rockn2_adpcmbankswitch(data);
			if (game == 1) rockn_adpcmbankswitch(data);
		return;

		case 0xb00000: // coincounter
		return;

		case 0xb80000:
			if (game == 3) nndmseal_sound_bankswitch(data);
		return;
	}
}

static void __fastcall tetrisp2_write_byte(UINT32 address, UINT8 data)
{
//	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);

	if ((address & 0xfc0000) == 0x200000) {
		DrvPriRAM[(address & 0x3fffe)/2] = data;
		return;
	}

	if ((address & 0xfe0000) == 0x300000) {
		DrvPalRAM[(address & 0x1ffff)^1] = data;
		palette_update(address & 0x1fffe);
		return;
	}

	if ((address & 0xff8000) == 0x900000) {
		DrvNvRAM[(address & 0x003ffe)+0] = data;
		DrvNvRAM[(address & 0x003ffe)+1] = data;
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall tetrisp2_read_word(UINT32 address)
{
//	bprintf (0, _T("RW: %5.5x\n"), address);

	if ((address & 0xfc0000) == 0x200000) {
		return DrvPriRAM[(address & 0x3fffe)/2] | 0xff00;
	}

	if ((address & 0xfffff0) == 0xb40000) {
		return *((UINT16*)(DrvFgScr + (address & 0x0e)));
	}

	if ((address & 0xfffff0) == 0xb40010) {
		return *((UINT16*)(DrvBgScr + (address & 0x0e)));
	}

	if (address >= 0xb60000 && address <= 0xb6002f) {
		return *((UINT16*)(DrvRotReg + (address & 0x3e)));
	}

	if ((address & 0xffffe0) == 0xba0000) {
		return *((UINT16*)(DrvSysReg + (address & 0x1e)));
	}

	switch (address)
	{
		case 0x800000:
		case 0x800002:
			if (game == 3) return MSM6295ReadStatus(0);
			return YMZ280BReadStatus();

		case 0xa30000: // rockn1 sound volume
			return 0xffff;

		case 0xa40000: // rockn1
		case 0xa40002:
			return YMZ280BReadStatus();

		case 0xa44000:
			return ((rockn_adpcmbank & 0xf0ff) | (rockn_protectdata << 8));

		case 0xbe0000:
			return 0; // nop

		case 0xbe0002:
			if (game == 1) return DrvInputs[0] ^ 0x0030;
			return DrvInputs[0];

		case 0xbe0004:
			if (game == 3) return DrvInputs[1]; // nndmseal
			if (game == 1) return (DrvInputs[1] & ~0x0010) | (DrvDips[2] & 0x10);
			return (DrvInputs[1] & 0xfcff) | (rand() & 0x0300) | (1 << ((rand() & 1) + 8));

		case 0xbe0006:
			return (DrvInputs[2] & 0xfffb); // nndmseal, bit 2 must be high!

		case 0xbe0008:
			return (DrvDips[1] << 8) | DrvDips[0];

		case 0xbe000a:
			watchdog = 0;
			return 0;
	}

	return 0;
}

static UINT8 __fastcall tetrisp2_read_byte(UINT32 address)
{
	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	if (game == 3) {
		MSM6295Reset(0);
	} else {
		YMZ280BReset();
	}

	watchdog = 0;

	rockn_adpcmbank = 0;
	rockn_soundvolume = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvGfxROM0	= Next; Next += 0x800000;
	DrvGfxROM1	= Next; Next += 0x800000;
	DrvGfxROM2	= DrvGfxROM1 +  0x400000;
	DrvGfxROM3	= Next; Next += 0x080000;

	MSM6295ROM	= Next;
	YMZ280BROM	= Next;
	DrvSndROM	= Next; Next += 0x7000000;

	DrvPalette	= (UINT32*)Next; Next += 0x8000 * sizeof(int);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x00c000;
	Drv68KRAM1	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x020000;

	DrvVFgRAM	= Next; Next += 0x004000;
	DrvVBgRAM	= Next; Next += 0x006000;

	DrvPriRAM	= Next; Next += 0x040000;
	DrvRotRAM	= Next; Next += 0x010000;
	DrvNvRAM	= Next; Next += 0x004000;

	DrvSprRAM	= Next; Next += 0x004000;

	DrvFgScr	= Next; Next += 0x000010;
	DrvBgScr	= Next; Next += 0x000010;
	DrvRotReg 	= Next; Next += 0x000040;
	DrvSysReg	= Next; Next += 0x000020;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static tilemap_callback( rtlayer )
{
	UINT16 *ram = (UINT16*)DrvRotRAM;

	TILE_SET_INFO(2, ram[offs * 2 + 0], ram[offs * 2 + 1] & 0x000f, 0);
}

static tilemap_callback( bglayer )
{
	UINT16 *ram = (UINT16*)DrvVBgRAM;

	TILE_SET_INFO(1, ram[offs * 2 + 0], ram[offs * 2 + 1] & 0x000f, 0);
}

static tilemap_callback( fglayer )
{
	UINT16 *ram = (UINT16*)DrvVFgRAM;

	TILE_SET_INFO(3, ram[offs * 2 + 0], ram[offs * 2 + 1] & 0x000f, 0);
}

static INT32 Tetrisp2Init()
{
	game = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 2)) return 1;

		for (INT32 i = 0; i < 0x800000; i+=4) {
			BurnByteswap(DrvGfxROM0 + i + 1, 2);
		}

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  7, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	0x104000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x31ffff, MAP_ROM); // palette
	SekMapMemory(DrvVFgRAM,		0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvVBgRAM,		0x404000, 0x409fff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x650000, 0x651fff, MAP_RAM); // mirror
	SekMapMemory(DrvNvRAM,		0x900000, 0x903fff, MAP_ROM); // handler
	SekMapMemory(DrvNvRAM,		0x904000, 0x907fff, MAP_ROM); // handler
	SekSetWriteWordHandler(0,	tetrisp2_write_word);
	SekSetWriteByteHandler(0,	tetrisp2_write_byte);
	SekSetReadWordHandler(0,	tetrisp2_read_word);
	SekSetReadByteHandler(0,	tetrisp2_read_byte);
	SekClose();

	YMZ280BInit(16934400, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	srand(0x9a89810f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, rtlayer_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bglayer_map_callback, 16, 16,  64,  64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fglayer_map_callback,  8,  8,  64,  64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x800000, 0x1000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 16, 16, 0x400000, 0x2000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 8,  8,  8, 0x080000, 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 RocknInit()
{
	game = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,   0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,   1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,   2, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,   3, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i+=4) {
			BurnByteswap(DrvGfxROM0 + i + 1, 2);
		}

		memcpy (DrvGfxROM0 + 0x400000, DrvGfxROM0, 0x400000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,   4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000,   5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,   6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x1000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x1400000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x1800000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x1c00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x2000000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x2400000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x2800000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x2c00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x3000000, 16, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x3400000, 17, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x3800000, 18, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x3c00000, 19, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x4000000, 20, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x4400000, 21, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	0x104000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x31ffff, MAP_ROM); // palette
	SekMapMemory(DrvVFgRAM,		0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvVBgRAM,		0x404000, 0x409fff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x650000, 0x651fff, MAP_RAM); // mirror
	SekMapMemory(DrvNvRAM,		0x900000, 0x903fff, MAP_RAM);
	SekSetWriteWordHandler(0,	tetrisp2_write_word);
	SekSetWriteByteHandler(0,	tetrisp2_write_byte);
	SekSetReadWordHandler(0,	tetrisp2_read_word);
	SekSetReadByteHandler(0,	tetrisp2_read_byte);
	SekClose();

	rockn_protectdata = 1;
	rockn_adpcmbank_max = (0x4800000 - 0x1000000) / 0xc00000;

	YMZ280BInit(16934400, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	srand(0x9a89810f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, rtlayer_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bglayer_map_callback, 16, 16, 256,  16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fglayer_map_callback,  8,  8,  64,  64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x800000, 0x1000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 16, 16, 0x400000, 0x2000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 8,  8,  8, 0x080000, 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 NndmsealInit()
{
	game = 3;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		memset (DrvGfxROM0, 0, 0x400000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x040000,  6, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	0x104000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x31ffff, MAP_ROM); // palette
	SekMapMemory(DrvVFgRAM,		0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvVBgRAM,		0x404000, 0x409fff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x650000, 0x651fff, MAP_RAM); // mirror
	SekMapMemory(DrvNvRAM,		0x900000, 0x903fff, MAP_ROM); // handler
	SekMapMemory(DrvNvRAM,		0x904000, 0x907fff, MAP_ROM); // handler
	SekSetWriteWordHandler(0,	tetrisp2_write_word);
	SekSetWriteByteHandler(0,	tetrisp2_write_byte);
	SekSetReadWordHandler(0,	tetrisp2_read_word);
	SekSetReadByteHandler(0,	tetrisp2_read_byte);
	SekClose();

	MSM6295Init(0, 2000000 / 132, 0);
	MSM6295SetRoute(0, 1.0, BURN_SND_ROUTE_BOTH);

	srand(0x9a89810f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, rtlayer_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bglayer_map_callback, 16, 16,  64,  64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fglayer_map_callback,  8,  8,  64,  64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x800000, 0x1000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 16, 16, 0x400000, 0x2000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 8,  8,  8, 0x080000, 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 NndmsealaInit()
{
	game = 3;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		memset (DrvGfxROM0, 0, 0x400000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x040000,  5, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	0x104000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x31ffff, MAP_ROM); // palette
	SekMapMemory(DrvVFgRAM,		0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvVBgRAM,		0x404000, 0x409fff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x650000, 0x651fff, MAP_RAM); // mirror
	SekMapMemory(DrvNvRAM,		0x900000, 0x903fff, MAP_ROM); // handler
	SekMapMemory(DrvNvRAM,		0x904000, 0x907fff, MAP_ROM); // handler
	SekSetWriteWordHandler(0,	tetrisp2_write_word);
	SekSetWriteByteHandler(0,	tetrisp2_write_byte);
	SekSetReadWordHandler(0,	tetrisp2_read_word);
	SekSetReadByteHandler(0,	tetrisp2_read_byte);
	SekClose();

	MSM6295Init(0, 2000000 / 132, 0);
	MSM6295SetRoute(0, 1.0, BURN_SND_ROUTE_BOTH);

	srand(0x9a89810f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, rtlayer_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bglayer_map_callback, 16, 16,  64,  64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fglayer_map_callback,  8,  8,  64,  64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x800000, 0x1000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 16, 16, 0x400000, 0x2000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 8,  8,  8, 0x080000, 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 Rockn2CommonInit(INT32 nSoundRoms)
{
	game = 2;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,   0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,   1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,   2, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,   3, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i+=4) {
			BurnByteswap(DrvGfxROM0 + i + 1, 2);
		}

		memcpy (DrvGfxROM0 + 0x400000, DrvGfxROM0, 0x400000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,   4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000,   5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,   6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  7, 1)) return 1;

		for (INT32 i = 0; i < nSoundRoms - 1; i++)
		{
			if (BurnLoadRom(DrvSndROM + 0x1000000 + (i * 0x400000), 8 + i, 1)) return 1;
		}
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	0x104000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x300000, 0x31ffff, MAP_ROM); // palette
	SekMapMemory(Drv68KRAM1,	0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvRotRAM,		0x650000, 0x651fff, MAP_RAM); // mirror
	SekMapMemory(DrvVFgRAM,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVBgRAM,		0x804000, 0x809fff, MAP_RAM);
	SekMapMemory(DrvNvRAM,		0x900000, 0x903fff, MAP_RAM);
	SekSetWriteWordHandler(0,	tetrisp2_write_word);
	SekSetWriteByteHandler(0,	tetrisp2_write_byte);
	SekSetReadWordHandler(0,	tetrisp2_read_word);
	SekSetReadByteHandler(0,	tetrisp2_read_byte);
	SekClose();

	YMZ280BInit(16934400, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	srand(0x9a89810f);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, rtlayer_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bglayer_map_callback, 16, 16, 256,  16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fglayer_map_callback,  8,  8,  64,  64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 16, 16, 0x800000, 0x1000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 8, 16, 16, 0x400000, 0x2000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 8,  8,  8, 0x080000, 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 Rockn2Init()
{
	rockn_protectdata = 2;

	return Rockn2CommonInit(22);
}

static INT32 Rockn3Init()
{
	rockn_protectdata = 4;

	return Rockn2CommonInit(21);
}

static INT32 Rockn4Init()
{
	rockn_protectdata = 4;

	return Rockn2CommonInit(10);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	if (game == 3) {
		MSM6295Exit(0);
	} else {
		YMZ280BExit();
	}

	SekExit();

	free (AllMem);
	AllMem = NULL;

	YMZ280BROM = NULL;

	return 0;
}

static void draw_sprites(UINT8 *SpriteRAM, INT32 sprram_size)
{
	INT32 x, y, tx, ty, sx, sy, flipx, flipy;
	INT32 xsize, ysize, xnum, ynum;
	INT32 xstart, ystart, xend, yend, xinc, yinc;
	INT32 code, attr, color, size;
	UINT32 primask;

	UINT16 *sprram_top = (UINT16*)SpriteRAM;
	UINT16	*source	=	sprram_top;
	UINT16	*finish	=	sprram_top + (sprram_size - 0x10) / 2;

	UINT8 *priority_ram = DrvPriRAM;

//	INT32 flipscreen = *((UINT16*)(DrvSysReg + 0)) & 0x02;

	for (; source <= finish; source += 0x10/2 )
	{
		attr	=	source[ 0 ];

		if ((attr & 0x0004) == 0) continue;

		flipx		=	attr & 1;
		flipy		=	attr & 2;

		code		=	source[ 1 ];
		color		=	source[ 2 ];

		tx		=	(code >> 0) & 0xff;
		ty		=	(code >> 8) & 0xff;

		code		=	(tx / 8) + (ty / 8) * (0x100/8) + (color & 0x7f) * (0x100/8) * (0x100/8);

		color		=	(color >> 12) & 0xf;

		size		=	source[ 3 ];

		xsize		=	((size >> 0) & 0xff) + 1;
		ysize		=	((size >> 8) & 0xff) + 1;

		xnum		=	( ((tx + xsize) & ~7) + (((tx + xsize) & 7) ? 8 : 0) - (tx & ~7) ) / 8;
		ynum		=	( ((ty + ysize) & ~7) + (((ty + ysize) & 7) ? 8 : 0) - (ty & ~7) ) / 8;

		sy		=	source[ 4 ];
		sx		=	source[ 5 ];

		sx		=	(sx & 0x3ff) - (sx & 0x400);
		sy		=	(sy & 0x1ff) - (sy & 0x200);

		if (flipx)	{ xstart = xnum-1;  xend = -1;    xinc = -1;  sx -= xnum*8 - xsize - (tx & 7); }
		else		{ xstart = 0;       xend = xnum;  xinc = +1;  sx -= tx & 7; }

		if (flipy)	{ ystart = ynum-1;  yend = -1;    yinc = -1;  sy -= ynum*8 - ysize - (ty & 7); }
		else		{ ystart = 0;       yend = ynum;  yinc = +1;  sy -= ty & 7; }

		primask = 0;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x1500) / 2] & 0x38) primask |= 1 << 0;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x1400) / 2] & 0x38) primask |= 1 << 1;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x1100) / 2] & 0x38) primask |= 1 << 2;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x1000) / 2] & 0x38) primask |= 1 << 3;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x0500) / 2] & 0x38) primask |= 1 << 4;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x0400) / 2] & 0x38) primask |= 1 << 5;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x0100) / 2] & 0x38) primask |= 1 << 6;
		if (priority_ram[((attr & 0x00f0) | 0x0a00 | 0x0000) / 2] & 0x38) primask |= 1 << 7;

		for (y = ystart; y != yend; y += yinc)
		{
			for (x = xstart; x != xend; x += xinc)
			{
				RenderPrioSprite(pTransDraw, DrvGfxROM0, code++, (color << 8), 0, sx + x * 8, sy + y * 8, flipx, flipy, 8, 8, primask);
			}

			code += (0x100/8) - xnum;
		}
	}
}

static INT32 Tetrisp2Draw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x20000; i++) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear(); // black?

	INT32 flipscreen = *((UINT16*)(DrvSysReg + 0)) & 0x02;

	UINT16 *bgregs = (UINT16 *)DrvBgScr;
	UINT16 *rtregs = (UINT16 *)DrvRotReg;
	UINT16 *fgregs = (UINT16 *)DrvFgScr;

	INT32 asc_pri = 0;
	INT32 scr_pri = 0;
	INT32 rot_pri = 0;

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen) ? TMAP_FLIPXY : 0);

	GenericTilemapSetScrollX(0, (rtregs[0] - (flipscreen ? 0x53f : 0x400)));
	GenericTilemapSetScrollY(0, (rtregs[2] - (flipscreen ? 0x4df : 0x400)));
	GenericTilemapSetScrollX(1, (bgregs[0] + bgregs[2] + 0x14));
	GenericTilemapSetScrollY(1, (bgregs[3] + bgregs[5]));
	GenericTilemapSetScrollX(2, (fgregs[2]));
	GenericTilemapSetScrollY(2, (fgregs[5]));

	if(DrvPriRAM[0x2b00 / 2] == 0x34)
		asc_pri++;
	else
		rot_pri++;

	if(DrvPriRAM[0x2e00 / 2] == 0x34)
		asc_pri++;
	else
		scr_pri++;

	if(DrvPriRAM[0x3a00 / 2] == 0x0c)
		scr_pri++;
	else
		rot_pri++;

	if (rot_pri == 0)
		GenericTilemapDraw(0, pTransDraw, 2); // rot
	else if (scr_pri == 0)
		GenericTilemapDraw(1, pTransDraw, 1); // bg
	else if (asc_pri == 0)
		GenericTilemapDraw(2, pTransDraw, 4); // fg

	if (rot_pri == 1)
		GenericTilemapDraw(0, pTransDraw, 2); // rot
	else if (scr_pri == 1)
		GenericTilemapDraw(1, pTransDraw, 1); // bg
	else if (asc_pri == 1)
		GenericTilemapDraw(2, pTransDraw, 4); // fg

	if (rot_pri == 2)
		GenericTilemapDraw(0, pTransDraw, 2); // rot
	else if (scr_pri == 2)
		GenericTilemapDraw(1, pTransDraw, 1); // bg
	else if (asc_pri == 2)
		GenericTilemapDraw(2, pTransDraw, 4); // fg

	if (nBurnLayer & 8) draw_sprites(DrvSprRAM, 0x4000);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Tetrisp2Frame()
{
	watchdog++;
	if (watchdog >= 180) {
		bprintf (0, _T("Watchdog!!!\n"));
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	SekOpen(0);
	SekRun(12000000 / 60);
	SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 RocknFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		bprintf (0, _T("Watchdog!!!\n"));
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(short));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	SekOpen(0);
	for (INT32 i = 0; i < 33; i++)
	{
		SekRun(12000000 / 60 / 33);

		if (rockn_14_timer != -1) {
			if (rockn_14_timer_countdown == 0) {
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
				rockn_14_timer_countdown = rockn_14_timer / 6000;
			}
			rockn_14_timer_countdown--;
		}

		if (i == 30 && GetCurrentFrame() & 1) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	}

	SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

	SekClose();

	if (pBurnSoundOut) {
		if (game == 3) {
			MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		} else {
			YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}


// save states

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029732;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = MemEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		if (game == 3) {
			MSM6295Scan(0, nAction);
		} else {
			YMZ280BScan();
		}

		SCAN_VAR(nndmseal_bank_lo);
		SCAN_VAR(nndmseal_bank_hi);
		SCAN_VAR(rockn_adpcmbank);
		SCAN_VAR(rockn_soundvolume);
		SCAN_VAR(rockn_14_timer);
		SCAN_VAR(rockn_14_timer_countdown);

	}

	if (nAction & ACB_WRITE) {
		if (game == 2) rockn2_adpcmbankswitch(rockn_adpcmbank);
		if (game == 1) rockn_adpcmbankswitch(rockn_adpcmbank);
		if (game == 3) {
			nndmseal_sound_bankswitch(nndmseal_bank_lo | 0x04);
			nndmseal_sound_bankswitch(nndmseal_bank_hi);
		}
	}

	return 0;
}


// Tetris Plus 2 (World)

static struct BurnRomInfo tetrisp2RomDesc[] = {
	{ "tet2_4_ver2.8.ic59",		0x080000, 0xe67f9c51, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tet2_1_ver2.8.ic65",		0x080000, 0x5020a4ed, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "96019-01.9",			0x400000, 0x06f7dc64, 0x02 | BRF_GRA },           //  2 Sprites
	{ "96019-02.8",			0x400000, 0x3e613bed, 0x02 | BRF_GRA },           //  3

	{ "96019-06.13",		0x400000, 0x16f7093c, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "96019-04.6",			0x100000, 0xb849dec9, 0x03 | BRF_GRA },           //  5

	{ "tetp2-10.ic27",		0x080000, 0x34dd1bad, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "96019-07.7",			0x400000, 0xa8a61954, 0x05 | BRF_SND },           //  7 YMZ280b Samples
};

STD_ROM_PICK(tetrisp2)
STD_ROM_FN(tetrisp2)

struct BurnDriver BurnDrvTetrisp2 = {
	"tetrisp2", NULL, NULL, NULL, "1997",
	"Tetris Plus 2 (World)\0", NULL, "Jaleco / The Tetris Company", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tetrisp2RomInfo, tetrisp2RomName, NULL, NULL, Tetrisp2InputInfo, Tetrisp2DIPInfo,
	Tetrisp2Init, DrvExit, Tetrisp2Frame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Tetris Plus 2 (Japan, V2.2)

static struct BurnRomInfo tetrisp2jRomDesc[] = {
	{ "tet2_4_ver2.2.ic59",		0x080000, 0x5bfa32c8, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tet2_1_ver2.2.ic65",		0x080000, 0x919116d0, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "96019-01.9",			0x400000, 0x06f7dc64, 0x02 | BRF_GRA },           //  2 Sprites
	{ "96019-02.8",			0x400000, 0x3e613bed, 0x02 | BRF_GRA },           //  3

	{ "96019-06.13",		0x400000, 0x16f7093c, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "96019-04.6",			0x100000, 0xb849dec9, 0x03 | BRF_GRA },           //  5

	{ "tetp2-10.ic27",		0x080000, 0x34dd1bad, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "96019-07.7",			0x400000, 0xa8a61954, 0x05 | BRF_SND },           //  7 YMZ280b Samples
};

STD_ROM_PICK(tetrisp2j)
STD_ROM_FN(tetrisp2j)

struct BurnDriver BurnDrvTetrisp2j = {
	"tetrisp2j", "tetrisp2", NULL, NULL, "1997",
	"Tetris Plus 2 (Japan, V2.2)\0", NULL, "Jaleco / The Tetris Company", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tetrisp2jRomInfo, tetrisp2jRomName, NULL, NULL, Tetrisp2InputInfo, Tetrisp2jDIPInfo,
	Tetrisp2Init, DrvExit, Tetrisp2Frame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Tetris Plus 2 (Japan, V2.1)

static struct BurnRomInfo tetrisp2jaRomDesc[] = {
	{ "tet2_ic4_ver2.1.ic59",	0x080000, 0x5bfa32c8, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tet2_ic1_ver2.1.ic65",	0x080000, 0x5b5f8377, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "96019-01.9",			0x400000, 0x06f7dc64, 0x02 | BRF_GRA },           //  2 Sprites
	{ "96019-02.8",			0x400000, 0x3e613bed, 0x02 | BRF_GRA },           //  3

	{ "96019-06.13",		0x400000, 0x16f7093c, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "96019-04.6",			0x100000, 0xb849dec9, 0x03 | BRF_GRA },           //  5

	{ "tetp2-10.ic27",		0x080000, 0x34dd1bad, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "96019-07.7",			0x400000, 0xa8a61954, 0x05 | BRF_SND },           //  7 YMZ280b Samples
};

STD_ROM_PICK(tetrisp2ja)
STD_ROM_FN(tetrisp2ja)

struct BurnDriver BurnDrvTetrisp2ja = {
	"tetrisp2ja", "tetrisp2", NULL, NULL, "1997",
	"Tetris Plus 2 (Japan, V2.1)\0", NULL, "Jaleco / The Tetris Company", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tetrisp2jaRomInfo, tetrisp2jaRomName, NULL, NULL, Tetrisp2InputInfo, Tetrisp2jDIPInfo,
	Tetrisp2Init, DrvExit, Tetrisp2Frame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};


// Rock'n Tread (Japan)

static struct BurnRomInfo rocknRomDesc[] = {
	{ "rock_n_1_vj-98344_1.bin",	0x080000, 0x4cf79e58, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rock_n_1_vj-98344_4.bin",	0x080000, 0xcaa33f79, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rock_n_1_vj-98344_8.bin",	0x200000, 0xfa3f6f9c, 0x02 | BRF_GRA },           //  2 Sprites
	{ "rock_n_1_vj-98344_9.bin",	0x200000, 0x3d12a688, 0x02 | BRF_GRA },           //  3

	{ "rock_n_1_vj-98344_13.bin",	0x200000, 0x261b99a0, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "rock_n_1_vj-98344_6.bin",	0x100000, 0x5551717f, 0x03 | BRF_GRA },           //  5

	{ "rock_n_1_vj-98344_10.bin",	0x080000, 0x918663a8, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "sound00",			0x400000, 0xc354f753, 0x05 | BRF_SND },           //  7 YMZ280b Samples
	{ "sound01",			0x400000, 0x5b42999e, 0x05 | BRF_SND },           //  8
	{ "sound02",			0x400000, 0x8306f302, 0x05 | BRF_SND },           //  9
	{ "sound03",			0x400000, 0x3fda842c, 0x05 | BRF_SND },           // 10
	{ "sound04",			0x400000, 0x86d4f289, 0x05 | BRF_SND },           // 11
	{ "sound05",			0x400000, 0xf8dbf47d, 0x05 | BRF_SND },           // 12
	{ "sound06",			0x400000, 0x525aff97, 0x05 | BRF_SND },           // 13
	{ "sound07",			0x400000, 0x5bd8bb95, 0x05 | BRF_SND },           // 14
	{ "sound08",			0x400000, 0x304c1643, 0x05 | BRF_SND },           // 15
	{ "sound09",			0x400000, 0x78c22c56, 0x05 | BRF_SND },           // 16
	{ "sound10",			0x400000, 0xd5e8d8a5, 0x05 | BRF_SND },           // 17
	{ "sound11",			0x400000, 0x569ef4dd, 0x05 | BRF_SND },           // 18
	{ "sound12",			0x400000, 0xaae8d59c, 0x05 | BRF_SND },           // 19
	{ "sound13",			0x400000, 0x9ec1459b, 0x05 | BRF_SND },           // 20
	{ "sound14",			0x400000, 0xb26f9a81, 0x05 | BRF_SND },           // 21
};

STD_ROM_PICK(rockn)
STD_ROM_FN(rockn)

struct BurnDriver BurnDrvRockn = {
	"rockn", NULL, NULL, NULL, "1999",
	"Rock'n Tread (Japan)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rocknRomInfo, rocknRomName, NULL, NULL, RocknInputInfo, RocknDIPInfo,
	RocknInit, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};


// Rock'n Tread (Japan, alternate)

static struct BurnRomInfo rocknaRomDesc[] = {
	{ "rock_n_1_vj-98344_1",	0x080000, 0x6078fa48, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rock_n_1_vj-98344_4",	0x080000, 0xc8310bd0, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rock_n_1_vj-98344_8.bin",	0x200000, 0xfa3f6f9c, 0x02 | BRF_GRA },           //  2 Sprites
	{ "rock_n_1_vj-98344_9.bin",	0x200000, 0x3d12a688, 0x02 | BRF_GRA },           //  3

	{ "rock_n_1_vj-98344_13.bin",	0x200000, 0x261b99a0, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "rock_n_1_vj-98344_6.bin",	0x100000, 0x5551717f, 0x03 | BRF_GRA },           //  5

	{ "rock_n_1_vj-98344_10.bin",	0x080000, 0x918663a8, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "sound00",			0x400000, 0xc354f753, 0x05 | BRF_SND },           //  7 YMZ280b Samples
	{ "sound01",			0x400000, 0x5b42999e, 0x05 | BRF_SND },           //  8
	{ "sound02",			0x400000, 0x8306f302, 0x05 | BRF_SND },           //  9
	{ "sound03",			0x400000, 0x3fda842c, 0x05 | BRF_SND },           // 10
	{ "sound04",			0x400000, 0x86d4f289, 0x05 | BRF_SND },           // 11
	{ "sound05",			0x400000, 0xf8dbf47d, 0x05 | BRF_SND },           // 12
	{ "sound06",			0x400000, 0x525aff97, 0x05 | BRF_SND },           // 13
	{ "sound07",			0x400000, 0x5bd8bb95, 0x05 | BRF_SND },           // 14
	{ "sound08",			0x400000, 0x304c1643, 0x05 | BRF_SND },           // 15
	{ "sound09",			0x400000, 0x78c22c56, 0x05 | BRF_SND },           // 16
	{ "sound10",			0x400000, 0xd5e8d8a5, 0x05 | BRF_SND },           // 17
	{ "sound11",			0x400000, 0x569ef4dd, 0x05 | BRF_SND },           // 18
	{ "sound12",			0x400000, 0xaae8d59c, 0x05 | BRF_SND },           // 19
	{ "sound13",			0x400000, 0x9ec1459b, 0x05 | BRF_SND },           // 20
	{ "sound14",			0x400000, 0xb26f9a81, 0x05 | BRF_SND },           // 21
};

STD_ROM_PICK(rockna)
STD_ROM_FN(rockna)

struct BurnDriver BurnDrvRockna = {
	"rockna", "rockn", NULL, NULL, "1999",
	"Rock'n Tread (Japan, alternate)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rocknaRomInfo, rocknaRomName, NULL, NULL, RocknInputInfo, RocknDIPInfo,
	RocknInit, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};


// Rock'n Tread 2 (Japan)

static struct BurnRomInfo rockn2RomDesc[] = {
	{ "rock_n_2_vj-98344_1_v1.0",	0x080000, 0x854b5a45, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rock_n_2_vj-98344_4_v1.0",	0x080000, 0x4665bbd2, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rock_n_2_vj-98344_8_v1.0",	0x200000, 0x673ce2c2, 0x02 | BRF_GRA },           //  2 Sprites
	{ "rock_n_2_vj-98344_9_v1.0",	0x200000, 0x9d3968cf, 0x02 | BRF_GRA },           //  3

	{ "rock_n_2_vj-98344_13_v1.0",	0x200000, 0xe35c55b3, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "rock_n_2_vj-98344_6_v1.0",	0x200000, 0x241d7449, 0x03 | BRF_GRA },           //  5

	{ "rock_n_2_vj-98344_10_v1.0",	0x080000, 0xae74d5b3, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "sound00",			0x400000, 0x4e9611a3, 0x05 | BRF_SND },           //  7 YMZ280b Samples
	{ "sound01",			0x400000, 0xec600f13, 0x05 | BRF_SND },           //  8
	{ "sound02",			0x400000, 0x8306f302, 0x05 | BRF_SND },           //  9
	{ "sound03",			0x400000, 0x3fda842c, 0x05 | BRF_SND },           // 10
	{ "sound04",			0x400000, 0x86d4f289, 0x05 | BRF_SND },           // 11
	{ "sound05",			0x400000, 0xf8dbf47d, 0x05 | BRF_SND },           // 12
	{ "sound06",			0x400000, 0x06f7bd63, 0x05 | BRF_SND },           // 13
	{ "sound07",			0x400000, 0x22f042f6, 0x05 | BRF_SND },           // 14
	{ "sound08",			0x400000, 0xdd294d8e, 0x05 | BRF_SND },           // 15
	{ "sound09",			0x400000, 0x8fedee6e, 0x05 | BRF_SND },           // 16
	{ "sound10",			0x400000, 0x01292f11, 0x05 | BRF_SND },           // 17
	{ "sound11",			0x400000, 0x20dc76ba, 0x05 | BRF_SND },           // 18
	{ "sound12",			0x400000, 0x11fff0bc, 0x05 | BRF_SND },           // 19
	{ "sound13",			0x400000, 0x2367dd18, 0x05 | BRF_SND },           // 20
	{ "sound14",			0x400000, 0x75ced8c0, 0x05 | BRF_SND },           // 21
	{ "sound15",			0x400000, 0xaeaca380, 0x05 | BRF_SND },           // 22
	{ "sound16",			0x400000, 0x21d50e32, 0x05 | BRF_SND },           // 23
	{ "sound17",			0x400000, 0xde785a2a, 0x05 | BRF_SND },           // 24
	{ "sound18",			0x400000, 0x18cabb1e, 0x05 | BRF_SND },           // 25
	{ "sound19",			0x400000, 0x33c89e53, 0x05 | BRF_SND },           // 26
	{ "sound20",			0x400000, 0x89c1b088, 0x05 | BRF_SND },           // 27
	{ "sound21",			0x400000, 0x13db74bd, 0x05 | BRF_SND },           // 28
};

STD_ROM_PICK(rockn2)
STD_ROM_FN(rockn2)

struct BurnDriver BurnDrvRockn2 = {
	"rockn2", NULL, NULL, NULL, "1999",
	"Rock'n Tread 2 (Japan)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rockn2RomInfo, rockn2RomName, NULL, NULL, RocknInputInfo, RocknDIPInfo,
	Rockn2Init, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};


// Rock'n 3 (Japan)

static struct BurnRomInfo rockn3RomDesc[] = {
	{ "rock_n_3_vj-98344_1_v1.0",	0x080000, 0xabc6ab4a, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rock_n_3_vj-98344_4_v1.0",	0x080000, 0x3ecba46e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rock_n_3_vj-98344_8_v1.0",	0x200000, 0x468bf696, 0x02 | BRF_GRA },           //  2 Sprites
	{ "rock_n_3_vj-98344_9_v1.0",	0x200000, 0x8a61fc18, 0x02 | BRF_GRA },           //  3

	{ "rock_n_3_vj-98344_13_v1.0",	0x200000, 0xe01bf471, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "rock_n_3_vj-98344_6_v1.0",	0x200000, 0x4e146de5, 0x03 | BRF_GRA },           //  5

	{ "rock_n_3_vj-98344_10_v1.0",	0x080000, 0x8100039e, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "mr99029-01.ic28",		0x400000, 0xe2f69042, 0x05 | BRF_SND },           //  7 YMZ280b Samples
	{ "mr99029-02.ic1",		0x400000, 0xb328b18f, 0x05 | BRF_SND },           //  8
	{ "mr99029-03.ic2",		0x400000, 0xf46438e3, 0x05 | BRF_SND },           //  9
	{ "mr99029-04.ic3",		0x400000, 0xb979e887, 0x05 | BRF_SND },           // 10
	{ "mr99029-05.ic4",		0x400000, 0x0bb2c212, 0x05 | BRF_SND },           // 11
	{ "mr99029-06.ic5",		0x400000, 0x3116e437, 0x05 | BRF_SND },           // 12
	{ "mr99029-07.ic6",		0x400000, 0x26b37ef6, 0x05 | BRF_SND },           // 13
	{ "mr99029-08.ic7",		0x400000, 0x1dd3f4e3, 0x05 | BRF_SND },           // 14
	{ "mr99029-09.ic8",		0x400000, 0xa1b03d67, 0x05 | BRF_SND },           // 15
	{ "mr99029-10.ic10",		0x400000, 0x35107aac, 0x05 | BRF_SND },           // 16
	{ "mr99029-11.ic11",		0x400000, 0x059ec592, 0x05 | BRF_SND },           // 17
	{ "mr99029-12.ic12",		0x400000, 0x84d4badb, 0x05 | BRF_SND },           // 18
	{ "mr99029-13.ic13",		0x400000, 0x4527a9b7, 0x05 | BRF_SND },           // 19
	{ "mr99029-14.ic14",		0x400000, 0xbfa4b7ce, 0x05 | BRF_SND },           // 20
	{ "mr99029-15.ic15",		0x400000, 0xa2ccd2ce, 0x05 | BRF_SND },           // 21
	{ "mr99029-16.ic16",		0x400000, 0x95baf678, 0x05 | BRF_SND },           // 22
	{ "mr99029-17.ic17",		0x400000, 0x5883c84b, 0x05 | BRF_SND },           // 23
	{ "mr99029-18.ic19",		0x400000, 0xf92098ce, 0x05 | BRF_SND },           // 24
	{ "mr99029-19.ic20",		0x400000, 0xdbb2c228, 0x05 | BRF_SND },           // 25
	{ "mr99029-20.ic21",		0x400000, 0x9efdae1c, 0x05 | BRF_SND },           // 26
	{ "mr99029-21.ic22",		0x400000, 0x5f301b83, 0x05 | BRF_SND },           // 27
};

STD_ROM_PICK(rockn3)
STD_ROM_FN(rockn3)

struct BurnDriver BurnDrvRockn3 = {
	"rockn3", NULL, NULL, NULL, "1999",
	"Rock'n 3 (Japan)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rockn3RomInfo, rockn3RomName, NULL, NULL, RocknInputInfo, RocknDIPInfo,
	Rockn3Init, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};


// Rock'n 4 (Japan, prototype)

static struct BurnRomInfo rockn4RomDesc[] = {
	{ "rock_n_4_vj-98344_1.bin",	0x080000, 0xc666caea, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rock_n_4_vj-98344_4.bin",	0x080000, 0xcc94e557, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rock_n_4_vj-98344_8.bin",	0x200000, 0x5eeae537, 0x02 | BRF_GRA },           //  2 Sprites
	{ "rock_n_4_vj-98344_9.bin",	0x200000, 0x3fedddc9, 0x02 | BRF_GRA },           //  3

	{ "rock_n_4_vj-98344_13.bin",	0x200000, 0xead41e79, 0x03 | BRF_GRA },           //  4 Background and Rotation tiles
	{ "rock_n_4_vj-98344_6.bin",	0x200000, 0xeb16fc67, 0x03 | BRF_GRA },           //  5

	{ "rock_n_4_vj-98344_10.bin",	0x100000, 0x37d50259, 0x04 | BRF_GRA },           //  6 Foreground Tiles

	{ "sound00",			0x400000, 0x918ea8eb, 0x05 | BRF_SND },           //  7 YMZ280b Samples
	{ "sound01",			0x400000, 0xc548e51e, 0x05 | BRF_SND },           //  8
	{ "sound02",			0x400000, 0xffda0253, 0x05 | BRF_SND },           //  9
	{ "sound03",			0x400000, 0x1f813af5, 0x05 | BRF_SND },           // 10
	{ "sound04",			0x400000, 0x035c4ff3, 0x05 | BRF_SND },           // 11
	{ "sound05",			0x400000, 0x0f01f7b0, 0x05 | BRF_SND },           // 12
	{ "sound06",			0x400000, 0x31574b1c, 0x05 | BRF_SND },           // 13
	{ "sound07",			0x400000, 0x388e2c91, 0x05 | BRF_SND },           // 14
	{ "sound08",			0x400000, 0x6e7e3f23, 0x05 | BRF_SND },           // 15
	{ "sound09",			0x400000, 0x39fa512f, 0x05 | BRF_SND },           // 16
};

STD_ROM_PICK(rockn4)
STD_ROM_FN(rockn4)

struct BurnDriver BurnDrvRockn4 = {
	"rockn4", NULL, NULL, NULL, "2000",
	"Rock'n 4 (Japan, prototype)\0", NULL, "Jaleco / PCCWJ", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rockn4RomInfo, rockn4RomName, NULL, NULL, RocknInputInfo, RocknDIPInfo,
	Rockn4Init, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};


// Nandemo Seal Iinkai

static struct BurnRomInfo nndmsealRomDesc[] = {
	{ "1.1",			0x040000, 0x45acea25, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "3.3",			0x040000, 0x0754d96a, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mr97006-02.5",		0x200000, 0x4793f84e, 0x03 | BRF_GRA },           //  2 Background and Rotation tiles
	{ "mr97001-01.6",		0x200000, 0xdd648e8a, 0x03 | BRF_GRA },           //  3
	{ "mr97006-01.2",		0x200000, 0x32283485, 0x03 | BRF_GRA },           //  4

	{ "mr97006-04.8",		0x100000, 0x6726a25b, 0x04 | BRF_GRA },           //  5 Foreground Tiles

	{ "mr96017-04.9",		0x200000, 0xc2e7b444, 0x05 | BRF_SND },           //  6 OKI M6295 Samples
};

STD_ROM_PICK(nndmseal)
STD_ROM_FN(nndmseal)

struct BurnDriverD BurnDrvNndmseal = {
	"nndmseal", NULL, NULL, NULL, "1997",
	"Nandemo Seal Iinkai\0", NULL, "I'Max / Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nndmsealRomInfo, nndmsealRomName, NULL, NULL, NndmsealInputInfo, NndmsealDIPInfo,
	NndmsealInit, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Nandemo Seal Iinkai (Astro Boy ver.)

static struct BurnRomInfo nndmsealaRomDesc[] = {
	{ "1.ic1",			0x040000, 0x4eab8565, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "3.ic3",			0x040000, 0x054ba50f, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mr97032-02.ic5",		0x200000, 0x460f16bd, 0x03 | BRF_GRA },           //  2 Background and Rotation tiles
	{ "mr97032-01.ic2",		0x400000, 0x18c1a394, 0x03 | BRF_GRA },           //  3

	{ "mr97032-03.ic8",		0x100000, 0x5678a378, 0x04 | BRF_GRA },           //  4 Foreground Tiles

	{ "mr97016-04.ic9",		0x200000, 0xf421232b, 0x05 | BRF_SND },           //  5 OKI M6295 Samples
};

STD_ROM_PICK(nndmseala)
STD_ROM_FN(nndmseala)

struct BurnDriverD BurnDrvNndmseala = {
	"nndmseala", "nndmseal", NULL, NULL, "1997",
	"Nandemo Seal Iinkai (Astro Boy ver.)\0", NULL, "I'Max / Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nndmsealaRomInfo, nndmsealaRomName, NULL, NULL, NndmsealInputInfo, NndmsealDIPInfo,
	NndmsealaInit, DrvExit, RocknFrame, Tetrisp2Draw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};

