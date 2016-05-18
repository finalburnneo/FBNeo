// Zodiack emu-layer for FB Alpha by dink/iq_132, based on the MAME driver by David Haywood and Pierpaolo Prazzoli.

#include "tiles_generic.h"
#include "driver.h"
#include "bitswap.h"
#include "z80_intf.h"
extern "C" {
    #include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvDecROM0;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *calorie_bg;

static UINT8 UpdatePal;

static struct BurnInputInfo CalorieInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Calorie)

static struct BurnDIPInfo CalorieDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0x30, NULL		},
	{0x12, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x00, "1 Coin 1 Credits "		},
	{0x11, 0x01, 0x03, 0x01, "1 Coin 2 Credits "		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin 3 Credits "		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin 6 Credits "		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits "		},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin 1 Credits "		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin 2 Credits "		},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin 3 Credits "		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x10, 0x10, "Upright"		},
	{0x11, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x20, 0x00, "Off"		},
	{0x11, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0xc0, 0xc0, "2"		},
	{0x11, 0x01, 0xc0, 0x00, "3"		},
	{0x11, 0x01, 0xc0, 0x40, "4"		},
	{0x11, 0x01, 0xc0, 0x80, "5"		},

	{0   , 0xfe, 0   ,    3, "Bonus Life"		},
	{0x12, 0x01, 0x03, 0x00, "None"		},
	{0x12, 0x01, 0x03, 0x01, "20,000 Only"		},
	{0x12, 0x01, 0x03, 0x03, "20,000 and 60,000"		},

	{0   , 0xfe, 0   ,    2, "Number of Bombs"		},
	{0x12, 0x01, 0x04, 0x00, "3"		},
	{0x12, 0x01, 0x04, 0x04, "5"		},

	{0   , 0xfe, 0   ,    2, "Difficulty - Mogura Nian"		},
	{0x12, 0x01, 0x08, 0x00, "Normal"		},
	{0x12, 0x01, 0x08, 0x08, "Hard"		},

	{0   , 0xfe, 0   ,    4, "Difficulty - Select of Mogura"		},
	{0x12, 0x01, 0x30, 0x00, "Easy"		},
	{0x12, 0x01, 0x30, 0x20, "Normal"		},
	{0x12, 0x01, 0x30, 0x10, "Hard"		},
	{0x12, 0x01, 0x30, 0x30, "Hardest"		},

	{0   , 0xfe, 0   ,    0, "Infinite Lives"		},
	{0x12, 0x01, 0x80, 0x00, "Off"		},
	{0x12, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Calorie)

void __fastcall calorie_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xdc00) {
		DrvPalRAM[address & 0x0ff] = data;
		UpdatePal = 1;
		return;
	}

	switch (address)
	{
		case 0xde00:
			*calorie_bg = data;
		return;

		case 0xf004:
			*flipscreen = data & 1;
		return;

		case 0xf800:
			*soundlatch = data;
		return;
	}
}

UINT8 __fastcall calorie_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvInputs[0];

		case 0xf001:
			return DrvInputs[1];

		case 0xf002:
			return DrvInputs[2];

		case 0xf004:
			return DrvDips[0];

		case 0xf005:
			return DrvDips[1];
	}

	return 0;
}

UINT8 __fastcall calorie_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			UINT8 latch = *soundlatch;
			*soundlatch = 0;
			return latch;
	}

	return 0;
}

UINT8 __fastcall calorie_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
			return AY8910Read(0);

		case 0x11:
			return AY8910Read(1);
	}

	return 0;
}

void __fastcall calorie_sound_out(UINT16 port, UINT8 data)
{
	port &= 0xff;
	switch (port)
	{
		case 0x00:
		case 0x01:
		case 0x10:
		case 0x11:
			AY8910Write(port >> 4, port & 1, data);
			return;
	}
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

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x010000;
	DrvDecROM0	= Next; Next += 0x010000;
	DrvZ80ROM1	= Next; Next += 0x010000;

	DrvGfxROM4	= Next; Next += 0x002000;
	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x020000;
	DrvGfxROM2	= Next; Next += 0x020000;
	DrvGfxROM3	= Next; Next += 0x020000;

	DrvPalette	= (UINT32*)Next; Next += 0x0200 * sizeof(INT32);

	pAY8910Buffer[0] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	AllRam		= Next;

	DrvSprRAM	= Next; Next += 0x000400;
	DrvPalRAM	= Next; Next += 0x000100;
	DrvVidRAM	= Next; Next += 0x000800;

	DrvZ80RAM0	= Next; Next += 0x001000;
	DrvZ80RAM1	= Next; Next += 0x000800;

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;
	calorie_bg	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { RGN_FRAC(0xc000, 0, 3), RGN_FRAC(0xc000, 1, 3), RGN_FRAC(0xc000, 2, 3) };
	INT32 Plane6000[3] = { RGN_FRAC(0x6000, 0, 3), RGN_FRAC(0x6000, 1, 3), RGN_FRAC(0x6000, 2, 3) };

	INT32 XOffs[32] = { 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
			40*8+0, 40*8+1, 40*8+2, 40*8+3, 40*8+4, 40*8+5, 40*8+6, 40*8+7 };
	INT32 YOffs[32] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			80*8, 81*8, 82*8, 83*8, 84*8, 85*8, 86*8, 87*8 };

	UINT8 *tmp = (UINT8*)malloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);
	GfxDecode(0x0080, 3, 32, 32, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0xc000);

	GfxDecode(0x0400, 3,  8,  8, Plane6000, XOffs, YOffs, 0x040, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0xc000);

	GfxDecode(0x0200, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM3);

	free (tmp);

	return 0;
}

