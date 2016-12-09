// FB Alpha Bogey Manor Driver Module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 color_bank;
static UINT8 flipscreen;
static UINT8 ay8910_last;
static UINT8 ay8910_psg_latch;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo BogeymanInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bogeyman)

static struct BurnDIPInfo BogeymanDIPList[]=
{
	{0x12, 0xff, 0xff, 0xdf, NULL			},
	{0x13, 0xff, 0xff, 0x0f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	// flip screen not fully supported
	{0   , 0xfe, 0   ,    1, "Cabinet"		},
	{0x12, 0x01, 0x20, 0x00, "Upright"		},
//	{0x12, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x80, 0x00, "No"			},
	{0x12, 0x01, 0x80, 0x80, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x01, 0x01, "3"			},
	{0x13, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x02, 0x02, "50K"			},
	{0x13, 0x01, 0x02, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},
};

STDDIPINFO(Bogeyman)

static void bogeyman_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3800:
		{
			flipscreen = data & 0x01;

			if ((ay8910_last & 0x20) == 0x20 && (data & 0x20) == 0) {
				AY8910Write(0, (ay8910_last & 0x10) ? 0 : 1, ay8910_psg_latch);
			}

			if ((ay8910_last & 0x80) == 0x80 && (data & 0x80) == 0) {
				AY8910Write(1, (ay8910_last & 0x40) ? 0 : 1, ay8910_psg_latch);
			}

			ay8910_last = data;
		}
		return;

		case 0x3801:
			ay8910_psg_latch = data;
		return;

		case 0x3803:
		return;	//nop
	}
}

static UINT8 bogeyman_read(UINT16 address)
{
	switch (address)
	{
		case 0x3800:
			return DrvInputs[0];

		case 0x3801:
			return (DrvInputs[1] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x3802:
			return DrvDips[0];

		case 0x3803:
			return (DrvDips[1] & 0xf) | (DrvInputs[2] & 0xf0);
	}

	return 0;
}

static void color_bank_write(UINT32, UINT32 data)
{
	color_bank = data & 0x01;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	ay8910_last = 0;
	ay8910_psg_latch = 0;
	color_bank = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x00c000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0110 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x001800;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000400;

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

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0x8000*8+4, 0, 4 };
	INT32 Plane1[3]  = { 0x8000*8+0, 0x1000*8+0, 0x1000*8+4 };
	INT32 Plane2[3]  = { 0x8000*8, 0x4000*8, 0 };
	INT32 XOffs0[8]  = { STEP4(0x2000*8+3, -1), STEP4(3,-1) };
	INT32 YOffs0[16] = { STEP16(0,8) };
	INT32 XOffs2[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 YOffs2[16] = { STEP16(0,8) };
	INT32 XOffs3[16] = { STEP4(1024*8*8+3, -1), STEP4(3,-1), STEP4(1024*8*8+3+64, -1), STEP4(3+64,-1) };
	INT32 YOffs3[16] = { STEP8(0,8), STEP8(128, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0200, 3,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp + 0x0000, DrvGfxROM0 + 0x00000);
	GfxDecode(0x0200, 3,  8,  8, Plane1, XOffs0, YOffs0, 0x040, tmp + 0x0000, DrvGfxROM0 + 0x08000);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0200, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x100, tmp + 0x0000, DrvGfxROM1 + 0x00000);

	memcpy (tmp, DrvGfxROM2, 0x10000);

	GfxDecode(0x0080, 3, 16, 16, Plane0, XOffs3, YOffs3, 0x100, tmp + 0x0000, DrvGfxROM2 + 0x00000);
	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs3, YOffs3, 0x100, tmp + 0x0000, DrvGfxROM2 + 0x08000);
	GfxDecode(0x0080, 3, 16, 16, Plane0, XOffs3, YOffs3, 0x100, tmp + 0x4000, DrvGfxROM2 + 0x10000);
	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs3, YOffs3, 0x100, tmp + 0x4000, DrvGfxROM2 + 0x18000);

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
		if (BurnLoadRom(DrvM6502ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  4, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xa000, DrvGfxROM0 + 0x9000, 0x1000);
		memset (DrvGfxROM0 + 0x9000, 0, 0x1000);

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x04000,  9, 1)) return 1;

		UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
		if (BurnLoadRom(tmp,  10, 1)) return 1;

		memcpy (DrvGfxROM2 + 0x8000, tmp + 0x0000, 0x1000);
		memcpy (DrvGfxROM2 + 0xa000, tmp + 0x1000, 0x1000);
		memcpy (DrvGfxROM2 + 0xc000, tmp + 0x2000, 0x1000);
		memcpy (DrvGfxROM2 + 0xe000, tmp + 0x3000, 0x1000);

		BurnFree(tmp);

		if (BurnLoadRom(DrvColPROM  + 0x00000,  11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100,  12, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvFgRAM,		0x1800, 0x1fff, MAP_RAM);
	M6502MapMemory(DrvBgRAM,		0x2000, 0x21ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x2800, 0x2bff, MAP_RAM);
	M6502MapMemory(DrvPalRAM,		0x3000, 0x30ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,		0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(bogeyman_write);
	M6502SetReadHandler(bogeyman_read);
	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &color_bank_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (DrvColPROM[i+0] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i+0] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i+0] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i+0]   >> 3) & 0x01;
		bit1 = (DrvColPROM[i+256] >> 0) & 0x01;
		bit2 = (DrvColPROM[i+256] >> 1) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit1 = (DrvColPROM[i+256] >> 2) & 0x01;
		bit2 = (DrvColPROM[i+256] >> 3) & 0x01;
		INT32 b = 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i+0x10] = BurnHighCol(r,g,b,0);
	}
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x10; i++)
	{
		UINT8 r = (~DrvPalRAM[i] >> 0) & 7;
		UINT8 g = (~DrvPalRAM[i] >> 3) & 7;
		UINT8 b = (~DrvPalRAM[i] >> 6) & 3;

		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | b;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 16 * 16; offs++)
	{
		INT32 sx = (offs & 0x0f) * 16;
		INT32 sy = (offs / 0x10) * 16;

		INT32 attr = DrvBgRAM[offs+0x100];
		INT32 code = DrvBgRAM[offs] + ((attr & 1) << 8);

		INT32 color = ((attr >> 1) & 7);

		if (sx >= nScreenWidth || sy > nScreenHeight) continue;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0x90, DrvGfxROM2);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < (32 * 32); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr = DrvFgRAM[offs+0x400];
		INT32 code = DrvFgRAM[offs] + ((attr & 3) << 8);

		if (sx >= nScreenWidth || sy > nScreenHeight) continue;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color_bank, 3, 0, 16, DrvGfxROM0);
	}
}

