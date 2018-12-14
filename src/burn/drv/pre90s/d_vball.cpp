// FB Alpha Technos "U.S. Championship V'ball driver module
// Based on MAME driver by Paul "TBBle" Hampson

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvAttRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gfxset;
static INT32 scrollx;
static INT32 scrollx_store[256];
static INT32 scrolly;
static INT32 flipscreen;
static INT32 soundlatch;
static INT32 mainbank;
static INT32 bgprom_bank;
static INT32 spprom_bank;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[5];

static struct BurnInputInfo VballInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Start",	BIT_DIGITAL,	DrvJoy4 + 7,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 left"	},
	{"P3 Right",	BIT_DIGITAL,	DrvJoy4 + 0,	"p3 right"	},
	{"P3 Button 1",	BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"	},
	{"P3 Button 2",	BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"	},

	{"P4 Start",	BIT_DIGITAL,	DrvJoy5 + 7,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy5 + 2,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy5 + 1,	"p4 left"	},
	{"P4 Right",	BIT_DIGITAL,	DrvJoy5 + 0,	"p4 right"	},
	{"P4 Button 1",	BIT_DIGITAL,	DrvJoy5 + 4,	"p4 fire 1"	},
	{"P4 Button 2",	BIT_DIGITAL,	DrvJoy5 + 5,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Vball)

static struct BurnInputInfo Vball2pjInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Vball2pj)

static struct BurnDIPInfo VballDIPList[]=
{
	{0x20, 0xff, 0xff, 0x43, NULL						},
	{0x21, 0xff, 0xff, 0xbf, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x20, 0x01, 0x03, 0x02, "Easy"						},
	{0x20, 0x01, 0x03, 0x03, "Medium"					},
	{0x20, 0x01, 0x03, 0x01, "Hard"						},
	{0x20, 0x01, 0x03, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    4, "Single Player Game Time"	},
	{0x20, 0x01, 0x0c, 0x00, "1:15"						},
	{0x20, 0x01, 0x0c, 0x04, "1:30"						},
	{0x20, 0x01, 0x0c, 0x0c, "1:45"						},
	{0x20, 0x01, 0x0c, 0x08, "2:00"						},

	{0   , 0xfe, 0   ,    4, "Start Buttons (4-player)"	},
	{0x20, 0x01, 0x30, 0x00, "Normal"					},
	{0x20, 0x01, 0x30, 0x20, "Button A"					},
	{0x20, 0x01, 0x30, 0x10, "Button B"					},
	{0x20, 0x01, 0x30, 0x30, "Normal"					},

	{0   , 0xfe, 0   ,    2, "PL 1&4 (4-player)"		},
	{0x20, 0x01, 0x40, 0x40, "Normal"					},
	{0x20, 0x01, 0x40, 0x00, "Rotate 90"				},

	{0   , 0xfe, 0   ,    2, "Player Mode"				},
	{0x20, 0x01, 0x80, 0x80, "2 Players"				},
	{0x20, 0x01, 0x80, 0x00, "4 Players"				},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x21, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x21, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x21, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x21, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x21, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x21, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x21, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x21, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x21, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x21, 0x01, 0x38, 0x08, "3 Coins 1 Credits"		},
	{0x21, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x21, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x21, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x21, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x21, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x21, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x21, 0x01, 0x40, 0x00, "Off"						},
	{0x21, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x21, 0x01, 0x80, 0x00, "Off"						},
	{0x21, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Vball)

static struct BurnDIPInfo Vball2pjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfc, NULL						},
	{0x13, 0xff, 0xff, 0xbf, NULL						},

	{0   , 0xfe, 0   ,    4, "Single Player Game Time"	},
	{0x12, 0x01, 0x03, 0x00, "1:30"						},
	{0x12, 0x01, 0x03, 0x01, "1:45"						},
	{0x12, 0x01, 0x03, 0x03, "2:00"						},
	{0x12, 0x01, 0x03, 0x02, "2:15"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x0c, 0x04, "Easy"						},
	{0x12, 0x01, 0x0c, 0x00, "Medium"					},
	{0x12, 0x01, 0x0c, 0x08, "Hard"						},
	{0x12, 0x01, 0x0c, 0x0c, "Very Hard"				},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x40, 0x00, "Off"						},
	{0x13, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x80, 0x00, "Off"						},
	{0x13, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Vball2pj)

