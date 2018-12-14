// FB Alpha Galaga 3 / Gaplus driver module
// Based on MAME driver by Manuel Abadia, Ernesto Corvi, and Nicola Salmoria

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "namco_snd.h"
#include "namcoio.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvM6809ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sub_cpu_in_reset;
static UINT8 sub2_cpu_in_reset;
static UINT8 main_irq_mask;
static UINT8 sub_irq_mask;
static UINT8 sub2_irq_mask;
static UINT8 flipscreen;

static UINT8 *custom_io;
static UINT8 *starfield_control;

struct star {
	float x, y;
	INT32 col, set;
};

static INT32 total_stars;
static struct star *stars;

static INT32 watchdog; // not hooked up

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[5];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo GaplusInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,      "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",		BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
};

STDINPUTINFO(Gaplus)

static struct BurnDIPInfo GaplusDIPList[]=
{
	{0x10, 0xff, 0xff, 0x0f, NULL				},
	{0x11, 0xff, 0xff, 0x0f, NULL				},
	{0x12, 0xff, 0xff, 0x0f, NULL				},
	{0x13, 0xff, 0xff, 0x0f, NULL				},
	{0x14, 0xff, 0xff, 0x0f, NULL				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x10, 0x01, 0x08, 0x00, "Off"				},
	{0x10, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x11, 0x01, 0x03, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x0c, 0x08, "2"				},
	{0x11, 0x01, 0x0c, 0x0c, "3"				},
	{0x11, 0x01, 0x0c, 0x04, "4"				},
	{0x11, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x12, 0x01, 0x07, 0x00, "30k 70k and every 70k"	},
	{0x12, 0x01, 0x07, 0x01, "30k 100k and every 100k"	},
	{0x12, 0x01, 0x07, 0x02, "30k 100k and every 200k"	},
	{0x12, 0x01, 0x07, 0x03, "50k 100k and every 100k"	},
	{0x12, 0x01, 0x07, 0x04, "50k 100k and every 200k"	},
	{0x12, 0x01, 0x07, 0x07, "50k 150k and every 150k"	},
	{0x12, 0x01, 0x07, 0x05, "50k 150k and every 300k"	},
	{0x12, 0x01, 0x07, 0x06, "50k 150k"			},

	{0   , 0xfe, 0   ,    2, "Round Advance"		},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
	{0x12, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x13, 0x01, 0x07, 0x07, "0 - Standard"			},
	{0x13, 0x01, 0x07, 0x06, "1 - Easiest"			},
	{0x13, 0x01, 0x07, 0x05, "2"				},
	{0x13, 0x01, 0x07, 0x04, "3"				},
	{0x13, 0x01, 0x07, 0x03, "4"				},
	{0x13, 0x01, 0x07, 0x02, "5"				},
	{0x13, 0x01, 0x07, 0x01, "6"				},
	{0x13, 0x01, 0x07, 0x00, "7 - Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x14, 0x01, 0x04, 0x04, "Upright"			},
	{0x14, 0x01, 0x04, 0x00, "Cocktail"			},
};

STDDIPINFO(Gaplus)

static void gaplus_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x6000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xfff0) == 0x6800) {
		namcoio_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x6810) {
		namcoio_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x6820) {
		custom_io[address & 0xf] = data;

		if (address == 0x6829 && data >= 0x0f) {
			BurnSamplePlay(0);
		}

		return;
	}

	if ((address & 0xf000) == 0x7000) {
		main_irq_mask = (~address >> 11) & 1;
		if (main_irq_mask == 0) M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}

	if ((address & 0xf000) == 0x8000) {
		sub_cpu_in_reset = (address >> 11) & 1;
		sub2_cpu_in_reset = sub_cpu_in_reset;
		if (sub_cpu_in_reset) {
			M6809Close();
			M6809Open(1);
			M6809Reset();
			M6809Close();
			M6809Open(2);
			M6809Reset();
			M6809Close();
			M6809Open(0);
		}
		namco_15xx_sound_enable((sub_cpu_in_reset)^1);
		return;
	}

	if ((address & 0xf000) == 0x9000) {
		INT32 bit = !(address >> 11) & 1;
		namcoio_set_reset_line(0, bit);
		namcoio_set_reset_line(1, bit);
		return;
	}

	if ((address & 0xf800) == 0xa000) {
		starfield_control[address & 3] = data;
		return;
	}
}

