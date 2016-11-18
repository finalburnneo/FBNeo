// FB Alpha Joyful Road / Munch Mobile driver module
// Based on MAME driver by Phil Stroffolino

/*
	To do (Remove as completed):
		Video frequency is not correct, should be 57hz.
*/

#include "tiles_generic.h"
#include "z80_intf.h"
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
static UINT8 *DrvMapROM1;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvStatRAM;
static UINT8 *DrvSprXRAM;
static UINT8 *DrvSprTRAM;
static UINT8 *DrvSprARAM;
static UINT8 *DrvVRegs;

static UINT16 *DrvBGBitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 flipscreen;
static UINT8 nmi_enable;
static UINT8 soundlatch;
static UINT8 palette_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 4,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 5,	"p3 right"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy3 + 4,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy3 + 5,	"p4 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dips"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dips"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL				},
	{0x12, 0xff, 0xff, 0x88, NULL				},

	{0   , 0xfe, 0   ,    2, "Continue after game over? (Cheat)"	},
	{0x11, 0x01, 0x01, 0x00, "Off"				},
	{0x11, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,   12, "Coin A"			},
	{0x11, 0x01, 0x1e, 0x14, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0x1e, 0x10, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0x1e, 0x16, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x1e, 0x00, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0x1e, 0x12, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x1e, 0x02, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x1e, 0x04, "1 Coins 3 Credits"		},
	{0x11, 0x01, 0x1e, 0x06, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x1e, 0x08, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x1e, 0x0c, "1 Coin  7 Credits"		},
	{0x11, 0x01, 0x1e, 0x0e, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    8, "First Bonus"			},
	{0x11, 0x01, 0xe0, 0x00, "10000"			},
	{0x11, 0x01, 0xe0, 0x20, "20000"			},
	{0x11, 0x01, 0xe0, 0x40, "30000"			},
	{0x11, 0x01, 0xe0, 0x60, "40000"			},
	{0x11, 0x01, 0xe0, 0x80, "50000"			},
	{0x11, 0x01, 0xe0, 0xa0, "60000"			},
	{0x11, 0x01, 0xe0, 0xc0, "70000"			},
	{0x11, 0x01, 0xe0, 0xe0, "None"				},

	{0   , 0xfe, 0   ,    4, "Second Bonus (First +)"	},
	{0x12, 0x01, 0x03, 0x00, "30000"			},
	{0x12, 0x01, 0x03, 0x01, "40000"			},
	{0x12, 0x01, 0x03, 0x02, "100000"			},
	{0x12, 0x01, 0x03, 0x03, "None"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x0c, 0x00, "1"				},
	{0x12, 0x01, 0x0c, 0x04, "2"				},
	{0x12, 0x01, 0x0c, 0x08, "3"				},
	{0x12, 0x01, 0x0c, 0x0c, "5"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x12, 0x01, 0x10, 0x00, "Off"				},
	{0x12, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x20, "Off"				},
	{0x12, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    1, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
//	{0x12, 0x01, 0x40, 0x40, "Cocktail"			}, // not supported

	{0   , 0xfe, 0   ,    2, "Additional Bonus (Second)"	},
	{0x12, 0x01, 0x80, 0x00, "No"				},
	{0x12, 0x01, 0x80, 0x80, "Yes"				},
};

STDDIPINFO(Drv)

static void __fastcall mnchmobl_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xbaba:
		return; // nop

		case 0xbe00: {
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xbe01:
			palette_bank = data & 0x03;
		return;

		case 0xbe11:
		case 0xbe21:
		case 0xbe31:
		return; // nop

		case 0xbe41:
			flipscreen = data;
		return;

		case 0xbe61:
			nmi_enable = data;
		return;

		case 0xbf00:
		case 0xbf01:
		case 0xbf02:
		case 0xbf03:
		case 0xbf04:
		case 0xbf05:
		case 0xbf06:
		case 0xbf07:
			DrvVRegs[address & 7] = data;
		return;
	}
}

static UINT8 __fastcall mnchmobl_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xbe02:
			return DrvDips[0];

		case 0xbe03:
			return DrvDips[1];

		case 0xbf01:
			return DrvInputs[0];

		case 0xbf02:
			return DrvInputs[1];

		case 0xbf03:
			return DrvInputs[2];
	}
	bprintf(0, _T("u-mr %X.\n"), address);
	return 0;
}

