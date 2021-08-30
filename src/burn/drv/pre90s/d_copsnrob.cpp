// FB Alpha Cops'n Robbers driver module
// Based on MAME driver by Zsolt Vasvari

// to do:
//	procure & hook-up samples

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "samples.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvTruckRAM;
static UINT8 *DrvBulletRAM;
static UINT8 *car_y;
static UINT8 *car_image;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 truck_y;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT16 Analog[4];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CopsnrobInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"Start 1",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"Start 2",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},

	A("P1 Gun",     	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 2"	},

	A("P2 Gun",     	BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"	},

	A("P3 Gun",     	BIT_ANALOG_REL, &Analog[2],		"p3 x-axis"),
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 fire 2"	},

	A("P4 Gun",     	BIT_ANALOG_REL, &Analog[3],		"p4 x-axis"),
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Copsnrob)

static struct BurnDIPInfo CopsnrobDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x03, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x03, 0x03, "1 Coin/1 Player"	},
	{0x00, 0x01, 0x03, 0x02, "1 Coin/2 Players"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin/Game"		},
	{0x00, 0x01, 0x03, 0x00, "2 Coins/1 Player"	},

	{0   , 0xfe, 0   ,    4, "Time Limit"		},
	{0x00, 0x01, 0x0c, 0x0c, "1min"				},
	{0x00, 0x01, 0x0c, 0x08, "1min 45sec"		},
	{0x00, 0x01, 0x0c, 0x04, "2min 20sec"		},
	{0x00, 0x01, 0x0c, 0x00, "3min"				},
};

STDDIPINFO(Copsnrob)

const INT32 trackball_divider = 0x3;

static UINT8 anlg(INT32 in)
{
	const UINT8 gunmap[7] = { 0x3f, 0x5f, 0x6f, 0x77, 0x7b, 0x7d, 0x7e };
	UINT8 an = BurnTrackballRead(in) / trackball_divider;

	return gunmap[an & 7] | ((~DrvJoy4[in] << 7) & 0x80);
}

static UINT8 copsnrob_read(UINT16 address)
{
	switch (address & 0x1fff)
	{
		case 0x1000:
			return vblank ? 0 : 0x80;

		case 0x1002:
			return anlg(0);

		case 0x1006:
			return anlg(1);

		case 0x100a:
			return anlg(2);

		case 0x100e:
			return anlg(3);

		case 0x1012:
			return (DrvDips[0] & 0xf) | (DrvInputs[2] & 0xf0);

		case 0x1016:
			return DrvInputs[0];

		case 0x101a:
			return DrvInputs[1];
	}

	return 0;
}

static void copsnrob_write(UINT16 address, UINT8 data)
{
	switch (address & 0x1fff)
	{
		case 0x0500:
		case 0x0501:
		case 0x0502:
		case 0x0503:
		case 0x0504:
		case 0x0505:
		case 0x0506:
		case 0x0507:
			// misc_w (sound)
			// samples?
		return;

		case 0x0600:
			truck_y = data;
		return;

		case 0x0900:
		case 0x0901:
		case 0x0902:
		case 0x0903:
			car_image[address & 3] = data;
		return;

		case 0x0a00:
		case 0x0a01:
		case 0x0a02:
		case 0x0a03:
			car_y[address & 3] = data;
		return;

		case 0x1000:
			// led = ~data & 0x40
		return;
	}
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs] & 0x3f, 0, 0);
}

static tilemap_scan( flipx )
{
	return (32 * row) + (31 - col);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

//	samples?

	truck_y = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x000e00;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM0	= Next; Next += 0x000200;
	DrvM6502RAM1	= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvTruckRAM		= Next; Next += 0x000100;
	DrvBulletRAM	= Next; Next += 0x000100;

	car_y			= Next; Next += 0x000004;
	car_image		= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1]   = { 0 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 XOffs1[32] = { STEP8(7,-1), STEP8(15,-1), STEP8(23,-1), STEP8(31,-1) };
	INT32 XOffs2[16] = { STEP4(768+4,1), STEP4(512+4,1), STEP4(256+4,1), STEP4(0+4,1) };
	INT32 YOffs0[32] = { STEP32(0,8) };
	INT32 YOffs1[32] = { STEP32(0,32) };
//	INT32 YOffs2 = YOffs0;

	UINT8 *tmp = (UINT8*)BurnMalloc(0x0800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x0200);

	GfxDecode(0x0040, 1,  8,  8, Plane, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x0800);

	GfxDecode(0x0010, 1, 32, 32, Plane, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x0100);

	GfxDecode(0x0002, 1, 16, 32, Plane, XOffs2, YOffs0, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0200,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0400,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0600,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0800,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0a00,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0c00,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0200,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0400, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0600, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x0100, 12, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x1fff);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvTruckRAM,			0x0700, 0x07ff, MAP_WRITE); // no read
	M6502MapMemory(DrvBulletRAM,		0x0800, 0x08ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM1,		0x0b00, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,			0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,			0x1200, 0x1fff, MAP_ROM);
	M6502SetWriteHandler(copsnrob_write);
	M6502SetReadHandler(copsnrob_read);
	M6502Close();

	// samples?

	GenericTilesInit();
	GenericTilemapInit(0, flipx_map_scan, background_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 1, 8, 8, 0x1000, 0, 0);

	BurnTrackballInit(2);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	// samples?

	BurnTrackballExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_cars()
{
	INT32 pos[4] = { 0xe4, 0xc4, 0x24, 0x04 };

	for (INT32 i = 0; i < 4; i++)
	{
		Draw32x32MaskTile(pTransDraw, car_image[i], pos[i], 256 - car_y[i], (i & 2) ? 0 : 1, 0, 0, 1, 0, 0, DrvGfxROM1);
	}
}