static UINT8 gaplus_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x6000) {
		return namco_15xx_sharedram_read(address);
	}

	if ((address & 0xfff0) == 0x6800) {
		return namcoio_read(0, address & 0xf);
	}

	if ((address & 0xfff0) == 0x6810) {
		return namcoio_read(1, address & 0xf);
	}

	if ((address & 0xfff0) == 0x6820) {
		INT32 mode = custom_io[8];

		switch (address & 0xf)
		{
			case 0: return DrvDips[4]; // in2
			case 1: return (mode == 2) ? custom_io[1] : 0xf;
			case 2: return (mode == 2) ? 0xf : 0xe;
			case 3:	return (mode == 2) ? custom_io[3] : 0x1;
		}

		return custom_io[address & 0xf];
	}

	if ((address & 0xf800) == 0x7800) {
		watchdog = 0;
		return 0;
	}

	return 0;
}

static void gaplus_sub_write(UINT16 address, UINT8 /*data*/)
{
	if ((address & 0xf000) == 0x6000) {
		sub_irq_mask = address & 1;
		if (sub_irq_mask == 0) M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}
static void gaplus_sub2_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x0000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xe000) == 0x2000) {
		watchdog = 0;
		return;
	}

	if ((address & 0xc000) == 0x4000) {
		sub2_irq_mask = (~address >> 13) & 1;
		if (sub2_irq_mask == 0) M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 gaplus_sub2_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x0000) {
		return namco_15xx_sharedram_read(address);
	}

	if ((address & 0xe000) == 0x6800) {
		watchdog = 0;
		return 0;
	}

	return 0;
}

static tilemap_scan( background )
{
	INT32 offs;

	row += 2;
	col -= 2;
	if (col & 0x20)
		offs = row + ((col & 0x1f) << 5);
	else
		offs = col + (row << 5);

	return offs;
}

static tilemap_callback( background )
{
	UINT8 code = DrvVidRAM[offs];
	UINT8 attr = DrvVidRAM[offs + 0x400];

	TILE_SET_INFO(0, code + ((attr & 0x80) << 1), attr, TILE_GROUP((attr >> 6) & 1));
}

static UINT8 nio0_i0(UINT8) { return DrvInputs[3]; }
static UINT8 nio0_i1(UINT8) { return DrvInputs[0]; }
static UINT8 nio0_i2(UINT8) { return DrvInputs[1]; }
static UINT8 nio0_i3(UINT8) { return DrvInputs[2]; }
static UINT8 nio1_i0(UINT8) { return DrvDips[1]; }
static UINT8 nio1_i1(UINT8) { return DrvDips[2]; }
static UINT8 nio1_i2(UINT8) { return DrvDips[3]; }
static UINT8 nio1_i3(UINT8) { return DrvDips[0]; }

