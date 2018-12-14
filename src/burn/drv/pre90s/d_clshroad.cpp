// FB Alpha Clash Road / Fire Battle driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "wiping.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *irq_mask;
static UINT8 *video_regs;
static UINT8 flipscreen;
static UINT8 sound_reset;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo ClshroadInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Clshroad)

static struct BurnInputInfo FirebatlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Firebatl)

static struct BurnDIPInfo ClshroadDIPList[]=
{
	{0x10, 0xff, 0xff, 0xfb, NULL					},
	{0x11, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x04, 0x00, "Upright"				},
	{0x10, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x18, 0x18, "Normal"				},
	{0x10, 0x01, 0x18, 0x10, "Hard"					},
	{0x10, 0x01, 0x18, 0x08, "Harder"				},
	{0x10, 0x01, 0x18, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x10, 0x01, 0x20, 0x20, "Off"					},
	{0x10, 0x01, 0x20, 0x00, "On"					},
};

STDDIPINFO(Clshroad)

static struct BurnDIPInfo FirebatlDIPList[]=
{
	{0x10, 0xff, 0xff, 0x03, NULL					},
	{0x11, 0xff, 0xff, 0x13, NULL					},

	{0   , 0xfe, 0   ,    8, "Lives"				},
	{0x10, 0x01, 0x7f, 0x00, "1"					},
	{0x10, 0x01, 0x7f, 0x01, "2"					},
	{0x10, 0x01, 0x7f, 0x03, "3"					},
	{0x10, 0x01, 0x7f, 0x07, "4"					},
	{0x10, 0x01, 0x7f, 0x0f, "5"					},
	{0x10, 0x01, 0x7f, 0x1f, "6"					},
	{0x10, 0x01, 0x7f, 0x3f, "7"					},
	{0x10, 0x01, 0x7f, 0x7f, "Infinite Lives"		},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x11, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x04, 0x00, "Upright"				},
	{0x11, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x08, 0x08, "Off"					},
	{0x11, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x10, 0x10, "10K 30K+"				},
	{0x11, 0x01, 0x10, 0x00, "20K 30K+"				},
};

STDDIPINFO(Firebatl)

static void __fastcall clshroad_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			sound_reset = ~data & 1;
			if (sound_reset ) {
				INT32 active = ZetGetActive();
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(active);
			}
		return;

		case 0xa001:
			irq_mask[0] = data & 1;
		return;

		case 0xa003:
			irq_mask[1] = data & 1;
		return;

		case 0xa004:
			flipscreen = data & 1;
		return;

		case 0xb000:
		case 0xb001:
		case 0xb002:
		case 0xb003:
			video_regs[address & 3] = data;
		return;
	}
}

static UINT8 __fastcall clshroad_main_read(UINT16 address)
{
	if ((address & 0xfff8) == 0xa100) {
		return DrvInputs[address & 7];
	}

	return 0;
}

static void __fastcall clshroad_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x4000) {
		wipingsnd_write(address, data);
		return;
	}

	if ((address & 0xfff8) == 0xa000) {
		clshroad_main_write(address, data);
		return;
	}
}

static tilemap_scan( bg )
{
	return (col + (row * 64)) * 2;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM0[offs + 0x40], 0, 0);
}

static tilemap_callback( mg )
{
	TILE_SET_INFO(0, DrvVidRAM0[offs + 0x00], 0, 0);
}

static tilemap_scan( fg )
{
	if (col <=  1) return row + (col + 30) * 0x20;
	if (col >= 34) return row + (col - 34) * 0x20;
	if (row <=  1 || row >= 30) return 0;

	return (col - 2) + row * 32;
}

