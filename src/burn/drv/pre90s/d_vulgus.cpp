// FB Alpha Vulgus drive module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *Mem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static INT16 *pAY8910Buffer[6];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT16 scroll[2];
static INT32 palette_bank;

static UINT8 DrvInputs[5];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1"       , BIT_DIGITAL  , DrvJoy1 + 7,	"p1 coin"  },
	{"Coin 2"       , BIT_DIGITAL  , DrvJoy1 + 6,	"p2 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 0,	"p1 start" },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy1 + 1,	"p2 start" },

	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 0, 	"p1 right" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 1, 	"p1 left"  },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy2 + 2, 	"p1 down"  },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy2 + 3, 	"p1 up"    },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 0, 	"p2 right" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 1, 	"p2 left"  },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy3 + 2, 	"p2 down"  },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy3 + 3, 	"p2 up"    },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 5,	"p2 fire 2"},

	
	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvInputs + 3, "dip 1"    },
	{"Dip 2"        , BIT_DIPSWITCH, DrvInputs + 4, "dip 2"    },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL                     },
	{0x12, 0xff, 0xff, 0x7f, NULL                     },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x11, 0x01, 0x03, 0x01, "1"                      },
	{0x11, 0x01, 0x03, 0x02, "2"                      },
	{0x11, 0x01, 0x03, 0x03, "3"                      },
	{0x11, 0x01, 0x03, 0x00, "5"                      },

	{0   , 0xfe, 0   , 7   , "Coin B"                 },
	{0x11, 0x01, 0x1c, 0x10, "5 Coins 1 Play"         },
	{0x11, 0x01, 0x1c, 0x08, "4 Coins 1 Play"         },
	{0x11, 0x01, 0x1c, 0x18, "3 Coins 1 Play"         },
	{0x11, 0x01, 0x1c, 0x04, "2 Coins 1 Play"         },
	{0x11, 0x01, 0x1c, 0x1c, "1 Coin  1 Play"         },
	{0x11, 0x01, 0x1c, 0x0c, "1 Coin  2 Plays"        },
	{0x11, 0x01, 0x1c, 0x14, "1 Coin  3 Plays"        },
//	{0x11, 0x01, 0x1c, 0x00, "Invalid"                },

	{0   , 0xfe, 0   , 8   , "Coin A"                 },
	{0x11, 0x01, 0xe0, 0x80, "5 Coins 1 Play"         },
	{0x11, 0x01, 0xe0, 0x40, "4 Coins 1 Play"         },
	{0x11, 0x01, 0xe0, 0xc0, "3 Coins 1 Play"         },
	{0x11, 0x01, 0xe0, 0x20, "2 Coins 1 Play"         },
	{0x11, 0x01, 0xe0, 0xe0, "1 Coin  1 Play"         },
	{0x11, 0x01, 0xe0, 0x60, "1 Coin  2 Plays"        },
	{0x11, 0x01, 0xe0, 0xa0, "1 Coin  3 Plays"        },
	{0x11, 0x01, 0xe0, 0x00, "Freeplay"               },

	{0   , 0xfe, 0   , 4   , "Difficulty?"            },
	{0x12, 0x01, 0x03, 0x02, "Easy?"                  },
	{0x12, 0x01, 0x03, 0x03, "Normal?"                },
	{0x12, 0x01, 0x03, 0x01, "Hard?"                  },
	{0x12, 0x01, 0x03, 0x00, "Hardest?"               },
	
	{0   , 0xfe, 0   , 2   , "Demo Music"             },
	{0x12, 0x01, 0x04, 0x00, "Off"                    },
	{0x12, 0x01, 0x04, 0x04, "On"                     },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x12, 0x01, 0x08, 0x00, "Off"                    },
	{0x12, 0x01, 0x08, 0x08, "On"                     },

	{0   , 0xfe, 0   , 8   , "Bonus Life"             },
	{0x12, 0x01, 0x70, 0x30, "10000 50000"            },
	{0x12, 0x01, 0x70, 0x50, "10000 60000"            },
	{0x12, 0x01, 0x70, 0x10, "10000 70000"            },
	{0x12, 0x01, 0x70, 0x70, "20000 60000"            },
	{0x12, 0x01, 0x70, 0x60, "20000 70000"            },
	{0x12, 0x01, 0x70, 0x20, "20000 80000"            },
	{0x12, 0x01, 0x70, 0x40, "30000 70000"            },
	{0x12, 0x01, 0x70, 0x00, "None"	                  },	

	{0   , 0xfe, 0   , 1   , "Cabinet"                },
	{0x12, 0x01, 0x80, 0x00, "Upright"                },