static void __fastcall mnchmobl_sound_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x1fff)
	{
		case 0x4000:
			AY8910Write(0, (~address >> 12) & 1, data);
		return;

		case 0x6000:
			AY8910Write(1, (~address >> 12) & 1, data);
		return;

		case 0x8000:
			AY8910Reset(0);
		return;

		case 0xa000:
			AY8910Reset(1);
		return;

		case 0xc000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall mnchmobl_sound_read(UINT16 address)
{
	switch (address & ~0x1fff)
	{
		case 0x2000:
			//ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x8000:
			AY8910Reset(0);
			return 0; 

		case 0xa000:
			AY8910Reset(1);
			return 0; 
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
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	HiscoreReset();

	flipscreen = 0;
	nmi_enable = 0;
	soundlatch = 0;
	palette_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x004000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvMapROM1		= Next; Next += 0x021000;
	DrvGfxROM1		= Next; Next += 0x042000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000400;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000100;
	DrvStatRAM		= Next; Next += 0x000100;
	DrvSprXRAM		= Next; Next += 0x000400;
	DrvSprTRAM		= Next; Next += 0x000400;
	DrvSprARAM		= Next; Next += 0x000400;

	DrvVRegs		= Next; Next += 0x0000080;

	DrvBGBitmap     = (UINT16*)Next; Next += (512 * 512) * sizeof(UINT16);

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;

		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;

		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;

		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0, 8, 0x8000, 0x8008 };
	INT32 Plane1[4] = { 8,12,0,4 };
	INT32 Plane2[3] = { 0x4000*8,0x2000*8,0 };
	INT32 Plane3[3] = { 0,0,0 }; // monochrome

	INT32 XOffs0[8] = { STEP8(7,-1) };
	INT32 XOffs1[8] = { 0,0,1,1,2,2,3,3 };
	INT32 XOffs2[32] =
	{
		7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0,
		0x8000+7,0x8000+7,0x8000+6,0x8000+6,0x8000+5,0x8000+5,0x8000+4,0x8000+4,
		0x8000+3,0x8000+3,0x8000+2,0x8000+2,0x8000+1,0x8000+1,0x8000+0,0x8000+0
	};
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs2[32] = { STEP32(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x1000);

	GfxDecode(0x0080, 4,  8,  8, Plane1, XOffs1, YOffs0, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x6000);

	GfxDecode(0x0080, 3, 32, 32, Plane2, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM2 + 0x20000, 0x2000);

	GfxDecode(0x0080, 3, 32, 32, Plane3, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM2 + 0x20000);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x01000,  4, 1)) return 1;

		if (BurnLoadRom(DrvMapROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;

		DrvPaletteInit();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvSprXRAM,	0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvSprXRAM,	0xa400, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvSprTRAM,	0xa800, 0xabff, MAP_RAM);
	ZetMapMemory(DrvSprTRAM,	0xac00, 0xafff, MAP_RAM);
	ZetMapMemory(DrvSprARAM,	0xb000, 0xb3ff, MAP_RAM);
	ZetMapMemory(DrvSprARAM,	0xb400, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xb800, 0xb8ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xb900, 0xb9ff, MAP_RAM);
	ZetMapMemory(DrvStatRAM,	0xbc00, 0xbcff, MAP_RAM);
	ZetSetWriteHandler(mnchmobl_main_write);
	ZetSetReadHandler(mnchmobl_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(mnchmobl_sound_write);
	ZetSetReadHandler(mnchmobl_sound_read);
	ZetClose();

	AY8910Init(0, 1875000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1875000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.50, BURN_SND_ROUTE_BOTH);

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

	BurnFree(AllMem);

	return 0;
}

static void draw_status()
{
	for (INT32 row = 0; row < 4; row++) {

		INT32 sx = ((row & 1) << 3) + ((~row & 2) ? 304 : 0);

		for (INT32 sy = 0; sy < 256; sy+=8) {
			INT32 code = DrvStatRAM[((~row & 3) << 5) + (sy >> 3)];

			Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, 0, DrvGfxROM0);
		}
	}
}

static void draw_background()
{
	INT32 scrollx = (-(DrvVRegs[6] *2 + (DrvVRegs[7] >> 7)) - 64 - 128 - 16) & 0x1ff; 

	GenericTilesSetClipRaw(0, 512, 0, 512);

	for (INT32 offs = 0; offs < 16 * 16; offs++)
	{
		INT32 sy = (offs % 16) * 32;
		INT32 sx = (offs / 16) * 32;

		INT32 code_offset = DrvVidRAM[offs];

		for (INT32 row = 0; row < 4; row++)
		{
			for (INT32 col = 0; col < 4; col++)
			{
				INT32 code = DrvMapROM1[col + code_offset * 4 + row * 0x400] & 0x7f;

				Render8x8Tile_Clip(DrvBGBitmap, code, (sx + (col * 8)), sy + (row * 8), palette_bank + 0x04, 4, 0, DrvGfxROM1);
			}
		}
	}

	GenericTilesClearClipRaw();

	// copy the BGBitmap to pTransDraw
	for (INT32 sy = 0; sy < nScreenHeight; sy++) {

		UINT16 *src = DrvBGBitmap + sy * 512;
		UINT16 *dst = pTransDraw + sy * nScreenWidth;

		for (INT32 sx = 0; sx < nScreenWidth; sx++) {
			dst[sx] = src[(sx-scrollx)&0x1ff];
		}
	}

}

static void draw_sprites()
{
	int scroll = DrvVRegs[6];
	int flags = DrvVRegs[7];
	int xadjust = - 128 - 16 - ((flags & 0x80) ? 1 : 0);
	int bank = (flags & 0x40) ? 0x80 : 0;

	int color_base = (palette_bank * 4) + 3;

	int firstsprite = DrvVRegs[4] & 0x3f;

	for (INT32 i = firstsprite; i < firstsprite + 0x40; i++)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			INT32 offs = (j << 6) | (i & 0x3f);
			INT32 attributes = DrvSprARAM[offs];

			if (attributes & 0x80)
			{
				INT32 tile_number = DrvSprTRAM[offs] ^ 0x7f;
				INT32 sx = DrvSprXRAM[offs];
				INT32 sy = ((offs >> 6) << 5) + ((attributes >> 2) & 0x1f);

				sx = (sx >> 1) | (tile_number & 0x80);
				sx = 2 * ((- 32 - scroll - sx) & 0xff) + xadjust;

				INT32 color = color_base - (attributes & 0x03);

				Render32x32Tile_Mask_Clip(pTransDraw, (tile_number & 0x7f) + bank, sx, sy, color, 3, 7, 0x80, DrvGfxROM2);
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
	BurnTransferClear();
	if (nBurnLayer & 1) draw_background();
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_status();

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

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3750000 / 57, 3750000 / 57 }; // 57HZ...
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 220) { // 220? weird? probably due to hz? any higher and car flickers
			if (nmi_enable) ZetNmi();
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == 220) {
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK); // nmi
		}
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(palette_bank);
	}

	return 0;
}


