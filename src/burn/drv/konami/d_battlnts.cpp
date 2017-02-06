// FB Alpha Battlantis driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "z80_intf.h"
#include "k007342_k007420.h"
#include "burn_ym3812.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;

static UINT8 HD6309Bank;
static UINT8 soundlatch;
static INT32 spritebank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvReset;

static UINT8 DrvInputs[3];

static INT32 watchdog;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo BattlntsDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL			},
	{0x11, 0xff, 0xff, 0xe0, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x5a, NULL			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x10, 0x01, 0x80, 0x80, "3 Times"		},
	{0x10, 0x01, 0x80, 0x00, "5 Times"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x11, 0x01, 0x40, 0x40, "Single"		},
	{0x11, 0x01, 0x40, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    16, "Free Play"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, ":1,2,3,4"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "2"			},
	{0x13, 0x01, 0x03, 0x02, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "30k And Every 70k"	},
	{0x13, 0x01, 0x18, 0x10, "40k And Every 80k"	},
	{0x13, 0x01, 0x18, 0x08, "40k"			},
	{0x13, 0x01, 0x18, 0x00, "50k"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Difficult"		},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Battlnts)

static struct BurnDIPInfo RackemupDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL			},
	{0x11, 0xff, 0xff, 0xe0, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x52, NULL			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x10, 0x01, 0x80, 0x80, "3 Times"		},
	{0x10, 0x01, 0x80, 0x00, "5 Times"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x11, 0x01, 0x40, 0x40, "Single"		},
	{0x11, 0x01, 0x40, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    16, "Free Play"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, ":1,2,3,4"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Balls"		},
	{0x13, 0x01, 0x03, 0x03, "2"			},
	{0x13, 0x01, 0x03, 0x02, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Time To Aim"		},
	{0x13, 0x01, 0x18, 0x18, "25s (Stage 1: 30s)"	},
	{0x13, 0x01, 0x18, 0x10, "20s (Stage 1: 25s)"	},
	{0x13, 0x01, 0x18, 0x08, "17s (Stage 1: 22s)"	},
	{0x13, 0x01, 0x18, 0x00, "15s (Stage 1: 20s)"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Difficult"		},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Rackemup)

static void bankswitch(INT32 bank)
{
	HD6309Bank = bank;
	HD6309MapMemory(DrvHD6309ROM + 0x10000 + ((bank >> 6) * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void battlnts_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x2600) {
		K007342Regs[0][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0x2e08:
			bankswitch(data);
		return;

		case 0x2e0c:
			spritebank = (data & 0x01) * 0x400;
		return;

		case 0x2e10:
			watchdog = 0;
		return;

		case 0x2e14:
			soundlatch = data;
		return;

		case 0x2e18:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 battlnts_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x2e00:
			return DrvDips[2];

		case 0x2e01:
			return DrvInputs[1];

		case 0x2e02:
			return (DrvInputs[0] & 0x7f) | (DrvDips[0] & 0x80);

		case 0x2e03:
			return (DrvInputs[2] & 0x1f) | (DrvDips[1] & 0xe0);

		case 0x2e04:
			return DrvDips[3];
	}

	return 0;
}

static void __fastcall battlnts_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xc000:
		case 0xc001:
			BurnYM3812Write(1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall battlnts_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM3812Read(0, address & 1);

		case 0xc000:
		case 0xc001:
			return BurnYM3812Read(1, address & 1);

		case 0xe000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void battlnts_tile_callback(INT32 /*layer*/, INT32 /*bank*/, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	*code |= ((*color & 0x0f) << 9) | ((*color & 0x40) << 2);
	*color = 0;
}

static void battlnts_sprite_callback(INT32 *code, INT32 *color)
{
	*code |= ((*color & 0xc0) << 2) | spritebank;
	*code = (*code << 2) | ((*color & 0x30) >> 4);
	*color = 4;
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	ZetOpen(0);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	HD6309Bank = 0;
	soundlatch = 0;
	spritebank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM		= Next; Next += 0x020000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x80 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;

	K007342VidRAM[0]	= Next; Next += 0x002000;
	K007342ScrRAM[0]	= Next; Next += 0x000200;

	K007420RAM[0]		= Next; Next += 0x000200;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvHD6309ROM + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvHD6309ROM + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  3, 1)) return 1;
		BurnByteswap(DrvGfxROM0, 0x40000);

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  4, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x40000);
		DrvGfxExpand(DrvGfxROM1, 0x40000);
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(K007342VidRAM[0],	0x0000, 0x1fff, MAP_RAM);
	HD6309MapMemory(K007420RAM[0], 		0x2000, 0x21ff, MAP_RAM);
	HD6309MapMemory(K007342ScrRAM[0],	0x2200, 0x23ff, MAP_RAM);
	HD6309MapMemory(DrvPalRAM,		0x2400, 0x24ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(battlnts_main_write);
	HD6309SetReadHandler(battlnts_main_read);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(battlnts_sound_write);
	ZetSetReadHandler(battlnts_sound_read);
	ZetClose();

	K007342Init(DrvGfxROM0, battlnts_tile_callback);
	K007342SetOffsets(0, 16);

	K007420Init(0x3ff, battlnts_sprite_callback);
	K007420SetOffsets(0, 16);

	BurnYM3812Init(2, 3000000, NULL, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM3812SetRoute(1, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	HD6309Exit();

	BurnYM3812Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x100 / 2; i++) {
		UINT16 p = (pal[i] << 8) | (pal[i] >> 8);
		INT32 r = (p & 0x1f);
		INT32 g = (p >> 5) & 0x1f;
		INT32 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) K007342DrawLayer(0, 0 | K007342_OPAQUE, 0);
	if (nSpriteEnable & 1) K007420DrawSprites(DrvGfxROM1);
	if (nBurnLayer & 2) K007342DrawLayer(0, 1 | K007342_OPAQUE, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (DrvReset) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] =  { 12000000 / 4 / 60, 4000000 / 60 }; // hd6309 has internal divider

	HD6309Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {

		HD6309Run(nCyclesTotal[0] / nInterleave);

		if (i == 248 && K007342_irq_enabled()) HD6309SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	HD6309Close();
	ZetClose();
	
	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(HD6309Bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(spritebank);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(HD6309Bank);
		HD6309Close();
	}

	return 0;
}


// Battlantis (program code G)

static struct BurnRomInfo battlntsRomDesc[] = {
	{ "777_g02.7e",		0x08000, 0xdbd8e17e, 1 }, //  0 HD6309 Code
	{ "777_g03.8e",		0x10000, 0x7bd44fef, 1 }, //  1

	{ "777_c01.10a",	0x08000, 0xc21206e9, 2 }, //  2 Z80 Code

	{ "777c04.13a",		0x40000, 0x45d92347, 3 }, //  3 Tiles

	{ "777c05.13e",		0x40000, 0xaeee778c, 4 }, //  4 Sprites
};

STD_ROM_PICK(battlnts)
STD_ROM_FN(battlnts)

struct BurnDriver BurnDrvBattlnts = {
	"battlnts", NULL, NULL, NULL, "1987",
	"Battlantis (program code G)\0", NULL, "Konami", "GX777",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, battlntsRomInfo, battlntsRomName, NULL, NULL, DrvInputInfo, BattlntsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Battlantis (program code F)

static struct BurnRomInfo battlntsaRomDesc[] = {
	{ "777_f02.7e",		0x08000, 0x9f1dc5c1, 1 }, //  0 HD6309 Code
	{ "777_f03.8e",		0x10000, 0x040d00bf, 1 }, //  1

	{ "777_c01.10a",	0x08000, 0xc21206e9, 2 }, //  2 Z80 Code

	{ "777c04.13a",		0x40000, 0x45d92347, 3 }, //  3 Tiles

	{ "777c05.13e",		0x40000, 0xaeee778c, 4 }, //  4 Sprites
};

STD_ROM_PICK(battlntsa)
STD_ROM_FN(battlntsa)

struct BurnDriver BurnDrvBattlntsa = {
	"battlntsa", "battlnts", NULL, NULL, "1987",
	"Battlantis (program code F)\0", NULL, "Konami", "GX777",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, battlntsaRomInfo, battlntsaRomName, NULL, NULL, DrvInputInfo, BattlntsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Battlantis (Japan, program code E)

static struct BurnRomInfo battlntsjRomDesc[] = {
	{ "777_e02.7e",		0x08000, 0xd631cfcb, 1 }, //  0 HD6309 Code
	{ "777_e03.8e",		0x10000, 0x5ef1f4ef, 1 }, //  1

	{ "777_c01.10a",	0x08000, 0xc21206e9, 2 }, //  2 Z80 Code

	{ "777c04.13a",		0x40000, 0x45d92347, 3 }, //  3 Tiles

	{ "777c05.13e",		0x40000, 0xaeee778c, 4 }, //  4 Sprites
};

STD_ROM_PICK(battlntsj)
STD_ROM_FN(battlntsj)

struct BurnDriver BurnDrvBattlntsj = {
	"battlntsj", "battlnts", NULL, NULL, "1987",
	"Battlantis (Japan, program code E)\0", NULL, "Konami", "GX777",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, battlntsjRomInfo, battlntsjRomName, NULL, NULL, DrvInputInfo, BattlntsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Rack 'em Up (program code L)

static struct BurnRomInfo rackemupRomDesc[] = {
	{ "765_l02.7e",		0x08000, 0x3dfc48bd, 1 }, //  0 HD6309 Code
	{ "765_j03.8e",		0x10000, 0xa13fd751, 1 }, //  1

	{ "765_j01.10a",	0x08000, 0x77ae753e, 2 }, //  2 Z80 Code

	{ "765_l04.13a",	0x40000, 0xd8fb9c64, 3 }, //  3 Tiles

	{ "765_l05.13e",	0x40000, 0x1bb6855f, 4 }, //  4 Sprites
};

STD_ROM_PICK(rackemup)
STD_ROM_FN(rackemup)

struct BurnDriver BurnDrvRackemup = {
	"rackemup", NULL, NULL, NULL, "1987",
	"Rack 'em Up (program code L)\0", NULL, "Konami", "GX765",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, rackemupRomInfo, rackemupRomName, NULL, NULL, DrvInputInfo, RackemupDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// The Hustler (Japan, program code M)

static struct BurnRomInfo thehustlRomDesc[] = {
	{ "765_m02.7e",		0x08000, 0x934807b9, 1 }, //  0 HD6309 Code
	{ "765_j03.8e",		0x10000, 0xa13fd751, 1 }, //  1

	{ "765_j01.10a",	0x08000, 0x77ae753e, 2 }, //  2 Z80 Code

	{ "765_e04.13a",	0x40000, 0x08c2b72e, 3 }, //  3 Tiles

	{ "765_e05.13e",	0x40000, 0xef044655, 4 }, //  4 Sprites
};

STD_ROM_PICK(thehustl)
STD_ROM_FN(thehustl)

struct BurnDriver BurnDrvThehustl = {
	"thehustl", "rackemup", NULL, NULL, "1987",
	"The Hustler (Japan, program code M)\0", NULL, "Konami", "GX765",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, thehustlRomInfo, thehustlRomName, NULL, NULL, DrvInputInfo, RackemupDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// The Hustler (Japan, program code J)

static struct BurnRomInfo thehustljRomDesc[] = {
	{ "765_j02.7e",		0x08000, 0x2ac14c75, 1 }, //  0 HD6309 Code
	{ "765_j03.8e",		0x10000, 0xa13fd751, 1 }, //  1

	{ "765_j01.10a",	0x08000, 0x77ae753e, 2 }, //  2 Z80 Code

	{ "765_e04.13a",	0x40000, 0x08c2b72e, 3 }, //  3 gfx1

	{ "765_e05.13e",	0x40000, 0xef044655, 4 }, //  4 gfx2
};

STD_ROM_PICK(thehustlj)
STD_ROM_FN(thehustlj)

struct BurnDriver BurnDrvThehustlj = {
	"thehustlj", "rackemup", NULL, NULL, "1987",
	"The Hustler (Japan, program code J)\0", NULL, "Konami", "GX765",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, thehustljRomInfo, thehustljRomName, NULL, NULL, DrvInputInfo, RackemupDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};