static void starfield_init()
{
	INT32 generator = 0;
	INT32 set = 0;

	total_stars = 0;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = nScreenWidth*2 - 1; x >= 0; x--)
		{
			generator <<= 1;
			INT32 bit1 = (~generator >> 17) & 1;
			INT32 bit2 = (generator >> 5) & 1;

			if (bit1 ^ bit2) generator |= 1;

			if (((~generator >> 16) & 1) && (generator & 0xff) == 0xff) {
				int color;

				color = (~(generator >> 8)) & 0x3f;
				if (color && total_stars < 240) {
					stars[total_stars].x = x;
					stars[total_stars].y = y;
					stars[total_stars].col = color;
					stars[total_stars].set = set++;

					if (set == 3) set = 0;

					if (total_stars+1 < 260)
						total_stars++;
				}
			}
		}
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	NamcoSoundReset();
	BurnSampleReset();
	M6809Close();

	M6809Open(2);
	M6809Reset();
	M6809Close();

	namcoio_reset(0);
	namcoio_reset(1);

	main_irq_mask = 0;
	sub_irq_mask = 0;
	sub2_irq_mask = 0;

	sub_cpu_in_reset = 1; // by default
	sub2_cpu_in_reset = 0;
	watchdog = 0;
	flipscreen = 0;

	starfield_init();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0    = Next; Next += 0x006000;
	DrvM6809ROM1    = Next; Next += 0x006000;
	DrvM6809ROM2    = Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000800;

	NamcoSoundProm  = Next;
	DrvSndPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001800;

	custom_io       = Next; Next += 0x000010;
	starfield_control = Next; Next += 0x000010;
	stars           = (star*)Next; Next += 260 * sizeof(star);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 4, 6 };
	INT32 Plane1[3]  = { 0x6000*8, 0, 4 };
	INT32 XOffs0[ 8] = { 16*8, 16*8+1, 24*8, 24*8+1, 0, 1, 8*8, 8*8+1 };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16]  = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0180, 3, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 gaplusd)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x4000,  5, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM2 + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x4000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x6000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0200, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0300, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0400, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0600, 17, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000, 18, 1)) return 1;

		for (INT32 i = 0; i < 0x200; i++) { // merge nibbles
			DrvColPROM[0x400+i] = (DrvColPROM[0x400+i] & 0xf) | (DrvColPROM[0x600+i] << 4);
		}

		for (INT32 i = 0; i < 0x2000; i++) {
			DrvGfxROM0[i + 0x2000] = DrvGfxROM0[i] >> 4;
			DrvGfxROM1[i + 0x8000] = DrvGfxROM1[i + 0x6000] << 4;
		}

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gaplus_main_write);
	M6809SetReadHandler(gaplus_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gaplus_sub_write);
	M6809Close();

	M6809Init(2);
	M6809Open(2);
	M6809MapMemory(DrvM6809ROM2,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gaplus_sub2_write);
	M6809SetReadHandler(gaplus_sub2_read);
	M6809Close();

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, gaplusd ? NAMCO58xx : NAMCO56xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL, NULL); // lamps not hooked up
	namcoio_init(1, gaplusd ? NAMCO56xx : NAMCO58xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, NULL, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, background_map_scan, background_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x4000*4, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	BurnSampleExit();
	NamcoSoundExit();
	NamcoSoundProm = NULL;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0; i < 256; i++)
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

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 4 * 64; i++) {
		DrvPalette[i] = pal[0xf0 | (DrvColPROM[i+0x300] & 0xf)];
	}

	for (INT32 i = 0; i < 8 * 64; i++) {
		DrvPalette[0x100+i] = pal[DrvColPROM[0x400+i]];
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM + 0x780;
	UINT8 *spriteram_2 = spriteram + 0x800;
	UINT8 *spriteram_3 = spriteram_2 + 0x800;

	for (INT32 offs = 0; offs < 0x80; offs += 2)
	{
		if ((spriteram_3[offs+1] & 2) == 0)
		{
			static const int gfx_offs[2][2] = {
				{ 0, 1 },
				{ 2, 3 }
			};
			INT32 sprite = spriteram[offs] | ((spriteram_3[offs] & 0x40) << 2);
			INT32 color = spriteram[offs+1] & 0x3f;
			INT32 sx = spriteram_2[offs+1] + 0x100 * (spriteram_3[offs+1] & 1) - 71;
			INT32 sy = 256 - spriteram_2[offs] - 8;
			INT32 flipx = (spriteram_3[offs] & 0x01);
			INT32 flipy = (spriteram_3[offs] & 0x02) >> 1;
			INT32 sizex = (spriteram_3[offs] & 0x08) >> 3;
			INT32 sizey = (spriteram_3[offs] & 0x20) >> 5;
			INT32 duplicate = spriteram_3[offs] & 0x80;

			if (flipscreen)
			{
				flipx ^= 1;
				flipy ^= 1;
			}

			sy = ((sy - 16 * sizey) & 0xff) - 32;

			for (INT32 y = 0;y <= sizey;y++)
			{
				for (INT32 x = 0;x <= sizex;x++)
				{
					INT32 code = sprite + (duplicate ? 0 : (gfx_offs[y ^ (sizey * flipy)][x ^ (sizex * flipx)]));

					RenderTileTranstab(pTransDraw, DrvGfxROM1, code, (color*8)+0x100, 0xff, sx + 16 * x, sy + 16 * y, flipx, flipy, 16, 16, DrvColPROM + 0x300);
				}
			}
		}
	}
}

