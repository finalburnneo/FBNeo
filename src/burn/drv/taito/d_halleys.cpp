// FinalBurn Neo Halley's Comet driver module
// Based on MAME driver by Phil Stroffolino, Acho A. Tang

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxPlanes[2];
static UINT8 *DrvColPROM;

static UINT8 *DrvM6809RAM;
static UINT8 *DrvBlitterRAM;
static UINT8 *DrvPaletteRAM;
static UINT8 *DrvIORAM;	
static UINT8 *DrvBGTileRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *PaletteLut;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *scrollx0, *scrolly0, *scrollx1, *scrolly1;

static INT32 collision_detection;
static INT32 collision_count;
static UINT8 *collision_list;
static INT32 firq_level;
static INT32 soundtimer;
static INT32 stars_enabled;
static INT32 vector_type;
static INT32 blitter_busy;

static INT32 is_halleys;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static INT32 nCyclesExtra[2];

static struct BurnInputInfo HalleysInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"			},
};

STDINPUTINFO(Halleys)

static struct BurnDIPInfo BenberobDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xcf, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xfd, NULL							},
	{0x03, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x00, 0x01, 0x03, 0x02, "Every 100k"					},
	{0x00, 0x01, 0x03, 0x03, "100k 300k 200k+"				},
	{0x00, 0x01, 0x03, 0x00, "100k 200k"					},
	{0x00, 0x01, 0x03, 0x01, "150k 300k"					},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x00, 0x01, 0x04, 0x04, "No"							},
	{0x00, 0x01, 0x04, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x18, 0x08, "3"							},
	{0x00, 0x01, 0x18, 0x10, "4"							},
	{0x00, 0x01, 0x18, 0x18, "5"							},
	{0x00, 0x01, 0x18, 0x00, "Infinite"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x00, 0x01, 0x40, 0x40, "Off"							},
	{0x00, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x00, 0x01, 0x80, 0x80, "Upright"						},
	{0x00, 0x01, 0x80, 0x00, "Cocktail"						},

	{0   , 0xfe, 0   ,   16, "Coin A"						},
	{0x01, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"			},
	{0x01, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"			},
	{0x01, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"			},
	{0x01, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"			},
	{0x01, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"			},
	{0x01, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"			},
	{0x01, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"			},
	{0x01, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"			},

	{0   , 0xfe, 0   ,   16, "Coin B"						},
	{0x01, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"			},
	{0x01, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"			},
	{0x01, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"			},
	{0x01, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"			},
	{0x01, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"			},
	{0x01, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"			},
	{0x01, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"			},
	{0x01, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x02, 0x01, 0x01, 0x01, "Easy"							},
	{0x02, 0x01, 0x01, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Use Both Buttons"				},
	{0x02, 0x01, 0x02, 0x02, "No"							},
	{0x02, 0x01, 0x02, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    4, "Timer Speed"					},
	{0x02, 0x01, 0x0c, 0x0c, "Slowest"						},
	{0x02, 0x01, 0x0c, 0x08, "Slow"							},
	{0x02, 0x01, 0x0c, 0x04, "Fast"							},
	{0x02, 0x01, 0x0c, 0x00, "Fastest"						},

	{0   , 0xfe, 0   ,    2, "Show Coinage"					},
	{0x02, 0x01, 0x10, 0x00, "No"							},
	{0x02, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Show Year"					},
	{0x02, 0x01, 0x20, 0x00, "No"							},
	{0x02, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    2, "No Hit"						},
	{0x02, 0x01, 0x40, 0x40, "Off"							},
	{0x02, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Maximum Credits"				},
	{0x02, 0x01, 0x80, 0x80, "9"							},
	{0x02, 0x01, 0x80, 0x00, "16"							},

	{0   , 0xfe, 0   ,    2, "Coin Slots"					},
	{0x03, 0x01, 0x20, 0x00, "1"							},
	{0x03, 0x01, 0x20, 0x20, "2"							},
};

STDDIPINFO(Benberob)

static struct BurnDIPInfo HalleysDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xfe, NULL							},
	{0x01, 0xff, 0xff, 0xff, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},
	{0x03, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x00, 0x01, 0x01, 0x00, "Upright"						},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x00, 0x01, 0x02, 0x02, "Off"							},
	{0x00, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x04, 0x04, "Off"							},
	{0x00, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x00, 0x01, 0x08, 0x00, "Off"							},
	{0x00, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"			},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"			},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x01, 0x01, 0x03, 0x02, "Easy"							},
	{0x01, 0x01, 0x03, 0x03, "Medium"						},
	{0x01, 0x01, 0x03, 0x01, "Hard"							},
	{0x01, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x01, 0x01, 0x0c, 0x00, "100k 600k 500k+"				},
	{0x01, 0x01, 0x0c, 0x0c, "200k 800k 600k+"				},
	{0x01, 0x01, 0x0c, 0x08, "200k 1000k 800k+"				},
	{0x01, 0x01, 0x0c, 0x04, "200k 1200k 1000k+"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x01, 0x01, 0x30, 0x20, "2"							},
	{0x01, 0x01, 0x30, 0x30, "3"							},
	{0x01, 0x01, 0x30, 0x10, "4"							},
	{0x01, 0x01, 0x30, 0x00, "4 (No Points Bonus Lives)"	},

	{0   , 0xfe, 0   ,    2, "Operation Data Recorder"		},
	{0x01, 0x01, 0x40, 0x40, "Not fixed"					},
	{0x01, 0x01, 0x40, 0x00, "When fixed"					},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x02, 0x01, 0x02, 0x02, "Off"							},
	{0x02, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Start Area"					},
	{0x02, 0x01, 0x1c, 0x1c, "1 (Earth)"					},
	{0x02, 0x01, 0x1c, 0x18, "4 (Venus)"					},
	{0x02, 0x01, 0x1c, 0x14, "7 (Mercury)"					},
	{0x02, 0x01, 0x1c, 0x10, "10 (Sun)"						},
	{0x02, 0x01, 0x1c, 0x0c, "13 (Pluto)"					},
	{0x02, 0x01, 0x1c, 0x08, "16 (Neptune)"					},
	{0x02, 0x01, 0x1c, 0x04, "19 (Uranus)"					},
	{0x02, 0x01, 0x1c, 0x00, "22 (Saturn)"					},

	{0   , 0xfe, 0   ,    2, "Invincibility"				},
	{0x02, 0x01, 0x40, 0x40, "Off"							},
	{0x02, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"				},
	{0x02, 0x01, 0x80, 0x80, "Off"							},
	{0x02, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Coin Slots"					},
	{0x03, 0x01, 0x20, 0x00, "1"							},
	{0x03, 0x01, 0x20, 0x20, "2"							},
};

STDDIPINFO(Halleys)


#define SCREEN_WIDTH_L2 8
#define SCREEN_HEIGHT 256
#define VIS_MINX      0
#define VIS_MAXX      255
#define VIS_MINY      8
#define VIS_MAXY      (255-8)

#define MAX_LAYERS    6
#define MAX_SPRITES   256

#define SP_2BACK     0x100
#define SP_ALPHA     0x200
#define SP_COLLD     0x300

#define BG_MONO      0x400
#define BG_RGB       0x500

#define PALETTE_SIZE 0x600

#define SCREEN_WIDTH        (1 << SCREEN_WIDTH_L2)
#define SCREEN_SIZE         (SCREEN_HEIGHT << SCREEN_WIDTH_L2)
#define SCREEN_BYTEWIDTH_L2 (SCREEN_WIDTH_L2 + 1)
#define SCREEN_BYTEWIDTH    (1 << SCREEN_BYTEWIDTH_L2)
#define SCREEN_BYTESIZE     (SCREEN_HEIGHT << SCREEN_BYTEWIDTH_L2)
#define CLIP_SKIP           (VIS_MINY * SCREEN_WIDTH + VIS_MINX)
#define CLIP_W              (VIS_MAXX - VIS_MINX + 1)
#define CLIP_H              (VIS_MAXY - VIS_MINY + 1)

static void blit(INT32 offset)
{
// status attributes
#define ACTIVE 0x02
#define DECAY  0x10
#define RETIRE 0x80

// mode attributes
#define IGNORE_0 0x01
#define MODE_02  0x02
#define COLOR_ON 0x04
#define MIRROR_X 0x08
#define MIRROR_Y 0x10
#define MODE_40  0x40
#define GROUP    0x80

// color attributes
#define PENCOLOR 0x0f
#define BANKBIT1 0x80

// code attributes
#define COMMAND  0x0f
#define BGLAYER  0x10
#define BANKBIT0 0x40
#define PLANE    0x80

// special draw commands
#define EFX1      0x8 // draws to back??
#define HORIZBAR  0xe // draws 8-bit RGB horizontal bars
#define STARPASS1 0x3 // draws the stars
#define STARPASS2 0xd // modifies stars color
#define TILEPASS1 0x5 // draws tiles(s1: embedded 8-bit RGB, s2: bit-packed pixel data)
#define TILEPASS2 0xb // modifies tiles color/alpha

// user flags
#define FLIP_X     0x0100
#define FLIP_Y     0x0200
#define SINGLE_PEN 0x0400
#define RGB_MASK   0x0800
#define AD_HIGH    0x1000
#define SY_HIGH    0x2000
#define S1_IDLE    0x4000
#define S1_REV     0x8000
#define S2_IDLE    0x10000
#define S2_REV     0x20000
#define BACKMODE   0x40000
#define ALPHAMODE  0x80000
#define PPCD_ON    0x100000

// hard-wired defs and interfaces
#define HALLEYS_SPLIT   0xf4
#define COLLISION_PORTA 0x67
#define COLLISION_PORTB 0x8b

// short cuts
#define XMASK (SCREEN_WIDTH-1)
#define YMASK (SCREEN_HEIGHT-1)

	static const UINT8 penxlat[16]={0x03,0x07,0x0b,0x0f,0x02,0x06,0x0a,0x0e,0x01,0x05,0x09,0x0d,0x00,0x04,0x08,0x0c};
	static const UINT8 rgbmask[16]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0xff,0xff,0xff,0xff,0xff};
	static const UINT8 tyremap[ 8]={ 0, 5, 9,13,16,20,24,28};

	UINT16 *render_layer[6] = { BurnBitmapGetBitmap(1), BurnBitmapGetBitmap(2), BurnBitmapGetBitmap(3), BurnBitmapGetBitmap(4), BurnBitmapGetBitmap(5), BurnBitmapGetBitmap(6) };

	UINT8 *param, *src_base;
	UINT16 *dst_base;
	INT32 stptr, mode, color, code, y, x, h, w, src1, src2;
	INT32 status, flags, group, command, bank, layer, pen0, pen1;
	INT32 yclip, xclip, src_yskip, src_xskip, dst_skip, hclip, wclip, src_dy, src_dx;
	UINT32 *pal_ptr;
	UINT8 *src1_ptr, *src2_ptr; 	// esi alias, ebx alias
	UINT16 *dst_ptr;             	// edi alias
	void *edi;                 	// scratch
	INT32 eax, ebx, ecx, edx;    	// scratch
	UINT16 ax; UINT8 al, ah;      	// partial regs

	param = DrvBlitterRAM + offset;

	// init blitter controls
	stptr = (INT32)(param[0x2]<<8) | (INT32)param[0x3];
	mode  = (INT32)param[0x5];
	color = (INT32)param[0x6];
	code  = (INT32)param[0x7];

	// update sprite status
	layer = (code>>3 & 2) | (code>>7 & 1);
	offset >>= 4;

	//bprintf(0, _T("blit: stptr  %x  layer  %x\n"), stptr, layer);
	status = (stptr) ? M6809ReadByte(stptr) : ACTIVE;
	flags = mode;
	group = mode & GROUP;
	command = code & COMMAND;
	if (is_halleys)
	{
		if (!layer) flags |= PPCD_ON;
		if (offset >= HALLEYS_SPLIT) flags |= AD_HIGH; else
		if (offset & 1) memcpy(param-0x10, param, 0x10); else group = param[0x15] & GROUP;

		// HACK: force engine flame in group zero to increase collision accuracy
		if (offset == 0x1a || offset == 0x1b) group = 0;
	}
	else if (offset & 1) memcpy(param-0x10, param, 0x10);

	// init draw parameters
	y = (INT32)param[0x8];
	x = (INT32)param[0x9];
	h = (INT32)param[0xa];
	w = (INT32)param[0xb] + 1;
	if (!h) return;

	// translate source addresses
	src1 = (INT32)param[0xc]<<8 | (INT32)param[0xd];
	src2 = (INT32)param[0xe]<<8 | (INT32)param[0xf];
	flags |= src1 & (S1_IDLE | S1_REV);
	flags |= src2<<2 & (S2_IDLE | S2_REV);
	src1 &= 0x3fff;
	src2 &= 0x3fff;
	bank = ((code & BANKBIT0) | (color & BANKBIT1)) << 8;
	pal_ptr = PaletteLut; //m_internal_palette.get();

	// the crossroad of fate
	if (!(code & BGLAYER || command & 7))
	{
		// reject off-screen objects
		if (flags & MIRROR_Y) { flags |= FLIP_Y; y -= (h - 1); }
		if (flags & MIRROR_X) { flags |= FLIP_X; x -= (w - 1); }
		if (y > VIS_MAXY || (y + h) <= VIS_MINY) return;
		if (x > VIS_MAXX || (x + w) <= VIS_MINX) return;

		// clip objects against the visible area
		yclip = y; xclip = x; hclip = h; wclip = w;
		src_yskip = src_xskip = 0;
		if (yclip < VIS_MINY) { src_yskip = VIS_MINY - yclip; yclip = VIS_MINY; hclip -= src_yskip; }
		if (yclip + hclip > VIS_MAXY+1) { hclip = VIS_MAXY+1 - yclip; }
		if (xclip < VIS_MINX) { src_xskip = VIS_MINX - xclip; xclip = VIS_MINX; wclip -= src_xskip; }
		if (xclip + wclip > VIS_MAXX+1) { wclip = VIS_MAXX+1 - xclip; }
		dst_skip = (yclip << SCREEN_WIDTH_L2) + xclip;

		// adjust orientations
		eax = 0;
		if (flags & (S1_REV | S2_REV)) { flags ^= FLIP_Y | FLIP_X; eax -= w * h - 8; }

		if (flags & FLIP_Y)
		{
			eax += w * (h - 1);
			src_yskip = -src_yskip;
			src_dy = (flags & FLIP_X) ? -w + wclip : -w - wclip;
		}
		else src_dy = (flags & FLIP_X) ? w + wclip : w - wclip;

		if (flags & FLIP_X)
		{
			eax += w - 1;
			src_xskip = -src_xskip;
			src_dx = -1;
		}
		else src_dx = 1;

		// calculate entry points and loop constants
		src1_ptr = DrvGfxPlanes[0] + ((bank + src1)<<3) + eax;
		src2_ptr = DrvGfxPlanes[1] + ((bank + src2)<<3) + eax;

		if (!(flags & (S1_IDLE | S2_IDLE)))
		{
			eax = src_yskip * w + src_xskip;
			src1_ptr += eax;
			src2_ptr += eax;
		}
		else src_dy = src_dx = 0;

		dst_ptr = render_layer[layer] + dst_skip;

		// look up pen values and set rendering flags
		pen0 = code>>3 & 0x10;
		pen1 = 0;
		if (command == EFX1) { flags |= BACKMODE; pen0 |= SP_2BACK; }
		if (src1 == src2)
		{
			flags |= SINGLE_PEN;
			eax = (UINT32)penxlat[color & PENCOLOR];
			if (eax) pen1 = pen0 + eax;
		}
		else if (color & PENCOLOR) flags |= RGB_MASK;

#define BLOCK_WIPE_COMMON { \
	eax = wclip<<1; \
	ecx = hclip; \
	edi = dst_ptr; \
	do { memset((UINT8*)edi, 0, eax); edi = (UINT8*)edi + SCREEN_BYTEWIDTH; } while (--ecx); \
}

		// multi-pen block or transparent blit
		if ((flags & (SINGLE_PEN | RGB_MASK | COLOR_ON)) == COLOR_ON)
		{
			if (!(flags & IGNORE_0)) BLOCK_WIPE_COMMON

			dst_ptr += wclip;
			ecx = wclip = -wclip;
			edx = src_dx;

			if (!(flags & PPCD_ON))
			{
				al = ah = (UINT8)pen0;

				if (!(flags & BACKMODE))
				{
					do {
						do {
							al |= *src1_ptr;
							src1_ptr += edx;
							al |= *src2_ptr;
							src2_ptr += edx;
							if (al & 0xf) { dst_ptr[ecx] = (UINT16)al;  al = ah;}
						}
						while (++ecx);
						ecx = wclip; src1_ptr += src_dy; src2_ptr += src_dy; dst_ptr += SCREEN_WIDTH;
					}
					while (--hclip);
				}
				else
				{
					do {
						do {
							al |= *src1_ptr;
							src1_ptr += edx;
							al |= *src2_ptr;
							src2_ptr += edx;
							if (al & 0xf) { dst_ptr[ecx] = (UINT16)al | SP_2BACK;  al = ah; }
						}
						while (++ecx);
						ecx = wclip; src1_ptr += src_dy; src2_ptr += src_dy; dst_ptr += SCREEN_WIDTH;
					}
					while (--hclip);
				}
				return;
			}
			ax = 0;
			if (group)
			{
				do {
					do {
						al = *src1_ptr;
						src1_ptr += edx;
						al |= *src2_ptr;
						src2_ptr += edx;
						if (al & 0xf) { dst_ptr[ecx] = (UINT16)al | SP_COLLD; } // set collision flag on group one pixels
					}
					while (++ecx);
					ecx = wclip; src1_ptr += src_dy; src2_ptr += src_dy; dst_ptr += SCREEN_WIDTH;
				}
				while (--hclip);
			}
			else
			{
				do {
					do {
						al = *src1_ptr;
						src1_ptr += edx;
						al |= *src2_ptr;
						src2_ptr += edx;
						if (al & 0xf) { ax |= dst_ptr[ecx]; dst_ptr[ecx] = (UINT16)al; } // combine collision flags in ax
					}
					while (++ecx);
					ecx = wclip; src1_ptr += src_dy; src2_ptr += src_dy; dst_ptr += SCREEN_WIDTH;
				}
				while (--hclip);
			}

			// update collision list if object collided with the other group
			if (status & ACTIVE && ax & SP_COLLD)
			{
				collision_list[collision_count & (MAX_SPRITES-1)] = offset;
				collision_count++;
			}
		}
		else if ((flags & (RGB_MASK | COLOR_ON)) == RGB_MASK + COLOR_ON)		// multi-pen, RGB masked block or transparent blit
		{
			if (!(flags & IGNORE_0)) BLOCK_WIPE_COMMON
			dst_ptr += wclip;
			ecx = wclip = -wclip;
			al = ah = (UINT8)pen0;
			ebx = rgbmask[color & PENCOLOR] | 0xffffff00;

			do {
				do {
					al |= *src1_ptr;
					src1_ptr += src_dx;
					al |= *src2_ptr;
					src2_ptr += src_dx;
					if (al & 0xf) { edx = (UINT32)al;  al = ah;  dst_ptr[ecx] = pal_ptr[edx] & ebx; }
				}
				while (++ecx);
				ecx = wclip; src1_ptr += src_dy; src2_ptr += src_dy; dst_ptr += SCREEN_WIDTH;
			}
			while (--hclip);
		}
		else if ((flags & (SINGLE_PEN | COLOR_ON)) == SINGLE_PEN + COLOR_ON)	// single-pen block or transparent blit
		{
			if (!(flags & IGNORE_0)) BLOCK_WIPE_COMMON
			dst_ptr += wclip;
			ebx = hclip;
			ecx = wclip = -wclip;
			edx = src_dx;
			ax = (UINT16)pen1;

			do {
				do {
					if (*src1_ptr) dst_ptr[ecx] = ax;
					src1_ptr += edx;
				}
				while (++ecx);

				ecx = wclip;
				src1_ptr += src_dy;
				dst_ptr  += SCREEN_WIDTH;
			}
			while (--ebx);
		}
		else if ((flags & (IGNORE_0 | COLOR_ON)) == IGNORE_0)	// transparent wipe
		{
			dst_ptr += wclip;
			wclip = -wclip;
			ecx = wclip;
			edx = src_dx;

			if (flags & PPCD_ON && !group)
			{
				// preserve collision flags when wiping group zero objects
				do {
					do {
						al = *src1_ptr;
						ah = *src2_ptr;
						src1_ptr += edx;
						src2_ptr += edx;
						if (al | ah) dst_ptr[ecx] &= SP_COLLD;
					}
					while (++ecx);

					ecx = wclip;
					src1_ptr += src_dy;
					src2_ptr += src_dy;
					dst_ptr  += SCREEN_WIDTH;
				}
				while (--hclip);
			}
			else
			{
				do {
					do {
						al = *src1_ptr;
						ah = *src2_ptr;
						src1_ptr += edx;
						src2_ptr += edx;
						if (al | ah) dst_ptr[ecx] = 0;
					}
					while (++ecx);

					ecx = wclip;
					src1_ptr += src_dy;
					src2_ptr += src_dy;
					dst_ptr  += SCREEN_WIDTH;
				}
				while (--hclip);
			}
		} else if ((flags & (IGNORE_0 | COLOR_ON)) == 0) BLOCK_WIPE_COMMON

		// End of Standard Mode
		return;
	}

#define GFX_HI 0x10000

	// reject illegal blits and adjust parameters
	if (command)
	{
		if (h > 8) return;
		if (y >= 0xf8) { y -= 0xf8; flags |= SY_HIGH; } else
		if (y >= 8) return;
		if (command != TILEPASS1) { h <<= 5; y <<= 5; }
	}

	// calculate entry points and loop constants
	if (flags & S1_IDLE) src_dx = 0; else src_dx = 1;
	if (flags & S1_REV ) src_dx = -src_dx;

	src_base = DrvGfxROM + bank;

	if (command == STARPASS1 || command == STARPASS2) layer = (layer & 1) + 4;
	dst_base = render_layer[layer];

#define WARP_WIPE_COMMON { \
	ebx = y + h; \
	ecx = (x + w - SCREEN_WIDTH) << 1; \
	for (edx=y; edx<ebx; edx++) { \
		dst_ptr = dst_base + ((edx & YMASK) << SCREEN_WIDTH_L2); \
		if (ecx > 0) { memset(dst_ptr, 0, ecx); eax = SCREEN_WIDTH - x; } else eax = w; \
		memset(dst_ptr+x, 0, eax<<1); \
	} \
}

	if ((flags & (IGNORE_0 | COLOR_ON)) == 0)	// specialized block wipe
	{
		if (!command && y == 0xff && h == 1)		// wipe star layer when the following conditions are met:
		{
			y = 0; h = SCREEN_HEIGHT;
			dst_base = render_layer[(layer&1)+4];
			WARP_WIPE_COMMON

			stars_enabled = ~layer & 1;
		}
		else		// wipe background and chain-wipe corresponding sprite layer when the command is zero
		{
			WARP_WIPE_COMMON
			if (!command) { dst_base = render_layer[layer&1]; WARP_WIPE_COMMON }
		}
	}
	else if (command == STARPASS1 && flags & COLOR_ON)	// gray shaded star map with 16x16 cells
	{
		#define RORB(R,N) ( ((R>>N)|(R<<(8-N))) & 0xff )
		#define C2S(X,Y,O) ( (((Y+(O>>4))&YMASK)<<SCREEN_WIDTH_L2) + ((X+(O&0xf))&XMASK) )

		src2_ptr = src_base + src2 + GFX_HI;

		for (yclip=y+h; y<yclip; y+=16)
		for (xclip=x+w; x<xclip; x+=16)
		{
			edx = (UINT32)*src2_ptr;
			src2_ptr++;
			if (edx)
			{
				ax = (UINT16)*(src2_ptr-0x100 -1);
				ebx = (UINT32)ax;
				ax |= BG_MONO;
				if (edx & 0x01) {                    dst_base[C2S(x,y,ebx)] = ax; }
				if (edx & 0x02) { ecx = RORB(ebx,1); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x04) { ecx = RORB(ebx,2); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x08) { ecx = RORB(ebx,3); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x10) { ecx = RORB(ebx,4); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x20) { ecx = RORB(ebx,5); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x40) { ecx = RORB(ebx,6); dst_base[C2S(x,y,ecx)] = ax; }
				if (edx & 0x80) { ecx = RORB(ebx,7); dst_base[C2S(x,y,ecx)] = ax; }
			}
		}

		stars_enabled = layer & 1;

		#undef RORB
		#undef C2S

	}
	else if (command == STARPASS2 && flags & COLOR_ON)	// stars color modifier
	{
		edx = SCREEN_WIDTH - x - w;
		w = -w;

		ax = (UINT16)(~src_base[src2] & 0xff);

		for (yclip=y+h; y<yclip; y++)
		{
			edi = dst_base + ((y & YMASK) << SCREEN_WIDTH_L2);

			if (edx < 0)
			{
				ecx = edx;
				dst_ptr = (UINT16*)edi - edx;
				do { if (dst_ptr[ecx]) dst_ptr[ecx] ^= ax; } while (++ecx);
				ecx = x - SCREEN_WIDTH;
			} else ecx = w;

			dst_ptr = (UINT16*)edi + x - ecx;
			do { if (dst_ptr[ecx]) dst_ptr[ecx] ^= ax; } while (++ecx);
		}

	}
	else if (command == TILEPASS1 && flags & COLOR_ON)	// RGB 8x8 bit-packed transparent tile
	{

		if (y & 1) x -= 8;
		y = tyremap[y] << 3;

		if (flags & SY_HIGH) y += 8;
		if (y > 0xf8) return;

		if (code & PLANE) return; /* WARNING: UNEMULATED */


		if (flags & IGNORE_0) { w=8; h=8; WARP_WIPE_COMMON }

		src1_ptr = src_base + src1;
		dst_ptr = render_layer[2] + (y << SCREEN_WIDTH_L2);

		src1_ptr += 8;
		edx = -8;
		if (x <= 0xf8)
		{
			dst_ptr += x;
			do {
				ax = (UINT16)src1_ptr[edx];
				al = src1_ptr[edx+0x10000];
				ax |= BG_RGB;
				if (al & 0x01) *dst_ptr   = ax;
				if (al & 0x02) dst_ptr[1] = ax;
				if (al & 0x04) dst_ptr[2] = ax;
				if (al & 0x08) dst_ptr[3] = ax;
				if (al & 0x10) dst_ptr[4] = ax;
				if (al & 0x20) dst_ptr[5] = ax;
				if (al & 0x40) dst_ptr[6] = ax;
				if (al & 0x80) dst_ptr[7] = ax;
				dst_ptr += SCREEN_WIDTH;
			} while (++edx);
		}
		else
		{
			#define WARPMASK ((SCREEN_WIDTH<<1)-1)
			do {
				ecx = x & WARPMASK;
				ax = (UINT16)src1_ptr[edx];
				al = src1_ptr[edx+0x10000];
				ax |= BG_RGB;
				if (al & 0x01) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x02) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x04) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x08) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x10) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x20) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x40) dst_ptr[ecx] = ax;
				ecx++; ecx &= WARPMASK;
				if (al & 0x80) dst_ptr[ecx] = ax;
				dst_ptr += SCREEN_WIDTH;
			} while (++edx);
			#undef WARPMASK
		}

	}
	else if (command == HORIZBAR && flags & COLOR_ON && !(layer & 1))	// RGB horizontal bar(foreground only)
	{
		#define WARP_LINE_COMMON { \
			if (ecx & 1) { ecx--; *dst_ptr = (UINT16)eax; dst_ptr++; } \
			dst_ptr += ecx; ecx = -ecx; \
			while (ecx) { *(UINT32*)(dst_ptr+ecx) = eax; ecx += 2; } \
		}

		src1_ptr = src_base + src1;
		src2_ptr = src_base + src2;
		hclip = y + h;

		if (!(flags & S2_IDLE))
		{
			// double source mode
			src2_ptr += GFX_HI;
			wclip = x + w;
			w = 32;
		}
		else
		{
			// single source mode
			if (color & 4) src1_ptr += GFX_HI;
			wclip = 32;
		}

		for (xclip=x; xclip<wclip; xclip+=32)
		{
			xclip &= XMASK;
			edx = xclip + w - SCREEN_WIDTH;

			for (yclip=y; yclip<hclip; yclip++)
			{
				eax = (UINT32)*src1_ptr;
				src1_ptr += src_dx;
				if (!eax) continue;
				eax = eax | (eax<<16) | ((BG_RGB<<16)|BG_RGB);
				edi = dst_base + ((yclip & YMASK) << SCREEN_WIDTH_L2);

				if (edx > 0)
				{
					ecx = edx;
					dst_ptr = (UINT16*)edi;
					WARP_LINE_COMMON
					ecx = SCREEN_WIDTH - xclip;
				} else ecx = w;

				dst_ptr = (UINT16*)edi + xclip;
				WARP_LINE_COMMON
			}

			edi = src1_ptr; src1_ptr = src2_ptr; src2_ptr = (UINT8*)edi;
		}

		#undef WARP_LINE_COMMON
	}
}

static void bgtile_write(INT32 offset, UINT8 data)
{
	INT32 yskip, xskip, ecx;
	UINT16 *edi;
	UINT16 ax;

	DrvBGTileRAM[offset] = data;
	offset -= 0x18;

	if (offset < 0 || offset >= 191) return;
	yskip = offset / 48;
	xskip = offset % 48;
	if (xskip > 43) return;

	yskip = yskip * 48 + 24;
	xskip = xskip * 5 + 2;

	edi = BurnBitmapGetBitmap(1+2) + (yskip<<SCREEN_WIDTH_L2) + xskip + (48<<SCREEN_WIDTH_L2);
	ecx = -(48<<SCREEN_WIDTH_L2);
	ax = (UINT16)data | BG_RGB;

	do { edi[ecx] = edi[ecx+1] = edi[ecx+2] = edi[ecx+3] = edi[ecx+4] = ax; } while (ecx += SCREEN_WIDTH);
}

static void palette_write(UINT8 offset)
{
	UINT8 data = DrvPaletteRAM[offset];

	UINT8 i = ((data >> 6) & 0x03);
	UINT8 r = ((data >> 2) & 0x0c) | i;
	UINT8 g = ((data     ) & 0x0c) | i;
	UINT8 b = ((data << 2) & 0x0c) | i;

	PaletteLut[offset] = PaletteLut[offset + 0x100] = PaletteLut[offset + 0x200] = PaletteLut[offset + 0x300] = data | 0x500;
	DrvPalette[offset] = DrvPalette[offset + 0x100] = DrvPalette[offset + 0x200] = DrvPalette[offset + 0x300] = BurnHighCol(pal4bit(r),pal4bit(g),pal4bit(b),0);

	offset += 0x20;

	i = ((data >> 5) & 0x01) | ((data>>3) & 0x02);

	r = DrvColPROM[i | ((data >> 5) & 0x04) | ((data >> 3) & 0x08)];
	g = DrvColPROM[i | (data & 0xc)];
	b = DrvColPROM[i | ((data << 2) & 0xc)];

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void halleys_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x0000) {
		DrvBlitterRAM[address & 0xfff] = data;
		if ((address & 0xf) == 0) {
			blit(address);
		}

		if (!is_halleys) {
			if ((address & 0xf) == 0 || ((address & 0xf) == 4 && !data)) {
				blitter_busy = 0;
				if (firq_level) M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_ACK); // FIRQ!
			} else {
				blitter_busy = 1 + 1; // expire after 1 scanline (109cycles)
			}
		}

		return;
	}

	if ((address & 0xff00) == 0x1f00) {
		bgtile_write(address & 0xff, data);
		return;
	}

	if ((address & 0xffe0) == 0xffc0) {
		DrvPaletteRAM[address & 0x1f] = data;
		palette_write(address & 0x1f);
		return;
	}

	if ((address & 0xff00) == 0xff00)
	{
		DrvIORAM[address & 0xff] = data;

		switch (address)
		{
			case 0xff8a:
				ZetNmi(); // soundlatch is DrvIORAM[0x8a] - just use directly!
			return;

			case 0xff9c:
				if (firq_level) firq_level--;
				M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_NONE); // FIRQ!
			return;

			default:
			return;  // just write to IORAM
		}
	}

	bprintf(0, _T("mw %x  %x\n"), address, data);
}

static UINT8 halleys_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x0000) {
		if ((address & 0xf) == 0 || (address & 0xf) == 4) return 1;
		return DrvBlitterRAM[address & 0xfff];
	}

	if ((address & 0xffe0) == 0xffc0) {
		return DrvPaletteRAM[address & 0x01f];
	}

	if ((address & 0xffe0) == 0xffe0) {
		return DrvM6809ROM[address ^ (vector_type * 16)];
	}

	if ((address & 0xff00) == 0xff00)
	{
		switch (address)
		{
			case 0xff66:
			{
				if (is_halleys && M6809GetPC() == collision_detection)
				{
					if (collision_count) {
						collision_count--;
						return collision_list[collision_count];
					}

					return 0;
				}
				return DrvIORAM[address & 0xff];
			}

			case 0xff71:
			{
				if (is_halleys && M6809GetPC() == 0x8017) {
					return 0x55; // HACK: trick SRAM test on startup
				}
				return 0;
			}

			case 0xff80: // coin/start (in0)
			case 0xff90:
			case 0xff81: // player 1 (in1)
			case 0xff91:
			case 0xff82: // player 2 (in2)
			case 0xff92:
			{
				return DrvInputs[address & 3];
			}
			case 0xff83: // unused (in3)
			case 0xff93:
			{
				return 0;
			}

			case 0xff94:
			{
				INT32 ret = 0;
				ret |= ((DrvDips[3] & 0x20) >> 5);
				ret |= ((DrvInputs[0] & 0x80) >> 6);
				ret |= ((DrvInputs[0] & 0x40) >> 4);
				return ret;
			}
			
			case 0xff95:
			case 0xff96:
			case 0xff97:
				return DrvDips[address - 0xff95];
		}
		
		return DrvIORAM[address & 0xff];
	}

	bprintf(0, _T("mr %x\n"), address);

	return 0;
}