static tilemap_callback( fg )
{
	UINT8 attr = DrvVidRAM1[offs + 0x400];

	TILE_SET_INFO(1, DrvVidRAM1[offs] + ((attr & 0x10) << 4), attr, 0);
	*category = attr & 0x3f;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memset (DrvVidRAM0, 0xf0, 0x800); // firebatl does not clear this on init

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	wipingsnd_reset();

	flipscreen = 0;
	sound_reset = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvSndROM		= Next; Next += 0x002000;
	DrvSndPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000200;
	DrvSprRAM		= Next; Next += 0x000200;

	irq_mask		= Next; Next += 0x000002;
	video_regs		= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0x4000 * 8 + 0, 0x4000 * 8 + 4, 0, 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(128,1), STEP4(128+8,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x0200, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (game_select == 0) // firebatl
		{
			if (BurnLoadRom(DrvZ80ROM0		+ 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0		+ 0x2000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0		+ 0x4000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1		+ 0x0000,  3, 1)) return 1;

			if (BurnLoadRomExt(DrvGfxROM0	+ 0x0000,  4, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x2000,  5, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x4000,  6, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x6000,  7, 1, LD_INVERT)) return 1;

			if (BurnLoadRomExt(DrvGfxROM1	+ 0x0000,  8, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x2000,  9, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x4000, 10, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x6000, 11, 1, LD_INVERT)) return 1;

			if (BurnLoadRom(DrvGfxROM2	    + 0x0000, 12, 1)) return 1;

			if (BurnLoadRom(DrvColPROM		+ 0x0000, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0100, 14, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0200, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0300, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0400, 17, 1)) return 1;

			if (BurnLoadRom(DrvSndROM		+ 0x0000, 18, 1)) return 1;

			if (BurnLoadRom(DrvSndPROM		+ 0x0000, 19, 1)) return 1;
			if (BurnLoadRom(DrvSndPROM		+ 0x0100, 20, 1)) return 1;
		}
		else if (game_select == 1) // clshroad
		{
			if (BurnLoadRom(DrvZ80ROM0		+ 0x0000,  0, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1		+ 0x0000,  1, 1)) return 1;

			if (BurnLoadRomExt(DrvGfxROM0	+ 0x0000,  2, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x4000,  3, 1, LD_INVERT)) return 1;

			if (BurnLoadRomExt(DrvGfxROM1	+ 0x0000,  4, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x4000,  5, 1, LD_INVERT)) return 1;

			if (BurnLoadRomExt(DrvGfxROM2	+ 0x0000,  6, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM2	+ 0x4000,  7, 1, LD_INVERT)) return 1;

			if (BurnLoadRom(DrvColPROM		+ 0x0000,  8, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0100,  9, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0200, 10, 1)) return 1;

			if (BurnLoadRom(DrvSndROM		+ 0x0000, 11, 1)) return 1;

			if (BurnLoadRom(DrvSndPROM		+ 0x0000, 12, 1)) return 1;
			if (BurnLoadRom(DrvSndPROM		+ 0x0100, 13, 1)) return 1;
		}
		else if (game_select == 2) // clshroads
		{
			if (BurnLoadRom(DrvZ80ROM0		+ 0x0000,  0, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1		+ 0x0000,  1, 1)) return 1;

			if (BurnLoadRomExt(DrvGfxROM0	+ 0x0000,  2, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x2000,  3, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x4000,  4, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM0	+ 0x6000,  5, 1, LD_INVERT)) return 1;

			if (BurnLoadRomExt(DrvGfxROM1	+ 0x0000,  6, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x2000,  7, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x4000,  8, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM1	+ 0x6000,  9, 1, LD_INVERT)) return 1;

			if (BurnLoadRomExt(DrvGfxROM2	+ 0x0000, 10, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM2	+ 0x4000, 11, 1, LD_INVERT)) return 1;

			if (BurnLoadRom(DrvColPROM		+ 0x0000, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0100, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM		+ 0x0200, 14, 1)) return 1;

			if (BurnLoadRom(DrvSndROM		+ 0x0000, 15, 1)) return 1;

			if (BurnLoadRom(DrvSndPROM		+ 0x0000, 16, 1)) return 1;
			if (BurnLoadRom(DrvSndPROM		+ 0x0100, 17, 1)) return 1;
		}

		for (INT32 i = 0; i < 0x300; i++) { // colors 4bit->8bit
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[i] << 4);
		}

		for (INT32 i = 0x300; i < 0x400; i++) { // lut merge nibbles
			DrvColPROM[i] = (DrvColPROM[i] << 4) | (DrvColPROM[i + 0x100] & 0xf);
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0x9600, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9e00, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,		0xa800, 0xafff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(clshroad_main_write);
	ZetSetReadHandler(clshroad_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x9600, 0x97ff, MAP_RAM);
	ZetSetWriteHandler(clshroad_sound_write);
	ZetClose();

	wipingsnd_init(DrvSndROM, DrvSndPROM);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(1, bg_map_scan, mg_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(2, fg_map_scan, fg_map_callback,  8,  8, 36, 32);
	GenericTilemapSetOffsets(2,   0, -16);

	if (game_select == 0) // firebatl
	{
		GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x10000, 0x010, 0x00);
		GenericTilemapSetGfx(1, DrvGfxROM2, 2,  8,  8, 0x04000, 0x100, 0x3f);
		GenericTilemapSetOffsets(0, -42, -16);
		GenericTilemapSetOffsets(1, -42, -16);
		GenericTilemapSetTransparent(1, 0);

		GenericTilemapCategoryConfig(2, 0x40);
		for (INT32 i = 0; i < 0x40; i++)
		{
			for (INT32 j = 0; j < 4; j++)
			{
				GenericTilemapSetCategoryEntry(2, i, j, DrvColPROM[0x300 + (i * 4) + j] == 0xf);
			}
		}
	}
	else // clash
	{
		GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x10000, 0x090, 0x00);
		GenericTilemapSetGfx(1, DrvGfxROM2, 4,  8,  8, 0x08000, 0x000, 0x0f);
		GenericTilemapSetTransparent(1, 0xf);
		GenericTilemapSetTransparent(2, 0xf);
		GenericTilemapSetOffsets(0, -48, -16);
		GenericTilemapSetOffsets(1, -48, -16);
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	wipingsnd_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++) {
		DrvPalette[i] = BurnHighCol(DrvColPROM[0x000 + i], DrvColPROM[0x100 + i], DrvColPROM[0x200 + i], 0);
	}

	for (INT32 i = 0; i < 0x100; i++) { // firebatl
		DrvPalette[0x100 + i] = DrvPalette[DrvColPROM[0x300 + i]];
	}
}