static inline void draw_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0, DrvGfxROM1);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 3, 0, 0, DrvGfxROM1);
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs];

		if (attr & 0x01)
		{
			INT32 code = DrvSprRAM[offs + 1] + ((attr & 0x40) << 2);
			INT32 color = (attr & 0x08) >> 3;
			INT32 flipx = !(attr & 0x04);
			INT32 flipy = attr & 0x02;
			INT32 sx = DrvSprRAM[offs + 3];
			INT32 sy = (240 - DrvSprRAM[offs + 2]) & 0xff;
			INT32 multi = attr & 0x10;

			if (multi) sy -= 16;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			draw_sprite(code, color, sx, sy, flipx, flipy);

			if (multi)
			{
				draw_sprite(code+1, color, sx, sy + (flipscreen ? -16 : 16), flipx, flipy);
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

	DrvPaletteUpdate();

	BurnTransferClear();

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
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 272;
	INT32 nCyclesTotal = 1500000 / 60;
	INT32 nCyclesDone = 0;
	INT32 nSoundBufferPos = 0;

	vblank = 1;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += M6502Run(nCyclesTotal / nInterleave);

		if (i == 8) vblank = 0;
		if (i == 248) vblank = 1;
		if ((i & 0x0f) == 0xf) M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_AUTO);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

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

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(ay8910_last);
		SCAN_VAR(ay8910_psg_latch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(color_bank);
	}

	return 0;
}


// Bogey Manor

static struct BurnRomInfo bogeymanRomDesc[] = {
	{ "j20.c14",	0x4000, 0xea90d637, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "j10.c15",	0x4000, 0x0a8f218d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "j00.c17",	0x4000, 0x5d486de9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "j70.h15",	0x4000, 0xfdc787bf, 2 | BRF_GRA },           //  3 Foreground Tiles
	{ "j60.c17",	0x2000, 0xcc03ceb2, 2 | BRF_GRA },           //  4

	{ "j30.c9",	0x4000, 0x41af81c0, 3 | BRF_GRA },           //  5 Sprites
	{ "j40.c7",	0x4000, 0x8b438421, 3 | BRF_GRA },           //  6
	{ "j50.c5",	0x4000, 0xb507157f, 3 | BRF_GRA },           //  7

	{ "j90.h12",	0x4000, 0x46b2d4d0, 4 | BRF_GRA },           //  8 Background Tiles
	{ "j80.h13",	0x4000, 0x77ebd0a4, 4 | BRF_GRA },           //  9
	{ "ja0.h10",	0x4000, 0xf2aa05ed, 4 | BRF_GRA },           // 10

	{ "82s129.5k",	0x0100, 0x4a7c5367, 5 | BRF_GRA },           // 11 Color Data
	{ "82s129.6k",	0x0100, 0xb6127713, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(bogeyman)
STD_ROM_FN(bogeyman)

struct BurnDriver BurnDrvBogeyman = {
	"bogeyman", NULL, NULL, NULL, "1985",
	"Bogey Manor\0", "NULL", "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bogeymanRomInfo, bogeymanRomName, NULL, NULL, BogeymanInputInfo, BogeymanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 272,
	256, 240, 4, 3
};
