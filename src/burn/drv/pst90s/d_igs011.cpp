// FinalBurn Neo IGS011 hardware driver module
// Based on MAME driver by Luca Elia, Olivier Galibert

// known issues:
// - vbowl & clones ICS2115 emulation has issues - sound dies, gives error on boot

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "ics2115.h"
#include "burn_ym3812.h"
#include "msm6295.h"
#include "bitswap.h"
#include "burn_pal.h"
#include "timer.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2] = { NULL, NULL };
static UINT32 DrvGfxROMLen[2] = { 0, 0 };
static UINT8 *DrvSndROM;
static UINT8 *DrvLayerRAM[4];
static UINT8 *DrvPalRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvPrioRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 igs012_prot;
static UINT8 igs012_prot_swap;
static UINT8 igs012_prot_mode;

static UINT8 igs011_prot2;
static UINT8 igs011_prot1;
static UINT8 igs011_prot1_swap;
static UINT32 igs011_prot1_addr;

static UINT16 igs003_reg;
static UINT16 igs003_prot_hold;
static UINT8 igs003_prot_z;
static UINT8 igs003_prot_y;
static UINT8 igs003_prot_x;
static UINT8 igs003_prot_h1;
static UINT8 igs003_prot_h2;

static UINT16 priority;

struct blitter_t
{
	UINT16 x;
	UINT16 y;
	UINT16 w;
	UINT16 h;
	UINT16 gfx_lo;
	UINT16 gfx_hi;
	UINT16 depth;
	UINT16 pen;
	UINT16 flags;
	UINT8 pen_hi;
};

static blitter_t s_blitter;

static UINT16 dips_select;
static UINT16 trackball[2];

static INT32 sound_system = 0;	// 0 = ics2115, 1 = ym3812+m6295
static INT32 timer_irq = 3;		// 3 = vbowl, 5 = drgnwrld
static UINT16 (*igs011_prot2_read)() = NULL;

static UINT8 DrvDips[4];
static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];

static struct BurnInputInfo VbowlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Vbowl)

static struct BurnInputInfo DrgnwrldInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy4 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drgnwrld)

static struct BurnDIPInfo VbowlhkDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x14, 0x01, 0x07, 0x00, "4 Coins 1 Credits"			},
	{0x14, 0x01, 0x07, 0x01, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x07, 0x02, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"			},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x08, 0x00, "Off"							},
	{0x14, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Sexy Interlude"				},
	{0x14, 0x01, 0x10, 0x00, "No"							},
	{0x14, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Open Picture"					},
	{0x14, 0x01, 0x20, 0x00, "No"							},
	{0x14, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x14, 0x01, 0x80, 0x80, "Off"							},
	{0x14, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0x03, 0x03, "Easy  "						},
	{0x15, 0x01, 0x03, 0x02, "Normal"						},
	{0x15, 0x01, 0x03, 0x01, "Medium"						},
	{0x15, 0x01, 0x03, 0x00, "Hard  "						},

	{0   , 0xfe, 0   ,    2, "Spares To Win (Frames 1-5)"	},
	{0x15, 0x01, 0x04, 0x04, "3"							},
	{0x15, 0x01, 0x04, 0x00, "4"							},

	{0   , 0xfe, 0   ,    4, "Points To Win (Frames 6-10)"	},
	{0x15, 0x01, 0x18, 0x18, "160"							},
	{0x15, 0x01, 0x18, 0x10, "170"							},
	{0x15, 0x01, 0x18, 0x08, "180"							},
	{0x15, 0x01, 0x18, 0x00, "190"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x16, 0x01, 0x80, 0x80, "Off"							},
	{0x16, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Protection & Comm Test"		},
	{0x17, 0x01, 0x81, 0x81, "No (0)"						},
	{0x17, 0x01, 0x81, 0x80, "No (1)"						},
	{0x17, 0x01, 0x81, 0x01, "No (2)"						},
	{0x17, 0x01, 0x81, 0x00, "Yes"							},
};

STDDIPINFO(Vbowlhk)

static struct BurnDIPInfo VbowlDIPList[] = {
	{0   , 0xfe, 0   ,    2, "Controls"						},
	{0x14, 0x01, 0x40, 0x40, "Joystick"						},
	{0x14, 0x01, 0x40, 0x00, "Trackball"					},

	{0   , 0xfe, 0   ,    4, "Cabinet ID"					},
	{0x16, 0x01, 0x03, 0x03, "1"							},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x01, "3"							},
	{0x16, 0x01, 0x03, 0x00, "4"							},

	{0   , 0xfe, 0   ,    2, "Linked Cabinets"				},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},
};

STDDIPINFOEXT(Vbowl, Vbowlhk, Vbowl)

static struct BurnDIPInfo VbowljDIPList[] = {
	{0   , 0xfe, 0   ,    2, "Controls"						},
	{0x14, 0x01, 0x20, 0x20, "Joystick"						},
	{0x14, 0x01, 0x20, 0x00, "Trackball"					},

	{0   , 0xfe, 0   ,    4, "Cabinet ID"					},
	{0x16, 0x01, 0x03, 0x03, "1"							},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x01, "3"							},
	{0x16, 0x01, 0x03, 0x00, "4"							},

	{0   , 0xfe, 0   ,    2, "Linked Cabinets"				},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},
};

STDDIPINFOEXT(Vbowlj, Vbowlhk, Vbowlj)

static struct BurnDIPInfo DrgnwrldCommonDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    1, "Service Mode"					},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x16, 0x01, 0x07, 0x00, "5 Coins 1 Credits"			},
	{0x16, 0x01, 0x07, 0x01, "4 Coins 1 Credits"			},
	{0x16, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x16, 0x01, 0x07, 0x03, "2 Coins 1 Credits"			},
	{0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x16, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x16, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x16, 0x01, 0x07, 0x04, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x18, 0x18, "Easy   "						},
	{0x16, 0x01, 0x18, 0x10, "Normal "						},
	{0x16, 0x01, 0x18, 0x08, "Hard   "						},
	{0x16, 0x01, 0x18, 0x00, "Hardest"						},
};

//STDDIPINFO(DrgnwrldCommon)

