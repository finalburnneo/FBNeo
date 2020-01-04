// FB Alpha Blue Print driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;

static UINT8 *watchdog;
static UINT8 *dipsw;
static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *gfx_bank;

static UINT8 Grasspin;

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];

static UINT8 DrvRecalc;

static struct BurnInputInfo BlueprntInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Blueprnt)

static struct BurnInputInfo SaturnInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Saturn)

static struct BurnInputInfo GrasspinInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Grasspin)

static struct BurnDIPInfo BlueprntDIPList[]=
{
	{0x11, 0xff, 0xff, 0xc3, NULL				},
	{0x12, 0xff, 0xff, 0xd5, NULL				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x00, "20K"				},
	{0x11, 0x01, 0x06, 0x02, "30K"				},
	{0x11, 0x01, 0x06, 0x04, "40K"				},
	{0x11, 0x01, 0x06, 0x06, "50K"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x08, 0x00, "Off"				},
	{0x11, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    2, "Maze Monster Appears In"	},
	{0x11, 0x01, 0x10, 0x00, "2nd Maze"			},
	{0x11, 0x01, 0x10, 0x10, "3rd Maze"			},

	{0   , 0xfe, 0   ,    2, "Coin A"			},
	{0x11, 0x01, 0x20, 0x20, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0x20, 0x00, "1 Coin 1 Credit"	},

	{0   , 0xfe, 0   ,    2, "Coin B"			},
	{0x11, 0x01, 0x40, 0x40, "1 Coin 3 Credits"	},
	{0x11, 0x01, 0x40, 0x00, "1 Coin 5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x00, "2"				},
	{0x12, 0x01, 0x03, 0x01, "3"				},
	{0x12, 0x01, 0x03, 0x02, "4"				},
	{0x12, 0x01, 0x03, 0x03, "5"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x08, 0x00, "Upright"			},
	{0x12, 0x01, 0x08, 0x08, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x30, 0x00, "Level 1"			},
	{0x12, 0x01, 0x30, 0x10, "Level 2"			},
	{0x12, 0x01, 0x30, 0x20, "Level 3"			},
	{0x12, 0x01, 0x30, 0x30, "Level 4"			},
};

STDDIPINFO(Blueprnt)

static struct BurnDIPInfo SaturnDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL				},
	{0x12, 0xff, 0xff, 0x04, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x11, 0x01, 0x02, 0x00, "Upright"			},
	{0x11, 0x01, 0x02, 0x02, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0x00, "3"				},
	{0x11, 0x01, 0xc0, 0x40, "4"				},
	{0x11, 0x01, 0xc0, 0x80, "5"				},
	{0x11, 0x01, 0xc0, 0xc0, "6"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x12, 0x01, 0x02, 0x02, "A 2/1 B 1/3"		},
	{0x12, 0x01, 0x02, 0x00, "A 1/1 B 1/6"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},
};

STDDIPINFO(Saturn)

static struct BurnDIPInfo GrasspinDIPList[]=
{
	{0x11, 0xff, 0xff, 0x61, NULL			},
	{0x12, 0xff, 0xff, 0x83, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x60, 0x00, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0x60, 0x40, "2 Coins 3 Credits" },
	{0x11, 0x01, 0x60, 0x60, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0x60, 0x20, "1 Coin  2 Credits" },

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x80, 0x00, "Off"			},
	{0x11, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x03, 0x00, "2"			},
	{0x12, 0x01, 0x03, 0x03, "3"			},
	{0x12, 0x01, 0x03, 0x02, "4"			},
	{0x12, 0x01, 0x03, 0x01, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x20, 0x00, "Upright"		},
	{0x12, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Grasspin)

static void __fastcall blueprint_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
		{
			*soundlatch = data;
			ZetNmi(1);
		}
		return;

		case 0xe000:
		{
			*flipscreen = ~data & 2;
			*gfx_bank   = ((data & 0x04) >> 2);
		}

		return;
	}
}

static UINT8 __fastcall blueprint_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return DrvInputs[address & 1];

		case 0xc003:
			return (Grasspin) ? (*dipsw & 0x7f) | 0x80 : *dipsw;

		case 0xe000:
			*watchdog = 0;
			return 0;
	}

	return 0;
}

