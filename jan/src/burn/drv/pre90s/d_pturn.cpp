// FB Alpha Parallel Turn driver module
// Based on MAME driver by Tomasz Slanina and Tatsuyuki Satoh

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
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvMapROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 soundlatch;
static UINT8 nmi_enable;
static UINT8 nmi_sub_enable;
static UINT8 sub_4000_data;
static UINT8 fgpalette;
static UINT8 bgpalette;
static UINT8 fgbank;
static UINT8 bgbank;
static UINT8 bgcolor;
static UINT8 bgscrollx;
static UINT16 bgscrolly;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x13, 0xff, 0xff, 0x04, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "7"			},
	{0x13, 0x01, 0x03, 0x03, "Infinite (Cheat)"	},

	{0   , 0xfe, 0   ,    4, "Bonus Lives"		},
	{0x13, 0x01, 0x0c, 0x00, "None"			},
	{0x13, 0x01, 0x0c, 0x04, "20000"		},
	{0x13, 0x01, 0x0c, 0x08, "50000"		},
	{0x13, 0x01, 0x0c, 0x0c, "100000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x00, "Upright"		},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x07, 0x07, "6 Coins 1 Credit"	},
	{0x14, 0x01, 0x07, 0x06, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x07, 0x04, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x07, 0x00, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x38, "6 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x30, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x20, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x00, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},


	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x40, 0x00, "Normal"		},
	{0x14, 0x01, 0x40, 0x40, "Stop Motion"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0x80, 0x00, "English"		},
	{0x14, 0x01, 0x80, 0x80, "Japanese"		},
};

STDDIPINFO(Drv)

static void __fastcall pturn_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xdfe0:
		return; // nop

		case 0xe400:
			fgpalette = data & 0x1f;
		return;

		case 0xe800:
			soundlatch = data;
		return;

		case 0xf400:
			bgscrolly = (data>>5)*32*8;
			bgpalette = (data & 0x1f);
		return;

		case 0xf800:
		return;  // nop

		case 0xf801:
			bgcolor = data;
		return;

		case 0xf803:
			bgscrollx = data;
		return;

		case 0xfc00:
			flipscreen = data;
		return;

		case 0xfc01:
			nmi_enable = data;
		return;

		case 0xfc02:
		case 0xfc03:
		return;	// nop

		case 0xfc04:
			bgbank = data & 1;
		return;

		case 0xfc05:
			fgbank = data & 1;
		return;

		case 0xfc06:
		case 0xfc07:
		return; // nop
	}
}

static UINT8 __fastcall pturn_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc803:
		case 0xca00:
			return 0x00; // protection

		case 0xca73:
			return 0xbe; // protection

		case 0xca74:
			return 0x66; // protection

		case 0xf800:
		case 0xf801:
		case 0xf802:
			return DrvInputs[address & 3];

		case 0xf804:
			return DrvDips[1];

		case 0xf805:
			return DrvDips[0];

		case 0xf806: // protection - status?
			return 0; // bit 1 (0x2) must be 0!
	}

	return 0;
}

static void __fastcall pturn_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			nmi_sub_enable = data;
		return;

		case 0x4000:
			sub_4000_data = data;
		return;

		case 0x5000:
		case 0x5001:
		case 0x6000:
		case 0x6001:
			AY8910Write((address >> 13) & 1, (address & 1), data);
		return;
	}
}

