// FB Alpha Bomb Jack driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 background_image;
static UINT8 nmi_mask;
static UINT8 flipscreen;
static UINT8 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo BombjackInputList[] = {
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

STDINPUTINFO(Bombjack)

static struct BurnDIPInfo BombjackDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xc0, NULL			},
	{0x10, 0xff, 0xff, 0x50, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x30, "2"			},
	{0x0f, 0x01, 0x30, 0x00, "3"			},
	{0x0f, 0x01, 0x30, 0x10, "4"			},
	{0x0f, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x40, 0x40, "Upright"		},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x80, 0x00, "Off"			},
	{0x0f, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x10, 0x01, 0x07, 0x02, "Every 30k"		},
	{0x10, 0x01, 0x07, 0x01, "Every 100k"		},
	{0x10, 0x01, 0x07, 0x07, "50k, 100k and 300k"	},
	{0x10, 0x01, 0x07, 0x05, "50k and 100k"		},
	{0x10, 0x01, 0x07, 0x03, "50k only"		},
	{0x10, 0x01, 0x07, 0x06, "100k and 300k"	},
	{0x10, 0x01, 0x07, 0x04, "100k only"		},
	{0x10, 0x01, 0x07, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Bird Speed"		},
	{0x10, 0x01, 0x18, 0x00, "Easy"			},
	{0x10, 0x01, 0x18, 0x08, "Medium"		},
	{0x10, 0x01, 0x18, 0x10, "Hard"			},
	{0x10, 0x01, 0x18, 0x18, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Enemies Number & Speed"},
	{0x10, 0x01, 0x60, 0x20, "Easy"			},
	{0x10, 0x01, 0x60, 0x00, "Medium"		},
	{0x10, 0x01, 0x60, 0x40, "Hard"			},
	{0x10, 0x01, 0x60, 0x60, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Special Coin"		},
	{0x10, 0x01, 0x80, 0x00, "Easy"			},
	{0x10, 0x01, 0x80, 0x80, "Hard"			},
};

STDDIPINFO(Bombjack)

static void __fastcall bombjack_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9a00:
		return;	//nop

		case 0x9e00:
			background_image = (data & 0x17);
		return;

		case 0xb000:
			nmi_mask = data & 1;
		return;

		case 0xb004:
			flipscreen = data & 1;
		return;

		case 0xb800:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall bombjack_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
		case 0xb001:
		case 0xb002:
			return DrvInputs[address & 3];

		case 0xb003:
			return 0; // watchdog?

		case 0xb004:
		case 0xb005:
			return DrvDips[address & 1];
	}

	return 0;
}

static UINT8 __fastcall bombjack_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
		{
			UINT8 ret = soundlatch;
			soundlatch = 0;
			return ret;
		}
	}

	return 0;
}

