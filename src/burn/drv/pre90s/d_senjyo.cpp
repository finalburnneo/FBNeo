// FB Alpha Senjyo / Star Force / Baluba-louk driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "dac.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Ops0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvGfxROM5;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRegs;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvBgRAM2;
static UINT8 *DrvRadarRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 sounddata;
static UINT8 soundclock;
static UINT8 soundstop;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static HoldCoin<2> hold_coin;

static INT32 is_senjyo = 0;
static INT32 is_starforc_encrypted = 0;
static INT32 starforce_small_sprites = 0;

static struct BurnInputInfo SenjyoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Senjyo)

static struct BurnDIPInfo SenjyoDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x80, NULL					},
	{0x10, 0xff, 0xff, 0x43, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0f, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0f, 0x01, 0x30, 0x30, "2"					},
	{0x0f, 0x01, 0x30, 0x00, "3"					},
	{0x0f, 0x01, 0x30, 0x10, "4"					},
	{0x0f, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x40, 0x40, "Upright"				},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x10, 0x01, 0x01, 0x00, "Off"					},
	{0x10, 0x01, 0x01, 0x01, "On"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x10, 0x01, 0x02, 0x02, "100k only"			},
	{0x10, 0x01, 0x02, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0xc0, 0x80, "Easy"					},
	{0x10, 0x01, 0xc0, 0x40, "Medium"				},
	{0x10, 0x01, 0xc0, 0x00, "Hard"					},
	{0x10, 0x01, 0xc0, 0xc0, "Hardest"				},
};

STDDIPINFO(Senjyo)

static struct BurnDIPInfo StarforcDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xc0, NULL					},
	{0x10, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0f, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0f, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0f, 0x01, 0x30, 0x30, "2"					},
	{0x0f, 0x01, 0x30, 0x00, "3"					},
	{0x0f, 0x01, 0x30, 0x10, "4"					},
	{0x0f, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x40, 0x40, "Upright"				},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x10, 0x01, 0x07, 0x00, "50k, 200k and 500k"	},
	{0x10, 0x01, 0x07, 0x01, "100k, 300k and 800k"	},
	{0x10, 0x01, 0x07, 0x02, "50k and 200k"			},
	{0x10, 0x01, 0x07, 0x03, "100k and 300k"		},
	{0x10, 0x01, 0x07, 0x04, "50k only"				},
	{0x10, 0x01, 0x07, 0x05, "100k only"			},
	{0x10, 0x01, 0x07, 0x06, "200k only"			},
	{0x10, 0x01, 0x07, 0x07, "None"					},

	{0   , 0xfe, 0   ,    6, "Difficulty"			},
	{0x10, 0x01, 0x38, 0x00, "Easiest"				},
	{0x10, 0x01, 0x38, 0x08, "Easy"					},
	{0x10, 0x01, 0x38, 0x10, "Medium"				},
	{0x10, 0x01, 0x38, 0x18, "Difficult"			},
	{0x10, 0x01, 0x38, 0x20, "Hard"					},
	{0x10, 0x01, 0x38, 0x28, "Hardest"				},
};

STDDIPINFO(Starforc)

static struct BurnDIPInfo BalubaDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xc0, NULL					},
	{0x10, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0f, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0f, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0f, 0x01, 0x30, 0x30, "2"					},
	{0x0f, 0x01, 0x30, 0x00, "3"					},
	{0x0f, 0x01, 0x30, 0x10, "4"					},
	{0x0f, 0x01, 0x30, 0x20, "5"					},
	
	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x40, 0x40, "Upright"				},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x10, 0x01, 0x07, 0x00, "30k, 100k and 200k"	},
	{0x10, 0x01, 0x07, 0x01, "50k, 200k and 500k"	},
	{0x10, 0x01, 0x07, 0x02, "30k and 100k"			},
	{0x10, 0x01, 0x07, 0x03, "50k and 200k"			},
	{0x10, 0x01, 0x07, 0x04, "30k only"				},
	{0x10, 0x01, 0x07, 0x05, "100k only"			},
	{0x10, 0x01, 0x07, 0x06, "200k only"			},
	{0x10, 0x01, 0x07, 0x07, "None"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x10, 0x01, 0x38, 0x00, "0"					},
	{0x10, 0x01, 0x38, 0x08, "1"					},
	{0x10, 0x01, 0x38, 0x10, "2"					},
	{0x10, 0x01, 0x38, 0x18, "3"					},
	{0x10, 0x01, 0x38, 0x20, "4"					},
	{0x10, 0x01, 0x38, 0x28, "5"					},
	{0x10, 0x01, 0x38, 0x30, "6"					},
	{0x10, 0x01, 0x38, 0x38, "7"					},
};

STDDIPINFO(Baluba)

static void __fastcall senjyo_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			flipscreen = data ? 1 : 0;
		return;

		case 0xd002:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xd004:
			ZetClose();
			ZetOpen(1);
			z80pio_port_write(0, data);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall senjyo_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
			return DrvInputs[address & 3];

		case 0xd003:
			return 0;

		case 0xd004:
		case 0xd005:
			return DrvDips[address & 1];
	}

	return 0;
}

static void __fastcall senjyo_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
			SN76496Write(0, data);
		return;
		case 0x9000:
			SN76496Write(1, data);
		return;
		case 0xa000:
			SN76496Write(2, data);
		return;

		case 0xd000:
			sounddata = (data & 0x0f) << 1;
			soundstop = 0;
		return;
	}
}