static void __fastcall halleys_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x4800) AY8910Write((address / 2) & 3, (address & 1), data);
}

static UINT8 __fastcall halleys_sound_read(UINT16 address)
{
	if ((address & 0xfff9) == 0x4801) return AY8910Read((address / 2) & 3);

	if (address == 0x5000) return DrvIORAM[0x8a];
	
	return 0;
}

static void sound_ay_3b_write_port(UINT32 , UINT32 data)
{
//	sndnmi_mask = data & 1; // iq_132 not used?
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0x00, RamEnd - AllRam);
	memset (DrvIORAM, 0xff, 0x100);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetReset(0);

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);
	AY8910Reset(3);

	collision_count = 0;
	firq_level = 0;
	soundtimer = 0;
	stars_enabled = 0;
	vector_type = 0;
	blitter_busy = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x020000;
	DrvGfxPlanes[0]	= Next; Next += 0x080000;
	DrvGfxPlanes[1]	= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000020;

	PaletteLut		= (UINT32*)Next; Next += 0x0601 * sizeof(UINT32);
	DrvPalette		= (UINT32*)Next; Next += 0x0601 * sizeof(UINT32);

	AllRam			= Next;

	DrvIORAM		= Next; Next += 0x000100;

	DrvBlitterRAM	= Next; Next += 0x001000;
	DrvBGTileRAM    = Next; Next += 0x000100;
	DrvPaletteRAM	= Next; Next += 0x000600;
	DrvM6809RAM		= Next; Next += 0x000f00;
	DrvZ80RAM		= Next; Next += 0x000800;

	collision_list	= Next; Next += 0x000100;

	RamEnd			= Next;

	scrollx0		= DrvIORAM + 0x9a;
	scrolly0		= DrvIORAM + 0x8e;
	scrollx1		= DrvIORAM + 0xa3;
	scrolly1		= DrvIORAM + 0xa2;

	MemEnd			= Next;

	return 0;
}