static UINT8 __fastcall pturn_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return soundlatch;

		case 0x4000:
			return sub_4000_data;
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

	soundlatch = 0;
	nmi_enable = 0;
	nmi_sub_enable = 0;
	sub_4000_data = 0;
	fgpalette = 0;
	bgpalette = 0;
	fgbank = 0;
	bgbank = 0;
	bgcolor = 0;
	bgscrolly = 0;
	bgscrollx = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

 	DrvZ80ROM0	= Next; Next += 0x008000;
 	DrvZ80ROM1	= Next; Next += 0x001000;

	DrvGfxROM0	= Next; Next += 0x008000;
	DrvGfxROM1	= Next; Next += 0x008000;
	DrvGfxROM2	= Next; Next += 0x010000;

 	DrvColPROM	= Next; Next += 0x000300;

 	DrvMapROM	= Next; Next += 0x002000;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x000800;
	DrvZ80RAM1	= Next; Next += 0x000400;
	DrvVidRAM	= Next; Next += 0x000400;
	DrvSprRAM	= Next; Next += 0x000100;

	RamEnd		= Next;

	pAY8910Buffer[0]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]    = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);


	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { RGN_FRAC(0x3000, 0,3), RGN_FRAC(0x3000, 1,3), RGN_FRAC(0x3000, 2,3) };
	INT32 Plane32[3]  = { RGN_FRAC(0x6000, 0,3), RGN_FRAC(0x6000, 1,3), RGN_FRAC(0x6000, 2,3) };
	INT32 XOffs[32] = { STEP8(0,1), STEP8(64,1), STEP8(128,1), STEP8(192,1) };
	INT32 YOffs[32] = { STEP8(0,8), STEP8(256,8), STEP8(512,8), STEP8(768,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) return 1;

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0200, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0200, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x6000);

	GfxDecode(0x0040, 3, 32, 32, Plane32, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void DrvPaletteInit()
{
	const UINT8 tab[16] = {
		0x00, 0x0e, 0x1f, 0x2d, 0x43, 0x51, 0x62, 0x70,
		0x8f, 0x9d, 0xae, 0xbc, 0xd2, 0xe0, 0xf1, 0xff
	};

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = tab[DrvColPROM[i+0x000] & 0xf];
		UINT8 g = tab[DrvColPROM[i+0x100] & 0xf];
		UINT8 b = tab[DrvColPROM[i+0x200] & 0xf];

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x01000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x01000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 16, 1)) return 1;

		if (BurnLoadRom(DrvMapROM  + 0x00000, 17, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf000, 0xf0ff, MAP_RAM);
	ZetSetWriteHandler(pturn_main_write);
	ZetSetReadHandler(pturn_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(pturn_sound_write);
	ZetSetReadHandler(pturn_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

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

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;
		sy -= 16;

		INT32 code = DrvVidRAM[offs];

		code = (code & 0x9f) | ((code & 0x20) << 1) | ((code & 0x40) >> 1) | (fgbank << 8);

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, fgpalette, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_bg_layer()
{
	INT32 color = ((bgpalette == 1) ? 25 : bgpalette);

	for (INT32 offs = 0; offs < 32 * (32 * 8); offs++)
	{
		INT32 sx = (offs % 0x20) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= bgscrollx;
		if (sx < -7) sx += 256;

		sy -= 16; //offset
		if (sy < -7) sy += 256;

		INT32 code = DrvMapROM[offs + ( bgscrolly * 4 )] + (bgbank * 256);

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80-4; offs >= 0; offs -= 4)
	{
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 sy    = 256 - DrvSprRAM[offs + 0] - 16;
		INT32 sx    = DrvSprRAM[offs + 3] - 16;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 code  = attr & 0x3f;
		INT32 color = DrvSprRAM[offs + 2] & 0x1f;

		sy -= 16; // offset

		if (flipscreen)
		{
			sx = 224 - sx;
			sy = 224 - sy;
			flipx ^= 0x40;
			flipy ^= 0x80;
		}

		if (sx | sy)
		{
			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
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

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = bgcolor;
	}

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 12;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1) && nmi_enable) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if ((i % (nInterleave / 3)) == ((nInterleave / 3)-1) && nmi_sub_enable) ZetNmi();
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

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(nmi_sub_enable);
		SCAN_VAR(sub_4000_data);
		SCAN_VAR(fgpalette);
		SCAN_VAR(bgpalette);
		SCAN_VAR(fgbank);
		SCAN_VAR(bgbank);
		SCAN_VAR(bgcolor);
		SCAN_VAR(bgscrolly);
		SCAN_VAR(bgscrollx);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Parallel Turn

static struct BurnRomInfo pturnRomDesc[] = {
	{ "prom4.8d",		0x2000, 0xd3ae0840, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "prom6.8b",		0x2000, 0x65f09c56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prom5.7d",		0x2000, 0xde48afb4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prom7.7b",		0x2000, 0xbfaeff9f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "prom9.5n",		0x1000, 0x8b4d944e, 2 | BRF_PRG | BRF_ESS }, //  4 - Z80 #1 Code

	{ "prom1.8k",		0x1000, 0x10aba36d, 3 | BRF_GRA },           //  5 - Foreground Tiles
	{ "prom2.7k",		0x1000, 0xb8a4d94e, 3 | BRF_GRA },           //  6
	{ "prom3.6k",		0x1000, 0x9f51185b, 3 | BRF_GRA },           //  7

	{ "prom11.16f",		0x1000, 0x129122c6, 4 | BRF_GRA },           //  8 - Background Tiles
	{ "prom12.16h",		0x1000, 0x69b09323, 4 | BRF_GRA },           //  9
	{ "prom13.16k",		0x1000, 0xe9f67599, 4 | BRF_GRA },           // 10

	{ "prom14.16l",		0x2000, 0xffaa0b8a, 5 | BRF_GRA },           // 11 - Sprites
	{ "prom15.16m",		0x2000, 0x41445155, 5 | BRF_GRA },           // 12
	{ "prom16.16p",		0x2000, 0x94814c5d, 5 | BRF_GRA },           // 13

	{ "prom_red.3p",	0x0100, 0x505fd8c2, 6 | BRF_GRA },           // 14 - Color PROMs
	{ "prom_grn.4p",	0x0100, 0x6a00199d, 6 | BRF_GRA },           // 15
	{ "prom_blu.4r",	0x0100, 0x7b4c5788, 6 | BRF_GRA },           // 16

	{ "prom10.16d",		0x2000, 0xa96e3c95, 7 | BRF_GRA },           // 17 - Background Tile Map
};

STD_ROM_PICK(pturn)
STD_ROM_FN(pturn)

struct BurnDriver BurnDrvPturn = {
	"pturn", NULL, NULL, NULL, "1984",
	"Parallel Turn\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, pturnRomInfo, pturnRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
