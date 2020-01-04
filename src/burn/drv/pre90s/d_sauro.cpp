// FB Alpha Sauro driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "sp0256.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 fg_scrollx;
static UINT8 bg_scrollx;
static INT32 palette_bank;

static INT32 watchdog;

static INT32 sp0256_inuse = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo TecfriInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tecfri)

static struct BurnDIPInfo TecfriDIPList[]=
{
	{0x11, 0xff, 0xff, 0x66, NULL			},
	{0x12, 0xff, 0xff, 0x2f, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x01, 0x00, "Off"			},
	{0x11, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x02, 0x00, "Off"			},
	{0x11, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x04, 0x04, "Upright"		},
	{0x11, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x30, 0x30, "Very Easy"		},
	{0x11, 0x01, 0x30, 0x20, "Easy"			},
	{0x11, 0x01, 0x30, 0x10, "Hard"			},
	{0x11, 0x01, 0x30, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x11, 0x01, 0x40, 0x00, "No"			},
	{0x11, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x00, "Off"			},
	{0x11, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0c, 0x00, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "2"			},
	{0x12, 0x01, 0x30, 0x20, "3"			},
	{0x12, 0x01, 0x30, 0x10, "4"			},
	{0x12, 0x01, 0x30, 0x00, "5"			},
};

STDDIPINFO(Tecfri)

static struct BurnDIPInfo TrckydocaDIPList[]=
{
	{0x11, 0xff, 0xff, 0x66, NULL			},
	{0x12, 0xff, 0xff, 0x2f, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x01, 0x00, "Off"			},
	{0x11, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x02, 0x00, "Off"			},
	{0x11, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x04, 0x04, "Upright"		},
	{0x11, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x30, 0x30, "Very Easy"		},
	{0x11, 0x01, 0x30, 0x20, "Easy"			},
	{0x11, 0x01, 0x30, 0x10, "Hard"			},
	{0x11, 0x01, 0x30, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x11, 0x01, 0x40, 0x00, "No"			},
	{0x11, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x00, "Off"			},
	{0x11, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "2"			},
	{0x12, 0x01, 0x30, 0x20, "3"			},
	{0x12, 0x01, 0x30, 0x10, "4"			},
	{0x12, 0x01, 0x30, 0x00, "5"			},
};

STDDIPINFO(Trckydoca)

static struct BurnDIPInfo SaurobDIPList[]=
{
	{0x11, 0xff, 0xff, 0x99, NULL			},
	{0x12, 0xff, 0xff, 0xd0, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x02, 0x00, "On"			},
	{0x11, 0x01, 0x02, 0x02, "Off"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x04, 0x04, "Cocktail"		},
	{0x11, 0x01, 0x04, 0x00, "Upright"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x08, 0x00, "On"			},
	{0x11, 0x01, 0x08, 0x08, "Off"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x30, 0x30, "Very Hard"		},
	{0x11, 0x01, 0x30, 0x20, "Hard"			},
	{0x11, 0x01, 0x30, 0x10, "Easy"			},
	{0x11, 0x01, 0x30, 0x00, "Very Easy"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x11, 0x01, 0x40, 0x00, "Yes"			},
	{0x11, 0x01, 0x40, 0x40, "No"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x00, "On"			},
	{0x11, 0x01, 0x80, 0x80, "Off"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "4 Coins 1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "5"			},
	{0x12, 0x01, 0x30, 0x20, "4"			},
	{0x12, 0x01, 0x30, 0x10, "3"			},
	{0x12, 0x01, 0x30, 0x00, "2"			},
};

STDDIPINFO(Saurob)

static void __fastcall sauro_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
			soundlatch = data | 0x80;
		return;

		case 0xa0:
			bg_scrollx = data;
		return;

		case 0xa1:
			fg_scrollx = data;
		return;

		case 0xc0:
			flipscreen = data ? 1 : 0;
		return;

		case 0xe0:
			watchdog = 0;
		return;

		case 0xca:
		case 0xcb:
			palette_bank = (data & 3) << 4;
		return;

		case 0xc3: // coin counter - coin 1
		case 0xc5: // coin counter - coin 2
		case 0xc2:
		case 0xc4:
		case 0xc6:
		case 0xc7:
		case 0xc8:
		case 0xc9:
		case 0xcc:
		case 0xcd:
		case 0xce:
		return;
	}
}