static void __fastcall bombjack_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;

		case 0x10:
		case 0x11:
			AY8910Write(1, port & 1, data);
		return;

		case 0x80:
		case 0x81:
			AY8910Write(2, port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 code = DrvGfxROM3[offs];
	INT32 attr = DrvGfxROM3[offs + 0x100];

	TILE_SET_INFO(0, code, attr, (attr & 0x80) ? TILE_FLIPY : 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x10) << 4);

	TILE_SET_INFO(1, code, attr, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	nmi_mask = 0;
	flipscreen = 0;
	soundlatch = 0;
	background_image = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x010000;
	DrvGfxROM3		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0080 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000400;

	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { 0, 0x2000*8, 0x4000*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0200, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 load_type)
{
	BurnAllocMemIndex();

	if (load_type == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xc000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 15, 1)) return 1;
	}
	else if (load_type == 1)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xc000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 13, 1)) return 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0x9400, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9800, 0x98ff, MAP_RAM); // 20-7f
	ZetMapMemory(DrvPalRAM,			0x9c00, 0x9cff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0xc000,	0xc000, 0xdfff, MAP_ROM);
	ZetSetWriteHandler(bombjack_main_write);
	ZetSetReadHandler(bombjack_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x43ff, MAP_RAM);
	ZetSetReadHandler(bombjack_sound_read);
	ZetSetOutHandler(bombjack_sound_write_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910Init(2, 1500000, 1);
	AY8910SetAllRoutes(0, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 16, 4096);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 16, 16, 0x10000, 0, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM0, 3,  8,  8, 0x08000, 0, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 BombjackInit()
{
	return DrvInit(0);
}

static INT32 BombjacktInit()
{
	return DrvInit(1);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i+=2)
	{
		UINT8 r = DrvPalRAM[i+0] & 0xf;
		UINT8 g = DrvPalRAM[i+0] >> 4;
		UINT8 b = DrvPalRAM[i+1] & 0xf;

		DrvPalette[i / 2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0x20; offs -= 4)
	{
		INT32 sy;

		INT32 sx = DrvSprRAM[offs + 3];

		if (DrvSprRAM[offs] & 0x80)
			sy = 225 - DrvSprRAM[offs + 2];
		else
			sy = 241 - DrvSprRAM[offs + 2];

		INT32 flipx = DrvSprRAM[offs + 1] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 1] & 0x80;

		if (flipscreen)
		{
			if (DrvSprRAM[offs + 1] & 0x20)
			{
				sx = 224 - sx;
				sy = 224 - sy;
			}
			else
			{
				sx = 240 - sx;
				sy = 240 - sy;
			}
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;

		INT32 code  = DrvSprRAM[offs] & 0x7f;
		INT32 size  = DrvSprRAM[offs] >> 7;
		INT32 color = DrvSprRAM[offs + 1] & 0xf;

		if (size) {
			code = 0x80 | ((code * 4) & 0x7c);

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code + 3, sx +  0, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code + 2, sx + 16, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code + 1, sx +  0, sy + 16, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code + 0, sx + 16, sy + 16, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code + 2, sx +  0, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code + 3, sx + 16, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code + 0, sx +  0, sy + 16, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code + 1, sx + 16, sy + 16, color, 3, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code + 1, sx +  0, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code + 0, sx + 16, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code + 3, sx +  0, sy + 16, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code + 2, sx + 16, sy + 16, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code + 0, sx +  0, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_Clip(pTransDraw, code + 1, sx + 16, sy +  0, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_Clip(pTransDraw, code + 2, sx +  0, sy + 16, color, 3, 0, 0, DrvGfxROM2);
					Render16x16Tile_Mask_Clip(pTransDraw, code + 3, sx + 16, sy + 16, color, 3, 0, 0, DrvGfxROM2);
				}
			}
		} else {
			code &= 0x7f; // ??

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 3, 0, 0, DrvGfxROM2);
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	GenericTilemapSetScrollY(0, (background_image & 7) * 0x200);

	if ((background_image & 0x10) && (nBurnLayer & 1))
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (nmi_mask && i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetNmi();
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(background_image);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Bomb Jack

static struct BurnRomInfo bombjackRomDesc[] = {
	{ "09_j01b.bin",	0x2000, 0xc668dc30, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "10_l01b.bin",	0x2000, 0x52a1e5fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "11_m01b.bin",	0x2000, 0xb68a062a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "12_n01b.bin",	0x2000, 0x1d3ecee5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "13.1r",			0x2000, 0x70e0244d, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "01_h03t.3h",		0x2000, 0x8407917d, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "03_e08t.bin",	0x1000, 0x9f0470d5, 3 | BRF_GRA },           //  6 Characters
	{ "04_h08t.bin",	0x1000, 0x81ec12e6, 3 | BRF_GRA },           //  7
	{ "05_k08t.bin",	0x1000, 0xe87ec8b1, 3 | BRF_GRA },           //  8

	{ "06_l08t.bin",	0x2000, 0x51eebd89, 4 | BRF_GRA },           //  9 Tiles
	{ "07_n08t.bin",	0x2000, 0x9dd98e9d, 4 | BRF_GRA },           // 10
	{ "08_r08t.bin",	0x2000, 0x3155ee7d, 4 | BRF_GRA },           // 11

	{ "16_m07b.bin",	0x2000, 0x94694097, 5 | BRF_GRA },           // 12 Sprites
	{ "15_l07b.bin",	0x2000, 0x013f58f2, 5 | BRF_GRA },           // 13
	{ "14_j07b.bin",	0x2000, 0x101c858d, 5 | BRF_GRA },           // 14

	{ "02_p04t.bin",	0x1000, 0x398d4a02, 6 | BRF_GRA },           // 15 Tilemap data
};

STD_ROM_PICK(bombjack)
STD_ROM_FN(bombjack)

struct BurnDriver BurnDrvBombjack = {
	"bombjack", NULL, NULL, NULL, "1984",
	"Bomb Jack\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bombjackRomInfo, bombjackRomName, NULL, NULL, NULL, NULL, BombjackInputInfo, BombjackDIPInfo,
	BombjackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Bomb Jack (earlier)

static struct BurnRomInfo bombjack2RomDesc[] = {
	{ "09_j01b.bin",	0x2000, 0xc668dc30, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "10_l01b.bin",	0x2000, 0x52a1e5fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "11_m01b.bin",	0x2000, 0xb68a062a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "12_n01b.bin",	0x2000, 0x1d3ecee5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "13_r01b.bin",	0x2000, 0xbcafdd29, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "01_h03t.bin",	0x2000, 0x8407917d, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "03_e08t.bin",	0x1000, 0x9f0470d5, 3 | BRF_GRA },           //  6 Characters
	{ "04_h08t.bin",	0x1000, 0x81ec12e6, 3 | BRF_GRA },           //  7
	{ "05_k08t.bin",	0x1000, 0xe87ec8b1, 3 | BRF_GRA },           //  8

	{ "06_l08t.bin",	0x2000, 0x51eebd89, 4 | BRF_GRA },           //  9 Tiles
	{ "07_n08t.bin",	0x2000, 0x9dd98e9d, 4 | BRF_GRA },           // 10
	{ "08_r08t.bin",	0x2000, 0x3155ee7d, 4 | BRF_GRA },           // 11

	{ "16_m07b.bin",	0x2000, 0x94694097, 5 | BRF_GRA },           // 12 Sprites
	{ "15_l07b.bin",	0x2000, 0x013f58f2, 5 | BRF_GRA },           // 13
	{ "14_j07b.bin",	0x2000, 0x101c858d, 5 | BRF_GRA },           // 14

	{ "02_p04t.bin",	0x1000, 0x398d4a02, 6 | BRF_GRA },           // 15 Tilemap data
};

STD_ROM_PICK(bombjack2)
STD_ROM_FN(bombjack2)

struct BurnDriver BurnDrvBombjack2 = {
	"bombjack2", "bombjack", NULL, NULL, "1984",
	"Bomb Jack (earlier)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bombjack2RomInfo, bombjack2RomName, NULL, NULL, NULL, NULL, BombjackInputInfo, BombjackDIPInfo,
	BombjackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Bomb Jack (Tecfri, Spain)

static struct BurnRomInfo bombjacktRomDesc[] = {
	{ "9.1j",		0x4000, 0x4b59a3bb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "12.1n",		0x4000, 0x0a32506a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13.1r",		0x2000, 0x964ac5c5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.6h",		0x2000, 0x8407917d, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 code

	{ "3.1e",		0x2000, 0x54e1dac1, 3 | BRF_GRA },           //  4 Characters
	{ "4.1h",		0x2000, 0x05e428ab, 3 | BRF_GRA },           //  5
	{ "5.1k",		0x2000, 0xf282f29a, 3 | BRF_GRA },           //  6

	{ "6.1l",		0x2000, 0x51eebd89, 4 | BRF_GRA },           //  7 Tiles
	{ "7.1n",		0x2000, 0x9dd98e9d, 4 | BRF_GRA },           //  8
	{ "8.1r",		0x2000, 0x3155ee7d, 4 | BRF_GRA },           //  9

	{ "16.7m",		0x2000, 0x94694097, 5 | BRF_GRA },           // 10 Sprites
	{ "15.7k",		0x2000, 0x013f58f2, 5 | BRF_GRA },           // 11
	{ "14.7j",		0x2000, 0x101c858d, 5 | BRF_GRA },           // 12

	{ "2.5n",		0x2000, 0xde796158, 6 | BRF_GRA },           // 13 Tilemap data
};

STD_ROM_PICK(bombjackt)
STD_ROM_FN(bombjackt)

struct BurnDriver BurnDrvBombjackt = {
	"bombjackt", "bombjack", NULL, NULL, "1984",
	"Bomb Jack (Tecfri, Spain)\0", NULL, "Tehkan (Tecfri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bombjacktRomInfo, bombjacktRomName, NULL, NULL, NULL, NULL, BombjackInputInfo, BombjackDIPInfo,
	BombjacktInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Bomb Jack (bootleg)

static struct BurnRomInfo bombjackblRomDesc[] = {
	{ "09.bin",			0x2000, 0xc668dc30, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "10.bin",			0x2000, 0x52a1e5fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "11.bin",			0x2000, 0xb68a062a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "12.bin",			0x2000, 0x0f4d0726, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "13.bin",			0x2000, 0x9740f99b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "01.bin",			0x2000, 0x8407917d, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "03.bin",			0x1000, 0x9f0470d5, 3 | BRF_GRA },           //  6 Characters
	{ "04.bin",			0x1000, 0x81ec12e6, 3 | BRF_GRA },           //  7
	{ "05.bin",			0x1000, 0xe87ec8b1, 3 | BRF_GRA },           //  8

	{ "06.bin",			0x2000, 0x51eebd89, 4 | BRF_GRA },           //  9 Tiles
	{ "07.bin",			0x2000, 0x9dd98e9d, 4 | BRF_GRA },           // 10
	{ "08.bin",			0x2000, 0x3155ee7d, 4 | BRF_GRA },           // 11

	{ "16.bin",			0x2000, 0x94694097, 5 | BRF_GRA },           // 12 Sprites
	{ "15.bin",			0x2000, 0x013f58f2, 5 | BRF_GRA },           // 13
	{ "14.bin",			0x2000, 0x101c858d, 5 | BRF_GRA },           // 14

	{ "02.bin",			0x1000, 0x398d4a02, 6 | BRF_GRA },           // 15 Tilemap data
};

STD_ROM_PICK(bombjackbl)
STD_ROM_FN(bombjackbl)

struct BurnDriver BurnDrvBombjackbl = {
	"bombjackbl", "bombjack", NULL, NULL, "1985",
	"Bomb Jack (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bombjackblRomInfo, bombjackblRomName, NULL, NULL, NULL, NULL, BombjackInputInfo, BombjackDIPInfo,
	BombjackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};