static void __fastcall senjyo_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			z80pio_write_alt(port & 3, data);
		return;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			z80ctc_write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall senjyo_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			return z80pio_read_alt(port & 3);

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return z80ctc_read(port & 3);
	}

	return 0;
}

static tilemap_callback( senjyo_fg )
{
	UINT16 attr = DrvFgRAM[offs + 0x400];
	UINT16 code = DrvFgRAM[offs] + ((attr & 0x10) << 4);
	INT32 flags = (attr & 0x80) ? TILE_FLIPY : 0;
	if ((offs & 0x1f) >= 0x18) flags |= TILE_OPAQUE; // ???

	TILE_SET_INFO(0, code, attr, flags);
}

static tilemap_callback( starforc_fg )
{
	UINT16 attr = DrvFgRAM[offs + 0x400];
	UINT16 code = DrvFgRAM[offs] + ((attr & 0x10) << 4);
	INT32 flags = (attr & 0x80) ? TILE_FLIPY : 0;

	TILE_SET_INFO(0, code, attr, flags);
}

static tilemap_callback( senjyo_bg0 )
{
	UINT8 code = DrvBgRAM0[offs];

	TILE_SET_INFO(1, code, (code >> 4) & 7, 0);
}

static tilemap_callback( starforc_bg0 )
{
	UINT8 code = DrvBgRAM0[offs];
	UINT8 color = ((code >> 4) & 6) | (code >> 7);

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_callback( senjyo_bg1 )
{
	UINT8 code = DrvBgRAM1[offs];

	TILE_SET_INFO(2, code, code >> 5, 0);
}

static tilemap_callback( senjyo_bg2 )
{
	UINT8 code = DrvBgRAM2[offs];

	TILE_SET_INFO(3, code, code >> 5, 0);
}

static void tilemap_init(INT32 type)
{
	if (type == 0) // senjyo
	{
		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, senjyo_fg_map_callback,   8,  8, 32, 32);
		GenericTilemapInit(1, TILEMAP_SCAN_ROWS, senjyo_bg0_map_callback, 16, 16, 16, 32);
		GenericTilemapInit(2, TILEMAP_SCAN_ROWS, senjyo_bg1_map_callback, 16, 16, 16, 48);
		GenericTilemapInit(3, TILEMAP_SCAN_ROWS, senjyo_bg2_map_callback, 16, 16, 16, 56);
	}
	else
	{
		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, starforc_fg_map_callback,   8,  8, 32, 32);
		GenericTilemapInit(1, TILEMAP_SCAN_ROWS, starforc_bg0_map_callback, 16, 16, 16, 32);
		GenericTilemapInit(2, TILEMAP_SCAN_ROWS, senjyo_bg1_map_callback,   16, 16, 16, 32);
		GenericTilemapInit(3, TILEMAP_SCAN_ROWS, senjyo_bg2_map_callback,   16, 16, 16, 32);
	}

	GenericTilemapSetGfx(0, DrvGfxROM0, 3,  8,  8, 0x08000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM1, 3, 16, 16, 0x10000, 0x40, 7);
	GenericTilemapSetGfx(2, DrvGfxROM2, 3, 16, 16, 0x10000, 0x80, 7);
	GenericTilemapSetGfx(3, DrvGfxROM3, 3, 16, 16, 0x08000, 0xc0, 7);

	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetTransparent(3, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
}

static INT32 DrvDoReset()
{
	memset (AllRam,   0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	DACReset();
	ZetClose();

	SN76496Reset();

	flipscreen = 0;
	sounddata = 0;
	soundclock = 0;
	soundstop = 0;

	hold_coin.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80Ops0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x010000;
	DrvGfxROM3		= Next; Next += 0x010000;
	DrvGfxROM4		= Next; Next += 0x020000;
	DrvGfxROM5		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x202 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvVidRegs		= Next; Next += 0x000100;
	DrvBgRAM0		= Next; Next += 0x000800;
	DrvBgRAM1		= Next; Next += 0x000800;
	DrvBgRAM2		= Next; Next += 0x000800;
	DrvRadarRAM		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()// 0, 64, 128, 192, 320, 320
{
	INT32 Plane0[4] = { 0, 0x2000*8*1, 0x2000*8*2 };
	INT32 Plane1[4] = { 0, 0x4000*8*1, 0x4000*8*2 };
	INT32 XOffs[32] = { STEP8(0,1), STEP8(64,1), STEP8(256,1), STEP8(256+64,1) };
	INT32 YOffs[32] = { STEP8(0,8), STEP8(128,8), STEP8(512,8), STEP8(512+128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);

	memcpy (tmp, DrvGfxROM0, 0x06000);

	GfxDecode(0x400, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x06000);

	GfxDecode(0x100, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x06000);

	GfxDecode(0x100, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x06000);

	GfxDecode(0x100, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x0c000);

	GfxDecode(0x200, 3, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM4);
	GfxDecode(0x080, 3, 32, 32, Plane1, XOffs, YOffs, 0x400, tmp, DrvGfxROM5);

	BurnFree(tmp);
}

static void pioctc_intr(INT32 state)
{
	ZetSetIRQLine(0, state);
}

static void ctc_trigger(INT32 offs, UINT8 data)
{
	z80ctc_trg_write(1, data);
}

static void ctc_clockdac(INT32 offs, UINT8 data)
{
	if (data) {
		DACWrite(0, (soundclock & 0x08) ? sounddata : 0);
		soundclock++;
		if (is_senjyo && soundstop++ > 0x30) sounddata = 0;
	}
}

static void MachineInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80Ops0,		0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9800, 0x98ff, MAP_RAM); // 0-7f
	ZetMapMemory(DrvPalRAM,			0x9c00, 0x9dff, MAP_RAM);
	ZetMapMemory(DrvVidRegs,		0x9e00, 0x9eff, MAP_RAM); // 0-3f
	ZetMapMemory(DrvBgRAM2,			0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvRadarRAM,		0xb800, 0xbbff, MAP_RAM);
	ZetSetWriteHandler(senjyo_main_write);
	ZetSetReadHandler(senjyo_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);

    ZetDaisyInit(Z80_PIO, Z80_CTC);
	z80pio_init(pioctc_intr, NULL, NULL, NULL, NULL, NULL, NULL);
	z80ctc_init(2000000, 0, pioctc_intr, ctc_trigger, NULL, ctc_clockdac);

    ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(senjyo_sound_write);
	ZetSetOutHandler(senjyo_sound_write_port);
	ZetSetInHandler(senjyo_sound_read_port);
	ZetClose();

	SN76496Init(0, 2000000, 0);
	SN76496Init(1, 2000000, 1);
	SN76496Init(2, 2000000, 1);
	SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 2000000);

	DACInit(0, 0, 1, ZetTotalCycles, 2000000);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
}

