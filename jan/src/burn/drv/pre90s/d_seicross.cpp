// FB Alpha Frisky Tom / Seicross / Radical Radial driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "dac.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMCUOps;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 irq_mask;
static UINT8 flipscreen;
static UINT8 portb_data;
static UINT8 mcu_halt;
static UINT8 watchdog;

static INT32 game_select = 0;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvDips[5];
static UINT8  DrvInputs[6];
static UINT8  DrvReset;

static struct BurnInputInfo FriskytInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Friskyt)

static struct BurnInputInfo RadradInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Radrad)

static struct BurnInputInfo SeicrossInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",		BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
};

STDINPUTINFO(Seicross)

static struct BurnDIPInfo FriskytDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL			},
	{0x0f, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    2, "Counter Check"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "Test Mode"		},
	{0x0f, 0x01, 0x01, 0x00, "Off"			},
	{0x0f, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Connection Error"	},
	{0x0f, 0x01, 0x02, 0x00, "Off"			},
	{0x0f, 0x01, 0x02, 0x02, "On"			},
};

STDDIPINFO(Friskyt)

static struct BurnDIPInfo RadradDIPList[]=
{
	{0x12, 0xff, 0xff, 0xf3, NULL			},
	{0x13, 0xff, 0xff, 0xf1, NULL			},
	{0x14, 0xff, 0xff, 0xf0, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x06, 0x00, "2"			},
	{0x12, 0x01, 0x06, 0x02, "3"			},
	{0x12, 0x01, 0x06, 0x04, "4"			},
	{0x12, 0x01, 0x06, 0x06, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    15, "Coin A"		},
	{0x13, 0x01, 0x0f, 0x07, "7 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "6 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "7 Coins/2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "6 Coins/2 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "5 Coins/2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "4 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "2 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    15, "Coin B"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "2 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "2 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "2 Coins 6 Credits"	},
	{0x14, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "2 Coins 7 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "2 Coins 8 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},
};

STDDIPINFO(Radrad)

static struct BurnDIPInfo SeicrossDIPList[]=
{
	{0x10, 0xff, 0xff, 0xfc, NULL			},
	{0x11, 0xff, 0xff, 0xf0, NULL			},
	{0x12, 0xff, 0xff, 0xf1, NULL			},
	{0x13, 0xff, 0xff, 0xf0, NULL			},
	{0x14, 0xff, 0xff, 0xe0, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Connection Error"	},
	{0x10, 0x01, 0x02, 0x00, "Off"			},
	{0x10, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x01, 0x00, "Off"			},
	{0x11, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x02, 0x00, "Off"			},
	{0x11, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x0c, 0x00, "20000 40000"		},
	{0x11, 0x01, 0x0c, 0x04, "30000"		},
	{0x11, 0x01, 0x0c, 0x08, "30000 50000"		},
	{0x11, 0x01, 0x0c, 0x0c, "30000 60000 90000"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x02, 0x00, "Easy"			},
	{0x12, 0x01, 0x02, 0x02, "Hard"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x0c, 0x08, "2"			},
	{0x12, 0x01, 0x0c, 0x00, "3"			},
	{0x12, 0x01, 0x0c, 0x04, "4"			},
	{0x12, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0x03, 0x03, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x02, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x13, 0x01, 0x20, 0x20, "Off"			},
	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Seicross)

static void __fastcall seicross_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x9c00) {
		DrvColRAM[address & 0x3df] = data;
		DrvColRAM[(address & 0x3ff)| 0x20] = data;
		return;
	}
}

static UINT8 __fastcall seicross_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];

		case 0xa800:
			return DrvInputs[1];

		case 0xb000:
			return DrvInputs[2];

		case 0xb800:
			watchdog = 0;
			return 0;
	}

	return 0;
}

static void __fastcall seicross_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf7) // mirror 0x08
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall seicross_main_read_port(UINT16 port)
{
	switch (port & 0xf7) // mirror 0x08
	{
		case 0x04:
			return AY8910Read(0);
	}

	return 0;
}

static void seicross_mcu_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			DACWrite(0, data);
		return;
	}
}

static UINT8 seicross_mcu_read(UINT16 address)
{
	switch (address)
	{
		case 0x1003:
			return DrvInputs[3];

		case 0x1005:
			return DrvInputs[4];

		case 0x1006:
			return DrvInputs[5];
	}

	return 0;
}

static UINT8 ay8910_read_B(UINT32)
{
	return (portb_data & 0x9f) | (DrvDips[4] & 0x60);
}

