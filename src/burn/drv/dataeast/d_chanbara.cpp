// FB Alpha Chanbara driver module
// Based on MAME driver by Tomasz Slanina and David Haywood
// Todo:
// 1) Figure out the sprite banking issue (strange blob of broken
//    sprites appears sporatically - usually when the floor pattern changes.
// 2) Patch up CompileInput() to use DrvJoy[3][8] and variants thereof, then
//    update all the games in pre90s that use CompileInput() to use this style -dink

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 scrolly;
static UINT8 flipscreen;
static UINT8 bankdata;

static INT32 vblank;

static UINT8 DrvJoy[3][8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvJoy[0] + 4, "p1 coin"  },
	{"P1 Start"          , BIT_DIGITAL  , DrvJoy[0] + 0, "p1 start" },
	{"P1 Up (left)"      , BIT_DIGITAL  , DrvJoy[1] + 1, "p1 up"    },
	{"P1 Down (left)"    , BIT_DIGITAL  , DrvJoy[1] + 0, "p1 down"  },
	{"P1 Left (left)"    , BIT_DIGITAL  , DrvJoy[1] + 2, "p1 left"  },
	{"P1 Right (left)"   , BIT_DIGITAL  , DrvJoy[1] + 3, "p1 right" },
	{"P1 Up (right)"     , BIT_DIGITAL  , DrvJoy[1] + 5, "p3 up"    },
	{"P1 Down (right)"   , BIT_DIGITAL  , DrvJoy[1] + 4, "p3 down"  },
	{"P1 Left (right)"   , BIT_DIGITAL  , DrvJoy[1] + 6, "p3 left"  },
	{"P1 Right (right)"  , BIT_DIGITAL  , DrvJoy[1] + 7, "p3 right" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvJoy[0] + 5, "p2 coin"  },
	{"P2 Start"          , BIT_DIGITAL  , DrvJoy[0] + 1, "p2 start" },
	{"P2 Up (left)"      , BIT_DIGITAL  , DrvJoy[2] + 1, "p2 up"    },
	{"P2 Down (left)"    , BIT_DIGITAL  , DrvJoy[2] + 0, "p2 down"  },
	{"P2 Left (left)"    , BIT_DIGITAL  , DrvJoy[2] + 2, "p2 left"  },
	{"P2 Right (left)"   , BIT_DIGITAL  , DrvJoy[2] + 3, "p2 right" },
	{"P2 Up (right)"     , BIT_DIGITAL  , DrvJoy[2] + 5, "p4 up"    },
	{"P2 Down (right)"   , BIT_DIGITAL  , DrvJoy[2] + 4, "p4 down"  },
	{"P2 Left (right)"   , BIT_DIGITAL  , DrvJoy[2] + 6, "p4 left"  },
	{"P2 Right (right)"  , BIT_DIGITAL  , DrvJoy[2] + 7, "p4 right" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset    , "reset"    },
	{"Service"	         , BIT_DIGITAL  , DrvJoy[0] + 6, "Service"  },
	{"Dip"               , BIT_DIPSWITCH, DrvDips + 0  , "dip"      },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x16, 0xff, 0xff, 0x7f, NULL              },
	
	{0   , 0xfe, 0   , 4   , "Coin B"          },
	{0x16, 0x01, 0x03, 0x00, "2 Coins 1 Play"  },
	{0x16, 0x01, 0x03, 0x03, "1 Coin 1 Play"  },
	{0x16, 0x01, 0x03, 0x02, "1 Coin 2 Plays" },
	{0x16, 0x01, 0x03, 0x01, "1 Coin 3 Plays" },

	{0   , 0xfe, 0   , 4   , "Coin A"          },
	{0x16, 0x01, 0x0c, 0x00, "2 Coins 1 Play"  },
	{0x16, 0x01, 0x0c, 0x0c, "1 Coin 1 Play"  },
	{0x16, 0x01, 0x0c, 0x08, "1 Coin 2 Plays" },
	{0x16, 0x01, 0x0c, 0x04, "1 Coin 3 Plays" },

	{0   , 0xfe, 0   , 2   , "Difficulty"      },
	{0x16, 0x01, 0x10, 0x10, "Easy"            },
	{0x16, 0x01, 0x10, 0x00, "Hard"            },

	{0   , 0xfe, 0   , 2   , "Lives"           },
	{0x16, 0x01, 0x20, 0x20, "3"               },
	{0x16, 0x01, 0x20, 0x00, "1"               },

	{0   , 0xfe, 0   , 2   , "Bonus Life"      },
	{0x16, 0x01, 0x40, 0x40, "50k and 70k"     },
	{0x16, 0x01, 0x40, 0x00, "None"            },

	{0   , 0xfe, 0   , 1   , "Cabinet"         },
	{0x16, 0x01, 0x80, 0x80, "Upright"         },
//	{0x16, 0x01, 0x80, 0x00, "Cocktail"        }, // not supported atm
};

STDDIPINFO(Drv)