static void DrvSpriteDraw()
{
	for (INT32 i = 0; i < 0x200; i += 8)
	{
		INT32 sy	=  DrvSprRAM[i+1];
		INT32 sx	=  DrvSprRAM[i+5] + (DrvSprRAM[i+6] << 8);
		INT32 code	= (DrvSprRAM[i+3] & 0x3f) + (DrvSprRAM[i+2] << 6);
		INT32 color	=  DrvSprRAM[i+7] & 0xf;

		if (flipscreen) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code & 0xff, sx - 37, sy - 16, color, 4, 0xf, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code & 0xff, sx - 37, (240 - sy) - 16, color, 4, 0xf, 0, DrvGfxROM0);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	INT32 scrollx = video_regs[0] + (video_regs[1] << 8);

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollX(1, scrollx);

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, pTransDraw, 0);
	if ((nBurnLayer & 2) != 0) GenericTilemapDraw(1, pTransDraw, 0);

	if ((nSpriteEnable & 1) != 0) DrvSpriteDraw();

	if ((nBurnLayer & 4) != 0) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[i]  =  ( DrvJoy1[i] & 1) << 0;
			DrvInputs[i] |=  ( DrvJoy2[i] & 1) << 1;
			DrvInputs[i] |= ((~DrvDips[0] >> i) & 1) << 2;
			DrvInputs[i] |= ((~DrvDips[1] >> i) & 1) << 3;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3686400 / 60, 3686400 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	INT32 nSndIRQ = (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) ? 127 : 255; // firebatl 2x / frame, clsroad 1x

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		if (i == 240) {
			if (irq_mask[0]) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (pBurnDraw) { // broken sprites on bike legs/pedals when street is on an incline fixed by drawing here
				BurnDrvRedraw();
			}
		}
		INT32 nCycles = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		if (sound_reset == 0) {
			nCyclesDone[1] += ZetRun(nCycles - ZetTotalCycles());
			if ((i & nSndIRQ) == nSndIRQ && irq_mask[1]) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); 
		} else {
			nCyclesDone[1] += (nCycles - ZetTotalCycles());
			ZetIdle(nCycles - ZetTotalCycles());
		}
		ZetClose();
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		wipingsnd_update(pBurnSoundOut, nBurnSoundLen);
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
		wipingsnd_scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(sound_reset);
	}

	return 0;
}


