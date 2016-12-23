// FB Alpha Exed Exes driver module
// Based on MAME driver by Richard Davies, Paul Swan, and various others

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

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
static UINT8 *DrvGfxROM4;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvTransTable;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 soundlatch;
static UINT8 txt_enable;
static UINT8 bg_enable;
static UINT8 fg_enable;
static UINT8 spr_enable;
static UINT16 fg_scrolly;
static UINT16 fg_scrollx;
static UINT16 bg_scrollx;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1"       , BIT_DIGITAL  , DrvJoy1 + 6, "p1 coin"  },
	{"Coin 2"       , BIT_DIGITAL  , DrvJoy1 + 7, "p2 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 0, "p1 start" },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy1 + 1, "p2 start" },

	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 0, "p1 right" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 1, "p1 left"  },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy2 + 2, "p1 down"  },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy2 + 3, "p1 up"    },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4, "p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 5, "p1 fire 2"},

	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 0, "p2 right" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 1, "p2 left"  },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy3 + 2, "p2 down"  },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy3 + 3, "p2 up"    },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 4, "p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 5, "p2 fire 2"},

	{"Service"      , BIT_DIGITAL  , DrvJoy1 + 5, "service"  },

	{"Reset"        , BIT_DIGITAL  , &DrvReset  , "reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvDips + 0, "dip 1"    },
	{"Dip 2"        , BIT_DIPSWITCH, DrvDips + 1, "dip 2"    },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xdf, NULL                     },
	{0x13, 0xff, 0xff, 0xff, NULL                     },

	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x12, 0x01, 0x03, 0x02, "Easy"                   },
	{0x12, 0x01, 0x03, 0x03, "Normal"                 },
	{0x12, 0x01, 0x03, 0x01, "Hard"                   },
	{0x12, 0x01, 0x03, 0x00, "Hardest"                },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x12, 0x01, 0x0c, 0x08, "1"                      },
	{0x12, 0x01, 0x0c, 0x04, "2"                      },
	{0x12, 0x01, 0x0c, 0x0c, "3"                      },
	{0x12, 0x01, 0x0c, 0x00, "5"                      },

	{0   , 0xfe, 0   , 2   , "2 Players Game"         },
	{0x12, 0x01, 0x10, 0x00, "1 Credit"               },
	{0x12, 0x01, 0x10, 0x10, "2 Credits"              },

	{0   , 0xfe, 0   , 2   , "Language"               },
	{0x12, 0x01, 0x20, 0x00, "English"                },
	{0x12, 0x01, 0x20, 0x20, "Japanese"               },

	{0   , 0xfe, 0   , 2   , "Freeze"                 },
	{0x12, 0x01, 0x40, 0x40, "Off"                    },
	{0x12, 0x01, 0x40, 0x00, "On"                     },

	{0   , 0xfe, 0   , 2   , "Service Mode"           },
	{0x12, 0x01, 0x80, 0x80, "Off"                    },
	{0x12, 0x01, 0x80, 0x00, "On"                     },

	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Play"         },
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Play"         },
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Play"         },
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Play"         },
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Plays"        },
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Plays"        },
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Plays"        },
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Plays"        },

	{0   , 0xfe, 0   , 8   , "Coin B"                 },
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Play"         },
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Play"         },
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Play"         },
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Play"         },
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Plays"        },
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Plays"        },
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Plays"        },
	{0x13, 0x01, 0x38, 0x18, "1 Coin  5 Plays"        },

	{0   , 0xfe, 0   , 2   , "Allow Continue"         },
	{0x13, 0x01, 0x40, 0x00, "No"                     },
	{0x13, 0x01, 0x40, 0x40, "Yes"                    },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x13, 0x01, 0x80, 0x00, "Off"                    },
	{0x13, 0x01, 0x80, 0x80, "On"                     },
};

STDDIPINFO(Drv)

static UINT8 __fastcall exedexes_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
			return DrvInputs[address & 3];	

		case 0xc003:
		case 0xc004:
			return DrvDips[~address & 1];
	}

	return 0;
}

static void __fastcall exedexes_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		break;

		case 0xc804:
			txt_enable = data & 0x80;
		break;

		case 0xc806:
		break;

		case 0xd800:
			fg_scrolly = (fg_scrolly & 0xff00) | (data << 0);
		break;

		case 0xd801:
			fg_scrolly = (fg_scrolly & 0x00ff) | (data << 8);
		break;

		case 0xd802:
			fg_scrollx = (fg_scrollx & 0xff00) | (data << 0);
		break;

		case 0xd803:
			fg_scrollx = (fg_scrollx & 0x00ff) | (data << 8);
		break;

		case 0xd804:
			bg_scrollx = (bg_scrollx & 0xff00) | (data << 0);
		break;

		case 0xd805:
			bg_scrollx = (bg_scrollx & 0x00ff) | (data << 8);
		break;

		case 0xd807:
			bg_enable  = data & 0x10;
			fg_enable  = data & 0x20;
			spr_enable = data & 0x40;
		break;
	}
}