static struct BurnDIPInfo DrgnwrldDIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Strip Girl"					},
	{0x17, 0x01, 0x01, 0x00, "No"							},
	{0x17, 0x01, 0x01, 0x01, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Background"					},
	{0x17, 0x01, 0x02, 0x02, "Girl"							},
	{0x17, 0x01, 0x02, 0x00, "Scene"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x04, 0x00, "Off"							},
	{0x17, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Fever Game"					},
	{0x17, 0x01, 0x08, 0x08, "One Kind Only"				},
	{0x17, 0x01, 0x08, 0x00, "Several Kinds"				},
};

STDDIPINFOEXT(Drgnwrld, DrgnwrldCommon, Drgnwrld)

static struct BurnDIPInfo DrgnwrldcDIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Strip Girl"					},
	{0x17, 0x01, 0x01, 0x00, "No"							},
	{0x17, 0x01, 0x01, 0x01, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Sex Question"					},
	{0x17, 0x01, 0x02, 0x00, "No"							},
	{0x17, 0x01, 0x02, 0x02, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Background"					},
	{0x17, 0x01, 0x04, 0x04, "Girl"							},
	{0x17, 0x01, 0x04, 0x00, "Scene"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x08, 0x00, "Off"							},
	{0x17, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Tiles"						},
	{0x17, 0x01, 0x10, 0x10, "Mahjong"						},
	{0x17, 0x01, 0x10, 0x00, "Symbols"						},

	{0   , 0xfe, 0   ,    2, "Fever Game"					},
	{0x17, 0x01, 0x40, 0x40, "One Kind Only"				},
	{0x17, 0x01, 0x40, 0x00, "Several Kinds"				},
};

STDDIPINFOEXT(Drgnwrldc, DrgnwrldCommon, Drgnwrldc)

static struct BurnDIPInfo DrgnwrldjDIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Background"					},
	{0x17, 0x01, 0x01, 0x01, "Girl"							},
	{0x17, 0x01, 0x01, 0x00, "Scene"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x02, 0x00, "Off"							},
	{0x17, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    2, "Tiles"						},
	{0x17, 0x01, 0x04, 0x04, "Mahjong"						},
	{0x17, 0x01, 0x04, 0x00, "Symbols"						},
};

STDDIPINFOEXT(Drgnwrldj, DrgnwrldCommon, Drgnwrldj)

static UINT16 inline dips_read(INT32 num)
{
	UINT16 ret = 0xff;

	for (INT32 i = 0; i < num; i++)
		if (BIT(~dips_select, i))
			ret &= DrvDips[i];

	return  (ret & 0xff) | 0x0000;	// 0x0100 is blitter busy
}

static void igs011_blit()
{
	int    const layer  =      s_blitter.flags & 0x0007;
	bool     const opaque = BIT(~s_blitter.flags,  3);
	bool     const clear  = BIT( s_blitter.flags,  4);
	bool     const flipx  = BIT( s_blitter.flags,  5);
	bool     const flipy  = BIT( s_blitter.flags,  6);
	bool     const blit   = BIT( s_blitter.flags, 10);

	if (!blit)
		return;

	int const pen_hi = (s_blitter.pen_hi & 0x07) << 5;
	int const hibpp_layers = 4 - (s_blitter.depth & 0x07);
	bool  const dst4     = layer >= hibpp_layers;

	bool const src4   = dst4 || (s_blitter.gfx_hi & 0x80); // see lhb2
	int const shift   = dst4 ? (((layer - hibpp_layers) & 0x01) << 2) : 0;
	int const mask    = dst4 ? (0xf0 >> shift) : 0x00;
	int const buffer  = dst4 ? (hibpp_layers + ((layer - hibpp_layers) >> 1)) : layer;

	if (buffer >= 4)
	{
		return;
	}

	UINT8 *dest = DrvLayerRAM[buffer];

	UINT32 z = ((s_blitter.gfx_hi & 0x7f) << 16) | s_blitter.gfx_lo;

	UINT8 const clear_pen = src4 ? (s_blitter.pen | 0xf0) : s_blitter.pen;

	UINT8 trans_pen;
	if (src4)
	{
		z <<= 1;
		if (DrvGfxROM[1] && (s_blitter.gfx_hi & 0x80)) trans_pen = 0x1f;   // lhb2
		else                                       trans_pen = 0x0f;
	}
	else
	{
		if (DrvGfxROM[1]) trans_pen = 0x1f;   // vbowl
		else          trans_pen = 0xff;
	}

	int xstart = s_blitter.x; //util::sext(m_blitter.x, 10);
	if (xstart & 0x400) xstart |= 0xfffffc00;
	int ystart = s_blitter.y; //util::sext(m_blitter.y, 9);
	if (ystart & 0x200) ystart |= 0xfffffe00;
	int const xsize = (s_blitter.w & 0x1ff) + 1;
	int const ysize = (s_blitter.h & 0x0ff) + 1;
	int const xend = flipx ? (xstart - xsize) : (xstart + xsize);
	int const yend = flipy ? (ystart - ysize) : (ystart + ysize);
	int const xinc = flipx ? -1 : 1;
	int const yinc = flipy ? -1 : 1;

	for (int y = ystart; y != yend; y += yinc)
	{
		for (int x = xstart; x != xend; x += xinc)
		{
			if (y >= 0 && y < 256 && x >= 0 && x < 512)
			{
				UINT8 pen = 0;
				if (!clear)
				{
					if (src4)
						pen = (DrvGfxROM[0][(z >> 1) & 0x7fffff] >> (BIT(z, 0) << 2)) & 0x0f;
					else
						pen = DrvGfxROM[0][z & 0x7fffff];

					if (DrvGfxROM[1])
						pen = (pen & 0x0f) | (BIT(DrvGfxROM[1][(z >> 3) & 0xfffff], z & 0x07) << 4);
				}

				if (clear || (pen != trans_pen) || opaque)
				{
					UINT8 const val = clear ? clear_pen : (pen != trans_pen) ? (pen | pen_hi) : 0xff;
					UINT8 &destbyte = dest[(y << 9) | x];
					if (dst4)
						destbyte = (destbyte & mask) | ((val & 0x0f) << shift);
					else
						destbyte = val;
				}
			}
			++z;
		}
	}
}

static void __fastcall igs011_blit_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x7fff)
	{
		case 0x0000: s_blitter.x = data; break;
		case 0x0800: s_blitter.y = data; break;
		case 0x1000: s_blitter.w = data; break;
		case 0x1800: s_blitter.h = data; break;
		case 0x2000: s_blitter.gfx_lo = data; break;
		case 0x2800: s_blitter.gfx_hi = data; break;
		case 0x3000: s_blitter.flags = data; igs011_blit(); break;
		case 0x3800: s_blitter.pen = data; break;
		case 0x4000: s_blitter.depth = data; break;
	}
}

static void __fastcall igs011_blit_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("Blit_wb: %5.5x, %2.2x\n"), address, data);
}

static void __fastcall igs011_palette_write_word(UINT32 address, UINT16 data)
{
	DrvPalRAM[(address >> 1) & 0xfff] = data;
}

static void __fastcall igs011_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address >> 1) & 0xfff] = data;
}

static UINT16 __fastcall igs011_palette_read_word(UINT32 address)
{
	bprintf (0, _T("pal_rw: %5.5x\n"), address);
	return 0;
}	

static UINT8 __fastcall igs011_palette_read_byte(UINT32 address)
{
	return DrvPalRAM[(address >> 1) & 0xfff];
}