static INT32 SenjyoInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		memcpy (DrvZ80Ops0, DrvZ80ROM0, 0x8000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  9, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x4000, DrvGfxROM1 + 0x3000, 0x1000);
		memcpy (DrvGfxROM1 + 0x1000, DrvGfxROM1 + 0x0000, 0x1000);
		memcpy (DrvGfxROM1 + 0x3000, DrvGfxROM1 + 0x2000, 0x1000);
		memcpy (DrvGfxROM1 + 0x5000, DrvGfxROM1 + 0x4000, 0x1000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 11, 1)) return 1;
		memcpy (DrvGfxROM2 + 0x4000, DrvGfxROM2 + 0x3000, 0x1000);
		memcpy (DrvGfxROM1 + 0x1000, DrvGfxROM1 + 0x0000, 0x1000);
		memcpy (DrvGfxROM1 + 0x3000, DrvGfxROM1 + 0x2000, 0x1000);
		memcpy (DrvGfxROM1 + 0x5000, DrvGfxROM1 + 0x4000, 0x1000);

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x02000, 13, 1)) return 1;
		memcpy (DrvGfxROM3 + 0x4000, DrvGfxROM3 + 0x3000, 0x1000);
		memcpy (DrvGfxROM1 + 0x1000, DrvGfxROM1 + 0x0000, 0x1000);
		memcpy (DrvGfxROM1 + 0x3000, DrvGfxROM1 + 0x2000, 0x1000);
		memcpy (DrvGfxROM1 + 0x5000, DrvGfxROM1 + 0x4000, 0x1000);

		if (BurnLoadRom(DrvGfxROM4 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x02000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x04000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x06000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x08000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x0a000, 19, 1)) return 1;

		DrvGfxDecode();
	}

	MachineInit();

	GenericTilesInit();
	tilemap_init(0);

	is_senjyo = 1;

	DrvDoReset();

	return 0;
}

static INT32 StarforcInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (is_starforc_encrypted) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  k++, 1)) return 1;

		} else {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  k++, 1)) return 1;
		}
		memcpy (DrvZ80Ops0, DrvZ80ROM0, 0x8000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x04000, k++, 1)) return 1;

        if (starforce_small_sprites) {
            if (BurnLoadRom(DrvGfxROM4 + 0x00000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x02000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x04000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x06000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x08000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x0a000, k++, 1)) return 1;
        } else {
            if (BurnLoadRom(DrvGfxROM4 + 0x00000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x04000, k++, 1)) return 1;
            if (BurnLoadRom(DrvGfxROM4 + 0x08000, k++, 1)) return 1;
        }

		DrvGfxDecode();
	}

	MachineInit();

	GenericTilesInit();
	tilemap_init(1);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	SN76496Exit();
	DACExit();

	BurnFree (AllMem);

	is_senjyo = 0;
	is_starforc_encrypted = 0;
    starforce_small_sprites = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		UINT8 raw = DrvPalRAM[i];

		UINT8 n = (raw >> 6) & 3;
		UINT8 r = (raw << 2) & 0x0c;
		UINT8 g = (raw     ) & 0x0c;
		UINT8 b = (raw >> 2) & 0x0c;

		r = pal4bit(r ? (r + n) : 0);
		g = pal4bit(g ? (g + n) : 0);
		b = pal4bit(b ? (b + n) : 0);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	DrvPalette[0x200] = BurnHighCol(0xff, 0x00, 0x00, 0);
	DrvPalette[0x201] = BurnHighCol(0xff, 0xff, 0x00, 0);
}