static UINT8 __fastcall sauro_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[0];

		case 0x20:
			return DrvDips[1];

		case 0x40:
			return DrvInputs[0];

		case 0x60:
			return DrvInputs[1];
	}

	return 0;
}

static void __fastcall trckydoc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf820:
		case 0xf821:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xf830:
			bg_scrollx = data;
		return;

		case 0xf839:
			flipscreen = data ? 1 : 0;
		return;

		case 0xf83c:
			watchdog = 0;
		return;

		case 0xf838: // nop
		case 0xf83a: // coin counters
		case 0xf83b: // coin counters
		case 0xf83f:
		return;
	}
}

static UINT8 __fastcall trckydoc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return DrvDips[0];

		case 0xf808:
			return DrvDips[1];

		case 0xf810:
			return DrvInputs[0];

		case 0xf818:
			return DrvInputs[1];
	}

	return 0;
}

static void __fastcall sauro_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xa000:
			sp0256_ald_write(data);
		return;

		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
		case 0xe004:
		case 0xe005:
		case 0xe006:
		case 0xe00e:
		case 0xe00f:
		return;
	}
}

static UINT8 __fastcall sauro_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			UINT8 ret = soundlatch;
			soundlatch = 0;
			return ret;
	}

	return 0;
}

static tilemap_callback( background )
{
	INT32 attr  = DrvVidRAM0[offs + 0x400];
	INT32 code  = DrvVidRAM0[offs] + ((attr & 0x07) << 8);
	INT32 color = ((attr >> 4) & 0x0f) + palette_bank;
	INT32 flipx = attr & 0x08;

	TILE_SET_INFO(0, code, color, flipx ? TILE_FLIPX : 0);
}

static tilemap_callback( foreground )
{
	INT32 attr  = DrvVidRAM1[offs + 0x400];
	INT32 code  = DrvVidRAM1[offs] + ((attr & 0x07) << 8);
	INT32 color = ((attr >> 4) & 0x0f) + palette_bank;
	INT32 flipx = attr & 0x08;

	TILE_SET_INFO(1, code, color, flipx ? TILE_FLIPX : 0);
}

static int DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM3812Reset();
	if (sp0256_inuse) sp0256_reset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;
	bg_scrollx = 0;
	fg_scrollx = 0;
	palette_bank = 0;
	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00e000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvSndROM		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000c00;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;

	DrvZ80RAM1		= Next; Next += 0x000800;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[4] = { STEP4(0,1) };
	INT32 XOffs[16] = { 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4, 9*4, 8*4, 11*4, 10*4, 13*4, 12*4, 15*4, 14*4 };
	INT32 YOffs[16] = {
		(0x8000*8*3) +   0, (0x8000*8*2) +   0, (0x8000*8*1) +   0, (0x8000*8*0) +   0,
		(0x8000*8*3) +  64, (0x8000*8*2) +  64, (0x8000*8*1) +  64, (0x8000*8*0) +  64,
		(0x8000*8*3) + 128, (0x8000*8*2) + 128, (0x8000*8*1) + 128, (0x8000*8*0) + 128,
		(0x8000*8*3) + 192, (0x8000*8*2) + 192, (0x8000*8*1) + 192, (0x8000*8*0) + 192
	};

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x0400, 4, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

	for (INT32 i = 0x10000-1; i >= 0; i--) {
		DrvGfxROM0[i*2+1] = DrvGfxROM0[i] & 0xf;
		DrvGfxROM0[i*2+0] = DrvGfxROM0[i] >> 4;
		DrvGfxROM1[i*2+1] = DrvGfxROM1[i] & 0xf;
		DrvGfxROM1[i*2+0] = DrvGfxROM1[i] >> 4;
	}
}

static void sauro_drq_cb(UINT8 data)
{
	ZetSetIRQLine(0x20, data);
}