static void __fastcall igs011_layerram_write_word(UINT32 address, UINT16 data)
{
	bprintf (0, _T("pal_ww: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall igs011_layerram_write_byte(UINT32 address, UINT8 data)
{
	DrvLayerRAM[((address >> 17) & 2) | (address & 1)][(address & 0x3fffe) >> 1] = data;
}

static UINT16 __fastcall igs011_layerram_read_word(UINT32 address)
{
	bprintf (0, _T("lay_rw: %5.5x\n"), address);
	return 0;
}	

static UINT8 __fastcall igs011_layerram_read_byte(UINT32 address)
{
	return DrvLayerRAM[((address >> 17) & 2) | (address & 1)][(address & 0x3fffe) >> 1];
}

#define MODE_AND_DATA(_MODE,_DATA)  (igs012_prot_mode == (_MODE) && ((data & 0xff) == (_DATA)) )

static inline void igs012_prot_swap_write(UINT8 data)
{
	if (MODE_AND_DATA(0, 0x55) || MODE_AND_DATA(1, 0xa5) )
	{
		UINT8 x = igs012_prot;
		igs012_prot_swap = (((BIT(x,3)|BIT(x,1))^1)<<3) | ((BIT(x,2)&BIT(x,1))<<2) | ((BIT(x,3)^BIT(x,0))<<1) | (BIT(x,2)^1);
	}
/*
	if ((igs012_prot_mode == 0 && data == 0x55) || (igs012_prot_mode == 1 && data == 0xa5))
	{
		UINT8 x = igs012_prot;
		igs012_prot_swap = (((BIT(x,3)|BIT(x,1))^1)<<3) | ((BIT(x,2)&BIT(x,1))<<2) | ((BIT(x,3)^BIT(x,0))<<1) | (BIT(x,2)^1);
	}
*/
}

static inline void igs012_prot_dec_inc_write(UINT8 data)
{
	if (MODE_AND_DATA(0, 0xaa) )
	{
		igs012_prot = (igs012_prot - 1) & 0x1f;
	}
	else if (MODE_AND_DATA(1, 0xfa) )
	{
		igs012_prot = (igs012_prot + 1) & 0x1f;
	}
/*
	if (igs012_prot_mode == 0 && data == 0xaa)
	{
		igs012_prot = (igs012_prot - 1) & 0x1f;
	}
	else if (igs012_prot_mode == 1 && data == 0xfa)
	{
		igs012_prot = (igs012_prot + 1) & 0x1f;
	}
*/
}

static inline void igs012_prot_inc_write(UINT8 data)
{
	if (MODE_AND_DATA(0, 0xff) )
	{
		igs012_prot = (igs012_prot + 1) & 0x1f;
	}
/*
	if (igs012_prot_mode == 0 && data == 0xff)
	{
		igs012_prot = (igs012_prot + 1) & 0x1f;
	}
*/
}

static inline void igs012_prot_copy_write(UINT8 data)
{
	if (MODE_AND_DATA(1, 0x22) )
	{
		igs012_prot = igs012_prot_swap;
	}
/*
	if (igs012_prot_mode == 1 && data == 0x22)
	{
		igs012_prot = igs012_prot_swap;
	}
*/
}

static inline void igs012_prot_dec_copy_write(UINT8 data)
{
	if (MODE_AND_DATA(0, 0x33) )
	{
		igs012_prot = igs012_prot_swap;
	}
	else if (MODE_AND_DATA(1, 0x5a) )
	{
		igs012_prot = (igs012_prot - 1) & 0x1f;
	}
/*
	if (igs012_prot_mode == 0 && data == 0x33)
	{
		igs012_prot = igs012_prot_swap;
	}
	else if (igs012_prot_mode == 1 && data == 0x5a)
	{
		igs012_prot = (igs012_prot - 1) & 0x1f;
	}
*/
}

static inline void igs012_prot_mode_write(UINT8 data)
{
	if (MODE_AND_DATA(0, 0xcc) || MODE_AND_DATA(1, 0xcc) || MODE_AND_DATA(0, 0xdd) || MODE_AND_DATA(1, 0xdd))
	{
		igs012_prot_mode = igs012_prot_mode ^ 1;
	}
/*
	if (data == 0xcc || data == 0xdd)
	{
		igs012_prot_mode ^= 1;
	}
*/
}

static UINT16 drgnwrldv20j_igs011_prot2_read()
{
	UINT8 x = igs011_prot2;
	UINT8 b9 = ((BIT(x,4)^1) | (BIT(x,0)^1)) | ((BIT(x,3) | BIT(x,1))^1) | ((BIT(x,2) & BIT(x,0))^1);
	return (b9 << 9);
}

static UINT16 drgnwrldv40k_igs011_prot2_read()
{
	UINT8 x = igs011_prot2;
	UINT8 b9 = ((BIT(x, 4) ^ 1) & (BIT(x, 0) ^ 1)) | (((BIT(x, 3)) & ((BIT(x, 2)) ^ 1)) & (((BIT(x, 1)) ^ (BIT(x, 0))) ^ 1));
	return (b9 << 9);
}

static UINT16 drgnwrldv21_igs011_prot2_read()
{
	UINT8 x = igs011_prot2;
	UINT8 b9 = (BIT(x,4)^1) | ((BIT(x,0)^1) & BIT(x,2)) | ((BIT(x,3)^BIT(x,1)^1) & ((((BIT(x,4)^1) & BIT(x,0)) | BIT(x,2))^1) );
	return (b9 << 9);
}

static UINT16 inline vbowl_igs011_prot2_read()
{
	UINT8 x = igs011_prot2;
	UINT8 b9 = ((BIT(x,4)^1) & (BIT(x,3)^1)) | ((BIT(x,2) & BIT(x,1))^1) | ((BIT(x,4) | BIT(x,0))^1);
	return (b9 << 9);
}

static UINT16 inline vbowlhk_igs011_prot2_read()
{
	UINT8 x = igs011_prot2;
	UINT8 b9 = ((BIT(x,4)^1) & (BIT(x,3)^1)) | (((BIT(x,2)^1) & (BIT(x,1)^1))^1) | ((BIT(x,4) | (BIT(x,0)^1))^1);
	return (b9 << 9);
}

static void igs011_prot1_write(UINT8 offset, UINT8 data)
{
	switch (offset << 1)
	{
		case 0: // COPY
			if (data == 0x33)
			{
				igs011_prot1 = igs011_prot1_swap;
			}
		break;

		case 2: // INC
			if (data == 0xff)
			{
				igs011_prot1++;
			}
		break;

		case 4: // DEC
			if (data == 0xaa)
			{
				igs011_prot1--;
			}
		break;

		case 6: // SWAP
			if (data == 0x55)
			{
				// b1 . (b2|b3) . b2 . (b0&b3)
				UINT8 x = igs011_prot1;
				igs011_prot1_swap = (BIT(x,1)<<3) | ((BIT(x,2)|BIT(x,3))<<2) | (BIT(x,2)<<1) | (BIT(x,0)&BIT(x,3));
			}
		break;
	}
}

static UINT16 inline igs011_prot1_read()
{
	UINT16 x = igs011_prot1;
	return (((BIT(x,1)&BIT(x,2))^1)<<5) | ((BIT(x,0)^BIT(x,3))<<2);
}

static void igs011_prot_addr_write(UINT16 data)
{
	igs011_prot1 = 0;
	igs011_prot1_swap = 0;
	igs011_prot1_addr = (data << 4) ^ 0x8340;
}

static void vbowl_igs003_write(UINT16 data)
{
	switch (igs003_reg)
	{
		case 0x02: break;

		case 0x40:
			igs003_prot_h2 = igs003_prot_h1;
			igs003_prot_h1 = data;
		break;

		case 0x48:
			igs003_prot_x = 0;
			if ((igs003_prot_h2 & 0x0a) != 0x0a) igs003_prot_x |= 0x08;
			if ((igs003_prot_h2 & 0x90) != 0x90) igs003_prot_x |= 0x04;
			if ((igs003_prot_h1 & 0x06) != 0x06) igs003_prot_x |= 0x02;
			if ((igs003_prot_h1 & 0x90) != 0x90) igs003_prot_x |= 0x01;
		break;

		case 0x50: // reset?
			igs003_prot_hold = 0;
		break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		{
			const UINT16 old = igs003_prot_hold;
			igs003_prot_y = igs003_reg & 0x07;
			igs003_prot_z = data;
			igs003_prot_hold <<= 1;
			igs003_prot_hold |= BIT(old, 15);
			igs003_prot_hold ^= 0x2bad;
			igs003_prot_hold ^= BIT(old, 10);
			igs003_prot_hold ^= BIT(old,  8);
			igs003_prot_hold ^= BIT(old,  5);
			igs003_prot_hold ^= BIT(igs003_prot_z, igs003_prot_y);
			igs003_prot_hold ^= BIT(igs003_prot_x, 0) <<  4;
			igs003_prot_hold ^= BIT(igs003_prot_x, 1) <<  6;
			igs003_prot_hold ^= BIT(igs003_prot_x, 2) << 10;
			igs003_prot_hold ^= BIT(igs003_prot_x, 3) << 12;
		}
		break;
	}
}

static void vbowlhk_igs003_write(UINT16 data)
{
	switch (igs003_reg)
	{
		case 0x02: break;

		case 0x40:
			igs003_prot_h2 = igs003_prot_h1;
			igs003_prot_h1 = data;
		break;

		case 0x48:
			igs003_prot_x = 0;
			if ((igs003_prot_h2 & 0x0a) != 0x0a) igs003_prot_x |= 0x08;
			if ((igs003_prot_h2 & 0x90) != 0x90) igs003_prot_x |= 0x04;
			if ((igs003_prot_h1 & 0x06) != 0x06) igs003_prot_x |= 0x02;
			if ((igs003_prot_h1 & 0x90) != 0x90) igs003_prot_x |= 0x01;
		break;

		case 0x50: // reset?
			igs003_prot_hold = 0;
		break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		{
			const UINT16 old = igs003_prot_hold;
			igs003_prot_y = igs003_reg & 0x07;
			igs003_prot_z = data;
			igs003_prot_hold <<= 1;
			igs003_prot_hold |= BIT(old, 15);
			igs003_prot_hold ^= 0x2bad;
			igs003_prot_hold ^= BIT(old, 7);
			igs003_prot_hold ^= BIT(old, 6);
			igs003_prot_hold ^= BIT(old, 5);
			igs003_prot_hold ^= BIT(igs003_prot_z, igs003_prot_y);
			igs003_prot_hold ^= BIT(igs003_prot_x, 0) <<  3;
			igs003_prot_hold ^= BIT(igs003_prot_x, 1) <<  8;
			igs003_prot_hold ^= BIT(igs003_prot_x, 2) << 10;
			igs003_prot_hold ^= BIT(igs003_prot_x, 3) << 14;
		}
		break;
	}
}

static UINT16 vbowl_igs003_read()
{
	switch (igs003_reg)
	{
		case 0x00: return DrvInputs[1];
		case 0x01: return DrvInputs[2];
		case 0x02: return DrvInputs[3]; // drgnwrld

		case 0x03: return BITSWAP08(igs003_prot_hold, 5,2,9,7,10,13,12,15);

		case 0x20: return 0x49;
		case 0x21: return 0x47;
		case 0x22: return 0x53;

		case 0x24: return 0x41;
		case 0x25: return 0x41;
		case 0x26: return 0x7f;
		case 0x27: return 0x41;
		case 0x28: return 0x41;

		case 0x2a: return 0x3e;
		case 0x2b: return 0x41;
		case 0x2c: return 0x49;
		case 0x2d: return 0xf9;
		case 0x2e: return 0x0a;

		case 0x30: return 0x26;
		case 0x31: return 0x49;
		case 0x32: return 0x49;
		case 0x33: return 0x49;
		case 0x34: return 0x32;
	}

	return 0;
}

static void __fastcall vbowl_main_write_word(UINT32 address, UINT16 data)
{	
	switch (address & ~0x1c00f)
	{
		case 0x001600:
			igs012_prot_swap_write(data);
		return;

		case 0x001620:
			igs012_prot_dec_inc_write( data);
		return;

		case 0x001630:
			igs012_prot_inc_write(data);
		return;

		case 0x001640:
			igs012_prot_copy_write(data);
		return;

		case 0x001650:
			igs012_prot_dec_copy_write(data);
		return;

		case 0x001670:
			igs012_prot_mode_write(data);
		return;
	}

	switch (address & ~0x3f)
	{
		case 0x00d400:
			igs011_prot2--;
		return;

		case 0x00d440: {
			UINT8 x = igs011_prot2;
			igs011_prot2 = ((BIT(x,3)&BIT(x,0))<<4) | (BIT(x,2)<<3) | ((BIT(x,0)|BIT(x,1))<<2) | ((BIT(x,2)^BIT(x,4)^1)<<1) | (BIT(x,1)^1^BIT(x,3));
		}
		return;

		case 0x00d480:
			igs011_prot2 = 0;
		return;
	}

	switch (address & ~0x1ff)
	{
		case 0x50f000:
			igs011_prot2--;
		return;

		case 0x50f200: {
			UINT8 x = igs011_prot2;
			igs011_prot2 = ((BIT(x,3)^BIT(x,2))<<4) | ((BIT(x,2)^BIT(x,1))<<3) | ((BIT(x,1)^BIT(x,0))<<2) | ((BIT(x,4)^BIT(x,0))<<1) | (BIT(x,4)^BIT(x,3));
		}
		return;

		case 0x50f400:
			igs011_prot2 = 0;
		return;
	}

	if ((address & 0xfff000) == 0x902000) {
		igs012_prot = 0;
		igs012_prot_swap = 0;
		igs012_prot_mode = 0;
		return;
	}

	switch (address)
	{      //0,1
		case 0x600002:
			ics2115write(1, data & 0xff);
		return;

		case 0x600004:
			ics2115write(2, data & 0xff);
			ics2115write(3, data >> 8);
		return;
		   //6,7
		case 0x600000:
		case 0x600006:
		return;

		case 0x700000:
		case 0x700002:
			trackball[(address & 2) / 2] = data;
		return;

		case 0x700004:
			s_blitter.pen_hi = data & 0x07;
		return;

		case 0x800000:
			igs003_reg = data;
		return;

		case 0x800002:
			vbowl_igs003_write(data);
		return;

		case 0xa00000:
		case 0xa08000:
		case 0xa10000:
		case 0xa18000:
			// link_#_w
		return;

		case 0xa20000:
			priority = data;
		return;

		case 0xa28000:
		case 0xa38000:
		return; // ??


		case 0xa40000:
			dips_select = data;
		return;

		case 0xa48000:
			igs011_prot_addr_write(data);
		return;
	}

//	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall vbowl_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffff8) == igs011_prot1_addr) {
		igs011_prot1_write((address & 6) / 2, data);
		return;
	}

	switch (address & ~0x1c00f)
	{
		case 0x001600:
			igs012_prot_swap_write(data);
		return;

		case 0x001620:
			igs012_prot_dec_inc_write( data);
		return;

		case 0x001630:
			igs012_prot_inc_write(data);
		return;

		case 0x001640:
			igs012_prot_copy_write(data);
		return;

		case 0x001650:
			igs012_prot_dec_copy_write(data);
		return;

		case 0x001670:
			igs012_prot_mode_write(data);
		return;
	}

	switch (address & ~0x3f)
	{
		case 0x00d400:
			igs011_prot2--;
		return;

		case 0x00d440: {
			UINT8 x = igs011_prot2;
			igs011_prot2 = ((BIT(x,3)&BIT(x,0))<<4) | (BIT(x,2)<<3) | ((BIT(x,0)|BIT(x,1))<<2) | ((BIT(x,2)^BIT(x,4)^1)<<1) | (BIT(x,1)^1^BIT(x,3));
		}
		return;

		case 0x00d480:
			igs011_prot2 = 0;
		return;
	}

	if ((address & 0xfff000) == 0x902000) {
		igs012_prot = 0;
		igs012_prot_swap = 0;
		igs012_prot_mode = 0;
		return;
	}

	switch (address)
	{
		case 0xa40001:
			dips_select = data;
		return;

		case 0x700005:
			s_blitter.pen_hi = data & 0x07;
		return;

		case 0x800001:
			igs003_reg = data;
		return;

		case 0x800003:
			vbowl_igs003_write(data);
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall vbowl_main_read_word(UINT32 address)
{
	switch (address & ~0x1c00f)
	{
		case 0x001610:
		case 0x001660:
		{
			UINT8 x = igs012_prot;

			UINT8 b1 = (BIT(x,3) | BIT(x,1))^1;
			UINT8 b0 = BIT(x,3) ^ BIT(x,0);

			return (b1 << 1) | (b0 << 0);
		}
	}

	switch (address & ~0x3f)
	{
		case 0x00d4c0:
			if (igs011_prot2_read)
				return igs011_prot2_read();
			return 0;
	}

	if (address < 0x80000) {
		return *((UINT16*)(Drv68KROM + address));
	}

	switch (address & ~0x1ff)
	{
		case 0x50f600:
		{
		//	return vbowlhk_igs011_prot2_read();
			return vbowl_igs011_prot2_read();
		}
	}

	switch (address)
	{
		case 0x500000:
		case 0x520000:
			return DrvInputs[0]; // coin

		case 0x600000:
			return ics2115read(0);
		case 0x600002:
			return ics2115read(1);
		case 0x600004:
			return ics2115read(2) | (ics2115read(3) << 8);
		case 0x600006:
			return 0;


		case 0x700000:
		case 0x700002:
			return trackball[(address / 2) & 1];

		case 0x800002:
			return vbowl_igs003_read();

		case 0xa80000:
			return ~0;

		case 0xa88000:
			return dips_read(4);

		case 0xa90000:
			return ~0;

		case 0xa98000:
			return ~0;
	}

//	bprintf (0, _T("MRW: %5.5x\n"), address);
	
	return 0;
}

static UINT8 __fastcall vbowl_main_read_byte(UINT32 address)
{

	if ((address & 0xffffff8) == igs011_prot1_addr+8) {
		return igs011_prot1_read();
	}

	switch (address)
	{
		case 0x500001:
		case 0x520001:
			return DrvInputs[0]; // coin

		case 0x800003:
			return vbowl_igs003_read();

		case 0xa88001:
			return dips_read(4);
	}

	if (address < 0x80000)
	{
		return vbowl_main_read_word(address & ~1) >> ((~address & 1) * 8);
	}
	
//	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall vbowlhk_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800002:
			vbowlhk_igs003_write(data);
		return;
	}

	vbowl_main_write_word(address, data);
}

static UINT16 __fastcall vbowlhk_main_read_word(UINT32 address)
{
	switch (address & ~0x1ff)
	{
		case 0x50f600:
			return vbowlhk_igs011_prot2_read();
	}

	return vbowl_main_read_word(address);
}


static void __fastcall drgnwrld_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x600000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x700000:
		case 0x700002:
			BurnYM3812Write(0, (address / 2) & 1, data & 0xff);
		return;
	}

	vbowl_main_write_word(address, data);
}

static void __fastcall drgnwrld_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x600000:
		case 0x600001:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x700000:
		case 0x700001:
		case 0x700002:
		case 0x700003:
			BurnYM3812Write(0, (address / 2) & 1, data & 0xff);
		return;
	}

	vbowl_main_write_byte(address, data);
}