static void ay8910_write_B(UINT32, UINT32 data)
{
	irq_mask = data & 1;

	flipscreen = data & 2;

	if (((portb_data & 4) == 0) && (data & 4))
	{
		NSC8105Reset();
		mcu_halt = 0;
	}

	portb_data = data;
}


static INT32 syncronize_dac()
{
	return (INT32)(float)(nBurnSoundLen * (nM6800CyclesTotal / (3072000.000 / 60)));
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	NSC8105Reset();
	mcu_halt = 1;

	AY8910Reset(0);
	DACReset();

	static const UINT8 nvram_data[32] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
		0, 1, 0, 1, 0, 1, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0
	};

	memset (DrvNVRAM, 0, 0x100);
	memcpy (DrvNVRAM, nvram_data, 32);

	watchdog = 0;
	irq_mask = 0;
	flipscreen = 0;
	portb_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;
	DrvMCUOps		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000040;

	DrvNVRAM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvMCURAM		= Next; Next += 0x000100;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100; // 8820-887f
	DrvVidRegs		= Next; Next += 0x000100; // 9800-981f scroll, 9880-989f spr2

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { 0, 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(16*8,1), STEP4(17*8,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(16*16,16) };
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);
	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_select = select;

	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000,   0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000,   1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000,   2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x3000,   3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000,   4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x5000,   5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x6000,   6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x7000,   7, 1)) return 1;

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "radradj") ==0) {
			if (BurnLoadRom(DrvZ80ROM + 0x7000,   7, 1)) return 1;
			memcpy (DrvZ80ROM + 0x7800, DrvGfxROM0 + 0x7000, 0x0800);
		}
		
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x3000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 13, 1)) return 1;

		if (game_select == 1) {
			for (INT32 i = 0; i < 0x7800; i++) {
				DrvMCUOps[i] = BITSWAP08(DrvZ80ROM[i], 6, 7, 5, 4, 3, 2, 0, 1);
			}
		}
				
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x8800, 0x88ff, MAP_RAM); // 8820-887f
	ZetMapMemory(DrvVidRAM,		0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRegs,	0x9800, 0x98ff, MAP_RAM); // 9800-981f scroll, 9880-989f spr2
	ZetMapMemory(DrvColRAM,		0x9c00, 0x9fff, MAP_READ);
	ZetSetWriteHandler(seicross_main_write);
	ZetSetReadHandler(seicross_main_read);
	ZetSetOutHandler(seicross_main_write_port);
	ZetSetInHandler(seicross_main_read_port);
	ZetClose();

	NSC8105Init(1);
	NSC8105MapMemory(DrvMCURAM,	0x0000, 0x00ff, MAP_RAM); // 0-7f
	if (game_select < 2)
		NSC8105MapMemory(DrvNVRAM,	0x1000, 0x10ff, MAP_RAM);
	NSC8105MapMemory(DrvZ80ROM,	0x8000, 0xf7ff, MAP_ROM);
	if (game_select == 1)
		NSC8105MapMemory(DrvMCUOps,	0x8000, 0xf7ff, MAP_FETCH);
	NSC8105MapMemory(DrvShareRAM,	0xf800, 0xffff, MAP_RAM);
	NSC8105SetWriteHandler(seicross_mcu_write);
	NSC8105SetReadHandler(seicross_mcu_read);

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, &ay8910_read_B, NULL, &ay8910_write_B);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, syncronize_dac);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	NSC8105Exit();

	AY8910Exit(0);
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sy -= (DrvVidRegs[sx/8] + 16) & 0xff;
		if (sy < -7) sy += 256;

		INT32 attr = DrvColRAM[offs];
		INT32 code = DrvVidRAM[offs] + ((attr & 0x10) << 4);
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_single_sprite(INT32 code, INT32 sx, INT32 sy, INT32 color, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0x20; offs -= 4)
	{
		INT32 attr = DrvSprRAM[offs + 1];
		INT32 sx   = DrvSprRAM[offs + 3];
		INT32 sy   = 240 - DrvSprRAM[offs + 2];
		INT32 code =(DrvSprRAM[offs + 0] & 0x3f) + ((attr & 0x10) << 2) + 128;
		INT32 color = attr & 0x0f;
		INT32 flipx = DrvSprRAM[offs + 0] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 0] & 0x80;

		if (nBurnLayer & 4) draw_single_sprite(code, sx, sy, color, flipx, flipy);
		if (nBurnLayer & 4) if (sx > 0xf0) draw_single_sprite(code, sx - 256, sy, color, flipx, flipy);
	}

	for (INT32 offs = 0xA0 - 4; offs >= 0x80; offs -= 4)
	{
		INT32 attr = DrvVidRegs[offs + 1];
		INT32 sx   = DrvVidRegs[offs + 3];
		INT32 sy   = 240 - DrvVidRegs[offs + 2];
		INT32 code =(DrvVidRegs[offs + 0] & 0x3f) + ((attr & 0x10) << 2);
		INT32 color = attr & 0x0f;
		INT32 flipx = DrvVidRegs[offs + 0] & 0x40;
		INT32 flipy = DrvVidRegs[offs + 0] & 0x80;

		if (nBurnLayer & 8) draw_single_sprite(code, sx, sy, color, flipx, flipy);
		if (nBurnLayer & 8) if (sx > 0xf0) draw_single_sprite(code, sx - 256, sy, color, flipx, flipy);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (DrvReset || watchdog >= 180) {
		DrvDoReset((watchdog < 180) ? 1 : 0);
	}

	ZetNewFrame();
	NSC8105NewFrame();

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		switch (game_select)
		{
			case 0: // friskyt, friskyta
			case 1: // friskytb
				DrvInputs[1] = (DrvDips[0] & 0x80) | (DrvInputs[1] & 0x7f);
				DrvInputs[2] = DrvDips[1];
			break;

			case 2: // radrad
				DrvInputs[3] = DrvDips[0];
				DrvInputs[4] = DrvDips[1];
				DrvInputs[5] = DrvDips[2];
			break;

			case 3: // seicross / sectrzon
				DrvInputs[2] = DrvDips[0];
				DrvInputs[3] = DrvDips[1];
				DrvInputs[4] = DrvDips[2];
				DrvInputs[5] = DrvDips[3];
			break;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 3072000 / 60;
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 240 && irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		if (mcu_halt) {
			nCyclesDone[1] = ZetTotalCycles();
			nM6800CyclesTotal += ZetTotalCycles() - nM6800CyclesTotal;
		} else {
			nCyclesDone[1] += NSC8105Run(ZetTotalCycles() - nM6800CyclesTotal);
		}

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}

	}

	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		NSC8105Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(mcu_halt);
		SCAN_VAR(irq_mask);
		SCAN_VAR(flipscreen);
		SCAN_VAR(portb_data);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x0100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Frisky Tom (set 1)