static INT32 SauroInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, 13, 1)) return 1;

		if ((BurnDrvGetFlags() & BDF_BOOTLEG) == 0)
			BurnLoadRom(DrvSndROM  + 0x01000, 14, 1);

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe800, 0xebff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,		0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(sauro_main_write_port);
	ZetSetInHandler(sauro_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(sauro_sound_write);
	ZetSetReadHandler(sauro_sound_read);
	ZetClose();

	BurnYM3812Init(1, 2500000, NULL, 0);
	BurnTimerAttachYM3812(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	sp0256_init(DrvSndROM, 3120000);
	sp0256_set_drq_cb(sauro_drq_cb);
	sp0256_inuse = 1;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x20000, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x20000, 0, 0x3f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -8, -16);

	// necessary?
	memset (DrvNVRAM, 0xff, 0x800);
	DrvNVRAM[0] = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 TrckydocInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, 10, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe800, 0xebff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xec00, 0xefff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(trckydoc_sound_write);
	ZetSetReadHandler(trckydoc_sound_read);
	ZetClose();

	ZetInit(1); // Just here to let us use same reset routine

	BurnYM3812Init(1, 2500000, NULL, 0);
	BurnTimerAttachYM3812(&ZetConfig, 5000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x20000, 0, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -8, -16);

	// necessary?
	memset (DrvNVRAM, 0xff, 0x800);
	DrvNVRAM[0] = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM3812Exit();
	if (sp0256_inuse) sp0256_exit();

	BurnFree (AllMem);

	sp0256_inuse = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		UINT8 r = DrvColPROM[0x000 + i] & 0xf;
		UINT8 g = DrvColPROM[0x400 + i] & 0xf;
		UINT8 b = DrvColPROM[0x800 + i] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites(INT32 ext_bit, INT32 color_bank, INT32 x_offset) // extbit = 8 sauro, 2 trick
{
	for (INT32 offs = 3; offs < 0x400 - 1; offs += 4)
	{
		INT32 sy = DrvSprRAM[offs];
		INT32 sx = DrvSprRAM[offs+2];
		INT32 code = DrvSprRAM[offs+1] + ((DrvSprRAM[offs+3] & 0x03) << 8);
		INT32 color = ((DrvSprRAM[offs+3] >> 4) & 0x0f) | color_bank;

		if (DrvSprRAM[offs+3] & ext_bit)
		{
			if (sx > 0xc0)
			{
				sx = (signed int)(signed char)sx;
			}
		}
		else
		{
			if (sx < 0x40) continue;
		}

		sx -= x_offset;

		if (ext_bit == 2) {
			if (DrvSprRAM[offs + 3] & 0x08) {
				sy += 8;
			}
			code &= 0x1ff;
		}

		sy = 236 - sy;

		INT32 flipy = flipscreen;
		INT32 flipx = DrvSprRAM[offs+3] & 0x04;

		if (flipy)
		{
			flipx = !flipx;
			sx = (235 - sx) & 0xff;
			sy = 240 - sy;
		}

		if (sx < -15 || sx > nScreenWidth) continue;
		if (sy < -15 || sy > nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM2);
			}
		}
	}
}

static INT32 SauroDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	// weird!
	const UINT8 fg_scrollmap[2][8] = {
		{2, 1, 4, 3, 6, 5, 0, 7}, // not flipped
		{0, 7, 2, 1, 4, 3, 6, 5}  // flipped
	};

	INT32 scrollx = (fg_scrollx & 0xf8) | fg_scrollmap[flipscreen][fg_scrollx & 7];

	GenericTilemapSetScrollX(0, bg_scrollx);
	GenericTilemapSetScrollX(1, scrollx);

	if (nBurnLayer & 1) {
		GenericTilemapDraw(0, pTransDraw, 0);
	} else {
		BurnTransferClear();
	}
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);

	if (nBurnLayer & 4) draw_sprites(0x08, palette_bank, 8);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TrckydocDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, bg_scrollx);

	if (nBurnLayer & 1) {
		GenericTilemapDraw(0, pTransDraw, 0);
	} else {
		BurnTransferClear();
	}

	if (nBurnLayer & 2) draw_sprites(0x02, 0, 8+2);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SauroFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		// bootleg's input lines are inverted
		INT32 state = (BurnDrvGetFlags() & BDF_BOOTLEG) ? 0xff : 0;

		memset (DrvInputs, state, 2);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 128;
	INT32 nCyclesTotal[2] = { 5000000 / 56, 4000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = (nCyclesTotal[0] / nInterleave) * (i + 1);
		nCyclesDone[0] += ZetRun(nCyclesSegment - nCyclesDone[0]);
		if (i == 120) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // vblank
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		nCyclesDone[1] += BurnTimerUpdateYM3812(nCyclesSegment);
		if ((i & 0xf) == 0xf) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 8x per frame
		ZetClose();
	}

	ZetOpen(1);
	
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		if (sp0256_inuse) sp0256_update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 TrckydocFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 128;
	INT32 nCyclesTotal[1] = { 5000000 / 56 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesSegment = (nCyclesTotal[0] / nInterleave) * (i + 1);
		nCyclesDone[0] += BurnTimerUpdateYM3812(nCyclesSegment - nCyclesDone[0]);
		if (i == 120) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // vblank
	}
	
	BurnTimerEndFrameYM3812(nCyclesTotal[0]);

	if (pBurnSoundOut) {		
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);
		if (sp0256_inuse) sp0256_scan(nAction, pnMin);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bg_scrollx);
		SCAN_VAR(fg_scrollx);
		SCAN_VAR(palette_bank);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Sauro