static UINT16 __fastcall drgnwrld_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x600000:
			return MSM6295Read(0);

		case 0xa88000:
			return dips_read(3);
	}

	return vbowl_main_read_word(address);
}

static UINT8 __fastcall drgnwrld_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x600000:
		case 0x600001:
			return MSM6295Read(0);

		case 0xa88000:
		case 0xa88001:
			return dips_read(3);
	}

	return vbowl_main_read_byte(address);
}

static void vbowl_decrypt()
{
	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x4100) == 0x0100)
			x ^= 0x0200;

		if ((i & 0x4000) == 0x4000 && (i & 0x0300) != 0x0100)
			x ^= 0x0200;

		if ((i & 0x5700) == 0x5100)
			x ^= 0x0200;

		if ((i & 0x5500) == 0x1000)
			x ^= 0x0200;

		if ((i & 0x0140) != 0x0000 || (i & 0x0012) == 0x0012)
			x ^= 0x0004;

		if ((i & 0x2004) != 0x2004 || (i & 0x0090) == 0x0000)
			x ^= 0x0020;

		src[i] = x;
	}
}

static void vbowlhk_decrypt()
{
	vbowl_decrypt();

	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0xd700) == 0x0100)
			x ^= 0x0200;

		if ((i & 0xc700) == 0x0500)
			x ^= 0x0200;

		if ((i & 0xd500) == 0x4000)
			x ^= 0x0200;
			
		if ((i & 0xc500) == 0x4400)
			x ^= 0x0200;

		if ((i & 0xd100) == 0x8000)
			x ^= 0x0200;

		if ((i & 0xd700) == 0x9500)
			x ^= 0x0200;

		if ((i & 0xd300) == 0xc100)
			x ^= 0x0200;

		if ((i & 0xd500) == 0xd400)
			x ^= 0x0200;

		src[i] = x;
	}
}

