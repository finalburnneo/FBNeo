// FB Neo Atari Tunnel Hunt driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "pokey.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvSpriteRAM;

static UINT8 lineh[16];
static UINT8 shl0st = 0;
static UINT8 shl1st = 0;
static UINT8 vstrlo = 0;
static UINT8 shl0pc = 0;
static UINT8 shl1pc = 0;
static UINT8 linec[16];
static UINT8 shl0v = 0;
static UINT8 shl1v = 0;
static UINT8 mobjh = 0;
static UINT8 linev[16];
static UINT8 shl0vs = 0;
static UINT8 shl1vs = 0;
static UINT8 linevs[16];
static UINT8 shel0h = 0;
static UINT8 shel1h = 0;
static UINT8 mobjv = 0;
static UINT8 control = 0;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[4];
static INT16 Analog[2];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo TunhuntInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"		},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"		},
	A("P1 Stick X",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Stick Y",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),

	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 0,	"tilt"			},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dips C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
	{"Dips D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"			},
};

STDINPUTINFO(Tunhunt)

#undef A

static struct BurnDIPInfo TunhuntDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x80, NULL					},
	{0x01, 0xff, 0xff, 0x02, NULL					},
	{0x02, 0xff, 0xff, 0x03, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					}, // fake, for language split

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x04, 0x00, "Off"					},
	{0x00, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x03, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0x03, 0x02, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0x03, 0x01, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin 2 Multiplier"	},
	{0x01, 0x01, 0x0c, 0x00, "1"					},
	{0x01, 0x01, 0x0c, 0x04, "4"					},
	{0x01, 0x01, 0x0c, 0x08, "5"					},
	{0x01, 0x01, 0x0c, 0x0c, "6"					},

	{0   , 0xfe, 0   ,    2, "Coin 1 Multiplier"	},
	{0x01, 0x01, 0x10, 0x00, "1"					},
	{0x01, 0x01, 0x10, 0x10, "2"					},

	{0   , 0xfe, 0   ,    4, "Bonus Credits"		},
	{0x01, 0x01, 0x60, 0x00, "None"					},
	{0x01, 0x01, 0x60, 0x60, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0x60, 0x40, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0x60, 0x20, "2 Coins/1 Credit"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x02, 0x01, 0x11, 0x00, "None"					},
	{0x02, 0x01, 0x11, 0x10, "30K, 100K"			},
	{0x02, 0x01, 0x11, 0x01, "60K, 100K"			},
	{0x02, 0x01, 0x11, 0x11, "90K, 100K"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x02, 0x01, 0x06, 0x00, "2"					},
	{0x02, 0x01, 0x06, 0x02, "3"					},
	{0x02, 0x01, 0x06, 0x04, "4"					},
	{0x02, 0x01, 0x06, 0x06, "5"					},

	{0   , 0xfe, 0   ,    4, "Language"				}, // decoded in get_lang()
	{0x03, 0x01, 0x03, 0x00, "English"				},
	{0x03, 0x01, 0x03, 0x01, "German"				},
	{0x03, 0x01, 0x03, 0x02, "French"				},
	{0x03, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    2, "Stick Y Type"			}, // fake option
	{0x03, 0x01, 0x10, 0x00, "Normal"				},
	{0x03, 0x01, 0x10, 0x10, "Inverted"				},
};

STDDIPINFO(Tunhunt)