static struct BurnRomInfo friskytRomDesc[] = {
	{ "ftom.01",	0x1000, 0xbce5d486, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "ftom.02",	0x1000, 0x63157d6e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ftom.03",	0x1000, 0xc8d9ef2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ftom.04",	0x1000, 0x23a01aac, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ftom.05",	0x1000, 0xbfaf702a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ftom.06",	0x1000, 0xbce70b9c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ftom.07",	0x1000, 0xb2ef303a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ft8_8.rom",	0x0800, 0x10461a24, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ftom.11",	0x1000, 0x1ec6ff65, 2 | BRF_GRA },           //  8 Graphics
	{ "ftom.12",	0x1000, 0x3b8f40b5, 2 | BRF_GRA },           //  9
	{ "ftom.09",	0x1000, 0x60642f25, 2 | BRF_GRA },           // 10
	{ "ftom.10",	0x1000, 0x07b9dcfc, 2 | BRF_GRA },           // 11

	{ "ft.9c",	0x0020, 0x0032167e, 3 | BRF_GRA },           // 12 Color Data
	{ "ft.9b",	0x0020, 0x6b364e69, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(friskyt)
STD_ROM_FN(friskyt)

static INT32 friskytInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvFriskyt = {
	"friskyt", NULL, NULL, NULL, "1981",
	"Frisky Tom (set 1)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, friskytRomInfo, friskytRomName, NULL, NULL, FriskytInputInfo, FriskytDIPInfo,
	friskytInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Frisky Tom (set 2)

static struct BurnRomInfo friskytaRomDesc[] = {
	{ "ft.01",	0x1000, 0x0ea46e19, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "ft.02",	0x1000, 0x4f7b8662, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ft.03",	0x1000, 0x1eb1b77c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ft.04",	0x1000, 0xb5c5400d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ft.05",	0x1000, 0xb465be8a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ft.06",	0x1000, 0x90141317, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ft.07",	0x1000, 0x0ba02b2e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ft8_8.rom",	0x0800, 0x10461a24, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ft.11",	0x1000, 0x956d924a, 2 | BRF_GRA },           //  8 Graphics
	{ "ft.12",	0x1000, 0xc028d3b8, 2 | BRF_GRA },           //  9
	{ "ftom.09",	0x1000, 0x60642f25, 2 | BRF_GRA },           // 10
	{ "ftom.10",	0x1000, 0x07b9dcfc, 2 | BRF_GRA },           // 11

	{ "ft.9c",	0x0020, 0x0032167e, 3 | BRF_GRA },           // 12 Color Data
	{ "ft.9b",	0x0020, 0x6b364e69, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(friskyta)
STD_ROM_FN(friskyta)

struct BurnDriver BurnDrvFriskyta = {
	"friskyta", "friskyt", NULL, NULL, "1981",
	"Frisky Tom (set 2)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, friskytaRomInfo, friskytaRomName, NULL, NULL, FriskytInputInfo, FriskytDIPInfo,
	friskytInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Frisky Tom (set 3, encrypted)

static struct BurnRomInfo friskytbRomDesc[] = {
	{ "1.3a",	0x1000, 0x554bdb0f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code (Encrypted)
	{ "2.3b",	0x1000, 0x0658633a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.3d",	0x1000, 0xc8de15ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.3e",	0x1000, 0x970e5d2b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.3f",	0x1000, 0x45c8bd32, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.3h",	0x1000, 0x2c1b7ecc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.3i",	0x1000, 0xaa36a6b8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.3j",	0x0800, 0x10461a24, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11.7l",	0x1000, 0xcaa93315, 2 | BRF_GRA },           //  8 Graphics
	{ "12.7n",	0x1000, 0xc028d3b8, 2 | BRF_GRA },           //  9
	{ "9.7h",	0x1000, 0x60642f25, 2 | BRF_GRA },           // 10
	{ "10.7j",	0x1000, 0x07b9dcfc, 2 | BRF_GRA },           // 11

	{ "ft.9c",	0x0020, 0x0032167e, 3 | BRF_GRA },           // 12 Color Data
	{ "ft.9b",	0x0020, 0x6b364e69, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(friskytb)
STD_ROM_FN(friskytb)

static INT32 friskytbInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvFriskytb = {
	"friskytb", "friskyt", NULL, NULL, "1981",
	"Frisky Tom (set 3, encrypted)\0", "Broken, please use parent romset!", "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, friskytbRomInfo, friskytbRomName, NULL, NULL, FriskytInputInfo, FriskytDIPInfo,
	friskytbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Radical Radial (US)

static struct BurnRomInfo radradRomDesc[] = {
	{ "1.3a",	0x1000, 0xb1e958ca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "2.3b",	0x1000, 0x30ba76b3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.3c",	0x1000, 0x1c9f397b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.3d",	0x1000, 0x453966a3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.3e",	0x1000, 0xc337c4bd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.3f",	0x1000, 0x06e15b59, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.3g",	0x1000, 0x02b1f9c9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.3h",	0x0800, 0x911c90e8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11.l7",	0x1000, 0x4ace7afb, 2 | BRF_GRA },           //  8 Graphics
	{ "12.n7",	0x1000, 0xb19b8473, 2 | BRF_GRA },           //  9
	{ "9.j7",	0x1000, 0x229939a3, 2 | BRF_GRA },           // 10
	{ "10.j7",	0x1000, 0x79237913, 2 | BRF_GRA },           // 11

	{ "clr.9c",	0x0020, 0xc9d88422, 3 | BRF_GRA },           // 12 Color Data
	{ "clr.9b",	0x0020, 0xee81af16, 3 | BRF_GRA },           // 13

	{ "pal16h2.2b",	0x0044, 0xa356803a, 0 | BRF_OPT },           // 14 plds
};

STD_ROM_PICK(radrad)
STD_ROM_FN(radrad)

static INT32 radradInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvRadrad = {
	"radrad", NULL, NULL, NULL, "1982",
	"Radical Radial (US)\0", NULL, "Nichibutsu USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, radradRomInfo, radradRomName, NULL, NULL, RadradInputInfo, RadradDIPInfo,
	radradInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Radical Radial (Japan)
// Top and bottom PCBs have Nihon Bussan etched and the top PCB has a Nichibutsu sticker

static struct BurnRomInfo radradjRomDesc[] = {
	{ "1.3a",	0x1000, 0xb1e958ca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "2.3b",	0x1000, 0x30ba76b3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.3d",	0x1000, 0x1c9f397b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.3d",	0x1000, 0x453966a3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.3f",	0x1000, 0xc337c4bd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.3h",	0x1000, 0x06e15b59, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.3i",	0x1000, 0x02b1f9c9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.3j",	0x1000, 0xbc9c7fae, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11.7k",	0x1000, 0xc75b96da, 2 | BRF_GRA },           //  8 Graphics
	{ "12.7m",	0x1000, 0x83f35c05, 2 | BRF_GRA },           //  9
	{ "9.7h",	0x1000, 0xf2da3954, 2 | BRF_GRA },           // 10
	{ "10.7j",	0x1000, 0x79237913, 2 | BRF_GRA },           // 11

	{ "clr.9c",	0x0020, 0xc9d88422, 3 | BRF_GRA },           // 12 Color Data
	{ "clr.9b",	0x0020, 0xee81af16, 3 | BRF_GRA },           // 13

	{ "pal16h2.2b",	0x0044, 0xa356803a, 0 | BRF_OPT },           // 14 plds
};

STD_ROM_PICK(radradj)
STD_ROM_FN(radradj)

struct BurnDriver BurnDrvRadradj = {
	"radradj", "radrad", NULL, NULL, "1982",
	"Radical Radial (Japan)\0", NULL, "Logitec Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, radradjRomInfo, radradjRomName, NULL, NULL, RadradInputInfo, RadradDIPInfo,
	radradInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Seicross

static struct BurnRomInfo seicrossRomDesc[] = {
	{ "smc1",	0x1000, 0xf6c3aeca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "smc2",	0x1000, 0x0ec6c218, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "smc3",	0x1000, 0xceb3c8f4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "smc4",	0x1000, 0x3112af59, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "smc5",	0x1000, 0xb494a993, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "smc6",	0x1000, 0x09d5b9da, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "smc7",	0x1000, 0x13052b03, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "smc8",	0x0800, 0x2093461d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sz11.7k",	0x1000, 0xfbd9b91d, 2 | BRF_GRA },           //  8 Graphics
	{ "smcd",	0x1000, 0xc3c953c4, 2 | BRF_GRA },           //  9
	{ "sz9.7j",	0x1000, 0x4819f0cd, 2 | BRF_GRA },           // 10
	{ "sz10.7h",	0x1000, 0x4c268778, 2 | BRF_GRA },           // 11

	{ "sz73.10c",	0x0020, 0x4d218a3c, 3 | BRF_GRA },           // 12 Color Data
	{ "sz74.10b",	0x0020, 0xc550531c, 3 | BRF_GRA },           // 13

	{ "pal16h2.3b",	0x0044, 0xe1a6a86d, 0 | BRF_OPT },           // 14 PLDs
};

STD_ROM_PICK(seicross)
STD_ROM_FN(seicross)

static INT32 seicrossInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvSeicross = {
	"seicross", NULL, NULL, NULL, "1984",
	"Seicross\0", NULL, "Nichibutsu / Alice", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, seicrossRomInfo, seicrossRomName, NULL, NULL, SeicrossInputInfo, SeicrossDIPInfo,
	seicrossInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Sector Zone

static struct BurnRomInfo sectrzonRomDesc[] = {
	{ "sz1.3a",	0x1000, 0xf0a45cb4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 & NSC8105 Code
	{ "sz2.3c",	0x1000, 0xfea68ddb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sz3.3d",	0x1000, 0xbaad4294, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sz4.3e",	0x1000, 0x75f2ca75, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sz5.3fg",	0x1000, 0xdc14f2c8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sz6.3h",	0x1000, 0x397a38c5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sz7.3i",	0x1000, 0x7b34dc1c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sz8.3j",	0x0800, 0x9933526a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sz11.7k",	0x1000, 0xfbd9b91d, 2 | BRF_GRA },           //  8 Graphics
	{ "sz12.7m",	0x1000, 0x2bdef9ad, 2 | BRF_GRA },           //  9
	{ "sz9.7j",	0x1000, 0x4819f0cd, 2 | BRF_GRA },           // 10
	{ "sz10.7h",	0x1000, 0x4c268778, 2 | BRF_GRA },           // 11

	{ "sz73.10c",	0x0020, 0x4d218a3c, 3 | BRF_GRA },           // 12 Color Data
	{ "sz74.10b",	0x0020, 0xc550531c, 3 | BRF_GRA },           // 13

	{ "pal16h2.3b",	0x0044, 0xe1a6a86d, 0 | BRF_OPT },           // 14 PLDs
};

STD_ROM_PICK(sectrzon)
STD_ROM_FN(sectrzon)

struct BurnDriver BurnDrvSectrzon = {
	"sectrzon", "seicross", NULL, NULL, "1984",
	"Sector Zone\0", NULL, "Nichibutsu / Alice", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sectrzonRomInfo, sectrzonRomName, NULL, NULL, SeicrossInputInfo, SeicrossDIPInfo,
	seicrossInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