static struct BurnRomInfo sauroRomDesc[] = {
	{ "sauro-2.bin",	0x8000, 0x19f8de25, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sauro-1.bin",	0x8000, 0x0f8b876f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sauro-3.bin",	0x8000, 0x0d501e1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "sauro-6.bin",	0x8000, 0x4b77cb0f, 3 | BRF_GRA },           //  3 Background Tiles
	{ "sauro-7.bin",	0x8000, 0x187da060, 3 | BRF_GRA },           //  4

	{ "sauro-4.bin",	0x8000, 0x9b617cda, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "sauro-5.bin",	0x8000, 0xa6e2640d, 4 | BRF_GRA },           //  6

	{ "sauro-8.bin",	0x8000, 0xe08b5d5e, 5 | BRF_GRA },           //  7 Sprites
	{ "sauro-9.bin",	0x8000, 0x7c707195, 5 | BRF_GRA },           //  8
	{ "sauro-10.bin",	0x8000, 0xc93380d1, 5 | BRF_GRA },           //  9
	{ "sauro-11.bin",	0x8000, 0xf47982a8, 5 | BRF_GRA },           // 10

	{ "82s137-3.bin",	0x0400, 0xd52c4cd0, 6 | BRF_GRA },           // 11 Color data
	{ "82s137-2.bin",	0x0400, 0xc3e96d5d, 6 | BRF_GRA },           // 12
	{ "82s137-1.bin",	0x0400, 0xbdfcf00c, 6 | BRF_GRA },           // 13

	{ "sp0256-al2.bin",	0x0800, 0xb504ac15, 7 | BRF_GRA },           // 14 Speech data
};

STD_ROM_PICK(sauro)
STD_ROM_FN(sauro)

struct BurnDriver BurnDrvSauro = {
	"sauro", NULL, NULL, NULL, "1987",
	"Sauro\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sauroRomInfo, sauroRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, TecfriDIPInfo,
	SauroInit, DrvExit, SauroFrame, SauroDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Sauro (Philko license)

static struct BurnRomInfo sauropRomDesc[] = {
	{ "s2.3k",		0x8000, 0x79846222, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "s1.3f",		0x8000, 0x3efd13ed, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s3.5x",		0x8000, 0x0d501e1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "s6.7x",		0x8000, 0x4b77cb0f, 3 | BRF_GRA },           //  3 Background Tiles
	{ "s7.7z",		0x8000, 0x187da060, 3 | BRF_GRA },           //  4

	{ "s4.7h",		0x8000, 0x9b617cda, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "s5.7k",		0x8000, 0xde5cd249, 4 | BRF_GRA },           //  6

	{ "s8.10l",		0x8000, 0xe08b5d5e, 5 | BRF_GRA },           //  7 Sprites
	{ "s9.10p",		0x8000, 0x7c707195, 5 | BRF_GRA },           //  8
	{ "s10.10r",		0x8000, 0xc93380d1, 5 | BRF_GRA },           //  9
	{ "s11.10t",		0x8000, 0xf47982a8, 5 | BRF_GRA },           // 10

	{ "82s137-3.bin",	0x0400, 0xd52c4cd0, 6 | BRF_GRA },           // 11 Color data
	{ "82s137-2.bin",	0x0400, 0xc3e96d5d, 6 | BRF_GRA },           // 12
	{ "82s137-1.bin",	0x0400, 0xbdfcf00c, 6 | BRF_GRA },           // 13

