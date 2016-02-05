// FB Alpha Battle Lane driver module
// Based on MAME driver by Paul Leaman (paul@vortexcomputing.demon.co.uk)

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym3526.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvShareRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBmpRAM;
static UINT8 *bgbitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvInputs[3];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static UINT16 scrollx;
static UINT8 scrollxhi;
static UINT16 scrolly;
static UINT8 scrollyhi;
static UINT8 flipscreen;
static UINT8 cpu_ctrl;
static UINT8 video_ctrl;
static UINT8 vblank;

static struct BurnInputInfo DrvInputList[] = {
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
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xdf, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x20, 0x00, "Upright"		},
	{0x12, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0xc0, 0xc0, "Easy"			},
	{0x12, 0x01, 0xc0, 0x80, "Normal"		},
	{0x12, 0x01, 0xc0, 0x40, "Hard"			},
	{0x12, 0x01, 0xc0, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "Free Play"		},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "4"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x0c, "20k and 50k"		},
	{0x13, 0x01, 0x0c, 0x08, "20k and 70k"		},
	{0x13, 0x01, 0x0c, 0x04, "20k and 90k"		},
	{0x13, 0x01, 0x0c, 0x00, "None"			},
};

STDDIPINFO(Drv)

static void bitmap_w(UINT16 offset, UINT8 data)
{
	INT32 i, orval;

	orval = (~video_ctrl >> 1) & 0x07;

	if (!orval)
		orval = 7;

	for (i = 0; i < 8; i++)
	{
		INT32 y = offset % 0x100;
		INT32 x = (offset / 0x100) * 8 + i;

		if (data & 1 << i)
		{
			bgbitmap[(y * 256) + x] |= orval;
		}
		else
		{
			bgbitmap[(y * 256) + x] &= ~orval;
		}
	}
}