static void draw_truck()
{
	for (INT32 y = 0; y < 256; y++)
	{
		if (DrvTruckRAM[255 - y] == 0) continue;

		if ((truck_y & 0x1f) == ((y + 31) & 0x1f))
		{
			RenderCustomTile_Mask_Clip(pTransDraw, 16, 32, 0, 128, 256 - (y + 31), 0, 1, 0, 0, DrvGfxROM2);

			y += 31;
		}
		else if ((truck_y & 0x1f) == (y & 0x1f))
		{
			RenderCustomTile_Mask_Clip(pTransDraw, 16, 32, 0, 128, 256 - y, 0, 1, 0, 0, DrvGfxROM2);
		}
	}
}

static void draw_bullets()
{
	for (INT32 x = 0; x < 256; x++)
	{
		UINT8 val = DrvBulletRAM[x];
		if ((val & 0x0f) == 0) continue;

		for (INT32 bullet = 0; bullet < 4; bullet++)
		{
			if (val & (1 << bullet))
			{
				for (INT32 y = 0; y < nScreenHeight; y++)
				{
					if (DrvBulletRAM[y] & (0x10 << bullet))
					{
						pTransDraw[(y * nScreenWidth) + (256 - x)] = 1;
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff,0xff,0xff, 0);
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_cars();
	draw_truck();
	draw_bullets();

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

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballConfigStartStopPoints(0, 0, 7*trackball_divider, 0, 7*trackball_divider);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x00, 0x01);
		BurnTrackballUpdate(0);

		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballConfigStartStopPoints(1, 0, 7*trackball_divider, 0, 7*trackball_divider);
		BurnTrackballFrame(1, Analog[2], Analog[3], 0x00, 0x01);
		BurnTrackballUpdate(1);
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { (894886 + (((nCurrentFrame & 3) == 3) ? 1 : 0)) / 60 }; // 894886.25
	INT32 nCyclesDone[1] =  { 0 };

	vblank = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, M6502);
		if (i == 200) vblank = 1;
	}

	M6502Close();

	if (pBurnSoundOut) {
//		samples?
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

		// samples?

		BurnGunScan();

		SCAN_VAR(truck_y);
	}

	return 0;
}


// Cops'n Robbers

static struct BurnRomInfo copsnrobRomDesc[] = {
	{ "5777.l7",	0x0200, 0x2b62d627, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "5776.k7",	0x0200, 0x7fb12a49, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5775.j7",	0x0200, 0x627dee63, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5774.h7",	0x0200, 0xdfbcb7f2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5773.e7",	0x0200, 0xff7c95f4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5772.d7",	0x0200, 0x8d26afdc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "5771.b7",	0x0200, 0xd61758d6, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "5782.m3",	0x0200, 0x82b86852, 2 | BRF_GRA },           //  7 Characters

	{ "5778.p1",	0x0200, 0x78bff86a, 3 | BRF_GRA },           //  8 Car tiles
	{ "5779.m1",	0x0200, 0x8b1d0d83, 3 | BRF_GRA },           //  9
	{ "5780.l1",	0x0200, 0x6f4c6bab, 3 | BRF_GRA },           // 10
	{ "5781.j1",	0x0200, 0xc87f2f13, 3 | BRF_GRA },           // 11

	{ "5770.m2",	0x0100, 0xb00bbe77, 4 | BRF_GRA },           // 12 Truck tile

	{ "5765.h8",	0x0020, 0x6cd58931, 0 | BRF_OPT },           // 13 Timing? proms
	{ "5766.k8",	0x0020, 0xe63edf4f, 0 | BRF_OPT },           // 14
	{ "5767.j8",	0x0020, 0x381b5ae4, 0 | BRF_OPT },           // 15
	{ "5768.n4",	0x0100, 0xcb7fc836, 0 | BRF_OPT },           // 16
	{ "5769.d5",	0x0100, 0x75081a5a, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(copsnrob)
STD_ROM_FN(copsnrob)

struct BurnDriverD BurnDrvCopsnrob = {
	"copsnrob", NULL, NULL, NULL, "1976",
	"Cops'n Robbers\0", "No sound", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, copsnrobRomInfo, copsnrobRomName, NULL, NULL, NULL, NULL, CopsnrobInputInfo, CopsnrobDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 208, 4, 3
};
