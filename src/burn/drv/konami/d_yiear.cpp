// FB Alpha Yie Ar Kung-Fu driver module
// Based on MAME driver by Enrique Sanchez (?)

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "vlm5030.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvVLMROM;
static UINT8 *DrvColPROM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;
static UINT8 *AllRam;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *flipscreen;
static UINT8 *nmi_enable;
static UINT8 *irq_enable;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static UINT8 sn76496_latch;

static struct BurnInputInfo YiearInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Yiear)

static struct BurnDIPInfo YiearDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x5b, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x03, "1"			},
	{0x15, 0x01, 0x03, 0x02, "2"			},
	{0x15, 0x01, 0x03, 0x01, "3"			},
	{0x15, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x04, 0x00, "Upright"		},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x15, 0x01, 0x08, 0x08, "30000 80000"		},
	{0x15, 0x01, 0x08, 0x00, "40000 90000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x30, 0x30, "Easy"			},
	{0x15, 0x01, 0x30, 0x10, "Normal"		},
	{0x15, 0x01, 0x30, 0x20, "Difficult"		},
	{0x15, 0x01, 0x30, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x16, 0x01, 0x02, 0x02, "Single"		},
	{0x16, 0x01, 0x02, 0x00, "Dual"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x04, 0x04, "Off"			},
	{0x16, 0x01, 0x04, 0x00, "On"			},
};

STDDIPINFO(Yiear)

static void yiear_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			*flipscreen = data & 0x01;
			*nmi_enable = data & 0x02;
			*irq_enable = data & 0x04;
	//		coincounter = data & 0x18; (0 -> 8, 1 -> 10)
		return;

		case 0x4800:
			sn76496_latch = data;
		return;

		case 0x4900:
			SN76496Write(0, sn76496_latch);
		return;

		case 0x4a00:
			vlm5030_st(0, (data >> 1) & 1);
			vlm5030_rst(0, (data >> 2) & 1);
		return;

		case 0x4b00:
			vlm5030_data_write(0, data);
		return;
	}
}

static UINT8 yiear_read(UINT16 address)
{
	switch (address)
	{
		case 0x0000:
			return vlm5030_bsy(0) ? 1 : 0;

		case 0x4c00:
			return DrvDips[1];

		case 0x4d00:
			return DrvDips[2];

		case 0x4e00:
			return DrvInputs[0];

		case 0x4e01:
			return DrvInputs[1];

		case 0x4e02:
			return DrvInputs[2];

		case 0x4e03:
			return DrvDips[2];
	}

	return 0;
}

static UINT32 vlm_sync(INT32 samples_rate)
{
	return (samples_rate * M6809TotalCycles()) / 25600;
}