static void battlane_write(UINT16 address, UINT8 data)
{
	if (address >= 0x2000 && address <= 0x3fff) {
		bitmap_w(address - 0x2000, data);
		return;
	}

	switch (address)
	{
		case 0x1c00:
			video_ctrl = data;
			scrollxhi = data & 0x01;
		return;

		case 0x1c01:
			scrollx = data | (scrollxhi << 8);
		return;

		case 0x1c02:
			scrolly = data | (scrollyhi << 8);
		return;

		case 0x1c03:
		{
			cpu_ctrl = data;
			scrollyhi = data & 0x01;
			flipscreen = data & 0x80;

			UINT8 activecpu = M6809GetActive();

			M6809Close();
			M6809Open(0);
			M6809SetIRQLine(0, (data & 0x04) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			M6809Close();

			M6809Open(1);
			M6809SetIRQLine(0, (data & 0x02) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(activecpu);
		}
		return;

		case 0x1c04:
		case 0x1c05:
			BurnYM3526Write(address & 1, data);
		return;
	}
}

static UINT8 battlane_read(UINT16 address)
{
	switch (address)
	{
		case 0x1c00:
			return DrvInputs[0];

		case 0x1c01:
			return (DrvInputs[1] & 0x7f) | ((vblank) ? 0x80 : 0);

		case 0x1c02:
			return DrvDips[0];

		case 0x1c03:
			return (DrvDips[1] & 0xf) | (DrvInputs[2] & 0xf0);

		case 0x1c04:
		case 0x1c05:
			return BurnYM3526Read(address & 1);
	}

	return 0;
}

inline static void DrvYM3526IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(1/*FIRQ*/, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);	
}

inline static INT32 DrvYM3526SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	BurnYM3526Reset();
	M6809Close();

	scrollx = 0;
	scrollxhi = 0;
	scrolly = 0;
	scrollyhi = 0;
	flipscreen = 0;
	cpu_ctrl = 0;
	video_ctrl = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x010000;
	DrvM6809ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x001000;
	DrvTileRAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvBmpRAM		= Next; Next += 0x002000;

	bgbitmap        = (UINT8*)Next; Next += 256 * 256 * sizeof(UINT8); // background bitmap (RAM section)

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes0[3] = { 0x8000 * 8 * 0 + 0, 0x8000 * 8 * 1 + 0, 0x8000 * 8 * 2 + 0 };
	INT32 Planes1[3] = { 0x4000 * 8 * 2 + 4, 0x4000 * 8 * 0 + 4, 0x4000 * 8 * 0 + 0 };
	INT32 Planes2[3] = { 0x4000 * 8 * 2 + 0, 0x4000 * 8 * 1 + 4, 0x4000 * 8 * 1 + 0 };
	INT32 XOffs0[16] = { STEP8(7,-1), STEP8(128+7,-1) };
	INT32 YOffs0[16] = { STEP16(128 - 8, -8) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(8+3,-1), STEP4(16+3,-1), STEP4(24+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Planes0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x0c000);

	GfxDecode(0x0100, 3, 16, 16, Planes1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1 + 0x00000);
	GfxDecode(0x0100, 3, 16, 16, Planes2, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1 + 0x10000);

	BurnFree(tmp);

	return 0;
}

static void DrvCpuMap(INT32 cpu)
{
	UINT8 *rom = (cpu) ? DrvM6809ROM1 : DrvM6809ROM0;

	M6809Open(cpu);
	M6809MapMemory(DrvShareRAM,	0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvTileRAM,	0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,	0x1800, 0x18ff, MAP_RAM);
	M6809MapMemory(DrvPalRAM,	0x1e00, 0x1e3f|0xff, MAP_RAM);
	//M6809MapMemory(DrvBmpRAM,	0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(rom + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(battlane_write);
	M6809SetReadHandler(battlane_read);
	M6809Close();
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
		if (BurnLoadRom(DrvM6809ROM1 + 0x00000, 0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x08000, 1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x08000, 2, 1)) return 1;
		memcpy (DrvM6809ROM0 + 0x4000, DrvM6809ROM1 + 0x0000, 0x4000);

		if (BurnLoadRom(DrvGfxROM0   + 0x00000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x08000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x10000, 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000, 6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x08000, 7, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(2);
	DrvCpuMap(0);
	DrvCpuMap(1);

	BurnYM3526Init(3000000, DrvYM3526IRQHandler, &DrvYM3526SynchroniseStream, 0);
	BurnTimerAttachM6809YM3526(1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	BurnYM3526Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate(INT32 offset)
{
	INT32 d = DrvPalRAM[offset];

	INT32 b0 = (~d >> 0) & 0x01;
	INT32 b1 = (~d >> 1) & 0x01;
	INT32 b2 = (~d >> 2) & 0x01;
	INT32 r = 0x21 * b0 + 0x47 * b1 + 0x97 * b2;

	b0 = (~d >> 3) & 0x01;
	b1 = (~d >> 4) & 0x01;
	b2 = (~d >> 5) & 0x01;
	INT32 g = 0x21 * b0 + 0x47 * b1 + 0x97 * b2;

	b0 = (~video_ctrl >> 7) & 0x01;
	b1 = (~d >> 6) & 0x01;
	b2 = (~d >> 7) & 0x01;
	INT32 b = 0x21 * b0 + 0x47 * b1 + 0x97 * b2;

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void draw_bmp_layer()
{
	INT32 x, y, data;

	for (y = 0; y < 32 * 8; y++)
	{
		for (x = 0; x < 32 * 8; x++)
		{
			data = bgbitmap[(y * 256) + x];

			if (data)
			{
				INT32 sx = x;
				INT32 sy = y;
				if (flipscreen) {
					sy = 255 - y;
					sx = 255 - x;
				}

				UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;
				*dst = data;
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs + 1];
		//INT32 code = DrvSprRAM[offs + 3] + ((attr & 0x60) << 3);
		INT32 code = DrvSprRAM[offs + 3];
		code += 256 * ((attr >> 6) & 0x02);
		code += 256 * ((attr >> 5) & 0x01);


		if (attr & 0x01)
		{
			INT32 color = (attr >> 3) & 0x01;

			INT32 sx = DrvSprRAM[offs + 2] + 8;
			INT32 sy = DrvSprRAM[offs];

			INT32 flipx = attr & 0x04;
			INT32 flipy = attr & 0x02;

			if (!flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

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

			if (attr & 0x10)
			{
				INT32 dy = flipy ? 16 : -16;

				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code+1, sx, sy+dy, color, 3, 0, 0, DrvGfxROM0);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code+1, sx, sy+dy, color, 3, 0, 0, DrvGfxROM0);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code+1, sx, sy+dy, color, 3, 0, 0, DrvGfxROM0);
						
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code+1, sx, sy+dy, color, 3, 0, 0, DrvGfxROM0);
					}
				}
			}
		}
	}
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 16;

		sx -= (scrollx + 8) & 0x1ff;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 ofst = ((offs & 0x1e0)>>1) + (offs & 0x20f) + ((offs & 0x10)<<4);

		INT32 attr  = DrvTileRAM[ofst + 0x400];
		INT32 code  = DrvTileRAM[ofst + 0x000] + ((attr & 0x01) << 8);
		INT32 color = (attr >> 1) & 3;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0x20, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	for (INT32 i = 0; i < 0x40; i++) {
		DrvPaletteUpdate(i);
	}
	BurnTransferClear();

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_bmp_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1500000 / 60, 1500000 / 60 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[0] / nInterleave));
		if (i == 240 && (~cpu_ctrl & 0x08)) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		M6809Close();
	
		M6809Open(1);
		M6809Run(nCyclesTotal[1] / nInterleave);
		if (i == 240 && (~cpu_ctrl & 0x08)) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		M6809Close();

		if (i == 240) vblank = 1;
	}

	M6809Open(0);

	BurnTimerEndFrameYM3526(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

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

		M6809Scan(nAction);
		BurnYM3526Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrollxhi);
		SCAN_VAR(scrolly);
		SCAN_VAR(scrollyhi);
		SCAN_VAR(flipscreen);
		SCAN_VAR(cpu_ctrl);
		SCAN_VAR(video_ctrl);
	}

	return 0;
}