static void __fastcall blueprint_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6000:
			AY8910Write(0, 0, data);
		return;

		case 0x6001:
			AY8910Write(0, 1, data);
		return;

		case 0x8000:
			AY8910Write(1, 0, data);
		return;

		case 0x8001:
			AY8910Write(1, 1, data);
		return;
	}
}

static UINT8 __fastcall blueprint_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6002:
			return AY8910Read(0);

		case 0x8002:
			return AY8910Read(1);
	}

	return 0;
}

static UINT8 ay8910_0_read_port_1(UINT32)
{
	return *soundlatch;
}

static void ay8910_0_write_port_0(UINT32, UINT32 data)
{
	*dipsw = data & 0xff;
}

static UINT8 ay8910_1_read_port_0(UINT32)
{
	return DrvDips[0];
}

static UINT8 ay8910_1_read_port_1(UINT32)
{
	return DrvDips[1];
}

static void palette_init()
{
	for (INT32 i = 0; i < 512 + 8; i++)
	{
		UINT8 pen;

		if (i < 0x200)
			pen = ((i & 0x100) >> 5) | ((i & 0x002) ? ((i & 0x0e0) >> 5) : 0) | ((i & 0x001) ? ((i & 0x01c) >> 2) : 0);
		else
			pen = i - 0x200;

		INT32 r = ((pen >> 0) & 1) * (((pen & 8) >> 1) ^ 0xff);
		INT32 g = ((pen >> 2) & 1) * (((pen & 8) >> 1) ^ 0xff);
		INT32 b = ((pen >> 1) & 1) * (((pen & 8) >> 1) ^ 0xff);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { (0x1000 * 8) * 2, (0x1000 * 8) * 1, (0x1000 * 8) * 0 };
	INT32 XOffs[8] = { STEP8(0, 1) };
	INT32 YOffs[16] = { STEP16(0, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x03000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x02000);

	GfxDecode(0x0200, 2,  8,  8, Plane + 1, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x03000);

	GfxDecode(0x0100, 3,  8, 16, Plane + 0, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x010000;
	DrvZ80ROM1	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x008000;
	DrvGfxROM1	= Next; Next += 0x008000;

	DrvPalette	= (UINT32*)Next; Next += 0x0208 * sizeof(UINT32);

	AllRam		= Next;

	DrvColRAM	= Next; Next += 0x000400;
	DrvVidRAM	= Next; Next += 0x000400;
	DrvScrollRAM	= Next; Next += 0x000100;
	DrvSprRAM	= Next; Next += 0x000100;

	DrvZ80RAM0	= Next; Next += 0x000800;
	DrvZ80RAM1	= Next; Next += 0x000800;

	dipsw		= Next; Next += 0x000001;
	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;
	gfx_bank	= Next; Next += 0x000001;
	watchdog	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
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
		INT32 lofst = 0;

		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,	0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1000,	1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,	2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x3000,	3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,	4, 1)) return 1;

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "saturnzi")) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x5000,	5, 1)) return 1;
			lofst = 1;
		}

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,	5+lofst, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,	6+lofst, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,	7+lofst, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,	8+lofst, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,	9+lofst, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,	10+lofst,1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,	11+lofst,1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x9400, 0x97ff, MAP_RAM); // mirror
	ZetMapMemory(DrvScrollRAM,	0xa000, 0xa0ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xb000, 0xb0ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xf000, 0xf3ff, MAP_RAM);
	ZetSetWriteHandler(blueprint_write);
	ZetSetReadHandler(blueprint_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1 + 0x0000,	0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM1 + 0x0000,	0x1000, 0x1fff, MAP_ROM); // mirror
	ZetMapMemory(DrvZ80ROM1 + 0x1000,	0x2000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM1 + 0x1000,	0x3000, 0x3fff, MAP_ROM); // mirror
	ZetMapMemory(DrvZ80RAM1         ,	0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(blueprint_sound_write);
	ZetSetReadHandler(blueprint_sound_read);
	ZetClose();

	AY8910Init(0, 1250000, 0);
	AY8910Init(1,  625000, 1);
	AY8910SetPorts(0, NULL, &ay8910_0_read_port_1, &ay8910_0_write_port_0, NULL);
	AY8910SetPorts(1, &ay8910_1_read_port_0, &ay8910_1_read_port_1, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);
	Grasspin = 0;

	return 0;
}