static void bankswitch(UINT8 data)
{
	mainbank = data;

	M6502MapMemory(DrvM6502ROM + (data * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void vball_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1008:
			flipscreen = ~data & 0x01;
			bgprom_bank = ((data >> 2) & 7) * 8;
			spprom_bank = ((data >> 5) & 7) * 8;
			scrollx = (scrollx & 0x00ff) + ((data << 7) & 0x100);
		return;

		case 0x1009:
			bankswitch(data & 0x01);
			gfxset = ~data  & 0x20;
			scrolly = (scrolly & 0x00ff) + ((data << 2) & 0x100);
		return;

		case 0x100a:
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x100b:
			M6502SetIRQLine(0x00, CPU_IRQSTATUS_NONE);
		return;

		case 0x100c:
			scrollx = (scrollx & 0xff00) + data;
		return;

		case 0x100d:
			soundlatch = data;
			ZetNmi();
		return;

		case 0x100e:
			scrolly = (scrolly & 0xff00) + data;
		return;
	}
}

static UINT8 vball_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			return DrvInputs[0];

		case 0x1001:
			return DrvInputs[1];

		case 0x1002:
			return (DrvInputs[2] & ~0x08) | (vblank ? 0x08 : 0);

		case 0x1003:
			return DrvDips[0];

		case 0x1004:
			return DrvDips[1];

		case 0x1005:
			return DrvInputs[3];

		case 0x1006:
			return DrvInputs[4];
	}

	return 0;
}

static void __fastcall vball_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x9800:
		case 0x9801:
		case 0x9802:
		case 0x9803:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall vball_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			return BurnYM2151Read();

		case 0x9800:
		case 0x9801:
		case 0x9802:
		case 0x9803:
			return MSM6295Read(0);

		case 0xa000:
			return soundlatch;
	}

	return 0;
}