static void draw_bgbitmap()
{
	INT32 stripe = DrvVidRegs[0x27];

	if (stripe == 0xff) {
		BurnTransferClear();
		return;
	}

	INT32 pen = 0;
	INT32 count = 0;
	if (stripe == 0) stripe = 0x100;
	INT32 flip = (flipscreen ? 0xff : 0);
	if (flip) stripe ^= 0xff;

	for (INT32 x = 0; x < nScreenWidth; x++)
	{
		UINT16 *dst = pTransDraw + (x^flip);
		UINT16 color = (pen & 0xf) + 0x180;

		for (INT32 y = 0; y < nScreenHeight; y++) {
			dst[((y^flip) % 224) * nScreenWidth] = color;
		}

		count += 0x10;
		if (count >= stripe) {
			pen++;
			count -= stripe;
		}
	}
}

static void draw_radar()
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		for (INT32 x = 0; x < 8; x++)
		{
			if (DrvRadarRAM[offs] & (1 << x))
			{
				INT32 sx = (8 * (offs % 8) + x) + 256-64;
				INT32 sy = ((offs & 0x1ff) / 8) + 96-(flipscreen ? -16 : 16);

				if (flipscreen)
				{
					sx = 255 - sx;
					sy = 255 - sy;
				}

				if (sy >= 0 && sx >= 0 && sy < nScreenHeight && sx < nScreenWidth)
				{
					pTransDraw[(sy * nScreenWidth) + sx] = (offs < 0x200) ? 0x200 : 0x201;
				}
			}
		}
	}
}

static void draw_sprites(INT32 bigmask, INT32 priority)
{
	for (INT32 offs = 0x80 - 4; offs >= 0; offs -= 4)
	{
		if (((DrvSprRAM[offs+1] & 0x30) >> 4) == priority)
		{
			INT32 big = ((DrvSprRAM[offs] & bigmask) == bigmask);

			INT32 sx = DrvSprRAM[offs+3];
			INT32 sy = DrvSprRAM[offs+2]; 
			INT32 flipx = DrvSprRAM[offs+1] & 0x40;
			INT32 flipy = DrvSprRAM[offs+1] & 0x80;
			INT32 code = DrvSprRAM[offs];
			if (big) code &= 0x7f; else code &= 0x1ff;
			// mask: 0x80 big, 0x200 small
			INT32 color = DrvSprRAM[offs + 1] & 7;

			sy = (big) ? (224 - sy) : (240 - sy);

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;

				if (big) {
					sx = 224 - sx;
					sy = 226 - sy;
				} else {
					sx = 240 - sx;
					sy = 242 - sy;
				}
			}

			sy -= 16;

			if (big) {
				Draw32x32MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 3, 0, 0x140, DrvGfxROM5);
			} else {
				Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 3, 0, 0x140, DrvGfxROM4);
			}
		}
	}
}

static void common_draw(INT32 scrollhack, INT32 spritemask) // senj = 0x80, starf = 0xc0
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update
	}

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvVidRegs[i]);
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen) ? TMAP_FLIPXY : 0);

	INT32 scrollx = (flipscreen) ? -DrvVidRegs[0x35] : DrvVidRegs[0x35];

	GenericTilemapSetScrollX(1, scrollx);
	GenericTilemapSetScrollY(1, DrvVidRegs[0x30] + (DrvVidRegs[0x31] * 256));

	scrollhack = scrollhack ? 8 : 0;
	scrollx = (flipscreen) ? -DrvVidRegs[0x2d + scrollhack] : DrvVidRegs[0x2d + scrollhack];
	GenericTilemapSetScrollX(2, scrollx);
	GenericTilemapSetScrollY(2, DrvVidRegs[0x28 + scrollhack] + (DrvVidRegs[0x29 + scrollhack] * 256));

	scrollx = (flipscreen) ? -DrvVidRegs[0x25] : DrvVidRegs[0x25];
	GenericTilemapSetScrollX(3, scrollx);
	GenericTilemapSetScrollY(3, DrvVidRegs[0x20] + (DrvVidRegs[0x21] * 256));

	BurnTransferClear();

	if (nBurnLayer & 0x01) draw_bgbitmap();
	if (nSpriteEnable & 1) draw_sprites(spritemask, 0);
	if (nBurnLayer & 0x02) GenericTilemapDraw(3, pTransDraw, 0);
	if (nSpriteEnable & 2) draw_sprites(spritemask, 1);
	if (nBurnLayer & 0x04) GenericTilemapDraw(2, pTransDraw, 0);
	if (nSpriteEnable & 4) draw_sprites(spritemask, 2);
	if (nBurnLayer & 0x08) GenericTilemapDraw(1, pTransDraw, 0);
	if (nSpriteEnable & 0x10) draw_sprites(spritemask, 3);
	if (nSpriteEnable & 0x20) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 0x40) draw_radar();

	BurnTransferFlip(flipscreen, flipscreen); // unflip coctail for netplay/etc
	BurnTransferCopy(DrvPalette);
}

static INT32 SenjyoDraw()
{
	common_draw(0, 0x80);

	return 0;
}

static INT32 StarforcDraw() // and starfora
{
	common_draw(1, 0xc0);

	return 0;
}

static INT32 StarforceDraw()
{
	common_draw(0, 0xc0);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
        memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		hold_coin.check(0, DrvInputs[2], 1 << 0, 5);
		hold_coin.check(1, DrvInputs[2], 1 << 1, 5);
    }

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	if (pBurnSoundOut) {
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(sounddata);
		SCAN_VAR(soundclock);
		SCAN_VAR(soundstop);

		hold_coin.scan();
	}

	return 0;
}


// Senjyo