	{ "sp0256-al2.bin",	0x0800, 0xb504ac15, 7 | BRF_GRA },           // 14 Speech data
};

STD_ROM_PICK(saurop)
STD_ROM_FN(saurop)

struct BurnDriver BurnDrvSaurop = {
	"saurop", "sauro", NULL, NULL, "1987",
	"Sauro (Philko license)\0", NULL, "Tecfri (Philko license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sauropRomInfo, sauropRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, TecfriDIPInfo,
	SauroInit, DrvExit, SauroFrame, SauroDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Sauro (Recreativos Real S.A. license)

static struct BurnRomInfo saurorrRomDesc[] = {
	{ "27256-2.bin",	0x8000, 0xb0d80eab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "27256-1.bin",	0x8000, 0xcbb5f06e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sauro-3.bin",	0x8000, 0x0d501e1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "sauro-6.bin",	0x8000, 0x4b77cb0f, 3 | BRF_GRA },           //  3 Background Tiles
	{ "sauro-7.bin",	0x8000, 0x187da060, 3 | BRF_GRA },           //  4

	{ "sauro-4.bin",	0x8000, 0x9b617cda, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "27256-5.bin",	0x8000, 0x9aabdbe5, 4 | BRF_GRA },           //  6

	{ "sauro-8.bin",	0x8000, 0xe08b5d5e, 5 | BRF_GRA },           //  7 Sprites
	{ "sauro-9.bin",	0x8000, 0x7c707195, 5 | BRF_GRA },           //  8
	{ "sauro-10.bin",	0x8000, 0xc93380d1, 5 | BRF_GRA },           //  9
	{ "sauro-11.bin",	0x8000, 0xf47982a8, 5 | BRF_GRA },           // 10

	{ "82s137-3.bin",	0x0400, 0xd52c4cd0, 6 | BRF_GRA },           // 11 Color data
	{ "82s137-2.bin",	0x0400, 0xc3e96d5d, 6 | BRF_GRA },           // 12
	{ "82s137-1.bin",	0x0400, 0xbdfcf00c, 6 | BRF_GRA },           // 13

	{ "sp0256-al2.bin",	0x0800, 0xb504ac15, 7 | BRF_GRA },           // 14 Speech data
};

STD_ROM_PICK(saurorr)
STD_ROM_FN(saurorr)

struct BurnDriver BurnDrvSaurorr = {
	"saurorr", "sauro", NULL, NULL, "1987",
	"Sauro (Recreativos Real S.A. license)\0", NULL, "Tecfri (Recreativos Real S.A. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, saurorrRomInfo, saurorrRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, TecfriDIPInfo,
	SauroInit, DrvExit, SauroFrame, SauroDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Sauro (bootleg)

static struct BurnRomInfo saurobRomDesc[] = {
	{ "sauro02.7c",		0x8000, 0x72026b9a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sauro01.6c",		0x8000, 0x4ff12c25, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sauro03.16e",	0x8000, 0xa30b60fc, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "sauro-6.bin",	0x8000, 0x4b77cb0f, 3 | BRF_GRA },           //  3 Background Tiles
	{ "sauro-7.bin",	0x8000, 0x187da060, 3 | BRF_GRA },           //  4

	{ "sauro-4.bin",	0x8000, 0x9b617cda, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "sauro-5.bin",	0x8000, 0xa6e2640d, 4 | BRF_GRA },           //  6

	{ "sauro-8.bin",	0x8000, 0xe08b5d5e, 5 | BRF_GRA },           //  7 Sprites
	{ "sauro-9.bin",	0x8000, 0x7c707195, 5 | BRF_GRA },           //  8
	{ "sauro-10.bin",	0x8000, 0xc93380d1, 5 | BRF_GRA },           //  9
	{ "sauro-11.bin",	0x8000, 0xf47982a8, 5 | BRF_GRA },           // 10

	{ "82s137-3.bin",	0x0400, 0xd52c4cd0, 6 | BRF_GRA },           // 11 Color data
	{ "82s137-2.bin",	0x0400, 0xc3e96d5d, 6 | BRF_GRA },           // 12
	{ "82s137-1.bin",	0x0400, 0xbdfcf00c, 6 | BRF_GRA },           // 13