static void drgnwrld_type1_decrypt()
{
	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
			x ^= 0x0004;

		if ((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
			x ^= 0x0020;

		if ((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
			x ^= 0x0024;

		src[i] = x;
	}
}

static void drgnwrld_type2_decrypt()
{
	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if (((i & 0x000090) == 0x000000) || ((i & 0x002004) != 0x002004))
			x ^= 0x0004;

		if ((((i & 0x000050) == 0x000000) || ((i & 0x000142) != 0x000000)) && ((i & 0x000150) != 0x000000))
			x ^= 0x0020;

		if (((i & 0x004280) == 0x004000) || ((i & 0x004080) == 0x000000))
			x ^= 0x0200;

		if ((i & 0x0011a0) != 0x001000)
			x ^= 0x0200;

		if ((i & 0x000180) == 0x000100)
			x ^= 0x0200;

		if ((x & 0x0024) == 0x0020 || (x & 0x0024) == 0x0004)
			x ^= 0x0024;

		src[i] = x;
	}
}

static void drgnwrld_type3_decrypt()
{
	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
			x ^= 0x0004;

		if ((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
			x ^= 0x0020;

		if ((((i & 0x1000) == 0x1000) ^ ((i & 0x0100) == 0x0100))
			|| (i & 0x0880) == 0x0800 || (i & 0x0240) == 0x0240)
				x ^= 0x0200;

		if ((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
			x ^= 0x0024;

		src[i] = x;
	}
}

static void drgnwrldv40k_decrypt()
{
	drgnwrld_type1_decrypt();

	UINT16 *src = (UINT16*)Drv68KROM;

	for (INT32 i = 0; i < 0x80000/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x0800) != 0x0800)
			x ^= 0x0200;

		if (((i & 0x3a00) == 0x0a00) ^ ((i & 0x3a00) == 0x2a00))
			x ^= 0x0200;

		if (((i & 0x3ae0) == 0x0860) ^ ((i & 0x3ae0) == 0x2860))
			x ^= 0x0200;

		if (((i & 0x1c00) == 0x1800) ^ ((i & 0x1e00) == 0x1e00))
			x ^= 0x0200;

		if ((i & 0x1ee0) == 0x1c60)
			x ^= 0x0200;

		src[i] = x;
	}
}

static void ics2115_sound_irq(INT32 )
{

}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();

	switch (sound_system)
	{
		case 0:
			ics2115_reset();
		break;

		case 1:
			BurnYM3812Reset();
			MSM6295Reset(0);
		break;
	}

	SekClose();

	igs012_prot = 0;
	igs012_prot_swap = 0;
	igs012_prot_mode = 0;

	igs011_prot2 = 0;

	igs011_prot1 = 0;
	igs011_prot1_swap = 0;
	igs011_prot1_addr = 0;

	igs003_reg = 0;
	igs003_prot_hold = 0;
	igs003_prot_z = 0;
	igs003_prot_y = 0;
	igs003_prot_x = 0;
	igs003_prot_h1 = 0;
	igs003_prot_h2 = 0;

	priority = 0;

//	memset ((void*)s_blitter, 0, sizeof(blitter_t));
	s_blitter.x = s_blitter.y = s_blitter.w = s_blitter.h = 0;
	s_blitter.gfx_lo = s_blitter.gfx_hi = 0;
	s_blitter.depth = 0;
	s_blitter.pen = 0;
	s_blitter.flags = 0;
	s_blitter.pen_hi = 0;

	dips_select = 0;
	trackball[0] = trackball[0] = 0;

	return 0;
}

static INT32 MemIndex(UINT32 gfxlen0, UINT32 gfxlen1)
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;

	DrvGfxROM[0]	= Next; Next += gfxlen0 * 2;
	if (gfxlen1) {
		DrvGfxROM[1]= Next; Next += gfxlen1;
	}

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x400000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	DrvNVRAM		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x002000;
	DrvPrioRAM		= Next; Next += 0x001000;
	DrvLayerRAM[0]	= Next; Next += 0x020000;
	DrvLayerRAM[1]	= Next; Next += 0x020000;
	DrvLayerRAM[2]	= Next; Next += 0x020000;
	DrvLayerRAM[3]	= Next; Next += 0x020000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 VbowlInit()
{
	AllMem = NULL;
	MemIndex(0x400000, 0x100000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x400000, 0x100000);

	{
		if (BurnLoadRom(Drv68KROM + 0x000000, 0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 1, 1)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x00000, 2, 1, LD_INVERT)) return 1;
		
		if (BurnLoadRom(DrvSndROM + 0x00000, 3, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x80000, 4, 1)) return 1;

		vbowl_decrypt();
		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x400000, 1, 0x00);

		*((UINT16*)(Drv68KROM + 0x080e0)) = 0xe549; // patch bad opcode
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_FETCH); // read in handler
	SekMapMemory(DrvNVRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvPrioRAM,	0x200000, 0x200fff, MAP_RAM);
	SekMapHandler(1,			0x300000, 0x3fffff, MAP_RAM);
	SekMapHandler(2,			0x400000, 0x401fff, MAP_RAM);
	SekMapHandler(3,			0xa58000, 0xa5ffff, MAP_WRITE);
	SekSetWriteWordHandler(0,	vbowl_main_write_word);
	SekSetWriteByteHandler(0,	vbowl_main_write_byte);
	SekSetReadWordHandler(0,	vbowl_main_read_word);
	SekSetReadByteHandler(0,	vbowl_main_read_byte);
	SekSetWriteWordHandler(1,	igs011_layerram_write_word);
	SekSetWriteByteHandler(1,	igs011_layerram_write_byte);
	SekSetReadWordHandler(1,	igs011_layerram_read_word);
	SekSetReadByteHandler(1,	igs011_layerram_read_byte);
	SekSetWriteWordHandler(2,	igs011_palette_write_word);
	SekSetWriteByteHandler(2,	igs011_palette_write_byte);
	SekSetReadWordHandler(2,	igs011_palette_read_word);
	SekSetReadByteHandler(2,	igs011_palette_read_byte);
	SekSetWriteWordHandler(3,	igs011_blit_write_word);
	SekSetWriteByteHandler(3, 	igs011_blit_write_byte);
	SekClose();

	igs011_prot2_read = drgnwrldv20j_igs011_prot2_read;

	ics2115_init(ics2115_sound_irq, DrvSndROM, 0x100000);
	BurnTimerAttachSek(7333333);
	
	GenericTilesInit();
	DrvGfxROMLen[0] = 0x800000;
	DrvGfxROMLen[1] = 0x100000;

	DrvDoReset();

	return 0;
}

