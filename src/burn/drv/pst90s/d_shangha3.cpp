// FB Alpha Shanghai 3 / Hebereke no Popoon / Blocken driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2612.h"
#include "msm6295.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxExp;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gfx_list;
static INT32 flipscreen;
static INT32 okibank;
static INT32 prot_counter;
static INT32 soundlatch;

static INT32 shadow_table[16];
static INT32 game_type = 0;
static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo Shangha3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 5,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Shangha3)

static struct BurnInputInfo HeberpopInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 5,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Heberpop)

static struct BurnInputInfo BlockenInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Blocken)

static struct BurnDIPInfo Shangha3DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xbf, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x13, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x13, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x14, 0x01, 0x03, 0x01, "Easy"						},
	{0x14, 0x01, 0x03, 0x03, "Normal"					},
	{0x14, 0x01, 0x03, 0x02, "Hard"						},
	{0x14, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Base Time"				},
	{0x14, 0x01, 0x0c, 0x04, "70 sec"					},
	{0x14, 0x01, 0x0c, 0x0c, "80 sec"					},
	{0x14, 0x01, 0x0c, 0x08, "90 sec"					},
	{0x14, 0x01, 0x0c, 0x00, "100 sec"					},

	{0   , 0xfe, 0   ,    4, "Additional Time"			},
	{0x14, 0x01, 0x30, 0x10, "4 sec"					},
	{0x14, 0x01, 0x30, 0x30, "5 sec"					},
	{0x14, 0x01, 0x30, 0x20, "6 sec"					},
	{0x14, 0x01, 0x30, 0x00, "7 sec"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x14, 0x01, 0x40, 0x40, "Off"						},
	{0x14, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Shangha3)

static struct BurnDIPInfo HeberpopDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Very Easy"				},
	{0x13, 0x01, 0x03, 0x01, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Allow Diagonal Moves"		},
	{0x13, 0x01, 0x10, 0x00, "No"						},
	{0x13, 0x01, 0x10, 0x10, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x20, 0x00, "Off"						},
	{0x13, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x14, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x14, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
};

STDDIPINFO(Heberpop)

static struct BurnDIPInfo BlockenDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbf, NULL						},
	{0x15, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x01, 0x01, "Off"						},
	{0x14, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x14, 0x01, 0x06, 0x04, "Easy"						},
	{0x14, 0x01, 0x06, 0x06, "Normal"					},
	{0x14, 0x01, 0x06, 0x02, "Hard"						},
	{0x14, 0x01, 0x06, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Game Type"				},
	{0x14, 0x01, 0x08, 0x08, "A"						},
	{0x14, 0x01, 0x08, 0x00, "B"						},

	{0   , 0xfe, 0   ,    4, "Players"					},
	{0x14, 0x01, 0x30, 0x30, "1"						},
	{0x14, 0x01, 0x30, 0x20, "2"						},
	{0x14, 0x01, 0x30, 0x10, "3"						},
	{0x14, 0x01, 0x30, 0x00, "4"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x14, 0x01, 0x40, 0x40, "Off"						},
	{0x14, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x15, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x15, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x15, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x15, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x15, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x00, "5 Coins 3 Credits"		},
	{0x15, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x15, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x15, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
};

STDDIPINFO(Blocken)

static void RenderZoomedShadowTile(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 *t_table, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	INT32 dh = (zoomy * height + 0x8000) / 0x10000;
	INT32 dw = (zoomx * width + 0x8000) / 0x10000;

	if (dw && dh)
	{
		INT32 dx = (width * 0x10000) / dw;
		INT32 dy = (height * 0x10000) / dh;
		INT32 ex = sx + dw;
		INT32 ey = sy + dh;
		INT32 x_index_base = 0;
		INT32 y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16]; // 0x6fffff max
						INT32 trans = t_table[pxl];

						if (trans == 0) dst[x] = pxl | color;
						else if (trans == 2) dst[x] |= 0x800;
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

static void blitter_write()
{
	UINT16 *ram = (UINT16*)Drv68KRAM;
	UINT16 *bitmap = BurnBitmapGetBitmap(1);

	for (INT32 offs = gfx_list * 8; offs < 0x10000/2; offs += 16)
	{
		INT32 code	=  BURN_ENDIAN_SWAP_INT16(ram[offs+1]);
		INT32 sx	= (BURN_ENDIAN_SWAP_INT16(ram[offs+2]) & 0x1ff0) >> 4;
		INT32 sy	= (BURN_ENDIAN_SWAP_INT16(ram[offs+3]) & 0x1ff0) >> 4;
		INT32 flipx	=  BURN_ENDIAN_SWAP_INT16(ram[offs+4]) & 0x01;
		INT32 flipy	=  BURN_ENDIAN_SWAP_INT16(ram[offs+4]) & 0x02;
		INT32 color	=  BURN_ENDIAN_SWAP_INT16(ram[offs+5]) & 0x7f;
		INT32 sizex	=  BURN_ENDIAN_SWAP_INT16(ram[offs+6]);
		INT32 sizey	=  BURN_ENDIAN_SWAP_INT16(ram[offs+7]);
		INT32 zoomx	=  BURN_ENDIAN_SWAP_INT16(ram[offs+10]);
		INT32 zoomy	=  BURN_ENDIAN_SWAP_INT16(ram[offs+13]);

		if (sx >= 0x180) sx -= 0x200;
		if (sy >= 0x100) sy -= 0x200;

		if (flipscreen)
		{
			sx = 383 - sx - sizex;
			sy = 255 - sy - sizey;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16; // adjust for clipping

		if ((sizex || sizey) && sizex < 512 && sizey < 256 && zoomx < 0x1f0 && zoomy < 0x1f0)
		{
			GenericTilesSetClip(sx, sx + sizex+1, sy, sy + sizey+1);

			if (BURN_ENDIAN_SWAP_INT16(ram[offs+4]) & 0x08)
			{
				INT32 condensed = BURN_ENDIAN_SWAP_INT16(ram[offs+4]) & 0x04;

				INT32 srcx = BURN_ENDIAN_SWAP_INT16(ram[offs+8])/16;
				INT32 srcy = BURN_ENDIAN_SWAP_INT16(ram[offs+9])/16;
				INT32 dispx = srcx & 0x0f;
				INT32 dispy = srcy & 0x0f;

				INT32 h = (sizey+15)/16+1;
				INT32 w = (sizex+15)/16+1;

				if (condensed)
				{
					h *= 2;
					w *= 2;
					srcx /= 8;
					srcy /= 8;
				}
				else
				{
					srcx /= 16;
					srcy /= 16;
				}

				for (INT32 y = 0;y < h;y++)
				{
					for (INT32 x = 0;x < w;x++)
					{
						INT32 dx,dy,tile;

						if (condensed)
						{
							INT32 addr = ((y+srcy) & 0x1f) + 0x20 * ((x+srcx) & 0xff);
							tile = BURN_ENDIAN_SWAP_INT16(ram[addr]);
							dx = 8*x*(0x200-zoomx)/0x100 - dispx;
							dy = 8*y*(0x200-zoomy)/0x100 - dispy;
						}
						else
						{
							INT32 addr = ((y+srcy) & 0x0f) + 0x10 * ((x+srcx) & 0xff) + 0x100 * ((y+srcy) & 0x10);
							tile = BURN_ENDIAN_SWAP_INT16(ram[addr]);
							dx = 16*x*(0x200-zoomx)/0x100 - dispx;
							dy = 16*y*(0x200-zoomy)/0x100 - dispy;
						}

						if (flipx) dx = sx + sizex-15 - dx;
						else dx = sx + dx;
						if (flipy) dy = sy + sizey-15 - dy;
						else dy = sy + dy;

						INT32 tileno = ((tile & 0x0fff) | (code & 0xf000)) % 0x7000;

						Draw16x16MaskTile(bitmap, tileno, dx, dy, flipx, flipy, ((tile >> 12) | (color & 0x70))&0x7f, 4, 0xf, 0, DrvGfxExp);
					}
				}
			}
			else
			{
				if (zoomx <= 1 && zoomy <= 1)
					RenderZoomedShadowTile(bitmap, DrvGfxExp, code % 0x7000, color << 4, shadow_table, sx, sy, flipx, flipy, 16, 16, 0x1000000, 0x1000000);
				else
				{
					INT32 w = (sizex+15)/16;

					for (INT32 x = 0;x < w;x++)
					{
						RenderZoomedShadowTile(bitmap, DrvGfxExp, code % 0x7000, color << 4, shadow_table, sx + 16 * x, sy, flipx, flipy, 16, 16, (0x200-zoomx)*0x100, (0x200-zoomy)*0x100);

						if ((code & 0x000f) == 0x0f)
							code = (code + 0x100) & 0xfff0;
						else
							code++;
					}
				}
			}

			GenericTilesClearClip();
		}
	}
}

static void oki_bank(INT32 data)
{
	okibank = data;

	if (game_type == 1)
	{
		MSM6295SetBank(0, DrvSndROM + okibank * 0x40000, 0x00000, 0x3ffff);
	}
	else if (game_type == 2)
	{
		MSM6295SetBank(0, DrvSndROM + okibank * 0x20000, 0x20000, 0x3ffff);
	}
}

static void __fastcall shangha3_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xf00000) == 0x100000) address += 0x100000; // blocken

	switch (address)
	{
		case 0x200008:
			blitter_write();
		return;

		case 0x20000a:
		return; // nop

		case 0x20000c:
			if (game_type == 1) oki_bank((data & 0x08) >> 3);
			if (game_type == 2) oki_bank((data & 0x30) >> 4);
		return;

		case 0x20000e: // heberpop
			if (game_type != 0)
			{
				soundlatch = data;
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		return;

		case 0x20002e:
			AY8910Write(0, 1, data);
		return;

		case 0x20003e:
			AY8910Write(0, 0, data);
		return;

		case 0x20004e:
			// prot_w
		return;

		case 0x20006e:
			MSM6295Write(0, data);
		return;

		case 0x340000:
			flipscreen = data & 0x80;
		return;

		case 0x360000:
			gfx_list = data;
		return;
	}
}

static void __fastcall shangha3_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x20000c: // shangha3 coin control
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address,data);
}

