// FB Alpha Mikie driver module
// Based on MAME driver by Allard Van Der Bas

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *palette_bank;
static UINT8 *irq_mask;
static UINT8 *sound_irq;

static int watchdog;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MikieInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Mikie)

static struct BurnDIPInfo MikieDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x7b, NULL			},
	{0x14, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x02, "4"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

//	flipscreen not implimented
//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x13, 0x01, 0x04, 0x00, "Upright"		},
//	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "20k 70k 50k+"		},
	{0x13, 0x01, 0x18, 0x10, "30K 90k 60k+"		},
	{0x13, 0x01, 0x18, 0x08, "30k only"		},
	{0x13, 0x01, 0x18, 0x00, "40K only"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Medium"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x00, "Off"			},
	{0x14, 0x01, 0x01, 0x01, "On"			},
	
	{0   , 0xfe, 0   ,    2, "Upright Controls"	},
	{0x14, 0x01, 0x02, 0x02, "Single"		},
	{0x14, 0x01, 0x02, 0x00, "Dual"			},
};

STDDIPINFO(Mikie)

static void mikie_main_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0x2000:
		case 0x2001:
			// coin_counter
		return;

		case 0x2002:
			if (*sound_irq == 0 && d == 1) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
			*sound_irq = d;
		return;

		case 0x2006:
			*flipscreen = d & 1;
		return;

		case 0x2007:
			*irq_mask = d & 1;
		return;

		case 0x2100:
			watchdog = 0;
		return;

		case 0x2200:
			*palette_bank = (d & 0x07) << 4;
		return;

		case 0x2300:
		return; // nop

		case 0x2400:
			*soundlatch = d;
		return;
	}
}

static UINT8 mikie_main_read(UINT16 a)
{
	switch (a)
	{
		case 0x2400:
		case 0x2401:
		case 0x2402:
			return DrvInputs[a & 3];

		case 0x2403:
			return DrvDips[2];

		case 0x2500:
		case 0x2501:
			return DrvDips[a & 1];
	}

	return 0;
}

static void __fastcall mikie_sound_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0x8000:
		case 0x8001:
		return; // nop

		case 0x8002:
		case 0x8004:
			SN76496Write((a / 4) & 1, d);
		return;

		case 0x8079:
		case 0xa003:
		return; // nop
	}
}

static UINT8 __fastcall mikie_sound_read(UINT16 a)
{
	switch (a)
	{
		case 0x8003:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0x8005:
			return ZetTotalCycles() / 512;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		pal[i] = BurnHighCol(r,g,b, 0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			UINT8 ctabentry = (j << 5) | ((~i & 0x100) >> 4) | (DrvColPROM[0x300+i] & 0x0f);
			DrvPalette[((i & 0x100) << 3) | (j << 8) | (i & 0xff)] = pal[ctabentry];
		}
	}

	DrvRecalc = 1;
}
static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 Plane1[4]  = { STEP2(0,4), STEP2(256*128*8, 4) };
	INT32 XOffs0[8]  = { STEP8(0,4) };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 XOffs1[16] = { STEP4(256, 1), STEP4(128, 1), STEP4(0, 1), STEP4(384,1) };
	INT32 YOffs1[16] = { STEP8(0,16), STEP8(512, 16) };

	UINT8 *buf = (UINT8*)BurnMalloc(0x10000);

	memcpy (buf, DrvGfxROM0, 0x4000);

	GfxDecode(0x00200, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, buf, DrvGfxROM0);

	memcpy (buf, DrvGfxROM1, 0x10000);

	GfxDecode(0x00100, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, buf+0, DrvGfxROM1);
	GfxDecode(0x00100, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, buf+1, DrvGfxROM1 + 0x10000);

	BurnFree(buf);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;

	DrvZ80RAM		= Next; Next += 0x000400;

	soundlatch		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	palette_bank		= Next; Next += 0x000001;
	irq_mask		= Next; Next += 0x000001;
	sound_irq		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	// Refresh Rate 60.59

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x6000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x8000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0xc000,  8, 1)) return 1;
 
		if (BurnLoadRom(DrvColPROM  + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0100, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0200, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0300, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0400, 13, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,	0x0000, 0x00ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,	0x2800, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,	0x3800, 0x3bff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,	0x3c00, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0x6000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mikie_main_write);
	M6809SetReadHandler(mikie_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(mikie_sound_write);
	ZetSetReadHandler(mikie_sound_read);
	ZetClose();

	SN76489AInit(0, 14318180 / 8, 0);
	SN76489AInit(1, 14318180 / 4, 1);
	SN76496SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer(INT32 prio)
{
	for (INT32 offs = 2 * 32; offs < (32 * 32) - (2 * 32); offs++)
	{
		if ((DrvColRAM[offs] & 0x10) != prio) continue;

		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvColRAM[offs];
		INT32 code  = DrvVidRAM[offs] + ((attr & 0x20) << 3);
		INT32 color = (attr & 0x0f) + *palette_bank;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile(pTransDraw, code, sx, sy - 16, color, 4, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x90; offs += 4)
	{
		int attr    =  DrvSprRAM[offs];
		int code    = (DrvSprRAM[offs + 2] & 0x3f) + ((DrvSprRAM[offs + 2] & 0x80) >> 1) + ((DrvSprRAM[offs + 2] & 0x40) << 2) + ((attr & 0x40) << 1);
		int color   = (attr & 0x0f) + *palette_bank;
		int sx      =  DrvSprRAM[offs + 3];
		int sy      = 244 - DrvSprRAM[offs + 1];
		int flipx   = ~attr & 0x10;
		int flipy   =  attr & 0x20;

		if (*flipscreen)
		{
			sy = 242 - sy;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x800, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x800, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x800, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x800, DrvGfxROM1);
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

	draw_layer(0x00);
	draw_sprites();
	draw_layer(0x10);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog == 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 18432000 / 12 / 60, 14318180 / 4 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (i == 240 && *irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

	}

	ZetClose();
	M6809Close();
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
		}
	}
	
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
		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);
	}

	return 0;
}

