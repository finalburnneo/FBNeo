// FB Alpha Pooyan Driver Module
// Based on MAME driver by Allard van der Bas, Mike Cuddy, Nicola Salmoria, Martin Binder, and Marco Cassili

#include "tiles_generic.h"
#include "z80_intf.h"
#include "timeplt_snd.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 irqtrigger;
static UINT8 irq_enable;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nCyclesExtra[2];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL			},
	{0x0d, 0xff, 0xff, 0x7b, NULL			},

	{0   , 0xfe, 0   , 16  , "Coin A"		},
	{0x0c, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"	},
	{0x0c, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"	},
	{0x0c, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"  	},
	{0x0c, 0x01, 0x0f, 0x04, "3 Coins 2 Credits" 	},
	{0x0c, 0x01, 0x0f, 0x01, "4 Coins 3 Credits" 	},
	{0x0c, 0x01, 0x0f, 0x0f, "1 Coin 1 Credit"   	},
	{0x0c, 0x01, 0x0f, 0x03, "3 Coins 4 Credits" 	},
	{0x0c, 0x01, 0x0f, 0x07, "2 Coins 3 Credits" 	},
	{0x0c, 0x01, 0x0f, 0x0e, "1 Coin 2 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x06, "2 Coins 5 Credits" 	},
	{0x0c, 0x01, 0x0f, 0x0d, "1 Coin 3 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x0c, "1 Coin 4 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x0b, "1 Coin 5 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x0a, "1 Coin 6 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x09, "1 Coin 7 Credits"  	},
	{0x0c, 0x01, 0x0f, 0x00, "Free Play"   	     	},

	{0   , 0xfe, 0   , 15  , "Coin B" },
	{0x0c, 0x82, 0xf0, 0x20, "4 Coins 1 Credit"	},
	{0x0c, 0x82, 0xf0, 0x50, "3 Coins 1 Credit"	},
	{0x0c, 0x82, 0xf0, 0x80, "2 Coins 1 Credit"	},
	{0x0c, 0x82, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x0c, 0x82, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x0c, 0x82, 0xf0, 0xf0, "1 Coin 1 Credit"	},
	{0x0c, 0x82, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x0c, 0x82, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x0c, 0x82, 0xf0, 0xe0, "1 Coin 2 Credits"	},
	{0x0c, 0x82, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x0c, 0x82, 0xf0, 0xd0, "1 Coin 3 Credits" 	},
	{0x0c, 0x82, 0xf0, 0xc0, "1 Coin 4 Credits"	},
	{0x0c, 0x82, 0xf0, 0xb0, "1 Coin 5 Credits"	},
	{0x0c, 0x82, 0xf0, 0xa0, "1 Coin 6 Credits"	},
	{0x0c, 0x82, 0xf0, 0x90, "1 Coin 7 Credits"	},

	{0   , 0xfe, 0   , 4   , "Lives"		},
	{0x0d, 0x01, 0x03, 0x03, "3"			},
	{0x0d, 0x01, 0x03, 0x02, "4"			},
	{0x0d, 0x01, 0x03, 0x01, "5"			},
	{0x0d, 0x01, 0x03, 0x00, "256"			},

	{0   , 0xfe, 0   , 1   , "Cabinet"		},
	{0x0d, 0x01, 0x04, 0x00, "Upright" 		},
//	{0x0d, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   , 2   , "Bonus Life"		},
	{0x0d, 0x01, 0x08, 0x08, "50K 80K+"		},
	{0x0d, 0x01, 0x08, 0x00, "30K 70K+"		},

	{0   , 0xfe, 0   , 8   , "Difficulty"		},
	{0x0d, 0x01, 0x70, 0x70, "1 (Easy)"		},
	{0x0d, 0x01, 0x70, 0x60, "2"    		},
	{0x0d, 0x01, 0x70, 0x50, "3"    		},
	{0x0d, 0x01, 0x70, 0x40, "4"    		},
	{0x0d, 0x01, 0x70, 0x30, "5"    		},
	{0x0d, 0x01, 0x70, 0x20, "6"    		},
	{0x0d, 0x01, 0x70, 0x10, "7"    		},
	{0x0d, 0x01, 0x70, 0x00, "8 (Hard)"		},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"		},
	{0x0d, 0x01, 0x80, 0x80, "Off"			},
	{0x0d, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Drv)

static UINT8 __fastcall pooyan_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvDips[1];

		case 0xa080:
			return DrvInputs[0];

		case 0xa0a0:
			return DrvInputs[1];

		case 0xa0c0:
			return DrvInputs[2];

		case 0xa0e0:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall pooyan_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000: 
			// watchdog
		break;

		case 0xa100:
			TimepltSndSoundlatch(data);
		break;

		case 0xa180:
			irq_enable = data & 1;
			if (!irq_enable)
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		break;

		case 0xa181:
		{
			if (irqtrigger == 0 && data) {
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}

			irqtrigger = data;
		}
		break;

		case 0xa183:
		break;

		case 0xa187:
			flipscreen = ~data & 1;
		break;
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	TimepltSndReset();

	irqtrigger = 0;
	flipscreen = 0;
	irq_enable = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	HiscoreReset();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x20];

	static const INT32 resistances_rg[3] = { 1000, 470, 220 };
	static const INT32 resistances_b[2]  = { 470, 220 };

	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 0xff, -1.0,
		3, &resistances_rg[0], rweights, 1000, 0,
		3, &resistances_rg[0], gweights, 1000, 0,
		2, &resistances_b[0],  bweights, 1000, 0);

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 r, g, b;
		INT32 bit0, bit1, bit2;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = combine_2_weights(bweights, bit0, bit1);

		pal[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i + 0x000] = pal[(DrvColPROM[i + 0x020] & 0x0f) | 0x10];
		DrvPalette[i + 0x100] = pal[(DrvColPROM[i + 0x120] & 0x0f) | 0x00];
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[4] = { 0x8004, 0x8000, 4, 0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x100, 4,  8,  8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x40,  4, 16, 16, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x08000;
	DrvZ80ROM1		= Next; Next += 0x02000;

	DrvGfxROM0		= Next; Next += 0x04000;
	DrvGfxROM1		= Next; Next += 0x04000;

	DrvColPROM		= Next; Next += 0x00220;

	DrvPalette		= (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM0		= Next; Next += 0x000100;
	DrvSprRAM1		= Next; Next += 0x000100;
	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120, 12, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,	0x9000, 0x90ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,	0x9400, 0x94ff, MAP_RAM);
	ZetSetWriteHandler(pooyan_main_write);
	ZetSetReadHandler(pooyan_main_read);
	ZetClose();

	TimepltSndInit(DrvZ80ROM1, DrvZ80RAM1, 1);
	TimepltSndVol(0.65, 0.65);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	TimepltSndExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer()
{
	for (INT32 i = 0; i < 0x0400; i++)
	{
		INT32 attr = DrvVidRAM[0x000 + i];
		INT32 code = DrvVidRAM[0x400 + i];
		INT32 color = attr & 0x0f;
		INT32 flipx = (attr >> 6) & 1;
		INT32 flipy = (attr >> 7) & 1;

		INT32 sx = (i << 3) & 0xf8;
		INT32 sy = (i >> 2) & 0xf8;

		if (flipscreen) {
			sx ^= 0xf8;
			sy ^= 0xf8;
			flipx ^= 1;
			flipy ^= 1;
		}

		if (sy > 239 || sy < 16) continue;
		sy -= 16;

		Draw8x8Tile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, DrvGfxROM0);
	}
}

static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = DrvGfxROM1;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (DrvPalette[pxl | (color << 4) | 0x100] == 0) continue; // 0 and 0x10 is transparent
			dest[sy * nScreenWidth + sx] = pxl | (color << 4) | 0x100;
		}
		sx -= width;
	}
}