static UINT16 __fastcall shangha3_main_read_word(UINT32 address)
{
	if ((address & 0xf00000) == 0x100000) address += 0x100000; // blocken

	switch (address)
	{
		case 0x200000:
			return (DrvInputs[0] & 0x7f7f) ^ (vblank ? 0x8080 : 0); // vblank for heberpop & blocken

		case 0x200002:
			return (DrvInputs[1] & 0x007f) ^ (vblank ? 0x80 : 0); // vblank for shangha3

		case 0x200004: // heberpop
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x20001e:
			return AY8910Read(0);

		case 0x20004e:
		{
			UINT8 ret = ((0x0f << prot_counter) >> 4) & 0xf;
			prot_counter = (prot_counter + 1) % 9;
			return ret;
		}

		case 0x20006e:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall shangha3_main_read_byte(UINT32 address)
{
	if ((address & 0xf00000) == 0x100000) address += 0x100000; // blocken

	switch (address)
	{
		case 0x200003:
			return DrvInputs[1] ^ (vblank ? 0x80 : 0); // vblank for shangha3

		case 0x200004:
			return DrvDips[1];

		case 0x200005:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall heberpop_sound_write(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			BurnYM3438Write(0, port & 3, data);
		return;

		case 0x80:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall heberpop_sound_read(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			return BurnYM3438Read(0, port & 3);

		case 0x80:
			return MSM6295Read(0);

		case 0xc0:
			return soundlatch;
	}

	return 0;
}

static UINT8 AY8910_portA(UINT32)
{
	return DrvDips[0];
}

static UINT8 AY8910_portB(UINT32)
{
	return DrvDips[1];
}

inline static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0x20, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekReset(0);

	oki_bank(1);

	ZetOpen(0);
	ZetReset();
	BurnYM3438Reset();
	MSM6295Reset(0);
	AY8910Reset(0);
	ZetSetVector(0xff);
	ZetClose();

	soundlatch = 0;
	prot_counter = 0;
	flipscreen = 0;
	gfx_list = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM	= Next; Next += 0x380000;
	DrvGfxExp	= Next; Next += 0x700000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM	= Next; Next += 0x000800;
	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x001000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		memset (Drv68KROM, 0xff, 0x100000);
		memset (DrvGfxROM, 0xff, 0x380000);

		if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

		if (game_select == 0) // shangha3
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			game_type = 0;
		}
		else if (game_select == 1) // shangha3u
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			game_type = 0;
		}
		else if (game_select == 2) // shangha3up
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			game_type = 0;
		}
		else if (game_select == 3) // heberpop
		{
			if (BurnLoadRom(DrvZ80ROM + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x100000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x180000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x280000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x300000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			game_type = 1;
		}
		else if (game_select == 4) // blocken
		{
			if (BurnLoadRom(DrvZ80ROM + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x080000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

			game_type = 2;
		}

		BurnNibbleExpand(DrvGfxROM, DrvGfxExp, 0x380000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	if (game_select == 4) { // blocken
		SekMapMemory(DrvPalRAM,	0x200000, 0x200fff, MAP_RAM);
	} else {
		SekMapMemory(DrvPalRAM,	0x100000, 0x100fff, MAP_RAM);
	}
	SekMapMemory(Drv68KRAM,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(DrvGfxROM,		0x800000, 0xb7ffff, MAP_ROM);
	SekSetWriteWordHandler(0,	shangha3_main_write_word);
	SekSetWriteByteHandler(0,	shangha3_main_write_byte);
	SekSetReadWordHandler(0,	shangha3_main_read_word);
	SekSetReadByteHandler(0,	shangha3_main_read_byte);
	SekClose();

	// blocken / heberpop only
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xffff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(heberpop_sound_write);
	ZetSetInHandler(heberpop_sound_read);
	ZetClose();

	// shangha3 only
	AY8910Init(0, 1500000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, &AY8910_portA, &AY8910_portB, NULL, NULL);

	// blocken / heberpop only
	BurnYM3438Init(1, 8000000, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig,6000000);
	BurnYM3438SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	BurnBitmapAllocate(1, 384, 224, true);

	for (INT32 i = 0; i < 14; i++) shadow_table[i] = 0;
	shadow_table[14] = !game_type ? 2 : 0; // shadow
	shadow_table[15] = 1; // transparent

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();
	AY8910Exit(0);
	MSM6295Exit(0);
	MSM6295ROM = NULL;
	BurnYM3438Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(pal[i]) >> 11) & 0x1f;
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(pal[i]) >>  6) & 0x1f;
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(pal[i]) >>  1) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);

		r = (r * 157) / 255;
		g = (g * 157) / 255;
		b = (b * 157) / 255;

		DrvPalette[0x800 + i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}

	BurnBitmapCopy(1, pTransDraw, NULL, 0, 0, 0, -1);

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262/2;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (240 / 2)) {
			vblank = 1;
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		if (game_type != 0) {
			BurnTimerUpdate((i + 1) * nCyclesTotal[1] / nInterleave);
		}
	}

	if (game_type != 0) BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		if (game_type == 0) AY8910Render(pBurnSoundOut, nBurnSoundLen);
		if (game_type != 0) BurnYM3438Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

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

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = (UINT8*)BurnBitmapGetBitmap(1);
		ba.nLen	  = 384 * 224 * sizeof(UINT16);
		ba.szName = "Blitter Buffer";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3438Scan(nAction, pnMin);
		AY8910Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(gfx_list);
		SCAN_VAR(flipscreen);
		SCAN_VAR(okibank);
		SCAN_VAR(prot_counter);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE) {
		oki_bank(okibank);
	}

	return 0;
}