// Fire Battle

static struct BurnRomInfo firebatlRomDesc[] = {
	{ "rom01.e8",		0x2000, 0x10e24ef6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rom02.d8",		0x2000, 0x47f79bee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom03.c8",		0x2000, 0x693459b9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "rom04.r6",		0x2000, 0x5f232d9a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "rom14.f4",		0x2000, 0x36a508a7, 3 | BRF_GRA },           //  4 Sprites
	{ "rom13.h4",		0x2000, 0xa2ec508e, 3 | BRF_GRA },           //  5
	{ "rom12.j4",		0x2000, 0xf80ece92, 3 | BRF_GRA },           //  6
	{ "rom11.k4",		0x2000, 0xb293e701, 3 | BRF_GRA },           //  7

	{ "rom09.n4",		0x2000, 0x77ea3e39, 4 | BRF_GRA },           //  8 Background Tiles
	{ "rom08.p4",		0x2000, 0x1b7585dd, 4 | BRF_GRA },           //  9
	{ "rom07.s4",		0x2000, 0xe3ec9825, 4 | BRF_GRA },           // 10
	{ "rom06.u4",		0x2000, 0xd29fab5f, 4 | BRF_GRA },           // 11

	{ "rom15.m4",		0x1000, 0x8b5464d6, 5 | BRF_GRA },           // 12 Foreground Tiles

	{ "prom6.h1",		0x0100, 0xb117d22c, 6 | BRF_GRA },           // 13 Color data
	{ "prom7.f1",		0x0100, 0x9b6b4f56, 6 | BRF_GRA },           // 14
	{ "prom8.e1",		0x0100, 0x67cb68ae, 6 | BRF_GRA },           // 15
	{ "prom9.p2",		0x0100, 0xdd015b80, 6 | BRF_GRA },           // 16
	{ "prom10.n2",		0x0100, 0x71b768c7, 6 | BRF_GRA },           // 17

	{ "rom05.f3",		0x2000, 0x21544cd6, 7 | BRF_SND },           // 18 Samples

	{ "prom3.j4",		0x0100, 0xbd2c080b, 8 | BRF_GRA },           // 19 Sample PROMs
	{ "prom2.k2",		0x0100, 0x4017a2a6, 8 | BRF_GRA },           // 20