static void draw_layer(INT32 prio)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 attr = DrvColRAM[offs];
		if ((attr >> 7) != prio) continue;
		INT32 bank = (*flipscreen) ? DrvColRAM[(offs+32)&0x3ff] & 0x40 : DrvColRAM[(offs-32)&0x3ff] & 0x40;
		INT32 code = DrvVidRAM[offs];
		if (bank) code += *gfx_bank << 8;
		INT32 color = attr & 0x7f;

		INT32 sx = (~offs & 0x3e0) >> 2;
		INT32 sy = (offs & 0x1f) << 3;

		sy -= DrvScrollRAM[(30 + *flipscreen) - (sx >> 3)];
		if (sy < -7) sy += 0x100;

		if (*flipscreen) {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx ^ 0xf8, (248 - sy) - 16, color, 2, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs + 1];
		INT32 sx    = DrvSprRAM[offs + 3];
		INT32 sy    = 240 - DrvSprRAM[offs];
		INT32 flipx = (DrvSprRAM[offs + 2] >> 6) & 1;
		INT32 flipy = (DrvSprRAM[offs + 2 - 4] >> 7) & 1;

		if (*flipscreen)
		{
			sx = 248 - sx;
			sy = 240 - sy;
			flipx ^= 1;
			flipy ^= 1;
		}

		DrawCustomMaskTile(pTransDraw, 8, 16, code, sx + 2, sy - 17, flipx, flipy, 0, 3, 0, 0x200, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		palette_init();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	draw_layer(0);
	draw_sprites();
	draw_layer(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		if (*watchdog > 180) {
			DrvDoReset();
		}

		*watchdog = *watchdog + 1;
	}

	{
		DrvInputs[0] = DrvInputs[1] = 0;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		ProcessJoystick(&DrvInputs[0], 0, 6,7,4,5, INPUT_CLEAROPPOSITES);
		ProcessJoystick(&DrvInputs[1], 1, 6,7,4,5, INPUT_CLEAROPPOSITES);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3500000 / 60, 1250000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if ((i & 0x3f) == 0x3f) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
	}

	return 0;
}


// Blue Print (Midway)

static struct BurnRomInfo blueprntRomDesc[] = {
	{ "bp-1.1m",	0x1000, 0xb20069a6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bp-2.1n",	0x1000, 0x4a30302e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bp-3.1p",	0x1000, 0x6866ca07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bp-4.1r",	0x1000, 0x5d3cfac3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bp-5.1s",	0x1000, 0xa556cac4, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "snd-1.3u",	0x1000, 0xfd38777a, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "snd-2.3v",	0x1000, 0x33d5bf5b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bg-1.3c",	0x1000, 0xac2a61bc, 3 | BRF_GRA           }, //  7 Background Tiles
	{ "bg-2.3d",	0x1000, 0x81fe85d7, 3 | BRF_GRA           }, //  8

	{ "red.17d",	0x1000, 0xa73b6483, 4 | BRF_GRA           }, //  9 Sprites
	{ "blue.18d",	0x1000, 0x7d622550, 4 | BRF_GRA           }, // 10
	{ "green.20d",	0x1000, 0x2fcb4f26, 4 | BRF_GRA           }, // 11
};

STD_ROM_PICK(blueprnt)
STD_ROM_FN(blueprnt)

struct BurnDriver BurnDrvBlueprnt = {
	"blueprnt", NULL, NULL, NULL, "1982",
	"Blue Print (Midway)\0", NULL, "[Zilec Electronics] Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, blueprntRomInfo, blueprntRomName, NULL, NULL, NULL, NULL, BlueprntInputInfo, BlueprntDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x208,
	224, 256, 3, 4
};


// Blue Print (Jaleco)

static struct BurnRomInfo blueprnjRomDesc[] = {
	{ "bp-1j.1m",	0x1000, 0x2e746693, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bp-2j.1n",	0x1000, 0xa0eb0b8e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bp-3j.1p",	0x1000, 0xc34981bb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bp-4j.1r",	0x1000, 0x525e77b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bp-5j.1s",	0x1000, 0x431a015f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "snd-1.3u",	0x1000, 0xfd38777a, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "snd-2.3v",	0x1000, 0x33d5bf5b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bg-1j.3c",	0x0800, 0x43718c34, 3 | BRF_GRA           }, //  7 Background Tiles
	{ "bg-2j.3d",	0x0800, 0xd3ce077d, 3 | BRF_GRA           }, //  8

