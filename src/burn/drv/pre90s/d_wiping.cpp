// FB Alpha Wiping driver module
// Based on MAME driver by Allard van der Bas

#include "tiles_generic.h"
#include "z80_intf.h"
#include "watchdog.h"
#include "resnet.h"
#include "wiping.h"

// to do
//	bug fix

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvTransTab;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvShareRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sound_irq_mask;
static UINT8 main_irq_mask;
static UINT8 flipscreen;

static INT32 sub_cpu_in_reset;
static INT32 skip_tile_enable = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo WipingInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wiping)

static struct BurnDIPInfo WipingDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL			},
	{0x10, 0xff, 0xff, 0x49, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x40, 0x40, "On"			},
	{0x0f, 0x01, 0x40, 0x00, "Off"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x80, 0x00, "30000 70000"		},
	{0x0f, 0x01, 0x80, 0x80, "50000 150000"		},

	{0   , 0xfe, 0   ,    7, "Coin B"		},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x10, 0x01, 0x38, 0x38, "7 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x30, "6 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x28, "5 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x18, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x08, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0xc0, 0x00, "2"			},
	{0x10, 0x01, 0xc0, 0x40, "3"			},
	{0x10, 0x01, 0xc0, 0x80, "4"			},
	{0x10, 0x01, 0xc0, 0xc0, "5"			},
};

STDDIPINFO(Wiping)

static struct BurnDIPInfo RugratsDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL			},
	{0x10, 0xff, 0xff, 0x49, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x40, 0x40, "On"			},
	{0x0f, 0x01, 0x40, 0x00, "Off"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x80, 0x00, "100000 200000"	},
	{0x0f, 0x01, 0x80, 0x80, "150000 300000"	},

	{0   , 0xfe, 0   ,    7, "Coin B"		},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x10, 0x01, 0x38, 0x38, "7 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x30, "6 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x28, "5 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x18, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x38, 0x08, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0xc0, 0x00, "2"			},
	{0x10, 0x01, 0xc0, 0x40, "3"			},
	{0x10, 0x01, 0xc0, 0x80, "4"			},
	{0x10, 0x01, 0xc0, 0xc0, "5"			},
};

STDDIPINFO(Rugrats)

static void __fastcall wiping_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			main_irq_mask = data & 1;
		return;

		case 0xa002:
			flipscreen = data & 1;
		return;

		case 0xa003:
			sub_cpu_in_reset = ~data & 1;
			if (sub_cpu_in_reset)
			{
				ZetReset(1);
			}
		return;

		case 0xb800:
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 __fastcall wiping_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
		case 0xa801:
		case 0xa802:
		case 0xa803:
		case 0xa804:
		case 0xa805:
		case 0xa806:
		case 0xa807:
			return DrvInputs[address & 7];
	}

	return 0;
}

static void __fastcall wiping_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x4000) {
		wipingsnd_write(address&0x3fff, data);
		return;
	}

	switch (address)
	{
		case 0xa001:
			sound_irq_mask = data & 1;
		return;
	}
}