static tilemap_scan( bg )
{
	return (col & 0x1f) + (((row & 0x1f) + (col & 0x20)) << 5) + ((row & 0x20) << 6);
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvAttRAM[offs];
	UINT16 code = DrvVidRAM[offs] + ((attr & 0x1f) * 256) + (gfxset * 256);

	TILE_SET_INFO(0, code, (attr >> 5) + bgprom_bank, 0);
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	MSM6295Reset(0);
	BurnYM2151Reset();

	gfxset = 0;
	scrollx = 0;
	scrolly = 0;
	flipscreen = 0;
	soundlatch = 0;
	memset (scrollx_store, 0, sizeof(scrollx_store));
	bgprom_bank = 0;
	spprom_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM	= Next; Next += 0x010000;
	DrvZ80ROM	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x100000;
	DrvGfxROM1	= Next; Next += 0x080000;

	DrvColPROM	= Next; Next += 0x001800;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x08000 * sizeof(UINT32);

	AllRam		= Next;

	DrvM6502RAM	= Next; Next += 0x0008000;
	DrvAttRAM	= Next; Next += 0x0010000;
	DrvVidRAM	= Next; Next += 0x00100000;
	DrvSprRAM	= Next; Next += 0x0001000;

	DrvZ80RAM	= Next; Next += 0x0008000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,2) };
	INT32 Plane1[4]  = { 0x20000*8+0, 0x20000*8+4, 0, 4 };
	INT32 XOffs0[8]  = { 0*8*8+1, 0*8*8+0, 1*8*8+1, 1*8*8+0, 2*8*8+1, 2*8*8+0, 3*8*8+1, 3*8*8+0 };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(16*8+3,-1), STEP4(32*8+3,-1), STEP4(48*8+3,-1) };
	INT32 YOffs[16]  = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x080000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x080000);

	GfxDecode(0x4000, 4,  8,  8, Plane0, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x040000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 romset)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		switch (romset)
		{
			case 0: // vball
			case 1: // vball2pj
			{
				if (BurnLoadRom(DrvM6502ROM + 0x00000,  0, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM   + 0x00000,  1, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0  + 0x00000,  2, 1)) return 1;
 
				if (BurnLoadRom(DrvGfxROM1  + 0x00000,  3, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1  + 0x20000,  4, 1)) return 1;

				if (BurnLoadRom(DrvSndROM   + 0x00000,  5, 1)) return 1;

				if (BurnLoadRom(DrvColPROM  + 0x00000,  6, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x00800,  7, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x01000,  8, 1)) return 1;
			}
			break;

			case 2: // vballb
			{
				if (BurnLoadRom(DrvM6502ROM + 0x00000,  0, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM   + 0x00000,  1, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0  + 0x00000,  2, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x10000,  3, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x20000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x30000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x40000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x50000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x60000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x70000,  9, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1  + 0x00000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1  + 0x20000, 11, 1)) return 1;

				if (BurnLoadRom(DrvSndROM   + 0x00000, 12, 1)) return 1;
				if (BurnLoadRom(DrvSndROM   + 0x10000, 13, 1)) return 1;

				if (BurnLoadRom(DrvColPROM  + 0x00000, 14, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x00800, 15, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x01000, 16, 1)) return 1;
			}
			break;

			case 3: // vball2pjb
			{
				if (BurnLoadRom(DrvM6502ROM + 0x00000,  0, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM   + 0x00000,  1, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0  + 0x00000,  2, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x10000,  3, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x20000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x30000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x40000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x50000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x60000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0  + 0x70000,  9, 1)) return 1;
 
				if (BurnLoadRom(DrvGfxROM1  + 0x00000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1  + 0x10000, 11, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1  + 0x20000, 12, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1  + 0x30000, 13, 1)) return 1;

				if (BurnLoadRom(DrvSndROM   + 0x00000, 14, 1)) return 1;
				if (BurnLoadRom(DrvSndROM   + 0x10000, 15, 1)) return 1;

				if (BurnLoadRom(DrvColPROM  + 0x00000, 16, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x00800, 17, 1)) return 1;
				if (BurnLoadRom(DrvColPROM  + 0x01000, 18, 1)) return 1;
			}
			break;
		}

		for (INT32 i = 0; i < 0x1800; i++) { // convert xxxxCCCC to CCCCCCCC
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[i] << 4);
		}
		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x0800, 0x08ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvAttRAM,				0x3000, 0x3fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(vball_main_write);
	M6502SetReadHandler(vball_main_read);
	M6502Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,					0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,					0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(vball_sound_write);
	ZetSetReadHandler(vball_sound_read);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x100000, 0, 0x3f);
	GenericTilemapSetOffsets(0, -4, 8);
	GenericTilemapSetScrollRows(0, 240);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	M6502Exit();
	ZetExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x800; i++)
	{
		UINT8 r = DrvColPROM[i];
		UINT8 g = DrvColPROM[i + 0x0800];
		UINT8 b = DrvColPROM[i + 0x1000];

		DrvPalette[i] = BurnHighCol(r, g, b,0);
	}
}	

