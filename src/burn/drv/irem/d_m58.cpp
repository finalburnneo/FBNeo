// FB Alpha Irem M58 driver module
// Based on MAME driver by Lee Taylor

#include "tiles_generic.h"
#include "z80_intf.h"
#include "irem_sound.h"
#include "bitswap.h" // BIT

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6803ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvScrollPanel;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 scrollx;
static UINT8 scrolly;
static UINT8 score_disable;
static UINT8 flipscreen;

static INT32 input_invertbit = 0;
static INT32 sprite_pal_xor = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo YardInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Yard)

static struct BurnDIPInfo YardDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xfd, NULL						},

	{0   , 0xfe, 0   ,    4, "Time Reduced by Ball Dead"},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x12, 0x01, 0x0c, 0x08, "x1.3"						},
	{0x12, 0x01, 0x0c, 0x04, "x1.5"						},
	{0x12, 0x01, 0x0c, 0x00, "x1.8"						},

	{0   , 0xfe, 0   ,   15, "Coinage"					},
	{0x12, 0x01, 0xf0, 0x90, "7 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    1, "Cabinet"					},
	{0x13, 0x01, 0x02, 0x00, "Upright"					},
//	{0x13, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    1, "Coin Mode"				},
	{0x13, 0x01, 0x04, 0x04, "Mode 1"					},
//	{0x13, 0x01, 0x04, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    2, "Slow Motion (Cheat)"		},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x13, 0x01, 0x10, 0x10, "Off"						},
	{0x13, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"		},
	{0x13, 0x01, 0x20, 0x20, "Off"						},
	{0x13, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x13, 0x01, 0x40, 0x40, "Off"						},
	{0x13, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x80, 0x00, "On"						},
	{0x13, 0x01, 0x80, 0x80, "Off"						},
};

STDDIPINFO(Yard)

static struct BurnDIPInfo Vs10yarjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xfd, NULL						},

	{0   , 0xfe, 0   ,    2, "Allow Continue (Vs. Mode)"},
	{0x12, 0x01, 0x01, 0x01, "No"						},
	{0x12, 0x01, 0x01, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Defensive Man Pause"		},
	{0x12, 0x01, 0x02, 0x02, "Off"						},
	{0x12, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Time Reduced by Ball Dead"},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x12, 0x01, 0x0c, 0x08, "x1.3"						},
	{0x12, 0x01, 0x0c, 0x04, "x1.5"						},
	{0x12, 0x01, 0x0c, 0x00, "x1.8"						},

	{0   , 0xfe, 0   ,   15, "Coinage"					},
	{0x12, 0x01, 0xf0, 0x90, "7 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    1, "Cabinet"					},
	{0x13, 0x01, 0x02, 0x00, "Upright"					},
//	{0x13, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    1, "Coin Mode"				},
	{0x13, 0x01, 0x04, 0x04, "Mode 1"					},
//	{0x13, 0x01, 0x04, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    2, "Slow Motion (Cheat)"		},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x13, 0x01, 0x10, 0x10, "Off"						},
	{0x13, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"		},
	{0x13, 0x01, 0x20, 0x20, "Off"						},
	{0x13, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x13, 0x01, 0x40, 0x40, "Off"						},
	{0x13, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x80, 0x00, "On"						},
	{0x13, 0x01, 0x80, 0x80, "Off"						},
};

STDDIPINFO(Vs10yarj)

static void __fastcall m58_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x9000) {
		DrvScrollPanel[address & 0xfff] = data;
		return;
	}

	switch (address)
	{
		case 0xa000:
			scrollx = (scrollx & 0xff00) | (data << 0);
		return;

		case 0xa200:
			scrollx = (scrollx & 0x00ff) | (data << 8);
		return;

		case 0xa400:
			scrolly = data;
		return;

		case 0xa800:
			score_disable = data;
		return;

		case 0xd000:
			IremSoundWrite(data);
		return;

		case 0xd001:
			flipscreen = (data & 1) ^ (~DrvDips[1] & 1);
		return;
	}
}

static UINT8 __fastcall m58_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
			return DrvInputs[address & 3];

		case 0xd003:
		case 0xd004:
			return DrvDips[address - 0xd003];
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] + ((attr & 0xc0) << 2);

	TILE_SET_INFO(0, code, attr, (attr & 0x20) ? TILE_FLIPX : 0);
}