	{ "redj.17d",	0x1000, 0x83da108f, 4 | BRF_GRA           }, //  9 Sprites
	{ "bluej.18d",	0x1000, 0xb440f32f, 4 | BRF_GRA           }, // 10
	{ "greenj.20d",	0x1000, 0x23026765, 4 | BRF_GRA           }, // 11
};

STD_ROM_PICK(blueprnj)
STD_ROM_FN(blueprnj)

struct BurnDriver BurnDrvBlueprnj = {
	"blueprntj", "blueprnt", NULL, NULL, "1982",
	"Blue Print (Jaleco)\0", NULL, "[Zilec Electronics] Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, blueprnjRomInfo, blueprnjRomName, NULL, NULL, NULL, NULL, BlueprntInputInfo, BlueprntDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x208,
	224, 256, 3, 4
};


// Saturn

static struct BurnRomInfo saturnziRomDesc[] = {
	{ "r1",		0x1000, 0x18a6d68e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "r2",		0x1000, 0xa7dd2665, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3",		0x1000, 0xb9cfa791, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "r4",		0x1000, 0xc5a997e7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "r5",		0x1000, 0x43444d00, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "r6",		0x1000, 0x4d4821f6, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "r7",		0x1000, 0xdd43e02f, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code
	{ "r8",		0x1000, 0x7f9d0877, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "r10",	0x1000, 0x35987d61, 3 | BRF_GRA           }, //  8 Background Tiles
	{ "r9",		0x1000, 0xca6a7fda, 3 | BRF_GRA           }, //  9

	{ "r11",	0x1000, 0x6e4e6e5d, 4 | BRF_GRA           }, // 10 Sprites
	{ "r12",	0x1000, 0x46fc049e, 4 | BRF_GRA           }, // 11
	{ "r13",	0x1000, 0x8b3e8c32, 4 | BRF_GRA           }, // 12
};

STD_ROM_PICK(saturnzi)
STD_ROM_FN(saturnzi)

struct BurnDriver BurnDrvSaturnzi = {
	"saturnzi", NULL, NULL, NULL, "1983",
	"Saturn\0", NULL, "[Zilec Electronics] Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, saturnziRomInfo, saturnziRomName, NULL, NULL, NULL, NULL, SaturnInputInfo, SaturnDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x208,
	224, 256, 3, 4
};


// Grasspin

static struct BurnRomInfo grasspinRomDesc[] = {
	{ "prom_1.4b",		0x1000, 0x6fd50509, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (maincpu)
	{ "jaleco-2.4c",	0x1000, 0xcd319007, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jaleco-3.4d",	0x1000, 0xac73ccc2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jaleco-4.4f",	0x1000, 0x41f6279d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jaleco-5.4h",	0x1000, 0xd20aead9, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "jaleco-6.4j",	0x1000, 0xf58bf3b0, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code (audiocpu)
	{ "jaleco-7.4l",	0x1000, 0x2d587653, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "jaleco-9.4p",	0x1000, 0xbccca24c, 3 | BRF_GRA }, //  7 Background Tiles
	{ "jaleco-8.3p",	0x1000, 0x9d6185ca, 3 | BRF_GRA }, //  8

	{ "jaleco-10.5p",	0x1000, 0x3a0765c6, 4 | BRF_GRA }, //  9 Sprites
	{ "jaleco-11.6p",	0x1000, 0xcccfbeb4, 4 | BRF_GRA }, // 10
	{ "jaleco-12.7p",	0x1000, 0x615b3299, 4 | BRF_GRA }, // 11
};

STD_ROM_PICK(grasspin)
STD_ROM_FN(grasspin)

static INT32 GrasspinInit()
{
	Grasspin = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvGrasspin = {
	"grasspin", NULL, NULL, NULL, "1983",
	"Grasspin\0", NULL, "[Zilec Electronics] Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, grasspinRomInfo, grasspinRomName, NULL, NULL, NULL, NULL, GrasspinInputInfo, GrasspinDIPInfo,
	GrasspinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