static INT32 DrvInit(void (*pInitCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,	0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,	1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,	2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,	3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x0000,	4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,	5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,	6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x8000,	7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,	8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,	9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000,   10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000,   11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x4000,   12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x8000,   13, 1)) return 1;

		DrvGfxDecode();
	}

	if (pInitCallback) {
		pInitCallback();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvDecROM0, DrvZ80ROM0);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM0 + 0x8000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM0 + 0x8000);
	ZetMapArea(0xc000, 0xcfff, 0, DrvZ80RAM0);
	ZetMapArea(0xc000, 0xcfff, 1, DrvZ80RAM0);
	ZetMapArea(0xc000, 0xcfff, 2, DrvZ80RAM0);
	ZetMapArea(0xd000, 0xd7ff, 0, DrvVidRAM);
	ZetMapArea(0xd000, 0xd7ff, 1, DrvVidRAM);
	ZetMapArea(0xd000, 0xd7ff, 2, DrvVidRAM);
	ZetMapArea(0xd800, 0xdbff, 0, DrvSprRAM);
	ZetMapArea(0xd800, 0xdbff, 1, DrvSprRAM);
	ZetMapArea(0xd800, 0xdbff, 2, DrvSprRAM);
	ZetSetWriteHandler(calorie_write);
	ZetSetReadHandler(calorie_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x3fff, 0, DrvZ80ROM1);
	ZetMapArea(0x0000, 0x3fff, 2, DrvZ80ROM1);
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80RAM1);
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80RAM1);
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80RAM1);
	ZetSetReadHandler(calorie_sound_read);
	ZetSetOutHandler(calorie_sound_out);
	ZetSetInHandler(calorie_sound_in);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

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

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static void draw_bg_layer()
{
	INT32 bg_base = (*calorie_bg & 0x0f) * 0x200;

	for (INT32 offs = 0; offs < 16 * 16; offs++)
	{
		INT32 sx = (offs & 0x0f) << 4;
		INT32 sy =  offs & 0xf0;

		INT32 attr    = DrvGfxROM4[bg_base + offs + 0x100];
		INT32 code    = DrvGfxROM4[bg_base + offs] | ((attr & 0x10) << 4);
		INT32 color   = attr & 0x0f;
		INT32 flipx   = attr & 0x40;

		sy -= 16;

		if (*flipscreen) {
			if (!flipx) {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM3);
			} else {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM3);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM3);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM3);
			}
		}
	}
}


static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 attr = DrvVidRAM[offs + 0x400];
		INT32 code  = ((attr & 0x30) << 4) | DrvVidRAM[offs];
		INT32 color = attr & 0x0f;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		sy -= 16;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 x = 0x400; x >= 0; x -= 4)
	{
		INT32 code  = DrvSprRAM[x+0];
		INT32 color = DrvSprRAM[x+1] & 0x0f;
		INT32 flipx = DrvSprRAM[x+1] & 0x40;
		INT32 flipy = 0;
		INT32 sy = 0xff - DrvSprRAM[x+2];
		INT32 sx = DrvSprRAM[x+3];

		if(*flipscreen)
		{
			if (DrvSprRAM[x+1] & 0x10 )
				sy = 0xff - sy + 32;
			else
				sy = 0xff - sy + 16;

			sx = 0xff - sx - 16;
			flipx = !flipx;
			flipy = !flipy;
		}
		sy -= 16;
		if(DrvSprRAM[x+1] & 0x10 )
		{
			code |= 0x40;
			sy -= 31;

			if (flipy) {
				if (flipx) {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				} else {
					Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				} else {
					Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				}
			}
		}
		else
		{
			sy -= 15;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM0);
				}
			}
		}
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc || UpdatePal) {
		for (INT32 i = 0; i < 0x100; i+=2) {
			INT32 p = (DrvPalRAM[i+0] << 0) | (DrvPalRAM[i+1] << 8);

			INT32 r = (p >> 0) & 0x0f;
			INT32 g = (p >> 4) & 0x0f;
			INT32 b = (p >> 8) & 0x0f;

			DrvPalette[i/2] = BurnHighCol((r << 4) | r, (g << 4) | g, (b << 4) | b, 0);
		}
		DrvRecalc = UpdatePal = 0;
	}

	if (*calorie_bg & 0x10)
	{
		draw_bg_layer();
		draw_fg_layer();
	}
	else
	{
		BurnTransferClear();
		draw_fg_layer();
	}

	draw_sprites();

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

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256;
	INT32 nTotalCycles[2] = { 4000000 / 60, 3000000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		ZetRun(nTotalCycles[0] / nInterleave);
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		ZetRun(nTotalCycles[1] / nInterleave);
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
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

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
	}

	return 0;
}


