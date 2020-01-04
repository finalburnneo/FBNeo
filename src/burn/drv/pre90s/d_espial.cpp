// FB Alpha Espial / Netwars driver module
// Based on MAME driver by Brad Oliver

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvAttRAM;
static UINT8 *DrvScrollRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch[2];
static UINT8 nmi_enable[2];
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo EspialInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Espial)

static struct BurnInputInfo NetwarsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Netwars)

static struct BurnDIPInfo EspialDIPList[]=
{
	{0x10, 0xff, 0xff, 0x02, NULL						},
	{0x11, 0xff, 0xff, 0x40, NULL						},

	{0   , 0xfe, 0   ,    2, "Number of Buttons"		},
	{0x10, 0x01, 0x01, 0x01, "1"						},
	{0x10, 0x01, 0x01, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Enemy Bullets Vulnerable"	},
	{0x10, 0x01, 0x02, 0x00, "Off"						},
	{0x10, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x11, 0x01, 0x03, 0x00, "3"						},
	{0x11, 0x01, 0x03, 0x01, "4"						},
	{0x11, 0x01, 0x03, 0x02, "5"						},
	{0x11, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x11, 0x01, 0x1c, 0x14, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x18, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x1c, 0x10, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x1c, 0x1c, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x11, 0x01, 0x20, 0x00, "20k 70k 70k+"				},
	{0x11, 0x01, 0x20, 0x20, "50k 100k 100k+"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x40, 0x40, "Upright"					},
	{0x11, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Reset on Check Error"		},
	{0x11, 0x01, 0x80, 0x80, "No"						},
	{0x11, 0x01, 0x80, 0x00, "Yes"						},
};

STDDIPINFO(Espial)

static struct BurnDIPInfo NetwarsDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL						},
	{0x0f, 0xff, 0xff, 0x40, NULL						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x0e, 0x01, 0x02, 0x00, "Easy"						},
	{0x0e, 0x01, 0x02, 0x02, "Hard"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x0f, 0x01, 0x03, 0x00, "3"						},
	{0x0f, 0x01, 0x03, 0x01, "4"						},
	{0x0f, 0x01, 0x03, 0x02, "5"						},
	{0x0f, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x0f, 0x01, 0x1c, 0x14, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x18, "3 Coins 2 Credits"		},
	{0x0f, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"		},
	{0x0f, 0x01, 0x1c, 0x10, "1 Coin  6 Credits"		},
	{0x0f, 0x01, 0x1c, 0x1c, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x0f, 0x01, 0x20, 0x00, "20k and 50k"				},
	{0x0f, 0x01, 0x20, 0x20, "40k and 70k"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0f, 0x01, 0x40, 0x40, "Upright"					},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Reset on Check Error"		},
	{0x0f, 0x01, 0x80, 0x80, "No"						},
	{0x0f, 0x01, 0x80, 0x00, "Yes"						},
};

STDDIPINFO(Netwars)

static void __fastcall espial_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x8000) {
		DrvSprRAM0[address & 0x1f] = data;
		return;
	}

	switch (address)
	{
		case 0x6090:
			soundlatch[0] = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x7000:
			BurnWatchdogWrite();
		return;

		case 0x7100:
			nmi_enable[0] = ~(data & 1);
		return;

		case 0x7200:
			flipscreen = data;
		return;
	}
}

static UINT8 __fastcall espial_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x6081:
			return (DrvInputs[0] & 0xf0) | (DrvDips[0] & 0x0f);

		case 0x6082:
			return DrvDips[1];

		case 0x6083:
			return DrvInputs[1];

		case 0x6084:
			return DrvInputs[2];

		case 0x6090:
			return soundlatch[1];

		case 0x7000:
			return BurnWatchdogRead();
	}

	return 0;
}

static void __fastcall espial_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			nmi_enable[1] = data & 1;
		return;

		case 0x6000:
			soundlatch[1] = data;
		return;
	}
}

static UINT8 __fastcall espial_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch[0];
	}

	return 0;
}