	{ "sauropr4.16h",	0x0200, 0x5261bc11, 7 | BRF_GRA },           // 14 Unknown prom
};

STD_ROM_PICK(saurob)
STD_ROM_FN(saurob)

struct BurnDriver BurnDrvSaurob = {
	"saurob", "sauro", NULL, NULL, "1987",
	"Sauro (bootleg)\0", "Missing speech is normal", "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, saurobRomInfo, saurobRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, SaurobDIPInfo,
	SauroInit, DrvExit, SauroFrame, SauroDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Tricky Doc (set 1)

static struct BurnRomInfo trckydocRomDesc[] = {
	{ "trckydoc.d9",	0x8000, 0xc6242fc3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "trckydoc.b9",	0x8000, 0x8645c840, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "trckydoc.e6",	0x8000, 0xec326392, 3 | BRF_GRA },           //  2 Background Tiles
	{ "trckydoc.g6",	0x8000, 0x6a65c088, 3 | BRF_GRA },           //  3

	{ "trckydoc.h1",	0x4000, 0x8b73cbf3, 5 | BRF_GRA },           //  4 Sprites
	{ "trckydoc.e1",	0x4000, 0x841be98e, 5 | BRF_GRA },           //  5
	{ "trckydoc.c1",	0x4000, 0x1d25574b, 5 | BRF_GRA },           //  6
	{ "trckydoc.a1",	0x4000, 0x436c59ba, 5 | BRF_GRA },           //  7

	{ "tdclr3.prm",		0x0100, 0x671d0140, 6 | BRF_GRA },           //  8 Color data
	{ "tdclr2.prm",		0x0100, 0x874f9050, 6 | BRF_GRA },           //  9
	{ "tdclr1.prm",		0x0100, 0x57f127b0, 6 | BRF_GRA },           // 10

	{ "tdprm.prm",		0x0200, 0x5261bc11, 0 | BRF_OPT },           // 11 Unknown prom
};

STD_ROM_PICK(trckydoc)
STD_ROM_FN(trckydoc)

struct BurnDriver BurnDrvTrckydoc = {
	"trckydoc", NULL, NULL, NULL, "1987",
	"Tricky Doc (set 1)\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, trckydocRomInfo, trckydocRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, TecfriDIPInfo,
	TrckydocInit, DrvExit, TrckydocFrame, TrckydocDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Tricky Doc (set 2)

static struct BurnRomInfo trckydocaRomDesc[] = {
	{ "trckydca.d9",	0x8000, 0x99c38aa4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "trckydca.b9",	0x8000, 0xb6048a15, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "trckydoc.e6",	0x8000, 0xec326392, 3 | BRF_GRA },           //  2 Background Tiles
	{ "trckydoc.g6",	0x8000, 0x6a65c088, 3 | BRF_GRA },           //  3

	{ "trckydoc.h1",	0x4000, 0x8b73cbf3, 5 | BRF_GRA },           //  4 Sprites
	{ "trckydoc.e1",	0x4000, 0x841be98e, 5 | BRF_GRA },           //  5
	{ "trckydoc.c1",	0x4000, 0x1d25574b, 5 | BRF_GRA },           //  6
	{ "trckydoc.a1",	0x4000, 0x436c59ba, 5 | BRF_GRA },           //  7

	{ "tdclr3.prm",		0x0100, 0x671d0140, 6 | BRF_GRA },           //  8 Color data
	{ "tdclr2.prm",		0x0100, 0x874f9050, 6 | BRF_GRA },           //  9
	{ "tdclr1.prm",		0x0100, 0x57f127b0, 6 | BRF_GRA },           // 10

	{ "tdprm.prm",		0x0200, 0x5261bc11, 0 | BRF_OPT },           // 11 Unkown prom
};

STD_ROM_PICK(trckydoca)
STD_ROM_FN(trckydoca)

struct BurnDriver BurnDrvTrckydoca = {
	"trckydoca", "trckydoc", NULL, NULL, "1987",
	"Tricky Doc (set 2)\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, trckydocaRomInfo, trckydocaRomName, NULL, NULL, NULL, NULL, TecfriInputInfo, TrckydocaDIPInfo,
	TrckydocInit, DrvExit, TrckydocFrame, TrckydocDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};