static struct BurnRomInfo senjyoRomDesc[] = {
	{ "08m_05t.bin",	0x2000, 0xb1f3544d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "08k_04t.bin",	0x2000, 0xe34468a8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "08j_03t.bin",	0x2000, 0xc33aedee, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "08f_02t.bin",	0x2000, 0x0ef4db9e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "02h_01t.bin",	0x2000, 0xc1c24455, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "08h_08b.bin",	0x1000, 0x0c875994, 3 | BRF_GRA },           //  5 Fg Layer Tiles
	{ "08f_07b.bin",	0x1000, 0x497bea8e, 3 | BRF_GRA },           //  6
	{ "08d_06b.bin",	0x1000, 0x4ef69b00, 3 | BRF_GRA },           //  7

	{ "05n_16m.bin",	0x1000, 0x0d3e00fb, 4 | BRF_GRA },           //  8 Bg Layer #0 Tiles
	{ "05k_15m.bin",	0x2000, 0x93442213, 4 | BRF_GRA },           //  9

	{ "07n_18m.bin",	0x1000, 0xd50fced3, 5 | BRF_GRA },           // 10 Bg Layer #1 Tiles
	{ "07k_17m.bin",	0x2000, 0x10c3a5f0, 5 | BRF_GRA },           // 11

	{ "09n_20m.bin",	0x1000, 0x54cb8126, 6 | BRF_GRA },           // 12 Bg Layer #2 Tiles
	{ "09k_19m.bin",	0x2000, 0x373e047c, 6 | BRF_GRA },           // 13

	{ "08p_13b.bin",	0x2000, 0x40127efd, 7 | BRF_GRA },           // 14 Sprites
	{ "08s_14b.bin",	0x2000, 0x42648ffa, 7 | BRF_GRA },           // 15
	{ "08m_11b.bin",	0x2000, 0xccc4680b, 7 | BRF_GRA },           // 16
	{ "08n_12b.bin",	0x2000, 0x742fafed, 7 | BRF_GRA },           // 17
	{ "08j_09b.bin",	0x2000, 0x1ee63b5c, 7 | BRF_GRA },           // 18
	{ "08k_10b.bin",	0x2000, 0xa9f41ec9, 7 | BRF_GRA },           // 19

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 20 Unused PROM
};

STD_ROM_PICK(senjyo)
STD_ROM_FN(senjyo)

struct BurnDriver BurnDrvSenjyo = {
	"senjyo", NULL, NULL, NULL, "1983",
	"Senjyo\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, senjyoRomInfo, senjyoRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, SenjyoDIPInfo,
	SenjyoInit, DrvExit, DrvFrame, SenjyoDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Star Force

static struct BurnRomInfo starforcRomDesc[] = {
	{ "3.3p",			0x4000, 0x8ba27691, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3mn",			0x4000, 0x0fc4d2d6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.3hj",			0x2000, 0x2735bb22, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "7.2fh",			0x1000, 0xf4803339, 3 | BRF_GRA },           //  3 Fg Layer Tiles
	{ "8.3fh",			0x1000, 0x96979684, 3 | BRF_GRA },           //  4
	{ "9.3fh",			0x1000, 0xeead1d5c, 3 | BRF_GRA },           //  5