static void tunhunt_write(UINT16 address, UINT8 data)
{
	if (address >= 0x1083 && address <= 0x108f) {
		lineh[address - 0x1083] = data;
		return;
	}

	if (address >= 0x1283 && address <= 0x128f) {
		linec[address - 0x1283] = data;
		return;
	}

	if (address >= 0x1403 && address <= 0x140f) {
		linev[address - 0x1403] = data;
		return;
	}

	if (address >= 0x1483 && address <= 0x148f) {
		linevs[address - 0x1483] = data;
		return;
	}

	if (address >= 0x1600 && address <= 0x160f) {
		BurnPalRAM[address & 0xf] = data;
		return;
	}

	if (address >= 0x3000 && address <= 0x300f) {
		pokey_write(0, address & 0xf, data);
		return;
	}

	if (address >= 0x4000 && address <= 0x400f) {
		pokey_write(1, address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x1080: /*mobsc0 = data;*/ return;
		case 0x1081: /*mobsc1 = data;*/ return;

		case 0x1200: shl0st = data; return;
		case 0x1201: shl1st = data; return;
		case 0x1202: vstrlo = data; return;
		case 0x1203: /*linesh = data;*/ return;
		case 0x1280: shl0pc = data; return;
		case 0x1281: shl1pc = data; return;

		case 0x1400: shl0v = data; return;
		case 0x1401: shl1v = data; return;
		case 0x1402: mobjh = data; return;

		case 0x1480: shl0vs = data; return;
		case 0x1481: shl1vs = data; return;
		case 0x1482: /*mobvs = data;*/ return;

		case 0x1800: shel0h = data; return;
		case 0x1802: /*mobst = data;*/ return;

		case 0x1a00: shel1h = data; return;

		case 0x1c00: mobjv = data; return;

		case 0x2000: return; // watchdog
		case 0x2400: return; // nop
		case 0x2800: // control
			control = data;
			// coin counter = data & 3
			// led = data & 0x40
		return;
	}
}

static UINT8 tunhunt_read(UINT16 address)
{
	if (address >= 0x2000 && address <= 0x2007) {
		UINT8 ret = ((DrvInputs[0] & 0x7b) | (vblank ? 0 : 0x80) | (DrvDips[0] & 0x04)) ^ 0xff;
		return (ret << (7 - (address & 7))) & 0x80;
	}

	if (address >= 0x3000 && address <= 0x300f) {
		return pokey_read(0, address & 0xf);
	}

	if (address >= 0x4000 && address <= 0x400f) {
		return pokey_read(1, address & 0xf);
	}

	return 0;
}

static UINT16 get_lang()
{
	switch (DrvDips[3]) {
		case 0: return 0x0000;
		case 1: return 0x0080;
		case 2: return 0x0800;
		case 3: return 0x0880;
	}
	return 0;
}

static INT32 pokey_dip_read(INT32 )
{
	return DrvDips[1] | (get_lang() & 0x80);
}

static INT32 pokey_analog1_read(INT32 )
{
	UINT8 ret = ProcessAnalog(Analog[1], (DrvDips[3] & 0x10) ? 1 : 0, INPUT_DEADZONE, 0x00, 0xff);

	return ret;
}

static INT32 pokey_analog2_read(INT32 )
{
	return ProcessAnalog(Analog[0], 1/*reverse*/, INPUT_DEADZONE, 0x00, 0xff);
}

static INT32 pokey_dsw2_0100_read(INT32 ) { return (DrvDips[2] & 0x01) ? 0x80 : 0; }
static INT32 pokey_dsw2_0200_read(INT32 ) { return (DrvDips[2] & 0x02) ? 0x80 : 0; }
static INT32 pokey_dsw2_0400_read(INT32 ) { return (DrvDips[2] & 0x04) ? 0x80 : 0; }
static INT32 pokey_dsw2_0800_read(INT32 ) { return (get_lang() & 0x0800) ? 0x80 : 0; }
static INT32 pokey_dsw2_1000_read(INT32 ) { return (DrvDips[2] & 0x10) ? 0x80 : 0; }

static tilemap_callback( fg )
{
	INT32 attr = DrvVideoRAM[offs];

	TILE_SET_INFO(0, attr, attr >> 6, (attr & 0xc0) ? TILE_OPAQUE : 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	PokeyReset();

	memset (lineh, 0, 16);
	memset (linec, 0, 16);
	memset (linev, 0, 16);
	memset (linevs, 0, 16);
	shl0st = 0;
	shl1st = 0;
	vstrlo = 0;
	shl0pc = 0;
	shl1pc = 0;
	shl0v = 0;
	shl1v = 0;
	mobjh = 0;
	shl0vs = 0;
	shl1vs = 0;
	shel0h = 0;
	shel1h = 0;
	mobjv = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x003000;

	DrvGfxROM[0]	= Next; Next += 0x001000;
	DrvGfxROM[1]	= Next; Next += 0x001000;

	BurnPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000400;
	DrvVideoRAM		= Next; Next += 0x000100;
	DrvSpriteRAM	= Next; Next += 0x000400;
	BurnPalRAM		= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(16+8,1) };
	INT32 YOffs0[8] = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x400);

	GfxDecode(0x0040, 1,  8,  8, Plane, XOffs, YOffs0, 0x080, tmp + 0x000, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x400);

	GfxDecode(0x0010, 1, 16, 16, Plane, XOffs, YOffs1, 0x200, tmp + 0x000, DrvGfxROM[1]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x1800, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x2800, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0200, k++, 1)) return 1;

	//	if (BurnLoadRom(DrvGfxROM[2] + 0x0000, k++, 1)) return 1;

	//	if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM   + 0x0020, k++, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM   + 0x0040, k++, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM   + 0x0140, k++, 1)) return 1;

		DrvGfxDecode();
	}

 	M6502Init(0, TYPE_M6502);
 	M6502Open(0);
 	M6502SetAddressMask(0x7fff);
 	M6502MapMemory(DrvM6502RAM,			0x0000, 0x03ff, MAP_RAM);
 	M6502MapMemory(DrvVideoRAM,			0x1e00, 0x1eff, MAP_RAM);
 	M6502MapMemory(DrvSpriteRAM,		0x2c00, 0x2fff, MAP_WRITE);
 	M6502MapMemory(DrvM6502ROM,			0x5000, 0x7fff, MAP_ROM);
 	M6502SetWriteHandler(tunhunt_write);
 	M6502SetReadHandler(tunhunt_read);
 	M6502Close();

	PokeyInit(12096000/10, 2, 0.30, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeySetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	PokeySetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	PokeyAllPotCallback(0, pokey_dip_read);
	PokeyPotCallback(1, 0, pokey_analog1_read);
	PokeyPotCallback(1, 1, pokey_analog2_read);
	PokeyPotCallback(1, 2, pokey_dsw2_0100_read);
	PokeyPotCallback(1, 3, pokey_dsw2_0200_read);
	PokeyPotCallback(1, 4, pokey_dsw2_0400_read);
	PokeyPotCallback(1, 5, pokey_dsw2_0800_read);
	PokeyPotCallback(1, 6, pokey_dsw2_1000_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, fg_map_callback, 8, 8, 8, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1,  8,  8, 0x1000, 0x10, 3);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 1, 16, 16, 0x1000, 0x18, 0);
	GenericTilemapSetOffsets(0, -64, 0);
	GenericTilemapSetTransparent(0, 0);
	BurnBitmapAllocate(1, 256, 64, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	PokeyExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	static const UINT8 palette_lut[0x10][3] = {
		{ 0xff, 0xff, 0xff }, { 0xff, 0x00, 0xff }, { 0x00, 0x00, 0xff }, { 0x00, 0xff, 0xff },
		{ 0x00, 0xff, 0x00 }, { 0xff, 0xff, 0x00 }, { 0xff, 0x00, 0x00 }, { 0x00, 0x00, 0x00 },
		{ 0xff, 0x7f, 0x00 }, { 0x7f, 0xff, 0x00 }, { 0x00, 0xff, 0x7f }, { 0x00, 0x7f, 0xff },
		{ 0xff, 0x00, 0x7f }, { 0x7f, 0x00, 0xff }, { 0xff, 0xaa, 0xaa }, { 0xaa, 0xaa, 0xff }
	};

	for (INT32 i = 0; i < 16; i++)
	{
		INT32 color =  BurnPalRAM[i] & 0xf;
		INT32 shade = (BurnPalRAM[i] >> 4) ^ 0xf;
		INT32 r = (palette_lut[color][0] * shade) / 15;
		INT32 g = (palette_lut[color][1] * shade) / 15;
		INT32 b = (palette_lut[color][2] * shade) / 15;
		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_box()
{
#if 0
	for (INT32 y = 0; y < 256; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			INT32 color = 0;
			INT32 z = 0;
			for (INT32 span = 0; span < 13; span++)
			{
				INT32 x0 = lineh[span];
				INT32 y0 = linevs[span];
				INT32 y1 = linev[span];

				if (y >= y0 && y <= y1 && x >= x0 && x0 >= z)
				{
					color = linec[span] & 0xf;
					z = x0; // give priority to rightmost spans
				}

				draw_plot_pixel(0, x, 0xff - y, color);
			}
		}
	}
#endif
	for (int y = 0; y < 256; y++)
	{
		if (0xff - y >= 0 && 0xff - y <= 239)
			for (int x = 0; x < 256; x++)
			{
				int color = 0;
				int z = 0;
				for (int span = 0; span < 13; span++)
				{
					int const x0 = lineh[span];
					int const y0 = linevs[span];
					int const y1 = linev[span];

					if (y >= y0 && y <= y1 && x >= x0 && x0 >= z)
					{
						color = linec[span] & 0xf;
						z = x0; // give priority to rightmost spans
					}
				}
				if (x >= 0 && x <= 255)
					draw_plot_pixel(0, x, 0xff - y, color);
			}
	}

}

static void draw_shell(INT32 code, INT32 hposition, INT32 vstart, INT32 /*vstop*/, INT32 /*vstretch*/, INT32 hstretch)
{
	if (hstretch)
	{
		for (INT32 sx = 0; sx < 256; sx += 16)
		{
			for (INT32 sy = 0; sy < 240; sy += 16)
			{
				DrawGfxMaskTile(0, 1, code, sx, sy, 0, 0, 0, 0);
			}
		}
	}
	else
		DrawGfxMaskTile(0, 1, code, 255 - hposition - 16, vstart - 32, 0, 0, 0, 0);
}

static inline void copy_roz(UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy)
{
	UINT16 *dst = pTransDraw;
	UINT16 *src = BurnBitmapGetPosition(1, 0, 0);

	for (INT32 sy = 0; sy < nScreenHeight; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++)
		{
			if ((cy >> 16) < 64 && (cx >> 16) < 256)
			{
				INT32 pxl = src[(((cy >> 16) & 0x3f) << 8) + ((cx >> 16) & 0xff)];

				if (pxl) {
					*dst = pxl;
				}
			}
		}
	}
}