static void calorie_decode()
{
	static const UINT8 swaptable[24][4] =
	{
		{ 6,4,2,0 }, { 4,6,2,0 }, { 2,4,6,0 }, { 0,4,2,6 },
		{ 6,2,4,0 }, { 6,0,2,4 }, { 6,4,0,2 }, { 2,6,4,0 },
		{ 4,2,6,0 }, { 4,6,0,2 }, { 6,0,4,2 }, { 0,6,4,2 },
		{ 4,0,6,2 }, { 0,4,6,2 }, { 6,2,0,4 }, { 2,6,0,4 },
		{ 0,6,2,4 }, { 2,0,6,4 }, { 0,2,6,4 }, { 4,2,0,6 },
		{ 2,4,0,6 }, { 4,0,2,6 }, { 2,0,4,6 }, { 0,2,4,6 },
	};

	static const UINT8 data_xor[1+64] =
	{
		0x54,
		0x14,0x15,0x41,0x14,0x50,0x55,0x05,0x41,0x01,0x10,0x51,0x05,0x11,0x05,0x14,0x55,
		0x41,0x05,0x04,0x41,0x14,0x10,0x45,0x50,0x00,0x45,0x00,0x00,0x00,0x45,0x00,0x00,
		0x54,0x04,0x15,0x10,0x04,0x05,0x11,0x44,0x04,0x01,0x05,0x00,0x44,0x15,0x40,0x45,
		0x10,0x15,0x51,0x50,0x00,0x15,0x51,0x44,0x15,0x04,0x44,0x44,0x50,0x10,0x04,0x04,
	};

	static const UINT8 opcode_xor[2+64] =
	{
		0x04,
		0x44,
		0x15,0x51,0x41,0x10,0x15,0x54,0x04,0x51,0x05,0x55,0x05,0x54,0x45,0x04,0x10,0x01,
		0x51,0x55,0x45,0x55,0x45,0x04,0x55,0x40,0x11,0x15,0x01,0x40,0x01,0x11,0x45,0x44,
		0x40,0x05,0x15,0x15,0x01,0x50,0x00,0x44,0x04,0x50,0x51,0x45,0x50,0x54,0x41,0x40,
		0x14,0x40,0x50,0x45,0x10,0x05,0x50,0x01,0x40,0x01,0x50,0x50,0x50,0x44,0x40,0x10,
	};

	static const INT32 data_swap_select[1+64] =
	{
		 7,
		 1,11,23,17,23, 0,15,19,
		20,12,10, 0,18,18, 5,20,
		13, 0,18,14, 5, 6,10,21,
		 1,11, 9, 3,21, 4, 1,17,
		 5, 7,16,13,19,23,20, 2,
		10,23,23,15,10,12, 0,22,
		14, 6,15,11,17,15,21, 0,
		 6, 1, 1,18, 5,15,15,20,
	};

	static const INT32 opcode_swap_select[2+64] =
	{
		 7,
		12,
		18, 8,21, 0,22,21,13,21,
		20,13,20,14, 6, 3, 5,20,
		 8,20, 4, 8,17,22, 0, 0,
		 6,17,17, 9, 0,16,13,21,
		 3, 2,18, 6,11, 3, 3,18,
		18,19, 3, 0, 5, 0,11, 8,
		 8, 1, 7, 2,10, 8,10, 2,
		 1, 3,12,16, 0,17,10, 1,
	};

	for (INT32 A = 0x0000; A < 0x8000; A++)
	{
		INT32 row = BITSWAP16(A, 15, 13, 11, 10, 8, 7, 5, 4, 2, 1,  14, 12, 9, 6, 3, 0) & 0x3f;

		const UINT8 *tbl = swaptable[opcode_swap_select[row]];
		DrvDecROM0[A] = BITSWAP08(DrvZ80ROM0[A],7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ opcode_xor[row];

		tbl = swaptable[data_swap_select[row]];
		DrvZ80ROM0[A] = BITSWAP08(DrvZ80ROM0[A],7,tbl[0],5,tbl[1],3,tbl[2],1,tbl[3]) ^ data_xor[row];
	}
}

static INT32 CalorieInit()
{
	return DrvInit(calorie_decode);
}

static void calorieb_decode()
{
	memset (DrvZ80ROM0, 0x00, 0x10000);

	BurnLoadRom(DrvDecROM0 + 0x0000, 0, 1);
	memcpy (DrvZ80ROM0 + 0x0000, DrvDecROM0 + 0x4000, 0x4000);

	BurnLoadRom(DrvDecROM0 + 0x4000, 1, 1);
	memcpy (DrvZ80ROM0 + 0x4000, DrvDecROM0 + 0x8000, 0x4000);

	BurnLoadRom(DrvZ80ROM0 + 0x8000, 2, 1);

	memset (DrvDecROM0 + 0x8000, 0x00, 0x4000);
}

static INT32 CaloriebInit()
{
	return DrvInit(calorieb_decode);
}

// Calorie Kun vs Moguranian

static struct BurnRomInfo calorieRomDesc[] = {
	{ "epr10072.1j",	0x4000, 0xade792c1, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "epr10073.1k",	0x4000, 0xb53e109f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr10074.1m",	0x4000, 0xa08da685, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr10075.4d",	0x4000, 0xca547036, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "epr10079.8d",	0x2000, 0x3c61a42c, 3 | BRF_GRA },           //  4 user1

	{ "epr10071.7m",	0x4000, 0x5f55527a, 4 | BRF_GRA },           //  5 gfx1
	{ "epr10070.7k",	0x4000, 0x97f35a23, 4 | BRF_GRA },           //  6
	{ "epr10069.7j",	0x4000, 0xc0c3deaf, 4 | BRF_GRA },           //  7

	{ "epr10082.5r",	0x2000, 0x5984ea44, 5 | BRF_GRA },           //  8 gfx2
	{ "epr10081.4r",	0x2000, 0xe2d45dd8, 5 | BRF_GRA },           //  9
	{ "epr10080.3r",	0x2000, 0x42edfcfe, 5 | BRF_GRA },           // 10

	{ "epr10078.7d",	0x4000, 0x5b8eecce, 6 | BRF_GRA },           // 11 gfx3
	{ "epr10077.6d",	0x4000, 0x01bcb609, 6 | BRF_GRA },           // 12
	{ "epr10076.5d",	0x4000, 0xb1529782, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(calorie)
STD_ROM_FN(calorie)

struct BurnDriver BurnDrvCalorie = {
	"calorie", NULL, NULL, NULL, "1986",
	"Calorie Kun vs Moguranian\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, calorieRomInfo, calorieRomName, NULL, NULL, CalorieInputInfo, CalorieDIPInfo,
	CalorieInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	256, 224, 4, 3
};


// Calorie Kun vs Moguranian (bootleg)

static struct BurnRomInfo caloriebRomDesc[] = {
	{ "12.bin",		0x8000, 0xcf5fa69e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "13.bin",		0x8000, 0x52e7263f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr10074.1m",	0x4000, 0xa08da685, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr10075.4d",	0x4000, 0xca547036, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "epr10079.8d",	0x2000, 0x3c61a42c, 3 | BRF_GRA },           //  4 user1

	{ "epr10071.7m",	0x4000, 0x5f55527a, 4 | BRF_GRA },           //  5 gfx1
	{ "epr10070.7k",	0x4000, 0x97f35a23, 4 | BRF_GRA },           //  6
	{ "epr10069.7j",	0x4000, 0xc0c3deaf, 4 | BRF_GRA },           //  7

	{ "epr10082.5r",	0x2000, 0x5984ea44, 5 | BRF_GRA },           //  8 gfx2
	{ "epr10081.4r",	0x2000, 0xe2d45dd8, 5 | BRF_GRA },           //  9
	{ "epr10080.3r",	0x2000, 0x42edfcfe, 5 | BRF_GRA },           // 10

	{ "epr10078.7d",	0x4000, 0x5b8eecce, 6 | BRF_GRA },           // 11 gfx3
	{ "epr10077.6d",	0x4000, 0x01bcb609, 6 | BRF_GRA },           // 12
	{ "epr10076.5d",	0x4000, 0xb1529782, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(calorieb)
STD_ROM_FN(calorieb)

struct BurnDriver BurnDrvCalorieb = {
	"calorieb", "calorie", NULL, NULL, "1986",
	"Calorie Kun vs Moguranian (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, caloriebRomInfo, caloriebRomName, NULL, NULL, CalorieInputInfo, CalorieDIPInfo,
	CaloriebInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	256, 224, 4, 3
};