	{ "15.10jk",		0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  6 Bg Layer #0 Tiles
	{ "14.9jk",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  7
	{ "13.8jk",			0x2000, 0x84603285, 4 | BRF_GRA },           //  8

	{ "12.10de",		0x2000, 0xfdd9e38b, 5 | BRF_GRA },           //  9 Bg Layer #1 Tiles
	{ "11.9de",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 10
	{ "10.8de",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 11

	{ "18.10pq",		0x1000, 0x6455c3ad, 6 | BRF_GRA },           // 12 Bg Layer #2 Tiles
	{ "17.9pq",			0x1000, 0x68c60d0f, 6 | BRF_GRA },           // 13
	{ "16.8pq",			0x1000, 0xce20b469, 6 | BRF_GRA },           // 14

	{ "6.10lm",			0x4000, 0x5468a21d, 7 | BRF_GRA },           // 15 Sprites
	{ "5.9lm",			0x4000, 0xf71717f8, 7 | BRF_GRA },           // 16
	{ "4.8lm",			0x4000, 0xdd9d68a4, 7 | BRF_GRA },           // 17

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 18 Unused PROM
};

STD_ROM_PICK(starforc)
STD_ROM_FN(starforc)

struct BurnDriver BurnDrvStarforc = {
	"starforc", NULL, NULL, NULL, "1984",
	"Star Force\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, starforcRomInfo, starforcRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforcInit, DrvExit, DrvFrame, StarforcDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Mega Force (World)

static struct BurnRomInfo megaforcRomDesc[] = {
	{ "3.3p",			0x4000, 0x8ba27691, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3mn",			0x4000, 0x0fc4d2d6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.3hj",			0x2000, 0x2735bb22, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
		
	{ "7.2f",			0x1000, 0x43ef8d20, 3 | BRF_GRA },           //  3 Fg Layer Tiles
	{ "8.3f",			0x1000, 0xc36fb746, 3 | BRF_GRA },           //  4
	{ "9.4f",			0x1000, 0x62e7c9ec, 3 | BRF_GRA },           //  5

	{ "15.10jk",		0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  6 Bg Layer #0 Tiles
	{ "14.9jk",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  7
	{ "13.8jk",			0x2000, 0x84603285, 4 | BRF_GRA },           //  8

	{ "12.10de",		0x2000, 0xfdd9e38b, 5 | BRF_GRA },           //  9 Bg Layer #1 Tiles
	{ "11.9de",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 10
	{ "10.8de",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 11

	{ "18.10pq",		0x1000, 0x6455c3ad, 6 | BRF_GRA },           // 12 Bg Layer #2 Tiles
	{ "17.9pq",			0x1000, 0x68c60d0f, 6 | BRF_GRA },           // 13
	{ "16.8pq",			0x1000, 0xce20b469, 6 | BRF_GRA },           // 14

	{ "6.10lm",			0x4000, 0x5468a21d, 7 | BRF_GRA },           // 15 Sprites
	{ "5.9lm",			0x4000, 0xf71717f8, 7 | BRF_GRA },           // 16
	{ "4.8lm",			0x4000, 0xdd9d68a4, 7 | BRF_GRA },           // 17

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 18 Unused PROM
};

STD_ROM_PICK(megaforc)
STD_ROM_FN(megaforc)

struct BurnDriver BurnDrvMegaforc = {
	"megaforc", "starforc", NULL, NULL, "1984",
	"Mega Force (World)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, megaforcRomInfo, megaforcRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforcInit, DrvExit, DrvFrame, StarforcDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Mega Force (US)

static struct BurnRomInfo megaforcuRomDesc[] = {
	{ "mf3.3p",			0x4000, 0xd3ea82ec, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "mf2.3mn",		0x4000, 0xaa320718, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.3hj",			0x2000, 0x2735bb22, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "7.2f",			0x1000, 0x43ef8d20, 3 | BRF_GRA },           //  3 Fg Layer Tiles
	{ "8.3f",			0x1000, 0xc36fb746, 3 | BRF_GRA },           //  4
	{ "9.4f",			0x1000, 0x62e7c9ec, 3 | BRF_GRA },           //  5

	{ "15.10jk",		0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  6 Bg Layer #0 Tiles
	{ "14.9jk",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  7
	{ "13.8jk",			0x2000, 0x84603285, 4 | BRF_GRA },           //  8

	{ "12.10de",		0x2000, 0xfdd9e38b, 5 | BRF_GRA },           //  9 Bg Layer #1 Tiles
	{ "11.9de",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 10
	{ "10.8de",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 11

	{ "18.10pq",		0x1000, 0x6455c3ad, 6 | BRF_GRA },           // 12 Bg Layer #2 Tiles
	{ "17.9pq",			0x1000, 0x68c60d0f, 6 | BRF_GRA },           // 13
	{ "16.8pq",			0x1000, 0xce20b469, 6 | BRF_GRA },           // 14

	{ "6.10lm",			0x4000, 0x5468a21d, 7 | BRF_GRA },           // 15 Sprites
	{ "5.9lm",			0x4000, 0xf71717f8, 7 | BRF_GRA },           // 16
	{ "4.8lm",			0x4000, 0xdd9d68a4, 7 | BRF_GRA },           // 17

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 18 Unused PROM
};

STD_ROM_PICK(megaforcu)
STD_ROM_FN(megaforcu)

struct BurnDriver BurnDrvMegaforcu = {
	"megaforcu", "starforc", NULL, NULL, "1985",
	"Mega Force (US)\0", NULL, "Tehkan (Video Ware license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, megaforcuRomInfo, megaforcuRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforcInit, DrvExit, DrvFrame, StarforcDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

static void sega_decode(UINT8 *rom, UINT8 *decrypted, INT32 length, INT32 table)
{
	const UINT8 suprloco_convtable[32][4] =
	{
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },
		{ 0x20,0x00,0xa0,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0xa8,0x88 }
	};

	const UINT8 yamato_convtable[32][4] =
	{
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x80,0xa0 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x80,0xa0 },
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },
		{ 0x88,0xa8,0x80,0xa0 }, { 0x20,0xa0,0x28,0xa8 },
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x20,0xa0,0x28,0xa8 },
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0xa0,0x20,0x80,0x00 }, { 0x20,0xa0,0x28,0xa8 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0x20,0xa0,0x28,0xa8 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },
		{ 0xa0,0x20,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },
		{ 0x20,0xa0,0x28,0xa8 }, { 0x00,0x08,0x20,0x28 }
	};

	const UINT8 (*convtable)[32][4];
	convtable = (table == 0) ? &suprloco_convtable : &yamato_convtable;

	memcpy (decrypted + 0x8000, rom + 0x8000, length - 0x8000);

	for (INT32 A = 0; A < 0x8000; A++)
	{
		INT32 xorval = 0;

		UINT8 src = rom[A];

		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);

		if (src & 0x80)
		{
			col = 3 - col;
			xorval = 0xa8;
		}

		decrypted[A] = (src & ~0xa8) | ((*convtable)[2*row+0][col] ^ xorval);
		rom[A]       = (src & ~0xa8) | ((*convtable)[2*row+1][col] ^ xorval);

		if ((*convtable)[2*row+0][col] == 0xff) decrypted[A] = 0xee;
		if ((*convtable)[2*row+1][col] == 0xff) rom[A] = 0xee;
	}
}

static INT32 StarforceInit()
{
	is_starforc_encrypted = 1;

	INT32 rc = StarforcInit();
	if (!rc) {
		sega_decode(DrvZ80ROM0, DrvZ80Ops0, 0x8000, 0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80Ops0,		0x0000, 0x7fff, MAP_FETCHOP);
		ZetClose();
	}

	return rc;
}

static INT32 StarforcaInit()
{
    starforce_small_sprites = 1;
	is_starforc_encrypted = 1;

	INT32 rc = StarforcInit();
	if (!rc) {
		sega_decode(DrvZ80ROM0, DrvZ80Ops0, 0x8000, 1);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80Ops0,		0x0000, 0x7fff, MAP_FETCHOP);
		ZetClose();
	}

	return rc;
}

static INT32 StarforcbInit()
{
    starforce_small_sprites = 1;

    return StarforceInit();
}

// Star Force (encrypted, bootleg)

static struct BurnRomInfo starforcbRomDesc[] = {
	{ "a2.8m",			0x2000, 0xe81e8b7d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a3.8k",			0x2000, 0x7e98f0ab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a4.8j",			0x2000, 0x285bc599, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a5.8f",			0x2000, 0x74d328b1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a0.2e",			0x2000, 0x5ab0e2fa, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "b8.8h",			0x1000, 0xf4803339, 3 | BRF_GRA },           //  5 Bg Layer #0 Tiles
	{ "b7.8f",			0x1000, 0x96979684, 3 | BRF_GRA },           //  6
	{ "b6.8d",			0x1000, 0xeead1d5c, 3 | BRF_GRA },           //  7

	{ "c17.8a",			0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  8 Bg Layer #0 Tiles
	{ "c16.7a",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  9
	{ "c15.6a",			0x2000, 0x84603285, 4 | BRF_GRA },           // 10

	{ "c20.8k",			0x2000, 0xfdd9e38b, 5 | BRF_GRA },           // 11 Bg Layer #1 Tiles
	{ "c19.7k",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 12
	{ "c18.6k",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 13

	{ "c5.8n",			0x1000, 0x6455c3ad, 6 | BRF_GRA },           // 14 Bg Layer #2 Tiles
	{ "c4.7n",			0x1000, 0x68c60d0f, 6 | BRF_GRA },           // 15
	{ "c3.6n",			0x1000, 0xce20b469, 6 | BRF_GRA },           // 16

	{ "b13.8p",			0x2000, 0x1cfc88a8, 7 | BRF_GRA },           // 17 Sprites
	{ "b14.8r",			0x2000, 0x902060b4, 7 | BRF_GRA },           // 18
	{ "b11.8m",			0x2000, 0x7676b970, 7 | BRF_GRA },           // 19
	{ "b12.8n",			0x2000, 0x6f4a5d67, 7 | BRF_GRA },           // 20
	{ "b9.8j",			0x2000, 0xe7d51959, 7 | BRF_GRA },           // 21
	{ "b10.8l",			0x2000, 0x6ea27bec, 7 | BRF_GRA },           // 22

	{ "a18s030.7b",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 23 Unused PROM
};

STD_ROM_PICK(starforcb)
STD_ROM_FN(starforcb)

struct BurnDriver BurnDrvStarforcb = {
	"starforcb", "starforc", NULL, NULL, "1984",
	"Star Force (encrypted, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, starforcbRomInfo, starforcbRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforcbInit, DrvExit, DrvFrame, StarforceDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Star Force (encrypted, set 2)

static struct BurnRomInfo starforcaRomDesc[] = {
	{ "5.bin",			0x2000, 0x7691bbd4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "4.bin",			0x2000, 0x32f3c34e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x2000, 0x5e99cfa0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.bin",			0x2000, 0x311c6e59, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1.3hj",			0x2000, 0x2735bb22, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "8.bin",			0x1000, 0xf4803339, 3 | BRF_GRA },           //  5 Fg Layer Tiles
	{ "7.bin",			0x1000, 0x96979684, 3 | BRF_GRA },           //  6
	{ "6.bin",			0x1000, 0xeead1d5c, 3 | BRF_GRA },           //  7

	{ "17.bin",			0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  8 Bg Layer #0 Tiles
	{ "16.bin",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  9
	{ "15.bin",			0x2000, 0x84603285, 4 | BRF_GRA },           // 10

	{ "20.bin",			0x2000, 0xfdd9e38b, 5 | BRF_GRA },           // 11 Bg Layer #1 Tiles
	{ "19.bin",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 12
	{ "18.bin",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 13

	{ "sw5.bin",		0x2000, 0xce6bbc11, 6 | BRF_GRA },           // 14 Bg Layer #2 Tiles
	{ "sw4.bin",		0x2000, 0xf5b4b629, 6 | BRF_GRA },           // 15
	{ "sw3.bin",		0x2000, 0x0965346d, 6 | BRF_GRA },           // 16

	{ "13.bin",			0x2000, 0x1cfc88a8, 7 | BRF_GRA },           // 17 Sprites
	{ "14.bin",			0x2000, 0x902060b4, 7 | BRF_GRA },           // 18
	{ "11.bin",			0x2000, 0x7676b970, 7 | BRF_GRA },           // 19
	{ "12.bin",			0x2000, 0x6f4a5d67, 7 | BRF_GRA },           // 20
	{ "9.bin",			0x2000, 0xe7d51959, 7 | BRF_GRA },           // 21
	{ "10.bin",			0x2000, 0x6ea27bec, 7 | BRF_GRA },           // 22
	
	{ "prom.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 23 Unused PROM
};

STD_ROM_PICK(starforca)
STD_ROM_FN(starforca)

struct BurnDriver BurnDrvStarforca = {
	"starforca", "starforc", NULL, NULL, "1984",
	"Star Force (encrypted, set 2)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, starforcaRomInfo, starforcaRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforcaInit, DrvExit, DrvFrame, StarforcDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Star Force (encrypted, set 1)

static struct BurnRomInfo starforceRomDesc[] = {
	{ "starfore.005",	0x2000, 0x825f7ebe, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "starfore.004",	0x2000, 0xfbcecb65, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "starfore.003",	0x2000, 0x9f8013b9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "starfore.002",	0x2000, 0xf8111eba, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1.3hj",			0x2000, 0x2735bb22, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "7.2fh",			0x1000, 0xf4803339, 3 | BRF_GRA },           //  5 Fg Layer Tiles
	{ "8.3fh",			0x1000, 0x96979684, 3 | BRF_GRA },           //  6
	{ "9.3fh",			0x1000, 0xeead1d5c, 3 | BRF_GRA },           //  7

	{ "15.10jk",		0x2000, 0xc3bda12f, 4 | BRF_GRA },           //  8 Bg Layer #0 Tiles
	{ "14.9jk",			0x2000, 0x9e9384fe, 4 | BRF_GRA },           //  9
	{ "13.8jk",			0x2000, 0x84603285, 4 | BRF_GRA },           // 10

	{ "12.10de",		0x2000, 0xfdd9e38b, 5 | BRF_GRA },           // 11 Bg Layer #1 Tiles
	{ "11.9de",			0x2000, 0x668aea14, 5 | BRF_GRA },           // 12
	{ "10.8de",			0x2000, 0xc62a19c1, 5 | BRF_GRA },           // 13

	{ "18.10pq",		0x1000, 0x6455c3ad, 6 | BRF_GRA },           // 14 Bg Layer #2 Tiles
	{ "17.9pq",			0x1000, 0x68c60d0f, 6 | BRF_GRA },           // 15
	{ "16.8pq",			0x1000, 0xce20b469, 6 | BRF_GRA },           // 16

	{ "6.10lm",			0x4000, 0x5468a21d, 7 | BRF_GRA },           // 17 Sprites
	{ "5.9lm",			0x4000, 0xf71717f8, 7 | BRF_GRA },           // 18
	{ "4.8lm",			0x4000, 0xdd9d68a4, 7 | BRF_GRA },           // 19

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 23 Unused PROM
};

STD_ROM_PICK(starforce)
STD_ROM_FN(starforce)

struct BurnDriver BurnDrvStarforce = {
	"starforce", "starforc", NULL, NULL, "1984",
	"Star Force (encrypted, set 1)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, starforceRomInfo, starforceRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, StarforcDIPInfo,
	StarforceInit, DrvExit, DrvFrame, StarforceDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Baluba-louk no Densetsu (Japan)

static struct BurnRomInfo balubaRomDesc[] = {
	{ "0",				0x4000, 0x0e2ebe32, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1",				0x4000, 0xcde97076, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2",				0x2000, 0x441fbc64, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "15",				0x1000, 0x3dda0d84, 3 | BRF_GRA },           //  3 Fg Tiles
	{ "16",				0x1000, 0x3ebc79d8, 3 | BRF_GRA },           //  4
	{ "17",				0x1000, 0xc4430deb, 3 | BRF_GRA },           //  5

	{ "9",				0x2000, 0x90f88c43, 4 | BRF_GRA },           //  6 Bg Layer #0 Tiles
	{ "10",				0x2000, 0xab117070, 4 | BRF_GRA },           //  7
	{ "11",				0x2000, 0xe13b44b0, 4 | BRF_GRA },           //  8

	{ "12",				0x2000, 0xa6541c8d, 5 | BRF_GRA },           //  9 Bg Layer #1 Tiles
	{ "13",				0x2000, 0xafccdd18, 5 | BRF_GRA },           // 10
	{ "14",				0x2000, 0x69542e65, 5 | BRF_GRA },           // 11

	{ "8",				0x1000, 0x31e97ef9, 6 | BRF_GRA },           // 12 Bg Layer #2 Tiles
	{ "7",				0x1000, 0x5915c5e2, 6 | BRF_GRA },           // 13
	{ "6",				0x1000, 0xad6881da, 6 | BRF_GRA },           // 14

	{ "5",				0x4000, 0x3b6b6e96, 7 | BRF_GRA },           // 15 Sprites
	{ "4",				0x4000, 0xdd954124, 7 | BRF_GRA },           // 16
	{ "3",				0x4000, 0x7ac24983, 7 | BRF_GRA },           // 17

	{ "07b.bin",		0x0020, 0x68db8300, 0 | BRF_OPT },           // 18 Unused PROM
};

STD_ROM_PICK(baluba)
STD_ROM_FN(baluba)

struct BurnDriver BurnDrvBaluba = {
	"baluba", NULL, NULL, NULL, "1986",
	"Baluba-louk no Densetsu (Japan)\0", NULL, "Able Corp, Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, balubaRomInfo, balubaRomName, NULL, NULL, NULL, NULL, SenjyoInputInfo, BalubaDIPInfo,
	StarforcInit, DrvExit, DrvFrame, StarforcDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