static inline void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x400, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x400, DrvGfxROM1);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x400, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x400, DrvGfxROM1);
		}
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x100; i += 4)
	{
		INT32 attr	= DrvSprRAM[i + 1];
		INT32 code	= DrvSprRAM[i + 2] + ((attr & 0x07) * 256);
		INT32 sx	= ((DrvSprRAM[i + 3] + 8) & 0xff) - 7;
		INT32 sy	= (240 - DrvSprRAM[i]) - 8;
		INT32 size	= attr >> 7;
		INT32 color	= ((attr & 0x38) >> 3) + spprom_bank;
		INT32 flipx	= ~attr & 0x40;
		INT32 flipy	= 0;
		INT32 dy	= -16;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
			dy = -dy;
		}

		switch (size)
		{
			case 0:
				draw_single_sprite(code + 0, color, sx, sy +  0, flipx, flipy);
			break;

			case 1:
				draw_single_sprite(code + 0, color, sx, sy + dy, flipx, flipy);
				draw_single_sprite(code + 1, color, sx, sy +  0, flipx, flipy);
			break;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

//	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	GenericTilemapSetScrollY(0, scrolly);

	for (INT32 i = 0; i < 240; i++) {
		GenericTilemapSetScrollRow(0, i, scrollx_store[i]);
	}

	GenericTilemapDraw(0, pTransDraw, 0);
	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline INT32 scanline_to_vcount(INT32 scanline) // ripped directly from MAME
{
	INT32 vcount = scanline + 8;

	if (vcount < 0x100)
		return vcount;
	else
		return (vcount - 0x18) | 0x100;
}

static void do_scanline(INT32 scanline) // ripped directly from MAME
{
	INT32 screen_height = 240;
	INT32 vcount_old = scanline_to_vcount((scanline == 0) ? screen_height - 1 : scanline - 1);
	INT32 vcount = scanline_to_vcount(scanline);

	if (!(vcount_old & 8) && (vcount & 8))
	{
		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}

	if (vcount == 0xf8)
	{
		M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
	}

	vblank = (vcount >= (248 - 1)) ? 1 : 0; // -1 is a hack

	if (scanline < 256) scrollx_store[scanline] = scrollx;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 5);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 272;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 2000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	M6502Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6502Run((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		do_scanline(i);

		nCyclesDone[1] += ZetRun((nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1]);

		if (pBurnSoundOut && i&1) {
			nSegment = nBurnSoundLen / (nInterleave / 2);

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();
	M6502Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(gfxset);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrollx_store);
		SCAN_VAR(scrolly);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(mainbank);
		SCAN_VAR(bgprom_bank);
		SCAN_VAR(spprom_bank);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(mainbank);
		M6502Close();
	}

	return 0;
}

// U.S. Championship V'ball (US)

static struct BurnRomInfo vballRomDesc[] = {
	{ "25a2-4.124",		0x10000, 0x06d0c013, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "25j1-0.47",		0x08000, 0x10ca79ad, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "trj-101.96",		0x80000, 0xf343eee4, 3 | BRF_GRA },           //  2 Background Tiles

	{ "25j4-0.35",		0x20000, 0x877826d8, 4 | BRF_GRA },           //  3 Sprites
	{ "25j3-0.5",		0x20000, 0xc6afb4fa, 4 | BRF_GRA },           //  4

	{ "25j0-0.78",		0x20000, 0x8e04bdbf, 5 | BRF_SND },           //  5 Samples

	{ "25j5-0.144",		0x00800, 0xa317240f, 6 | BRF_GRA },           //  6 Color data
	{ "25j6-0.143",		0x00800, 0x1ff70b4f, 6 | BRF_GRA },           //  7
	{ "25j7-0.160",		0x00800, 0x2ffb68b3, 6 | BRF_GRA },           //  8
};

STD_ROM_PICK(vball)
STD_ROM_FN(vball)

static INT32 VballInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvVball = {
	"vball", NULL, NULL, NULL, "1988",
	"U.S. Championship V'ball (US)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, vballRomInfo, vballRomName, NULL, NULL, NULL, NULL, VballInputInfo, VballDIPInfo,
	VballInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// U.S. Championship V'ball (Japan)

static struct BurnRomInfo vball2pjRomDesc[] = {
	{ "25j2-2-5.124",	0x10000, 0x432509c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "25j1-0.47",		0x08000, 0x10ca79ad, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "trj-101.96",		0x80000, 0xf343eee4, 3 | BRF_GRA },           //  2 Background Tiles

	{ "25j4-0.35",		0x20000, 0x877826d8, 4 | BRF_GRA },           //  3 Sprites
	{ "25j3-0.5",		0x20000, 0xc6afb4fa, 4 | BRF_GRA },           //  4

	{ "25j0-0.78",		0x20000, 0x8e04bdbf, 5 | BRF_SND },           //  5 Samples

	{ "25j5-0.144",		0x00800, 0xa317240f, 6 | BRF_GRA },           //  6 Color data
	{ "25j6-0.143",		0x00800, 0x1ff70b4f, 6 | BRF_GRA },           //  7
	{ "25j7-0.160",		0x00800, 0x2ffb68b3, 6 | BRF_GRA },           //  8
};

STD_ROM_PICK(vball2pj)
STD_ROM_FN(vball2pj)

static INT32 Vball2pjInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvVball2pj = {
	"vball2pj", "vball", NULL, NULL, "1988",
	"U.S. Championship V'ball (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, vball2pjRomInfo, vball2pjRomName, NULL, NULL, NULL, NULL, Vball2pjInputInfo, Vball2pjDIPInfo,
	Vball2pjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// U.S. Championship V'ball (bootleg of US set)

static struct BurnRomInfo vballbRomDesc[] = {
	{ "vball.124",		0x10000, 0xbe04c2b5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "25j1-0.47",		0x08000, 0x10ca79ad, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "13",				0x10000, 0xf26df8e1, 3 | BRF_GRA },           //  2 Background Tiles
	{ "14",				0x10000, 0xc9798d0e, 3 | BRF_GRA },           //  3
	{ "15",				0x10000, 0x68e69c4b, 3 | BRF_GRA },           //  4
	{ "16",				0x10000, 0x936457ba, 3 | BRF_GRA },           //  5
	{ "9",				0x10000, 0x42874924, 3 | BRF_GRA },           //  6
	{ "10",				0x10000, 0x6cc676ee, 3 | BRF_GRA },           //  7
	{ "11",				0x10000, 0x4754b303, 3 | BRF_GRA },           //  8
	{ "12",				0x10000, 0x21294a84, 3 | BRF_GRA },           //  9

	{ "vball.35",		0x20000, 0x877826d8, 4 | BRF_GRA },           // 10 Sprites
	{ "vball.5",		0x20000, 0xc6afb4fa, 4 | BRF_GRA },           // 11

	{ "vball.78a",		0x10000, 0xf3e63b76, 5 | BRF_SND },           // 12 Samples
	{ "vball.78b",		0x10000, 0x7ad9d338, 5 | BRF_SND },           // 13

	{ "25j5-0.144",		0x00800, 0xa317240f, 6 | BRF_GRA },           // 14 Color data
	{ "25j6-0.143",		0x00800, 0x1ff70b4f, 6 | BRF_GRA },           // 15
	{ "25j7-0.160",		0x00800, 0x2ffb68b3, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(vballb)
STD_ROM_FN(vballb)

static INT32 VballbInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvVballb = {
	"vballb", "vball", NULL, NULL, "1988",
	"U.S. Championship V'ball (bootleg of US set)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, vballbRomInfo, vballbRomName, NULL, NULL, NULL, NULL, VballInputInfo, VballDIPInfo,
	VballbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// U.S. Championship V'ball (bootleg of Japan set)

static struct BurnRomInfo vball2pjbRomDesc[] = {
	{ "1.124",			0x10000, 0x432509c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "4.ic47",			0x08000, 0x534dfbd9, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "13",				0x10000, 0xf26df8e1, 3 | BRF_GRA },           //  2 Background Tiles
	{ "14",				0x10000, 0xc9798d0e, 3 | BRF_GRA },           //  3
	{ "15",				0x10000, 0x68e69c4b, 3 | BRF_GRA },           //  4
	{ "16",				0x10000, 0x936457ba, 3 | BRF_GRA },           //  5
	{ "9",				0x10000, 0x42874924, 3 | BRF_GRA },           //  6
	{ "10",				0x10000, 0x6cc676ee, 3 | BRF_GRA },           //  7
	{ "11",				0x10000, 0x4754b303, 3 | BRF_GRA },           //  8
	{ "12",				0x10000, 0x21294a84, 3 | BRF_GRA },           //  9

	{ "8",				0x10000, 0xb18d083c, 4 | BRF_GRA },           // 10 Sprites
	{ "7",				0x10000, 0x79a35321, 4 | BRF_GRA },           // 11
	{ "6",				0x10000, 0x49c6aad7, 4 | BRF_GRA },           // 12
	{ "5",				0x10000, 0x9bb95651, 4 | BRF_GRA },           // 13

	{ "vball.78a",		0x10000, 0xf3e63b76, 5 | BRF_SND },           // 14 Samples
	{ "3.ic79",			0x08000, 0xd77349ba, 5 | BRF_SND },           // 15

	{ "25j5-0.144",		0x00800, 0xa317240f, 6 | BRF_GRA },           // 16 Color data
	{ "25j6-0.143",		0x00800, 0x1ff70b4f, 6 | BRF_GRA },           // 17
	{ "25j7-0.160",		0x00800, 0x2ffb68b3, 6 | BRF_GRA },           // 18
};

STD_ROM_PICK(vball2pjb)
STD_ROM_FN(vball2pjb)

static INT32 Vball2pjbInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvVball2pjb = {
	"vball2pjb", "vball", NULL, NULL, "1988",
	"U.S. Championship V'ball (bootleg of Japan set)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, vball2pjbRomInfo, vball2pjbRomName, NULL, NULL, NULL, NULL, VballInputInfo, VballDIPInfo,
	Vball2pjbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};