// Mikie

static struct BurnRomInfo mikieRomDesc[] = {
	{ "n14.11c",	0x2000, 0xf698e6dd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "o13.12a",	0x4000, 0x826e7035, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17.12d",	0x4000, 0x161c25c8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "n10.6e",	0x2000, 0x2cf9d670, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "o11.8i",	0x4000, 0x3c82aaf3, 3 | BRF_GRA },           //  4 Characters

	{ "001.f1",	0x4000, 0xa2ba0df5, 4 | BRF_GRA },           //  5 Sprites
	{ "003.f3",	0x4000, 0x9775ab32, 4 | BRF_GRA },           //  6
	{ "005.h1",	0x4000, 0xba44aeef, 4 | BRF_GRA },           //  7
	{ "007.h3",	0x4000, 0x31afc153, 4 | BRF_GRA },           //  8

	{ "d19.1i",	0x0100, 0x8b83e7cf, 5 | BRF_GRA },           //  9 Color Proms
	{ "d21.3i",	0x0100, 0x3556304a, 5 | BRF_GRA },           // 10
	{ "d20.2i",	0x0100, 0x676a0669, 5 | BRF_GRA },           // 11
	{ "d22.12h",	0x0100, 0x872be05c, 5 | BRF_GRA },           // 12
	{ "d18.f9",	0x0100, 0x7396b374, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(mikie)
STD_ROM_FN(mikie)

struct BurnDriver BurnDrvMikie = {
	"mikie", NULL, NULL, NULL, "1984",
	"Mikie\0", NULL, "Konami", "GX469",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, mikieRomInfo, mikieRomName, NULL, NULL, MikieInputInfo, MikieDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Shinnyuushain Tooru-kun

static struct BurnRomInfo mikiejRomDesc[] = {
	{ "n14.11c",	0x2000, 0xf698e6dd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "o13.12a",	0x4000, 0x826e7035, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17.12d",	0x4000, 0x161c25c8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "n10.6e",	0x2000, 0x2cf9d670, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "q11.8i",	0x4000, 0xc48b269b, 3 | BRF_GRA },           //  4 Characters

	{ "q01.f1",	0x4000, 0x31551987, 4 | BRF_GRA },           //  5 Sprites
	{ "q03.f3",	0x4000, 0x34414df0, 4 | BRF_GRA },           //  6
	{ "q05.h1",	0x4000, 0xf9e1ebb1, 4 | BRF_GRA },           //  7
	{ "q07.h3",	0x4000, 0x15dc093b, 4 | BRF_GRA },           //  8

	{ "d19.1i",	0x0100, 0x8b83e7cf, 5 | BRF_GRA },           //  9 Color Proms
	{ "d21.3i",	0x0100, 0x3556304a, 5 | BRF_GRA },           // 10
	{ "d20.2i",	0x0100, 0x676a0669, 5 | BRF_GRA },           // 11
	{ "d22.12h",	0x0100, 0x872be05c, 5 | BRF_GRA },           // 12
	{ "d18.f9",	0x0100, 0x7396b374, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(mikiej)
STD_ROM_FN(mikiej)

struct BurnDriver BurnDrvMikiej = {
	"mikiej", "mikie", NULL, NULL, "1984",
	"Shinnyuushain Tooru-kun\0", NULL, "Konami", "GX469",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, mikiejRomInfo, mikiejRomName, NULL, NULL, MikieInputInfo, MikieDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Mikie (High School Graffiti)

static struct BurnRomInfo mikiehsRomDesc[] = {
	{ "l14.11c",	0x2000, 0x633f3a6d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "m13.12a",	0x4000, 0x9c42d715, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m17.12d",	0x4000, 0xcb5c03c9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "h10.6e",	0x2000, 0x4ed887d2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "l11.8i",	0x4000, 0x5ba9d86b, 3 | BRF_GRA },           //  4 Characters

	{ "i01.f1",	0x4000, 0x0c0cab5f, 4 | BRF_GRA },           //  5 Sprites
	{ "i03.f3",	0x4000, 0x694da32f, 4 | BRF_GRA },           //  6
	{ "i05.h1",	0x4000, 0x00e357e1, 4 | BRF_GRA },           //  7
	{ "i07.h3",	0x4000, 0xceeba6ac, 4 | BRF_GRA },           //  8

	{ "d19.1i",	0x0100, 0x8b83e7cf, 5 | BRF_GRA },           //  9 Color Proms
	{ "d21.3i",	0x0100, 0x3556304a, 5 | BRF_GRA },           // 10
	{ "d20.2i",	0x0100, 0x676a0669, 5 | BRF_GRA },           // 11
	{ "d22.12h",	0x0100, 0x872be05c, 5 | BRF_GRA },           // 12
	{ "d18.f9",	0x0100, 0x7396b374, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(mikiehs)
STD_ROM_FN(mikiehs)

struct BurnDriver BurnDrvMikiehs = {
	"mikiehs", "mikie", NULL, NULL, "1984",
	"Mikie (High School Graffiti)\0", NULL, "Konami", "GX469",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, mikiehsRomInfo, mikiehsRomName, NULL, NULL, MikieInputInfo, MikieDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};