	{ "prom4.s1",		0x0100, 0x06523b81, 0 | BRF_OPT },           // 21 Unused PROMs
	{ "prom5.p1",		0x0100, 0x75ea8f70, 0 | BRF_OPT },           // 22
	{ "prom11.m2",		0x0100, 0xba42a582, 0 | BRF_OPT },           // 23
	{ "prom12.h2",		0x0100, 0xf2540c51, 0 | BRF_OPT },           // 24
	{ "prom13.w8",		0x0100, 0x4e2a2781, 0 | BRF_OPT },           // 25
	{ "prom1.n2",		0x0020, 0x1afc04f0, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(firebatl)
STD_ROM_FN(firebatl)

static void FirebatlPatch()
{
	// Without this the death sequence never ends so the game is unplayable after you die once,
	DrvZ80ROM0[0x05C6] = 0xc3;
	DrvZ80ROM0[0x05C7] = 0x8d;
	DrvZ80ROM0[0x05C8] = 0x23;
}

static INT32 FirebatlInit()
{
	INT32 nRet = DrvInit(0);

	if (nRet == 0)
	{
		FirebatlPatch();
	}

	return nRet;
}

struct BurnDriver BurnDrvFirebatl = {
	"firebatl", NULL, NULL, NULL, "1984",
	"Fire Battle\0", NULL, "Wood Place Inc. (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, firebatlRomInfo, firebatlRomName, NULL, NULL, NULL, NULL, FirebatlInputInfo, FirebatlDIPInfo,
	FirebatlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4 
};


// Clash-Road

static struct BurnRomInfo clshroadRomDesc[] = {
	{ "clashr3.bin",	0x8000, 0x865c32ae, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "clashr2.bin",	0x2000, 0xe6389ec1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "clashr5.bin",	0x4000, 0x094858b8, 3 | BRF_GRA },           //  2 Sprites
	{ "clashr6.bin",	0x4000, 0xdaa1daf3, 3 | BRF_GRA },           //  3

	{ "clashr8.bin",	0x4000, 0xcbb66719, 4 | BRF_GRA },           //  4 Background Tiles
	{ "clashr9.bin",	0x4000, 0xc15e8eed, 4 | BRF_GRA },           //  5

	{ "clashr4.bin",	0x2000, 0x664201d9, 5 | BRF_GRA },           //  6 Foreground Tiles
	{ "clashr7.bin",	0x2000, 0x97973030, 5 | BRF_GRA },           //  7

	{ "82s129.6",		0x0100, 0x38f443da, 6 | BRF_GRA },           //  8 Color data
	{ "82s129.7",		0x0100, 0x977fab0c, 6 | BRF_GRA },           //  9
	{ "82s129.8",		0x0100, 0xae7ae54d, 6 | BRF_GRA },           // 10

	{ "clashr1.bin",	0x2000, 0x0d0a8068, 7 | BRF_SND },           // 11 Samples

	{ "clashrd.g8",		0x0100, 0xbd2c080b, 8 | BRF_GRA },           // 12 Sample PROMs
	{ "clashrd.g7",		0x0100, 0x4017a2a6, 8 | BRF_GRA },           // 13

	{ "clashrd.a2",		0x0100, 0x4e2a2781, 0 | BRF_OPT },           // 14 Unused PROMs
	{ "clashrd.g4",		0x0020, 0x1afc04f0, 0 | BRF_OPT },           // 15
	{ "clashrd.b11",	0x0020, 0xd453f2c5, 0 | BRF_OPT },           // 16
	{ "clashrd.g10",	0x0100, 0x73afefd0, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(clshroad)
STD_ROM_FN(clshroad)

static INT32 ClshroadInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvClshroad = {
	"clshroad", NULL, NULL, NULL, "1986",
	"Clash-Road\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, clshroadRomInfo, clshroadRomName, NULL, NULL, NULL, NULL, ClshroadInputInfo, ClshroadDIPInfo,
	ClshroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	288, 224, 4, 3
};


// Clash-Road (Data East license)

static struct BurnRomInfo clshroaddRomDesc[] = {
	{ "crdeco-3.bin",	0x8000, 0x1d54195c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "clashr2.bin",	0x2000, 0xe6389ec1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "clashr5.bin",	0x4000, 0x094858b8, 3 | BRF_GRA },           //  2 Sprites
	{ "clashr6.bin",	0x4000, 0xdaa1daf3, 3 | BRF_GRA },           //  3

	{ "clashr8.bin",	0x4000, 0xcbb66719, 4 | BRF_GRA },           //  4 Background Tiles
	{ "clashr9.bin",	0x4000, 0xc15e8eed, 4 | BRF_GRA },           //  5

	{ "clashr4.bin",	0x2000, 0x664201d9, 5 | BRF_GRA },           //  6 Foreground Tiles
	{ "clashr7.bin",	0x2000, 0x97973030, 5 | BRF_GRA },           //  7

	{ "82s129.6",		0x0100, 0x38f443da, 6 | BRF_GRA },           //  8 Color data
	{ "82s129.7",		0x0100, 0x977fab0c, 6 | BRF_GRA },           //  9
	{ "82s129.8",		0x0100, 0xae7ae54d, 6 | BRF_GRA },           // 10

	{ "clashr1.bin",	0x2000, 0x0d0a8068, 7 | BRF_SND },           // 11 Samples

	{ "clashrd.g8",		0x0100, 0xbd2c080b, 8 | BRF_GRA },           // 12 Sample PROMs
	{ "clashrd.g7",		0x0100, 0x4017a2a6, 8 | BRF_GRA },           // 13

	{ "clashrd.a2",		0x0100, 0x4e2a2781, 0 | BRF_OPT },           // 14 Unused PROMs
	{ "clashrd.g4",		0x0020, 0x1afc04f0, 0 | BRF_OPT },           // 15
	{ "clashrd.b11",	0x0020, 0xd453f2c5, 0 | BRF_OPT },           // 16
	{ "clashrd.g10",	0x0100, 0x73afefd0, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(clshroadd)
STD_ROM_FN(clshroadd)

struct BurnDriver BurnDrvClshroadd = {
	"clshroadd", "clshroad", NULL, NULL, "1986",
	"Clash-Road (Data East license)\0", NULL, "Wood Place Inc. (Data East license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, clshroaddRomInfo, clshroaddRomName, NULL, NULL, NULL, NULL, ClshroadInputInfo, ClshroadDIPInfo,
	ClshroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	288, 224, 4, 3
};


// Clash-Road (Status license)

static struct BurnRomInfo clshroadsRomDesc[] = {
	{ "cr-3",			0x8000, 0x23559df2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "clashr2.bin",	0x2000, 0xe6389ec1, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "cr-12",			0x2000, 0xe5aa4c46, 3 | BRF_GRA },           //  2 Sprites
	{ "cr-11",			0x2000, 0x7fc11c7c, 3 | BRF_GRA },           //  3
	{ "cr-10",			0x2000, 0x6b1293b7, 3 | BRF_GRA },           //  4
	{ "cr-9",			0x2000, 0xd219722c, 3 | BRF_GRA },           //  5

	{ "cr-7",			0x2000, 0xe8aa7ac3, 4 | BRF_GRA },           //  6 Background Tiles
	{ "cr-6",			0x2000, 0x037be475, 4 | BRF_GRA },           //  7
	{ "cr-5",			0x2000, 0xa4151734, 4 | BRF_GRA },           //  8
	{ "cr-4",			0x2000, 0x5ef24757, 4 | BRF_GRA },           //  9

	{ "cr-13",			0x2000, 0x012a6412, 5 | BRF_GRA },           // 10 Foreground Tiles
	{ "cr-8",			0x2000, 0x3c2b816c, 5 | BRF_GRA },           // 11

	{ "82s129.6",		0x0100, 0x38f443da, 6 | BRF_GRA },           // 12 Color data
	{ "82s129.7",		0x0100, 0x977fab0c, 6 | BRF_GRA },           // 13
	{ "82s129.8",		0x0100, 0xae7ae54d, 6 | BRF_GRA },           // 14

	{ "clashr1.bin",	0x2000, 0x0d0a8068, 7 | BRF_SND },           // 15 Samples

	{ "clashrd.g8",		0x0100, 0xbd2c080b, 8 | BRF_GRA },           // 16 Sample PROMs
	{ "clashrd.g7",		0x0100, 0x4017a2a6, 8 | BRF_GRA },           // 17

	{ "clashrd.a2",		0x0100, 0x4e2a2781, 0 | BRF_OPT },           // 18 Unused PROMs
	{ "clashrd.g4",		0x0020, 0x1afc04f0, 0 | BRF_OPT },           // 19
	{ "clashrd.b11",	0x0020, 0xd453f2c5, 0 | BRF_OPT },           // 20
	{ "clashrd.g10",	0x0100, 0x73afefd0, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(clshroads)
STD_ROM_FN(clshroads)

static INT32 ClshroadsInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvClshroads = {
	"clshroads", "clshroad", NULL, NULL, "1986",
	"Clash-Road (Status license)\0", NULL, "Wood Place Inc. (Status Game Corp. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, clshroadsRomInfo, clshroadsRomName, NULL, NULL, NULL, NULL, ClshroadInputInfo, ClshroadDIPInfo,
	ClshroadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	288, 224, 4, 3
};