static INT32 VbowlhkInit()
{
	AllMem = NULL;
	MemIndex(0x400000, 0x100000);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x400000, 0x100000);

	{
		if (BurnLoadRom(Drv68KROM + 0x000000, 0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, 1, 1)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x00000, 2, 1, LD_INVERT)) return 1;
		
		if (BurnLoadRom(DrvSndROM + 0x00000, 3, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x80000, 4, 1)) return 1;

		vbowlhk_decrypt();
		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x400000, 1, 0x00);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_FETCH); // read in handler
	SekMapMemory(DrvNVRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvPrioRAM,	0x200000, 0x200fff, MAP_RAM);
	SekMapHandler(1,			0x300000, 0x3fffff, MAP_RAM);
	SekMapHandler(2,			0x400000, 0x401fff, MAP_RAM);
	SekMapHandler(3,			0xa58000, 0xa5ffff, MAP_WRITE);
	SekSetWriteWordHandler(0,	vbowlhk_main_write_word);
	SekSetWriteByteHandler(0,	vbowl_main_write_byte);
	SekSetReadWordHandler(0,	vbowlhk_main_read_word);
	SekSetReadByteHandler(0,	vbowl_main_read_byte);
	SekSetWriteWordHandler(1,	igs011_layerram_write_word);
	SekSetWriteByteHandler(1,	igs011_layerram_write_byte);
	SekSetReadWordHandler(1,	igs011_layerram_read_word);
	SekSetReadByteHandler(1,	igs011_layerram_read_byte);
	SekSetWriteWordHandler(2,	igs011_palette_write_word);
	SekSetWriteByteHandler(2,	igs011_palette_write_byte);
	SekSetReadWordHandler(2,	igs011_palette_read_word);
	SekSetReadByteHandler(2,	igs011_palette_read_byte);
	SekSetWriteWordHandler(3,	igs011_blit_write_word);
	SekSetWriteByteHandler(3, 	igs011_blit_write_byte);
	SekClose();

	igs011_prot2_read = drgnwrldv20j_igs011_prot2_read;

	ics2115_init(ics2115_sound_irq, DrvSndROM, 0x100000);
	BurnTimerAttachSek(7333333);
	
	GenericTilesInit();
	DrvGfxROMLen[0] = 0x800000;
	DrvGfxROMLen[1] = 0x100000;

	DrvDoReset();

	return 0;
}

static void drgnwrld_gfx_decrypt()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);

	memcpy (tmp, DrvGfxROM[0], 0x400000);

	for (INT32 i = 0; i < 0x400000; i++)
	{
		DrvGfxROM[0][i] = tmp[(i & ~0x5000) | ((i & 0x1000) << 2) | ((i & 0x4000) >> 2)];
	}

	BurnFree (tmp);
}