static UINT8 __fastcall exedexes_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall exedexes_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
			AY8910Write(0, address & 1, data);
		break;

		case 0x8002:
		case 0x8003:
			SN76496Write(address & 1, data);
		break;
	}
}

static tilemap_callback( background )
{
	INT32 attr = DrvGfxROM4[offs];

	TILE_SET_INFO(1, attr & 0x3f, DrvGfxROM4[offs + 0x40], TILE_FLIPYX(attr >> 6));
}

static tilemap_callback( foreground )
{
	TILE_SET_INFO(2, DrvGfxROM4[offs], 0, 0);
}

static tilemap_callback( text )
{
	INT32 attr = DrvColRAM[offs];

	TILE_SET_INFO(0, DrvVidRAM[offs] + ((attr & 0x80) << 1), attr & 0x3f, 0);

	// hacky
	attr = (attr & 0x3f) << 2;
	GenericTilemapSetTransTable(2, 0, DrvTransTable[attr+0]);
	GenericTilemapSetTransTable(2, 1, DrvTransTable[attr+1]);
	GenericTilemapSetTransTable(2, 2, DrvTransTable[attr+2]);
	GenericTilemapSetTransTable(2, 3, DrvTransTable[attr+3]);
}

static tilemap_scan( background )
{
	return ((col * 32 & 0xe0) >> 5) + ((row * 32 & 0xe0) >> 2) + ((col * 32 & 0x3f00) >> 1) + 0x4000;
}