static tilemap_scan( background )
{
	if (col <= 0x01) return (row + 2) + (col + 0x1e) * 0x20;
	if (col >= 0x22) return (row + 2) + (col - 0x22) * 0x20;

	return (col - 2) + (row + 2) * 0x20;
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], DrvColRAM[offs], (~DrvColRAM[offs] & skip_tile_enable) ? TILE_SKIP : 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	ZetReset(1);

	wipingsnd_reset();

	BurnWatchdogReset();

	sub_cpu_in_reset = 1;
	main_irq_mask = 0;
	sound_irq_mask = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x006000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvSndROM		= Next; Next += 0x004000;

	DrvSndPROM		= Next; Next += 0x000200;

	DrvTransTab		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000400;
	DrvShareRAM0		= Next; Next += 0x000400;
	DrvShareRAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(128,1), STEP4(136,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0080, 2, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00120,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x02000, 10, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSndPROM + 0x00100, 12, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvShareRAM0,	0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xb000, 0xb7ff, MAP_RAM);
	ZetSetWriteHandler(wiping_main_write);
	ZetSetReadHandler(wiping_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_RAM);
	ZetMapMemory(DrvShareRAM0,	0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0x9800, 0x9bff, MAP_RAM);
	ZetSetWriteHandler(wiping_sound_write);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	wipingsnd_init(DrvSndROM, DrvSndPROM);
	wipingsnd_wipingmode(); // lowers the volume of sample 0x1800 (game start: superloud noise)

	GenericTilesInit();
	GenericTilemapInit(0, background_map_scan, background_map_callback, 8, 8, 36, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x4000, 0, 0x3f);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	wipingsnd_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	static const int resistances_rg[3] = { 1000, 470, 220 };
	static const int resistances_b [2] = { 470, 220 };
	double rweights[3], gweights[3], bweights[2];
	UINT32 pal[32];

	compute_resistor_weights(0, 255, -1.0,
			3, &resistances_rg[0], rweights, 470, 0,
			3, &resistances_rg[0], gweights, 470, 0,
			2, &resistances_b[0],  bweights, 470, 0);

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = combine_2_weights(bweights, bit0, bit1);

		pal[i] = BurnHighCol(r,g,b, 0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 entry = (DrvColPROM[0x020 + (i ^ 0x03)] & 0xf) | ((i >> 4) & 0x10);
		DrvPalette[i] = pal[entry];
		DrvTransTab[i] = entry;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 128; offs += 2)
	{
		INT32 attr  =  DrvSprRAM[offs + 0x000];
		INT32 color =  DrvSprRAM[offs + 0x001] & 0x3f;
		INT32 sx    =  DrvSprRAM[offs + 0x101] + ((DrvSprRAM[offs + 0x081] & 1) << 8);
		INT32 sy    = 224 - DrvSprRAM[offs + 0x100];
		INT32 code  = (attr & 0x3f) | ((DrvSprRAM[offs + 0x080] & 1) << 6);
		INT32 flipy =  attr & 0x40;
		INT32 flipx =  attr & 0x80;

		if (flipscreen)
		{
			sy = (nScreenHeight - 16) - sy;
			flipy ^= 0x40;
			flipx ^= 0x80;
		}

		RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code, (color << 2), 0x1f, sx - 40, sy, flipx, flipy, 16, 16, DrvTransTab + 0x100, 0x100);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	BurnTransferClear();

	skip_tile_enable = 0;
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nBurnLayer & 4) draw_sprites();

	skip_tile_enable = 0x80;
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		{ // 4-Way conversion...
			UINT8 *DrvJoy[2] = { DrvJoy1, DrvJoy2 };
			UINT32 DrvJoyInit[2] = { 0x00, 0x00 };

			CompileInput(DrvJoy, (void*)DrvInputs, 2, 8, DrvJoyInit);

			// Convert to 4-way
			ProcessJoystick(&DrvInputs[0], 0, 0,1,3,2, INPUT_4WAY);
			ProcessJoystick(&DrvInputs[1], 1, 0,1,3,2, INPUT_4WAY);

			// ..and back again - game needs a very strange DrvInputs layout - below this block
			for (INT32 i = 0; i < 8; i++) {
				DrvJoy1[i] = (DrvInputs[0] & 1<<i) ? 1 : 0;
				DrvJoy2[i] = (DrvInputs[1] & 1<<i) ? 1 : 0;
			}
		}

		memset (DrvInputs, 0, 8);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[i] ^= (DrvJoy1[i] & 1) << 0;
			DrvInputs[i] ^= (DrvJoy2[i] & 1) << 1;
			DrvInputs[i] ^= (DrvJoy3[i] & 1) << 6;
			DrvInputs[i] ^= ((DrvDips[0] >> i) & 1) << 6;
			DrvInputs[i] ^= ((DrvDips[1] >> i) & 1) << 7;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1))
			if (main_irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		if (sub_cpu_in_reset) {
			nCyclesDone[1] += nCyclesTotal[1] / nInterleave;
		} else {
			nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		}
		if (i == (nInterleave - 1) || i == (nInterleave / 2) - 1)
			if (sound_irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();
	}

	if (pBurnSoundOut) {
		wipingsnd_update(pBurnSoundOut, nBurnSoundLen);
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

		BurnWatchdogScan(nAction);

		wipingsnd_scan(nAction, pnMin);

		SCAN_VAR(sound_irq_mask);
		SCAN_VAR(main_irq_mask);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sub_cpu_in_reset);
	}

	return 0;
}


