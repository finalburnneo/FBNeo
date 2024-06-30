// FB Neo Goindol driver module
// Based on MAME driver by Jarek Parchanski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

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
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 prot_toggle;
static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 bankdata;
static UINT8 char_bank;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;
static INT16 DrvAnalogPort0 = 0;
static UINT8 PaddleX = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GoindolInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	A("P1 Paddle",      BIT_ANALOG_REL, &DrvAnalogPort0,"p1 z-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A

STDINPUTINFO(Goindol)

static struct BurnDIPInfo GoindolDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xce, NULL					},
	{0x0f, 0xff, 0xff, 0xc7, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0e, 0x01, 0x03, 0x03, "2"					},
	{0x0e, 0x01, 0x03, 0x02, "3"					},
	{0x0e, 0x01, 0x03, 0x01, "4"					},
	{0x0e, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x0e, 0x01, 0x1c, 0x1c, "Easiest"				},
	{0x0e, 0x01, 0x1c, 0x18, "Very Very Easy"		},
	{0x0e, 0x01, 0x1c, 0x14, "Very Easy"			},
	{0x0e, 0x01, 0x1c, 0x10, "Easy"					},
	{0x0e, 0x01, 0x1c, 0x0c, "Normal"				},
	{0x0e, 0x01, 0x1c, 0x08, "Difficult"			},
	{0x0e, 0x01, 0x1c, 0x04, "Hard"					},
	{0x0e, 0x01, 0x1c, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0e, 0x01, 0x20, 0x20, "Off"					},
	{0x0e, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x0e, 0x01, 0x40, 0x40, "Off"					},
	{0x0e, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x0f, 0x01, 0x07, 0x04, "30k and every 50k"	},
	{0x0f, 0x01, 0x07, 0x05, "50k and every 100k"	},
	{0x0f, 0x01, 0x07, 0x06, "50k and every 200k"	},
	{0x0f, 0x01, 0x07, 0x07, "100k and every 200k"	},
	{0x0f, 0x01, 0x07, 0x01, "10000 only"			},
	{0x0f, 0x01, 0x07, 0x02, "30000 only"			},
	{0x0f, 0x01, 0x07, 0x03, "50000 only"			},
	{0x0f, 0x01, 0x07, 0x00, "None"					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x0f, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x38, 0x30, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x38, 0x38, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x40, 0x40, "Upright"				},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x80, 0x80, "Off"					},
	{0x0f, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Goindol)

static void bankswitch(INT32 data)
{
	bankdata = data;
	char_bank = (data >> 4) & 1;
	flipscreen = data & 0x20;

	ZetMapMemory(DrvZ80ROM0 + 0x8000 + (data & 3) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall goindol_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		return;

		case 0xc810:
			bankswitch(data);
		return;

		case 0xc820:
			scrolly = data;
		return;

		case 0xc830:
			scrollx = data;
		return;

		case 0xfc44:
			DrvZ80RAM0[0x0419] = 0x5b;
			DrvZ80RAM0[0x041a] = 0x3f;
			DrvZ80RAM0[0x041b] = 0x6d;
		return;

		case 0xfc66:
			DrvZ80RAM0[0x0423] = 0x06;
		return;

		case 0xfcb0:
			DrvZ80RAM0[0x0425] = 0x06;
		return;

		case 0xfd99:
			DrvZ80RAM0[0x0421] = 0x3f;
		return;
	}
}

