// FB Alpha Acrobatic Dog-Fight driver module
// Based on MAME driver by Nicola Salmoria

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
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBMPRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvM6502RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 bankdata;
static UINT8 flipscreen;
static UINT8 pixelcolor;
static UINT8 soundlatch;
static UINT8 last_sound_control;
static UINT8 scroll[4];

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8]; // button 1
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DogfgtInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dogfgt)

static struct BurnDIPInfo DogfgtDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3b, NULL			},
	{0x14, 0xff, 0xff, 0x7c, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x01, 0x01, "3"			},
	{0x13, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x02, 0x02, "Normal"		},
	{0x13, 0x01, 0x02, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x08, 0x00, "No"			},
	{0x13, 0x01, 0x08, 0x08, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x10, 0x10, "Off"			},
	{0x13, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x13, 0x01, 0x20, 0x20, "Off"			},
	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    3, "Cabinet"		},
	{0x13, 0x01, 0xc0, 0x00, "Upright 1 Player"	},
	{0x13, 0x01, 0xc0, 0x80, "Upright 2 Players"	},
	{0x13, 0x01, 0xc0, 0xc0, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},
};

STDDIPINFO(Dogfgt)

static void bitmap_bankswitch(INT32 data)
{
	if (data > 2) return;

	bankdata = data;

	M6502MapMemory(DrvBMPRAM + (data * 0x2000), 0x2000, 0x3fff, MAP_RAM);
}

static void dogfgt_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1870) {
		DrvPalRAM[address & 0xf] = data;
		return;
	}

	switch (address)
	{
		case 0x1800:
			flipscreen = data & 0x80;
			pixelcolor = ((data << 1) & 2) | ((data & 2) >> 1);
			// coin counter data & 0x30
		return;

		case 0x1810:
			if (data & 0x04) {
				M6502Close();
				M6502Open(1);
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
				M6502Close();
				M6502Open(0);
			}
		return;

		case 0x1820:
		case 0x1821:
		case 0x1822:
		case 0x1823:
			scroll[address & 3] = data;
		return;

		case 0x1824:
			bitmap_bankswitch(data);
		return;

		case 0x1830:
			soundlatch = data;
		return;

		case 0x1840:
		{
			if ((last_sound_control & 0x20) == 0x20 && !(data & 0x20))
				AY8910Write(0, (~last_sound_control >> 4) & 1, soundlatch);

			if ((last_sound_control & 0x80) == 0x80 && !(data & 0x80))
				AY8910Write(1, (~last_sound_control >> 6) & 1, soundlatch);

			last_sound_control = data;
		}
	}
}

static UINT8 dogfgt_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1800:
			return DrvInputs[0];

		case 0x1810:
			return DrvInputs[1];

		case 0x1820:
			return DrvDips[0];

		case 0x1830:
			return (((DrvDips[1] & ~3) | (DrvInputs[2] & 3)) & 0x7f) | (vblank ? 0x80 : 0);
	}

	return 0;
}