// Battle Lane! Vol. 5 (set 1)

static struct BurnRomInfo battlaneRomDesc[] = {
	{ "da00-5",		0x8000, 0x85b4ed73, 0 | BRF_PRG | BRF_ESS }, //  0 M6809 Shared Code

	{ "da01-5",		0x8000, 0x7a6c3f02, 1 | BRF_PRG | BRF_ESS }, //  1 M6809 0 Code

	{ "da02-2",		0x8000, 0x69d8dafe, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 1 Code

	{ "da05",		0x8000, 0x834829d4, 3 | BRF_GRA },           //  3 Sprite Tiles
	{ "da04",		0x8000, 0xf083fd4c, 3 | BRF_GRA },           //  4
	{ "da03",		0x8000, 0xcf187f25, 3 | BRF_GRA },           //  5

	{ "da06",		0x8000, 0x9c6a51b3, 4 | BRF_GRA },           //  6 Background Tiles
	{ "da07",		0x4000, 0x56df4077, 4 | BRF_GRA },           //  7

	{ "82s123.7h",		0x0020, 0xb9933663, 5 | BRF_OPT },           //  8 PROMs (unused)
	{ "82s123.9n",		0x0020, 0x06491e53, 5 | BRF_OPT },           //  9
};

STD_ROM_PICK(battlane)
STD_ROM_FN(battlane)

struct BurnDriver BurnDrvBattlane = {
	"battlane", NULL, NULL, NULL, "1986",
	"Battle Lane! Vol. 5 (set 1)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, battlaneRomInfo, battlaneRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 256, 3, 4
};


// Battle Lane! Vol. 5 (set 2)

static struct BurnRomInfo battlane2RomDesc[] = {
	{ "da00-3",		0x8000, 0x7a0a5d58, 0 | BRF_PRG | BRF_ESS }, //  0 M6809 Shared Code

	{ "da01-3",		0x8000, 0xd9e40800, 1 | BRF_PRG | BRF_ESS }, //  1 M6809 0 Code

	{ "da02-2",		0x8000, 0x69d8dafe, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 1 Code

	{ "da05",		0x8000, 0x834829d4, 3 | BRF_GRA },           //  3 Sprite Tiles
	{ "da04",		0x8000, 0xf083fd4c, 3 | BRF_GRA },           //  4
	{ "da03",		0x8000, 0xcf187f25, 3 | BRF_GRA },           //  5

	{ "da06",		0x8000, 0x9c6a51b3, 4 | BRF_GRA },           //  6 Background Tiles
	{ "da07",		0x4000, 0x56df4077, 4 | BRF_GRA },           //  7

	{ "82s123.7h",		0x0020, 0xb9933663, 5 | BRF_OPT },           //  8 PROMs (unused)
	{ "82s123.9n",		0x0020, 0x06491e53, 5 | BRF_OPT },           //  9
};

STD_ROM_PICK(battlane2)
STD_ROM_FN(battlane2)

struct BurnDriver BurnDrvBattlane2 = {
	"battlane2", "battlane", NULL, NULL, "1986",
	"Battle Lane! Vol. 5 (set 2)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, battlane2RomInfo, battlane2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 256, 3, 4
};


// Battle Lane! Vol. 5 (set 3)

static struct BurnRomInfo battlane3RomDesc[] = {
	{ "bl_04.rom",		0x8000, 0x5681564c, 0 | BRF_PRG | BRF_ESS }, //  0 M6809 Shared Code

	{ "bl_05.rom",		0x8000, 0x001c4bbe, 1 | BRF_PRG | BRF_ESS }, //  1 M6809 0 Code

	{ "da02-2",		0x8000, 0x69d8dafe, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 1 Code

	{ "da05",		0x8000, 0x834829d4, 3 | BRF_GRA },           //  3 Sprite Tiles
	{ "da04",		0x8000, 0xf083fd4c, 3 | BRF_GRA },           //  4
	{ "da03",		0x8000, 0xcf187f25, 3 | BRF_GRA },           //  5

	{ "da06",		0x8000, 0x9c6a51b3, 4 | BRF_GRA },           //  6 Background Tiles
	{ "da07",		0x4000, 0x56df4077, 4 | BRF_GRA },           //  7

	{ "82s123.7h",		0x0020, 0xb9933663, 5 | BRF_OPT },           //  8 PROMs (unused)
	{ "82s123.9n",		0x0020, 0x06491e53, 5 | BRF_OPT },           //  9
};

STD_ROM_PICK(battlane3)
STD_ROM_FN(battlane3)

struct BurnDriver BurnDrvBattlane3 = {
	"battlane3", "battlane", NULL, NULL, "1986",
	"Battle Lane! Vol. 5 (set 3)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, battlane3RomInfo, battlane3RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 256, 3, 4
};