static void starfield_render()
{
	if ((starfield_control[0] & 1) == 0)
		return;

	for (INT32 i = 0; i < total_stars; i++)
	{
		INT32 x = (INT32)stars[i].x;
		INT32 y = (INT32)stars[i].y;

		if (x >= 0 && x < nScreenWidth && y >= 0 && y < nScreenHeight)
		{
			pTransDraw[(y * nScreenWidth) + x] = stars[i].col;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	flipscreen = DrvSprRAM[0x1f7f-0x800] & 1;
	GenericTilemapSetFlip(0, flipscreen);

	BurnTransferClear();

	if (nBurnLayer & 1) starfield_render();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void vblank_update()
{
	if ((starfield_control[0] & 1) == 0)
		return;

	for (INT32 i = 0; i < total_stars; i++)
	{
		switch (starfield_control[stars[i].set + 1])
		{
			case 0x86: stars[i].x += 0.5f; break;
			case 0x85: stars[i].x += 1.0f; break;
			case 0x06: stars[i].x += 2.0f; break;
			case 0x80: stars[i].x -= 0.5f; break;
			case 0x82: stars[i].x -= 1.0f; break;
			case 0x81: stars[i].x -= 2.0f; break;
			case 0x9f: stars[i].y += 1.0f; break;
			case 0xaf: stars[i].y += 0.5f; break;
		}

		if (stars[i].x < 0) stars[i].x = (float)(nScreenWidth*2 ) + stars[i].x;
		if (stars[i].x >= (float)(nScreenWidth*2)) stars[i].x -= (float)(nScreenWidth*2);
		if (stars[i].y < 0) stars[i].y = (float)(nScreenHeight) + stars[i].y;
		if (stars[i].y >= (float)(nScreenHeight)) stars[i].y -= (float)(nScreenHeight);
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[3] = { (INT32)((double)1536000 / 60.606061), (INT32)((double)1536000 / 60.606061), (INT32)((double)1536000 / 60.606061) };
	INT32 nCyclesDone[3] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		nCyclesDone[0] += M6809Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == (nInterleave - 1))
		{
			if (main_irq_mask)
				M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);

			vblank_update();

			if (!namcoio_read_reset_line(0))
				namcoio_run(0);

			if (!namcoio_read_reset_line(1))
				namcoio_run(1);
		}
		M6809Close();

		if (sub_cpu_in_reset) {
			nCyclesDone[1] += ((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1];
		} else {
			M6809Open(1);
			nCyclesDone[1] += M6809Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
			if (i == nInterleave-1 && sub_irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
		}

		if (sub2_cpu_in_reset) {
			nCyclesDone[2] += ((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2];
		} else {
			M6809Open(2);
			nCyclesDone[2] += M6809Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
			if (i == nInterleave-1 && sub_irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
		}
	}

	if (pBurnSoundOut) {
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		NamcoSoundScan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);
		namcoio_scan(0);
		namcoio_scan(1);

		SCAN_VAR(sub_cpu_in_reset);
		SCAN_VAR(sub2_cpu_in_reset);
		SCAN_VAR(main_irq_mask);
		SCAN_VAR(sub_irq_mask);
		SCAN_VAR(sub2_irq_mask);
		SCAN_VAR(flipscreen);
	}

	return 0;
}

static struct BurnSampleInfo GaplusSampleDesc[] = {
	{ "bang", SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Gaplus)
STD_SAMPLE_FN(Gaplus)


// Gaplus (GP2 rev. B)

static struct BurnRomInfo gaplusRomDesc[] = {
	{ "gp2-4.8d",		0x2000, 0xe525d75d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp2-3b.8c",		0x2000, 0xd77840a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp2-2b.8b",		0x2000, 0xb3cb90db, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp2-8.11d",		0x2000, 0x42b9fd7c, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp2-6.11b",		0x2000, 0x75b18652, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp2-5.8s",		0x2000, 0xf3d19987, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp2-6.6p",		0x0200, 0x6f99c2da, 6 | BRF_GRA },           // 16
	{ "gp2-5.6n",		0x0200, 0xc7d31657, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(gaplus)
STD_ROM_FN(gaplus)

static INT32 GaplusInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvGaplus = {
	"gaplus", NULL, NULL, "gaplus", "1984",
	"Gaplus (GP2 rev. B)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gaplusRomInfo, gaplusRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Gaplus (GP2)

static struct BurnRomInfo gaplusaRomDesc[] = {
	{ "gp2-4.8d",		0x2000, 0xe525d75d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp2-3b.8c",		0x2000, 0xd77840a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp2-2.8b",		0x2000, 0x61f6cc65, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp2-8.11d",		0x2000, 0x42b9fd7c, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp2-6.11b",		0x2000, 0x75b18652, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp2-5.8s",		0x2000, 0xf3d19987, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp2-6.6p",		0x0200, 0x6f99c2da, 6 | BRF_GRA },           // 16
	{ "gp2-5.6n",		0x0200, 0xc7d31657, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(gaplusa)
STD_ROM_FN(gaplusa)

struct BurnDriver BurnDrvGaplusa = {
	"gaplusa", "gaplus", NULL, "gaplus", "1984",
	"Gaplus (GP2)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gaplusaRomInfo, gaplusaRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Gaplus (GP2 rev D, alternate hardware)

static struct BurnRomInfo gaplusdRomDesc[] = {
	{ "gp2-4b.8d",		0x2000, 0x484f11e0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp2-3c.8c",		0x2000, 0xa74b0266, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp2-2d.8b",		0x2000, 0x69fdfdb7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp2-8b.11d",		0x2000, 0xbff601a6, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp2-6b.11b",		0x2000, 0x14cd61ea, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp2-5.8s",		0x2000, 0xf3d19987, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp2-6.6p",		0x0200, 0x6f99c2da, 6 | BRF_GRA },           // 16
	{ "gp2-5.6n",		0x0200, 0xc7d31657, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(gaplusd)
STD_ROM_FN(gaplusd)

static INT32 GaplusdInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvGaplusd = {
	"gaplusd", "gaplus", NULL, "gaplus", "1984",
	"Gaplus (GP2 rev D, alternate hardware)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gaplusdRomInfo, gaplusdRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Gaplus (Tecfri PCB)

static struct BurnRomInfo gaplustRomDesc[] = {
	{ "gp2_4.4",		0x2000, 0xd891a70d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp2_3.3",		0x2000, 0x1df6e319, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp2_2.2",		0x2000, 0xfc764728, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp2_8.8",		0x2000, 0x9ec3dce5, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2_7.7",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp2_6.6",		0x2000, 0x6a2942c5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp2-5.8s",		0x2000, 0xf3d19987, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp2-6.6p",		0x0200, 0x6f99c2da, 6 | BRF_GRA },           // 16
	{ "gp2-5.6n",		0x0200, 0xc7d31657, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(gaplust)
STD_ROM_FN(gaplust)

struct BurnDriver BurnDrvGaplust = {
	"gaplust", "gaplus", NULL, "gaplus", "1992",
	"Gaplus (Tecfri PCB)\0", NULL, "bootleg (Tecfri)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gaplustRomInfo, gaplustRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Galaga 3 (GP3 rev. D)

static struct BurnRomInfo galaga3RomDesc[] = {
	{ "gp3-4c.8d",		0x2000, 0x10d7f64c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp3-3c.8c",		0x2000, 0x962411e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp3-2d.8b",		0x2000, 0xecc01bdb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp3-8b.11d",		0x2000, 0xf5e056d1, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp3-6b.11b",		0x2000, 0x026491b6, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp3-5.8s",		0x2000, 0x8d4dcebf, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp3-6.6p",		0x0200, 0xd48c0eef, 6 | BRF_GRA },           // 16
	{ "gp3-5.6n",		0x0200, 0x417ba0dc, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(galaga3)
STD_ROM_FN(galaga3)

struct BurnDriver BurnDrvGalaga3 = {
	"galaga3", "gaplus", NULL,"gaplus", "1984",
	"Galaga 3 (GP3 rev. D)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga3RomInfo, galaga3RomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Galaga 3 (GP3 rev. C)

static struct BurnRomInfo galaga3aRomDesc[] = {
	{ "gp3-4c.8d",		0x2000, 0x10d7f64c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp3-3c.8c",		0x2000, 0x962411e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp3-2c.8b",		0x2000, 0xf72d6fc5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp3-8b.11d",		0x2000, 0xf5e056d1, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp3-6b.11b",		0x2000, 0x026491b6, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp3-5.8s",		0x2000, 0x8d4dcebf, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp3-6.6p",		0x0200, 0xd48c0eef, 6 | BRF_GRA },           // 16
	{ "gp3-5.6n",		0x0200, 0x417ba0dc, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(galaga3a)
STD_ROM_FN(galaga3a)

struct BurnDriver BurnDrvGalaga3a = {
	"galaga3a", "gaplus", NULL, "gaplus", "1984",
	"Galaga 3 (GP3 rev. C)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga3aRomInfo, galaga3aRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Galaga 3 (GP3)

static struct BurnRomInfo galaga3bRomDesc[] = {
	{ "gp3-4.8d",		0x2000, 0x58de387c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gp3-3.8c",		0x2000, 0x94a3fd4e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gp3-2.8b",		0x2000, 0x4b1cb589, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gp3-8.11d",		0x2000, 0xd390ef28, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gp2-7.11c",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gp3-6.11b",		0x2000, 0xb36a9a2b, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.4b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gp3-5.8s",		0x2000, 0x8d4dcebf, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.11p",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.11n",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.11r",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.11m",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1p",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1n",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2n",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.6s",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "gp3-6.6p",		0x0200, 0xd48c0eef, 6 | BRF_GRA },           // 16
	{ "gp3-5.6n",		0x0200, 0x417ba0dc, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom

	{ "pal10l8.8n",		0x002c, 0x08e5b2fe, 0 | BRF_OPT },           // 19 plds
};

STD_ROM_PICK(galaga3b)
STD_ROM_FN(galaga3b)

struct BurnDriver BurnDrvGalaga3b = {
	"galaga3b", "gaplus", NULL, "gaplus", "1984",
	"Galaga 3 (GP3)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga3bRomInfo, galaga3bRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Galaga 3 (set 4)

static struct BurnRomInfo galaga3cRomDesc[] = {
	{ "gal3_9e.9e",		0x2000, 0xf4845e7f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gal3_9d.9d",		0x2000, 0x86fac687, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gal3_9c.9c",		0x2000, 0xf1b00073, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gal3_6l.6l",		0x2000, 0x9ec3dce5, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "gal3_6m.6m",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "gal3_6n.6n",		0x2000, 0x6a2942c5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.7b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gal3_9l.bin",	0x2000, 0x8d4dcebf, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.5m",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.5l",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.5k",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.5n",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1c",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1d",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2d",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.4f",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "g3_3f.3f",		0x0200, 0xd48c0eef, 6 | BRF_GRA },           // 16
	{ "g3_3e.3e",		0x0200, 0x417ba0dc, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom
};

STD_ROM_PICK(galaga3c)
STD_ROM_FN(galaga3c)

struct BurnDriver BurnDrvGalaga3c = {
	"galaga3c", "gaplus", NULL, "gaplus", "1984",
	"Galaga 3 (set 4)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga3cRomInfo, galaga3cRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Galaga 3 (set 5)

static struct BurnRomInfo galaga3mRomDesc[] = {
	{ "m1.9e",		0x2000, 0xe392704e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "m2.9d",		0x2000, 0x86fac687, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m3.9c",		0x2000, 0xf1b00073, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "m6.6l",		0x2000, 0x9ec3dce5, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code
	{ "m5.6m",		0x2000, 0x0621f7df, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "m4.6n",		0x2000, 0x6a2942c5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gp2-1.7b",		0x2000, 0xed8aa206, 3 | BRF_GRA },           //  6 M6809 #2 Code

	{ "gal3_9l.bin",	0x2000, 0x8d4dcebf, 4 | BRF_GRA },           //  7 Background Tiles

	{ "gp2-11.5m",		0x2000, 0x57740ff9, 5 | BRF_GRA },           //  8 Sprites
	{ "gp2-10.5l",		0x2000, 0x6cd8ce11, 5 | BRF_GRA },           //  9
	{ "gp2-12.5k",		0x2000, 0x7316a1f1, 5 | BRF_GRA },           // 10
	{ "gp2-9.5n",		0x2000, 0xe6a9ae67, 5 | BRF_GRA },           // 11

	{ "gp2-3.1c",		0x0100, 0xa5091352, 6 | BRF_GRA },           // 12 Color Data
	{ "gp2-1.1d",		0x0100, 0x8bc8022a, 6 | BRF_GRA },           // 13
	{ "gp2-2.2d",		0x0100, 0x8dabc20b, 6 | BRF_GRA },           // 14
	{ "gp2-7.4f",		0x0100, 0x2faa3e09, 6 | BRF_GRA },           // 15
	{ "g3_3f.3f",		0x0200, 0xd48c0eef, 6 | BRF_GRA },           // 16
	{ "g3_3e.3e",		0x0200, 0x417ba0dc, 6 | BRF_GRA },           // 17

	{ "gp2-4.3f",		0x0100, 0x2d9fbdd8, 7 | BRF_GRA },           // 18 Sound Prom
};

STD_ROM_PICK(galaga3m)
STD_ROM_FN(galaga3m)

struct BurnDriver BurnDrvGalaga3m = {
	"galaga3m", "gaplus", NULL, "gaplus", "1984",
	"Galaga 3 (set 5)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaga3mRomInfo, galaga3mRomName, NULL, NULL, GaplusSampleInfo, GaplusSampleName, GaplusInputInfo, GaplusDIPInfo,
	GaplusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};