static void draw_motion_object()
{
	INT32 x0 = 255 - mobjv;
	INT32 y0 = 255 - mobjh;

	for (INT32 line = 0, x = 0; line < 64; line++, x = 0)
	{
		UINT8 *const source = &DrvSpriteRAM[line * 0x10];
		UINT16 *dest = BurnBitmapGetPosition(1, 0, line);
		memset (dest, 0, 256*sizeof(dest));

		for (INT32 span = 0; span < 0x10; span++)
		{
			INT32 span_data = source[span];
			if (span_data == 0xff) break;
			INT32 color = ((span_data >> 6) & 0x3) ^ 0x3;
			INT32 count = (span_data & 0x1f) + 1;
			while (count-- && x < 256) {
				dest[x++] = color;
			}
		}
	}

	UINT32 scaley = 0x10000;

	switch (vstrlo)
	{
		case 0x01:
			scaley *= 0.33; // seems correct
		break;

		case 0x02:
			scaley *= 0.50; // seems correct
		break;

		default:
			scaley *= vstrlo / 4; // ???
		break;
	}

	copy_roz(-x0 * 0x10000, -y0 * scaley, 0x10000, 0, 0, scaley);
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteUpdate();
		BurnRecalc = 1;
	}

	//UINT16 tmp[256 * sizeof(UINT16)];

	static const INT32 color_lut[0x20] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		0, 4, 5, 4, 6, 4, 15,4, 0, 4,  1,  2,  3,  4,  5,  6
	};

	BurnTransferClear();

	if (nBurnLayer & 1) draw_box();

	if (nSpriteEnable & 1) draw_motion_object();

	if (nSpriteEnable & 2) draw_shell(shl0pc|8, shel0h, shl0v, shl0vs, shl0st, control & 0x08);
	if (nSpriteEnable & 4) draw_shell(shl1pc, shel1h, shl1v, shl1vs, shl1st, control & 0x10);

	GenericTilesSetClip(192, -1, -1, -1);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilesClearClip();

	// apply color lut
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = color_lut[pTransDraw[i]&0x1f];
	}

	// flip vertical
	BurnTransferFlip(0, 1);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

    M6502NewFrame();

	{
		DrvInputs[0] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 2016000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6502);

		if ((i & 0x3f) == 0x3f) M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD); // 4x per frame
		if (i == 239) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			vblank = 1;
		}

		if (pBurnSoundOut && (i%4)==3) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		M6502Scan(nAction);
		pokey_scan(nAction, pnMin);

		SCAN_VAR(lineh);
		SCAN_VAR(linec);
		SCAN_VAR(linev);
		SCAN_VAR(linevs);
		SCAN_VAR(shl0st);
		SCAN_VAR(shl1st);
		SCAN_VAR(vstrlo);
		SCAN_VAR(shl0pc);
		SCAN_VAR(shl1pc);
		SCAN_VAR(shl0v);
		SCAN_VAR(shl1v);
		SCAN_VAR(mobjh);
		SCAN_VAR(shl0vs);
		SCAN_VAR(shl1vs);
		SCAN_VAR(shel0h);
		SCAN_VAR(shel1h);
		SCAN_VAR(mobjv);
		SCAN_VAR(control);
	}

	return 0;
}