static void decrypt_main_cpu()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	for (INT32 i = 0; i < 0x10000; i++)
	{
		tmp[i] = BITSWAP08(DrvM6809ROM[BITSWAP16(i,15,14,13,12,11,10,1,0,4,5,6,3,7,8,9,2)],0,7,6,5,1,4,2,3);
	}

	memcpy(DrvM6809ROM, tmp, 0x10000);

	BurnFree(tmp);
}

static void graphics_expand()
{
	INT32 al,ah,dl,dh,i,j;
	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);

	for (i=0xffff,j=0; i>=0; i--,j+=8)
	{
		al = DrvGfxROM[i];
		ah = DrvGfxROM[i+0x10000];
		tmp[0xffff-i] = al;
		tmp[0x1ffff-i] = ah;

		DrvGfxPlanes[0][j+0] = dl = (al    & 1) | (ah<<2 & 4);  dl <<= 1;
		DrvGfxPlanes[0][j+1] = dh = (al>>1 & 1) | (ah<<1 & 4);  dh <<= 1;
		DrvGfxPlanes[1][j+0] = dl;
		DrvGfxPlanes[1][j+1] = dh;
		DrvGfxPlanes[0][j+2] = dl = (al>>2 & 1) | (ah    & 4);  dl <<= 1;
		DrvGfxPlanes[0][j+3] = dh = (al>>3 & 1) | (ah>>1 & 4);  dh <<= 1;
		DrvGfxPlanes[1][j+2] = dl;
		DrvGfxPlanes[1][j+3] = dh;
		DrvGfxPlanes[0][j+4] = dl = (al>>4 & 1) | (ah>>2 & 4);  dl <<= 1;
		DrvGfxPlanes[0][j+5] = dh = (al>>5 & 1) | (ah>>3 & 4);  dh <<= 1;
		DrvGfxPlanes[1][j+4] = dl;
		DrvGfxPlanes[1][j+5] = dh;
		DrvGfxPlanes[0][j+6] = dl = (al>>6 & 1) | (ah>>4 & 4);  dl <<= 1;
		DrvGfxPlanes[0][j+7] = dh = (al>>7    ) | (ah>>5 & 4);  dh <<= 1;
		DrvGfxPlanes[1][j+6] = dl;
		DrvGfxPlanes[1][j+7] = dh;
	}
	
	memcpy (DrvGfxROM, tmp, 0x20000);
	
	BurnFree(tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	is_halleys = (strstr(BurnDrvGetTextA(DRV_NAME), "halley") != NULL);
	bprintf(0, _T("is_halleys = %x\n"), is_halleys);

	{
		INT32 k = 0;

		if (is_halleys) {
			if (BurnLoadRom(DrvM6809ROM + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x08000, k++, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x0c000, k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvM6809ROM + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x08000, k++, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x0c000, k++, 1)) return 1;
		}
		
		if (BurnLoadRom(DrvZ80ROM   + 0x00000, k++, 1)) return 1;
		if (is_halleys) {
			if (BurnLoadRom(DrvZ80ROM   + 0x02000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvGfxROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x1c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, k++, 1)) return 1;

		decrypt_main_cpu();
		graphics_expand();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809ROM + 0x1000, 0x1000, 0xefff, MAP_ROM);
	M6809MapMemory(DrvM6809RAM + 0x0000, 0xf000, 0xfeff, MAP_RAM);
	M6809SetWriteHandler(halleys_main_write);
	M6809SetReadHandler(halleys_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM + 0x0000, 0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM + 0x0000, 0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0xe000, 0xe000, 0xefff, MAP_ROM);
	ZetSetWriteHandler(halleys_sound_write);
	ZetSetReadHandler(halleys_sound_read);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910Init(2, 1500000, 1);
	AY8910Init(3, 1500000, 1);
	AY8910SetPorts(3, NULL, NULL, NULL, &sound_ay_3b_write_port);
	AY8910SetAllRoutes(0, 0.07, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.07, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.07, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(3, 0.07, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();
	BurnBitmapAllocate(1, 256, 256, false);
	BurnBitmapAllocate(2, 256, 256, false);
	BurnBitmapAllocate(3, 256, 256, false);
	BurnBitmapAllocate(4, 256, 256, false);
	BurnBitmapAllocate(5, 256, 256, false);
	BurnBitmapAllocate(6, 256, 256, false);

	for (INT32 i = 0x1000; i < 0xf000; i++) {
		if (DrvM6809ROM[i] == 0x96 && DrvM6809ROM[i+1] == 0x66 && DrvM6809ROM[i+2] == 0x26 && DrvM6809ROM[i+3] == 0x0e) {
			collision_detection = i + 2;
			bprintf (0, _T("Collision pc: %4.4x\n"), collision_detection);
			break;
		}
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	M6809Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	AY8910Exit(3);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	memset (PaletteLut, 0, 0x400 * sizeof(UINT32));
	memset (DrvPalette, 0, 0x400 * sizeof(UINT32));

	for (INT32 i = 0; i < 16; i++)
	{
		INT32 d = ((i << 6) & 0xc0) | ((i << 2) & 0x30) | (i & 0x0c) | ((i >> 2) & 0x03) | 0x500;

		for (INT32 count = 0; count < 16; count++)
		{
			INT32 r = (i << 4) + i;
			INT32 g = (i << 4) + count + 0x400;

			PaletteLut[g] = d;
			DrvPalette[g] = BurnHighCol(r,r,r,0);
		}
	}

	for (INT32 d = 0; d < 256; d++)
	{
		INT32 i = ((d >> 6) & 0x03);
		INT32 r = ((d >> 2) & 0x0c) | i;
		INT32 g = ((d     ) & 0x0c) | i;
		INT32 b = ((d << 2) & 0x0c) | i;

		PaletteLut[d + 0x500] = d + 0x500;
		DrvPalette[d + 0x500] = BurnHighCol(pal4bit(r), pal4bit(g), pal4bit(b), 0);
	}
	
	PaletteLut[0x600] = 0x600;
}

static void copy_scroll_op(INT32 source_bitmap, INT32 sx, INT32 sy)
{
	UINT16 *source = BurnBitmapGetBitmap(source_bitmap + 1);

	sx = -sx & 0xff;
	sy = -sy & 0xff;

	INT32 rcw = CLIP_W - sx;
	if (rcw < 0)
		rcw = 0;

	INT32 bch = CLIP_H - sy;
	if (bch < 0)
		bch = 0;

	const UINT16 *src = source + CLIP_SKIP + sy * SCREEN_WIDTH;

	// draw top split
	for (INT32 y=0; y != bch; y++) {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		memcpy(dest, src+sx, 2*rcw);
		memcpy(dest + rcw, src, 2*(CLIP_W - rcw));
		src += SCREEN_WIDTH;
	}

	src = source + CLIP_SKIP;

	// draw bottom split
	for (INT32 y = bch; y != CLIP_H; y++) {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		memcpy(dest, src+sx, 2*rcw);
		memcpy(dest + rcw, src, 2*(CLIP_W - rcw));
		src += SCREEN_WIDTH;
	}
}

static void copy_scroll_xp(INT32 source_bitmap, INT32 sx, INT32 sy)
{
	UINT16 *source = BurnBitmapGetBitmap(source_bitmap + 1);
	
	sx = -sx & 0xff;
	sy = -sy & 0xff;

	INT32 rcw = CLIP_W - sx;
	if (rcw < 0)
		rcw = 0;

	INT32 bch = CLIP_H - sy;
	if (bch < 0)
		bch = 0;

	const UINT16 *src_base = source + CLIP_SKIP + sy * SCREEN_WIDTH;

	// draw top split
	for (INT32 y=0; y != bch; y++)  {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		const UINT16 *src = src_base + sx;
		for (INT32 x=0; x != rcw; x++) {
			UINT16 pixel = *src++;
			if(pixel)
				*dest = pixel;
			dest++;
		}

		src = src_base;

		for (INT32 x=rcw; x != CLIP_W; x++) {
			UINT16 pixel = *src++;
			if(pixel)
				*dest = pixel;
			dest++;
		}

		src_base += SCREEN_WIDTH;
	}

	src_base = source + CLIP_SKIP;

	// draw bottom split
	for (INT32 y = bch; y != CLIP_H; y++) {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		const UINT16 *src = src_base + sx;
		for (INT32 x=0; x != rcw; x++) {
			UINT16 pixel = *src++;
			if(pixel)
				*dest = pixel;
			dest++;
		}

		src = src_base;

		for (INT32 x=rcw; x != CLIP_W; x++) {
			UINT16 pixel = *src++;
			if(pixel)
				*dest = pixel;
			dest++;
		}

		src_base += SCREEN_WIDTH;
	}
}

static void copy_fixed_xp(INT32 source_bitmap)
{
	UINT16 *source = BurnBitmapGetBitmap(source_bitmap + 1);
	UINT16 *src = source + CLIP_SKIP;
	
	for (INT32 y = 0; y != CLIP_H; y++) {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		for (INT32 x = 0; x != CLIP_W; x++) {
			UINT16 pixel = src[x];

			if (pixel)
				dest[x] = pixel;
		}

		src += SCREEN_WIDTH;
	}
}

static void copy_fixed_2b(INT32 source_bitmap)
{
	UINT16 *source = BurnBitmapGetBitmap(source_bitmap + 1);
	UINT16 *src = source + CLIP_SKIP;
	
	for (INT32 y = 0; y != CLIP_H; y++) {
		UINT16 *dest = pTransDraw + (y * nScreenWidth);
		for (INT32 x = 0; x != CLIP_W; x++) {
			UINT16 pixel = src[x];

			if ((pixel && !(pixel & SP_2BACK)) || !dest[x])
				dest[x] = pixel;
		}

		src += SCREEN_WIDTH;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();

		for (INT32 i = 0; i < 0x20; i++) {
			palette_write(i & 0x1f);
		}

		DrvPalette[0x600] = 0; // black
		DrvRecalc = 0;
	}

	BurnTransferClear(0x600); // black!

	if (is_halleys) {
		if (stars_enabled)
		{
			if (nBurnLayer & 1) copy_scroll_op(5, *scrollx0, *scrolly0);
			if (nBurnLayer & 2) copy_scroll_xp(4, *scrollx1, *scrolly1);
		}

		if (nBurnLayer & 4) copy_scroll_xp(2, *scrollx1, *scrolly1);
		if (nSpriteEnable & 1) copy_fixed_2b(1);
		if (nSpriteEnable & 2) copy_fixed_xp(0);
	} else {
		if (DrvIORAM[0xa0] & 0x80) {
			if (nBurnLayer & 1) copy_scroll_op(2, *scrollx1, *scrolly1);
		}
		if (nBurnLayer & 2) copy_fixed_xp(1);
		if (nBurnLayer & 4) copy_fixed_xp(0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void halleys_scanline_cb(INT32 scanline)
{
	switch (scanline)
	{
		case 56:
			if (is_halleys) {
				vector_type = 1;
				M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_ACK);
			} else {
				if (blitter_busy) firq_level++;
				else M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_ACK);
			}
		break;

		case 112:
			if (is_halleys) {
				vector_type = 0;
				M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_ACK);
			} else {
				if (blitter_busy) firq_level++;
				else M6809SetIRQLine(CPU_IRQLINE_FIRQ, CPU_IRQSTATUS_ACK);
			}
		break;

		case 168:
			M6809SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
		break;

		case 248:
			collision_count = 0;
		break;
	}
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
		}
		
		DrvInputs[0] ^= 0xc3;
	}

	M6809NewFrame();
	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1664000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);
		halleys_scanline_cb(i);

		if (blitter_busy) {
			blitter_busy--;
		}

		CPU_RUN(1, Zet);
		soundtimer++;
		if (soundtimer == 419) {
			soundtimer = 0;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	ZetClose();
	M6809Close();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(collision_count);
		SCAN_VAR(firq_level);
		SCAN_VAR(soundtimer);
		SCAN_VAR(stars_enabled);
		SCAN_VAR(vector_type);
		SCAN_VAR(blitter_busy);

		SCAN_VAR(nCyclesExtra);

		ScanVar(BurnBitmapGetBitmap(1), 256*256*2, "Bitmap 1");
		ScanVar(BurnBitmapGetBitmap(2), 256*256*2, "Bitmap 2");
		ScanVar(BurnBitmapGetBitmap(3), 256*256*2, "Bitmap 3");
		ScanVar(BurnBitmapGetBitmap(4), 256*256*2, "Bitmap 4");
		ScanVar(BurnBitmapGetBitmap(5), 256*256*2, "Bitmap 5");
		ScanVar(BurnBitmapGetBitmap(6), 256*256*2, "Bitmap 6");
	}

	if (nAction & ACB_WRITE) {
		if (is_halleys == 0) {
			// Benberob, restore bg tile layer
			for (INT32 i = 0; i < 0x100; i++) {
				bgtile_write(i, DrvBGTileRAM[i]);
			}
		}
	}

	return 0;
}


// Halley's Comet (US)

static struct BurnRomInfo halleyscRomDesc[] = {
	{ "a62_01.30",		0x4000, 0xa5e82b3e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "a62_02.31",		0x4000, 0x25f5bcd3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a62-15.52",		0x4000, 0xe65d8312, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a62_04.50",		0x4000, 0xfad74dfe, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a62_13.5",		0x2000, 0x7ce290db, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "a62_14.4",		0x2000, 0xea74b1a2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a62_12.78",		0x4000, 0xc5834a7a, 3 | BRF_GRA },           //  6 Graphics Data
	{ "a62_10.77",		0x4000, 0x3ae7231e, 3 | BRF_GRA },           //  7
	{ "a62_08.80",		0x4000, 0xb9210dbe, 3 | BRF_GRA },           //  8
	{ "a62-16.79",		0x4000, 0x1165a622, 3 | BRF_GRA },           //  9
	{ "a62_11.89",		0x4000, 0xd0e9974e, 3 | BRF_GRA },           // 10
	{ "a62_09.88",		0x4000, 0xe93ef281, 3 | BRF_GRA },           // 11
	{ "a62_07.91",		0x4000, 0x64c95e8b, 3 | BRF_GRA },           // 12
	{ "a62_05.90",		0x4000, 0xc3c877ef, 3 | BRF_GRA },           // 13

	{ "a26-13.109",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 14 Color PROMs (All three are the same!)
	{ "a26-13.110",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 15
	{ "a26-13.111",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(halleysc)
STD_ROM_FN(halleysc)

struct BurnDriver BurnDrvHalleysc = {
	"halleysc", NULL, NULL, NULL, "1986",
	"Halley's Comet (US)\0", NULL, "Taito America Corporation (Coin-It license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, halleyscRomInfo, halleyscRomName, NULL, NULL, NULL, NULL, HalleysInputInfo, HalleysDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};


// Halley's Comet (Japan, rev 1)

static struct BurnRomInfo halleyscjRomDesc[] = {
	{ "a62_01.30",		0x4000, 0xa5e82b3e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "a62_02.31",		0x4000, 0x25f5bcd3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a62_03-1.52",	0x4000, 0xe2fffbe4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a62_04.50",		0x4000, 0xfad74dfe, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a62_13.5",		0x2000, 0x7ce290db, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "a62_14.4",		0x2000, 0xea74b1a2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a62_12.78",		0x4000, 0xc5834a7a, 3 | BRF_GRA },           //  6 Graphics Data
	{ "a62_10.77",		0x4000, 0x3ae7231e, 3 | BRF_GRA },           //  7
	{ "a62_08.80",		0x4000, 0xb9210dbe, 3 | BRF_GRA },           //  8
	{ "a62_06.79",		0x4000, 0x600be9ca, 3 | BRF_GRA },           //  9
	{ "a62_11.89",		0x4000, 0xd0e9974e, 3 | BRF_GRA },           // 10
	{ "a62_09.88",		0x4000, 0xe93ef281, 3 | BRF_GRA },           // 11
	{ "a62_07.91",		0x4000, 0x64c95e8b, 3 | BRF_GRA },           // 12
	{ "a62_05.90",		0x4000, 0xc3c877ef, 3 | BRF_GRA },           // 13

	{ "a26-13.109",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 14 Color PROMs
	{ "a26-13.110",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 15
	{ "a26-13.111",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(halleyscj)
STD_ROM_FN(halleyscj)

struct BurnDriver BurnDrvHalleyscj = {
	"halleyscj", "halleysc", NULL, NULL, "1986",
	"Halley's Comet (Japan, rev 1)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, halleyscjRomInfo, halleyscjRomName, NULL, NULL, NULL, NULL, HalleysInputInfo, HalleysDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};


// Halley's Comet (Japan)

static struct BurnRomInfo halleyscjaRomDesc[] = {
	{ "a62_01.30",		0x4000, 0xa5e82b3e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "a62_02.31",		0x4000, 0x25f5bcd3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a62_03.52",		0x4000, 0x8e90a97b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a62_04.50",		0x4000, 0xfad74dfe, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a62_13.5",		0x2000, 0x7ce290db, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "a62_14.4",		0x2000, 0xea74b1a2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a62_12.78",		0x4000, 0xc5834a7a, 3 | BRF_GRA },           //  6 Graphics Data
	{ "a62_10.77",		0x4000, 0x3ae7231e, 3 | BRF_GRA },           //  7
	{ "a62_08.80",		0x4000, 0xb9210dbe, 3 | BRF_GRA },           //  8
	{ "a62_06.79",		0x4000, 0x600be9ca, 3 | BRF_GRA },           //  9
	{ "a62_11.89",		0x4000, 0xd0e9974e, 3 | BRF_GRA },           // 10
	{ "a62_09.88",		0x4000, 0xe93ef281, 3 | BRF_GRA },           // 11
	{ "a62_07.91",		0x4000, 0x64c95e8b, 3 | BRF_GRA },           // 12
	{ "a62_05.90",		0x4000, 0xc3c877ef, 3 | BRF_GRA },           // 13

	{ "a26-13.109",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 14 Color PROMs
	{ "a26-13.110",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 15
	{ "a26-13.111",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(halleyscja)
STD_ROM_FN(halleyscja)

struct BurnDriver BurnDrvHalleyscja = {
	"halleyscja", "halleysc", NULL, NULL, "1986",
	"Halley's Comet (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, halleyscjaRomInfo, halleyscjaRomName, NULL, NULL, NULL, NULL, HalleysInputInfo, HalleysDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};


// Halley's Comet (Japan, Prototype)

static struct BurnRomInfo halleyscjpRomDesc[] = {
	{ "p_0_19f8.30",	0x4000, 0x10acefe8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "p_1_87b4.31",	0x4000, 0x1fe05cff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p_2_aaaa.52",	0x4000, 0xde4a14f0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p_3_0341.50",	0x4000, 0xb4b2b4f1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s1_3d02.5",		0x2000, 0x7ce290db, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "s2_213d.4",		0x2000, 0xea74b1a2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic-78_3441.78",	0x4000, 0xc5834a7a, 3 | BRF_GRA },           //  6 Graphics Data
	{ "ic-77_a097.77",	0x4000, 0x3ae7231e, 3 | BRF_GRA },           //  7
	{ "ic-80_7f98.80",	0x4000, 0xb9210dbe, 3 | BRF_GRA },           //  8
	{ "ic-79_9383.79",	0x4000, 0x600be9ca, 3 | BRF_GRA },           //  9
	{ "ic-89_ef2b.89",	0x4000, 0xd0e9974e, 3 | BRF_GRA },           // 10
	{ "ic-88_a03d.88",	0x4000, 0xe93ef281, 3 | BRF_GRA },           // 11
	{ "ic-91_8b4c.91",	0x4000, 0x64c95e8b, 3 | BRF_GRA },           // 12
	{ "ic-90_057c.90",	0x4000, 0xc3c877ef, 3 | BRF_GRA },           // 13

	{ "a26-13.109",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 14 Color PROMs
	{ "a26-13.110",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 15
	{ "a26-13.111",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(halleyscjp)
STD_ROM_FN(halleyscjp)

struct BurnDriver BurnDrvHalleyscjp = {
	"halleyscjp", "halleysc", NULL, NULL, "1985",
	"Halley's Comet (Japan, Prototype)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, halleyscjpRomInfo, halleyscjpRomName, NULL, NULL, NULL, NULL, HalleysInputInfo, HalleysDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};


// Halley's Comet '87

static struct BurnRomInfo halleysc87RomDesc[] = {
	{ "a62-17.30",		0x4000, 0xfa2a58a6, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "a62-18.31",		0x4000, 0xf3a078e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a62-19.52",		0x4000, 0xe8bb695c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a62-20.50",		0x4000, 0x59ed52cd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a62_13.5",		0x2000, 0x7ce290db, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "a62_14.4",		0x2000, 0xea74b1a2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a62_12.78",		0x4000, 0xc5834a7a, 3 | BRF_GRA },           //  6 Graphics Data
	{ "a62_10.77",		0x4000, 0x3ae7231e, 3 | BRF_GRA },           //  7
	{ "a62_08.80",		0x4000, 0xb9210dbe, 3 | BRF_GRA },           //  8
	{ "a62_06.79",		0x4000, 0x600be9ca, 3 | BRF_GRA },           //  9
	{ "a62_11.89",		0x4000, 0xd0e9974e, 3 | BRF_GRA },           // 10
	{ "a62_09.88",		0x4000, 0xe93ef281, 3 | BRF_GRA },           // 11
	{ "a62_07.91",		0x4000, 0x64c95e8b, 3 | BRF_GRA },           // 12
	{ "a62_05.90",		0x4000, 0xc3c877ef, 3 | BRF_GRA },           // 13

	{ "a26-13.109",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 14 Color PROMs
	{ "a26-13.110",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 15
	{ "a26-13.111",		0x0020, 0xec449aee, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(halleysc87)
STD_ROM_FN(halleysc87)

struct BurnDriver BurnDrvHalleysc87 = {
	"halleysc87", "halleysc", NULL, NULL, "1986",
	"Halley's Comet '87\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, halleysc87RomInfo, halleysc87RomName, NULL, NULL, NULL, NULL, HalleysInputInfo, HalleysDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

// Ben Bero Beh (Japan)

static struct BurnRomInfo benberobRomDesc[] = {
	{ "a26_01.31",	0x4000, 0x9ed566ba, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a26_02.52",	0x4000, 0xa563a033, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a26_03.50",	0x4000, 0x975849ef, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a26_12.5",	0x4000, 0x7fd728f3, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "a26_06.78",	0x4000, 0x79ae2e58, 3 | BRF_GRA },           //  4 gfx1
	{ "a26_07.77",	0x4000, 0xfe976343, 3 | BRF_GRA },           //  5
	{ "a26_04.80",	0x4000, 0x77d10723, 3 | BRF_GRA },           //  6
	{ "a26_05.79",	0x4000, 0x098eebcc, 3 | BRF_GRA },           //  7
	{ "a26_10.89",	0x4000, 0x3d406060, 3 | BRF_GRA },           //  8
	{ "a26_11.88",	0x4000, 0x4561836a, 3 | BRF_GRA },           //  9
	{ "a26_08.91",	0x4000, 0x7e63059d, 3 | BRF_GRA },           // 10
	{ "a26_09.90",	0x4000, 0xebd9a16c, 3 | BRF_GRA },           // 11

	{ "a26_13.r",	0x0020, 0xec449aee, 4 | BRF_GRA },           // 12 proms
	{ "a26_13.g",	0x0020, 0xec449aee, 4 | BRF_GRA },           // 13
	{ "a26_13.b",	0x0020, 0xec449aee, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(benberob)
STD_ROM_FN(benberob)

struct BurnDriver BurnDrvBenberob = {
	"benberob", NULL, NULL, NULL, "1984",
	"Ben Bero Beh (Japan)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, benberobRomInfo, benberobRomName, NULL, NULL, NULL, NULL, HalleysInputInfo, BenberobDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	256, 240, 4, 3
};