static INT32 DrvDoReset()
{
	memset (AllRam,   0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	vlm5030Reset(0);

	sn76496_latch = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvVLMROM		= Next; Next += 0x002000;

	DrvColPROM		= Next; Next += 0x000020;
;
	DrvPalette		= (UINT32*)Next; Next += 0x20 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; // 0x800
	DrvSprRAM0		= Next; Next += 0x000400;
	DrvSprRAM1		= Next; Next += 0x000400;

	DrvVidRAM		= Next; Next += 0x000800;

	flipscreen		= Next; Next += 0x000001;
	nmi_enable		= Next; Next += 0x000001;
	irq_enable		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[4] = { 4, 0, (0x2000*8)+4, (0x2000*8)+0 };
	INT32 Plane1[4] = { 4, 0, (0x8000*8)+4, (0x8000*8)+0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x200, 4,  8,  8, Plane0, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x200, 4, 16, 16, Plane1, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 32; i++)
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

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvInit()
{
//	BurnSetRefreshRate(60.58);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x8000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0xc000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000,  8, 1)) return 1;

		if (BurnLoadRom(DrvVLMROM   + 0x0000,  9, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,	0x5000, 0x57ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,	0x5800, 0x5fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(yiear_write);
	M6809SetReadHandler(yiear_read);
	M6809Close();

	SN76489AInit(0, 1536000, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	vlm5030Init(0, 3579545, vlm_sync, DrvVLMROM, 0x2000, 1);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	vlm5030Exit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	for (INT32 offs = 32 * 2; offs < 32 * 30; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = ((offs / 0x20) * 8) - 16;

		INT32 attr  = DrvVidRAM[offs*2+0];
		INT32 code  = DrvVidRAM[offs*2+1] | ((attr & 0x10) << 4);
		INT32 flipx = attr & 0x80;
		INT32 flipy = attr & 0x40;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY(pTransDraw, code, sx, sy, 0, 4, 0x10, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY(pTransDraw, code, sx, sy, 0, 4, 0x10, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX(pTransDraw, code, sx, sy, 0, 4, 0x10, DrvGfxROM0);
			} else {
				Render8x8Tile(pTransDraw, code, sx, sy, 0, 4, 0x10, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x30 - 2; offs >= 0; offs -= 2)
	{
		INT32 attr = DrvSprRAM0[offs + 0];
		INT32 code = DrvSprRAM1[offs + 1] + 256 * (attr & 0x01);
		INT32 flipx = ~attr & 0x40;
		INT32 flipy =  attr & 0x80;
		INT32 sy = 240 - DrvSprRAM0[offs + 1];
		INT32 sx = DrvSprRAM1 [offs + 0];

		if (0) // if (*flipscreen)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		if (offs < 0x26) sy++;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, 0, 4, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, 0, 4, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, 0, 4, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, 0, 4, 0, 0, DrvGfxROM1);
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

	draw_layer();
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
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	M6809NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 1536000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if (*nmi_enable && (i & 0x1f) == 0) // copy shao-lin's road
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // 480x/second (8x/frame)

		if (i == 240 && *irq_enable) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
		// vlm5030 won't interlace, so just run it at the end of the frame..
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);

		vlm5030Scan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(sn76496_latch);
	}

	return 0;
}


// Yie Ar Kung-Fu (program code I)

static struct BurnRomInfo yiearRomDesc[] = {
	{ "407_i08.10d",	0x4000, 0xe2d7458b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "407_i07.8d",		0x4000, 0x7db7442e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "407_c01.6h",		0x2000, 0xb68fd91d, 2 | BRF_GRA },           //  2 Background Tiles
	{ "407_c02.7h",		0x2000, 0xd9b167c6, 2 | BRF_GRA },           //  3

	{ "407_d05.16h",	0x4000, 0x45109b29, 3 | BRF_GRA },           //  4 Sprites
	{ "407_d06.17h",	0x4000, 0x1d650790, 3 | BRF_GRA },           //  5
	{ "407_d03.14h",	0x4000, 0xe6aa945b, 3 | BRF_GRA },           //  6
	{ "407_d04.15h",	0x4000, 0xcc187c22, 3 | BRF_GRA },           //  7

	{ "407c10.1g",		0x0020, 0xc283d71f, 4 | BRF_GRA },           //  8 Color PROM

	{ "407_c09.8b",		0x2000, 0xf75a1539, 5 | BRF_GRA },           //  9 VLM Samples
};

STD_ROM_PICK(yiear)
STD_ROM_FN(yiear)

struct BurnDriver BurnDrvYiear = {
	"yiear", NULL, NULL, NULL, "1985",
	"Yie Ar Kung-Fu (program code I)\0", NULL, "Konami", "GX407",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, yiearRomInfo, yiearRomName, NULL, NULL, NULL, NULL, YiearInputInfo, YiearDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};


// Yie Ar Kung-Fu (program code G)

static struct BurnRomInfo yiear2RomDesc[] = {
	{ "407_g08.10d",	0x4000, 0x49ecd9dd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "407_g07.8d",		0x4000, 0xbc2e1208, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "407_c01.6h",		0x2000, 0xb68fd91d, 2 | BRF_GRA },           //  2 Background Tiles
	{ "407_c02.7h",		0x2000, 0xd9b167c6, 2 | BRF_GRA },           //  3

	{ "407_d05.16h",	0x4000, 0x45109b29, 3 | BRF_GRA },           //  4 Sprites
	{ "407_d06.17h",	0x4000, 0x1d650790, 3 | BRF_GRA },           //  5
	{ "407_d03.14h",	0x4000, 0xe6aa945b, 3 | BRF_GRA },           //  6
	{ "407_d04.15h",	0x4000, 0xcc187c22, 3 | BRF_GRA },           //  7

	{ "407c10.1g",		0x0020, 0xc283d71f, 4 | BRF_GRA },           //  8 Color PROM

	{ "407_c09.8b",		0x2000, 0xf75a1539, 5 | BRF_GRA },           //  9 VLM Samples
};

STD_ROM_PICK(yiear2)
STD_ROM_FN(yiear2)

struct BurnDriver BurnDrvYiear2 = {
	"yiear2", "yiear", NULL, NULL, "1985",
	"Yie Ar Kung-Fu (program code G)\0", NULL, "Konami", "GX407",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, yiear2RomInfo, yiear2RomName, NULL, NULL, NULL, NULL, YiearInputInfo, YiearDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};