// Joyful Road (Japan)

static struct BurnRomInfo joyfulrRomDesc[] = {
	{ "m1j.10e",		0x2000, 0x1fe86e25, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "m2j.10d",		0x2000, 0xb144b9a6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mu.2j",		0x2000, 0x420adbd4, 2 | BRF_PRG | BRF_ESS }, //  2 - Z80 #1 Code

	{ "s1.10a",		0x1000, 0xc0bcc301, 3 | BRF_GRA },           //  3 - Characters
	{ "s2.10b",		0x1000, 0x96aa11ca, 3 | BRF_GRA },           //  4

	{ "b1.2c",		0x1000, 0x8ce3a403, 4 | BRF_GRA },           //  5 - Background Tile Map

	{ "b2.2b",		0x1000, 0x0df28913, 5 | BRF_GRA },           //  6 - Background Tiles

	{ "f1j.1g",		0x2000, 0x93c3c17e, 6 | BRF_GRA },           //  7 - Sprites
	{ "f2j.3g",		0x2000, 0xb3fb5bd2, 6 | BRF_GRA },           //  8
	{ "f3j.5g",		0x2000, 0x772a7527, 6 | BRF_GRA },           //  9
	{ "h", 			0x2000, 0x332584de, 6 | BRF_GRA },           // 10 - Monochrome Sprites

	{ "a2001.clr", 		0x0100, 0x1b16b907, 7 | BRF_GRA },           // 11 - Color PROMs
};

STD_ROM_PICK(joyfulr)
STD_ROM_FN(joyfulr)

struct BurnDriver BurnDrvJoyfulr = {
	"joyfulr", NULL, NULL, NULL, "1983",
	"Joyful Road (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, joyfulrRomInfo, joyfulrRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 320, 3, 4
};


// Munch Mobile (US)

static struct BurnRomInfo mnchmoblRomDesc[] = {
	{ "m1.10e",		0x2000, 0xa4bebc6a, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "m2.10d",		0x2000, 0xf502d466, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mu.2j",		0x2000, 0x420adbd4, 2 | BRF_PRG | BRF_ESS }, //  2 - Z80 #1 Code

	{ "s1.10a",		0x1000, 0xc0bcc301, 3 | BRF_GRA },           //  3 - Characters
	{ "s2.10b",		0x1000, 0x96aa11ca, 3 | BRF_GRA },           //  4

	{ "b1.2c",		0x1000, 0x8ce3a403, 4 | BRF_GRA },           //  5 - Background Tile Map

	{ "b2.2b",		0x1000, 0x0df28913, 5 | BRF_GRA },           //  6 - Background Tiles

	{ "f1.1g",		0x2000, 0xb75411d4, 6 | BRF_GRA },           //  7 - Sprites
	{ "f2.3g",		0x2000, 0x539a43ba, 6 | BRF_GRA },           //  8
	{ "f3.5g",		0x2000, 0xec996706, 5 | BRF_GRA },           //  9
	{ "h", 			0x2000, 0x332584de, 6 | BRF_GRA },           // 10 - Monochrome Sprites

	{ "a2001.clr", 		0x0100, 0x1b16b907, 7 | BRF_GRA },           // 11 - Color PROMs
};

STD_ROM_PICK(mnchmobl)
STD_ROM_FN(mnchmobl)

struct BurnDriver BurnDrvMnchmobl = {
	"mnchmobl", "joyfulr", NULL, NULL, "1983",
	"Munch Mobile (US)\0", NULL, "SNK (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, mnchmoblRomInfo, mnchmoblRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 320, 3, 4
};