static void chanbara_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3800:
		case 0x3801:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 chanbara_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
			return DrvDips[0];

		case 0x2001:
			return (DrvInputs[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x2002:
			return DrvInputs[2];

		case 0x2003:
			return DrvInputs[1];

		case 0x3800:
		case 0x3801:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	bankdata = data;

	M6809MapMemory(DrvM6809ROM + (data * 0x1000), 0x4000, 0x7fff, MAP_ROM);
}

static void chanbara_ay_writeA(UINT32, UINT32 data)
{
	scrolly = (scrolly & 0x100) + data;
}

static void chanbara_ay_writeB(UINT32, UINT32 data)
{
	scrolly = ((data & 0x01) * 0x100) + (scrolly & 0xff);

	flipscreen = data & 0x02;

	bankswitch(data & 0x04);
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 1500000;
}

static double DrvGetTime()
{
	return (double)M6809TotalCycles() / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	BurnYM2203Reset(); // this calls bankswitch() through the ay ports(!)
	bankswitch(0);
	M6809Reset();
	M6809Close();

	scrolly = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x10000;

	DrvGfxROM0		= Next; Next += 0x08000;
	DrvGfxROM1		= Next; Next += 0x80000;
	DrvGfxROM2		= Next; Next += 0x10000;
	DrvGfxROM3		= Next; Next += 0x02000;

	DrvColPROM		= Next; Next += 0x00300;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x00800;
	DrvVidRAM0		= Next; Next += 0x00400;
	DrvVidRAM1		= Next; Next += 0x00200;
	DrvColRAM0		= Next; Next += 0x00400;
	DrvColRAM1		= Next; Next += 0x00200;
	DrvSprRAM		= Next; Next += 0x00100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 Plane1[3] = { (0x24000/3) * 8 * 2, (0x24000/3) * 8 * 1, (0x24000/3) * 8 * 0 };
	INT32 Plane2[3] = { (0x8000 / 2) * 8, 0, 4 };

	INT32 XOffs0[8] = { STEP4(((0x2000/2) * 8),1), STEP4(0,1) };
	INT32 XOffs1[16]= { STEP8(128,1), STEP8(0,1) };
	INT32 XOffs2[16]= { STEP4((((0x8000/4)*8) + 128), 1), STEP4(0,1), STEP4(((0x8000/4)*8),1), STEP4(128,1) };

	INT32 YOffs0[8] = { STEP8(0,8) };
	INT32 YOffs1[16]= { STEP8(0,8), STEP8(64,8) };
	INT32 YOffs2[16]= { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x24000);

	memcpy (tmp, DrvGfxROM0, 0x02000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x24000);

	GfxDecode(0x0600, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x0100, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM2);

	BurnFree(tmp);
}

UINT8 *temp4;

static void DrvGfxExpand()
{
	/*UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);

	memcpy (tmp, DrvGfxROM2 + 0x4000, 0x2000);

	for (INT32 i = 0; i < 0x4000; i++) {
		DrvGfxROM2[i + 0x4000] = (tmp[i/2] << ((~i&1)<<2)) & 0xf0;
	}

	BurnFree(tmp);
	*/
	// replacing the above with this fixes the "blue lines through everything" issue
	// Leaving this here incase iq_132 wants to fix it.

	UINT8 *dst = DrvGfxROM2 + 0x4000;
	UINT8 *src = DrvGfxROM3;

	for (INT32 i = 0; i < 0x1000; i++)
	{
		dst[i + 0x1000] = src[i] & 0xf0;
		dst[i + 0x0000] = (src[i] & 0x0f) << 4;
		dst[i + 0x3000] = src[i + 0x1000] & 0xf0;
		dst[i + 0x2000] = (src[i + 0x1000] & 0x0f) << 4;
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
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3  + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x08000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x14000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x10000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0c000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x1c000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x18000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00200, 17, 1)) return 1;

		DrvGfxExpand();
		DrvGfxDecode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM0,		0x0800, 0x0bff, MAP_RAM);
	M6809MapMemory(DrvColRAM0,		0x0c00, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x10ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1,		0x1800, 0x19ff, MAP_RAM);
	M6809MapMemory(DrvColRAM1,		0x1a00, 0x1bff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,		0x4000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(chanbara_write);
	M6809SetReadHandler(chanbara_read);
	M6809Close();

	BurnYM2203Init(1, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &chanbara_ay_writeA, &chanbara_ay_writeB);
	BurnTimerAttachM6809(1500000);
	BurnYM2203SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	M6809Exit();

	BurnYM2203Exit();
	
	GenericTilesExit();
	
	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = (DrvColPROM[i + 0x000] << 1) & 0x0e;
		INT32 g = (DrvColPROM[i + 0x100] << 1) & 0x0e;
		INT32 b = (DrvColPROM[i + 0x200] << 1) & 0x0e;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_bg_layer()
{
	INT32 yscroll = (scrolly + 16) & 0x1ff;

	for (INT32 offs = 0; offs < 16 * 32; offs++)
	{
		INT32 sx = (offs & 0x0f) * 16;
		INT32 sy = (offs / 0x10) * 16;

		sy -= yscroll;
		if (sy < -15) sy += 512;

		INT32 code  = DrvVidRAM1[offs];
		INT32 color =(DrvColRAM1[offs] & 0x3e) >> 1;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM2);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 2 * 32; offs < 32 * 30; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvColRAM0[offs];
		INT32 code  = DrvVidRAM0[offs] + ((attr & 0x01) * 0x100);
		INT32 color = ((attr & 0x3e) >> 1) + (0x40 >> 2);

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM0);
	}
}
extern int counter;

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x80; offs += 4)
	{
		if (DrvSprRAM[offs + 0x80] & 0x80)
		{
			INT32 attr0 = DrvSprRAM[offs + 0x00];
			INT32 attr1 = DrvSprRAM[offs + 0x80];
			INT32 code = DrvSprRAM[offs + 0x01] | ((attr1 & 0x10) << 5) |  ((attr1 & 0x20) << 5) | ((attr1 & 0x40) << 2);
			INT32 color = (attr1 & 0x0f) + (0x80>>3);
			INT32 flipy = attr0 & 0x2;
			INT32 sx = 240 - DrvSprRAM[offs + 0x03];
			INT32 sy = (232 - DrvSprRAM[offs + 0x02]);

			if (attr0 & 0x10)
			{
				if (flipy)
				{
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code+0, sx, sy     , color, 3, 0, 0, DrvGfxROM1);
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code+1, sx, sy - 16, color, 3, 0, 0, DrvGfxROM1);
				}
				else
				{
					Render16x16Tile_Mask_Clip(pTransDraw, code+0, sx, sy - 16, color, 3, 0, 0, DrvGfxROM1);
					Render16x16Tile_Mask_Clip(pTransDraw, code+1, sx, sy     , color, 3, 0, 0, DrvGfxROM1);
				}
			}
			else
			{
				if (flipy) {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
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

	BurnTransferClear();

	if (nBurnLayer & 2) draw_bg_layer();
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		UINT32 JoyInit[3] = { 0x7f, 0xff, 0xff };
		UINT8 *DrvJoys[3] = { DrvJoy[0], DrvJoy[1], DrvJoy[2] };

		CompileInput(DrvJoys, (void*)DrvInputs, 3, 8, JoyInit);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 1500000 / 60;

	M6809Open(0);
	M6809NewFrame();

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((i + 1) * (nCyclesTotal / nInterleave));

		if (i == 240) vblank = 1;
	}

	BurnTimerEndFrame(nCyclesTotal);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
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

		M6809Open(0);
		BurnYM2203Scan(nAction, pnMin);
		M6809Close();

		SCAN_VAR(scrolly);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bankdata);
	}

	return 0;
}