static void dogfgt_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
	}
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], (DrvVidRAM[offs+0x400] & 3) + (0x10 >> 3), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	bitmap_bankswitch(0);
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	memset (scroll, 0, 4);

	last_sound_control = 0;
	flipscreen = 0;
	pixelcolor = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0	= Next; Next += 0x008000;
	DrvM6502ROM1	= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x030000;

	DrvColPROM		= Next; Next += 0x000040;

	DrvPalette		= (UINT32*)Next; Next += 0x0050 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x000010;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvBMPRAM		= Next; Next += 0x008000;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvM6502RAM1	= Next; Next += 0x000800;

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
	INT32 Plane[3]   = { RGN_FRAC(0x6000, 2, 3), RGN_FRAC(0x6000, 1, 3), RGN_FRAC(0x6000, 0, 3) };
	INT32 PlaneS[3]  = { RGN_FRAC(0x12000, 2, 3), RGN_FRAC(0x12000, 1, 3), RGN_FRAC(0x12000, 0, 3) };
	INT32 XOffs[16]  = { STEP8(128+7,-1), STEP8(7,-1) };
	INT32 YOffs[16]  = { STEP16(0,8) };
	INT32 XOffsS[16] = { STEP8(7,-1), STEP8(128+7,-1) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x12000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x0e000);

	GfxDecode(0x0100, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x12000);

	GfxDecode(0x0300, 3, 16, 16, PlaneS, XOffsS, YOffs, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvM6502ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x06000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x02000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x06000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x04000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x04000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x06000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x08000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x0a000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x0c000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x0e000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x10000, 19, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00020, 21, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvShareRAM,		0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x0f00, 0x0fff, MAP_RAM); // 80-df
	M6502MapMemory(DrvVidRAM,		0x1000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(dogfgt_main_write);
	M6502SetReadHandler(dogfgt_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,	0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,		0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(dogfgt_sub_write);
	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 16, 16, 0x10000, 0, 7); // offset and mask weird due to | bug in rendering system
	GenericTilemapSetOffsets(0, 0, -8);

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
	for (INT32 i = 0; i < 64; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i+0x10] = BurnHighCol(r,g,b,0);
	}
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x10; i++)
	{
		INT32 b = (DrvPalRAM[i] >> 6) & 3;
		INT32 g = (DrvPalRAM[i] >> 3) & 7;
		INT32 r = (DrvPalRAM[i] >> 0) & 7;

		r = (r << 5) + (r << 2) + (r >> 1);
		g = (g << 5) + (g << 2) + (g >> 1);
		b += (b << 6) + (b << 4) + (b << 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void copy_bitmap()
{
	INT32 color = (pixelcolor * 8) + 0x30;

	for (INT32 offs = 0; offs < 0x2000; offs++)
	{
		INT32 sy = offs & 0xff;
		if (sy < 8 || sy >= 248) continue;
		sy -= 8;
		INT32 sx = (offs / 0x100) * 8;

		UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;
		UINT8 src0 = DrvBMPRAM[0x0000 + offs];
		UINT8 src1 = DrvBMPRAM[0x2000 + offs];
		UINT8 src2 = DrvBMPRAM[0x4000 + offs];

		for (INT32 x = 0; x < 8; x++, src0>>=1, src1>>=1, src2>>=1)
		{
			INT32 pxl = ((src0 & 1) | ((src1 & 1) << 1) | ((src2 & 1) << 2));
			if (pxl) dst[x] = color + pxl;
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80; offs < 0xe0; offs += 4)
	{
		if (DrvSprRAM[offs] & 0x01)
		{
			INT32 attr = DrvSprRAM[offs];
			INT32 sx = DrvSprRAM[offs + 3];
			INT32 sy = (240 - DrvSprRAM[offs + 2]) & 0xff;
			INT32 code = DrvSprRAM[offs + 1] | ((attr & 0x30) << 4);
			INT32 color = ((attr & 0x08) >> 3);
			INT32 flipx = attr & 0x04;
			INT32 flipy = attr & 0x02;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

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
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	// update ram-based palette
	DrvPaletteUpdate(); 

	BurnTransferClear();

	GenericTilemapSetFlip(0, (flipscreen) ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollX(0, scroll[0] + (scroll[1] * 256) + 256);
	GenericTilemapSetScrollY(0, scroll[2] + (scroll[3] * 256));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) copy_bitmap();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 128;
	INT32 nCyclesTotal[2] = { 1500000 / 60, 1500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += M6502Run(nSegment);
		if ((i & 7) == 7) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO); // 16x per frame
		nSegment = M6502TotalCycles();
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(nSegment - M6502TotalCycles());
		M6502Close();

		if (i == 119) vblank = 1; // (240 / 256) * 128
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

		M6502Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(flipscreen);
		SCAN_VAR(pixelcolor);
		SCAN_VAR(soundlatch);
		SCAN_VAR(last_sound_control);
		SCAN_VAR(scroll);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bitmap_bankswitch(bankdata);
		M6502Close();
	}

	return 0;
}


// Acrobatic Dog-Fight

static struct BurnRomInfo dogfgtRomDesc[] = {
	{ "bx00.52",	0x2000, 0xe602a21c, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bx01.37",	0x2000, 0x4921c4fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bx02-5.36",	0x2000, 0xd11b50c3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bx03-5.22",	0x2000, 0x0e4813fb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bx04.117",	0x2000, 0xf8945f9d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code
	{ "bx05.118",	0x2000, 0x3ade57ad, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bx06.119",	0x2000, 0x4a3b34cf, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bx07.120",	0x2000, 0xae21f907, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "bx17.56",	0x2000, 0xfd3245d7, 3 | BRF_GRA },           //  8 Background Tiles
	{ "bx18.57",	0x2000, 0x03a5ef06, 3 | BRF_GRA },           //  9
	{ "bx19.58",	0x2000, 0xf62a16f4, 3 | BRF_GRA },           // 10

	{ "bx08.128",	0x2000, 0x8bf41b27, 4 | BRF_GRA },           // 11 Sprites
	{ "bx09.127",	0x2000, 0xc3ea6509, 4 | BRF_GRA },           // 12
	{ "bx10.126",	0x2000, 0x474a1c64, 4 | BRF_GRA },           // 13
	{ "bx11.125",	0x2000, 0xba67e382, 4 | BRF_GRA },           // 14
	{ "bx12.124",	0x2000, 0x102c0e1c, 4 | BRF_GRA },           // 15
	{ "bx13.123",	0x2000, 0xca47de34, 4 | BRF_GRA },           // 16
	{ "bx14.122",	0x2000, 0x51b95bb4, 4 | BRF_GRA },           // 17
	{ "bx15.121",	0x2000, 0xcf45d025, 4 | BRF_GRA },           // 18
	{ "bx16.120",	0x2000, 0xd1933837, 4 | BRF_GRA },           // 19

	{ "bx20.52",	0x0020, 0x4e475f05, 5 | BRF_GRA },           // 20 proms
	{ "bx21.64",	0x0020, 0x5de4319f, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(dogfgt)
STD_ROM_FN(dogfgt)

struct BurnDriver BurnDrvDogfgt = {
	"dogfgt", NULL, NULL, NULL, "1984",
	"Acrobatic Dog-Fight\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, dogfgtRomInfo, dogfgtRomName, NULL, NULL, DogfgtInputInfo, DogfgtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	256, 240, 4, 3
};


// Acrobatic Dog-Fight (USA)

static struct BurnRomInfo dogfgtuRomDesc[] = {
	{ "bx00.52",	0x2000, 0xe602a21c, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bx01-6.37",	0x2000, 0x8bb66399, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bx02-7.36",	0x2000, 0xafaf10e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bx03-6.33",	0x2000, 0x51b20e8b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bx04-7.117",	0x2000, 0xc4c2183b, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code
	{ "bx05-7.118",	0x2000, 0xd9a705ab, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bx06.119",	0x2000, 0x4a3b34cf, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bx07-7.120",	0x2000, 0x868df3dd, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "bx17.56",	0x2000, 0xfd3245d7, 3 | BRF_GRA },           //  8 Background Tiles
	{ "bx18.57",	0x2000, 0x03a5ef06, 3 | BRF_GRA },           //  9
	{ "bx19.58",	0x2000, 0xf62a16f4, 3 | BRF_GRA },           // 10

	{ "bx08.128",	0x2000, 0x8bf41b27, 4 | BRF_GRA },           // 11 Sprites
	{ "bx09.127",	0x2000, 0xc3ea6509, 4 | BRF_GRA },           // 12
	{ "bx10.126",	0x2000, 0x474a1c64, 4 | BRF_GRA },           // 13
	{ "bx11.125",	0x2000, 0xba67e382, 4 | BRF_GRA },           // 14
	{ "bx12.124",	0x2000, 0x102c0e1c, 4 | BRF_GRA },           // 15
	{ "bx13.123",	0x2000, 0xca47de34, 4 | BRF_GRA },           // 16
	{ "bx14.122",	0x2000, 0x51b95bb4, 4 | BRF_GRA },           // 17
	{ "bx15.121",	0x2000, 0xcf45d025, 4 | BRF_GRA },           // 18
	{ "bx16.120",	0x2000, 0xd1933837, 4 | BRF_GRA },           // 19

	{ "bx20.52",	0x0020, 0x4e475f05, 5 | BRF_GRA },           // 20 Color data
	{ "bx21.64",	0x0020, 0x5de4319f, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(dogfgtu)
STD_ROM_FN(dogfgtu)

struct BurnDriver BurnDrvDogfgtu = {
	"dogfgtu", "dogfgt", NULL, NULL, "1985",
	"Acrobatic Dog-Fight (USA)\0", NULL, "Technos Japan (Data East USA, Inc. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, dogfgtuRomInfo, dogfgtuRomName, NULL, NULL, DogfgtInputInfo, DogfgtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	256, 240, 4, 3
};


// Dog-Fight (Japan)

static struct BurnRomInfo dogfgtjRomDesc[] = {
	{ "bx00.52",	0x2000, 0xe602a21c, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bx01.37",	0x2000, 0x4921c4fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bx02.36",	0x2000, 0x91f1b9b3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bx03.22",	0x2000, 0x959ebf93, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bx04.117",	0x2000, 0xf8945f9d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code
	{ "bx05.118",	0x2000, 0x3ade57ad, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bx06.119",	0x2000, 0x4a3b34cf, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bx07.120",	0x2000, 0xae21f907, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "bx17.56",	0x2000, 0xfd3245d7, 3 | BRF_GRA },           //  8 Background Tiles
	{ "bx18.57",	0x2000, 0x03a5ef06, 3 | BRF_GRA },           //  9
	{ "bx19.58",	0x2000, 0xf62a16f4, 3 | BRF_GRA },           // 10

	{ "bx08.128",	0x2000, 0x8bf41b27, 4 | BRF_GRA },           // 11 Sprites
	{ "bx09.127",	0x2000, 0xc3ea6509, 4 | BRF_GRA },           // 12
	{ "bx10.126",	0x2000, 0x474a1c64, 4 | BRF_GRA },           // 13
	{ "bx11.125",	0x2000, 0xba67e382, 4 | BRF_GRA },           // 14
	{ "bx12.124",	0x2000, 0x102c0e1c, 4 | BRF_GRA },           // 15
	{ "bx13.123",	0x2000, 0xca47de34, 4 | BRF_GRA },           // 16
	{ "bx14.122",	0x2000, 0x51b95bb4, 4 | BRF_GRA },           // 17
	{ "bx15.121",	0x2000, 0xcf45d025, 4 | BRF_GRA },           // 18
	{ "bx16.120",	0x2000, 0xd1933837, 4 | BRF_GRA },           // 19

	{ "bx20.52",	0x0020, 0x4e475f05, 5 | BRF_GRA },           // 20 Color data
	{ "bx21.64",	0x0020, 0x5de4319f, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(dogfgtj)
STD_ROM_FN(dogfgtj)

struct BurnDriver BurnDrvDogfgtj = {
	"dogfgtj", "dogfgt", NULL, NULL, "1984",
	"Dog-Fight (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, dogfgtjRomInfo, dogfgtjRomName, NULL, NULL, DogfgtInputInfo, DogfgtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	256, 240, 4, 3
};