static tilemap_scan( bg )
{
	if (col >= 32)
		return (row + 32) * 32 + col - 32;
	else
		return row * 32 + col;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	IremSoundReset();

	score_disable = 0;
	scrollx = 0;
	scrolly = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x006000;
	
	DrvM6803ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000520;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;
	
	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvScrollPanel	= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { 0x2000*8*2, 0x2000*8*1, 0x2000*8*0 };
	INT32 Plane1[3] = { 0x4000*8*2, 0x4000*8*1, 0x4000*8*0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvZ80ROM   + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x4000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6803ROM + 0x8000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6803ROM + 0xa000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6803ROM + 0xc000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6803ROM + 0xe000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x4000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x6000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x8000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0xa000, 15, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0200, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0400, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0420, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0100, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0300, 21, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xc800, 0xc8ff, MAP_RAM); // 20-7f
	ZetMapMemory(DrvZ80RAM,			0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(m58_write);
	ZetSetReadHandler(m58_read);
	ZetClose();

	IremSoundInit(DrvM6803ROM, 2, 3072000); // or type 2?
    AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x10000, 0, 0x1f);
	GenericTilemapSetOffsets(0, 0, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	IremSoundExit();

	BurnFree(AllMem);

	input_invertbit = 0;
	sprite_pal_xor = 0;

	return 0;
}

static void DrvPaletteInit()
{
#define combine_3_weights(w, b0, b1, b2) ((((b0 * w[2]) + (b1 * w[1]) + (b2 * w[0])) * 255) / (w[0]+w[1]+w[2]))
#define combine_2_weights(w, b0, b1) ((((b0 * w[1]) + (b1 * w[0])) * 255) / (w[0]+w[1]))

	UINT32 tmp[16];
	static const INT32 weights_gb[3] = { 1000, 470, 220 };
	static const INT32 weights_r[2]  = { 470, 220 };

	for (INT32 i = 0; i < 512; i++)
	{
		UINT8 promval = (DrvColPROM[i] & 0x0f) | (DrvColPROM[i + 0x200] << 4);
		INT32 r = combine_2_weights(weights_r,  BIT(promval,6), BIT(promval,7));
		INT32 g = combine_3_weights(weights_gb, BIT(promval,3), BIT(promval,4), BIT(promval,5));
		INT32 b = combine_3_weights(weights_gb, BIT(promval,0), BIT(promval,1), BIT(promval,2));

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 16; i++)
	{
		UINT8 promval = DrvColPROM[(i ^ sprite_pal_xor) + 0x400];
		INT32 r = combine_2_weights(weights_r,  BIT(promval,6), BIT(promval,7));
		INT32 g = combine_3_weights(weights_gb, BIT(promval,3), BIT(promval,4), BIT(promval,5));
		INT32 b = combine_3_weights(weights_gb, BIT(promval,0), BIT(promval,1), BIT(promval,2));

		tmp[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[0x200 + i] = tmp[DrvColPROM[i + 0x420] & 0x0f];
	}
}

static void draw_scroll_panel()
{
	for (INT32 offset = 0; offset < ((nScreenHeight + 16) * 16); offset++)
	{
		INT32 sx = (offset & 0xf);
		INT32 sy = (offset >> 4);

		if (sx < (1 + 2) || sx > 14 || (sy - 16) < 0) // + 2 on sx check is to take care of the 16px offset
			continue;

		sx = 4 * (sx - 1);

		INT32 data = DrvScrollPanel[offset];
		INT32 color = (sy & 0xfc) + 0x100;
		UINT16 *dst = pTransDraw + (sy - 16) * nScreenWidth + sx + (nScreenWidth - 56);

		for (INT32 i = 0; i < 4; i++)
		{
			INT32 pxl = ((data >> i) & 1) | ((data >> (i + 3)) & 2);

			dst[i] = color + pxl;
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0x20; offs -= 4)
	{
		INT32 attr = DrvSprRAM[offs + 1];
		INT32 bank = (attr & 0x20) >> 5;
		INT32 code1 = DrvSprRAM[offs + 2] & 0xbf;
		INT32 code2 = 0;
		INT32 color = attr & 0x1f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy1 = 233 - DrvSprRAM[offs];
		INT32 sy2 = 0;

		if (flipy)
		{
			code2 = code1;
			code1 += 0x40;
		}
		else
		{
			code2 = code1 + 0x40;
		}

		if (flipscreen)
		{
			sx = 240 - sx;
			sy2 = 192 - sy1;
			sy1 = sy2 + 0x10;
			flipx = !flipx;
			flipy = !flipy;
		}
		else
		{
			sy2 = sy1 + 0x10;
		}

		if (nSpriteEnable & 1) RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code1 + (bank * 256), color << 3, 0x00, sx, sy1, flipx, flipy, 16, 16, DrvColPROM + 0x420, 0x200);
		if (nSpriteEnable & 2) RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code2 + (bank * 256), color << 3, 0x00, sx, sy2, flipx, flipy, 16, 16, DrvColPROM + 0x420, 0x200);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);
	if ((nBurnLayer & 1) == 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ((nBurnLayer & 2) == 2) draw_sprites();

	if (score_disable == 0) {
		if ((nBurnLayer & 4) == 4) draw_scroll_panel();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6803NewFrame();
	ZetNewFrame();
	
	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff ^ input_invertbit;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 3072000);
	INT32 nCyclesTotal[2] = { 3072000 / 60, 894886 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	M6803Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
        CPU_RUN(0, Zet);

		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

        CPU_RUN(1, M6803);
		
		MSM5205Update();
		IremSoundClockSlave();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	M6803Close();
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		IremSoundScan(nAction, pnMin);

		SCAN_VAR(score_disable);
		SCAN_VAR(scrolly);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
	}

	return 0;
}


// 10-Yard Fight (World, set 1)

static struct BurnRomInfo yard10RomDesc[] = {
	{ "yf-a-3p-b",			0x2000, 0x2e205ec2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "yf-a-3n-b",			0x2000, 0x82fcd980, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yf-a-3m-b",			0x2000, 0xa8d5c311, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "yf-a.3e",			0x2000, 0x77e9e9cc, 3 | BRF_GRA },           //  7 Tiles
	{ "yf-a.3d",			0x2000, 0x854d5ff4, 3 | BRF_GRA },           //  8
	{ "yf-a.3c",			0x2000, 0x0cd8ffad, 3 | BRF_GRA },           //  9

	{ "yf-b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yard.1c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yard.1d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yard.1f",			0x0020, 0xb8554da5, 5 | BRF_GRA },           // 18
	{ "yard.2h",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yard.2n",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yard.2m",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(yard10)
STD_ROM_FN(yard10)

struct BurnDriver BurnDrvyard10 = {
	"10yard", NULL, NULL, NULL, "1983",
	"10-Yard Fight (World, set 1)\0", NULL, "Irem", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, yard10RomInfo, yard10RomName, NULL, NULL, NULL, NULL, YardInputInfo, YardDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// 10-Yard Fight (Japan)

static struct BurnRomInfo yardj10RomDesc[] = {
	{ "yf-a.3p",			0x2000, 0x4586114f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "yf-a.3n",			0x2000, 0x947fa760, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yf-a.3m",			0x2000, 0xd4975633, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "yf-a.3e",			0x2000, 0x77e9e9cc, 3 | BRF_GRA },           //  7 Tiles
	{ "yf-a.3d",			0x2000, 0x854d5ff4, 3 | BRF_GRA },           //  8
	{ "yf-a.3c",			0x2000, 0x0cd8ffad, 3 | BRF_GRA },           //  9

	{ "yf-b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yard.1c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yard.1d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yard.1f",			0x0020, 0xb8554da5, 5 | BRF_GRA },           // 18
	{ "yard.2h",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yard.2n",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yard.2m",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(yardj10)
STD_ROM_FN(yardj10)

struct BurnDriver BurnDrvyardj10 = {
	"10yardj", "10yard", NULL, NULL, "1983",
	"10-Yard Fight (Japan)\0", NULL, "Irem", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, yardj10RomInfo, yardj10RomName, NULL, NULL, NULL, NULL, YardInputInfo, YardDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Vs 10-Yard Fight (World, 11/05/84)

static struct BurnRomInfo vs10yardRomDesc[] = {
	{ "a.3p",				0x2000, 0x1edac08f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "vyf-a.3m",			0x2000, 0x3b9330f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a.3m",				0x2000, 0xcf783dad, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "vyf-a.3a",			0x2000, 0x354d7330, 3 | BRF_GRA },           //  7 Tiles
	{ "vyf-a.3c",			0x2000, 0xf48eedca, 3 | BRF_GRA },           //  8
	{ "vyf-a.3d",			0x2000, 0x7d1b4d93, 3 | BRF_GRA },           //  9

	{ "yf-b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yard.1c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yard.1d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yard.1f",			0x0020, 0xb8554da5, 5 | BRF_GRA },           // 18
	{ "yard.2h",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yard.2n",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yard.2m",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(vs10yard)
STD_ROM_FN(vs10yard)

static INT32 Vs10yard()
{
	input_invertbit = 0x10;

	return DrvInit();
}

struct BurnDriver BurnDrvVs10yard = {
	"vs10yard", "10yard", NULL, NULL, "1984",
	"Vs 10-Yard Fight (World, 11/05/84)\0", NULL, "Irem", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, vs10yardRomInfo, vs10yardRomName, NULL, NULL, NULL, NULL, YardInputInfo, YardDIPInfo,
	Vs10yard, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Vs 10-Yard Fight (Japan)

static struct BurnRomInfo vs10yardjRomDesc[] = {
	{ "vyf-a.3n",			0x2000, 0x418e01fc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "vyf-a.3m",			0x2000, 0x3b9330f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vyf-a.3k",			0x2000, 0xa0ec15bb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "vyf-a.3a",			0x2000, 0x354d7330, 3 | BRF_GRA },           //  7 Tiles
	{ "vyf-a.3c",			0x2000, 0xf48eedca, 3 | BRF_GRA },           //  8
	{ "vyf-a.3d",			0x2000, 0x7d1b4d93, 3 | BRF_GRA },           //  9

	{ "yf-b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yard.1c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yard.1d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yard.1f",			0x0020, 0xb8554da5, 5 | BRF_GRA },           // 18
	{ "yard.2h",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yard.2n",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yard.2m",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(vs10yardj)
STD_ROM_FN(vs10yardj)

struct BurnDriver BurnDrvVs10yardj = {
	"vs10yardj", "10yard", NULL, NULL, "1984",
	"Vs 10-Yard Fight (Japan)\0", NULL, "Irem", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, vs10yardjRomInfo, vs10yardjRomName, NULL, NULL, NULL, NULL, YardInputInfo, Vs10yarjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Vs 10-Yard Fight (US, Taito license)

static struct BurnRomInfo vs10yarduRomDesc[] = {
	{ "yf-a-3p-vu.3p",		0x2000, 0xf5243513, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "yf-a-3n-h-vs.3n",	0x2000, 0xa14d7a14, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yf-a-3m-h-vs.3m",	0x2000, 0xdc4bb0ce, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s-3b.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s-1b.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s-3a.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s-1a.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "yf-a-3e-h-vs.3e",	0x2000, 0x354d7330, 3 | BRF_GRA },           //  7 Tiles
	{ "yf-a-3c-h-vs.3c",	0x2000, 0xf48eedca, 3 | BRF_GRA },           //  8
	{ "yf-a-3d-h-vs.3d",	0x2000, 0x7d1b4d93, 3 | BRF_GRA },           //  9

	{ "yf-b-5b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b-5c.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b-5f.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b-5e.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b-5j.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b-5k.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yf-a-5c.5c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yf-a-5d.5d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yf-b-2b.2b",			0x0020, 0xfcd283ea, 5 | BRF_GRA },           // 18
	{ "yf-b-3l.3l",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yf-b-2r.2r",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yf-b-2p.2p",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(vs10yardu)
STD_ROM_FN(vs10yardu)

static INT32 Vs10yarduInit()
{
	input_invertbit = 0x10;
	sprite_pal_xor = 0xf;
	return DrvInit();
}

struct BurnDriver BurnDrvVs10yardu = {
	"vs10yardu", "10yard", NULL, NULL, "1984",
	"Vs 10-Yard Fight (US, Taito license)\0", NULL, "Irem (Taito license)", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, vs10yarduRomInfo, vs10yarduRomName, NULL, NULL, NULL, NULL, YardInputInfo, YardDIPInfo,
	Vs10yarduInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// 10-Yard Fight '85 (US, Taito license)

static struct BurnRomInfo yard1085RomDesc[] = {
	{ "yf-a-3p-h.3p",		0x2000, 0xc83da5e3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "yf-a-3n-h.3n",		0x2000, 0x8dc5f32f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yf-a-3m-h.3m",		0x2000, 0x7d5d0c20, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "yf-s-3b.3b",			0x2000, 0x0392a60c, 2 | BRF_GRA },           //  3 M6803 Code
	{ "yf-s-1b.1b",			0x2000, 0x6588f41a, 2 | BRF_GRA },           //  4
	{ "yf-s-3a.3a",			0x2000, 0xbd054e44, 2 | BRF_GRA },           //  5
	{ "yf-s-1a.1a",			0x2000, 0x2490d4c3, 2 | BRF_GRA },           //  6

	{ "yf-a-3e-h.3e",		0x2000, 0x5fba9074, 3 | BRF_GRA },           //  7 Tiles
	{ "yf-a-3c-h.3c",		0x2000, 0xf48eedca, 3 | BRF_GRA },           //  8
	{ "yf-a-3d-h.3d",		0x2000, 0x7d1b4d93, 3 | BRF_GRA },           //  9

	{ "yf-b-5b.5b",			0x2000, 0x1299ae30, 4 | BRF_GRA },           // 10 Sprites
	{ "yf-b-5c.5c",			0x2000, 0x8708b888, 4 | BRF_GRA },           // 11
	{ "yf-b-5f.5f",			0x2000, 0xd9bb8ab8, 4 | BRF_GRA },           // 12
	{ "yf-b-5e.5e",			0x2000, 0x47077e8d, 4 | BRF_GRA },           // 13
	{ "yf-b-5j.5j",			0x2000, 0x713ef31f, 4 | BRF_GRA },           // 14
	{ "yf-b-5k.5k",			0x2000, 0xf49651cc, 4 | BRF_GRA },           // 15

	{ "yf-a-5c.5c",			0x0100, 0x08fa5103, 5 | BRF_GRA },           // 16 Color Data
	{ "yf-a-5d.5d",			0x0100, 0x7c04994c, 5 | BRF_GRA },           // 17
	{ "yf-b-2b.2b",			0x0020, 0xfcd283ea, 5 | BRF_GRA },           // 18
	{ "yf-b-3l.3l",			0x0100, 0xe1cdfb06, 5 | BRF_GRA },           // 19
	{ "yf-b-2r.2r",			0x0100, 0xcd85b646, 5 | BRF_GRA },           // 20
	{ "yf-b-2p.2p",			0x0100, 0x45384397, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(yard1085)
STD_ROM_FN(yard1085)

struct BurnDriver BurnDrvyard1085 = {
	"10yard85", "10yard", NULL, NULL, "1985",
	"10-Yard Fight '85 (US, Taito license)\0", NULL, "Irem (Taito license)", "M58",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_SPORTSMISC, 0,
	NULL, yard1085RomInfo, yard1085RomName, NULL, NULL, NULL, NULL, YardInputInfo, YardDIPInfo,
	Vs10yarduInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};