static void draw_sprites()
{
	for (INT32 i = 0x10; i < 0x40; i += 2)
	{
		INT32 sx = DrvSprRAM0[i];
		INT32 sy = 240 - DrvSprRAM1[1 + i];

		INT32 code = DrvSprRAM0[i + 1] & 0x3f;
		INT32 color = (DrvSprRAM1[i] & 0x0f);// | 0x10;
		INT32 flipx = ~DrvSprRAM1[i] & 0x40;
		INT32 flipy = DrvSprRAM1[i] & 0x80;

		if (sy == 0 || sy >= 240) continue;

		sy -= 16;

		RenderTileCPMP(code, color, sx, sy, flipx, flipy, 16, 16);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites();

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

	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 1789772 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (irq_enable && i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		TimepltSndUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		TimepltSndScan(nAction, pnMin);

		SCAN_VAR(irqtrigger);
		SCAN_VAR(irq_enable);
		SCAN_VAR(flipscreen);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Pooyan

static struct BurnRomInfo pooyanRomDesc[] = {
	{ "1.4a",         0x2000, 0xbb319c63, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.5a",         0x2000, 0xa1463d98, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.6a",         0x2000, 0xfe1a9e08, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.7a",         0x2000, 0x9e0f9bcc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "xx.7a",        0x1000, 0xfbe2b368, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "xx.8a",        0x1000, 0xe1795b3d, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "8.10g",        0x1000, 0x931b29eb, 3 | BRF_GRA },	       //  6 Characters
	{ "7.9g",         0x1000, 0xbbe6d6e4, 3 | BRF_GRA },	       //  7 

	{ "6.9a",         0x1000, 0xb2d8c121, 4 | BRF_GRA },	       //  8 Sprites
	{ "5.8a",         0x1000, 0x1097c2b6, 4 | BRF_GRA },	       //  9

	{ "pooyan.pr1",   0x0020, 0xa06a6d0e, 5 | BRF_GRA },	       // 10 Color Proms
	{ "pooyan.pr3",   0x0100, 0x8cd4cd60, 5 | BRF_GRA },	       // 11
	{ "pooyan.pr2",   0x0100, 0x82748c0b, 5 | BRF_GRA },	       // 12
};

STD_ROM_PICK(pooyan)
STD_ROM_FN(pooyan)

struct BurnDriver BurnDrvPooyan = {
	"pooyan", NULL, NULL, NULL, "1982",
	"Pooyan\0", NULL, "Konami", "GX320",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, pooyanRomInfo, pooyanRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Pooyan (Stern Electronics)

static struct BurnRomInfo pooyansRomDesc[] = {
	{ "ic22_a4.cpu",  0x2000, 0x916ae7d7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic23_a5.cpu",  0x2000, 0x8fe38c61, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic24_a6.cpu",  0x2000, 0x2660218a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic25_a7.cpu",  0x2000, 0x3d2a10ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "xx.7a",        0x1000, 0xfbe2b368, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "xx.8a",        0x1000, 0xe1795b3d, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic13_g10.cpu", 0x1000, 0x7433aea9, 3 | BRF_GRA },	       //  6 Characters
	{ "ic14_g9.cpu",  0x1000, 0x87c1789e, 3 | BRF_GRA },	       //  7

	{ "6.9a",         0x1000, 0xb2d8c121, 4 | BRF_GRA },	       //  8 Sprites
	{ "5.8a",         0x1000, 0x1097c2b6, 4 | BRF_GRA },	       //  9

	{ "pooyan.pr1",   0x0020, 0xa06a6d0e, 5 | BRF_GRA },	       // 10 Color Proms
	{ "pooyan.pr3",   0x0100, 0x8cd4cd60, 5 | BRF_GRA },	       // 11
	{ "pooyan.pr2",   0x0100, 0x82748c0b, 5 | BRF_GRA },	       // 12
};

STD_ROM_PICK(pooyans)
STD_ROM_FN(pooyans)

struct BurnDriver BurnDrvPooyans = {
	"pooyans", "pooyan", NULL, NULL, "1982",
	"Pooyan (Stern Electronics)\0", NULL, "Konami (Stern Electronics license)", "GX320",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, pooyansRomInfo, pooyansRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Pootan

static struct BurnRomInfo pootanRomDesc[] = {
	{ "poo_ic22.bin", 0x2000, 0x41b23a24, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "poo_ic23.bin", 0x2000, 0xc9d94661, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.6a",         0x2000, 0xfe1a9e08, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "poo_ic25.bin", 0x2000, 0x8ae459ef, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "xx.7a",        0x1000, 0xfbe2b368, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "xx.8a",        0x1000, 0xe1795b3d, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "poo_ic13.bin", 0x1000, 0x0be802e4, 3 | BRF_GRA },	       //  6 Characters
	{ "poo_ic14.bin", 0x1000, 0xcba29096, 3 | BRF_GRA },	       //  7

	{ "6.9a",         0x1000, 0xb2d8c121, 4 | BRF_GRA },	       //  8 Sprites
	{ "5.8a",         0x1000, 0x1097c2b6, 4 | BRF_GRA },	       //  9

	{ "pooyan.pr1",   0x0020, 0xa06a6d0e, 5 | BRF_GRA },	       // 10 Color Proms
	{ "pooyan.pr3",   0x0100, 0x8cd4cd60, 5 | BRF_GRA },	       // 11
	{ "pooyan.pr2",   0x0100, 0x82748c0b, 5 | BRF_GRA },	       // 12
};

STD_ROM_PICK(pootan)
STD_ROM_FN(pootan)

struct BurnDriver BurnDrvPootan = {
	"pootan", "pooyan", NULL, NULL, "1982",
	"Pootan\0", NULL, "bootleg", "GX320",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, pootanRomInfo, pootanRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