//	{0x12, 0x01, 0x80, 0x80, "Cocktail"               },
};

STDDIPINFO(Drv)

static void __fastcall vulgus_write_main(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		break;

		case 0xc802:
		case 0xc803:
			scroll[address & 1] = (scroll[address & 1] & 0x100) | data;
		break;

		case 0xc804:
			flipscreen = data & 0x80;
		break;

		case 0xc805:
			palette_bank = data & 3;
		break;

		case 0xc902:
		case 0xc903:
			scroll[address & 1] = (scroll[address & 1] & 0x0ff) | ((data & 1) << 8);
		break;
	}
}

static UINT8 __fastcall vulgus_read_main(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
		case 0xc003:
		case 0xc004:
			return DrvInputs[address - 0xc000];
	}

	return 0;
}

static void __fastcall vulgus_write_sound(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0xc000:
		case 0xc001:
			AY8910Write((address >> 14) & 1, address & 1, data);
		break;
	}
}

static UINT8 __fastcall vulgus_read_sound(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();
			
	ZetOpen(1);
	ZetReset();
	AY8910Reset(0);
	AY8910Reset(1);
	ZetClose();

	flipscreen = 0;
	soundlatch = 0;
	palette_bank = 0;

	scroll[0] = 0;
	scroll[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next		= Mem;

	DrvZ80ROM0		= Next; Next += 0x0a000;
	DrvZ80ROM1		= Next; Next += 0x02000;
	DrvGfxROM0		= Next; Next += 0x08000;
	DrvGfxROM1		= Next; Next += 0x20000;
	DrvGfxROM2		= Next; Next += 0x10000;
	DrvColPROM		= Next; Next += 0x00600;

	DrvPalette		= (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x01000;
	DrvZ80RAM1		= Next; Next += 0x00800;
	DrvSprRAM		= Next; Next += 0x00100;
	DrvFgRAM		= Next; Next += 0x00800;
	DrvBgRAM		= Next; Next += 0x00800;

	RamEnd			= Next;

	pAY8910Buffer[0] 	= (INT16*) Next; Next += nBurnSoundLen;
	pAY8910Buffer[1] 	= (INT16*) Next; Next += nBurnSoundLen;
	pAY8910Buffer[2] 	= (INT16*) Next; Next += nBurnSoundLen;
	pAY8910Buffer[3] 	= (INT16*) Next; Next += nBurnSoundLen;
	pAY8910Buffer[4] 	= (INT16*) Next; Next += nBurnSoundLen;
	pAY8910Buffer[5] 	= (INT16*) Next; Next += nBurnSoundLen;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	INT32 SpriPlanes[4] = { 0x20004, 0x20000, 0x00004, 0x00000 };
	INT32 SpriXOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(264,1) };
	INT32 SpriYOffs[16] = { STEP16(0,16) };

	INT32 TilePlanes[3] = { 0x00000, 0x20000, 0x40000 };
	INT32 TileXOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 TileYOffs[16] = { STEP16(0,8) };

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x200, 2,  8,  8, SpriPlanes + 2, SpriXOffs, SpriYOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x200, 3, 16, 16, TilePlanes + 0, TileXOffs, TileYOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x100, 4, 16, 16, SpriPlanes + 0, SpriXOffs, SpriYOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  4, 1)) return 1;
			
		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  5, 1)) return 1;
		
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x6000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x8000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0xa000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x6000, 16, 1)) return 1;
			
		if (BurnLoadRom(DrvColPROM + 0x0000, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0300, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0400, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0500, 22, 1)) return 1;

		if (DrvGfxDecode()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xcc00, 0xccff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(vulgus_write_main);
	ZetSetReadHandler(vulgus_read_main);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(vulgus_write_sound);
	ZetSetReadHandler(vulgus_read_sound);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	GenericTilesExit();

	BurnFree (Mem);

	return 0;
}