static tilemap_scan( foreground )
{
	return ((col * 16 & 0xf0) >> 4) + (row * 16 & 0xf0) + (col * 16 & 0x700) + ((row * 16 & 0x700) << 3);
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

	soundlatch = 0;
	txt_enable = 0;
	spr_enable = 0;
	bg_enable = 0;
	fg_enable = 0;
	fg_scrollx = 0;
	fg_scrolly = 0;
	bg_scrollx = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c000;
	DrvZ80ROM1		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x020000;
	DrvGfxROM3		= Next; Next += 0x010000;
	DrvGfxROM4		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000800;

	DrvTransTable		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvSprBuf		= Next; Next += 0x001000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 TilePlanes[2] = { 0x004, 0x000 };
	static INT32 SpriPlanes[4] = { 0x20004, 0x20000, 0x00004, 0x00000 };
	static INT32 TileXOffs[32] = { STEP4(0,1), STEP4(8,1), STEP4(512,1), STEP4(520,1),
					STEP4(1024, 1), STEP4(1032, 1), STEP4(1536,1), STEP4(1544,1) };
	static INT32 SpriXOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	static INT32 TileYOffs[32] = { STEP32(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x02000);

	GfxDecode(0x200, 2,  8,  8, TilePlanes, TileXOffs, TileYOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x04000);

	GfxDecode(0x040, 2, 32, 32, TilePlanes, TileXOffs, TileYOffs, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x08000);

	GfxDecode(0x100, 4, 16, 16, SpriPlanes, SpriXOffs, TileYOffs, 0x200, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x08000);

	GfxDecode(0x100, 4, 16, 16, SpriPlanes, SpriXOffs, TileYOffs, 0x200, tmp, DrvGfxROM3);

	BurnFree (tmp);
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x04000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x04000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00500, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00600, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00700, 19, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(exedexes_main_write);
	ZetSetReadHandler(exedexes_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(exedexes_sound_write);
	ZetSetReadHandler(exedexes_sound_read);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);

	SN76489Init(0, 3000000, 0);
	SN76489Init(1, 3000000, 1);
	SN76496SetRoute(0, 0.36, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.36, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, background_map_scan, background_map_callback, 32, 32, 64, 64);
	GenericTilemapInit(1, foreground_map_scan, foreground_map_callback, 16, 16, 128, 128);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS,   text_map_callback,        8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x08000, 0x000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 32, 32, 0x10000, 0x100, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, 0x20000, 0x200, 0x0f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);
//	GenericTilemapSetTransparent(2, 0); // wrong

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	AY8910Exit(0);
	SN76496Exit();

	ZetExit();

	GenericTilesExit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvPaletteInit()
{
	UINT32 tmp[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = DrvColPROM[i + 0x000] & 0xf;
		UINT8 g = DrvColPROM[i + 0x100] & 0xf;
		UINT8 b = DrvColPROM[i + 0x200] & 0xf;

		tmp[i] = BurnHighCol((r*16)+r,(g*16)+g,(b*16)+b, 0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i + 0x000] = tmp[DrvColPROM[i + 0x300] | 0xc0];
		DrvPalette[i + 0x100] = tmp[DrvColPROM[i + 0x400]];
		DrvPalette[i + 0x200] = tmp[DrvColPROM[i + 0x500] | 0x40];
		DrvPalette[i + 0x300] = tmp[DrvColPROM[i + 0x600] | (DrvColPROM[i + 0x700] << 4) | 0x80];

		DrvTransTable[i] = (DrvColPROM[0x300 + i] == 0xf) ? 1 : 0;
	}

	return 0;
}

static void draw_sprites(INT32 priority)
{
	priority = priority ? 0x40 : 0x00;

	for (INT32 offs = 0x1000 - 32; offs >= 0; offs -= 32)
	{
		if ((DrvSprBuf[offs + 1] & 0x40) == priority)
		{
			INT32 code = DrvSprBuf[offs];
			INT32 color = DrvSprBuf[offs + 1] & 0x0f;
			INT32 flipx = DrvSprBuf[offs + 1] & 0x10;
			INT32 flipy = DrvSprBuf[offs + 1] & 0x20;
			INT32 sx = DrvSprBuf[offs + 3] - ((DrvSprBuf[offs + 1] & 0x80) << 1);
			INT32 sy = DrvSprBuf[offs + 2] - 16;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x300, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x300, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x300, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x300, DrvGfxROM3);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (bg_enable) {
		GenericTilemapSetScrollX(0, bg_scrollx);

		GenericTilemapDraw(0, pTransDraw, 0);
	} else {
		BurnTransferClear();
	}

	if (spr_enable) {
		draw_sprites(1);
	}

	if (fg_enable) {
		GenericTilemapSetScrollX(1, fg_scrolly); // Swapped!
		GenericTilemapSetScrollY(1, fg_scrollx);

		GenericTilemapDraw(1, pTransDraw, 0);
	}

	if (spr_enable) {
		draw_sprites(0);
	}

	if (txt_enable) {
		GenericTilemapDraw(2, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 0) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == (nInterleave - 2)) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if ((i & 3) == 3) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);

		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 1);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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
		AY8910Scan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(txt_enable);
		SCAN_VAR(spr_enable);
		SCAN_VAR(bg_enable);
		SCAN_VAR(fg_enable);
		SCAN_VAR(fg_scrolly);
		SCAN_VAR(fg_scrollx);
		SCAN_VAR(bg_scrollx);
	}

	return 0;
}


// Exed Exes

static struct BurnRomInfo exedexesRomDesc[] = {
	{ "11m_ee04.bin", 0x4000, 0x44140dbd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "10m_ee03.bin", 0x4000, 0xbf72cfba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "09m_ee02.bin", 0x4000, 0x7ad95e2f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "11e_ee01.bin", 0x4000, 0x73cdf3b2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "05c_ee00.bin", 0x2000, 0xcadb75bd, 3 | BRF_GRA },	       //  4 Characters

	{ "h01_ee08.bin", 0x4000, 0x96a65c1d, 4 | BRF_GRA },	       //  5 32x32 tiles

	{ "a03_ee06.bin", 0x4000, 0x6039bdd1, 5 | BRF_GRA },	       //  6 16x16 tiles
	{ "a02_ee05.bin", 0x4000, 0xb32d8252, 5 | BRF_GRA },	       //  7

	{ "j11_ee10.bin", 0x4000, 0xbc83e265, 6 | BRF_GRA },	       //  8 Sprites
	{ "j12_ee11.bin", 0x4000, 0x0e0f300d, 6 | BRF_GRA },	       //  9

	{ "c01_ee07.bin", 0x4000, 0x3625a68d, 7 | BRF_GRA },	       // 10 Tile Maps
	{ "h04_ee09.bin", 0x2000, 0x6057c907, 7 | BRF_GRA },	       // 11