static UINT8 __fastcall goindol_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
			return 0;

		case 0xc820:
			return PaddleX;

		case 0xc830:
			return DrvInputs[0];

		case 0xc834:
			return DrvInputs[1];

		case 0xf000:
			return DrvDips[0];

		case 0xf422:
			prot_toggle ^= 0x80;
			return prot_toggle;

		case 0xf800:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall goindol_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall goindol_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd800:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvBgRAM[offs * 2 + 0];
	INT32 code = DrvBgRAM[offs * 2 + 1] + ((attr & 7) * 256) + (char_bank * 0x800);

	TILE_SET_INFO(0, code, attr >> 3, 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvFgRAM[offs * 2 + 0];
	INT32 code = DrvFgRAM[offs * 2 + 1] + ((attr & 7) * 256) + (char_bank * 0x800);

	TILE_SET_INFO(1, code, attr >> 3, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	HiscoreReset();


	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;
	prot_toggle = 0;
	PaddleX = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x018000;
	DrvZ80ROM1	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x040000;

	DrvColPROM	= Next; Next += 0x000300;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x000800;
	DrvZ80RAM1	= Next; Next += 0x000800;
	DrvSprRAM0	= Next; Next += 0x000800;
	DrvSprRAM1	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x000800;
	DrvBgRAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x8000*8*0, 0x8000*8*1, 0x8000*8*2 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x18000);

	GfxDecode(0x1000, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x18000);

	GfxDecode(0x1000, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,		0xd000, 0xd7ff, MAP_RAM); // 0-3f
	ZetMapMemory(DrvBgRAM,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xe800, 0xefff, MAP_RAM);
	ZetSetWriteHandler(goindol_main_write);
	ZetSetReadHandler(goindol_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(goindol_sound_write);
	ZetSetReadHandler(goindol_sound_read);
	ZetClose();

	BurnYM2203Init(1,  1500000, NULL, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 8, 8, 0x40000, 0, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 3, 8, 8, 0x40000, 0, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();

	GenericTilesExit();

	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(UINT8 *ram, UINT8 *gfx)
{
	for (INT32 offs = 0; offs < 0x40; offs += 4)
	{
		INT32 sx = ram[offs];
		INT32 sy = 240 - ram[offs + 1];

		if (flipscreen)
		{
			sx = 248 - sx;
			sy = 248 - sy;
		}

		if ((ram[offs + 1] >> 3) && (sx < 248))
		{
			INT32 code = (((ram[offs + 3]) + ((ram[offs + 2] & 7) << 8))) << 1;
			INT32 color = ram[offs + 2] >> 3;

			Draw8x8MaskTile(pTransDraw, code + 0, sx, sy - 16, flipscreen, flipscreen, color, 3, 0, 0, gfx);
			Draw8x8MaskTile(pTransDraw, code + 1, sx, sy + (flipscreen ? -8 : 8) - 16, flipscreen, flipscreen, color, 3, 0, 0, gfx);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen) ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollX(1, scrollx);
	GenericTilemapSetScrollY(1, scrolly);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, pTransDraw, 0);
	if ((nBurnLayer & 2) != 0) GenericTilemapDraw(1, pTransDraw, 0);
	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM0, DrvGfxROM1);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM1, DrvGfxROM0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		static UINT8 lastcoin0 = 0;
		static UINT8 lastcoin1 = 0;

		DrvJoy1[4] |= DrvJoy2[4]; // shared controls - game only responds to player 1 inputs.
		DrvJoy1[5] |= DrvJoy2[5];

		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		// only 1 coin impulse per coin button
		if (!lastcoin0 && !(DrvInputs[0]&0x80)) { DrvInputs[0] |= 0x80; };
		lastcoin0 = DrvJoy1[7]^1;

		if (!lastcoin1 && !(DrvInputs[1]&0x80)) { DrvInputs[1] |= 0x80; };
		lastcoin1 = DrvJoy2[7]^1;

		UINT8 Temp = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x01, 0xff);
		if (Temp > 0x90 || (DrvJoy1[3] | DrvJoy2[3])) PaddleX+=8;
		if (Temp < 0x70 || (DrvJoy1[2] | DrvJoy2[2])) PaddleX-=8;

	}

	INT32 nInterleave = 4;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 5000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 4x / frame
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(bankdata); // charbank & flipscreen bits in here
		SCAN_VAR(soundlatch);
		SCAN_VAR(prot_toggle);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(PaddleX);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Goindol (SunA, World)

static struct BurnRomInfo goindolRomDesc[] = {
	{ "r1w",			0x8000, 0xdf77c502, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "r2",				0x8000, 0x1ff6e3a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3",				0x8000, 0xe9eec24a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "r10",			0x8000, 0x72e1add1, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "r4",				0x8000, 0x1ab84225, 3 | BRF_GRA },           //  4 Background Tiles / Sprites
	{ "r5",				0x8000, 0x4997d469, 3 | BRF_GRA },           //  5
	{ "r6",				0x8000, 0x752904b0, 3 | BRF_GRA },           //  6

	{ "r7",				0x8000, 0x362f2a27, 4 | BRF_GRA },           //  7 Foreground Tiles / Sprites
	{ "r8",				0x8000, 0x9fc7946e, 4 | BRF_GRA },           //  8
	{ "r9",				0x8000, 0xe6212fe4, 4 | BRF_GRA },           //  9

	{ "am27s21.pr1",	0x0100, 0x361f0868, 5 | BRF_GRA },           // 10 Color data
	{ "am27s21.pr2",	0x0100, 0xe355da4d, 5 | BRF_GRA },           // 11
	{ "am27s21.pr3",	0x0100, 0x8534cfb5, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(goindol)
STD_ROM_FN(goindol)

static INT32 GoindolInit()
{
	INT32 nRet = DrvInit();

	if (nRet != 0) return 1;

	DrvZ80ROM0[0x18e9] = 0x18; // ROM 1 check
	DrvZ80ROM0[0x1964] = 0x00; // ROM 9 error (MCU?)
	DrvZ80ROM0[0x1965] = 0x00; //
	DrvZ80ROM0[0x1966] = 0x00; //
//  DrvZ80ROM0[0x17c7] = 0x00; // c421 == 3f
//  DrvZ80ROM0[0x17c8] = 0x00; //
//  DrvZ80ROM0[0x16f0] = 0x18; // c425 == 06
//  DrvZ80ROM0[0x172c] = 0x18; // c423 == 06
//  DrvZ80ROM0[0x1779] = 0x00; // c419 == 5b 3f 6d
//  DrvZ80ROM0[0x177a] = 0x00; //
	DrvZ80ROM0[0x063f] = 0x18; //->fc55
	DrvZ80ROM0[0x0b30] = 0x00; // verify code at 0601-064b
	DrvZ80ROM0[0x1bdf] = 0x18; //->fc49
	DrvZ80ROM0[0x04a7] = 0xc9;
	DrvZ80ROM0[0x0831] = 0xc9;
	DrvZ80ROM0[0x3365] = 0x00; // verify code at 081d-0876
	DrvZ80ROM0[0x0c13] = 0xc9;
	DrvZ80ROM0[0x134e] = 0xc9;
	DrvZ80ROM0[0x333d] = 0xc9;

	return 0;
}

struct BurnDriver BurnDrvGoindol = {
	"goindol", NULL, NULL, NULL, "1987",
	"Goindol (SunA, World)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, goindolRomInfo, goindolRomName, NULL, NULL, NULL, NULL, GoindolInputInfo, GoindolDIPInfo,
	GoindolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Goindol (SunA, US)

static struct BurnRomInfo goindoluRomDesc[] = {
	{ "r1",				0x8000, 0x3111c61b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "r2",				0x8000, 0x1ff6e3a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3",				0x8000, 0xe9eec24a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "r10",			0x8000, 0x72e1add1, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "r4",				0x8000, 0x1ab84225, 3 | BRF_GRA },           //  4 Background Tiles / Sprites
	{ "r5",				0x8000, 0x4997d469, 3 | BRF_GRA },           //  5
	{ "r6",				0x8000, 0x752904b0, 3 | BRF_GRA },           //  6

	{ "r7",				0x8000, 0x362f2a27, 4 | BRF_GRA },           //  7 Foreground Tiles / Sprites
	{ "r8",				0x8000, 0x9fc7946e, 4 | BRF_GRA },           //  8
	{ "r9",				0x8000, 0xe6212fe4, 4 | BRF_GRA },           //  9

	{ "am27s21.pr1",	0x0100, 0x361f0868, 5 | BRF_GRA },           // 10 Color data
	{ "am27s21.pr2",	0x0100, 0xe355da4d, 5 | BRF_GRA },           // 11
	{ "am27s21.pr3",	0x0100, 0x8534cfb5, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(goindolu)
STD_ROM_FN(goindolu)

struct BurnDriver BurnDrvGoindolu = {
	"goindolu", "goindol", NULL, NULL, "1987",
	"Goindol (SunA, US)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, goindoluRomInfo, goindoluRomName, NULL, NULL, NULL, NULL, GoindolInputInfo, GoindolDIPInfo,
	GoindolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Goindol (SunA, Japan)

static struct BurnRomInfo goindoljRomDesc[] = {
	{ "r1j",			0x8000, 0xdde33ad3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "r2",				0x8000, 0x1ff6e3a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3",				0x8000, 0xe9eec24a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "r10",			0x8000, 0x72e1add1, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "r4",				0x8000, 0x1ab84225, 3 | BRF_GRA },           //  4 Background Tiles / Sprites
	{ "r5",				0x8000, 0x4997d469, 3 | BRF_GRA },           //  5
	{ "r6",				0x8000, 0x752904b0, 3 | BRF_GRA },           //  6

	{ "r7",				0x8000, 0x362f2a27, 4 | BRF_GRA },           //  7 Foreground Tiles / Sprites
	{ "r8",				0x8000, 0x9fc7946e, 4 | BRF_GRA },           //  8
	{ "r9",				0x8000, 0xe6212fe4, 4 | BRF_GRA },           //  9

	{ "am27s21.pr1",	0x0100, 0x361f0868, 5 | BRF_GRA },           // 10 Color data
	{ "am27s21.pr2",	0x0100, 0xe355da4d, 5 | BRF_GRA },           // 11
	{ "am27s21.pr3",	0x0100, 0x8534cfb5, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(goindolj)
STD_ROM_FN(goindolj)

struct BurnDriver BurnDrvGoindolj = {
	"goindolj", "goindol", NULL, NULL, "1987",
	"Goindol (SunA, Japan)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, goindoljRomInfo, goindoljRomName, NULL, NULL, NULL, NULL, GoindolInputInfo, GoindolDIPInfo,
	GoindolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Homo

static struct BurnRomInfo homoRomDesc[] = {
	{ "homo.01",		0x8000, 0x28c539ad, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "r2",				0x8000, 0x1ff6e3a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3",				0x8000, 0xe9eec24a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "r10",			0x8000, 0x72e1add1, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "r4",				0x8000, 0x1ab84225, 3 | BRF_GRA },           //  4 Background Tiles / Sprites
	{ "r5",				0x8000, 0x4997d469, 3 | BRF_GRA },           //  5
	{ "r6",				0x8000, 0x752904b0, 3 | BRF_GRA },           //  6

	{ "r7",				0x8000, 0x362f2a27, 4 | BRF_GRA },           //  7 Foreground Tiles / Sprites
	{ "r8",				0x8000, 0x9fc7946e, 4 | BRF_GRA },           //  8
	{ "r9",				0x8000, 0xe6212fe4, 4 | BRF_GRA },           //  9

	{ "am27s21.pr1",	0x0100, 0x361f0868, 5 | BRF_GRA },           // 10 Color data
	{ "am27s21.pr2",	0x0100, 0xe355da4d, 5 | BRF_GRA },           // 11
	{ "am27s21.pr3",	0x0100, 0x8534cfb5, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(homo)
STD_ROM_FN(homo)

struct BurnDriver BurnDrvHomo = {
	"homo", "goindol", NULL, NULL, "1987",
	"Homo\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, homoRomInfo, homoRomName, NULL, NULL, NULL, NULL, GoindolInputInfo, GoindolDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