static INT32 DrvPaletteInit()
{
	UINT32 tmp[0x100];

	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0,bit1,bit2,bit3,r,g,b;

		bit0 = (DrvColPROM[  0 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[  0 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[  0 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[  0 + i] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[256 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[256 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[256 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[256 + i] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[512 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[512 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[512 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[512 + i] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		tmp[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++) {
		DrvPalette[i] = tmp[32 + DrvColPROM[0x300 + i]];
	}

	for (INT32 i = 0; i < 0x100; i++) {
		DrvPalette[0x100 + i] = tmp[16 + DrvColPROM[0x400 + i]];
	}

	for (INT32 i = 0; i < 0x100; i++)	{
		DrvPalette[0x400 + i] = tmp[DrvColPROM[0x500 + i] + 0x00];
		DrvPalette[0x500 + i] = tmp[DrvColPROM[0x500 + i] + 0x40];
		DrvPalette[0x600 + i] = tmp[DrvColPROM[0x500 + i] + 0x80];
		DrvPalette[0x700 + i] = tmp[DrvColPROM[0x500 + i] + 0xc0];
	}

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = ((offs / 32) * 16) - scroll[1];
		INT32 sy = ((offs & 31) * 16) - scroll[0];
		
		if (sx < -15) sx += 512;
		if (sy < -15) sy += 512;

		INT32 color = DrvBgRAM[0x400 + offs];
		INT32 code  = DrvBgRAM[0x000 + offs] | ((color & 0x80) << 1);

		INT32 flipx = color & 0x20;
		INT32 flipy = color & 0x40;

		color = (color & 0x1f) | (palette_bank << 5);

		if (flipy) {
			if (flipx) {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0x400, DrvGfxROM1);
			} else {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0x400, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0x400, DrvGfxROM1);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy - 16, color, 3, 0x400, DrvGfxROM1);
			}
		}
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 2 * 32; offs < (32 * 32) - (2 * 32); offs++)
	{
		INT32 sx = (offs & 31) * 8;
		INT32 sy = ((offs / 32) * 8) - 16;

		INT32 color = DrvFgRAM[0x400 + offs];
		INT32 code  = DrvFgRAM[0x000 + offs] | ((color & 0x80) << 1);

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, (color&0x3f)<<2, 0xf, sx, sy, 0, 0, 8, 8, DrvColPROM + 0x300);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0; offs -= 4)
	{
		INT32 code  = DrvSprRAM[0x00 + offs];
		INT32 color = DrvSprRAM[0x01 + offs] & 0x0f;
		INT32 sx    = DrvSprRAM[0x03 + offs];
		INT32 sy    = DrvSprRAM[0x02 + offs];

		INT32 i = (DrvSprRAM[0x01 + offs] >> 6) & 3;
		if (i == 2) i = 3;

		for (; i >= 0; i--) {
			INT32 ssy = (sy + (i * 16)) - 16;
			Render16x16Tile_Mask_Clip(pTransDraw, code + i, sx, ssy, color, 4, 0x0f, 0x100, DrvGfxROM2);
			if (ssy > 240) { // wrap
				Render16x16Tile_Mask_Clip(pTransDraw, code + i, sx, ssy - 256, color, 4, 0x0f, 0x100, DrvGfxROM2);
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

	draw_bg_layer();
	draw_sprites();
	draw_fg_layer();

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
	
	INT32 nInterleave = 8;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == ((nInterleave / 2) - 1)) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == ( nInterleave      - 1)) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] -= ZetRun(nCyclesTotal[1] / nInterleave);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

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
		ba.szName = "All RAM";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(palette_bank);
		SCAN_VAR(scroll[0]);
		SCAN_VAR(scroll[1]);
	}

	return 0;
}


// Vulgus (set 1)

static struct BurnRomInfo vulgusRomDesc[] = {
	{ "vulgus.002",   0x2000, 0xe49d6c5d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "vulgus.003",   0x2000, 0x51acef76, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vulgus.004",   0x2000, 0x489e7f60, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vulgus.005",   0x2000, 0xde3a24a8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1-8n.bin",     0x2000, 0x6ca5ca41, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "1-11c.bin",    0x2000, 0x3bd2acf4, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "1-3d.bin",     0x2000, 0x8bc5d7a5, 3 | BRF_GRA },	       //  6 Foreground Tiles

	{ "2-2a.bin",     0x2000, 0xe10aaca1, 4 | BRF_GRA },	       //  7 Background Tiles
	{ "2-3a.bin",     0x2000, 0x8da520da, 4 | BRF_GRA },	       //  8
	{ "2-4a.bin",     0x2000, 0x206a13f1, 4 | BRF_GRA },	       //  9
	{ "2-5a.bin",     0x2000, 0xb6d81984, 4 | BRF_GRA },	       // 10
	{ "2-6a.bin",     0x2000, 0x5a26b38f, 4 | BRF_GRA },	       // 11 
	{ "2-7a.bin",     0x2000, 0x1e1ca773, 4 | BRF_GRA },	       // 12 

	{ "2-2n.bin",     0x2000, 0x6db1b10d, 5 | BRF_GRA },	       // 13 Sprites
	{ "2-3n.bin",     0x2000, 0x5d8c34ec, 5 | BRF_GRA },	       // 14
	{ "2-4n.bin",     0x2000, 0x0071a2e3, 5 | BRF_GRA },	       // 15
	{ "2-5n.bin",     0x2000, 0x4023a1ec, 5 | BRF_GRA },	       // 16

	{ "e8.bin",       0x0100, 0x06a83606, 6 | BRF_GRA },	       // 17 Color DrvColPROMs
	{ "e9.bin",       0x0100, 0xbeacf13c, 6 | BRF_GRA },	       // 18
	{ "e10.bin",      0x0100, 0xde1fb621, 6 | BRF_GRA },	       // 19
	{ "d1.bin",       0x0100, 0x7179080d, 6 | BRF_GRA },	       // 20
	{ "j2.bin",       0x0100, 0xd0842029, 6 | BRF_GRA },	       // 21
	{ "c9.bin",       0x0100, 0x7a1f0bd6, 6 | BRF_GRA },	       // 22

	{ "82s126.9k",    0x0100, 0x32b10521, 0 | BRF_OPT },	       // 23 Misc. DrvColPROMs
	{ "82s129.8n",    0x0100, 0x4921635c, 0 | BRF_OPT },	       // 24
};

STD_ROM_PICK(vulgus)
STD_ROM_FN(vulgus)

struct BurnDriver BurnDrvvulgus = {
	"vulgus", NULL, NULL, NULL, "1984",
	"Vulgus (set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, vulgusRomInfo, vulgusRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Vulgus (set 2)

static struct BurnRomInfo vulgusaRomDesc[] = {
	{ "v2",           0x2000, 0x3e18ff62, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "v3",           0x2000, 0xb4650d82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v4",           0x2000, 0x5b26355c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v5",           0x2000, 0x4ca7f10e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1-8n.bin",     0x2000, 0x6ca5ca41, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "1-11c.bin",    0x2000, 0x3bd2acf4, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "1-3d.bin",     0x2000, 0x8bc5d7a5, 3 | BRF_GRA },	       //  6 Foreground Tiles

	{ "2-2a.bin",     0x2000, 0xe10aaca1, 4 | BRF_GRA },	       //  7 Background Tiles
	{ "2-3a.bin",     0x2000, 0x8da520da, 4 | BRF_GRA },	       //  8
	{ "2-4a.bin",     0x2000, 0x206a13f1, 4 | BRF_GRA },	       //  9
	{ "2-5a.bin",     0x2000, 0xb6d81984, 4 | BRF_GRA },	       // 10
	{ "2-6a.bin",     0x2000, 0x5a26b38f, 4 | BRF_GRA },	       // 11 
	{ "2-7a.bin",     0x2000, 0x1e1ca773, 4 | BRF_GRA },	       // 12 

	{ "2-2n.bin",     0x2000, 0x6db1b10d, 5 | BRF_GRA },	       // 13 Sprites
	{ "2-3n.bin",     0x2000, 0x5d8c34ec, 5 | BRF_GRA },	       // 14
	{ "2-4n.bin",     0x2000, 0x0071a2e3, 5 | BRF_GRA },	       // 15
	{ "2-5n.bin",     0x2000, 0x4023a1ec, 5 | BRF_GRA },	       // 16

	{ "e8.bin",       0x0100, 0x06a83606, 6 | BRF_GRA },	       // 17 Color DrvColPROMs
	{ "e9.bin",       0x0100, 0xbeacf13c, 6 | BRF_GRA },	       // 18
	{ "e10.bin",      0x0100, 0xde1fb621, 6 | BRF_GRA },	       // 19
	{ "d1.bin",       0x0100, 0x7179080d, 6 | BRF_GRA },	       // 20
	{ "j2.bin",       0x0100, 0xd0842029, 6 | BRF_GRA },	       // 21
	{ "c9.bin",       0x0100, 0x7a1f0bd6, 6 | BRF_GRA },	       // 22

	{ "82s126.9k",    0x0100, 0x32b10521, 0 | BRF_OPT },	       // 23 Misc. DrvColPROMs
	{ "82s129.8n",    0x0100, 0x4921635c, 0 | BRF_OPT },	       // 24
};

STD_ROM_PICK(vulgusa)
STD_ROM_FN(vulgusa)

struct BurnDriver BurnDrvvulgusa = {
	"vulgusa", "vulgus", NULL, NULL, "1984",
	"Vulgus (set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, vulgusaRomInfo, vulgusaRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Vulgus (Japan)

static struct BurnRomInfo vulgusjRomDesc[] = {
	{ "1-4n.bin",     0x2000, 0xfe5a5ca5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1-5n.bin",     0x2000, 0x847e437f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1-6n.bin",     0x2000, 0x4666c436, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1-7n.bin",     0x2000, 0xff2097f9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1-8n.bin",     0x2000, 0x6ca5ca41, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "1-11c.bin",    0x2000, 0x3bd2acf4, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "1-3d.bin",     0x2000, 0x8bc5d7a5, 3 | BRF_GRA },	       //  6 Foreground Tiles

	{ "2-2a.bin",     0x2000, 0xe10aaca1, 4 | BRF_GRA },	       //  7 Background Tiles
	{ "2-3a.bin",     0x2000, 0x8da520da, 4 | BRF_GRA },	       //  8
	{ "2-4a.bin",     0x2000, 0x206a13f1, 4 | BRF_GRA },	       //  9
	{ "2-5a.bin",     0x2000, 0xb6d81984, 4 | BRF_GRA },	       // 10
	{ "2-6a.bin",     0x2000, 0x5a26b38f, 4 | BRF_GRA },	       // 11 
	{ "2-7a.bin",     0x2000, 0x1e1ca773, 4 | BRF_GRA },	       // 12 

	{ "2-2n.bin",     0x2000, 0x6db1b10d, 5 | BRF_GRA },	       // 13 Sprites
	{ "2-3n.bin",     0x2000, 0x5d8c34ec, 5 | BRF_GRA },	       // 14
	{ "2-4n.bin",     0x2000, 0x0071a2e3, 5 | BRF_GRA },	       // 15
	{ "2-5n.bin",     0x2000, 0x4023a1ec, 5 | BRF_GRA },	       // 16

	{ "e8.bin",       0x0100, 0x06a83606, 6 | BRF_GRA },	       // 17 Color DrvColPROMs
	{ "e9.bin",       0x0100, 0xbeacf13c, 6 | BRF_GRA },	       // 18
	{ "e10.bin",      0x0100, 0xde1fb621, 6 | BRF_GRA },	       // 19
	{ "d1.bin",       0x0100, 0x7179080d, 6 | BRF_GRA },	       // 20
	{ "j2.bin",       0x0100, 0xd0842029, 6 | BRF_GRA },	       // 21
	{ "c9.bin",       0x0100, 0x7a1f0bd6, 6 | BRF_GRA },	       // 22

	{ "82s126.9k",    0x0100, 0x32b10521, 0 | BRF_OPT },	       // 23 Misc. DrvColPROMs
	{ "82s129.8n",    0x0100, 0x4921635c, 0 | BRF_OPT },	       // 24
};

STD_ROM_PICK(vulgusj)
STD_ROM_FN(vulgusj)

struct BurnDriver BurnDrvvulgusj = {
	"vulgusj", "vulgus", NULL, NULL, "1984",
	"Vulgus (Japan?)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, vulgusjRomInfo, vulgusjRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};