	{ "02d_e-02.bin", 0x0100, 0x8d0d5935, 8 | BRF_GRA },	       // 12 Color Proms
	{ "03d_e-03.bin", 0x0100, 0xd3c17efc, 8 | BRF_GRA },	       // 13
	{ "04d_e-04.bin", 0x0100, 0x58ba964c, 8 | BRF_GRA },	       // 14
	{ "06f_e-05.bin", 0x0100, 0x35a03579, 8 | BRF_GRA },	       // 15
	{ "l04_e-10.bin", 0x0100, 0x1dfad87a, 8 | BRF_GRA },	       // 16
	{ "c04_e-07.bin", 0x0100, 0x850064e0, 8 | BRF_GRA },	       // 17
	{ "l09_e-11.bin", 0x0100, 0x2bb68710, 8 | BRF_GRA },	       // 18
	{ "l10_e-12.bin", 0x0100, 0x173184ef, 8 | BRF_GRA },	       // 19

	{ "06l_e-06.bin", 0x0100, 0x712ac508, 0 | BRF_OPT },	       // 20 Misc. Proms
	{ "k06_e-08.bin", 0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 21
	{ "l03_e-09.bin", 0x0100, 0x0d968558, 0 | BRF_OPT },	       // 22
	{ "03e_e-01.bin", 0x0020, 0x1acee376, 0 | BRF_OPT },	       // 23
};

STD_ROM_PICK(exedexes)
STD_ROM_FN(exedexes)

struct BurnDriver BurnDrvExedexes = {
	"exedexes", NULL, NULL, NULL, "1985",
	"Exed Exes\0", NULL, "Capcom", "Miscellaneous",
	L"Exed Exes\0\u30A8\u30B0\u30BC\u30C9 \u30A8\u30B0\u30BC\u30B9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, exedexesRomInfo, exedexesRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// Savage Bees

static struct BurnRomInfo savgbeesRomDesc[] = {
	{ "ee04e.11m",    0x4000, 0xc0caf442, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ee03e.10m",    0x4000, 0x9cd70ae1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ee02e.9m",     0x4000, 0xa04e6368, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ee01e.11e",    0x4000, 0x93d3f952, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "ee00e.5c",     0x2000, 0x5972f95f, 3 | BRF_GRA },	       //  4 Characters

	{ "h01_ee08.bin", 0x4000, 0x96a65c1d, 4 | BRF_GRA },	       //  5 32x32 tiles

	{ "a03_ee06.bin", 0x4000, 0x6039bdd1, 5 | BRF_GRA },	       //  6 16x16 tiles
	{ "a02_ee05.bin", 0x4000, 0xb32d8252, 5 | BRF_GRA },	       //  7

	{ "j11_ee10.bin", 0x4000, 0xbc83e265, 6 | BRF_GRA },	       //  8 Sprites
	{ "j12_ee11.bin", 0x4000, 0x0e0f300d, 6 | BRF_GRA },	       //  9

	{ "c01_ee07.bin", 0x4000, 0x3625a68d, 7 | BRF_GRA },	       // 10 Tile Maps
	{ "h04_ee09.bin", 0x2000, 0x6057c907, 7 | BRF_GRA },	       // 11

	{ "02d_e-02.bin", 0x0100, 0x8d0d5935, 8 | BRF_GRA },	       // 12 Color Proms
	{ "03d_e-03.bin", 0x0100, 0xd3c17efc, 8 | BRF_GRA },	       // 13
	{ "04d_e-04.bin", 0x0100, 0x58ba964c, 8 | BRF_GRA },	       // 14
	{ "06f_e-05.bin", 0x0100, 0x35a03579, 8 | BRF_GRA },	       // 15
	{ "l04_e-10.bin", 0x0100, 0x1dfad87a, 8 | BRF_GRA },	       // 16
	{ "c04_e-07.bin", 0x0100, 0x850064e0, 8 | BRF_GRA },	       // 17
	{ "l09_e-11.bin", 0x0100, 0x2bb68710, 8 | BRF_GRA },	       // 18
	{ "l10_e-12.bin", 0x0100, 0x173184ef, 8 | BRF_GRA },	       // 19

	{ "06l_e-06.bin", 0x0100, 0x712ac508, 0 | BRF_OPT },	       // 20 Misc. Proms
	{ "k06_e-08.bin", 0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 21
	{ "l03_e-09.bin", 0x0100, 0x0d968558, 0 | BRF_OPT },	       // 22
	{ "03e_e-01.bin", 0x0020, 0x1acee376, 0 | BRF_OPT },	       // 23
};

STD_ROM_PICK(savgbees)
STD_ROM_FN(savgbees)

struct BurnDriver BurnDrvSavgbees = {
	"savgbees", "exedexes", NULL, NULL, "1985",
	"Savage Bees\0", NULL, "Capcom (Memetron license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, savgbeesRomInfo, savgbeesRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};