static INT32 DrgnwrldCommonInit(void (*decrypt_cb)(), UINT16 (*igs011_prot2_cb)())
{
	AllMem = NULL;
	MemIndex(0x400000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x400000, 0);

	{
		INT32 k = 0;
		struct BurnRomInfo ri;

		if (BurnLoadRom(Drv68KROM + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		BurnDrvGetRomInfo(&ri, k);
		if (ri.nType & BRF_GRA) {
			if (BurnLoadRom(DrvGfxROM[0] + 0x400000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvSndROM + 0x00000, k++, 1)) return 1;

		if (decrypt_cb) decrypt_cb();
		drgnwrld_gfx_decrypt();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_FETCH); // read in handler
	SekMapMemory(DrvNVRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvPrioRAM,	0x200000, 0x200fff, MAP_RAM);
//	SekMapHandler(1,			0x300000, 0x3fffff, MAP_RAM);
	SekMapHandler(2,			0x400000, 0x401fff, MAP_RAM);
	SekMapHandler(3,			0xa58000, 0xa5ffff, MAP_WRITE);
	SekSetWriteWordHandler(0,	drgnwrld_main_write_word);
	SekSetWriteByteHandler(0,	drgnwrld_main_write_byte);
	SekSetReadWordHandler(0,	drgnwrld_main_read_word);
	SekSetReadByteHandler(0,	drgnwrld_main_read_byte);
//	SekSetWriteWordHandler(1,	igs011_layerram_write_word);
//	SekSetWriteByteHandler(1,	igs011_layerram_write_byte);
//	SekSetReadWordHandler(1,	igs011_layerram_read_word);
//	SekSetReadByteHandler(1,	igs011_layerram_read_byte);
	SekSetWriteWordHandler(2,	igs011_palette_write_word);
	SekSetWriteByteHandler(2,	igs011_palette_write_byte);
	SekSetReadWordHandler(2,	igs011_palette_read_word);
	SekSetReadByteHandler(2,	igs011_palette_read_byte);
	SekSetWriteWordHandler(3,	igs011_blit_write_word);
	SekSetWriteByteHandler(3, 	igs011_blit_write_byte);
	SekClose();

	igs011_prot2_read = igs011_prot2_cb;

	BurnYM3812Init(1, 3579545, NULL, 0);
	BurnTimerAttachSek(7333333);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.90, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1047619 / 132, 1);
	MSM6295SetRoute(0, 0.95, BURN_SND_ROUTE_BOTH);
	
	sound_system = 1;
	timer_irq = 5;

	GenericTilesInit();
	DrvGfxROMLen[0] = 0x400000;
	DrvGfxROMLen[1] = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	switch (sound_system)
	{
		case 0:
			ics2115_exit();
		break;

		case 1:
			BurnYM3812Exit();
			MSM6295Exit(0);
		break;
	}

	timer_irq = 3;
	sound_system = 0;
	igs011_prot2_read = NULL;

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (int i = 0; i < 0x1000/2; i++)
	{
		UINT16 p = (DrvPalRAM[0x800 + i] << 8) | (DrvPalRAM[i]);

		DrvPalette[i] = BurnHighCol(pal5bit(p >>  0), pal5bit(p >>  5), pal5bit(p >> 10), 0);
	}
}

template<class T>
const T& min(const T& a, const T& b)
{
    return (b < a) ? b : a;
}

static void screen_update()
{
	UINT16 *priority_ram = (UINT16*)DrvPrioRAM;
	UINT16 const *const pri_ram = &priority_ram[(priority & 7) * 512/2];
	unsigned const hibpp_layers = min(4 - (s_blitter.depth & 0x07), 0x20000);

	if (BIT(s_blitter.depth, 4))
	{
		UINT16 const pri = pri_ram[0xff] & 7;
		BurnTransferClear((pri << 8) | 0xff);
		return;
	}

	UINT8 layerpix[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	for (int y = 0; y < nScreenHeight; y++)
	{
		for (int x = 0; x < nScreenWidth; x++)
		{
			const int scr_addr = (y << 9) | x;
			int pri_addr = 0xff;

			int l = 0;
			unsigned i = 0;
			while (hibpp_layers > i)
			{
				layerpix[l] = DrvLayerRAM[i++][scr_addr];
				if (layerpix[l] != 0xff)
					pri_addr &= ~(1 << l);
				++l;
			}

			while (4 > i)
			{
				UINT8 const pixdata = DrvLayerRAM[i++][scr_addr];
				layerpix[l] = pixdata & 0x0f;
				if (layerpix[l] != 0x0f)
					pri_addr &= ~(1 << l);
				++l;
				layerpix[l] = (pixdata >> 4) & 0x0f;
				if (layerpix[l] != 0x0f)
					pri_addr &= ~(1 << l);
				++l;
			}

			UINT16 const pri = pri_ram[pri_addr] & 7;
			pTransDraw[y * nScreenWidth + x] = layerpix[pri] | (pri << 8);
		}
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	screen_update();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 260;
	INT32 nCyclesTotal[1] = { 7333333 / 60 };
//	INT32 nCyclesDone[1] = { 0 };

	SekNewFrame();
	
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN_TIMER(0);

		if (i == 64 || i == 129 || i == 194 || i == 259) SekSetIRQLine(timer_irq, CPU_IRQSTATUS_AUTO);

		if (i == 239) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
	}

	if (pBurnSoundOut) {
		switch (sound_system)
		{
			case 0:
				ics2115_update(nBurnSoundLen);
			break;

			case 1:
				BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
				MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
			break;
		}
	}

	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_ROM) {	
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x80000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {	
		ba.Data		= DrvLayerRAM[0];
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x300000;
		ba.szName	= "Layer RAM 0";
		BurnAcb(&ba);
		
		ba.Data		= DrvLayerRAM[1];
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x340000;
		ba.szName	= "Layer RAM 1";
		BurnAcb(&ba);

		ba.Data		= DrvLayerRAM[2];
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x360000;
		ba.szName	= "Layer RAM 2";
		BurnAcb(&ba);
		
		ba.Data		= DrvLayerRAM[3];
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x380000;
		ba.szName	= "Layer RAM 3";
		BurnAcb(&ba);
		
		ba.Data			= DrvPalRAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0x400000;
		ba.szName		= "Palette Regs";
		BurnAcb(&ba);
		
		ba.Data			= DrvPrioRAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0x200000;
		ba.szName		= "Priority RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data			= DrvNVRAM;
		ba.nLen			= 0x004000;
		ba.nAddress		= 0x100000;
		ba.szName		= "NV/WORK RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);

		switch (sound_system)
		{
			case 0:
				ics2115_scan(nAction, pnMin);
			break;

			case 1:
				BurnYM3812Scan(nAction, pnMin);
				MSM6295Scan(nAction, pnMin);
			break;
		}

		SCAN_VAR(igs012_prot);
		SCAN_VAR(igs012_prot_swap);
		SCAN_VAR(igs012_prot_mode);
		SCAN_VAR(igs011_prot2);
		SCAN_VAR(igs011_prot1);
		SCAN_VAR(igs011_prot1_swap);
		SCAN_VAR(igs011_prot1_addr);
		SCAN_VAR(igs003_reg);
		SCAN_VAR(igs003_prot_hold);
		SCAN_VAR(igs003_prot_z);
		SCAN_VAR(igs003_prot_y);
		SCAN_VAR(igs003_prot_x);
		SCAN_VAR(igs003_prot_h1);
		SCAN_VAR(igs003_prot_h2);
		SCAN_VAR(priority);
		SCAN_VAR(s_blitter);
		SCAN_VAR(dips_select);
		SCAN_VAR(trackball);
	}

 	return 0;
}


// Virtua Bowling (World, V101XCM)

static struct BurnRomInfo vbowlRomDesc[] = {
	{ "bowlingv101xcm.u45",	0x080000, 0xab8e3f1f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "vrbowlng.u69",		0x400000, 0xb0d339e8, 2 | BRF_GRA },           //  1 Blitter Data
	{ "vrbowlng.u68",		0x100000, 0xb0ce27e7, 3 | BRF_GRA },           //  2

	{ "vrbowlng.u67",		0x080000, 0x53000936, 4 | BRF_SND },           //  3 ICS2115 Samples
	{ "vrbowlng.u66",		0x080000, 0xf62cf8ed, 4 | BRF_SND },           //  4
};

STD_ROM_PICK(vbowl)
STD_ROM_FN(vbowl)

struct BurnDriver BurnDrvVbowl = {
	"vbowl", NULL, NULL, NULL, "1996",
	"Virtua Bowling (World, V101XCM)\0", "Sound doesn't work", "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, vbowlRomInfo, vbowlRomName, NULL, NULL, NULL, NULL, VbowlInputInfo, VbowlDIPInfo,
	VbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Virtua Bowling (Japan, V100JCM)

static struct BurnRomInfo vbowljRomDesc[] = {
	{ "vrbowlng.u45",		0x080000, 0x091c19c1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "vrbowlng.u69",		0x400000, 0xb0d339e8, 2 | BRF_GRA },           //  1 Blitter Data
	{ "vrbowlng.u68",		0x100000, 0xb0ce27e7, 3 | BRF_GRA },           //  2

	{ "vrbowlng.u67",		0x080000, 0x53000936, 4 | BRF_SND },           //  3 ICS2115 Samples
	{ "vrbowlng.u66",		0x080000, 0xf62cf8ed, 4 | BRF_SND },           //  4
};

STD_ROM_PICK(vbowlj)
STD_ROM_FN(vbowlj)

struct BurnDriver BurnDrvVbowlj = {
	"vbowlj", "vbowl", NULL, NULL, "1996",
	"Virtua Bowling (Japan, V100JCM)\0", "Sound doesn't work", "IGS / Alta", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, vbowljRomInfo, vbowljRomName, NULL, NULL, NULL, NULL, VbowlInputInfo, VbowljDIPInfo,
	VbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Virtua Bowling (Hong Kong, V101HJS)

static struct BurnRomInfo vbowlhkRomDesc[] = {
	{ "bowlingv101hjs.u45",	0x080000, 0x92fbfa72, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "vrbowlng.u69",		0x400000, 0xb0d339e8, 2 | BRF_GRA },           //  1 Blitter Data
	{ "vrbowlng.u68",		0x100000, 0xb0ce27e7, 3 | BRF_GRA },           //  2

	{ "vrbowlng.u67",		0x080000, 0x53000936, 4 | BRF_SND },           //  3 ICS2115 Samples
	{ "vrbowlng.u66",		0x080000, 0xf62cf8ed, 4 | BRF_SND },           //  4
};

STD_ROM_PICK(vbowlhk)
STD_ROM_FN(vbowlhk)

struct BurnDriver BurnDrvVbowlhk = {
	"vbowlhk", "vbowl", NULL, NULL, "1996",
	"Virtua Bowling (Hong Kong, V101HJS)\0", "Sound doesn't work", "IGS / Tai Tin Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, vbowlhkRomInfo, vbowlhkRomName, NULL, NULL, NULL, NULL, VbowlInputInfo, VbowlhkDIPInfo,
	VbowlhkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dragon World (World, V040O)

static struct BurnRomInfo drgnwrldRomDesc[] = {
	{ "chinadr-v0400.u3",	0x080000, 0xa6daa2b8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
};

STD_ROM_PICK(drgnwrld)
STD_ROM_FN(drgnwrld)

static INT32 DrgnwrldInit()
{
	return DrgnwrldCommonInit(drgnwrld_type1_decrypt, drgnwrldv20j_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrld = {
	"drgnwrld", NULL, NULL, NULL, "1997",
	"Dragon World (World, V040O)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldRomInfo, drgnwrldRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldDIPInfo,
	DrgnwrldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dongbang Jiju (Korea, V040K)

static struct BurnRomInfo drgnwrldv40kRomDesc[] = {
	{ "v-040k.u3",			0x080000, 0x397404ef, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples

	{ "ccdu15.u15",			0x0002e5, 0xa15fce69, 0 | BRF_OPT },           //  3 PLDs
	{ "ccdu45.u45", 		0x0002e5, 0xa15fce69, 0 | BRF_OPT },           //  4
};

STD_ROM_PICK(drgnwrldv40k)
STD_ROM_FN(drgnwrldv40k)

static INT32 Drgnwrldv40kInit()
{
	return DrgnwrldCommonInit(drgnwrldv40k_decrypt, drgnwrldv40k_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv40k = {
	"drgnwrldv40k", "drgnwrld", NULL, NULL, "1995",
	"Dongbang Jiju (Korea, V040K)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv40kRomInfo, drgnwrldv40kRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldcDIPInfo,
	Drgnwrldv40kInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dragon World (World, V030O)

static struct BurnRomInfo drgnwrldv30RomDesc[] = {
	{ "chinadr-v0300.u3",	0x080000, 0x5ac243e5, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv30)
STD_ROM_FN(drgnwrldv30)

struct BurnDriver BurnDrvDrgnwrldv30 = {
	"drgnwrldv30", "drgnwrld", NULL, NULL, "1995",
	"Dragon World (World, V030O)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv30RomInfo, drgnwrldv30RomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldDIPInfo,
	DrgnwrldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dragon World (World, V021O)

static struct BurnRomInfo drgnwrldv21RomDesc[] = {
	{ "china-dr-v-0210.u3",	0x080000, 0x60c2b018, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv21)
STD_ROM_FN(drgnwrldv21)

static INT32 Drgnwrldv21Init()
{
	return DrgnwrldCommonInit(drgnwrld_type2_decrypt, drgnwrldv21_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv21 = {
	"drgnwrldv21", "drgnwrld", NULL, NULL, "1995",
	"Dragon World (World, V021O)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv21RomInfo, drgnwrldv21RomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldDIPInfo,
	Drgnwrldv21Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Chuugokuryuu (Japan, V021J)

static struct BurnRomInfo drgnwrldv21jRomDesc[] = {
	{ "v-021j",				0x080000, 0x2f87f6e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data
	{ "cg",           		0x020000, 0x2dda0be3, 2 | BRF_GRA },           //  2

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  3 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv21j)
STD_ROM_FN(drgnwrldv21j)

static INT32 Drgnwrldv21jInit()
{
	return DrgnwrldCommonInit(drgnwrld_type3_decrypt, drgnwrldv20j_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv21j = {
	"drgnwrldv21j", "drgnwrld", NULL, NULL, "1995",
	"Chuugokuryuu (Japan, V021J)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv21jRomInfo, drgnwrldv21jRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldjDIPInfo,
	Drgnwrldv21jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Chuugokuryuu (Japan, V020J)

static struct BurnRomInfo drgnwrldv20jRomDesc[] = {
	{ "china_jp.v20",		0x080000, 0x9e018d1a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data
	{ "china.u44",     		0x020000, 0x10549746, 2 | BRF_GRA },           //  2

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  3 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv20j)
STD_ROM_FN(drgnwrldv20j)

static INT32 Drgnwrldv20jInit()
{
	return DrgnwrldCommonInit(drgnwrld_type3_decrypt, drgnwrldv20j_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv20j = {
	"drgnwrldv20j", "drgnwrld", NULL, NULL, "1995",
	"Chuugokuryuu (Japan, V020J)\0", NULL, "IGS / Alta", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv20jRomInfo, drgnwrldv20jRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldjDIPInfo,
	Drgnwrldv20jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dung Fong Zi Zyu (Hong Kong, V011H, set 1)

static struct BurnRomInfo drgnwrldv11hRomDesc[] = {
	{ "c_drgn_hk.u3",		0x080000, 0x182037ce, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv11h)
STD_ROM_FN(drgnwrldv11h)

static INT32 Drgnwrldv11hInit()
{
	return DrgnwrldCommonInit(drgnwrld_type1_decrypt, drgnwrldv20j_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv11h = {
	"drgnwrldv11h", "drgnwrld", NULL, NULL, "1995",
	"Dung Fong Zi Zyu (Hong Kong, V011H, set 1)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv11hRomInfo, drgnwrldv11hRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldcDIPInfo,
	Drgnwrldv11hInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Dung Fong Zi Zyu (Hong Kong, V011H, set 2)

static struct BurnRomInfo drgnwrldv11haRomDesc[] = {
	{ "u3",			 		0x080000, 0xb68113c4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "china-dr-sp.u43",	0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
};

STD_ROM_PICK(drgnwrldv11ha)
STD_ROM_FN(drgnwrldv11ha)

struct BurnDriver BurnDrvDrgnwrldv11ha = {
	"drgnwrldv11ha", "drgnwrld", NULL, NULL, "1995",
	"Dung Fong Zi Zyu (Hong Kong, V011H, set 2)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv11haRomInfo, drgnwrldv11haRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldcDIPInfo,
	Drgnwrldv40kInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};


// Zhongguo Long (China, V010C)

static struct BurnRomInfo drgnwrldv10cRomDesc[] = {
	{ "igs-d0303.u3",		0x080000, 0x3b3c29bb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs-d0301.u39",		0x400000, 0x78ab45d9, 2 | BRF_GRA },           //  1 Blitter Data

	{ "igs-s0302.u43",		0x040000, 0xfde63ce1, 4 | BRF_SND },           //  2 MSM6295 Samples
	
	{ "ccdu15.u15", 		0x0002e5, 0xa15fce69, 0 | BRF_OPT },           //  3 PLDs
	{ "ccdu45.u45", 		0x0002e5, 0xa15fce69, 0 | BRF_OPT },           //  4
};

STD_ROM_PICK(drgnwrldv10c)
STD_ROM_FN(drgnwrldv10c)

static INT32 Drgnwrldv10cInit()
{
	return DrgnwrldCommonInit(drgnwrld_type1_decrypt, drgnwrldv20j_igs011_prot2_read);
}

struct BurnDriver BurnDrvDrgnwrldv10c = {
	"drgnwrldv10c", "drgnwrld", NULL, NULL, "1995",
	"Zhongguo Long (China, V010C)\0", NULL, "IGS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drgnwrldv10cRomInfo, drgnwrldv10cRomName, NULL, NULL, NULL, NULL, DrgnwrldInputInfo, DrgnwrldcDIPInfo,
	Drgnwrldv10cInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 240, 4, 3
};