// Wiping

static struct BurnRomInfo wipingRomDesc[] = {
	{ "1",			0x2000, 0xb55d0d19, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",			0x2000, 0xb1f96e47, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",			0x2000, 0xc67bab5a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4",			0x1000, 0xa1547e18, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "8",			0x1000, 0x601160f6, 3 | BRF_GRA },           //  4 Characters

	{ "7",			0x2000, 0x2c2cc054, 4 | BRF_GRA },           //  5 Sprites

	{ "wip-g13.bin",	0x0020, 0xb858b897, 5 | BRF_GRA },           //  6 Color data
	{ "wip-f4.bin",		0x0100, 0x3f56c8d5, 5 | BRF_GRA },           //  7
	{ "wip-e11.bin",	0x0100, 0xe7400715, 5 | BRF_GRA },           //  8

	{ "rugr5c8",		0x2000, 0x67bafbbf, 6 | BRF_SND },           //  9 Samples
	{ "rugr6c9",		0x2000, 0xcac84a87, 6 | BRF_SND },           // 10

	{ "wip-e8.bin",		0x0100, 0xbd2c080b, 7 | BRF_SND },           // 11 Sound Proms
	{ "wip-e9.bin",		0x0100, 0x4017a2a6, 7 | BRF_SND },           // 12
};

STD_ROM_PICK(wiping)
STD_ROM_FN(wiping)

struct BurnDriver BurnDrvWiping = {
	"wiping", NULL, NULL, NULL, "1982",
	"Wiping\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, wipingRomInfo, wipingRomName, NULL, NULL, NULL, NULL, WipingInputInfo, WipingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Rug Rats

static struct BurnRomInfo rugratsRomDesc[] = {
	{ "rugr1d1",		0x2000, 0xe7e1bd6d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rugr2d2",		0x2000, 0x5f47b9ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rugr3d3",		0x2000, 0x3d748d1a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "rugr4c4",		0x2000, 0xd4a92c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "rugr8d2",		0x1000, 0xa3dcaca5, 3 | BRF_GRA },           //  4 Characters

	{ "rugr7c13",		0x2000, 0xfe1191dd, 4 | BRF_GRA },           //  5 Sprites

	{ "prom.13g",		0x0020, 0xf21238f0, 5 | BRF_GRA },           //  6 Color data
	{ "prom.4f",		0x0100, 0xcfc90f3d, 5 | BRF_GRA },           //  7
	{ "prom.11e",		0x0100, 0xcfc90f3d, 5 | BRF_GRA },           //  8

	{ "rugr5c8",		0x2000, 0x67bafbbf, 6 | BRF_SND },           //  9 Samples
	{ "rugr6c9",		0x2000, 0xcac84a87, 6 | BRF_SND },           // 10

	{ "wip-e8.bin",		0x0100, 0xbd2c080b, 7 | BRF_SND },           // 11 Sound Proms
	{ "wip-e9.bin",		0x0100, 0x4017a2a6, 7 | BRF_SND },           // 12
};

STD_ROM_PICK(rugrats)
STD_ROM_FN(rugrats)

struct BurnDriver BurnDrvRugrats = {
	"rugrats", "wiping", NULL, NULL, "1983",
	"Rug Rats\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, rugratsRomInfo, rugratsRomName, NULL, NULL, NULL, NULL, WipingInputInfo, RugratsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};