// Shanghai III (World)

static struct BurnRomInfo shangha3RomDesc[] = {
	{ "ic3",								0x040000, 0x4a9cdcfd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic2",								0x040000, 0x714bfdbc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s3j_char-a1.ic43",					0x200000, 0x2dbf9d17, 2 | BRF_GRA },           //  2 Graphics
	{ "27c4000.ic44",						0x080000, 0x6344ffb7, 2 | BRF_GRA },           //  3

	{ "s3j_v10.ic75",						0x040000, 0xf0cdc86a, 3 | BRF_SND },           //  4 Samples
};

STD_ROM_PICK(shangha3)
STD_ROM_FN(shangha3)

static INT32 Shangha3Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvShangha3 = {
	"shangha3", NULL, NULL, NULL, "1993",
	"Shanghai III (World)\0", NULL, "Sunsoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, shangha3RomInfo, shangha3RomName, NULL, NULL, NULL, NULL, Shangha3InputInfo, Shangha3DIPInfo,
	Shangha3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Shanghai III (US)

static struct BurnRomInfo shangha3uRomDesc[] = {
	{ "ic3.ic3",							0x080000, 0x53ef4988, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic2.ic2",							0x080000, 0xfdea0232, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s3j_char-a1.ic43",					0x200000, 0x2dbf9d17, 2 | BRF_GRA },           //  2 Graphics

	{ "ic75.ic75",							0x080000, 0xa8136d8c, 3 | BRF_SND },           //  3 Samples
};

STD_ROM_PICK(shangha3u)
STD_ROM_FN(shangha3u)

static INT32 Shangha3uInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvShangha3u = {
	"shangha3u", "shangha3", NULL, NULL, "1993",
	"Shanghai III (US)\0", NULL, "Sunsoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, shangha3uRomInfo, shangha3uRomName, NULL, NULL, NULL, NULL, Shangha3InputInfo, Shangha3DIPInfo,
	Shangha3uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Shanghai III (US, prototype)

static struct BurnRomInfo shangha3upRomDesc[] = {
	{ "syan3u_evn_10-7.ic3",				0x40000, 0xa1f5275a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "syan3u_odd_10-7.ic2",				0x40000, 0xfe3960bf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s3j_chr-a1_1_sum_53b1_93.9.20.ic80",	0x80000, 0xfcaf795b, 2 | BRF_GRA },           //  2 Graphics
	{ "s3j_chr-a1_2_sum_0e32_93.9.20.ic81",	0x80000, 0x5a564f50, 2 | BRF_GRA },           //  3
	{ "s3j_chr-a1_3_sum_0d9a_93.9.20.ic82",	0x80000, 0x2b333c69, 2 | BRF_GRA },           //  4
	{ "s3j_chr-a1_4_sum_27e7_93.9.20.ic83",	0x80000, 0x19be7039, 2 | BRF_GRA },           //  5

	{ "pcm_9-16_0166.ic75",					0x40000, 0xf0cdc86a, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(shangha3up)
STD_ROM_FN(shangha3up)

static INT32 Shangha3upInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvShangha3up = {
	"shangha3up", "shangha3", NULL, NULL, "1993",
	"Shanghai III (US, prototype)\0", NULL, "Sunsoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, shangha3upRomInfo, shangha3upRomName, NULL, NULL, NULL, NULL, Shangha3InputInfo, Shangha3DIPInfo,
	Shangha3upInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Shanghai III (Japan)

static struct BurnRomInfo shangha3jRomDesc[] = {
	{ "s3j_v11.ic3",						0x040000, 0xe98ce9c8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "s3j_v11.ic2",						0x040000, 0x09174620, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s3j_char-a1.ic43",					0x200000, 0x2dbf9d17, 2 | BRF_GRA },           //  2 Graphics

	{ "s3j_v10.ic75",						0x040000, 0xf0cdc86a, 3 | BRF_SND },           //  3 Samples
};

STD_ROM_PICK(shangha3j)
STD_ROM_FN(shangha3j)

struct BurnDriver BurnDrvShangha3j = {
	"shangha3j", "shangha3", NULL, NULL, "1993",
	"Shanghai III (Japan)\0", NULL, "Sunsoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, shangha3jRomInfo, shangha3jRomName, NULL, NULL, NULL, NULL, Shangha3InputInfo, Shangha3DIPInfo,
	Shangha3uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Hebereke no Popoon (Japan)

static struct BurnRomInfo heberpopRomDesc[] = {
	{ "hbp_ic31.ic31",						0x80000, 0xc430d264, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "hbp_ic32.ic32",						0x80000, 0xbfa555a8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hbp_ic34_v1.0.ic34",					0x10000, 0x0cf056c6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "hbp_ic98_v1.0.ic98",					0x80000, 0xa599100a, 3 | BRF_GRA },           //  3 Graphics
	{ "hbp_ic99_v1.0.ic99",					0x80000, 0xfb8bb12f, 3 | BRF_GRA },           //  4
	{ "hbp_ic100_v1.0.ic100",				0x80000, 0x05a0f765, 3 | BRF_GRA },           //  5
	{ "hbp_ic101_v1.0.ic101",				0x80000, 0x151ba025, 3 | BRF_GRA },           //  6
	{ "hbp_ic102_v1.0.ic102",				0x80000, 0x2b5e341a, 3 | BRF_GRA },           //  7
	{ "hbp_ic103_v1.0.ic103",				0x80000, 0xefa0e745, 3 | BRF_GRA },           //  8
	{ "hbp_ic104_v1.0.ic104",				0x80000, 0xbb896bbb, 3 | BRF_GRA },           //  9

	{ "hbp_ic53_v1.0.ic53",					0x80000, 0xa4483aa0, 4 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(heberpop)
STD_ROM_FN(heberpop)

static INT32 HeberpopInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvHeberpop = {
	"heberpop", NULL, NULL, NULL, "1994",
	"Hebereke no Popoon (Japan)\0", NULL, "Sunsoft / Atlus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, heberpopRomInfo, heberpopRomName, NULL, NULL, NULL, NULL, HeberpopInputInfo, HeberpopDIPInfo,
	HeberpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Blocken (Japan)

static struct BurnRomInfo blockenRomDesc[] = {
	{ "ic31j.bin",							0x20000, 0xec8de2a3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic32j.bin",							0x20000, 0x79b96240, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic34.bin",							0x10000, 0x23e446ff, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ic98j.bin",							0x80000, 0x35dda273, 3 | BRF_GRA },           //  3 Graphics
	{ "ic99j.bin",							0x80000, 0xce43762b, 3 | BRF_GRA },           //  4
	{ "ic100j.bin",							0x80000, 0xa34786fd, 3 | BRF_GRA },           //  5

	{ "ic53.bin",							0x80000, 0x86108c56, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(blocken)
STD_ROM_FN(blocken)

static INT32 BlockenInit()
{
	return DrvInit(4);
}

struct BurnDriver BurnDrvBlocken = {
	"blocken", NULL, NULL, NULL, "1994",
	"Blocken (Japan)\0", NULL, "Visco / KID", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, blockenRomInfo, blockenRomName, NULL, NULL, NULL, NULL, BlockenInputInfo, BlockenDIPInfo,
	BlockenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};