static void __fastcall espial_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvAttRAM[offs];

	TILE_SET_INFO(0, DrvVidRAM[offs] + ((attr & 3) * 256), DrvColRAM[offs], TILE_FLIPYX(attr >> 2));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);

		soundlatch[0] = 0;
		soundlatch[1] = 0;
		nmi_enable[0] = 0;
		nmi_enable[1] = 0;
		flipscreen = 0;
	}

	ZetReset(0);

	ZetReset(1);

	AY8910Reset(0);

	BurnWatchdogReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x0100000;
	DrvZ80ROM1		= Next; Next += 0x0020000;

	DrvGfxROM0		= Next; Next += 0x0020000;
	DrvGfxROM1		= Next; Next += 0x0080000;

	DrvColPROM		= Next; Next += 0x0002000;

	DrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x0008000;
	DrvZ80RAM1		= Next; Next += 0x0004000;

	DrvSprRAM0		= Next; Next += 0x0001000;
	DrvSprRAM1		= Next; Next += 0x0001000;
	DrvSprRAM2		= Next; Next += 0x0001000;

	DrvVidRAM		= Next; Next += 0x0008000;
	DrvColRAM		= Next; Next += 0x0008000;
	DrvAttRAM		= Next; Next += 0x0008000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 XOffs0[8] = { STEP4(0,1), STEP4(64,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };
	INT32 Plane1[2] = { 0, 0x1000*8 };
	INT32 XOffs1[16]= { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs1[16]= { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0080, 2, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 EspialInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xc000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 11, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x4fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0x5800, 0x5fff, MAP_RAM);
//	ZetMapMemory(DrvSprRAM0,			0x8000, 0x80ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM2,			0x8800,	0x88ff, MAP_WRITE);
	ZetMapMemory(DrvAttRAM,				0x8c00, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,			0x9000, 0x90ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0x9400, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0xc000,	0xc000, 0xcfff, MAP_ROM);
	ZetSetWriteHandler(espial_main_write);
	ZetSetReadHandler(espial_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(espial_sound_write);
	ZetSetReadHandler(espial_sound_read);
	ZetSetOutHandler(espial_sound_write_port);
	ZetClose();

	DrvScrollRAM = DrvSprRAM1 + 0x20;

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1500000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x10000, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 NetwarsInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100,  9, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0x5800, 0x5fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvAttRAM,				0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0x9000, 0x97ff, MAP_RAM);
	ZetSetWriteHandler(espial_main_write);
	ZetSetReadHandler(espial_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(espial_sound_write);
	ZetSetReadHandler(espial_sound_read);
	ZetSetOutHandler(espial_sound_write_port);
	ZetClose();

	DrvSprRAM0 = DrvVidRAM;
	DrvSprRAM1 = DrvColRAM;
	DrvSprRAM2 = DrvAttRAM;
	DrvScrollRAM = DrvColRAM + 0x20;

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1500000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x10000, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static inline void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
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
	for (INT32 offs = 0; offs < 16; offs++)
	{
		INT32 sx    = DrvSprRAM0[offs + 16];
		INT32 sy    = DrvSprRAM1[offs];
		INT32 code  = DrvSprRAM0[offs] >> 1;
		INT32 color = DrvSprRAM1[offs + 16];
		INT32 flipx = DrvSprRAM2[offs] & 0x04;
		INT32 flipy = DrvSprRAM2[offs] & 0x08;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
		}
		else
		{
			sy = 240 - sy;
		}

		if (DrvSprRAM0[offs] & 0x01)
		{
			draw_single_sprite(code + 0, color, sx, sy + ((flipscreen) ? 16 : -16), flipx, flipy);
			draw_single_sprite(code + 1, color, sx, sy, flipx, flipy);
		}
		else
		{
			draw_single_sprite(code, color, sx, sy, flipx, flipy);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvScrollRAM[i]); // ??
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 16) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		if (i == 240) {
			if (nmi_enable[0] != 0) ZetNmi();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if ((i & 0x3f) == 0x3f && nmi_enable[1] != 0) ZetNmi(); // 4x per frame
		ZetClose();

		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}
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
		BurnWatchdogScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Espial (Europe)

static struct BurnRomInfo espialRomDesc[] = {
	{ "esp3.4f",		0x2000, 0x0973c8a4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "esp4.4h",		0x2000, 0x6034d7e5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "esp6.bin",		0x1000, 0x357025b4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "esp5.bin",		0x1000, 0xd03a2fc4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "esp1.4n",		0x1000, 0xfc7729e9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "esp2.4r",		0x1000, 0xe4e256da, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "espial8.4b",		0x2000, 0x2f43036f, 3 | BRF_GRA },           //  6 Background Tiles
	{ "espial7.4a",		0x1000, 0xebfef046, 3 | BRF_GRA },           //  7

	{ "espial10.4e",	0x1000, 0xde80fbc1, 4 | BRF_GRA },           //  8 Sprites
	{ "espial9.4d",		0x1000, 0x48c258a0, 4 | BRF_GRA },           //  9

	{ "mmi6301.1f",		0x0100, 0xd12de557, 5 | BRF_GRA },           // 10 Color data
	{ "mmi6301.1h",		0x0100, 0x4c84fe70, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(espial)
STD_ROM_FN(espial)

struct BurnDriver BurnDrvEspial = {
	"espial", NULL, NULL, NULL, "1983",
	"Espial (Europe)\0", NULL, "Orca / Thunderbolt", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, espialRomInfo, espialRomName, NULL, NULL, NULL, NULL, EspialInputInfo, EspialDIPInfo,
	EspialInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Espial (US?)

static struct BurnRomInfo espialuRomDesc[] = {
	{ "espial3.4f",		0x2000, 0x10f1da30, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "espial4.4h",		0x2000, 0xd2adbe39, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "espial.6",		0x1000, 0xbaa60bc1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "espial.5",		0x1000, 0x6d7bbfc1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "espial1.4n",		0x1000, 0x1e5ec20b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "espial2.4r",		0x1000, 0x3431bb97, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "espial8.4b",		0x2000, 0x2f43036f, 3 | BRF_GRA },           //  6 Background Tiles
	{ "espial7.4a",		0x1000, 0xebfef046, 3 | BRF_GRA },           //  7

	{ "espial10.4e",	0x1000, 0xde80fbc1, 4 | BRF_GRA },           //  8 Sprites
	{ "espial9.4d",		0x1000, 0x48c258a0, 4 | BRF_GRA },           //  9

	{ "mmi6301.1f",		0x0100, 0xd12de557, 5 | BRF_GRA },           // 10 Color data
	{ "mmi6301.1h",		0x0100, 0x4c84fe70, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(espialu)
STD_ROM_FN(espialu)

struct BurnDriver BurnDrvEspialu = {
	"espialu", "espial", NULL, NULL, "1983",
	"Espial (US?)\0", NULL, "Orca / Thunderbolt", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, espialuRomInfo, espialuRomName, NULL, NULL, NULL, NULL, EspialInputInfo, EspialDIPInfo,
	EspialInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Net Wars

static struct BurnRomInfo netwarsRomDesc[] = {
	{ "netw3.4f",	0x2000, 0x8e782991, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "netw4.4h",	0x2000, 0x6e219f61, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "netw1.4n",	0x1000, 0x53939e16, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "netw2.4r",	0x1000, 0xc096317a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "netw8.4b",	0x2000, 0x2320277e, 3 | BRF_GRA },           //  4 Background Tiles
	{ "netw7.4a",	0x2000, 0x25cc5b7f, 3 | BRF_GRA },           //  5

	{ "netw10.4e",	0x1000, 0x87b65625, 4 | BRF_GRA },           //  6 Sprites
	{ "netw9.4d",	0x1000, 0x830d0218, 4 | BRF_GRA },           //  7

	{ "mmi6301.1f",	0x0100, 0xf3ae1fe2, 5 | BRF_GRA },           //  8 Color data
	{ "mmi6301.1h",	0x0100, 0xc44c3771, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(netwars)
STD_ROM_FN(netwars)

struct BurnDriver BurnDrvNetwars = {
	"netwars", NULL, NULL, NULL, "1983",
	"Net Wars\0", NULL, "Orca (Esco Trading Co license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, netwarsRomInfo, netwarsRomName, NULL, NULL, NULL, NULL, NetwarsInputInfo, NetwarsDIPInfo,
	NetwarsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