// Tunnel Hunt

static struct BurnRomInfo tunhuntRomDesc[] = {
	{ "001.lm1",		0x000800, 0x2601a3a4,  1 | BRF_PRG | BRF_ESS },	//  0 M6502 Code
	{ "002.k1",			0x000800, 0x29bbf3df,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "136000.103",		0x000800, 0x1a6a60a4,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "004.fh1",		0x000800, 0x4d6c920e,  1 | BRF_PRG | BRF_ESS },	//  3 
	{ "005.ef1",		0x000800, 0xe17badf0,  1 | BRF_PRG | BRF_ESS },	//  4 
	{ "006.d1",			0x000800, 0xc3ae8519,  1 | BRF_PRG | BRF_ESS },	//  5 

	{ "019.c10",		0x000400, 0xd6fd45a9,  2 | BRF_GRA },			//  6 Characters

	{ "016.a8",			0x000200, 0x830e6c34,  3 | BRF_GRA },			//  7 Sprites
	{ "017.b8",			0x000200, 0x5bef8b5a,  3 | BRF_GRA },			//  8 

	{ "013.d11",		0x000020, 0x66f1f5eb,  4 | BRF_GRA },			//  9 PROMs
	{ "014.c11",		0x000020, 0x662444b2,  4 | BRF_GRA },			// 10 
	{ "015.n4",			0x000100, 0x00e224a0,  4 | BRF_GRA },			// 11 
	{ "018.h9",			0x000400, 0x6547c208,  4 | BRF_GRA },			// 12 
};