// Chanbara

static struct BurnRomInfo ChanbaraRomDesc[] = {
	{ "cp01.16c",     0x4000, 0xa0c3c24c, 0 | BRF_ESS | BRF_PRG }, //  0 M6809 Code
	{ "cp00-2.17c",   0x4000, 0xa045e463, 0 | BRF_ESS | BRF_PRG }, //  1
	{ "cp02.14c",     0x8000, 0xc2b66cea, 0 | BRF_ESS | BRF_PRG }, //  2

	{ "cp12.17h",     0x2000, 0xb87b96de, 1 | BRF_GRA },           //  3 Characters

	{ "cp13.15h",     0x4000, 0x2dc38c3d, 1 | BRF_GRA },           //  4 Background Tiles
	{ "cp14.13h",     0x2000, 0xd31db368, 1 | BRF_GRA },           //  5

	{ "cp03.12c",     0x4000, 0xdea247fb, 1 | BRF_GRA },           //  6 Sprites
	{ "cp04.10c",     0x4000, 0xf7dce87b, 1 | BRF_GRA },           //  7
	{ "cp05.9c",      0x4000, 0xdf2dc3cb, 1 | BRF_GRA },           //  8
	{ "cp06.7c",      0x4000, 0x2f337c08, 1 | BRF_GRA },           //  9
	{ "cp07.6c",      0x4000, 0x0e3727f2, 1 | BRF_GRA },           // 10
	{ "cp08.5c",      0x4000, 0x4cf35192, 1 | BRF_GRA },           // 11
	{ "cp09.4c",      0x4000, 0x3f58b647, 1 | BRF_GRA },           // 12
	{ "cp10.2c",      0x4000, 0xbfa324c0, 1 | BRF_GRA },           // 13
	{ "cp11.1c",      0x4000, 0x33e6160a, 1 | BRF_GRA },           // 14

	{ "cp17.4k",      0x0100, 0xcf03706e, 1 | BRF_GRA },           // 15 Color Proms
	{ "cp16.5k",      0x0100, 0x5fedc8ba, 1 | BRF_GRA },           // 16
	{ "cp15.6k",      0x0100, 0x655936eb, 1 | BRF_GRA },           // 17
};

STD_ROM_PICK(Chanbara)
STD_ROM_FN(Chanbara)

struct BurnDriver BurnDrvChanbara = {
	"chanbara", NULL, NULL, NULL, "1985",
	"Chanbara\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, ChanbaraRomInfo, ChanbaraRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