STD_ROM_PICK(tunhunt)
STD_ROM_FN(tunhunt)

struct BurnDriver BurnDrvTunhunt = {
	"tunhunt", NULL, NULL, NULL, "1979",
	"Tunnel Hunt\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tunhuntRomInfo, tunhuntRomName, NULL, NULL, NULL, NULL, TunhuntInputInfo, TunhuntDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x10,
	240, 256, 3, 4
};


// Tunnel Hunt (Centuri)

static struct BurnRomInfo tunhuntcRomDesc[] = {
	{ "001.lm1",		0x000800, 0x2601a3a4,  1 | BRF_PRG | BRF_ESS },	//  0 M6502 Code
	{ "002.k1",			0x000800, 0x29bbf3df,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "003.j1",			0x000800, 0x360c0f47,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "004.fh1",		0x000800, 0x4d6c920e,  1 | BRF_PRG | BRF_ESS },	//  3 
	{ "005.ef1",		0x000800, 0xe17badf0,  1 | BRF_PRG | BRF_ESS },	//  4 
	{ "006.d1",			0x000800, 0xc3ae8519,  1 | BRF_PRG | BRF_ESS },	//  5 

	{ "019.c10",		0x000400, 0xd6fd45a9,  2 | BRF_GRA },			//  6 Characters

	{ "016.a8",			0x000200, 0x830e6c34,  3 | BRF_GRA },			//  7 Sprites
	{ "017.b8",			0x000200, 0x5bef8b5a,  3 | BRF_GRA },			//  8 

	{ "013.d11",		0x000020, 0x66f1f5eb,  4 | BRF_GRA },			//  9 PROMs
	{ "014.c11",		0x000020, 0x662444b2,  4 | BRF_GRA },			// 10 
	{ "015.n4",			0x000100, 0x00e224a0,  4 | BRF_GRA },			// 11 
	{ "018.h9",			0x000400, 0x6547c208,  4 | BRF_GRA },			// 12 
};

STD_ROM_PICK(tunhuntc)
STD_ROM_FN(tunhuntc)

struct BurnDriver BurnDrvTunhuntc = {
	"tunhuntc", "tunhunt", NULL, NULL, "1981",
	"Tunnel Hunt (Centuri)\0", NULL, "Atari (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tunhuntcRomInfo, tunhuntcRomName, NULL, NULL, NULL, NULL, TunhuntInputInfo, TunhuntDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x10,
	240, 256, 3, 4
};
