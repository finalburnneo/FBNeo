// FinalBurn Neo Sega System C/C2 driver module
// Based on MAME driver by David Haywood and Aaron Giles

// System C/C2 vdp note: (Ribbit)
// System C mixes the raw data from the sprite & other layers externally
// Because of this the sprite:sprite masking (x == 0) feature, internal to the
// 315-5313 vdp, is not used.  When properly implemented and coming from the
// rgb output (using internal VDP mixer), The beach and Metal Factory levels
// will have sprites with incorrect masking due to the game hiding sprites
// with x == 0.

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2612.h"
#include "upd7759.h"
#include "sn76496.h"
#include "burn_pal.h"
#include "burn_gun.h" // for dial (BurnTrackball)

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[1];

static INT32 sound_rom_length = 0;
static UINT8 (*protection_read_cb)(UINT8);

static INT32 prot_write_buf = 0;
static INT32 prot_read_buf = 0;

static INT32 enable_display = 0;

static INT32 alt_palette_mode = 0;
static INT32 palette_bank = 0;
static INT32 bg_palbase = 0;
static INT32 sp_palbase = 0;

static UINT8 output_latch[8];
static UINT8 dir = 0;
static UINT8 dir_override = 0;
static UINT8 iocnt = 0;

static INT32 sound_bank = 0;

static INT32 irq6_line;
static INT32 irq4_counter;
static UINT16 SegaC2BgPalLookup[4];
static UINT16 SegaC2SpPalLookup[4];

static UINT8 DrvInputs[8];
static UINT8 DrvDips[4];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvReset;

static INT16 Analog[2];

static INT32 is_wwmarine = 0;
static INT32 is_tfrceacb = 0;
static INT32 is_ribbit = 0;

static INT32 has_dial = 0;

struct PicoVideo {
	UINT8 reg[0x20];
	UINT32 command;		// 32-bit Command
	UINT8 pending;		// 1 if waiting for second half of 32-bit command
	UINT8 type;			// Command type (v/c/vsram read/write)
	UINT16 addr;		// Read/Write address
	UINT8 addr_u;       // bit16 of .addr (for 128k)
	INT32 status;		// Status bits
	UINT8 pending_ints;	// pending interrupts: ??VH????
	INT8 lwrite_cnt;    // VDP write count during active display line
	UINT16 v_counter;   // V-counter
	INT32 h_mask;
	INT32 field;		// for interlace mode 2.  -dink
	INT32 rotate;
	INT32 debug_p;		// debug port
	INT32 rendstatus;	// status of vdp renderer
};

#define LF_PLANE_1 (1 << 0)
#define LF_SH      (1 << 1) // must be = 2
#define LF_FORCE   (1 << 2)

#define SPRL_HAVE_HI     0x80 // have hi priority sprites
#define SPRL_HAVE_LO     0x40 // *lo*
#define SPRL_MAY_HAVE_OP 0x20 // may have operator sprites on the line
#define SPRL_LO_ABOVE_HI 0x10 // low priority sprites may be on top of hi

#define PVD_KILL_A    (1 << 0)
#define PVD_KILL_B    (1 << 1)
#define PVD_KILL_S_LO (1 << 2)
#define PVD_KILL_S_HI (1 << 3)
#define PVD_KILL_32X  (1 << 4)
#define PVD_FORCE_A   (1 << 5)
#define PVD_FORCE_B   (1 << 6)
#define PVD_FORCE_S   (1 << 7)

#define PDRAW_SPRITES_MOVED (1<<0) // (asm)
#define PDRAW_WND_DIFF_PRIO (1<<1) // not all window tiles use same priority
#define PDRAW_INTERLACE     (1<<3)
#define PDRAW_DIRTY_SPRITES (1<<4) // (asm)
#define PDRAW_SONIC_MODE    (1<<5) // mid-frame palette changes for 8bit renderer
#define PDRAW_PLANE_HI_PRIO (1<<6) // have layer with all hi prio tiles (mk3)
#define PDRAW_SHHI_DONE     (1<<7) // layer sh/hi already processed
#define PDRAW_32_COLS       (1<<8) // 32 column mode

#define MAX_LINE_SPRITES 29

static UINT8 HighLnSpr[240][3 + MAX_LINE_SPRITES]; // sprite_count, ^flags, tile_count, [spritep]...

struct TileStrip
{
	INT32 nametab; // Position in VRAM of name table (for this tile line)
	INT32 line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap
	INT32 hscroll; // Horizontal scroll value in pixels for the line
	INT32 xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
	INT32 *hc;     // cache for high tile codes and their positions
	INT32 cells;   // cells (tiles) to draw (32 col mode doesn't need to update whole 320)
};

static UINT16 *RamPal;
static UINT16 *RamVid;
static UINT16 *RamSVid;
static struct PicoVideo *RamVReg;

static UINT16 *HighCol;
static UINT16 *HighColFull;

static INT32 *HighCacheA;
static INT32 *HighCacheB;
static INT32 *HighPreSpr;

static INT32 Scanline = 0;
static INT32 line_base_cycles;

static INT32 Hardware = 0x00; // japan-ntsc
static INT32 dma_xfers = 0; // vdp dma
static INT32 BlankedLine = 0;
static INT32 interlacemode2 = 0;

#define SekCyclesLine()         ( (SekTotalCycles() - line_base_cycles) )

static struct BurnInputInfo SegaC2_3ButtonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(SegaC2_3Button)

static struct BurnInputInfo SegaC2_2ButtonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},


	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(SegaC2_2Button)

static struct BurnInputInfo SegaC2_1ButtonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(SegaC2_1Button)

static struct BurnInputInfo RibbitInputList[] = { // Look Ma! No buttons!
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ribbit)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TwinsquaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 1"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A
STDINPUTINFO(Twinsqua)

static struct BurnInputInfo SoniccarInputList[] = {
	{"Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Lt Stick Left",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"Lt Stick Right",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"Rt Stick Left",	BIT_DIGITAL,	DrvJoy1 + 7,	"p2 left"	},
	{"Rt Stick Right",	BIT_DIGITAL,	DrvJoy1 + 6,	"p2 right"	},
	{"Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 3"	},
	{"Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Soniccar)

static struct BurnInputInfo OopartsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ooparts)

static struct BurnInputInfo WwmarineInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};	

STDINPUTINFO(Wwmarine)

static struct BurnInputInfo SonicfgtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sonicfgt)



#define coinage_dips(_num)										\
	{0   , 0xfe, 0   ,   16, "Coin A"						},	\
	{_num, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"			},	\
	{_num, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"			},	\
	{_num, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"			},	\
	{_num, 0x01, 0x0f, 0x05, "2 Coins/1 Credit, 5/3, 6/4"	},	\
	{_num, 0x01, 0x0f, 0x04, "2 Coins/1 Credit, 4/3"		},	\
	{_num, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"			},	\
	{_num, 0x01, 0x0f, 0x03, "1 Coin/1 Credit, 5/6"			},	\
	{_num, 0x01, 0x0f, 0x02, "1 Coin/1 Credit, 4/5"			},	\
	{_num, 0x01, 0x0f, 0x01, "1 Coin/1 Credit, 2/3"			},	\
	{_num, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"			},	\
	{_num, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"			},	\
	{_num, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"			},	\
	{_num, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"			},	\
	{_num, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"			},	\
	{_num, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"			},	\
	{_num, 0x01, 0x0f, 0x00, "Free Play (if Coin B too) or 1/1"		},	\
																\
	{0   , 0xfe, 0   ,   16, "Coin B"						},	\
	{_num, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"			},	\
	{_num, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"			},	\
	{_num, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"			},	\
	{_num, 0x01, 0xf0, 0x50, "2 Coins/1 Credit, 5/3, 6/4"	},	\
	{_num, 0x01, 0xf0, 0x40, "2 Coins/1 Credit, 4/3"		},	\
	{_num, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"			},	\
	{_num, 0x01, 0xf0, 0x30, "1 Coin/1 Credit, 5/6"			},	\
	{_num, 0x01, 0xf0, 0x20, "1 Coin/1 Credit, 4/5"			},	\
	{_num, 0x01, 0xf0, 0x10, "1 Coin/1 Credit, 2/3"			},	\
	{_num, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"			},	\
	{_num, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"			},	\
	{_num, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"			},	\
	{_num, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"			},	\
	{_num, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"			},	\
	{_num, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"			},	\
	{_num, 0x01, 0xf0, 0x00, "Free Play (if Coin A too) or 1/1"		},


static struct BurnDIPInfo BloxeedcDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0x77, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    4, "Credits to Start (VS)"		},
	{0x12, 0x01, 0x01, 0x00, "1"							},
	{0x12, 0x01, 0x01, 0x01, "2"							},
	{0x12, 0x01, 0x01, 0x00, "2"							},
	{0x12, 0x01, 0x01, 0x01, "4"							},

	{0   , 0xfe, 0   ,    2, "Credits to Start (Normal)"	},
	{0x12, 0x01, 0x02, 0x02, "1"							},
	{0x12, 0x01, 0x02, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x08, 0x08, "Off"							},
	{0x12, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x30, 0x20, "Easy"							},
	{0x12, 0x01, 0x30, 0x30, "Normal"						},
	{0x12, 0x01, 0x30, 0x10, "Hard"							},
	{0x12, 0x01, 0x30, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "High Speed Mode"				},
	{0x12, 0x01, 0x80, 0x00, "Off"							},
	{0x12, 0x01, 0x80, 0x80, "On"							},
};

STDDIPINFO(Bloxeedc)

static struct BurnDIPInfo BloxeeduDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0x77, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    4, "Credits to Start (Normal/VS)"	},
	{0x12, 0x01, 0x03, 0x01, "1 / 1"						},
	{0x12, 0x01, 0x03, 0x03, "1 / 2"						},
	{0x12, 0x01, 0x03, 0x02, "1 / 2"						},
	{0x12, 0x01, 0x03, 0x00, "2 / 4"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x08, 0x08, "Off"							},
	{0x12, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x30, 0x20, "Easy"							},
	{0x12, 0x01, 0x30, 0x30, "Normal"						},
	{0x12, 0x01, 0x30, 0x10, "Hard"							},
	{0x12, 0x01, 0x30, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "High Speed Mode"				},
	{0x12, 0x01, 0x80, 0x00, "Off"							},
	{0x12, 0x01, 0x80, 0x80, "On"							},
};

STDDIPINFO(Bloxeedu)

static struct BurnDIPInfo ColumnsDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x02, 0x02, "Off"							},
	{0x12, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x30, 0x00, "Easiest"						},
	{0x12, 0x01, 0x30, 0x10, "Easy"							},
	{0x12, 0x01, 0x30, 0x30, "Normal"						},
	{0x12, 0x01, 0x30, 0x20, "Hard"							},
};

STDDIPINFO(Columns)

static struct BurnDIPInfo Columns2DIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x12, 0x01, 0x01, 0x01, "Upright"						},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x02, 0x02, "Off"							},
	{0x12, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "VS. Mode Credits/Match"		},
	{0x12, 0x01, 0x0c, 0x0c, "1"							},
	{0x12, 0x01, 0x0c, 0x08, "2"							},
	{0x12, 0x01, 0x0c, 0x04, "3"							},
	{0x12, 0x01, 0x0c, 0x00, "4"							},

	{0   , 0xfe, 0   ,    4, "Flash Mode Difficulty"		},
	{0x12, 0x01, 0x30, 0x20, "Easy"							},
	{0x12, 0x01, 0x30, 0x30, "Normal"						},
	{0x12, 0x01, 0x30, 0x10, "Hard"							},
	{0x12, 0x01, 0x30, 0x00, "Hardest"						},
};

STDDIPINFO(Columns2)

static struct BurnDIPInfo TfrceacDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL							},
	{0x16, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x15)

	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x16, 0x01, 0x01, 0x01, "1"							},
	{0x16, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x02, 0x02, "Off"							},
	{0x16, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x0c, 0x00, "2"							},
	{0x16, 0x01, 0x0c, 0x0c, "3"							},
	{0x16, 0x01, 0x0c, 0x08, "4"							},
	{0x16, 0x01, 0x0c, 0x04, "5"							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x16, 0x01, 0x30, 0x10, "10k, 70k, 150k"				},
	{0x16, 0x01, 0x30, 0x30, "20k, 100k, 200k"				},
	{0x16, 0x01, 0x30, 0x20, "40k, 150k, 300k"				},
	{0x16, 0x01, 0x30, 0x00, "None"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0xc0, 0x80, "Easy"							},
	{0x16, 0x01, 0xc0, 0xc0, "Medium"						},
	{0x16, 0x01, 0xc0, 0x40, "Hard"							},
	{0x16, 0x01, 0xc0, 0x00, "Hardest"						},
};

STDDIPINFO(Tfrceac)

static struct BurnDIPInfo BorenchDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x13)

	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x14, 0x01, 0x01, 0x01, "1"							},
	{0x14, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x02, 0x02, "Off"							},
	{0x14, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives 1P Mode"				},
	{0x14, 0x01, 0x0c, 0x00, "1"							},
	{0x14, 0x01, 0x0c, 0x0c, "2"							},
	{0x14, 0x01, 0x0c, 0x08, "3"							},
	{0x14, 0x01, 0x0c, 0x04, "4"							},

	{0   , 0xfe, 0   ,    4, "Lives 2P Mode"				},
	{0x14, 0x01, 0x30, 0x00, "2"							},
	{0x14, 0x01, 0x30, 0x30, "3"							},
	{0x14, 0x01, 0x30, 0x20, "4"							},
	{0x14, 0x01, 0x30, 0x10, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x14, 0x01, 0xc0, 0x80, "Easy"							},
	{0x14, 0x01, 0xc0, 0xc0, "Medium"						},
	{0x14, 0x01, 0xc0, 0x40, "Hard"							},
	{0x14, 0x01, 0xc0, 0x00, "Hardest"						},
};

STDDIPINFO(Borench)

static struct BurnDIPInfo StkclmnsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0x0b, NULL							},

	coinage_dips(0x13)

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x14, 0x01, 0x03, 0x02, "Easy"							},
	{0x14, 0x01, 0x03, 0x03, "Medium"						},
	{0x14, 0x01, 0x03, 0x01, "Hard"							},
	{0x14, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x04, 0x04, "Off"							},
	{0x14, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Match Mode Prices"			},
	{0x14, 0x01, 0x08, 0x08, "1"							},
	{0x14, 0x01, 0x08, 0x00, "2"							},
};

STDDIPINFO(Stkclmns)

static struct BurnDIPInfo RibbitDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL							},
	{0x10, 0xff, 0xff, 0xf5, NULL							},

	coinage_dips(0xf)
	
	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x10, 0x01, 0x01, 0x01, "1"							},
	{0x10, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x10, 0x01, 0x02, 0x02, "Off"							},
	{0x10, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x10, 0x01, 0x0c, 0x08, "1"							},
	{0x10, 0x01, 0x0c, 0x0c, "2"							},
	{0x10, 0x01, 0x0c, 0x04, "3"							},
	{0x10, 0x01, 0x0c, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x10, 0x01, 0x30, 0x20, "Easy"							},
	{0x10, 0x01, 0x30, 0x30, "Normal"						},
	{0x10, 0x01, 0x30, 0x10, "Hard"							},
	{0x10, 0x01, 0x30, 0x00, "Hardest"						},
};

STDDIPINFO(Ribbit)

static struct BurnDIPInfo RibbitjDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL							},
	{0x10, 0xff, 0xff, 0xf5, NULL							},

	coinage_dips(0xf)
	
	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x10, 0x01, 0x01, 0x01, "1"							},
	{0x10, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x10, 0x01, 0x02, 0x02, "Off"							},
	{0x10, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x10, 0x01, 0x0c, 0x04, "1"							},
	{0x10, 0x01, 0x0c, 0x08, "2"							},
	{0x10, 0x01, 0x0c, 0x0c, "3"							},
	{0x10, 0x01, 0x0c, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x10, 0x01, 0x30, 0x20, "Easy"							},
	{0x10, 0x01, 0x30, 0x30, "Normal"						},
	{0x10, 0x01, 0x30, 0x10, "Hard"							},
	{0x10, 0x01, 0x30, 0x00, "Hardest"						},
};

STDDIPINFO(Ribbitj)


static struct BurnDIPInfo TwinsquaDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xff, NULL							},
	{0x0c, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x0b)

	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x0c, 0x01, 0x01, 0x01, "1"							},
	{0x0c, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x0c, 0x01, 0x02, 0x02, "Off"							},
	{0x0c, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Buy In"						},
	{0x0c, 0x01, 0x04, 0x04, "Off"							},
	{0x0c, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x0c, 0x01, 0x18, 0x10, "Easy"							},
	{0x0c, 0x01, 0x18, 0x18, "Medium"						},
	{0x0c, 0x01, 0x18, 0x08, "Hard"							},
	{0x0c, 0x01, 0x18, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Seat Type"					},
	{0x0c, 0x01, 0x20, 0x20, "Normal"						},
	{0x0c, 0x01, 0x20, 0x00, "Moving"						},
};

STDDIPINFO(Twinsqua)

static struct BurnDIPInfo SoniccarDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL							},
	{0x0d, 0xff, 0xff, 0xff, NULL							},

	coinage_dips(0x0c)

	{0   , 0xfe, 0   ,    4, "Demo Sound Interval"		},
	{0x0d, 0x01, 0x03, 0x00, "Off"							},
	{0x0d, 0x01, 0x03, 0x01, "Every 4 Minutes"				},
	{0x0d, 0x01, 0x03, 0x02, "Every 2 Minutes"				},
	{0x0d, 0x01, 0x03, 0x03, "On"							},

	{0   , 0xfe, 0   ,    2, "Lighting Time"				},
	{0x0d, 0x01, 0x04, 0x04, "Advertise & Playtime"			},
	{0x0d, 0x01, 0x04, 0x00, "Playtime Only"				},

	{0   , 0xfe, 0   ,    2, "Light"						},
	{0x0d, 0x01, 0x08, 0x00, "Off"							},
	{0x0d, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Screen Display"				},
	{0x0d, 0x01, 0x10, 0x10, "Insert 100-400 Yen"			},
	{0x0d, 0x01, 0x10, 0x00, "Insert Money"					},
};

STDDIPINFO(Soniccar)

static struct BurnDIPInfo SsonicbrDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xff, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x01, 0x01, "On"							},
	{0x12, 0x01, 0x01, 0x00, "Off"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x06, 0x04, "Easy"							},
	{0x12, 0x01, 0x06, 0x06, "Medium"						},
	{0x12, 0x01, 0x06, 0x02, "Hard"							},
	{0x12, 0x01, 0x06, 0x00, "Hardest"						},
};

STDDIPINFO(Ssonicbr)

static struct BurnDIPInfo OopartsDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL							},
	{0x16, 0xff, 0xff, 0xfe, NULL							},

	coinage_dips(0x15)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x01, 0x01, "Off"							},
	{0x16, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x06, 0x04, "2"							},
	{0x16, 0x01, 0x06, 0x06, "3"							},
	{0x16, 0x01, 0x06, 0x02, "4"							},
	{0x16, 0x01, 0x06, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x18, 0x10, "Easy"							},
	{0x16, 0x01, 0x18, 0x18, "Medium"						},
	{0x16, 0x01, 0x18, 0x08, "Hard"							},
	{0x16, 0x01, 0x18, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Region"						},
	{0x16, 0x01, 0x60, 0x60, "Japan"						},
	{0x16, 0x01, 0x60, 0x40, "USA"							},
	{0x16, 0x01, 0x60, 0x20, "Export"						},
	{0x16, 0x01, 0x60, 0x00, "Export"						},
};

STDDIPINFO(Ooparts)

static struct BurnDIPInfo PuyoDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x02, 0x02, "Off"							},
	{0x12, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "VS. Mode Credits/Match"		},
	{0x12, 0x01, 0x04, 0x04, "1"							},
	{0x12, 0x01, 0x04, 0x00, "3"							},

	{0   , 0xfe, 0   ,    4, "1P Mode Difficulty"			},
	{0x12, 0x01, 0x18, 0x10, "Easy"							},
	{0x12, 0x01, 0x18, 0x18, "Medium"						},
	{0x12, 0x01, 0x18, 0x08, "Hard"							},
	{0x12, 0x01, 0x18, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Moving Seat"					},
	{0x12, 0x01, 0x80, 0x80, "No Use"						},
	{0x12, 0x01, 0x80, 0x00, "In Use"						},
};

STDDIPINFO(Puyo)

static struct BurnDIPInfo IchirDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfe, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x01, 0x01, "Off"							},
	{0x12, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x06, 0x04, "Easy"							},
	{0x12, 0x01, 0x06, 0x06, "Medium"						},
	{0x12, 0x01, 0x06, 0x02, "Hard"							},
	{0x12, 0x01, 0x06, 0x00, "Hardest"						},
};

STDDIPINFO(Ichir)

static struct BurnDIPInfo WwmarineDIPList[]=
{	
	{0x09, 0xff, 0xff, 0xff, NULL							},
	{0x0a, 0xff, 0xff, 0xfc, NULL							},

	coinage_dips(0x09)
	
	{0   , 0xfe, 0   ,    4, "Demo Sound Interval"			},
	{0x0a, 0x01, 0x03, 0x00, "Off"							},
	{0x0a, 0x01, 0x03, 0x01, "Every 3 Demo Cycles"			},
	{0x0a, 0x01, 0x03, 0x02, "Every 2 Demo Cycles"			},
	{0x0a, 0x01, 0x03, 0x03, "On"							},

	{0   , 0xfe, 0   ,    2, "Capsule Mode"					},
	{0x0a, 0x01, 0x04, 0x00, "Off"							},
	{0x0a, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Credit Mode"					},
	{0x0a, 0x01, 0x10, 0x10, "Off"							},
	{0x0a, 0x01, 0x10, 0x00, "On"							},
};

STDDIPINFO(Wwmarine)

static struct BurnDIPInfo SonicfgtDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL							},
	{0x0a, 0xff, 0xff, 0xff, NULL							},

	coinage_dips(0x09)

	{0   , 0xfe, 0   ,    2, "Credit Mode"					},
	{0x0a, 0x01, 0x01, 0x01, "Off"							},
	{0x0a, 0x01, 0x01, 0x00, "On"							},
};

STDDIPINFO(Sonicfgt)

static struct BurnDIPInfo PotopotoDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Credits to Start"				},
	{0x12, 0x01, 0x01, 0x01, "1"							},
	{0x12, 0x01, 0x01, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x02, 0x02, "Off"							},
	{0x12, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Coin Chute Type"				},
	{0x12, 0x01, 0x04, 0x04, "Common"						},
	{0x12, 0x01, 0x04, 0x00, "Individual"					},

	{0   , 0xfe, 0   ,    2, "Credits to Continue"			},
	{0x12, 0x01, 0x08, 0x08, "1"							},
	{0x12, 0x01, 0x08, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Buy-In"						},
	{0x12, 0x01, 0x10, 0x10, "No"							},
	{0x12, 0x01, 0x10, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x60, 0x40, "Easy"							},
	{0x12, 0x01, 0x60, 0x60, "Medium"						},
	{0x12, 0x01, 0x60, 0x20, "Hard"							},
	{0x12, 0x01, 0x60, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Moving Seat"					},
	{0x12, 0x01, 0x80, 0x80, "No Use"						},
	{0x12, 0x01, 0x80, 0x00, "In Use"						},
};

STDDIPINFO(Potopoto)

static struct BurnDIPInfo Puyopuy2DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0xfd, NULL							},

	coinage_dips(0x13)

	{0   , 0xfe, 0   ,    2, "Rannyu Off Button"			},
	{0x14, 0x01, 0x01, 0x01, "Use"							},
	{0x14, 0x01, 0x01, 0x00, "No Use"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x02, 0x02, "Off"							},
	{0x14, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Turn Direction"				},
	{0x14, 0x01, 0x04, 0x04, "1:Right  2:Left"				},
	{0x14, 0x01, 0x04, 0x00, "1:Left  2:Right"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x14, 0x01, 0x18, 0x10, "Easy"							},
	{0x14, 0x01, 0x18, 0x18, "Medium"						},
	{0x14, 0x01, 0x18, 0x08, "Hard"							},
	{0x14, 0x01, 0x18, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "VS Mode Match/1 Play"			},
	{0x14, 0x01, 0x60, 0x60, "1"							},
	{0x14, 0x01, 0x60, 0x40, "2"							},
	{0x14, 0x01, 0x60, 0x20, "3"							},
	{0x14, 0x01, 0x60, 0x00, "4"							},

	{0   , 0xfe, 0   ,    2, "Battle Start credit"			},
	{0x14, 0x01, 0x80, 0x00, "1"							},
	{0x14, 0x01, 0x80, 0x80, "2"							},
};

STDDIPINFO(Puyopuy2)

static struct BurnDIPInfo ZunkyouDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0xef, NULL							},

	coinage_dips(0x13)
	
	{0   , 0xfe, 0   ,    2, "Game Difficulty 1"			},
	{0x14, 0x01, 0x01, 0x01, "Medium"						},
	{0x14, 0x01, 0x01, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Game Difficulty 2"			},
	{0x14, 0x01, 0x02, 0x02, "Medium"						},
	{0x14, 0x01, 0x02, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x14, 0x01, 0x0c, 0x08, "1"							},
	{0x14, 0x01, 0x0c, 0x04, "2"							},
	{0x14, 0x01, 0x0c, 0x0c, "3"							},
	{0x14, 0x01, 0x0c, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x10, 0x10, "Off"							},
	{0x14, 0x01, 0x10, 0x00, "On"							},
};

STDDIPINFO(Zunkyou)

static struct BurnDIPInfo HeadonchDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0xfe, NULL							},

	coinage_dips(0x11)

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x12, 0x01, 0x01, 0x01, "Off"							},
	{0x12, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x06, 0x04, "Easy"							},
	{0x12, 0x01, 0x06, 0x06, "Medium"						},
	{0x12, 0x01, 0x06, 0x02, "Hard"							},
	{0x12, 0x01, 0x06, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x18, 0x10, "2"							},
	{0x12, 0x01, 0x18, 0x18, "3"							},
	{0x12, 0x01, 0x18, 0x08, "4"							},
	{0x12, 0x01, 0x18, 0x00, "5"							},
};

STDDIPINFO(Headonch)

static UINT16 palette_read(UINT16 offset)
{
	offset = (offset >> 1) & 0x1ff;
	UINT16 *ram = (UINT16*)DrvPalRAM;

	if (alt_palette_mode)
		offset = ((offset << 1) & 0x100) | ((offset << 2) & 0x80) | ((~offset >> 2) & 0x40) | ((offset >> 1) & 0x20) | (offset & 0x1f);

	return ram[offset + palette_bank * 0x200];
}

static void palette_update(UINT16 offset)
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	INT32 data = ram[offset];

	INT32 r = ((data << 1) & 0x1e) | ((data >> 12) & 0x01);
	INT32 g = ((data >> 3) & 0x1e) | ((data >> 13) & 0x01);
	INT32 b = ((data >> 7) & 0x1e) | ((data >> 14) & 0x01);
	DrvPalette[offset + 0x0000] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);

	INT32 tmpr = r >> 1;
	INT32 tmpg = g >> 1;
	INT32 tmpb = b >> 1;

	DrvPalette[offset + 0x0800] = BurnHighCol(pal5bit(tmpr), pal5bit(tmpg), pal5bit(tmpb), 0);
	DrvPalette[offset + 0x1800] = BurnHighCol(pal5bit(tmpr), pal5bit(tmpg), pal5bit(tmpb), 0); // shadow + hilight = shadow?

	tmpr = tmpr | 0x10;
	tmpg = tmpg | 0x10;
	tmpb = tmpb | 0x10;

	DrvPalette[offset + 0x1000] = BurnHighCol(pal5bit(tmpr), pal5bit(tmpg), pal5bit(tmpb), 0);
}

static void palette_write(UINT16 offset, UINT16 data, UINT16 mem_mask)
{
	offset = (offset >> 1) & 0x1ff;

	if (alt_palette_mode)
		offset = ((offset << 1) & 0x100) | ((offset << 2) & 0x80) | ((~offset >> 2) & 0x40) | ((offset >> 1) & 0x20) | (offset & 0x1f);
	offset += palette_bank * 0x200;

	UINT16 *ram = (UINT16*)DrvPalRAM;

	ram[offset] = (ram[offset] & ~mem_mask) | (data & mem_mask);

	palette_update(offset);
}

static void recompute_palette_tables()
{
	for (INT32 i = 0; i < 4; i++)
	{
		INT32 bgpal = 0x000 + bg_palbase * 0x40 + i * 0x10;
		INT32 sppal = 0x100 + sp_palbase * 0x40 + i * 0x10;

		if (!alt_palette_mode)
		{
			SegaC2BgPalLookup[i] = 0x200 * palette_bank + bgpal;
			SegaC2SpPalLookup[i] = 0x200 * palette_bank + sppal;
		}
		else
		{
			SegaC2BgPalLookup[i] = 0x200 * palette_bank + ((bgpal << 1) & 0x180) + ((~bgpal >> 2) & 0x40) + (bgpal & 0x30);
			SegaC2SpPalLookup[i] = 0x200 * palette_bank + ((~sppal << 2) & 0x100) + ((sppal << 2) & 0x80) + ((~sppal >> 2) & 0x40) + ((sppal >> 2) & 0x20) + (sppal & 0x10);
		}
	}
}

static void protection_write(UINT8 data)
{
	if (is_tfrceacb) return;

	INT32 new_sp_palbase = (data >> 2) & 3;
	INT32 new_bg_palbase = data & 3;
	INT32 table_index = (prot_write_buf << 4) | prot_read_buf;

	prot_write_buf = data & 0x0f;
	prot_read_buf = protection_read_cb(table_index);

	if (new_sp_palbase != sp_palbase || new_bg_palbase != bg_palbase)
	{
		sp_palbase = new_sp_palbase;
		bg_palbase = new_bg_palbase;
		//bprintf(0, _T("sp %x  bg %x  palbases.  (scanline: %d\tframe: %d)\n"), sp_palbase, bg_palbase, Scanline, nCurrentFrame);
		recompute_palette_tables();
	}
}

static void io_porth_write(UINT8 data)
{
	INT32 newbank = data & 3;
	if (newbank != palette_bank)
	{
		//bprintf(0, _T("palette bank: %x  (scanline: %d\tframe: %d)\n"), newbank, Scanline, nCurrentFrame);
		palette_bank = newbank;
		recompute_palette_tables();
	}
	if (sound_rom_length)
	{
		sound_bank = (data >> 2) & (((sound_rom_length) / 0x20000) - 1);
		memcpy (DrvSndROM + 0x80000, DrvSndROM + (sound_bank * 0x20000), 0x20000);
	}
}

static UINT8 sega_315_5296_read(UINT8 offset)
{
	if (offset <= 7) {
		if (dir & dir_override & (1 << offset)) {
			return output_latch[offset];
		}

		if (offset == 2) {
			if (sound_rom_length == 0) return 0xff;
			return UPD7759BusyRead(0) ? 0xff : 0xbf;
		}

		return DrvInputs[offset];
	}
	if (offset <= 0x0b)
	{
		char *s = "SEGA";
		return s[offset - 8];
	}
	if (offset & 1) {
		return dir;
	} else {
		return iocnt;
	}

	return 0xff;
}

void sega_315_5296_write(UINT8 offset, UINT8 data)
{
	offset &= 0x3f;
	if (offset <= 0x07) {
		if (dir & (1 << offset)) {
		//	if (offset == 0x03) return; // port D (coin counters)
			if (offset == 0x07) io_porth_write(data);
		}
		output_latch[offset] = data;
		return;
	}
	if (offset == 0x0e) {
		//	for (INT32 i = 0; i < 3; i++) out_cnt_callback[i]((data >> i) & 1);
		if (sound_rom_length) {
			UPD7759ResetWrite(0, (data >> 1) & 1); // cnt1
		}
		iocnt = data;
		return;
	}
	if (offset == 0x0f) {
		for (INT32 i = 0; i < 8; i++) {
			if ((dir ^ data) & (1 << i)) {
		//		if (i == 0x03) continue; // port A (coin counters)
				if (i == 0x07) io_porth_write((data & (1 << i)) ? output_latch[i] : 0);
			}
		}
		dir = data;
		return;
	}
}

//---------------------------------------------------------------
// Megadrive Video Port Read Write
//---------------------------------------------------------------

static void VideoWrite128(UINT32 a, UINT16 d)
{
  a = ((a & 2) >> 1) | ((a & 0x400) >> 9) | (a & 0x3FC) | ((a & 0x1F800) >> 1);
  ((UINT8 *)RamVid)[a] = d;
}

static INT32 GetDmaLength()
{
  INT32 len = 0;
  // 16-bit words to transfer:
  len  = RamVReg->reg[0x13];
  len |= RamVReg->reg[0x14]<<8;
  // Charles MacDonald:
  len = ((len - 1) & 0xffff) + 1;
  return len;
}

// dma2vram settings are just hacks to unglitch Legend of Galahad (needs <= 104 to work)
// same for Outrunners (92-121, when active is set to 24)
// 96 is VR hack
static const INT32 dma_timings[] = {
  167, 167, 166,  83, // vblank: 32cell: dma2vram dma2[vs|c]ram vram_fill vram_copy
  103, 205, 204, 102, // vblank: 40cell:
  16,   16,  15,   8, // active: 32cell:
  24,   18,  17,   9  // ...
};

static const INT32 dma_bsycles[] = {
  (488<<8)/167, (488<<8)/167, (488<<8)/166, (488<<8)/83,
  (488<<8)/103, (488<<8)/233, (488<<8)/204, (488<<8)/102,
  (488<<8)/16,  (488<<8)/16,  (488<<8)/15,  (488<<8)/8,
  (488<<8)/24,  (488<<8)/18,  (488<<8)/17,  (488<<8)/9
};

static UINT32 CheckDMA(void)
{
  INT32 burn = 0, xfers_can, dma_op = RamVReg->reg[0x17]>>6; // see gens for 00 and 01 modes
  INT32 xfers = dma_xfers;
  INT32 dma_op1;

  if(!(dma_op&2)) dma_op = (RamVReg->type == 1) ? 0 : 1; // setting dma_timings offset here according to Gens
  dma_op1 = dma_op;
  if(RamVReg->reg[12] & 1) dma_op |= 4; // 40 cell mode?
  if(!(RamVReg->status&8)&&(RamVReg->reg[1]&0x40)) dma_op|=8; // active display?
  xfers_can = dma_timings[dma_op];

  if(xfers <= xfers_can)
  {
    if(dma_op&2) RamVReg->status&=~2; // dma no longer busy
    else {
      burn = xfers * dma_bsycles[dma_op] >> 8; // have to be approximate because can't afford division..
    }
    dma_xfers = 0;
  } else {
    if(!(dma_op&2)) burn = 488;
    dma_xfers -= xfers_can;
  }

  //elprintf(EL_VDPDMA, "~Dma %i op=%i can=%i burn=%i [%i]", Pico.m.dma_xfers, dma_op1, xfers_can, burn, SekCyclesDone());
  //dprintf("~aim: %i, cnt: %i", SekCycleAim, SekCycleCnt);
  //bprintf(0, _T("burn[%d]"), burn);
  return burn;
}

static INT32 DMABURN() { // add cycles to the 68k cpu
    if (dma_xfers) {
        return CheckDMA();
    } else return 0;
}

static void DmaSlow(INT32 len)
{
	UINT16 *pd=0, *pdend, *r;
	UINT32 a = RamVReg->addr, a2, d;
	UINT8 inc = RamVReg->reg[0xf];
	UINT32 source;
	UINT32 fromrom = 0;

	source  = RamVReg->reg[0x15] <<  1;
	source |= RamVReg->reg[0x16] <<  9;
	source |= RamVReg->reg[0x17] << 17;

  //dprintf("DmaSlow[%i] %06x->%04x len %i inc=%i blank %i [%i|%i]", Pico.video.type, source, a, len, inc,
  //         (Pico.video.status&8)||!(Pico.video.reg[1]&0x40), Pico.m.scanline, SekCyclesDone());

	dma_xfers += len;

	INT32 dmab = CheckDMA();
	SekCyclesBurnRun(dmab);

#ifdef CYCDBUG
//	bprintf(0, _T("dma @ ln %d cyc %d, burnt: %d.\n"), Scanline, SekCyclesLine(), dmab);
#endif
	//SekCyclesBurnRun(dmab);

	const INT32 RomSize = 0x200000;

	if ((source & 0xe00000) == 0xe00000) { // RAM
		pd    = (UINT16 *)(Drv68KRAM + (source & 0xfffe));
		pdend = (UINT16 *)(Drv68KRAM + 0x10000);
	} else if( source < RomSize) {	// ROM
		fromrom = 1;
		source &= ~1;
		pd    = (UINT16 *)(Drv68KROM + source);
		pdend = (UINT16 *)(Drv68KROM + RomSize);
	} else return; // Invalid source address

	// overflow protection, might break something..
	if (len > pdend - pd) {
		len = pdend - pd;
		//bprintf(0, _T("DmaSlow() overflow(!).\n"));
	}

	switch ( RamVReg->type ) {
	case 1: // vram
		r = RamVid;
		for(; len; len--) {
			d = *pd++;
			if(a&1) d=(d<<8)|(d>>8);
			r[a>>1] = (UINT16)d; // will drop the upper bits
			// AutoIncrement
			a = (UINT16)(a+inc);
			// didn't src overlap?
			//if(pd >= pdend) pd -= 0x8000; // should be good for RAM, bad for ROM
		}

		RamVReg->rendstatus |= PDRAW_DIRTY_SPRITES;
		break;

	case 3: // cram
		//dprintf("DmaSlow[%i] %06x->%04x len %i inc=%i blank %i [%i|%i]", Pico.video.type, source, a, len, inc,
		//         (Pico.video.status&8)||!(Pico.video.reg[1]&0x40), Pico.m.scanline, SekCyclesDone());
		for(a2 = a&0x7f; len; len--) {
			d = *pd++;
			//CalcCol( a2>>1, BURN_ENDIAN_SWAP_INT16(d) );
			//pd++;
			// AutoIncrement
			a2+=inc;
			// didn't src overlap?
			//if(pd >= pdend) pd-=0x8000;
			// good dest?
			if(a2 >= 0x80) break; // Todds Adventures in Slime World / Andre Agassi tennis
		}
		a = (a&0xff00) | a2;
		break;

	case 5: // vsram[a&0x003f]=d;
		r = RamSVid;
		for(a2=a&0x7f; len; len--) {
			d = *pd++;
			r[a2>>1] = (UINT16)d;
			// AutoIncrement
			a2+=inc;
			// didn't src overlap?
			//if(pd >= pdend) pd-=0x8000;
			// good dest?
			if(a2 >= 0x80) break;
		}
		a=(a&0xff00)|a2;
		break;

	case 0x81: // vram 128k
      a |= RamVReg->addr_u << 16;
      for(; len; len--)
      {
        VideoWrite128(a, *pd++);
        // AutoIncrement
        a = (a + inc) & 0x1ffff;
      }
      RamVReg->addr_u = a >> 16;
      break;

	}
	// remember addr
	RamVReg->reg[0x13] = RamVReg->reg[0x14] = 0; // Dino Dini's Soccer (E) (by Haze)
	RamVReg->addr = (UINT16)a;
}

static void DmaCopy(INT32 len)
{
	UINT8 * vr = (UINT8 *) RamVid;
	UINT8 * vrs;
	UINT16 a = RamVReg->addr;
	UINT8 inc = RamVReg->reg[0xf];
	INT32 source;

	//dprintf("DmaCopy len %i [%i|%i]", len, Pico.m.scanline, SekCyclesDone());

	RamVReg->status |= 2; // dma busy
	dma_xfers += len;

	source  = RamVReg->reg[0x15];
	source |= RamVReg->reg[0x16]<<8;
	vrs = vr + source;

	if (source+len > 0x10000)
		len = 0x10000 - source; // clip??

	for(;len;len--) {
		vr[a] = *vrs++;
		// AutoIncrement
		a = (UINT16)(a + inc);
	}
	// remember addr
	RamVReg->addr = a;
	RamVReg->reg[0x13] = RamVReg->reg[0x14] = 0; // Dino Dini's Soccer (E) (by Haze)
	RamVReg->rendstatus |= PDRAW_DIRTY_SPRITES;
}

static void DmaFill(INT32 data)
{
	INT32 len = GetDmaLength();
	UINT8 *vr = (UINT8 *) RamVid;
	UINT8 high = (UINT8) (data >> 8);
	UINT16 a = RamVReg->addr;
	UINT8 inc = RamVReg->reg[0xf];

	//dprintf("DmaFill len %i inc %i [%i|%i]", len, inc, Pico.m.scanline, SekCyclesDone());

	// from Charles MacDonald's genvdp.txt:
	// Write lower byte to address specified
	RamVReg->status |= 2; // dma busy
	dma_xfers += len;
	vr[a] = (UINT8) data;
	a = (UINT16)(a+inc);

	if(!inc) len=1;

	for(;len;len--) {
		// Write upper byte to adjacent address
		// (here we are byteswapped, so address is already 'adjacent')
		vr[a] = high;
		// Increment address register
		a = (UINT16)(a+inc);
	}
	// remember addr
	RamVReg->addr = a;
	// update length
	RamVReg->reg[0x13] = RamVReg->reg[0x14] = 0; // Dino Dini's Soccer (E) (by Haze)

	RamVReg->rendstatus |= PDRAW_DIRTY_SPRITES;
}

static void CommandChange()
{
	//struct PicoVideo *pvid=&Pico.video;
	UINT32 cmd = RamVReg->command;
	UINT32 addr = 0;

	// Get type of transfer 0xc0000030 (v/c/vsram read/write)
	RamVReg->type = (UINT8)(((cmd >> 2) & 0xc) | (cmd >> 30));
	if (RamVReg->type == 1) { // vram
		RamVReg->type |= RamVReg->reg[1] & 0x80; // 128k
	}

	// Get address 0x3fff0003
	addr  = (cmd >> 16) & 0x3fff;
	addr |= (cmd << 14) & 0xc000;
	RamVReg->addr = (UINT16)addr;
	RamVReg->addr_u = (UINT8)((cmd >> 2) & 1);

	//dprintf("addr set: %04x", addr);

	// Check for dma:
	if (cmd & 0x80) {
		// Command DMA
		if ((RamVReg->reg[1] & 0x10) == 0) return; // DMA not enabled
		INT32 len = GetDmaLength();
		switch ( RamVReg->reg[0x17]>>6 ) {
		case 0x00:
		case 0x01:
			DmaSlow(len);	// 68000 to VDP
			break;
		case 0x03:
			DmaCopy(len);	// VRAM Copy
			break;
		case 0x02:			// DMA Fill Flag ???
		default:
			;//bprintf(PRINT_NORMAL, _T("Video Command DMA Unknown %02x len %d\n"), RamVReg->reg[0x17]>>6, len);
		}
	}
}

// H-counter table for hvcounter reads in 40col mode
// based on Gens code
static const UINT8 hcounts_40[] = {
	0x07,0x07,0x08,0x08,0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0b,0x0b,0x0c,0x0c,0x0d,0x0d,
	0x0e,0x0e,0x0e,0x0f,0x0f,0x10,0x10,0x10,0x11,0x11,0x12,0x12,0x13,0x13,0x13,0x14,
	0x14,0x15,0x15,0x15,0x16,0x16,0x17,0x17,0x18,0x18,0x18,0x19,0x19,0x1a,0x1a,0x1b,
	0x1b,0x1b,0x1c,0x1c,0x1d,0x1d,0x1d,0x1e,0x1e,0x1f,0x1f,0x20,0x20,0x20,0x21,0x21,
	0x22,0x22,0x23,0x23,0x23,0x24,0x24,0x25,0x25,0x25,0x26,0x26,0x27,0x27,0x28,0x28,
	0x28,0x29,0x29,0x2a,0x2a,0x2a,0x2b,0x2b,0x2c,0x2c,0x2d,0x2d,0x2d,0x2e,0x2e,0x2f,
	0x2f,0x30,0x30,0x30,0x31,0x31,0x32,0x32,0x32,0x33,0x33,0x34,0x34,0x35,0x35,0x35,
	0x36,0x36,0x37,0x37,0x38,0x38,0x38,0x39,0x39,0x3a,0x3a,0x3a,0x3b,0x3b,0x3c,0x3c,
	0x3d,0x3d,0x3d,0x3e,0x3e,0x3f,0x3f,0x3f,0x40,0x40,0x41,0x41,0x42,0x42,0x42,0x43,
	0x43,0x44,0x44,0x45,0x45,0x45,0x46,0x46,0x47,0x47,0x47,0x48,0x48,0x49,0x49,0x4a,
	0x4a,0x4a,0x4b,0x4b,0x4c,0x4c,0x4d,0x4d,0x4d,0x4e,0x4e,0x4f,0x4f,0x4f,0x50,0x50,
	0x51,0x51,0x52,0x52,0x52,0x53,0x53,0x54,0x54,0x55,0x55,0x55,0x56,0x56,0x57,0x57,
	0x57,0x58,0x58,0x59,0x59,0x5a,0x5a,0x5a,0x5b,0x5b,0x5c,0x5c,0x5c,0x5d,0x5d,0x5e,
	0x5e,0x5f,0x5f,0x5f,0x60,0x60,0x61,0x61,0x62,0x62,0x62,0x63,0x63,0x64,0x64,0x64,
	0x65,0x65,0x66,0x66,0x67,0x67,0x67,0x68,0x68,0x69,0x69,0x6a,0x6a,0x6a,0x6b,0x6b,
	0x6c,0x6c,0x6c,0x6d,0x6d,0x6e,0x6e,0x6f,0x6f,0x6f,0x70,0x70,0x71,0x71,0x71,0x72,
	0x72,0x73,0x73,0x74,0x74,0x74,0x75,0x75,0x76,0x76,0x77,0x77,0x77,0x78,0x78,0x79,
	0x79,0x79,0x7a,0x7a,0x7b,0x7b,0x7c,0x7c,0x7c,0x7d,0x7d,0x7e,0x7e,0x7f,0x7f,0x7f,
	0x80,0x80,0x81,0x81,0x81,0x82,0x82,0x83,0x83,0x84,0x84,0x84,0x85,0x85,0x86,0x86,
	0x86,0x87,0x87,0x88,0x88,0x89,0x89,0x89,0x8a,0x8a,0x8b,0x8b,0x8c,0x8c,0x8c,0x8d,
	0x8d,0x8e,0x8e,0x8e,0x8f,0x8f,0x90,0x90,0x91,0x91,0x91,0x92,0x92,0x93,0x93,0x94,
	0x94,0x94,0x95,0x95,0x96,0x96,0x96,0x97,0x97,0x98,0x98,0x99,0x99,0x99,0x9a,0x9a,
	0x9b,0x9b,0x9b,0x9c,0x9c,0x9d,0x9d,0x9e,0x9e,0x9e,0x9f,0x9f,0xa0,0xa0,0xa1,0xa1,
	0xa1,0xa2,0xa2,0xa3,0xa3,0xa3,0xa4,0xa4,0xa5,0xa5,0xa6,0xa6,0xa6,0xa7,0xa7,0xa8,
	0xa8,0xa9,0xa9,0xa9,0xaa,0xaa,0xab,0xab,0xab,0xac,0xac,0xad,0xad,0xae,0xae,0xae,
	0xaf,0xaf,0xb0,0xb0,0xe4,0xe4,0xe4,0xe5,0xe5,0xe6,0xe6,0xe6,0xe7,0xe7,0xe8,0xe8,
	0xe9,0xe9,0xe9,0xea,0xea,0xeb,0xeb,0xeb,0xec,0xec,0xed,0xed,0xee,0xee,0xee,0xef,
	0xef,0xf0,0xf0,0xf1,0xf1,0xf1,0xf2,0xf2,0xf3,0xf3,0xf3,0xf4,0xf4,0xf5,0xf5,0xf6,
	0xf6,0xf6,0xf7,0xf7,0xf8,0xf8,0xf9,0xf9,0xf9,0xfa,0xfa,0xfb,0xfb,0xfb,0xfc,0xfc,
	0xfd,0xfd,0xfe,0xfe,0xfe,0xff,0xff,0x00,0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,
	0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x06,0x07,0x07,0x08,0x08,0x08,0x09,0x09,0x0a,
	0x0a,0x0b,0x0b,0x0b,0x0c,0x0c,0x0d,0x0d,0x0e,0x0e,0x0e,0x0f,0x0f,0x10,0x10,0x10,
};

// H-counter table for hvcounter reads in 32col mode
static const UINT8 hcounts_32[] = {
	0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x07,0x08,0x08,0x08,0x09,0x09,0x09,0x0a,0x0a,
	0x0a,0x0b,0x0b,0x0b,0x0c,0x0c,0x0c,0x0d,0x0d,0x0d,0x0e,0x0e,0x0f,0x0f,0x0f,0x10,
	0x10,0x10,0x11,0x11,0x11,0x12,0x12,0x12,0x13,0x13,0x13,0x14,0x14,0x14,0x15,0x15,
	0x15,0x16,0x16,0x17,0x17,0x17,0x18,0x18,0x18,0x19,0x19,0x19,0x1a,0x1a,0x1a,0x1b,
	0x1b,0x1b,0x1c,0x1c,0x1c,0x1d,0x1d,0x1d,0x1e,0x1e,0x1f,0x1f,0x1f,0x20,0x20,0x20,
	0x21,0x21,0x21,0x22,0x22,0x22,0x23,0x23,0x23,0x24,0x24,0x24,0x25,0x25,0x26,0x26,
	0x26,0x27,0x27,0x27,0x28,0x28,0x28,0x29,0x29,0x29,0x2a,0x2a,0x2a,0x2b,0x2b,0x2b,
	0x2c,0x2c,0x2c,0x2d,0x2d,0x2e,0x2e,0x2e,0x2f,0x2f,0x2f,0x30,0x30,0x30,0x31,0x31,
	0x31,0x32,0x32,0x32,0x33,0x33,0x33,0x34,0x34,0x34,0x35,0x35,0x36,0x36,0x36,0x37,
	0x37,0x37,0x38,0x38,0x38,0x39,0x39,0x39,0x3a,0x3a,0x3a,0x3b,0x3b,0x3b,0x3c,0x3c,
	0x3d,0x3d,0x3d,0x3e,0x3e,0x3e,0x3f,0x3f,0x3f,0x40,0x40,0x40,0x41,0x41,0x41,0x42,
	0x42,0x42,0x43,0x43,0x43,0x44,0x44,0x45,0x45,0x45,0x46,0x46,0x46,0x47,0x47,0x47,
	0x48,0x48,0x48,0x49,0x49,0x49,0x4a,0x4a,0x4a,0x4b,0x4b,0x4b,0x4c,0x4c,0x4d,0x4d,
	0x4d,0x4e,0x4e,0x4e,0x4f,0x4f,0x4f,0x50,0x50,0x50,0x51,0x51,0x51,0x52,0x52,0x52,
	0x53,0x53,0x53,0x54,0x54,0x55,0x55,0x55,0x56,0x56,0x56,0x57,0x57,0x57,0x58,0x58,
	0x58,0x59,0x59,0x59,0x5a,0x5a,0x5a,0x5b,0x5b,0x5c,0x5c,0x5c,0x5d,0x5d,0x5d,0x5e,
	0x5e,0x5e,0x5f,0x5f,0x5f,0x60,0x60,0x60,0x61,0x61,0x61,0x62,0x62,0x62,0x63,0x63,
	0x64,0x64,0x64,0x65,0x65,0x65,0x66,0x66,0x66,0x67,0x67,0x67,0x68,0x68,0x68,0x69,
	0x69,0x69,0x6a,0x6a,0x6a,0x6b,0x6b,0x6c,0x6c,0x6c,0x6d,0x6d,0x6d,0x6e,0x6e,0x6e,
	0x6f,0x6f,0x6f,0x70,0x70,0x70,0x71,0x71,0x71,0x72,0x72,0x72,0x73,0x73,0x74,0x74,
	0x74,0x75,0x75,0x75,0x76,0x76,0x76,0x77,0x77,0x77,0x78,0x78,0x78,0x79,0x79,0x79,
	0x7a,0x7a,0x7b,0x7b,0x7b,0x7c,0x7c,0x7c,0x7d,0x7d,0x7d,0x7e,0x7e,0x7e,0x7f,0x7f,
	0x7f,0x80,0x80,0x80,0x81,0x81,0x81,0x82,0x82,0x83,0x83,0x83,0x84,0x84,0x84,0x85,
	0x85,0x85,0x86,0x86,0x86,0x87,0x87,0x87,0x88,0x88,0x88,0x89,0x89,0x89,0x8a,0x8a,
	0x8b,0x8b,0x8b,0x8c,0x8c,0x8c,0x8d,0x8d,0x8d,0x8e,0x8e,0x8e,0x8f,0x8f,0x8f,0x90,
	0x90,0x90,0x91,0x91,0xe8,0xe8,0xe8,0xe9,0xe9,0xe9,0xea,0xea,0xea,0xeb,0xeb,0xeb,
	0xec,0xec,0xec,0xed,0xed,0xed,0xee,0xee,0xee,0xef,0xef,0xf0,0xf0,0xf0,0xf1,0xf1,
	0xf1,0xf2,0xf2,0xf2,0xf3,0xf3,0xf3,0xf4,0xf4,0xf4,0xf5,0xf5,0xf5,0xf6,0xf6,0xf6,
	0xf7,0xf7,0xf8,0xf8,0xf8,0xf9,0xf9,0xf9,0xfa,0xfa,0xfa,0xfb,0xfb,0xfb,0xfc,0xfc,
	0xfc,0xfd,0xfd,0xfd,0xfe,0xfe,0xfe,0xff,0xff,0x00,0x00,0x00,0x01,0x01,0x01,0x02,
	0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04,0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x07,
	0x08,0x08,0x08,0x09,0x09,0x09,0x0a,0x0a,0x0a,0x0b,0x0b,0x0b,0x0c,0x0c,0x0c,0x0d,
};

static UINT16 __fastcall MegadriveVideoReadWord(UINT32 sekAddress)
{
	if (sekAddress > 0xC0001F)
		bprintf(PRINT_NORMAL, _T("Video Attempt to read word value of location %x\n"), sekAddress);

	UINT16 res = 0;

	switch (sekAddress & 0x1c) {
	case 0x00:	// data
		switch (RamVReg->type) {
			case 0: res = BURN_ENDIAN_SWAP_INT16(RamVid [(RamVReg->addr >> 1) & 0x7fff]); break;
			case 4: res = BURN_ENDIAN_SWAP_INT16(RamSVid[(RamVReg->addr >> 1) & 0x003f]); break;
			case 8: res = BURN_ENDIAN_SWAP_INT16(RamPal [(RamVReg->addr >> 1) & 0x003f]); break;
		}
		RamVReg->addr += RamVReg->reg[0xf];
		break;

	case 0x04:	// command
		{
			UINT16 d = RamVReg->status;
			if (SekCyclesLine() >= (328) && SekCyclesLine() <= (460))
				d|=0x0004; // H-Blank (Sonic3 vs)

			d |= ((RamVReg->reg[1]&0x40)^0x40) >> 3;  // set V-Blank if display is disabled
			d |= (RamVReg->pending_ints&0x20)<<2;     // V-int pending?
			if (d&0x100) RamVReg->status&=~0x100; // FIFO no longer full
			d |= (nCurrentFrame&1) ? 0x10 : 0x00;

			RamVReg->pending = 0; // ctrl port reads clear write-pending flag (Charles MacDonald)
			return d;
		}
		break;
	case 0x08: 	// H-counter info
		{
			UINT32 d;

			d = (SekCyclesLine()) & 0x1ff; // FIXME

			if (RamVReg->reg[12]&1)
				d = hcounts_40[d];
			else d = hcounts_32[d];

			//elprintf(EL_HVCNT, "hv: %02x %02x (%i) @ %06x", d, Pico.video.v_counter, SekCyclesDone(), SekPc);
			return d | (RamVReg->v_counter << 8);
		}
		break;

	default:
		bprintf(PRINT_NORMAL, _T("Video Attempt to read word value of location %x, %x\n"), sekAddress, sekAddress & 0x1c);
		break;
	}

	return res;
}

static UINT8 __fastcall MegadriveVideoReadByte(UINT32 sekAddress)
{
//	bprintf(PRINT_NORMAL, _T("Video Attempt to read byte value of location %x\n"), sekAddress);
	UINT16 res = MegadriveVideoReadWord(sekAddress & ~1);
	if ((sekAddress&1)==0) res >>= 8;
	return res & 0xff;
}

static void __fastcall MegadriveVideoWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	if (sekAddress > 0xC0001F)
		bprintf(PRINT_NORMAL, _T("Video Attempt to write word value %x to location %x\n"), wordValue, sekAddress);

	switch (sekAddress & 0x1c) {
	case 0x00:	// data
		if (RamVReg->pending) {
			CommandChange();
			RamVReg->pending = 0;
		}
		if ((RamVReg->command & 0x80) && (RamVReg->reg[1]&0x10) && (RamVReg->reg[0x17]>>6)==2) {

			DmaFill(wordValue);

		} else {

			// FIFO
			// preliminary FIFO emulation for Chaos Engine, The (E)
			if (!(RamVReg->status&8) && (RamVReg->reg[1]&0x40)) // active display?
			{
				RamVReg->status&=~0x200; // FIFO no longer empty
				RamVReg->lwrite_cnt++;
				if (RamVReg->lwrite_cnt >= 4) RamVReg->status|=0x100; // FIFO full
				if (RamVReg->lwrite_cnt >  4) {
					//SekRunAdjust(0-80);
					//SekIdle(80);
					//SekCyclesBurnRun(32);
				}
				//elprintf(EL_ASVDP, "VDP data write: %04x [%06x] {%i} #%i @ %06x", d, Pico.video.addr,
				//		 Pico.video.type, pvid->lwrite_cnt, SekPc);
			}

			//UINT32 a=Pico.video.addr;
			switch (RamVReg->type) {
			case 1:
				// If address is odd, bytes are swapped (which game needs this?)
				// williams arcade greatest hits -dink
				if (RamVReg->addr & 1) {
					//bprintf(PRINT_NORMAL, _T("Video address is odd, bytes are swapped!!!\n"));
					wordValue = (wordValue<<8)|(wordValue>>8);
				}
				RamVid[(RamVReg->addr >> 1) & 0x7fff] = BURN_ENDIAN_SWAP_INT16(wordValue);
            	RamVReg->rendstatus |= PDRAW_DIRTY_SPRITES;
            	break;
			case 3:
				//Pico.m.dirtyPal = 1;
				//dprintf("w[%i] @ %04x, inc=%i [%i|%i]", Pico.video.type, a, Pico.video.reg[0xf], Pico.m.scanline, SekCyclesDone());
				//CalcCol((RamVReg->addr >> 1) & 0x003f, wordValue); // no cram in C2
				break;
			case 5:
				RamSVid[(RamVReg->addr >> 1) & 0x003f] = BURN_ENDIAN_SWAP_INT16(wordValue);
				break;
			case 0x81: {
				UINT32 a = RamVReg->addr | (RamVReg->addr_u << 16);
				VideoWrite128(a, wordValue);
				break;
				}
			}
			//dprintf("w[%i] @ %04x, inc=%i [%i|%i]", Pico.video.type, a, Pico.video.reg[0xf], Pico.m.scanline, SekCyclesDone());
			//AutoIncrement();
			RamVReg->addr += RamVReg->reg[0xf];
		}
    	return;

	case 0x04:	// command
		if(RamVReg->pending) {
			// Low word of command:
			RamVReg->command &= 0xffff0000;
			RamVReg->command |= wordValue;
			RamVReg->pending = 0;
			CommandChange();
		} else {
			if((wordValue & 0xc000) == 0x8000) {
				INT32 num = (wordValue >> 8) & 0x1f;
				RamVReg->type = 0; // register writes clear command (else no Sega logo in Golden Axe II)
				if (num > 0x0a && !(RamVReg->reg[1] & 4)) {
					//bprintf(0, _T("%02x written to reg %02x in SMS mode @ %06x"), d, num, SekGetPC(-1));
					return;
				}

				// Blank last line
				if (num == 1 && !(wordValue&0x40) && SekCyclesLine() <= (488-390)) {
					BlankedLine = 1;
				}

				UINT8 oldreg = RamVReg->reg[num];
				RamVReg->reg[num] = wordValue & 0xff;

				// update IRQ level (Lemmings, Wiz 'n' Liz intro, ... )
				// may break if done improperly:
				// International Superstar Soccer Deluxe (crash), Street Racer (logos), Burning Force (gfx), Fatal Rewind (hang), Sesame Street Counting Cafe
				if(num < 2 && !SekShouldInterrupt()) {

					INT32 irq = 0;
					INT32 lines = (RamVReg->reg[1] & 0x20) | (RamVReg->reg[0] & 0x10);
					INT32 pints = (RamVReg->pending_ints&lines);
					if (pints & 0x20) irq = 6;
					else if (pints & 0x10) irq = 4;

					if (pints) {
						SekSetVIRQLine(irq, CPU_IRQSTATUS_ACK);
					} else {
						if (irq != 0) SekSetVIRQLine(irq, CPU_IRQSTATUS_NONE);
					}
				}

				if (num == 5) if (RamVReg->reg[num]^oldreg) RamVReg->rendstatus |= PDRAW_SPRITES_MOVED;

				if (num == 11) {
					const UINT8 h_msks[4] = { 0x00, 0x07, 0xf8, 0xff };
					RamVReg->h_mask = h_msks[RamVReg->reg[11] & 3];
				}
			} else {
				// High word of command:
				RamVReg->command &= 0x0000ffff;
				RamVReg->command |= wordValue << 16;
				RamVReg->pending = 1;
			}
		}
    	return;

	case 0x10:
	case 0x14:
		// PSG Sound
		//bprintf(PRINT_NORMAL, _T("PSG Attempt to write word value %04x to location %08x\n"), wordValue, sekAddress);
		SN76496Write(0, wordValue & 0xFF);
		return;

	}
	bprintf(0, _T("vdp unmapped write %X %X\n"), sekAddress, wordValue);
}

static void __fastcall MegadriveVideoWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	//bprintf(PRINT_NORMAL, _T("Video Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
	MegadriveVideoWriteWord(sekAddress, (byteValue << 8) | byteValue);
}

static void __fastcall segac2_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xec0200) == 0x800000) {
		protection_write(data);
		return;
	}

	if ((address & 0xec0200) == 0x800200) {
		enable_display = ~data & 1;
		if (!(data & 2)) prot_write_buf = prot_read_buf = 0;
		alt_palette_mode = ((~data & 4) >> 2);
		//bprintf(0, _T("alt_pallette_mode: %x  disp_en  %x  (scanline: %d\tframe: %d)\n"), alt_palette_mode, enable_display, Scanline, nCurrentFrame);
		recompute_palette_tables();
		return;
	}

	if ((address & 0xec0100) == 0x840000) {
		sega_315_5296_write((address & 0x1f) >> 1, data);
		return;
	}

	if ((address & 0xec0100) == 0x840100) {
		BurnYM3438Write(0, (address >> 1) & 3, data);
		return;
	}

	if ((address & 0xec0100) == 0x880000) {
		if (sound_rom_length) {
			UPD7759PortWrite(0, data & 0xff);
			UPD7759StartWrite(0, 0);
			UPD7759StartWrite(0, 1);
		}
		return;
	}

	if ((address & 0xec0100) == 0x880100) {
		// coin counters, other functions?
		return;
	}

	if ((address & 0xec0000) == 0x8c0000) {
		//bprintf(0, _T("pal_w %x  %x  (scanline: %d\tframe: %d)\n"), address, data, Scanline, nCurrentFrame);
		palette_write(address & 0xfff, data, 0xffff);
		return;
	}

	if ((address & 0xe70000) == 0xc00000) {
		MegadriveVideoWriteWord(address & 0x1f, data);
		return;
	}
	bprintf(0, _T("ww  %x  %x\n"), address, data);
}

static void __fastcall segac2_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xec0201) == 0x800001) {
		protection_write(data);
		return;
	}

	if ((address & 0xec0201) == 0x800201) {
		enable_display = ~data & 1;
		if (!(data & 2)) prot_write_buf = prot_read_buf = 0;
		alt_palette_mode = ((~data & 4) >> 2);
		//bprintf(0, _T("alt_pallette_mode.b: %x  disp_en  %x  (scanline: %d\tframe: %d)\n"), alt_palette_mode, enable_display, Scanline, nCurrentFrame);
		recompute_palette_tables();
		return;
	}

	if ((address & 0xec0101) == 0x840001) {
		sega_315_5296_write((address & 0x1f) >> 1, data);
		return;
	}

	if ((address & 0xec0101) == 0x840101) {
		BurnYM3438Write(0, (address >> 1) & 3, data);
		return;
	}

	if ((address & 0xec0101) == 0x880001) {
		if (sound_rom_length) {
			UPD7759PortWrite(0, data);
			UPD7759StartWrite(0, 0);
			UPD7759StartWrite(0, 1);
		}
		return;
	}

	if ((address & 0xec0100) == 0x880100) {
		// coin counters, other functions?
		return;
	}

	if ((address & 0xec0000) == 0x8c0000) {
		//bprintf(0, _T("pal_w.b %x  %x  (scanline: %d\tframe: %d)\n"), address, data, Scanline, nCurrentFrame);
		palette_write(address & 0xfff, data << ((~address & 1) * 8), 0xff << ((~address & 1) * 8));
		return;
	}

	if ((address & 0xe70000) == 0xc00000) {
		MegadriveVideoWriteByte(address & 0x1f, data);
		return;
	}

	if (address > 0x1fffff) { // puyopuy2 writes to romspace a lot
		bprintf(0, _T("wb  %x  %x\n"), address, data);
	}
}

static UINT16 __fastcall segac2_main_read_word(UINT32 address)
{
	if ((address & 0xec0200) == 0x800000) {
		return prot_read_buf | 0xf0;
	}

	if ((address & 0xec0100) == 0x840000) {
		return sega_315_5296_read((address & 0x1f) >> 1);
	}

	if ((address & 0xec0100) == 0x840100) {
		return BurnYM3438Read(0, (address >> 1) & 3);
	}

	if ((address & 0xec0000) == 0x8c0000) {
		return palette_read(address & 0x3ff);
	}

	if ((address & 0xe70000) == 0xc00000) {
		return MegadriveVideoReadWord(address & 0x1f);
	}

	bprintf(0, _T("rw %x\n"), address);

	return 0;
}

static UINT8 __fastcall segac2_main_read_byte(UINT32 address)
{
	if ((address & 0xec0200) == 0x800000) {
		return prot_read_buf | 0xf0;
	}

	if ((address & 0xec0101) == 0x840001) {
		return sega_315_5296_read((address & 0x1f) >> 1);
	}

	if ((address & 0xec0101) == 0x840101) {
		return BurnYM3438Read(0, (address >> 1) & 3);
	}

	if ((address & 0xec0101) == 0x880101) {
		// nop? (puyopuy2)
		return 0;
	}

	if ((address & 0xec0201) == 0x800201) {
		// nop? (puyopuy2)
		return 0;
	}

	if ((address & 0xec0000) == 0x8c0000) {
		return palette_read(address & 0x3ff) >> ((~address & 1) * 8);
	}

	if ((address & 0xe70000) == 0xc00000) {
		return MegadriveVideoReadByte(address & 0x1f);
	}

	bprintf(0, _T("rb %x\n"), address);

	return 0;
}

inline static void DrvFMIRQHandler(INT32, INT32 state)
{
	SekSetVIRQLine(2, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 __fastcall segac2_irq_callback(INT32 irqline)
{
	if (irqline == 4) {
		RamVReg->pending_ints &= ~0x10;
		SekSetVIRQLine(irqline, CPU_IRQSTATUS_NONE);
	}
	if (irqline == 6) {
		RamVReg->pending_ints &= ~0x20;
		SekSetVIRQLine(irqline, CPU_IRQSTATUS_NONE);
	}

	return (0x60 + irqline * 4) / 4; // vector address
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	BurnYM3438Reset();
	if (sound_rom_length) UPD7759Reset();
	SekClose();

	prot_write_buf = 0;
	prot_read_buf = 0;
	enable_display = 0;
	alt_palette_mode = 0;
	palette_bank = 0;
	bg_palbase = 0;
	sp_palbase = 0;

	// I/O chip
	memset (output_latch, 0, sizeof(output_latch));
	dir = 0;
	iocnt = 0;
	io_porth_write(0);

	SegaC2BgPalLookup[0] = 0x00;
	SegaC2BgPalLookup[1] = 0x10;
	SegaC2BgPalLookup[2] = 0x20;
	SegaC2BgPalLookup[3] = 0x30;

	SegaC2SpPalLookup[0] = 0x00;
	SegaC2SpPalLookup[1] = 0x10;
	SegaC2SpPalLookup[2] = 0x20;
	SegaC2SpPalLookup[3] = 0x30;

	irq4_counter = -1;
	irq6_line = 224;

	// default VDP register values (based on Fusion)
	memset(RamVReg, 0, sizeof(struct PicoVideo));
	RamVReg->reg[0x00] = 0x04;
	RamVReg->reg[0x01] = 0x04;
	RamVReg->reg[0x0c] = 0x81;
	RamVReg->reg[0x0f] = 0x02;
	RamVReg->status = 0x3408 | 0; // 'always set' bits | vblank | collision | pal
	RamVReg->rotate = 0;
	dma_xfers = 0;
	Scanline = 0;
	RamVReg->rendstatus = 0;
	interlacemode2 = 0;

	nExtraCycles[0] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x200000;

	DrvSndROM	= Next; Next += 0x0a0000;

	DrvPalette	= (UINT32*)Next; Next += 0x3001 * sizeof(UINT32); // +1 for black

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x001000;

	// picodrive vdp stuff
	RamPal		= (UINT16 *) Next; Next += 0x000040 * sizeof(UINT16);
	RamSVid		= (UINT16 *) Next; Next += 0x000040 * sizeof(UINT16);	// VSRam
	RamVid		= (UINT16 *) Next; Next += 0x010000 * sizeof(UINT16);	// Video Ram
	RamVReg		= (struct PicoVideo *)Next; Next += sizeof(struct PicoVideo);

	RamEnd		= Next;

	HighColFull	= (UINT16*) Next; Next += ((8 + 320 + 8) * ((240 + 1) * 2)) * sizeof(UINT16);

	HighCacheA	= (INT32 *) Next; Next += (41+1) * sizeof(INT32);	// caches for high layers
	HighCacheB	= (INT32 *) Next; Next += (41+1) * sizeof(INT32);
	HighPreSpr	= (INT32 *) Next; Next += (80*2+1) * sizeof(INT32);	// slightly preprocessed sprites

	MemEnd		= Next;

	return 0;
}

static INT32 SegaC2Init(UINT8 (*prot_read_cb)(UINT8))
{
	BurnAllocMemIndex();

	{
		char* pRomName;
		struct BurnRomInfo ri;
		UINT8 *pLoad = Drv68KROM;
		UINT8 *sLoad = DrvSndROM;

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
		{
			BurnDrvGetRomInfo(&ri, i);

			if ((ri.nType & BRF_PRG) && (ri.nType & 0x03) == 1)     // everyone else
			{
				if (BurnLoadRom(pLoad + 1, i+0, 2)) return 1;
				if (BurnLoadRom(pLoad + 0, i+1, 2)) return 1;
				pLoad += 0x100000;
				i++;
				continue;
			}

			if ((ri.nType & BRF_PRG) && (ri.nType & 0x03) == 3)		// Bloxeed is weird
			{
				if (BurnLoadRom(pLoad + 1, i+0, 2)) return 1;
				if (BurnLoadRom(pLoad + 0, i+1, 2)) return 1;
				pLoad += (ri.nLen * 2);
				i++;
				continue;
			}

			if ((ri.nType & BRF_SND) && (ri.nType & 0x03) == 2)
			{
				if (BurnLoadRom(sLoad, i, 1)) return 1;
				sLoad += ri.nLen;
				sound_rom_length += ri.nLen;
				continue;
			}
		}

		memcpy (DrvSndROM + 0x80000, DrvSndROM, 0x20000);
	}

	bprintf (0, _T("soundlen: %5.5x\n"), sound_rom_length);

	protection_read_cb = prot_read_cb;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetIrqCallback(segac2_irq_callback);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	for (INT32 i = 0; i < 0x200000; i += 0x10000) {
		SekMapMemory(Drv68KRAM,		0xe00000 + i, 0xe0ffff + i, MAP_RAM);
	}
	SekSetWriteWordHandler(0,	segac2_main_write_word);
	SekSetWriteByteHandler(0,	segac2_main_write_byte);
	SekSetReadWordHandler(0,	segac2_main_read_word);
	SekSetReadByteHandler(0,	segac2_main_read_byte);
	SekClose();

	BurnYM3438Init(1, 53693175 / 7, &DrvFMIRQHandler, 0);
	BurnTimerAttachNull(53693175 / 6);
	BurnYM3438SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);

	SN76496Init(0, 53693175 / 15, 1);
	SN76496SetBuffered(SekTotalCycles, 53693175 / 6);
	SN76496SetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);

	if (sound_rom_length)
	{
		UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvSndROM + 0x80000);
		UPD7759SetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);
		UPD7759SetSyncCallback(0, SekTotalCycles, 53693175 / 6);
	}

	GenericTilesInit();

	if (has_dial) BurnTrackballInit(2); // twinsqua

	// I/O Chip
	dir_override = 0xff;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3438Exit();
	SN76496Exit();
	if (sound_rom_length) UPD7759Exit();
	SekExit();

	BurnFreeMemIndex();

	if (has_dial) BurnTrackballExit();
	has_dial = 0;

	sound_rom_length = 0;

	is_wwmarine = 0;
	is_tfrceacb = 0;
	is_ribbit = 0;

	return 0;
}

//---------------------------------------------------------------
// Megadrive Draw
//---------------------------------------------------------------

#define TileNormMaker_(pix_func)                             \
{                                                            \
  unsigned int t;                                            \
                                                             \
  t = (pack&0x0000f000)>>12; pix_func(0);                    \
  t = (pack&0x00000f00)>> 8; pix_func(1);                    \
  t = (pack&0x000000f0)>> 4; pix_func(2);                    \
  t = (pack&0x0000000f)    ; pix_func(3);                    \
  t = (pack&0xf0000000)>>28; pix_func(4);                    \
  t = (pack&0x0f000000)>>24; pix_func(5);                    \
  t = (pack&0x00f00000)>>20; pix_func(6);                    \
  t = (pack&0x000f0000)>>16; pix_func(7);                    \
}

#define TileFlipMaker_(pix_func)                             \
{                                                            \
  unsigned int t;                                            \
                                                             \
  t = (pack&0x000f0000)>>16; pix_func(0);                    \
  t = (pack&0x00f00000)>>20; pix_func(1);                    \
  t = (pack&0x0f000000)>>24; pix_func(2);                    \
  t = (pack&0xf0000000)>>28; pix_func(3);                    \
  t = (pack&0x0000000f)    ; pix_func(4);                    \
  t = (pack&0x000000f0)>> 4; pix_func(5);                    \
  t = (pack&0x00000f00)>> 8; pix_func(6);                    \
  t = (pack&0x0000f000)>>12; pix_func(7);                    \
}

#define TileNormMaker(funcname, pix_func) \
static void funcname(UINT16 *pd, unsigned int pack, int pal) \
TileNormMaker_(pix_func)

#define TileFlipMaker(funcname, pix_func) \
static void funcname(UINT16 *pd, unsigned int pack, int pal) \
TileFlipMaker_(pix_func)

#define TileNormMakerAS(funcname, pix_func) \
static void funcname(UINT16 *pd, unsigned char *mb, unsigned int pack, int pal) \
TileNormMaker_(pix_func)

#define TileFlipMakerAS(funcname, pix_func) \
static void funcname(UINT16 *pd, unsigned char *mb, unsigned int pack, int pal) \
TileFlipMaker_(pix_func)

#define pix_just_write(x) \
  if (t) pd[x]=pal|t

TileNormMaker(TileNorm,pix_just_write)
TileFlipMaker(TileFlip,pix_just_write)

// draw a sprite pixel, process operator colors
#define pix_sh(x) \
  if (!t); \
  else if (t>=0xe) pd[x]=(pd[x]&0x3f)|(t<<6); /* c0 shadow, 80 hilight */ \
  else pd[x]=pal|t

TileNormMaker(TileNormSH, pix_sh)
TileFlipMaker(TileFlipSH, pix_sh)

// draw a sprite pixel, mark operator colors
#define pix_sh_markop(x) \
  if (!t); \
  else if (t>=0xe) pd[x]|=0x80; \
  else pd[x]=pal|t

TileNormMaker(TileNormSH_markop, pix_sh_markop)
TileFlipMaker(TileFlipSH_markop, pix_sh_markop)

// process operator pixels only, apply only on low pri tiles and other op pixels
#define pix_sh_onlyop(x) \
  if (t>=0xe && (pd[x]&0xc0)) \
    pd[x]=(pd[x]&0x3f)|(t<<6); /* c0 shadow, 80 hilight */ \

TileNormMaker(TileNormSH_onlyop_lp, pix_sh_onlyop)
TileFlipMaker(TileFlipSH_onlyop_lp, pix_sh_onlyop)

// draw a sprite pixel (AS)
#define pix_as(x) \
  if (t & mb[x]) mb[x] = 0, pd[x] = pal | t

TileNormMakerAS(TileNormAS, pix_as)
TileFlipMakerAS(TileFlipAS, pix_as)

// draw a sprite pixel, process operator colors (AS)
#define pix_sh_as(x) \
  if (t & mb[x]) { \
    mb[x] = 0; \
    if (t>=0xe) pd[x]=(pd[x]&0x3f)|(t<<6); /* c0 shadow, 80 hilight */ \
    else pd[x] = pal | t; \
  }

TileNormMakerAS(TileNormSH_AS, pix_sh_as)
TileFlipMakerAS(TileFlipSH_AS, pix_sh_as)

#define pix_sh_as_onlyop(x) \
  if (t & mb[x]) { \
    mb[x] = 0; \
    pix_sh_onlyop(x); \
  }

TileNormMakerAS(TileNormSH_AS_onlyop_lp, pix_sh_as_onlyop)
TileFlipMakerAS(TileFlipSH_AS_onlyop_lp, pix_sh_as_onlyop)

// mark pixel as sprite pixel (AS)
#define pix_sh_as_onlymark(x) \
  if (t) mb[x] = 0

TileNormMakerAS(TileNormAS_onlymark, pix_sh_as_onlymark)
TileFlipMakerAS(TileFlipAS_onlymark, pix_sh_as_onlymark)

// forced both layer draw (through debug reg)
#define pix_and(x) \
  pd[x] = (pd[x] & 0xc0) | (pd[x] & (pal | t))

TileNormMaker(TileNorm_and, pix_and)
TileFlipMaker(TileFlip_and, pix_and)

// --------------------------------------------


static void DrawStrip(struct TileStrip *ts, int lflags, int cellskip)
{
  UINT16 *pd = HighCol;
  int tilex,dx,ty,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0,sh;

  // Draw tiles across screen:
  sh = (lflags & LF_SH) << 5; // 0x40
  tilex=((-ts->hscroll)>>3)+cellskip;
  ty=(ts->line&7)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells - cellskip;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more
  dx+=cellskip<<3;

  for (; cells > 0; dx+=8, tilex++, cells--)
  {
    unsigned int pack;

    code = RamVid[ts->nametab + (tilex & ts->xmask)];
    if (code == blank)
      continue;
	if ((code >> 15) | (lflags & LF_FORCE)) { // high priority tile
      int cval = code | (dx<<16) | (ty<<25);
      if(code&0x1000) cval^=7<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      addr+=ty;
      if (code&0x1000) addr^=0xe; // Y-flip

      pal=((code>>9)&0x30)|sh;
    }

    pack = *(unsigned int *)(RamVid + addr);
    if (!pack) {
      blank = code;
      continue;
    }

	if (~nBurnLayer & 4) return;

    if (code & 0x0800) TileFlip(pd + dx, pack, pal);
    else               TileNorm(pd + dx, pack, pal);
  }

  // terminate the cache list
  *ts->hc = 0;
  // if oldcode wasn't changed, it means all layer is hi priority
  if (oldcode == -1) RamVReg->rendstatus |= PDRAW_PLANE_HI_PRIO;
}

static void DrawStripVSRam(struct TileStrip *ts, int plane_sh, int cellskip)
{
  UINT16 *pd = HighCol;
  int tilex,dx,code=0,addr=0,cell=0;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0,scan=Scanline;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  dx=((ts->hscroll-1)&7)+1;
  if (ts->hscroll & 0x0f) {
    int adj = ((ts->hscroll ^ dx) >> 3) & 1;
    cell -= adj + 1;
    ts->cells -= adj;
  }
  cell+=cellskip;
  tilex+=cellskip;
  dx+=cellskip<<3;

  for (; cell < ts->cells; dx+=8,tilex++,cell++)
  {
    int nametabadd, ty;
    unsigned int pack;

    //if((cell&1)==0)
    {
      int line,vscroll;
      vscroll=RamSVid[(plane_sh&1)+(cell&~1)];

      // Find the line in the name table
      line=(vscroll+scan)&ts->line&0xffff; // ts->line is really ymask ..
      nametabadd=(line>>3)<<(ts->line>>24);    // .. and shift[width]
      ty=(line&7)<<1; // Y-Offset into tile
    }

    code=RamVid[ts->nametab+nametabadd+(tilex&ts->xmask)];
    if (code==blank) continue;
    if (code>>15) { // high priority tile
      int cval = code | (dx<<16) | (ty<<25);
      if(code&0x1000) cval^=7<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pal=((code>>9)&0x30)|((plane_sh<<5)&0x40);
    }

    pack = *(unsigned int *)(RamVid + addr);
    if (!pack) {
      blank = code;
      continue;
    }

	if (~nBurnLayer & 2) return;

    if (code & 0x0800) TileFlip(pd + dx, pack, pal);
    else               TileNorm(pd + dx, pack, pal);
  }

  // terminate the cache list
  *ts->hc = 0;
  if (oldcode == -1) RamVReg->rendstatus |= PDRAW_PLANE_HI_PRIO;
}

void DrawStripInterlace(struct TileStrip *ts)
{
  UINT16 *pd = HighCol;
  int tilex=0,dx=0,ty=0,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  ty=(ts->line&15)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more

  for (; cells; dx+=8,tilex++,cells--)
  {
    unsigned int pack;

    code = RamVid[ts->nametab + (tilex & ts->xmask)];
    if (code==blank) continue;
    if (code>>15) { // high priority tile
      int cval = (code&0xfc00) | (dx<<16) | (ty<<25);
      cval|=(code&0x3ff)<<1;
      if(code&0x1000) cval^=0xf<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<5;
      if (code&0x1000) addr+=30-ty; else addr+=ty; // Y-flip

//      pal=Pico.cram+((code>>9)&0x30);
      pal=((code>>9)&0x30);
    }

    pack = *(unsigned int *)(RamVid + addr);
    if (!pack) {
      blank = code;
      continue;
    }

    if (code & 0x0800) TileFlip(pd + dx, pack, pal);
    else               TileNorm(pd + dx, pack, pal);
  }

  // terminate the cache list
  *ts->hc = 0;
}

// --------------------------------------------


static void DrawLayer(int plane_sh, int *hcache, int cellskip, int maxcells)
{
  const char shift[4]={5,6,5,7}; // 32,64 or 128 sized tilemaps (2 is invalid)
  const unsigned char h_masks[4] = { 0x00, 0x07, 0xf8, 0xff };
  struct TileStrip ts;
  int width, height, ymask;
  int vscroll, htab;

  ts.hc=hcache;
  ts.cells=maxcells;

  // Work out the TileStrip to draw

  // Work out the name table size: 32 64 or 128 tiles (0-3)
  width=RamVReg->reg[16];
  height=(width>>4)&3; width&=3;

  ts.xmask=(1<<shift[width])-1; // X Mask in tiles (0x1f-0x7f)
  ymask=(height<<8)|0xff;       // Y Mask in pixels
  switch (width) {
    case 1: ymask &= 0x1ff; break;
    case 2: ymask =  0x007; break;
    case 3: ymask =  0x0ff; break;
  }

  // Find name table:
  if (plane_sh&1) ts.nametab=(RamVReg->reg[4]&0x07)<<12; // B
  else            ts.nametab=(RamVReg->reg[2]&0x38)<< 9; // A

  htab=RamVReg->reg[13]<<9; // Horizontal scroll table address
  htab+=(Scanline&h_masks[RamVReg->reg[11]&3])<<1; // Point to line (masked)
  htab+=plane_sh&1; // A or B

  // Get horizontal scroll value, will be masked later
  ts.hscroll = RamVid[htab & 0x7fff];

  if((RamVReg->reg[12]&6) == 6) {
    // interlace mode 2
    vscroll = RamSVid[plane_sh & 1]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+(Scanline<<1))&((ymask<<1)|1);
    ts.nametab+=(ts.line>>4)<<shift[width];

    DrawStripInterlace(&ts);
  } else if( RamVReg->reg[11]&4) {
    // shit, we have 2-cell column based vscroll
    // luckily this doesn't happen too often
    ts.line=ymask|(shift[width]<<24); // save some stuff instead of line
    DrawStripVSRam(&ts, plane_sh, cellskip);
  } else {
    vscroll = RamSVid[plane_sh & 1]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+Scanline)&ymask;
    ts.nametab+=(ts.line>>3)<<shift[width];

    DrawStrip(&ts, plane_sh, cellskip);
  }
}

// --------------------------------------------

// tstart & tend are tile pair numbers
static void DrawWindow(int tstart, int tend, int prio, int sh)
{
  UINT16 *pd = HighCol;
  int tilex,ty,nametab,code=0;
  int blank=-1; // The tile we know is blank

  if (~nSpriteEnable & 0x10) return;

  // Find name table line:
  if (RamVReg->reg[12]&1)
  {
    nametab=(RamVReg->reg[3]&0x3c)<<9; // 40-cell mode
    nametab+=(Scanline>>3)<<6;
  }
  else
  {
    nametab=(RamVReg->reg[3]&0x3e)<<9; // 32-cell mode
    nametab+=(Scanline>>3)<<5;
  }

  tilex=tstart<<1;

  if (!(RamVReg->rendstatus & PDRAW_WND_DIFF_PRIO)) {
    // check the first tile code
    code = RamVid[nametab + tilex];
    // if the whole window uses same priority (what is often the case), we may be able to skip this field
    if ((code>>15) != prio) return;
  }

  tend<<=1;
  ty=(Scanline&7)<<1; // Y-Offset into tile

  // Draw tiles across screen:
  if (!sh)
  {
    for (; tilex < tend; tilex++)
    {
      unsigned int pack;
      int dx, addr;
      int pal;

      code = RamVid[nametab + tilex];
      if (code==blank) continue;
      if ((code>>15) != prio) {
        RamVReg->rendstatus |= PDRAW_WND_DIFF_PRIO;
        continue;
      }

      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pack = *(unsigned int *)(RamVid + addr);
      if (!pack) {
        blank = code;
        continue;
      }

      pal = ((code >> 9) & 0x30);
      dx = 8 + (tilex << 3);

      if (code & 0x0800) TileFlip(pd + dx, pack, pal);
      else               TileNorm(pd + dx, pack, pal);
    }
  }
  else
  {
    for (; tilex < tend; tilex++)
    {
      unsigned int pack;
      int dx, addr;
      int pal;

      code = RamVid[nametab + tilex];
      if(code==blank) continue;
      if((code>>15) != prio) {
        RamVReg->rendstatus |= PDRAW_WND_DIFF_PRIO;
        continue;
      }

      pal=((code>>9)&0x30);

      if (prio) {
        UINT16 *zb = (UINT16 *)(HighCol+8+(tilex<<3));
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
        *zb &= 0x00bf; zb++;
      } else {
        pal |= 0x40;
      }

      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pack = *(unsigned int *)(RamVid + addr);
      if (!pack) {
        blank = code;
        continue;
      }

      dx = 8 + (tilex << 3);

      if (code & 0x0800) TileFlip(pd + dx, pack, pal);
      else               TileNorm(pd + dx, pack, pal);
    }
  }
}

// --------------------------------------------

static void DrawTilesFromCacheShPrep(void)
{
  // as some layer has covered whole line with hi priority tiles,
  // we can process whole line and then act as if sh/hi mode was off,
  // but leave lo pri op sprite markers alone
	int c = 320;
	UINT16 *zb = (UINT16 *)(HighCol+8);
  RamVReg->rendstatus |= PDRAW_SHHI_DONE;
  while (c--)
  {
    *zb++ &= 0x80bf; // dink maybe 0x80bf ?
  }
}

static void DrawTilesFromCache(int *hc, int sh, int rlim)
{
  UINT16 *pd = HighCol;
  int code, addr, dx;
  unsigned int pack;
  int pal;

  // *ts->hc++ = code | (dx<<16) | (ty<<25); // cache it

  if (~nBurnLayer & 1) return;

  if (sh && (RamVReg->rendstatus & (PDRAW_SHHI_DONE|PDRAW_PLANE_HI_PRIO)))
  {
    if (!(RamVReg->rendstatus & PDRAW_SHHI_DONE))
      DrawTilesFromCacheShPrep();
    sh = 0;
  }

  if (!sh)
  {
	  if (~nSpriteEnable & 0x40) return;
    short blank=-1; // The tile we know is blank
    while ((code=*hc++)) {
      if (!(code & 0x8000) || (short)code == blank)
        continue;
      // Get tile address/2:
      addr = (code & 0x7ff) << 4;
      addr += code >> 25; // y offset into tile

      pack = *(unsigned int *)(RamVid + addr);
      if (!pack) {
        blank = (short)code;
        continue;
      }

      dx = (code >> 16) & 0x1ff;
      pal = ((code >> 9) & 0x30);
      if (rlim-dx < 0)
        goto last_cut_tile;

      if (code & 0x0800) TileFlip(pd + dx, pack, pal);
      else               TileNorm(pd + dx, pack, pal);
    }
  }
  else
  {
	  if (~nSpriteEnable & 0x80) return;
    while ((code=*hc++)) {
      UINT16 *zb;

      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      addr+=(unsigned int)code>>25; // y offset into tile
      dx=(code>>16)&0x1ff;
      zb = HighCol+dx;
      *zb++ &= 0x00bf; *zb++ &= 0x00bf; *zb++ &= 0x00bf; *zb++ &= 0x00bf;
      *zb++ &= 0x00bf; *zb++ &= 0x00bf; *zb++ &= 0x00bf; *zb++ &= 0x00bf;

      pack = *(unsigned int *)(RamVid + addr);
      if (!pack)
        continue;

      pal = ((code >> 9) & 0x30);
      if (rlim - dx < 0)
        goto last_cut_tile;

      if (code & 0x0800) TileFlip(pd + dx, pack, pal);
      else               TileNorm(pd + dx, pack, pal);
    }
  }
  return;

last_cut_tile:
  // for vertical window cutoff
  {
    unsigned int t;

    pd += dx;
    if (code&0x0800)
    {
      switch (rlim-dx+8)
      {
        case 7: t=pack&0x00000f00; if (t) pd[6]=(pal|(t>> 8)); // "break" is left out intentionally
        case 6: t=pack&0x000000f0; if (t) pd[5]=(pal|(t>> 4));
        case 5: t=pack&0x0000000f; if (t) pd[4]=(pal|(t    ));
        case 4: t=pack&0xf0000000; if (t) pd[3]=(pal|(t>>28));
        case 3: t=pack&0x0f000000; if (t) pd[2]=(pal|(t>>24));
        case 2: t=pack&0x00f00000; if (t) pd[1]=(pal|(t>>20));
        case 1: t=pack&0x000f0000; if (t) pd[0]=(pal|(t>>16));
        default: break;
      }
    }
    else
    {
      switch (rlim-dx+8)
      {
        case 7: t=pack&0x00f00000; if (t) pd[6]=(pal|(t>>20));
        case 6: t=pack&0x0f000000; if (t) pd[5]=(pal|(t>>24));
        case 5: t=pack&0xf0000000; if (t) pd[4]=(pal|(t>>28));
        case 4: t=pack&0x0000000f; if (t) pd[3]=(pal|(t    ));
        case 3: t=pack&0x000000f0; if (t) pd[2]=(pal|(t>> 4));
        case 2: t=pack&0x00000f00; if (t) pd[1]=(pal|(t>> 8));
        case 1: t=pack&0x0000f000; if (t) pd[0]=(pal|(t>>12));
        default: break;
      }
    }
  }
}

// Index + 0  :    hhhhvvvv ab--hhvv yyyyyyyy yyyyyyyy // a: offscreen h, b: offs. v, h: horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

static void DrawSprite(int *sprite, int sh)
{
  void (*fTileFunc)(UINT16 *pd, unsigned int pack, int pal);
  UINT16 *pd = HighCol;
  int width=0,height=0;
  int row=0,code=0;
  int pal;
  int tile=0,delta=0;
  int sx, sy;

  if (~nSpriteEnable & 0x01) return;

  // parse the sprite data
  sy=sprite[0];
  code=sprite[1];
  sx=code>>16; // X
  width=sy>>28;
  height=(sy>>24)&7; // Width and height in tiles
  sy=(sy<<16)>>16; // Y

  row=Scanline-sy; // Row of the sprite we are on

  if (code&0x1000) row=(height<<3)-1-row; // Flip Y

  tile=code + (row>>3); // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile &= 0x7ff; tile<<=4; tile+=(row&7)<<1; // Tile address
  delta<<=4; // Delta of address

  pal=(code>>9)&0x30;
  pal|=(sh<<6)|0x8000; // 0x8000 = c2 sprite (dink)

  if (sh && (code&0x6000) == 0x6000) {
    if(code&0x0800) fTileFunc=TileFlipSH_markop;
    else            fTileFunc=TileNormSH_markop;
  } else {
    if(code&0x0800) fTileFunc=TileFlip;
    else            fTileFunc=TileNorm;
  }

  for (; width; width--,sx+=8,tile+=delta)
  {
    unsigned int pack;

    if(sx<=0)   continue;
    if(sx>=328) break; // Offscreen

    pack = *(unsigned int *)(RamVid + (tile & 0x7fff));
    fTileFunc(pd + sx, pack, pal);
  }
}

static void DrawTilesFromCacheForced(const int *hc)
{
  UINT16 *pd = HighCol;
  int code, addr, dx;
  unsigned int pack;
  int pal;

  // *ts->hc++ = code | (dx<<16) | (ty<<25);
  while ((code = *hc++)) {
    // Get tile address/2:
    addr = (code & 0x7ff) << 4;
    addr += (code >> 25) & 0x0e; // y offset into tile

    dx = (code >> 16) & 0x1ff;
    pal = ((code >> 9) & 0x30);
    pack = *(unsigned int *)(RamVid + addr);

    if (code & 0x0800) TileFlip_and(pd + dx, pack, pal);
    else               TileNorm_and(pd + dx, pack, pal);
  }
}



static void DrawSpriteInterlace(unsigned int *sprite)
{
  UINT16 *pd = HighCol;
  int width=0,height=0;
  int row=0,code=0;
  int pal;
  int tile=0,delta=0;
  int sx, sy;

  if (~nSpriteEnable & 0x02) return;

  // parse the sprite data
  sy=sprite[0];
  height=sy>>24;
  sy=(sy&0x3ff)-0x100; // Y
  width=(height>>2)&3; height&=3;
  width++; height++; // Width and height in tiles

  row=(Scanline<<1)-sy; // Row of the sprite we are on

  code=sprite[1];
  sx=((code>>16)&0x1ff)-0x78; // X

  if (code&0x1000) row^=(16<<height)-1; // Flip Y

  tile=code&0x3ff; // Tile number
  tile+=row>>4; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=5; tile+=(row&15)<<1; // Tile address

  delta<<=5; // Delta of address
  pal=((code>>9)&0x30)|0x8000; // Get palette pointer

  for (; width; width--,sx+=8,tile+=delta)
  {
    unsigned int pack;

    if(sx<=0)   continue;
    if(sx>=328) break; // Offscreen

    pack = *(unsigned int *)(RamVid + (tile & 0x7fff));
    if (code & 0x0800) TileFlip(pd + sx, pack, pal);
    else               TileNorm(pd + sx, pack, pal);
  }
}


static void DrawAllSpritesInterlace(int pri, int sh)
{
  int i,u,table,link=0,sline=(Scanline<<1)+RamVReg->field;;
  unsigned int *sprites[80]; // Sprite index

  table=RamVReg->reg[5]&0x7f;
  if (RamVReg->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  for (i=u=0; u < 80 && i < 21; u++)
  {
    unsigned int *sprite;
    int code, sx, sy, height;

    sprite=(unsigned int *)(RamVid+((table+(link<<2))&0x7ffc)); // Find sprite

    // get sprite info
    code = sprite[0];
    sx = sprite[1];
    if(((sx>>15)&1) != pri) goto nextsprite; // wrong priority sprite

    // check if it is on this line
    sy = (code&0x3ff)-0x100;
    height = (((code>>24)&3)+1)<<4;
    if(sline < sy || sline >= sy+height) goto nextsprite; // no

    // check if sprite is not hidden offscreen
    sx = (sx>>16)&0x1ff;
    sx -= 0x78; // Get X coordinate + 8
    if(sx <= -8*3 || sx >= 328) goto nextsprite;

    // sprite is good, save it's pointer
    sprites[i++]=sprite;

    nextsprite:
    // Find next sprite
    link=(code>>16)&0x7f;
    if(!link) break; // End of sprites
  }

  // Go through sprites backwards:
  for (i-- ;i>=0; i--)
    DrawSpriteInterlace(sprites[i]);
}


/*
 * s/h drawing: lo_layers|40, lo_sprites|40 && mark_op,
 *        hi_layers&=~40, hi_sprites
 *
 * Index + 0  :    hhhhvvvv ----hhvv yyyyyyyy yyyyyyyy // v, h: vert./horiz. size
 * Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8
 */
static void DrawSpritesSHi(unsigned char *sprited)
{
  void (*fTileFunc)(UINT16 *pd, unsigned int pack, int pal);
  UINT16 *pd = HighCol;
  unsigned char *p;
  int cnt;

  if (~nSpriteEnable & 0x04) return;

  cnt = sprited[0] & 0x7f;
  if (cnt == 0) return;

  p = &sprited[3];

  // Go through sprites backwards:
  for (cnt--; cnt >= 0; cnt--)
  {
    int *sprite, code, pal, tile, sx, sy;
    int offs, delta, width, height, row;

    offs = (p[cnt] & 0x7f) * 2;
    sprite = HighPreSpr + offs;
    code = sprite[1];
    pal = (code>>9)&0x30;

    if (pal == 0x30)
    {
      if (code & 0x8000) // hi priority
      {
        if (code&0x800) fTileFunc=TileFlipSH;
        else            fTileFunc=TileNormSH;
      } else {
        if (code&0x800) fTileFunc=TileFlipSH_onlyop_lp;
        else            fTileFunc=TileNormSH_onlyop_lp;
      }
    } else {
      if (!(code & 0x8000)) continue; // non-operator low sprite, already drawn
      if (code&0x800) fTileFunc=TileFlip;
      else            fTileFunc=TileNorm;
    }

    // parse remaining sprite data
    sy=sprite[0];
    sx=code>>16; // X
    width=sy>>28;
    height=(sy>>24)&7; // Width and height in tiles
    sy=(sy<<16)>>16; // Y

    row=Scanline-sy; // Row of the sprite we are on

    if (code&0x1000) row=(height<<3)-1-row; // Flip Y

    tile=code + (row>>3); // Tile number increases going down
    delta=height; // Delta to increase tile by going right
    if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

    tile &= 0x7ff; tile<<=4; tile+=(row&7)<<1; // Tile address
    delta<<=4; // Delta of address

    for (; width; width--,sx+=8,tile+=delta)
    {
      unsigned int pack;

      if(sx<=0)   continue;
      if(sx>=328) break; // Offscreen

      pack = *(unsigned int *)(RamVid + (tile & 0x7fff));
      fTileFunc(pd + sx, pack, pal|0x8000);
    }
  }
}

static void DrawSpritesHiAS(unsigned char *sprited, int sh)
{
  void (*fTileFunc)(UINT16 *pd, unsigned char *mb,
                    unsigned int pack, int pal);
  UINT16 *pd = HighCol;
  unsigned char mb[8+320+8];
  unsigned char *p;
  int entry, cnt;

  if (~nSpriteEnable & 0x08) return;

  cnt = sprited[0] & 0x7f;
  if (cnt == 0) return;

  memset(mb, 0xff, sizeof(mb));
  p = &sprited[3];

  // Go through sprites:
  for (entry = 0; entry < cnt; entry++)
  {
    int *sprite, code, pal, tile, sx, sy;
    int offs, delta, width, height, row;

    offs = (p[entry] & 0x7f) * 2;
    sprite = HighPreSpr + offs;
    code = sprite[1];
    pal = (code>>9)&0x30;

    if (sh && pal == 0x30)
    {
      if (code & 0x8000) // hi priority
      {
        if (code&0x800) fTileFunc = TileFlipSH_AS;
        else            fTileFunc = TileNormSH_AS;
      } else {
        if (code&0x800) fTileFunc = TileFlipSH_AS_onlyop_lp;
        else            fTileFunc = TileNormSH_AS_onlyop_lp;
      }
    } else {
      if (code & 0x8000) // hi priority
      {
        if (code&0x800) fTileFunc = TileFlipAS;
        else            fTileFunc = TileNormAS;
      } else {
        if (code&0x800) fTileFunc = TileFlipAS_onlymark;
        else            fTileFunc = TileNormAS_onlymark;
      }
    }

    // parse remaining sprite data
    sy=sprite[0];
    sx=code>>16; // X
    width=sy>>28;
    height=(sy>>24)&7; // Width and height in tiles
    sy=(sy<<16)>>16; // Y

    row=Scanline-sy; // Row of the sprite we are on

    if (code&0x1000) row=(height<<3)-1-row; // Flip Y

    tile=code + (row>>3); // Tile number increases going down
    delta=height; // Delta to increase tile by going right
    if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

    tile &= 0x7ff; tile<<=4; tile+=(row&7)<<1; // Tile address
    delta<<=4; // Delta of address

    for (; width; width--,sx+=8,tile+=delta)
    {
      unsigned int pack;

      if(sx<=0)   continue;
      if(sx>=328) break; // Offscreen

      pack = *(unsigned int *)(RamVid + (tile & 0x7fff));
      fTileFunc(pd + sx, mb + sx, pack, pal|0x8000);
    }
  }
}


// Index + 0  :    ----hhvv -lllllll -------y yyyyyyyy
// Index + 4  :    -------x xxxxxxxx pccvhnnn nnnnnnnn
// v
// Index + 0  :    hhhhvvvv ----hhvv yyyyyyyy yyyyyyyy // v, h: vert./horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

static void PrepareSprites(int full)
{
  int u,link=0,sh;
  int table=0;
  int *pd = HighPreSpr;
  int max_lines = 224, max_sprites = 80, max_width = 328;
  int max_line_sprites = 20; // 20 sprites, 40 tiles

  if (!(RamVReg->reg[12]&1))
    max_sprites = 64, max_line_sprites = 16, max_width = 264;
  if (0) //PicoIn.opt & POPT_DIS_SPRITE_LIM)
    max_line_sprites = MAX_LINE_SPRITES;

  if (RamVReg->reg[1]&8) max_lines = 240;
  sh = RamVReg->reg[0xC]&8; // shadow/hilight?

  table=RamVReg->reg[5]&0x7f;
  if (RamVReg->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  if (!full)
  {
    int pack;
    // updates: tilecode, sx
    for (u=0; u < max_sprites && (pack = *pd); u++, pd+=2)
    {
      unsigned int *sprite;
      int code2, sx, sy, height;

      sprite=(unsigned int *)(RamVid+((table+(link<<2))&0x7ffc)); // Find sprite

      // parse sprite info
      code2 = sprite[1];
      sx = (code2>>16)&0x1ff;
      sx -= 0x78; // Get X coordinate + 8
      sy = (pack << 16) >> 16;
      height = (pack >> 24) & 0xf;

      if (sy < max_lines &&
	  sy + (height<<3) > Scanline && // sprite onscreen (y)?
          (sx > -24 || sx < max_width))                   // onscreen x
      {
        int y = (sy >= Scanline) ? sy : Scanline;
        int entry = ((pd - HighPreSpr) / 2) | ((code2>>8)&0x80);
        for (; y < sy + (height<<3) && y < max_lines; y++)
        {
          int i, cnt;
          cnt = HighLnSpr[y][0] & 0x7f;
          if (cnt >= max_line_sprites) continue;              // sprite limit?

          for (i = 0; i < cnt; i++)
            if (((HighLnSpr[y][3+i] ^ entry) & 0x7f) == 0) goto found;

          // this sprite was previously missing
          HighLnSpr[y][3+cnt] = entry;
          HighLnSpr[y][0] = cnt + 1;
found:;
          if (entry & 0x80)
               HighLnSpr[y][1] |= SPRL_HAVE_HI;
          else HighLnSpr[y][1] |= SPRL_HAVE_LO;
        }
      }

      code2 &= ~0xfe000000;
      code2 -=  0x00780000; // Get X coordinate + 8 in upper 16 bits
      pd[1] = code2;

      // Find next sprite
      link=(sprite[0]>>16)&0x7f;
      if (!link) break; // End of sprites
    }
  }
  else
  {
    for (u = 0; u < max_lines; u++)
	  *((int *)&HighLnSpr[u][0]) = 0;

    for (u = 0; u < max_sprites; u++)
    {
      unsigned int *sprite;
      int code, code2, sx, sy, hv, height, width;

      sprite=(unsigned int *)(RamVid+((table+(link<<2))&0x7ffc)); // Find sprite

      // parse sprite info
      code = sprite[0];
      sy = (code&0x1ff)-0x80;
      hv = (code>>24)&0xf;
      height = (hv&3)+1;

      width  = (hv>>2)+1;
      code2 = sprite[1];
      sx = (code2>>16)&0x1ff;
      sx -= 0x78; // Get X coordinate + 8

      if (sy < max_lines && sy + (height<<3) > Scanline) // sprite onscreen (y)?
      {
        int entry, y, sx_min, onscr_x, maybe_op = 0;

        sx_min = 8-(width<<3);
        onscr_x = sx_min < sx && sx < max_width;
        if (sh && (code2 & 0x6000) == 0x6000)
          maybe_op = SPRL_MAY_HAVE_OP;

        entry = ((pd - HighPreSpr) / 2) | ((code2>>8)&0x80);
        y = (sy >= Scanline) ? sy : Scanline;
        for (; y < sy + (height<<3) && y < max_lines; y++)
        {
		  unsigned char *p = &HighLnSpr[y][0];
          int cnt = p[0];
          if (cnt >= max_line_sprites) continue;              // sprite limit?

          if (p[2] >= max_line_sprites*2) {        // tile limit?
            p[0] |= 0x80;
            continue;
          }
          p[2] += width;

		  if (sx == -0x78) {
			  if (sy == -1 && is_ribbit) continue; // ribbit: bad sprite:sprite masking on the beach level

			  //bprintf(0, _T("masked @  %d,%d (x,y)\twidth,height  %x,%x\tcnt/pri %x %x\tScanline  %d\ty  %d\n"), sx, sy, width,height, cnt,(entry & 0x80), Scanline, y);

			  if (cnt > 0)
				  p[0] |= 0x80; // masked, no more sprites for this line
			  continue;
          }
          // must keep the first sprite even if it's offscreen, for masking
          if (cnt > 0 && !onscr_x) continue; // offscreen x

          p[3+cnt] = entry;
          p[0] = cnt + 1;
          p[1] |= (entry & 0x80) ? SPRL_HAVE_HI : SPRL_HAVE_LO;
          p[1] |= maybe_op; // there might be op sprites on this line
          if (cnt > 0 && (code2 & 0x8000) && !(p[3+cnt-1]&0x80))
            p[1] |= SPRL_LO_ABOVE_HI;
        }
      }

      *pd++ = (width<<28)|(height<<24)|(hv<<16)|((unsigned short)sy);
      *pd++ = (sx<<16)|((unsigned short)code2);

      // Find next sprite
      link=(code>>16)&0x7f;
      if (!link) break; // End of sprites
    }
    *pd = 0;

#if 0
    for (u = 0; u < max_lines; u++)
    {
      int y;
      printf("c%03i: %2i, %2i: ", u, HighLnSpr[u][0] & 0x7f, HighLnSpr[u][2]);
      for (y = 0; y < HighLnSpr[u][0] & 0x7f; y++)
        printf(" %i", HighLnSpr[u][y+3]);
      printf("\n");
    }
#endif
  }
}

static void DrawAllSprites(unsigned char *sprited, int prio, int sh)
{
  unsigned char *p;
  int cnt;

  cnt = sprited[0] & 0x7f;
  if (cnt == 0) return;

  p = &sprited[3];

  // Go through sprites backwards:
  for (cnt--; cnt >= 0; cnt--)
  {
    int offs;
    if ((p[cnt] >> 7) != prio) continue;
    offs = (p[cnt]&0x7f) * 2;
    DrawSprite(HighPreSpr + offs, sh);
  }
}


// --------------------------------------------

static void BackFill(INT32 reg7, INT32 sh)
{
	// Start with a blank scanline (background colour):
	UINT16 *pd = (UINT16 *)(HighCol+8);
	UINT16 *end= (UINT16 *)(HighCol+8+320);
	UINT16 back = (reg7 & 0x3f) | (sh<<6);
	back |= back<<8;
	do { pd[0]=pd[1]=back; pd+=2; } while (pd < end);
}


static int DrawDisplay(int sh)
{
  unsigned char *sprited = &HighLnSpr[Scanline][0];
  int win=0, edge=0, hvwind=0, lflags;
  int maxw, maxcells;

  if (RamVReg->rendstatus & (PDRAW_SPRITES_MOVED|PDRAW_DIRTY_SPRITES)) {
    // elprintf(EL_STATUS, "PrepareSprites(%i)", (est->rendstatus>>4)&1);
    PrepareSprites(RamVReg->rendstatus & PDRAW_DIRTY_SPRITES);
    RamVReg->rendstatus &= ~(PDRAW_SPRITES_MOVED|PDRAW_DIRTY_SPRITES);
  }

  RamVReg->rendstatus &= ~(PDRAW_SHHI_DONE|PDRAW_PLANE_HI_PRIO);

  if (RamVReg->reg[12]&1) {
    maxw = 328; maxcells = 40;
  } else {
    maxw = 264; maxcells = 32;
  }

  // Find out if the window is on this line:
  win=RamVReg->reg[0x12];
  edge=(win&0x1f)<<3;

  if (win&0x80) { if (Scanline>=edge) hvwind=1; }
  else          { if (Scanline< edge) hvwind=1; }

  if (!hvwind) // we might have a vertical window here
  {
    win=RamVReg->reg[0x11];
    edge=win&0x1f;
    if (win&0x80) {
      if (!edge) hvwind=1;
      else if(edge < (maxcells>>1)) hvwind=2;
    } else {
      if (!edge);
      else if(edge < (maxcells>>1)) hvwind=2;
      else hvwind=1;
    }
  }

//  if (hvwind) bprintf(0, _T("we have window! %x\n"), hvwind);

  /* - layer B low - */
  if (!(RamVReg->debug_p & PVD_KILL_B)) {
    lflags = LF_PLANE_1 | (sh << 1);
    if (RamVReg->debug_p & PVD_FORCE_B)
      lflags |= LF_FORCE;
    DrawLayer(lflags, HighCacheB, 0, maxcells);
  }
  /* - layer A low - */
  lflags = 0 | (sh << 1);
  if (RamVReg->debug_p & PVD_FORCE_A)
    lflags |= LF_FORCE;
  if (RamVReg->debug_p & PVD_KILL_A)
    ;
  else if (hvwind == 1)
    DrawWindow(0, maxcells>>1, 0, sh);
  else if (hvwind == 2) {
    DrawLayer(lflags, HighCacheA, (win&0x80) ?    0 : edge<<1, (win&0x80) ?     edge<<1 : maxcells);
    DrawWindow(                   (win&0x80) ? edge :       0, (win&0x80) ? maxcells>>1 : edge, 0, sh);
  }
  else
    DrawLayer(lflags, HighCacheA, 0, maxcells);
  /* - sprites low - */
  if (RamVReg->debug_p & PVD_KILL_S_LO)
    ;
  else if (RamVReg->rendstatus & PDRAW_INTERLACE)
    DrawAllSpritesInterlace(0, sh);
  else if (sprited[1] & SPRL_HAVE_LO)
    DrawAllSprites(sprited, 0, sh);

  /* - layer B hi - */
  if (!(RamVReg->debug_p & PVD_KILL_B) && HighCacheB[0])
    DrawTilesFromCache(HighCacheB, sh, maxw);
  /* - layer A hi - */
  if (RamVReg->debug_p & PVD_KILL_A)
    ;
  else if (hvwind == 1)
    DrawWindow(0, maxcells>>1, 1, sh);
  else if (hvwind == 2) {
    if (HighCacheA[0])
      DrawTilesFromCache(HighCacheA, sh, (win&0x80) ? edge<<4 : maxw);
    DrawWindow((win&0x80) ? edge : 0, (win&0x80) ? maxcells>>1 : edge, 1, sh);
  } else
    if (HighCacheA[0])
      DrawTilesFromCache(HighCacheA, sh, maxw);
  /* - sprites hi - */
  if (RamVReg->debug_p & PVD_KILL_S_HI)
    ;
  else if (RamVReg->rendstatus & PDRAW_INTERLACE)
    DrawAllSpritesInterlace(1, sh);
  // have sprites without layer pri bit ontop of sprites with that bit
  else if ((sprited[1] & 0xd0) == 0xd0 && 1)// (PicoIn.opt & POPT_ACC_SPRITES))
    DrawSpritesHiAS(sprited, sh);
  else if (sh && (sprited[1] & SPRL_MAY_HAVE_OP))
    DrawSpritesSHi(sprited);
  else if (sprited[1] & SPRL_HAVE_HI)
    DrawAllSprites(sprited, 1, 0);

  if (RamVReg->debug_p & PVD_FORCE_B)
    DrawTilesFromCacheForced(HighCacheB);
  else if (RamVReg->debug_p & PVD_FORCE_A)
    DrawTilesFromCacheForced(HighCacheA);

#if 0
  {
    int *c, a, b;
    for (a = 0, c = HighCacheA; *c; c++, a++);
    for (b = 0, c = HighCacheB; *c; c++, b++);
    printf("%i:%03i: a=%i, b=%i\n", Pico.m.frame_count,
           Scanline, a, b);
  }
#endif

  return 0;
}

static void SetHighCol(INT32 line)
{
	INT32 offset = (~RamVReg->reg[1] & 8) ? 8 : 0;

	HighCol = HighColFull + ( (offset + line) * (8 + 320 + 8) );
}

static void PicoFrameStart()
{
	// prepare to do this frame
	RamVReg->status &= ~0x0020;                     // mask collision bit

	INT32 offs = 8, lines = 224;

	// prepare to do this frame
	RamVReg->rendstatus = 0;
	if ((RamVReg->reg[12] & 6) == 6)
		RamVReg->rendstatus |= PDRAW_INTERLACE; // interlace mode
	if (!(RamVReg->reg[12] & 1))
		RamVReg->rendstatus |= PDRAW_32_COLS;
	if (RamVReg->reg[1] & 8) {
		offs = 0;
		lines = 240;
	}

	Scanline = 0;
	BlankedLine = 0;

	interlacemode2 = ((RamVReg->reg[12] & (4|2)) == (4|2));

	SetHighCol(0); // start rendering here

	PrepareSprites(1);

	RamVReg->status &= ~0x88; // clear V-Int, come out of vblank
	RamVReg->v_counter = 0;
}

static void fix_palette(UINT8 *dest8, INT32 DestXY, UINT16 *psrc)
{
	UINT16 *dest16 = (UINT16*)dest8 + DestXY;
	UINT32 *dest32 = (UINT32*)dest8 + DestXY;
	dest8 += DestXY;

	for (INT32 x = 0; x < nScreenWidth; x++)
	{
		UINT16 src = psrc[x];
		UINT16 *pPalLUT = (src & 0x8000) ? SegaC2SpPalLookup : SegaC2BgPalLookup;
		UINT32 destrgb = 0;

		switch (src & 0xc0)
		{
			case 0x00:
				destrgb = DrvPalette[(src & 0x0f) | pPalLUT[(src & 0x30) >> 4] | 0x0000];
				break;

			case 0x40:
			case 0xc0:
				destrgb = DrvPalette[(src & 0x0f) | pPalLUT[(src & 0x30) >> 4] | 0x0800];
				break;

			case 0x80:
				destrgb = DrvPalette[(src & 0x0f) | pPalLUT[(src & 0x30) >> 4] | 0x1000];
				break;
		}

		switch (nBurnBpp) {
			case 4: dest32[x] = destrgb; break;
			case 3: dest8[x*3 + 0] = destrgb; dest8[x*3 + 1] = destrgb >> 8; dest8[x*3 + 2] = destrgb >> 16; break;
			case 2: dest16[x] = destrgb; break;
		}
	}
}

static INT32 PicoLine(INT32 y)
{
	INT32 lines_vis = 224;
	if (RamVReg->reg[1]&8) lines_vis = 240;
	INT32 vcnt_wrap = 0;

	irq6_line = lines_vis;

	// handle fifo and vcounter zoop
	RamVReg->v_counter = y;
	if (y < lines_vis) {
		// VDP FIFO
		RamVReg->lwrite_cnt -= 12;
		if (RamVReg->lwrite_cnt <= 0) {
			RamVReg->lwrite_cnt=0;
			RamVReg->status|=0x200;
		}
		if ((RamVReg->reg[12]&6) == 6) { // interlace mode 2
			RamVReg->v_counter <<= 1;
			RamVReg->v_counter |= RamVReg->v_counter >> 8;
			RamVReg->v_counter &= 0xff;
		}
	} else if (y == lines_vis) {
		vcnt_wrap = (Hardware & 0x40) ? 0x103 : 0xEB; // based on Gens, TODO: verify
		if ((RamVReg->reg[12]&6) == 6) RamVReg->v_counter = 0xc1;
		// VDP FIFO
		RamVReg->lwrite_cnt = 0;
		RamVReg->status |= 0x200;

		return 0; // ---- don't draw ----
	} else if (y > lines_vis) {
		if (y >= vcnt_wrap)
			RamVReg->v_counter -= (Hardware & 0x40) ? 313 : 262;//(Hardware & 0x40) ? 56 : 6;
		if ((RamVReg->reg[12]&6) == 6)
			RamVReg->v_counter = (RamVReg->v_counter << 1) | 1;
		RamVReg->v_counter &= 0xff;

		return 0; // ---- don't draw ----
	}

	INT32 sh = (RamVReg->reg[0xC] & 8)>>3; // shadow/hilight?

	BackFill(RamVReg->reg[7], sh);

	INT32 offset = (~RamVReg->reg[1] & 8) ? 8 : 0;

	if (BlankedLine && Scanline > 0 && !interlacemode2)  // blank last line stuff
	{
		{ // copy blanked line to previous line
//			UINT16 *pDest = LineBuf + ((Scanline-1) * 320) + ((interlacemode2 & RamVReg->field) * 240 * 320);
//			UINT16 *pSrc = HighColFull + (Scanline + offset + ((interlacemode2 & RamVReg->field) * 240))*(8+320+8) + 8;

//			memcpy(pDest, pSrc, 320 * sizeof(UINT16));
		}
	}
	BlankedLine = 0;

	if (RamVReg->reg[1] & 0x40)
		DrawDisplay(sh);

	{
		SetHighCol(Scanline + 1); // Set-up pointer to next line to be rendered to (see: PicoFrameStart();)

		{ // copy current line to linebuf, for mid-screen palette changes (referred to as SONIC rendering mode, for water & etc.)
			UINT32 DestXY = (Scanline * nScreenWidth) + ((interlacemode2 & RamVReg->field) * 240 * 320);
			UINT16 *pSrc = HighColFull + (Scanline + offset + ((interlacemode2 & RamVReg->field) * 240))*(8+320+8) + 8;

			if (pBurnDraw) fix_palette(pBurnDraw, DestXY, pSrc);
		}
	}

	return 0;
}

static void DrvDrawBegin() // run in-frame
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_update(i);
		}
		DrvPalette[0x3000] = 0; // black
		DrvRecalc = 0;
	}
}

static void DrvDrawEnd()
{
	if (!enable_display) {
		BurnTransferClear(0x3000);
		BurnTransferCopy(DrvPalette);
	}
}

static INT32 DrvDraw() // called when paused for redraw or mode change
{
	DrvDrawBegin();

	INT32 offset = (~RamVReg->reg[1] & 8) ? 8 : 0;

	for (INT32 i = 0; i < nScreenHeight; i++)
	{
		UINT32 DestXY = (i * nScreenWidth) + ((interlacemode2 & RamVReg->field) * 240 * 320);
		UINT16 *pSrc = HighColFull + (i + offset + ((interlacemode2 & RamVReg->field) * 240))*(8+320+8) + 8;

		if (pBurnDraw) fix_palette(pBurnDraw, DestXY, pSrc);
	}

	DrvDrawEnd();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	NullNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvInputs[5] = DrvDips[0];
		DrvInputs[6] = DrvDips[1];

		if (is_wwmarine) {
			DrvInputs[0] |= (nCurrentFrame & 1) * 0xc0; // l/r pulsed by digital steering wheel
		}

		if (has_dial) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0], Analog[1], 1, 0x1f);
			BurnTrackballUpdate(0);
			DrvInputs[0] = BurnTrackballRead(0, 0);
			DrvInputs[1] = BurnTrackballRead(0, 1);
		}
	}

	if (pBurnDraw) DrvDrawBegin(); // re-calc palette if necessary

	PicoFrameStart();

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(53693175 / 6 / 59.922745), (INT32)(53693175 / 6 / 59.922745) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	SekIdle(nExtraCycles[0]);

	irq4_counter = RamVReg->reg[0x0a];

	for (INT32 i = 0; i < nInterleave; i++)
	{
		Scanline = i;
		line_base_cycles = SekTotalCycles();

		SekIdle(DMABURN());

		CPU_RUN_SYNCINT(0, Sek);
		CPU_RUN_TIMER(1); // nullcpu for fm timer (we can't have the fm timer breaking Sek execution)

		PicoLine(Scanline); // draw & blit line

		if (Scanline <= 224) {
			irq4_counter--;
			if (irq4_counter < 0) {
				irq4_counter = RamVReg->reg[0x0a];
				RamVReg->pending_ints |= 0x10;
				if (RamVReg->reg[0x00] & 0x10) {
					SekSetVIRQLine(4, CPU_IRQSTATUS_ACK);
				}
			}
		}
		if (Scanline == irq6_line) {
			RamVReg->status |= 0x08; // V-Int
			RamVReg->pending_ints |= 0x20;
			SekRun(68); // this is called "v-int lag" on megadrive
			SekSetVIRQLine(6, CPU_IRQSTATUS_ACK);  // irq hooked to z80_irq (vdp), not the usual vdp vblank line!

			if (pBurnDraw) DrvDrawEnd();
		}
	}

	if (pBurnSoundOut) {
		BurnYM3438Update(pBurnSoundOut, nBurnSoundLen);
		if (sound_rom_length) {
			UPD7759Render(pBurnSoundOut, nBurnSoundLen);
		}
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = SekTotalCycles() - nCyclesTotal[0];

	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029709;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		BurnYM3438Scan(nAction, pnMin);
		if (sound_rom_length) UPD7759Scan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		if (has_dial) BurnTrackballScan();

		SCAN_VAR(prot_write_buf);
		SCAN_VAR(prot_read_buf);
		SCAN_VAR(enable_display);
		SCAN_VAR(alt_palette_mode);
		SCAN_VAR(palette_bank);
		SCAN_VAR(bg_palbase);
		SCAN_VAR(sp_palbase);
		SCAN_VAR(output_latch);
		SCAN_VAR(dir);
		SCAN_VAR(iocnt);
		SCAN_VAR(sound_bank);
		SCAN_VAR(irq6_line);
		SCAN_VAR(irq4_counter);
		SCAN_VAR(SegaC2BgPalLookup);
		SCAN_VAR(SegaC2SpPalLookup);

		SCAN_VAR(Hardware);
		SCAN_VAR(dma_xfers);
		SCAN_VAR(BlankedLine);
		SCAN_VAR(interlacemode2);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		recompute_palette_tables();

		if (sound_rom_length) {
			memcpy (DrvSndROM + 0x80000, DrvSndROM + (sound_bank * 0x20000), 0x20000);
		}
	}

	return 0;
}


// Bloxeed (World, C System)

static struct BurnRomInfo bloxeedcRomDesc[] = {
	{ "epr-12908.ic32",			0x20000, 0xfc77cb91, 3 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12907.ic31",			0x20000, 0xe5fcbac6, 3 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-12993.ic34",			0x20000, 0x487bc8fc, 3 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-12992.ic33",			0x20000, 0x19b0084c, 3 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(bloxeedc)
STD_ROM_FN(bloxeedc)

static UINT8 no_protection_callback(UINT8 in)
{
	return 0;
}

static INT32 NoProtectionInit()
{
	return SegaC2Init(no_protection_callback);
}

struct BurnDriver BurnDrvBloxeedc = {
	"bloxeedc", "bloxeed", NULL, NULL, "1989",
	"Bloxeed (World, C System)\0", NULL, "Sega / Elorg", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, bloxeedcRomInfo, bloxeedcRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, BloxeedcDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Bloxeed (US, C System, Rev A)

static struct BurnRomInfo bloxeeduRomDesc[] = {
	{ "epr-12997a.ic32",		0x20000, 0x23655bc9, 3 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12996a.ic31",		0x20000, 0x83c83f0c, 3 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-12993.ic34",			0x20000, 0x487bc8fc, 3 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-12992.ic33",			0x20000, 0x19b0084c, 3 | BRF_PRG | BRF_ESS }, //  3

	{ "315-5393.ic24",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  4 pals
	{ "315-5394.ic25",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  5
	{ "315-5395.ic26",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  6
	{ "317-0140.ic27",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7
};

STD_ROM_PICK(bloxeedu)
STD_ROM_FN(bloxeedu)

struct BurnDriver BurnDrvBloxeedu = {
	"bloxeedu", "bloxeed", NULL, NULL, "1989",
	"Bloxeed (US, C System, Rev A)\0", NULL, "Sega / Elorg", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, bloxeeduRomInfo, bloxeeduRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, BloxeeduDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Columns (World)

static struct BurnRomInfo columnsRomDesc[] = {
	{ "epr-13114.ic32",			0x20000, 0xff78f740, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13113.ic31",			0x20000, 0x9a426d9b, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(columns)
STD_ROM_FN(columns)

static UINT8 columns_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x02, 0x00, 0x0a, 0x04, 0x01, 0x03, 0x09, 0x07, 0x06, 0x04, 0x0e, 0x00, 0x05, 0x07, 0x0d, 0x03,
		0x02, 0x00, 0x0a, 0x04, 0x01, 0x03, 0x09, 0x07, 0x06, 0x04, 0x0e, 0x00, 0x05, 0x07, 0x0d, 0x03, 
		0x02, 0x00, 0x0a, 0x04, 0x01, 0x03, 0x09, 0x07, 0x06, 0x04, 0x0e, 0x00, 0x05, 0x07, 0x0d, 0x03, 
		0x0a, 0x08, 0x02, 0x04, 0x09, 0x0b, 0x01, 0x07, 0x0e, 0x0c, 0x06, 0x00, 0x0d, 0x0f, 0x05, 0x03, 
		0x02, 0x00, 0x0a, 0x04, 0x01, 0x03, 0x09, 0x07, 0x06, 0x04, 0x0e, 0x00, 0x05, 0x07, 0x0d, 0x03, 
		0x07, 0x05, 0x0f, 0x05, 0x04, 0x06, 0x0c, 0x06, 0x03, 0x01, 0x0b, 0x01, 0x00, 0x02, 0x08, 0x02, 
		0x02, 0x00, 0x0a, 0x04, 0x01, 0x03, 0x09, 0x07, 0x06, 0x04, 0x0e, 0x00, 0x05, 0x07, 0x0d, 0x03, 
		0x0f, 0x0d, 0x07, 0x05, 0x0c, 0x0e, 0x04, 0x06, 0x0b, 0x09, 0x03, 0x01, 0x08, 0x0a, 0x00, 0x02, 
		0x0b, 0x08, 0x03, 0x04, 0x08, 0x0b, 0x00, 0x07, 0x0f, 0x0c, 0x07, 0x00, 0x0c, 0x0f, 0x04, 0x03, 
		0x0b, 0x08, 0x03, 0x04, 0x08, 0x0b, 0x00, 0x07, 0x0f, 0x0c, 0x07, 0x00, 0x0c, 0x0f, 0x04, 0x03, 
		0x09, 0x0a, 0x01, 0x06, 0x08, 0x0b, 0x00, 0x07, 0x0d, 0x0e, 0x05, 0x02, 0x0c, 0x0f, 0x04, 0x03, 
		0x09, 0x0a, 0x01, 0x06, 0x08, 0x0b, 0x00, 0x07, 0x0d, 0x0e, 0x05, 0x02, 0x0c, 0x0f, 0x04, 0x03, 
		0x03, 0x00, 0x0b, 0x04, 0x00, 0x03, 0x08, 0x07, 0x07, 0x04, 0x0f, 0x00, 0x04, 0x07, 0x0c, 0x03, 
		0x07, 0x05, 0x0f, 0x05, 0x04, 0x06, 0x0c, 0x06, 0x03, 0x01, 0x0b, 0x01, 0x00, 0x02, 0x08, 0x02, 
		0x03, 0x00, 0x0b, 0x04, 0x00, 0x03, 0x08, 0x07, 0x07, 0x04, 0x0f, 0x00, 0x04, 0x07, 0x0c, 0x03, 
		0x0f, 0x0d, 0x07, 0x05, 0x0c, 0x0e, 0x04, 0x06, 0x0b, 0x09, 0x03, 0x01, 0x08, 0x0a, 0x00, 0x02
	};

	return prot_lut_table[in];
}

static INT32 ColumnsInit()
{
	return SegaC2Init(columns_protection_callback);
}

struct BurnDriver BurnDrvColumns = {
	"columns", NULL, NULL, NULL, "1990",
	"Columns (World)\0", NULL, "Sega", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, columnsRomInfo, columnsRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, ColumnsDIPInfo,
	ColumnsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Columns (US, cocktail, Rev A)

static struct BurnRomInfo columnsuRomDesc[] = {
	{ "epr-13116a.ic32",		0x20000, 0xa0284b16, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13115a.ic31",		0x20000, 0xe37496f3, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(columnsu)
STD_ROM_FN(columnsu)

struct BurnDriver BurnDrvColumnsu = {
	"columnsu", "columns", NULL, NULL, "1990",
	"Columns (US, cocktail, Rev A)\0", NULL, "Sega", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, columnsuRomInfo, columnsuRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, ColumnsDIPInfo,
	ColumnsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Columns (Japan)

static struct BurnRomInfo columnsjRomDesc[] = {
	{ "epr-13112.ic32",			0x20000, 0xbae6e53e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13111.ic31",			0x20000, 0xaa5ccd6d, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(columnsj)
STD_ROM_FN(columnsj)

struct BurnDriver BurnDrvColumnsj = {
	"columnsj", "columns", NULL, NULL, "1990",
	"Columns (Japan)\0", NULL, "Sega", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, columnsjRomInfo, columnsjRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, ColumnsDIPInfo,
	ColumnsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Columns II: The Voyage Through Time (World)

static struct BurnRomInfo columns2RomDesc[] = {
	{ "epr-13363.ic32",			0x20000, 0xc99e4ffd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13362.ic31",			0x20000, 0x394e2419, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(columns2)
STD_ROM_FN(columns2)

static UINT8 columns2_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x00, 0x0c, 
		0x08, 0x08, 0x09, 0x0d, 0x09, 0x09, 0x08, 0x04, 0x0c, 0x0e, 0x0d, 0x0b, 0x09, 0x0b, 0x08, 0x06, 
		0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x04, 0x0c, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x04, 0x0c, 
		0x0d, 0x0d, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x04, 0x09, 0x0b, 0x09, 0x0b, 0x0c, 0x0e, 0x0c, 0x06, 
		0x02, 0x02, 0x03, 0x07, 0x03, 0x03, 0x02, 0x0e, 0x02, 0x02, 0x03, 0x07, 0x03, 0x03, 0x02, 0x0e, 
		0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x06, 0x0e, 0x02, 0x00, 0x03, 0x01, 0x07, 0x05, 0x06, 0x0c, 
		0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x06, 0x0e, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x06, 0x0e, 
		0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x0e, 0x03, 0x01, 0x03, 0x01, 0x06, 0x04, 0x06, 0x0c, 
		0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x00, 0x0c, 
		0x08, 0x08, 0x09, 0x0d, 0x09, 0x09, 0x08, 0x04, 0x0c, 0x0e, 0x0d, 0x0b, 0x09, 0x0b, 0x08, 0x06, 
		0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x06, 0x0e, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x06, 0x0e, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x06, 0x0b, 0x09, 0x0b, 0x09, 0x0e, 0x0c, 0x0e, 0x04, 
		0x0a, 0x0a, 0x0b, 0x0f, 0x0b, 0x0b, 0x0a, 0x06, 0x0a, 0x0a, 0x0b, 0x0f, 0x0b, 0x0b, 0x0a, 0x06, 
		0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x06, 0x0a, 0x08, 0x0b, 0x09, 0x0f, 0x0d, 0x0e, 0x04, 
		0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x06, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x06, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x06, 0x0b, 0x09, 0x0b, 0x09, 0x0e, 0x0c, 0x0e, 0x04
	};

	return prot_lut_table[in];
}

static INT32 Columns2Init()
{
	return SegaC2Init(columns2_protection_callback);
}

struct BurnDriver BurnDrvColumns2 = {
	"columns2", NULL, NULL, NULL, "1990",
	"Columns II: The Voyage Through Time (World)\0", NULL, "Sega", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, columns2RomInfo, columns2RomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, Columns2DIPInfo,
	Columns2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Columns II: The Voyage Through Time (Japan)

static struct BurnRomInfo column2jRomDesc[] = {
	{ "epr-13361.ic32",			0x20000, 0xb54b5f12, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13360.ic31",			0x20000, 0xa59b1d4f, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(column2j)
STD_ROM_FN(column2j)

struct BurnDriver BurnDrvColumn2j = {
	"column2j", "columns2", NULL, NULL, "1990",
	"Columns II: The Voyage Through Time (Japan)\0", NULL, "Sega", "C",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, column2jRomInfo, column2jRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, Columns2DIPInfo,
	Columns2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Thunder Force AC

static struct BurnRomInfo tfrceacRomDesc[] = {
	{ "epr-13675.ic32",			0x40000, 0x95ecf202, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13674.ic31",			0x40000, 0xe63d7f1a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13659.ic34",			0x40000, 0x29f23461, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-13658.ic33",			0x40000, 0x9e23734f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13655.ic4",			0x40000, 0xe09961f6, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(tfrceac)
STD_ROM_FN(tfrceac)

static UINT8 tfrceac_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x03, 0x0a, 0x03, 0x0a, 0x06, 0x0f, 0x06, 0x0f, 0x03, 0x08, 0x03, 0x08, 0x06, 0x0d, 0x06, 0x0d,
		0x03, 0x0a, 0x03, 0x0a, 0x06, 0x0f, 0x06, 0x0f, 0x02, 0x08, 0x02, 0x08, 0x07, 0x0d, 0x07, 0x0d,
		0x03, 0x0a, 0x03, 0x0a, 0x06, 0x0f, 0x06, 0x0f, 0x03, 0x08, 0x03, 0x08, 0x06, 0x0d, 0x06, 0x0d,
		0x03, 0x0a, 0x03, 0x0a, 0x06, 0x0f, 0x06, 0x0f, 0x02, 0x08, 0x02, 0x08, 0x07, 0x0d, 0x07, 0x0d,
		0x07, 0x0e, 0x03, 0x0a, 0x02, 0x0b, 0x06, 0x0f, 0x07, 0x0c, 0x03, 0x08, 0x02, 0x09, 0x06, 0x0d,
		0x07, 0x0e, 0x0b, 0x02, 0x02, 0x0b, 0x0e, 0x07, 0x06, 0x0c, 0x0a, 0x00, 0x03, 0x09, 0x0f, 0x05,
		0x07, 0x0e, 0x03, 0x0a, 0x02, 0x0b, 0x06, 0x0f, 0x07, 0x0c, 0x03, 0x08, 0x02, 0x09, 0x06, 0x0d,
		0x07, 0x0e, 0x0b, 0x02, 0x02, 0x0b, 0x0e, 0x07, 0x06, 0x0c, 0x0a, 0x00, 0x03, 0x09, 0x0f, 0x05,
		0x03, 0x0b, 0x03, 0x0b, 0x06, 0x0e, 0x06, 0x0e, 0x03, 0x09, 0x03, 0x09, 0x06, 0x0c, 0x06, 0x0c,
		0x05, 0x0d, 0x0d, 0x05, 0x00, 0x08, 0x08, 0x00, 0x04, 0x0e, 0x0c, 0x06, 0x01, 0x0b, 0x09, 0x03,
		0x03, 0x0b, 0x03, 0x0b, 0x06, 0x0e, 0x06, 0x0e, 0x03, 0x09, 0x03, 0x09, 0x06, 0x0c, 0x06, 0x0c,
		0x03, 0x0b, 0x0b, 0x03, 0x06, 0x0e, 0x0e, 0x06, 0x02, 0x08, 0x0a, 0x00, 0x07, 0x0d, 0x0f, 0x05,
		0x05, 0x0d, 0x01, 0x09, 0x00, 0x08, 0x04, 0x0c, 0x05, 0x0d, 0x01, 0x09, 0x00, 0x08, 0x04, 0x0c,
		0x07, 0x0f, 0x0f, 0x07, 0x02, 0x0a, 0x0a, 0x02, 0x06, 0x0e, 0x0e, 0x06, 0x03, 0x0b, 0x0b, 0x03,
		0x05, 0x0d, 0x01, 0x09, 0x00, 0x08, 0x04, 0x0c, 0x05, 0x0d, 0x01, 0x09, 0x00, 0x08, 0x04, 0x0c,
		0x05, 0x0d, 0x09, 0x01, 0x00, 0x08, 0x0c, 0x04, 0x04, 0x0c, 0x08, 0x00, 0x01, 0x09, 0x0d, 0x05,
	};

	return prot_lut_table[in];
}

static INT32 TfrceacInit()
{
	return SegaC2Init(tfrceac_protection_callback);
}

struct BurnDriver BurnDrvTfrceac = {
	"tfrceac", NULL, NULL, NULL, "1990",
	"Thunder Force AC\0", NULL, "Technosoft / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, tfrceacRomInfo, tfrceacRomName, NULL, NULL, NULL, NULL, SegaC2_3ButtonInputInfo, TfrceacDIPInfo,
	TfrceacInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Thunder Force AC (Japan)

static struct BurnRomInfo tfrceacjRomDesc[] = {
	{ "epr-13657.ic32",			0x40000, 0xa0f38ffd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13656.ic31",			0x40000, 0xb9438d1e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13659.ic34",			0x40000, 0x29f23461, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-13658.ic33",			0x40000, 0x9e23734f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13655.ic4",			0x40000, 0xe09961f6, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(tfrceacj)
STD_ROM_FN(tfrceacj)

struct BurnDriver BurnDrvTfrceacj = {
	"tfrceacj", "tfrceac", NULL, NULL, "1990",
	"Thunder Force AC (Japan)\0", NULL, "Technosoft / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, tfrceacjRomInfo, tfrceacjRomName, NULL, NULL, NULL, NULL, SegaC2_3ButtonInputInfo, TfrceacDIPInfo,
	TfrceacInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Thunder Force AC (Japan, prototype, bootleg)

static struct BurnRomInfo tfrceacjpbRomDesc[] = {
	{ "ic32_t.f.ac_075f.ic32",	0x40000, 0x2167dd93, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic31_t.f.ac_0d26.id31",	0x40000, 0xebf02bba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic34_t.f.ac_549d.ic34",	0x40000, 0x902ad2ec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic33_t.f.ac_d131.ic33",	0x40000, 0xb162219d, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(tfrceacjpb)
STD_ROM_FN(tfrceacjpb)

static INT32 TfrceacjpbInit()
{
	INT32 rc = SegaC2Init(tfrceac_protection_callback);
	dir_override = 0xf; // issue w/io chip

	return rc;
}

struct BurnDriver BurnDrvTfrceacjpb = {
	"tfrceacjpb", "tfrceac", NULL, NULL, "1990",
	"Thunder Force AC (Japan, prototype, bootleg)\0", NULL, "Technosoft / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, tfrceacjpbRomInfo, tfrceacjpbRomName, NULL, NULL, NULL, NULL, SegaC2_3ButtonInputInfo, TfrceacDIPInfo,
	TfrceacjpbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Thunder Force AC (bootleg)

static struct BurnRomInfo tfrceacbRomDesc[] = {
	{ "4.bin",					0x40000, 0xeba059d3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.bin",					0x40000, 0x3e5dc542, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13659.ic34",			0x40000, 0x29f23461, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-13658.ic33",			0x40000, 0x9e23734f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13655.ic4",			0x40000, 0xe09961f6, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(tfrceacb)
STD_ROM_FN(tfrceacb)

static INT32 TfrceacbInit()
{
	is_tfrceacb = 1;

	return SegaC2Init(no_protection_callback);
}

struct BurnDriver BurnDrvTfrceacb = {
	"tfrceacb", "tfrceac", NULL, NULL, "1990",
	"Thunder Force AC (bootleg)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 1, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, tfrceacbRomInfo, tfrceacbRomName, NULL, NULL, NULL, NULL, SegaC2_3ButtonInputInfo, TfrceacDIPInfo,
	TfrceacbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Borench (set 1)

static struct BurnRomInfo borenchRomDesc[] = {
	{ "ic32.bin",				0x40000, 0x2c54457d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic31.bin",				0x40000, 0xb46445fc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-13587.ic4",			0x20000, 0x62b85e56, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(borench)
STD_ROM_FN(borench)

static UINT8 borench_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x01, 0x02, 0x0f, 0x0e, 0x05, 0x06, 0x0b, 0x0a, 0x05, 0x06, 0x0b, 0x0a, 0x05, 0x06, 0x0b, 0x0a, 
		0x00, 0x00, 0x0a, 0x0a, 0x04, 0x04, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 
		0x01, 0x03, 0x0f, 0x0f, 0x05, 0x07, 0x0b, 0x0b, 0x07, 0x05, 0x09, 0x09, 0x05, 0x07, 0x0b, 0x0b, 
		0x01, 0x01, 0x0b, 0x0b, 0x05, 0x05, 0x0f, 0x0f, 0x0f, 0x0f, 0x0d, 0x0d, 0x0d, 0x0d, 0x0f, 0x0f, 
		0x01, 0x02, 0x0b, 0x0a, 0x05, 0x06, 0x0f, 0x0e, 0x05, 0x06, 0x0f, 0x0e, 0x05, 0x06, 0x0f, 0x0e, 
		0x00, 0x00, 0x0a, 0x0a, 0x04, 0x04, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 
		0x09, 0x03, 0x03, 0x0b, 0x0d, 0x07, 0x07, 0x0f, 0x0f, 0x05, 0x05, 0x0d, 0x0d, 0x07, 0x07, 0x0f, 
		0x09, 0x01, 0x03, 0x0b, 0x0d, 0x05, 0x07, 0x0f, 0x07, 0x0f, 0x05, 0x0d, 0x05, 0x0d, 0x07, 0x0f, 
		0x01, 0x02, 0x0f, 0x0e, 0x05, 0x06, 0x0b, 0x0a, 0x05, 0x06, 0x0b, 0x0a, 0x05, 0x06, 0x0a, 0x0b, 
		0x00, 0x00, 0x0a, 0x0a, 0x04, 0x04, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 0x0c, 0x0c, 0x0f, 0x0f, 
		0x0d, 0x07, 0x03, 0x0b, 0x0d, 0x07, 0x03, 0x0b, 0x0f, 0x05, 0x01, 0x09, 0x0d, 0x07, 0x02, 0x0a, 
		0x0d, 0x05, 0x07, 0x0f, 0x0d, 0x05, 0x07, 0x0f, 0x07, 0x0f, 0x05, 0x0d, 0x05, 0x0d, 0x06, 0x0e, 
		0x01, 0x02, 0x0b, 0x0a, 0x05, 0x06, 0x0f, 0x0e, 0x05, 0x06, 0x0f, 0x0e, 0x05, 0x06, 0x0e, 0x0f, 
		0x00, 0x00, 0x0a, 0x0a, 0x04, 0x04, 0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 0x0c, 0x0c, 0x0f, 0x0f, 
		0x0d, 0x07, 0x07, 0x0f, 0x0d, 0x07, 0x07, 0x0f, 0x0f, 0x05, 0x05, 0x0d, 0x0d, 0x07, 0x06, 0x0e, 
		0x0d, 0x05, 0x07, 0x0f, 0x0d, 0x05, 0x07, 0x0f, 0x07, 0x0f, 0x05, 0x0d, 0x05, 0x0d, 0x06, 0x0e 
	};

	return prot_lut_table[in];
}

static INT32 BorenchInit()
{
	return SegaC2Init(borench_protection_callback);
}

struct BurnDriver BurnDrvBorench = {
	"borench", NULL, NULL, NULL, "1990",
	"Borench (set 1)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, borenchRomInfo, borenchRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, BorenchDIPInfo,
	BorenchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Borench (set 2)

static struct BurnRomInfo borenchaRomDesc[] = {
	{ "epr-13591.ic32",			0x40000, 0x7851078b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13590.ic31",			0x40000, 0x01bc6fe6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-13587.ic4",			0x20000, 0x62b85e56, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(borencha)
STD_ROM_FN(borencha)

struct BurnDriver BurnDrvBorencha = {
	"borencha", "borench", NULL, NULL, "1990",
	"Borench (set 2)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, borenchaRomInfo, borenchaRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, BorenchDIPInfo,
	BorenchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Borench (Japan)

static struct BurnRomInfo borenchjRomDesc[] = {
	{ "epr-13586.ic32",			0x40000, 0x62d7f8e8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13585.ic31",			0x40000, 0x087b9704, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-13587.ic4",			0x20000, 0x62b85e56, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(borenchj)
STD_ROM_FN(borenchj)

struct BurnDriver BurnDrvBorenchj = {
	"borenchj", "borench", NULL, NULL, "1990",
	"Borench (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, borenchjRomInfo, borenchjRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, BorenchDIPInfo,
	BorenchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Ribbit!

static struct BurnRomInfo ribbitRomDesc[] = {
	{ "epr-13833.ic32",			0x40000, 0x5347f8ce, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13832.ic31",			0x40000, 0x889c42c2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13838.ic34",			0x80000, 0xa5d62ac3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-13837.ic33",			0x80000, 0x434de159, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13834.ic4",			0x20000, 0xab0c1833, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ribbit)
STD_ROM_FN(ribbit)
#include "bitswap.h"
static UINT8 ribbit_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x0f, 0x0f, 0x0b, 0x0b, 0x07, 0x07, 0x03, 0x0b, 0x0f, 0x0f, 0x0b, 0x0f, 0x07, 0x07, 0x03, 0x0f,
		0x0f, 0x0e, 0x0b, 0x0a, 0x0f, 0x0e, 0x0b, 0x0a, 0x0f, 0x0e, 0x0b, 0x0e, 0x0f, 0x0e, 0x0b, 0x0e,
		0x0e, 0x0e, 0x08, 0x08, 0x06, 0x06, 0x01, 0x09, 0x0f, 0x0f, 0x09, 0x0d, 0x07, 0x07, 0x01, 0x0d,
		0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x0c, 0x0f, 0x0e, 0x09, 0x0c,
		0x0d, 0x0f, 0x09, 0x0b, 0x05, 0x07, 0x01, 0x0b, 0x05, 0x07, 0x01, 0x07, 0x0f, 0x0f, 0x0b, 0x07,
		0x0d, 0x0e, 0x09, 0x0a, 0x0d, 0x0e, 0x09, 0x0a, 0x05, 0x06, 0x01, 0x06, 0x07, 0x06, 0x03, 0x06,
		0x0c, 0x0e, 0x0a, 0x08, 0x04, 0x06, 0x03, 0x09, 0x05, 0x07, 0x03, 0x05, 0x0f, 0x0f, 0x09, 0x05,
		0x0c, 0x0f, 0x0a, 0x09, 0x0c, 0x0f, 0x0b, 0x08, 0x05, 0x06, 0x03, 0x04, 0x07, 0x06, 0x01, 0x04,
		0x0f, 0x0f, 0x0f, 0x0f, 0x03, 0x03, 0x03, 0x0b, 0x0f, 0x0f, 0x0f, 0x0f, 0x03, 0x03, 0x03, 0x0b,
		0x0f, 0x0e, 0x0f, 0x0e, 0x0b, 0x0a, 0x0b, 0x0a, 0x0f, 0x0e, 0x0f, 0x0e, 0x0b, 0x0a, 0x0b, 0x0a,
		0x0e, 0x0e, 0x0c, 0x0c, 0x02, 0x02, 0x01, 0x09, 0x0f, 0x0f, 0x0d, 0x0d, 0x03, 0x03, 0x01, 0x09,
		0x0e, 0x0f, 0x0c, 0x0d, 0x0a, 0x0b, 0x09, 0x08, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08,
		0x0d, 0x0f, 0x0d, 0x0f, 0x01, 0x03, 0x01, 0x0b, 0x05, 0x07, 0x05, 0x07, 0x0b, 0x0b, 0x0b, 0x03,
		0x0d, 0x0e, 0x0d, 0x0e, 0x09, 0x0a, 0x09, 0x0a, 0x05, 0x06, 0x05, 0x06, 0x03, 0x02, 0x03, 0x02,
		0x0c, 0x0e, 0x0e, 0x0c, 0x00, 0x02, 0x03, 0x09, 0x05, 0x07, 0x07, 0x05, 0x0b, 0x0b, 0x09, 0x01,
		0x0c, 0x0f, 0x0e, 0x0d, 0x08, 0x0b, 0x0b, 0x08, 0x05, 0x06, 0x07, 0x04, 0x03, 0x02, 0x01, 0x00,
	};

	return prot_lut_table[in];
}

static INT32 RibbitInit()
{
	is_ribbit = 1;

	INT32 rc = SegaC2Init(ribbit_protection_callback);

	if (!rc) {
		memmove(Drv68KROM + 0x80000, Drv68KROM + 0x00000, 0x80000);
		UPD7759SetStartDelay(0, 250);
	}

	return rc;
}

struct BurnDriver BurnDrvRibbit = {
	"ribbit", NULL, NULL, NULL, "1991",
	"Ribbit!\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_ACTION, 0,
	NULL, ribbitRomInfo, ribbitRomName, NULL, NULL, NULL, NULL, RibbitInputInfo, RibbitDIPInfo,
	RibbitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Ribbit! (Japan)

static struct BurnRomInfo ribbitjRomDesc[] = {
	{ "epr-13836.ic32",			0x40000, 0x21f222e2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-13835.ic31",			0x40000, 0x1c4b291a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-13838.ic34",			0x80000, 0xa5d62ac3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-13837.ic33",			0x80000, 0x434de159, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13834.ic4",			0x20000, 0xab0c1833, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ribbitj)
STD_ROM_FN(ribbitj)

struct BurnDriver BurnDrvRibbitj = {
	"ribbitj", "ribbit", NULL, NULL, "1991",
	"Ribbit! (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_ACTION, 0,
	NULL, ribbitjRomInfo, ribbitjRomName, NULL, NULL, NULL, NULL, RibbitInputInfo, RibbitjDIPInfo,
	RibbitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Twin Squash

static struct BurnRomInfo twinsquaRomDesc[] = {
	{ "epr-14657.ic32",			0x40000, 0xbecbb1a1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-14656.ic31",			0x40000, 0x411906e7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-14588.ic4",			0x20000, 0x5a9b6881, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(twinsqua)
STD_ROM_FN(twinsqua)

static UINT8 twinsqua_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x0b, 0x0b, 0x03, 0x03, 0x0a, 0x0a, 0x02, 0x02, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e,
		0x0a, 0x08, 0x02, 0x00, 0x0b, 0x0b, 0x03, 0x03, 0x0f, 0x0d, 0x07, 0x05, 0x0e, 0x0e, 0x06, 0x06,
		0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0a, 0x08, 0x02, 0x00, 0x0a, 0x0a, 0x02, 0x02, 0x0b, 0x09, 0x03, 0x01, 0x0b, 0x0b, 0x03, 0x03,
		0x03, 0x03, 0x0b, 0x0b, 0x02, 0x02, 0x0a, 0x0a, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e,
		0x02, 0x02, 0x0a, 0x0a, 0x03, 0x01, 0x0b, 0x09, 0x07, 0x07, 0x0f, 0x0f, 0x06, 0x04, 0x0e, 0x0c,
		0x03, 0x03, 0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x02, 0x02, 0x0a, 0x0a, 0x02, 0x00, 0x0a, 0x08, 0x03, 0x03, 0x0b, 0x0b, 0x03, 0x01, 0x0b, 0x09,
		0x0b, 0x0b, 0x03, 0x03, 0x0a, 0x0a, 0x02, 0x02, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e,
		0x0e, 0x0c, 0x06, 0x04, 0x0f, 0x0f, 0x07, 0x07, 0x0b, 0x09, 0x03, 0x01, 0x0a, 0x0a, 0x02, 0x02,
		0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0e, 0x0c, 0x06, 0x04, 0x0e, 0x0e, 0x06, 0x06, 0x0f, 0x0d, 0x07, 0x05, 0x0f, 0x0f, 0x07, 0x07,
		0x03, 0x03, 0x0b, 0x0b, 0x02, 0x02, 0x0a, 0x0a, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e,
		0x06, 0x06, 0x0e, 0x0e, 0x07, 0x05, 0x0f, 0x0d, 0x03, 0x03, 0x0b, 0x0b, 0x02, 0x00, 0x0a, 0x08,
		0x03, 0x03, 0x0b, 0x0b, 0x03, 0x03, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x06, 0x06, 0x0e, 0x0e, 0x06, 0x04, 0x0e, 0x0c, 0x07, 0x07, 0x0f, 0x0f, 0x07, 0x05, 0x0f, 0x0d
	};

	return prot_lut_table[in];
}

static INT32 TwinsquaInit()
{
	has_dial = 1;

	return SegaC2Init(twinsqua_protection_callback);
}

struct BurnDriver BurnDrvTwinsqua = {
	"twinsqua", NULL, NULL, NULL, "1991",
	"Twin Squash\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_BREAKOUT, 0,
	NULL, twinsquaRomInfo, twinsquaRomName, NULL, NULL, NULL, NULL, TwinsquaInputInfo, TwinsquaDIPInfo,
	TwinsquaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Waku Waku Sonic Patrol Car

static struct BurnRomInfo soniccarRomDesc[] = {
	{ "epr-14369.ic32",			0x40000, 0x2ea4c9a3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-14395.ic31",			0x40000, 0x39622e18, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-14394.ic4",			0x40000, 0x476e30dd, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(soniccar)
STD_ROM_FN(soniccar)

struct BurnDriver BurnDrvSoniccar = {
	"soniccar", NULL, NULL, NULL, "1991",
	"Waku Waku Sonic Patrol Car\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_ACTION, 0,
	NULL, soniccarRomInfo, soniccarRomName, NULL, NULL, NULL, NULL, SoniccarInputInfo, SoniccarDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// SegaSonic Bros. (prototype, hack)

static struct BurnRomInfo ssonicbrRomDesc[] = {
	{ "ssonicbr.ic32",			0x40000, 0xcf254ecd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ssonicbr.ic31",			0x40000, 0x03709746, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ssonicbr.ic4",			0x20000, 0x78e56a51, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(ssonicbr)
STD_ROM_FN(ssonicbr)

struct BurnDriver BurnDrvSsonicbr = {
	"ssonicbr", NULL, NULL, NULL, "1992",
	"SegaSonic Bros. (prototype, hack)\0", NULL, "hack", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HACK, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, ssonicbrRomInfo, ssonicbrRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, SsonicbrDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// OOPArts (prototype, joystick hack)

static struct BurnRomInfo oopartsRomDesc[] = {
	{ "ooparts.ic32",			0x80000, 0x8dcf2940, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ooparts.ic31",			0x80000, 0x35381899, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ooparts.ic34",			0x80000, 0x7192ac29, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ooparts.ic33",			0x80000, 0x42755dc2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-13655.ic4",			0x40000, 0xe09961f6, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ooparts)
STD_ROM_FN(ooparts)

struct BurnDriver BurnDrvOoparts = {
	"ooparts", NULL, NULL, NULL, "1992",
	"OOPArts (prototype, joystick hack)\0", NULL, "hack", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_BREAKOUT, 0,
	NULL, oopartsRomInfo, oopartsRomName, NULL, NULL, NULL, NULL, OopartsInputInfo, OopartsDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	224, 320, 3, 4
};


// Puyo Puyo (World)

static struct BurnRomInfo puyoRomDesc[] = {
	{ "epr-15198.ic32",			0x20000, 0x9610d80c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15197.ic31",			0x20000, 0x7b1f3229, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15200.ic34",			0x20000, 0x0a0692e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15199.ic33",			0x20000, 0x353109b8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15196.ic4",			0x20000, 0x79112b3b, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(puyo)
STD_ROM_FN(puyo)

static UINT8 puyo_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x03, 0x03, 0x0a, 0x0a, 0x05, 0x05, 0x0c, 0x0c, 0x03, 0x03, 0x0a, 0x0a, 0x05, 0x05, 0x0c, 0x0c,
		0x0b, 0x0a, 0x02, 0x02, 0x0f, 0x0e, 0x06, 0x06, 0x0b, 0x0a, 0x02, 0x02, 0x0f, 0x0e, 0x06, 0x06,
		0x07, 0x07, 0x0e, 0x0e, 0x05, 0x05, 0x0c, 0x0c, 0x05, 0x05, 0x0c, 0x0c, 0x07, 0x07, 0x0e, 0x0e,
		0x0f, 0x0e, 0x06, 0x06, 0x0f, 0x0e, 0x06, 0x06, 0x0d, 0x0c, 0x04, 0x04, 0x0d, 0x0c, 0x04, 0x04,
		0x03, 0x03, 0x0a, 0x0a, 0x07, 0x07, 0x0e, 0x0e, 0x07, 0x07, 0x0a, 0x0a, 0x03, 0x03, 0x0e, 0x0e,
		0x0b, 0x0a, 0x02, 0x02, 0x0f, 0x0e, 0x06, 0x06, 0x0f, 0x0e, 0x02, 0x02, 0x0b, 0x0a, 0x06, 0x06,
		0x07, 0x07, 0x0e, 0x0e, 0x07, 0x07, 0x0e, 0x0e, 0x01, 0x01, 0x0c, 0x0c, 0x01, 0x01, 0x0c, 0x0c,
		0x0f, 0x0e, 0x06, 0x06, 0x0f, 0x0e, 0x06, 0x06, 0x09, 0x08, 0x04, 0x04, 0x09, 0x08, 0x04, 0x04,
		0x02, 0x02, 0x0b, 0x0b, 0x04, 0x04, 0x0d, 0x0d, 0x03, 0x0b, 0x0a, 0x02, 0x05, 0x0d, 0x0c, 0x04,
		0x0a, 0x0b, 0x03, 0x03, 0x0e, 0x0f, 0x07, 0x07, 0x0b, 0x0a, 0x02, 0x02, 0x0f, 0x0e, 0x06, 0x06,
		0x06, 0x06, 0x0f, 0x0f, 0x04, 0x04, 0x0d, 0x0d, 0x05, 0x0d, 0x0c, 0x04, 0x07, 0x0f, 0x0e, 0x06,
		0x0e, 0x0f, 0x07, 0x07, 0x0e, 0x0f, 0x07, 0x07, 0x0d, 0x0c, 0x04, 0x04, 0x0d, 0x0c, 0x04, 0x04,
		0x02, 0x02, 0x0b, 0x0b, 0x06, 0x06, 0x0f, 0x0f, 0x07, 0x0f, 0x0a, 0x02, 0x03, 0x0b, 0x0e, 0x06,
		0x0a, 0x0b, 0x03, 0x03, 0x0e, 0x0f, 0x07, 0x07, 0x0f, 0x0e, 0x02, 0x02, 0x0b, 0x0a, 0x06, 0x06,
		0x06, 0x06, 0x0f, 0x0f, 0x06, 0x06, 0x0f, 0x0f, 0x01, 0x09, 0x0c, 0x04, 0x01, 0x09, 0x0c, 0x04,
		0x0e, 0x0f, 0x07, 0x07, 0x0e, 0x0f, 0x07, 0x07, 0x09, 0x08, 0x04, 0x04, 0x09, 0x08, 0x04, 0x04,
	};

	return prot_lut_table[in];
}

static INT32 PuyoInit()
{
	return SegaC2Init(puyo_protection_callback);
}

struct BurnDriver BurnDrvPuyo = {
	"puyo", NULL, NULL, NULL, "1992",
	"Puyo Puyo (World)\0", NULL, "Compile / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, puyoRomInfo, puyoRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, PuyoDIPInfo,
	PuyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puyo Puyo (Japan, Rev B)

static struct BurnRomInfo puyojRomDesc[] = {
	{ "epr-15036b.ic32",		0x20000, 0x5310ca1b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15035b.ic31",		0x20000, 0xbc62e400, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15038.ic34",			0x20000, 0x3b9eea0c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15037.ic33",			0x20000, 0xbe2f7974, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15034.ic4",			0x20000, 0x5688213b, 2 | BRF_SND },           //  4 UPD Samples

	{ "315-5452.ic24",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  5 pals
	{ "315-5394.ic25",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  6
	{ "315-5395.ic26",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7
	{ "317-0203.ic27",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  8
};

STD_ROM_PICK(puyoj)
STD_ROM_FN(puyoj)

struct BurnDriver BurnDrvPuyoj = {
	"puyoj", "puyo", NULL, NULL, "1992",
	"Puyo Puyo (Japan, Rev B)\0", NULL, "Compile / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, puyojRomInfo, puyojRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, PuyoDIPInfo,
	PuyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puyo Puyo (Japan, Rev A)

static struct BurnRomInfo puyojaRomDesc[] = {
	{ "epr-15036a.ic32",		0x20000, 0x61b35257, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15035a.ic31",		0x20000, 0xdfebb6d9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15038.ic34",			0x20000, 0x3b9eea0c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15037.ic33",			0x20000, 0xbe2f7974, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15034.ic4",			0x20000, 0x5688213b, 2 | BRF_SND },           //  4 UPD Samples

	{ "315-5452.ic24",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  5 pals
	{ "315-5394.ic25",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  6
	{ "315-5395.ic26",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7
	{ "317-0203.ic27",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  8
};

STD_ROM_PICK(puyoja)
STD_ROM_FN(puyoja)

struct BurnDriver BurnDrvPuyoja = {
	"puyoja", "puyo", NULL, NULL, "1992",
	"Puyo Puyo (Japan, Rev A)\0", NULL, "Compile / Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, puyojaRomInfo, puyojaRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, PuyoDIPInfo,
	PuyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puyo Puyo (World, bootleg)

static struct BurnRomInfo puyoblRomDesc[] = {
	{ "puyopuyb.4bo",			0x20000, 0x89ea4d33, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "puyopuyb.3bo",			0x20000, 0xc002e545, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "puyopuyb.6bo",			0x20000, 0x0a0692e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "puyopuyb.5bo",			0x20000, 0x353109b8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "puyopuyb.abo",			0x20000, 0x79112b3b, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(puyobl)
STD_ROM_FN(puyobl)

struct BurnDriver BurnDrvPuyobl = {
	"puyobl", "puyo", NULL, NULL, "1992",
	"Puyo Puyo (World, bootleg)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, puyoblRomInfo, puyoblRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, PuyoDIPInfo,
	PuyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Japan)

static struct BurnRomInfo tantrRomDesc[] = {
	{ "epr-15614.ic32",			0x80000, 0x557782bc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15613.ic31",			0x80000, 0x14bbb235, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15616.ic34",			0x80000, 0x17b80202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15615.ic33",			0x80000, 0x36a88bd4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15617.ic4",			0x40000, 0x338324a1, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(tantr)
STD_ROM_FN(tantr)

static UINT8 tantr_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x09, 0x01, 0x0d, 0x0d, 0x0d, 0x01, 0x09, 0x0d, 0x09, 0x01, 0x0d, 0x0d, 0x0d, 0x01, 0x09, 0x0d,
		0x0d, 0x04, 0x0d, 0x0c, 0x09, 0x04, 0x09, 0x0c, 0x0f, 0x06, 0x0f, 0x0e, 0x0b, 0x06, 0x0b, 0x0e,
		0x09, 0x01, 0x0b, 0x0b, 0x0d, 0x01, 0x0f, 0x0b, 0x09, 0x01, 0x0b, 0x0b, 0x0d, 0x01, 0x0f, 0x0b,
		0x0d, 0x04, 0x0f, 0x0e, 0x09, 0x04, 0x0b, 0x0e, 0x0f, 0x06, 0x0f, 0x0e, 0x0b, 0x06, 0x0b, 0x0e,
		0x08, 0x00, 0x0c, 0x0c, 0x0e, 0x02, 0x0a, 0x0e, 0x08, 0x08, 0x0c, 0x0c, 0x0e, 0x0a, 0x0a, 0x0e,
		0x0c, 0x05, 0x0c, 0x0d, 0x0a, 0x07, 0x0a, 0x0f, 0x0e, 0x0f, 0x0e, 0x0f, 0x08, 0x0d, 0x08, 0x0d,
		0x09, 0x01, 0x0b, 0x0b, 0x0f, 0x03, 0x0d, 0x09, 0x09, 0x09, 0x0b, 0x0b, 0x0f, 0x0b, 0x0d, 0x09,
		0x0d, 0x04, 0x0f, 0x0e, 0x0b, 0x06, 0x09, 0x0c, 0x0f, 0x0e, 0x0f, 0x0e, 0x09, 0x0c, 0x09, 0x0c,
		0x05, 0x0d, 0x05, 0x05, 0x09, 0x05, 0x09, 0x0d, 0x05, 0x0d, 0x05, 0x05, 0x09, 0x05, 0x09, 0x0d,
		0x05, 0x0c, 0x05, 0x04, 0x09, 0x04, 0x09, 0x0c, 0x07, 0x0e, 0x07, 0x06, 0x0b, 0x06, 0x0b, 0x0e,
		0x05, 0x0d, 0x07, 0x07, 0x09, 0x05, 0x0b, 0x0f, 0x05, 0x0d, 0x07, 0x07, 0x09, 0x05, 0x0b, 0x0f,
		0x05, 0x0c, 0x07, 0x06, 0x09, 0x04, 0x0b, 0x0e, 0x07, 0x0e, 0x07, 0x06, 0x0b, 0x06, 0x0b, 0x0e,
		0x05, 0x0d, 0x05, 0x05, 0x0b, 0x07, 0x0b, 0x0f, 0x04, 0x04, 0x04, 0x04, 0x0a, 0x0e, 0x0a, 0x0e,
		0x05, 0x0c, 0x05, 0x04, 0x0b, 0x06, 0x0b, 0x0e, 0x06, 0x07, 0x06, 0x07, 0x08, 0x0d, 0x08, 0x0d,
		0x05, 0x0d, 0x07, 0x07, 0x0b, 0x07, 0x09, 0x0d, 0x05, 0x05, 0x07, 0x07, 0x0b, 0x0f, 0x09, 0x0d,
		0x05, 0x0c, 0x07, 0x06, 0x0b, 0x06, 0x09, 0x0c, 0x07, 0x06, 0x07, 0x06, 0x09, 0x0c, 0x09, 0x0c,
	};

	return prot_lut_table[in];
}

static INT32 TantrInit()
{
	return SegaC2Init(tantr_protection_callback);
}

struct BurnDriver BurnDrvTantr = {
	"tantr", NULL, NULL, NULL, "1992",
	"Puzzle & Action: Tant-R (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrRomInfo, tantrRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	TantrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Korea)

static struct BurnRomInfo tantrkorRomDesc[] = {
	{ "mpr-15592b.ic32",		0x80000, 0x7efe26b3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mpr-15592b.ic31",		0x80000, 0xaf5a860f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15992b.ic34",		0x80000, 0x6282a5d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15592b.ic33",		0x80000, 0x82d78413, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15617.ic4",			0x40000, 0x338324a1, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(tantrkor)
STD_ROM_FN(tantrkor)

static UINT8 tantrkor_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x08, 0x00, 0x09, 0x03, 0x01, 0x01, 0x00, 0x02, 0x0c, 0x04, 0x0d, 0x07, 0x05, 0x05, 0x04, 0x06,
		0x0d, 0x05, 0x08, 0x02, 0x05, 0x05, 0x00, 0x02, 0x09, 0x01, 0x0c, 0x06, 0x01, 0x01, 0x04, 0x06,
		0x00, 0x08, 0x01, 0x0b, 0x09, 0x09, 0x08, 0x0a, 0x04, 0x0c, 0x05, 0x0f, 0x0d, 0x0d, 0x0c, 0x0e,
		0x05, 0x0d, 0x00, 0x0a, 0x0d, 0x0d, 0x08, 0x0a, 0x01, 0x09, 0x04, 0x0e, 0x09, 0x09, 0x0c, 0x0e,
		0x0c, 0x04, 0x0d, 0x07, 0x07, 0x07, 0x06, 0x04, 0x0c, 0x04, 0x0d, 0x07, 0x07, 0x07, 0x06, 0x04,
		0x09, 0x01, 0x0c, 0x06, 0x03, 0x03, 0x06, 0x04, 0x09, 0x01, 0x0c, 0x06, 0x03, 0x03, 0x06, 0x04,
		0x0c, 0x04, 0x0d, 0x07, 0x07, 0x07, 0x06, 0x04, 0x0c, 0x04, 0x0d, 0x07, 0x07, 0x07, 0x06, 0x04,
		0x09, 0x01, 0x0c, 0x06, 0x03, 0x03, 0x06, 0x04, 0x09, 0x01, 0x0c, 0x06, 0x03, 0x03, 0x06, 0x04,
		0x09, 0x01, 0x09, 0x03, 0x00, 0x00, 0x00, 0x02, 0x0d, 0x05, 0x0d, 0x07, 0x04, 0x04, 0x04, 0x06,
		0x0c, 0x04, 0x08, 0x02, 0x04, 0x04, 0x00, 0x02, 0x08, 0x00, 0x0c, 0x06, 0x00, 0x00, 0x04, 0x06,
		0x01, 0x09, 0x01, 0x0b, 0x08, 0x08, 0x08, 0x0a, 0x05, 0x0d, 0x05, 0x0f, 0x0c, 0x0c, 0x0c, 0x0e,
		0x04, 0x0c, 0x00, 0x0a, 0x0c, 0x0c, 0x08, 0x0a, 0x00, 0x08, 0x04, 0x0e, 0x08, 0x08, 0x0c, 0x0e,
		0x0d, 0x05, 0x0d, 0x07, 0x06, 0x06, 0x06, 0x04, 0x0d, 0x05, 0x0d, 0x07, 0x06, 0x06, 0x06, 0x04,
		0x08, 0x00, 0x0c, 0x06, 0x02, 0x02, 0x06, 0x04, 0x08, 0x00, 0x0c, 0x06, 0x02, 0x02, 0x06, 0x04,
		0x0d, 0x05, 0x0d, 0x07, 0x06, 0x06, 0x06, 0x04, 0x0d, 0x05, 0x0d, 0x07, 0x06, 0x06, 0x06, 0x04,
		0x08, 0x00, 0x0c, 0x06, 0x02, 0x02, 0x06, 0x04, 0x08, 0x00, 0x0c, 0x06, 0x02, 0x02, 0x06, 0x04 
	};

	return prot_lut_table[in];
}

static INT32 TantrkorInit()
{
	return SegaC2Init(tantrkor_protection_callback);
}

struct BurnDriver BurnDrvTantrkor = {
	"tantrkor", "tantr", NULL, NULL, "1993",
	"Puzzle & Action: Tant-R (Korea)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrkorRomInfo, tantrkorRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	TantrkorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Japan) (bootleg set 1)

static struct BurnRomInfo tantrblRomDesc[] = {
	{ "pa_e10.bin",				0x80000, 0x6c3f711f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pa_f10.bin",				0x80000, 0x75526786, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15616.ic34",			0x80000, 0x17b80202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15615.ic33",			0x80000, 0x36a88bd4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pa_e03.bin",				0x20000, 0x72918c58, 2 | BRF_SND },           //  4 UPD Samples
	{ "pa_e02.bin",				0x20000, 0x4e85b2a3, 2 | BRF_SND },           //  5
};

STD_ROM_PICK(tantrbl)
STD_ROM_FN(tantrbl)

struct BurnDriver BurnDrvTantrbl = {
	"tantrbl", "tantr", NULL, NULL, "1992",
	"Puzzle & Action: Tant-R (Japan) (bootleg set 1)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrblRomInfo, tantrblRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Japan) (bootleg set 2)

static struct BurnRomInfo tantrbl2RomDesc[] = {
	{ "trb2_2.32",				0x80000, 0x8fc99c48, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "trb2_1.31",				0x80000, 0xc318d00d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-15616.ic34",			0x80000, 0x17b80202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-15615.ic33",			0x80000, 0x36a88bd4, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(tantrbl2)
STD_ROM_FN(tantrbl2)

struct BurnDriver BurnDrvTantrbl2 = {
	"tantrbl2", "tantr", NULL, NULL, "1994",
	"Puzzle & Action: Tant-R (Japan) (bootleg set 2)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrbl2RomInfo, tantrbl2RomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	TantrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Japan) (bootleg set 3)

static struct BurnRomInfo tantrbl3RomDesc[] = {
	{ "2.u29",					0x80000, 0xfaecb7b1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.u28",					0x80000, 0x3304556d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u31",					0x80000, 0x17b80202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u30",					0x80000, 0x36a88bd4, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(tantrbl3)
STD_ROM_FN(tantrbl3)

struct BurnDriver BurnDrvTantrbl3 = {
	"tantrbl3", "tantr", NULL, NULL, "1994",
	"Puzzle & Action: Tant-R (Japan) (bootleg set 3)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrbl3RomInfo, tantrbl3RomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	TantrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Tant-R (Japan) (bootleg set 4)

static struct BurnRomInfo tantrbl4RomDesc[] = {
	{ "a.bin",					0x80000, 0x25cafec2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "b.bin",					0x80000, 0x8cf5ffd5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d.bin",					0x80000, 0x17b80202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c.bin",					0x80000, 0x36a88bd4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "2.bin",					0x20000, 0x72918c58, 2 | BRF_SND },           //  4 UPD Samples
	{ "1.bin",					0x20000, 0x4e85b2a3, 2 | BRF_SND },           //  5
};

STD_ROM_PICK(tantrbl4)
STD_ROM_FN(tantrbl4)

struct BurnDriver BurnDrvTantrbl4 = {
	"tantrbl4", "tantr", NULL, NULL, "1992",
	"Puzzle & Action: Tant-R (Japan) (bootleg set 4)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, tantrbl4RomInfo, tantrbl4RomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Waku Waku Marine

static struct BurnRomInfo wwmarineRomDesc[] = {
	{ "epr-15097.ic32",			0x40000, 0x1223a77a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15096.ic31",			0x40000, 0x1b833932, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-15095.ic4",			0x40000, 0xdf13755b, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(wwmarine)
STD_ROM_FN(wwmarine)

static INT32 WwmarineInit()
{
	is_wwmarine = 1;
	return NoProtectionInit();
}

struct BurnDriver BurnDrvWwmarine = {
	"wwmarine", NULL, NULL, NULL, "1992",
	"Waku Waku Marine\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, wwmarineRomInfo, wwmarineRomName, NULL, NULL, NULL, NULL, WwmarineInputInfo, WwmarineDIPInfo,
	WwmarineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};

// Waku Waku Anpanman (Rev A)

static struct BurnRomInfo wwanpanmRomDesc[] = {
	{ "epr-14123a.ic32",		0x40000, 0x0e4f38c6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-14122a.ic31",		0x40000, 0x01b8fe20, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-14121.ic4",			0x40000, 0x69adf3a1, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(wwanpanm)
STD_ROM_FN(wwanpanm)

struct BurnDriver BurnDrvwwanpanm = {
	"wwanpanm", NULL, NULL, NULL, "1992",
	"Waku Waku Anpanman (Rev A)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, wwanpanmRomInfo, wwanpanmRomName, NULL, NULL, NULL, NULL, WwmarineInputInfo, WwmarineDIPInfo,
	WwmarineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// SegaSonic Cosmo Fighter (World)

static struct BurnRomInfo sonicfgtRomDesc[] = {
	{ "epr-17178.ic32",			0x40000, 0xe05b7388, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-17177.ic31",			0x40000, 0x7c2ec4eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-17180.ic34",			0x40000, 0x8933e91c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-17179.ic33",			0x40000, 0x0ae979cd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-17176.ic4",			0x40000, 0x4211745d, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(sonicfgt)
STD_ROM_FN(sonicfgt)

struct BurnDriver BurnDrvSonicfgt = {
	"sonicfgt", NULL, NULL, NULL, "1993",
	"SegaSonic Cosmo Fighter (World)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sonicfgtRomInfo, sonicfgtRomName, NULL, NULL, NULL, NULL, SonicfgtInputInfo, SonicfgtDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// SegaSonic Cosmo Fighter (Japan)

static struct BurnRomInfo sonicfgtjRomDesc[] = {
	{ "epr-16001.ic32",			0x40000, 0x8ed1dc11, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-16000.ic31",			0x40000, 0x1440caec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16003.ic34",			0x40000, 0x8933e91c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16002.ic33",			0x40000, 0x0ae979cd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16004.ic4",			0x40000, 0xe87e8433, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(sonicfgtj)
STD_ROM_FN(sonicfgtj)

struct BurnDriver BurnDrvSonicfgtj = {
	"sonicfgtj", "sonicfgt", NULL, NULL, "1993",
	"SegaSonic Cosmo Fighter (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sonicfgtjRomInfo, sonicfgtjRomName, NULL, NULL, NULL, NULL, SonicfgtInputInfo, SonicfgtDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// SegaSonic Cosmo Fighter (US)

static struct BurnRomInfo sonicfgtuRomDesc[] = {
	{ "epr-17778.ic32",			0x40000, 0xe05b7388, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-17177.ic31",			0x40000, 0x7c2ec4eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-17180.ic34",			0x40000, 0x8933e91c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-17179.ic33",			0x40000, 0x0ae979cd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-17176.ic4",			0x40000, 0x4211745d, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(sonicfgtu)
STD_ROM_FN(sonicfgtu)

struct BurnDriver BurnDrvSonicfgtu = {
	"sonicfgtu", "sonicfgt", NULL, NULL, "1993",
	"SegaSonic Cosmo Fighter (US)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sonicfgtuRomInfo, sonicfgtuRomName, NULL, NULL, NULL, NULL, SonicfgtInputInfo, SonicfgtDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Poto Poto (Japan, Rev A)

static struct BurnRomInfo potopotoRomDesc[] = {
	{ "epr-16662a.ic32",		0x40000, 0xbbd305d6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-16661a.ic31",		0x40000, 0x5a7d14f4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-16660.ic4",			0x40000, 0x8251c61c, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(potopoto)
STD_ROM_FN(potopoto)

static UINT8 potopoto_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x0b, 0x03, 0x03, 0x0b, 0x0a, 0x02, 0x02, 0x0a, 0x0a, 0x02, 0x03, 0x0b, 0x0b, 0x03, 0x02, 0x0a,
		0x0a, 0x02, 0x02, 0x0a, 0x0a, 0x02, 0x02, 0x0a, 0x0b, 0x03, 0x02, 0x0a, 0x0b, 0x03, 0x02, 0x0a,
		0x0b, 0x01, 0x03, 0x09, 0x0a, 0x00, 0x02, 0x08, 0x0a, 0x00, 0x03, 0x09, 0x0b, 0x01, 0x02, 0x08,
		0x0a, 0x00, 0x02, 0x08, 0x0a, 0x00, 0x02, 0x08, 0x0b, 0x01, 0x02, 0x08, 0x0b, 0x01, 0x02, 0x08,
		0x03, 0x0f, 0x03, 0x0f, 0x02, 0x0e, 0x02, 0x0e, 0x02, 0x0e, 0x03, 0x0f, 0x03, 0x0f, 0x02, 0x0e,
		0x02, 0x0e, 0x02, 0x0e, 0x02, 0x0e, 0x02, 0x0e, 0x03, 0x0f, 0x02, 0x0e, 0x03, 0x0f, 0x02, 0x0e,
		0x03, 0x0d, 0x03, 0x0d, 0x02, 0x0c, 0x02, 0x0c, 0x02, 0x0c, 0x03, 0x0d, 0x03, 0x0d, 0x02, 0x0c,
		0x02, 0x0c, 0x02, 0x0c, 0x02, 0x0c, 0x02, 0x0c, 0x03, 0x0d, 0x02, 0x0c, 0x03, 0x0d, 0x02, 0x0c,
		0x0d, 0x0d, 0x01, 0x01, 0x0e, 0x0e, 0x02, 0x02, 0x0c, 0x0c, 0x01, 0x01, 0x0f, 0x0f, 0x02, 0x02,
		0x0c, 0x0c, 0x00, 0x00, 0x0e, 0x0e, 0x02, 0x02, 0x0d, 0x0d, 0x00, 0x00, 0x0f, 0x0f, 0x02, 0x02,
		0x0d, 0x0f, 0x01, 0x03, 0x0e, 0x0c, 0x02, 0x00, 0x0c, 0x0e, 0x01, 0x03, 0x0f, 0x0d, 0x02, 0x00,
		0x0c, 0x0e, 0x00, 0x02, 0x0e, 0x0c, 0x02, 0x00, 0x0d, 0x0f, 0x00, 0x02, 0x0f, 0x0d, 0x02, 0x00,
		0x05, 0x01, 0x01, 0x05, 0x06, 0x02, 0x02, 0x06, 0x04, 0x00, 0x01, 0x05, 0x07, 0x03, 0x02, 0x06,
		0x04, 0x00, 0x00, 0x04, 0x06, 0x02, 0x02, 0x06, 0x05, 0x01, 0x00, 0x04, 0x07, 0x03, 0x02, 0x06,
		0x05, 0x03, 0x01, 0x07, 0x06, 0x00, 0x02, 0x04, 0x04, 0x02, 0x01, 0x07, 0x07, 0x01, 0x02, 0x04,
		0x04, 0x02, 0x00, 0x06, 0x06, 0x00, 0x02, 0x04, 0x05, 0x03, 0x00, 0x06, 0x07, 0x01, 0x02, 0x04
	};

	return prot_lut_table[in];
}

static INT32 PotopotoInit()
{
	return SegaC2Init(potopoto_protection_callback);
}

struct BurnDriver BurnDrvPotopoto = {
	"potopoto", NULL, NULL, NULL, "1994",
	"Poto Poto (Japan, Rev A)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, potopotoRomInfo, potopotoRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, PotopotoDIPInfo,
	PotopotoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};



// Stack Columns (World)

static struct BurnRomInfo stkclmnsRomDesc[] = {
	{ "epr-16874.ic32",	0x80000, 0xd78a871c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "epr-16873.ic31",	0x80000, 0x1def1da4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-16797.ic34",	0x80000, 0xb28e9bd5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-16796.ic33",	0x80000, 0xec7de52d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16793.ic4",	0x20000, 0xebb2d057, 2 | BRF_SND },           //  4 upd
};

STD_ROM_PICK(stkclmns)
STD_ROM_FN(stkclmns)

static UINT8 stkclmns_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x01, 0x0d, 0x05, 0x09, 0x01, 0x0d, 0x05, 0x09, 0x00, 0x0c, 0x05, 0x09, 0x00, 0x0c, 0x05, 0x09,
		0x01, 0x0d, 0x05, 0x09, 0x00, 0x0c, 0x04, 0x08, 0x00, 0x0c, 0x05, 0x09, 0x01, 0x0d, 0x04, 0x08,
		0x01, 0x0f, 0x05, 0x0b, 0x01, 0x0f, 0x05, 0x0b, 0x00, 0x0e, 0x05, 0x0b, 0x00, 0x0e, 0x05, 0x0b,
		0x01, 0x0f, 0x05, 0x0b, 0x00, 0x0e, 0x04, 0x0a, 0x00, 0x0e, 0x05, 0x0b, 0x01, 0x0f, 0x04, 0x0a,
		0x09, 0x01, 0x05, 0x0d, 0x09, 0x01, 0x05, 0x0d, 0x08, 0x00, 0x05, 0x0d, 0x08, 0x00, 0x05, 0x0d,
		0x09, 0x01, 0x05, 0x0d, 0x08, 0x00, 0x04, 0x0c, 0x08, 0x00, 0x05, 0x0d, 0x09, 0x01, 0x04, 0x0c,
		0x09, 0x03, 0x05, 0x0f, 0x09, 0x03, 0x05, 0x0f, 0x08, 0x02, 0x05, 0x0f, 0x08, 0x02, 0x05, 0x0f,
		0x09, 0x03, 0x05, 0x0f, 0x08, 0x02, 0x04, 0x0e, 0x08, 0x02, 0x05, 0x0f, 0x09, 0x03, 0x04, 0x0e,
		0x01, 0x05, 0x01, 0x05, 0x03, 0x07, 0x03, 0x07, 0x00, 0x04, 0x01, 0x05, 0x02, 0x06, 0x03, 0x07,
		0x01, 0x05, 0x01, 0x05, 0x02, 0x06, 0x02, 0x06, 0x00, 0x04, 0x01, 0x05, 0x03, 0x07, 0x02, 0x06,
		0x01, 0x07, 0x01, 0x07, 0x03, 0x05, 0x03, 0x05, 0x00, 0x06, 0x01, 0x07, 0x02, 0x04, 0x03, 0x05,
		0x01, 0x07, 0x01, 0x07, 0x02, 0x04, 0x02, 0x04, 0x00, 0x06, 0x01, 0x07, 0x03, 0x05, 0x02, 0x04,
		0x09, 0x09, 0x01, 0x01, 0x0b, 0x0b, 0x03, 0x03, 0x08, 0x08, 0x01, 0x01, 0x0a, 0x0a, 0x03, 0x03,
		0x09, 0x09, 0x01, 0x01, 0x0a, 0x0a, 0x02, 0x02, 0x08, 0x08, 0x01, 0x01, 0x0b, 0x0b, 0x02, 0x02,
		0x09, 0x0b, 0x01, 0x03, 0x0b, 0x09, 0x03, 0x01, 0x08, 0x0a, 0x01, 0x03, 0x0a, 0x08, 0x03, 0x01,
		0x09, 0x0b, 0x01, 0x03, 0x0a, 0x08, 0x02, 0x00, 0x08, 0x0a, 0x01, 0x03, 0x0b, 0x09, 0x02, 0x00
	};

	return prot_lut_table[in];
}

static INT32 StkclmnsInit()
{
	return SegaC2Init(stkclmns_protection_callback);
}

struct BurnDriver BurnDrvStkclmns = {
	"stkclmns", NULL, NULL, NULL, "1994",
	"Stack Columns (World)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, stkclmnsRomInfo, stkclmnsRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, StkclmnsDIPInfo,
	StkclmnsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Stack Columns (Japan)

static struct BurnRomInfo stkclmnsjRomDesc[] = {
	{ "epr-16795.ic32",			0x80000, 0xb478fd02, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-16794.ic31",			0x80000, 0x6d0e8c56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpr-16797.ic34",			0x80000, 0xb28e9bd5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpr-16796.ic33",			0x80000, 0xec7de52d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16793.ic4",			0x20000, 0xebb2d057, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(stkclmnsj)
STD_ROM_FN(stkclmnsj)

static UINT8 stkclmnsj_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x0c, 0x0c, 0x08, 0x08, 0x0c, 0x0c, 0x08, 0x08, 0x0c, 0x0c, 0x08, 0x08, 0x0c, 0x0c, 0x08, 0x08,
		0x0c, 0x0c, 0x09, 0x09, 0x0c, 0x0c, 0x09, 0x09, 0x0c, 0x0c, 0x09, 0x09, 0x0c, 0x0c, 0x09, 0x09,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09,
		0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08,
		0x0a, 0x0a, 0x0e, 0x0e, 0x08, 0x08, 0x0c, 0x0c, 0x0e, 0x0e, 0x0a, 0x0a, 0x0c, 0x0c, 0x08, 0x08,
		0x0a, 0x0a, 0x0f, 0x0f, 0x08, 0x08, 0x0d, 0x0d, 0x0e, 0x0e, 0x0b, 0x0b, 0x0c, 0x0c, 0x09, 0x09,
		0x06, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x0a, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09,
		0x06, 0x06, 0x07, 0x07, 0x05, 0x05, 0x04, 0x04, 0x0a, 0x0a, 0x0b, 0x0b, 0x09, 0x09, 0x08, 0x08,
		0x0e, 0x0e, 0x0a, 0x0a, 0x0e, 0x0e, 0x0a, 0x0a, 0x0e, 0x0e, 0x0a, 0x0a, 0x0e, 0x0e, 0x0a, 0x0a,
		0x0e, 0x0e, 0x0b, 0x0b, 0x0e, 0x0e, 0x0b, 0x0b, 0x0e, 0x0e, 0x0b, 0x0b, 0x0e, 0x0e, 0x0b, 0x0b,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09,
		0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08,
		0x00, 0x00, 0x04, 0x04, 0x02, 0x02, 0x06, 0x06, 0x04, 0x04, 0x00, 0x00, 0x06, 0x06, 0x02, 0x02,
		0x00, 0x00, 0x05, 0x05, 0x02, 0x02, 0x07, 0x07, 0x04, 0x04, 0x01, 0x01, 0x06, 0x06, 0x03, 0x03,
		0x0e, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d, 0x0d, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01,
		0x0e, 0x0e, 0x0f, 0x0f, 0x0d, 0x0d, 0x0c, 0x0c, 0x02, 0x02, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00
	};

	return prot_lut_table[in];
}

static INT32 StkclmnsjInit()
{
	return SegaC2Init(stkclmnsj_protection_callback);
}

struct BurnDriver BurnDrvStkclmnsj = {
	"stkclmnsj", "stkclmns", NULL, NULL, "1994",
	"Stack Columns (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, stkclmnsjRomInfo, stkclmnsjRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, StkclmnsDIPInfo,
	StkclmnsjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Ichidant-R (World)

static struct BurnRomInfo ichirRomDesc[] = {
	{ "pa2_32.bin",				0x80000, 0x7ba0c025, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pa2_31.bin",				0x80000, 0x5f86e5cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16888.ic34",			0x80000, 0x85d73722, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16887.ic33",			0x80000, 0xbc3bbf25, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pa2_02.bin",				0x80000, 0xfc7b0da5, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ichir)
STD_ROM_FN(ichir)

static UINT8 ichir_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x04, 0x0c, 0x04, 0x0c, 0x04, 0x0c, 0x04, 0x0c, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,
		0x05, 0x0d, 0x05, 0x0d, 0x04, 0x0c, 0x04, 0x0c, 0x01, 0x09, 0x01, 0x09, 0x00, 0x08, 0x00, 0x08,
		0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02,
		0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
		0x00, 0x08, 0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a,
		0x01, 0x09, 0x03, 0x0b, 0x00, 0x08, 0x02, 0x0a, 0x01, 0x09, 0x03, 0x0b, 0x00, 0x08, 0x02, 0x0a,
		0x07, 0x07, 0x05, 0x05, 0x06, 0x06, 0x04, 0x04, 0x03, 0x03, 0x01, 0x01, 0x02, 0x02, 0x00, 0x00,
		0x06, 0x06, 0x04, 0x04, 0x06, 0x06, 0x04, 0x04, 0x02, 0x02, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00,
		0x06, 0x0e, 0x06, 0x0e, 0x06, 0x0e, 0x06, 0x0e, 0x02, 0x0a, 0x02, 0x0a, 0x02, 0x0a, 0x02, 0x0a,
		0x07, 0x0f, 0x07, 0x0f, 0x06, 0x0e, 0x06, 0x0e, 0x03, 0x0b, 0x03, 0x0b, 0x02, 0x0a, 0x02, 0x0a,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a,
		0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
		0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a, 0x00, 0x08, 0x02, 0x0a, 0x00, 0x08,
		0x03, 0x0b, 0x01, 0x09, 0x02, 0x0a, 0x00, 0x08, 0x03, 0x0b, 0x01, 0x09, 0x02, 0x0a, 0x00, 0x08,
		0x0f, 0x0f, 0x0d, 0x0d, 0x0e, 0x0e, 0x0c, 0x0c, 0x0b, 0x0b, 0x09, 0x09, 0x0a, 0x0a, 0x08, 0x08,
		0x0e, 0x0e, 0x0c, 0x0c, 0x0e, 0x0e, 0x0c, 0x0c, 0x0a, 0x0a, 0x08, 0x08, 0x0a, 0x0a, 0x08, 0x08
	};

	return prot_lut_table[in];
}

static INT32 IchirInit()
{
	return SegaC2Init(ichir_protection_callback);
}

struct BurnDriver BurnDrvIchir = {
	"ichir", NULL, NULL, NULL, "1994",
	"Puzzle & Action: Ichidant-R (World)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, ichirRomInfo, ichirRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	IchirInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Ichidant-R (World) (bootleg)

static struct BurnRomInfo ichirblRomDesc[] = {
	{ "2.f11",					0x80000, 0xb8201c2e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.f10",					0x80000, 0xaf0dd811, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.f8",					0x80000, 0x85d73722, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.f9",					0x80000, 0xbc3bbf25, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "2.e3",					0x80000, 0xfc7b0da5, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ichirbl)
STD_ROM_FN(ichirbl)

struct BurnDriver BurnDrvIchirbl = {
	"ichirbl", "ichir", NULL, NULL, "1994",
	"Puzzle & Action: Ichidant-R (World) (bootleg)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, ichirblRomInfo, ichirblRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Ichidant-R (Korea)

static struct BurnRomInfo ichirkRomDesc[] = {
	{ "epr_ichi.32",			0x80000, 0x804dea11, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr_ichi.31",			0x80000, 0x92452353, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16888.ic34",			0x80000, 0x85d73722, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16887.ic33",			0x80000, 0xbc3bbf25, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pa2_02.bin",				0x80000, 0xfc7b0da5, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ichirk)
STD_ROM_FN(ichirk)

static UINT8 ichirk_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04,
		0x05, 0x05, 0x01, 0x01, 0x04, 0x04, 0x00, 0x00, 0x01, 0x01, 0x05, 0x05, 0x00, 0x00, 0x04, 0x04,
		0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08,
		0x06, 0x06, 0x0b, 0x0b, 0x07, 0x07, 0x0a, 0x0a, 0x06, 0x06, 0x0b, 0x0b, 0x07, 0x07, 0x0a, 0x0a,
		0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x04, 0x06, 0x04, 0x06, 0x04, 0x06, 0x04, 0x06,
		0x01, 0x03, 0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x05, 0x07, 0x05, 0x07, 0x04, 0x06, 0x04, 0x06,
		0x01, 0x03, 0x08, 0x0a, 0x01, 0x03, 0x08, 0x0a, 0x01, 0x03, 0x08, 0x0a, 0x01, 0x03, 0x08, 0x0a,
		0x02, 0x00, 0x0b, 0x09, 0x03, 0x01, 0x0a, 0x08, 0x02, 0x00, 0x0b, 0x09, 0x03, 0x01, 0x0a, 0x08,
		0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04,
		0x05, 0x05, 0x01, 0x01, 0x04, 0x04, 0x00, 0x00, 0x01, 0x01, 0x05, 0x05, 0x00, 0x00, 0x04, 0x04,
		0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08, 0x05, 0x05, 0x08, 0x08,
		0x06, 0x06, 0x0b, 0x0b, 0x07, 0x07, 0x0a, 0x0a, 0x06, 0x06, 0x0b, 0x0b, 0x07, 0x07, 0x0a, 0x0a,
		0x08, 0x0a, 0x08, 0x0a, 0x08, 0x0a, 0x08, 0x0a, 0x0c, 0x0e, 0x0c, 0x0e, 0x0c, 0x0e, 0x0c, 0x0e,
		0x09, 0x0b, 0x09, 0x0b, 0x08, 0x0a, 0x08, 0x0a, 0x0d, 0x0f, 0x0d, 0x0f, 0x0c, 0x0e, 0x0c, 0x0e,
		0x09, 0x0b, 0x00, 0x02, 0x09, 0x0b, 0x00, 0x02, 0x09, 0x0b, 0x00, 0x02, 0x09, 0x0b, 0x00, 0x02,
		0x0a, 0x08, 0x03, 0x01, 0x0b, 0x09, 0x02, 0x00, 0x0a, 0x08, 0x03, 0x01, 0x0b, 0x09, 0x02, 0x00,
	};

	return prot_lut_table[in];
}

static INT32 IchirkInit()
{
	return SegaC2Init(ichirk_protection_callback);
}

struct BurnDriver BurnDrvIchirk = {
	"ichirk", "ichir", NULL, NULL, "1994",
	"Puzzle & Action: Ichidant-R (Korea)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, ichirkRomInfo, ichirkRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	IchirkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Ichidant-R (Japan)

static struct BurnRomInfo ichirjRomDesc[] = {
	{ "epr-16886.ic32",			0x80000, 0x38208e28, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-16885.ic31",			0x80000, 0x1ce4e837, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16888.ic34",			0x80000, 0x85d73722, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16887.ic33",			0x80000, 0xbc3bbf25, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16884.ic4",			0x80000, 0xfd9dcdd6, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(ichirj)
STD_ROM_FN(ichirj)

static UINT8 ichirj_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x05, 0x05, 0x01, 0x01, 0x06, 0x06, 0x02, 0x02, 0x05, 0x05, 0x01, 0x01, 0x06, 0x06, 0x02, 0x02,
		0x05, 0x05, 0x01, 0x01, 0x07, 0x07, 0x03, 0x03, 0x05, 0x05, 0x01, 0x01, 0x07, 0x07, 0x03, 0x03,
		0x08, 0x08, 0x00, 0x00, 0x0a, 0x0a, 0x02, 0x02, 0x08, 0x08, 0x00, 0x00, 0x0a, 0x0a, 0x02, 0x02,
		0x08, 0x08, 0x00, 0x00, 0x0b, 0x0b, 0x03, 0x03, 0x08, 0x08, 0x00, 0x00, 0x0b, 0x0b, 0x03, 0x03,
		0x01, 0x01, 0x05, 0x05, 0x00, 0x00, 0x04, 0x04, 0x05, 0x05, 0x01, 0x01, 0x04, 0x04, 0x00, 0x00,
		0x01, 0x01, 0x05, 0x05, 0x01, 0x01, 0x05, 0x05, 0x05, 0x05, 0x01, 0x01, 0x05, 0x05, 0x01, 0x01,
		0x0c, 0x0c, 0x04, 0x04, 0x0c, 0x0c, 0x04, 0x04, 0x08, 0x08, 0x00, 0x00, 0x08, 0x08, 0x00, 0x00,
		0x0c, 0x0c, 0x04, 0x04, 0x0d, 0x0d, 0x05, 0x05, 0x08, 0x08, 0x00, 0x00, 0x09, 0x09, 0x01, 0x01,
		0x0d, 0x0d, 0x09, 0x09, 0x0e, 0x0e, 0x0a, 0x0a, 0x0d, 0x0d, 0x09, 0x09, 0x0e, 0x0e, 0x0a, 0x0a,
		0x0d, 0x0d, 0x09, 0x09, 0x0f, 0x0f, 0x0b, 0x0b, 0x0d, 0x0d, 0x09, 0x09, 0x0f, 0x0f, 0x0b, 0x0b,
		0x0a, 0x0a, 0x02, 0x02, 0x08, 0x08, 0x00, 0x00, 0x0a, 0x0a, 0x02, 0x02, 0x08, 0x08, 0x00, 0x00,
		0x0a, 0x0a, 0x02, 0x02, 0x09, 0x09, 0x01, 0x01, 0x0a, 0x0a, 0x02, 0x02, 0x09, 0x09, 0x01, 0x01,
		0x09, 0x09, 0x0d, 0x0d, 0x08, 0x08, 0x0c, 0x0c, 0x0d, 0x0d, 0x09, 0x09, 0x0c, 0x0c, 0x08, 0x08,
		0x09, 0x09, 0x0d, 0x0d, 0x09, 0x09, 0x0d, 0x0d, 0x0d, 0x0d, 0x09, 0x09, 0x0d, 0x0d, 0x09, 0x09,
		0x0e, 0x0e, 0x06, 0x06, 0x0e, 0x0e, 0x06, 0x06, 0x0a, 0x0a, 0x02, 0x02, 0x0a, 0x0a, 0x02, 0x02,
		0x0e, 0x0e, 0x06, 0x06, 0x0f, 0x0f, 0x07, 0x07, 0x0a, 0x0a, 0x02, 0x02, 0x0b, 0x0b, 0x03, 0x03,
	};

	return prot_lut_table[in];
}

static INT32 IchirjInit()
{
	return SegaC2Init(ichirj_protection_callback);
}

struct BurnDriver BurnDrvIchirj = {
	"ichirj", "ichir", NULL, NULL, "1994",
	"Puzzle & Action: Ichidant-R (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, ichirjRomInfo, ichirjRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	IchirjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puzzle & Action: Ichidant-R (Japan) (bootleg)

static struct BurnRomInfo ichirjblRomDesc[] = {
	{ "27c4000.2",				0x80000, 0x5a194f44, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "27c4000.1",				0x80000, 0xde209f84, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16888",				0x80000, 0x85d73722, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16887",				0x80000, 0xbc3bbf25, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(ichirjbl)
STD_ROM_FN(ichirjbl)

struct BurnDriver BurnDrvIchirjbl = {
	"ichirjbl", "ichir", NULL, NULL, "1994",
	"Puzzle & Action: Ichidant-R (Japan) (bootleg)\0", NULL, "bootleg", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MINIGAMES | GBF_PUZZLE, 0,
	NULL, ichirjblRomInfo, ichirjblRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, IchirDIPInfo,
	IchirInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Puyo Puyo 2 (Japan)

static struct BurnRomInfo puyopuy2RomDesc[] = {
	{ "epr-17241.ic32",			0x80000, 0x1cad1149, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-17240.ic31",			0x80000, 0xbeecf96d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-17239.ic4",			0x80000, 0x020ff6ef, 2 | BRF_SND },           //  2 UPD Samples
};

STD_ROM_PICK(puyopuy2)
STD_ROM_FN(puyopuy2)

static UINT8 puyopuy2_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x00, 0x03, 0x00, 0x03, 0x08, 0x0b, 0x08, 0x0b, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03,
		0x0c, 0x0f, 0x04, 0x07, 0x04, 0x07, 0x0c, 0x0f, 0x0c, 0x0f, 0x04, 0x07, 0x0c, 0x0f, 0x04, 0x07,
		0x00, 0x03, 0x00, 0x03, 0x08, 0x0b, 0x08, 0x0b, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01,
		0x0c, 0x0f, 0x04, 0x07, 0x04, 0x07, 0x0c, 0x0f, 0x0e, 0x0d, 0x06, 0x05, 0x0e, 0x0d, 0x06, 0x05,
		0x04, 0x01, 0x04, 0x01, 0x0c, 0x09, 0x0c, 0x09, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01,
		0x09, 0x0c, 0x00, 0x05, 0x01, 0x04, 0x08, 0x0d, 0x09, 0x0c, 0x00, 0x05, 0x09, 0x0c, 0x00, 0x05,
		0x04, 0x01, 0x04, 0x01, 0x0c, 0x09, 0x0c, 0x09, 0x06, 0x03, 0x06, 0x03, 0x06, 0x03, 0x06, 0x03,
		0x09, 0x0c, 0x00, 0x05, 0x01, 0x04, 0x08, 0x0d, 0x0b, 0x0e, 0x02, 0x07, 0x0b, 0x0e, 0x02, 0x07,
		0x05, 0x07, 0x05, 0x07, 0x0d, 0x0f, 0x0d, 0x0f, 0x05, 0x07, 0x05, 0x07, 0x05, 0x07, 0x05, 0x07,
		0x0d, 0x0f, 0x05, 0x07, 0x05, 0x07, 0x0d, 0x0f, 0x0d, 0x0f, 0x05, 0x07, 0x0d, 0x0f, 0x05, 0x07,
		0x05, 0x07, 0x05, 0x07, 0x0d, 0x0f, 0x0d, 0x0f, 0x07, 0x05, 0x07, 0x05, 0x07, 0x05, 0x07, 0x05,
		0x0d, 0x0f, 0x05, 0x07, 0x05, 0x07, 0x0d, 0x0f, 0x0f, 0x0d, 0x07, 0x05, 0x0f, 0x0d, 0x07, 0x05,
		0x01, 0x05, 0x01, 0x05, 0x09, 0x0d, 0x09, 0x0d, 0x01, 0x05, 0x01, 0x05, 0x01, 0x05, 0x01, 0x05,
		0x08, 0x0c, 0x01, 0x05, 0x00, 0x04, 0x09, 0x0d, 0x08, 0x0c, 0x01, 0x05, 0x08, 0x0c, 0x01, 0x05,
		0x01, 0x05, 0x01, 0x05, 0x09, 0x0d, 0x09, 0x0d, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07,
		0x08, 0x0c, 0x01, 0x05, 0x00, 0x04, 0x09, 0x0d, 0x0a, 0x0e, 0x03, 0x07, 0x0a, 0x0e, 0x03, 0x07 
	};

	return prot_lut_table[in];
}

static INT32 Puyopuy2Init()
{
	return SegaC2Init(puyopuy2_protection_callback);
}

struct BurnDriver BurnDrvPuyopuy2 = {
	"puyopuy2", NULL, NULL, NULL, "1994",
	"Puyo Puyo 2 (Japan)\0", NULL, "Compile (Sega license)", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, puyopuy2RomInfo, puyopuy2RomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, Puyopuy2DIPInfo,
	Puyopuy2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Zunzunkyou no Yabou (Japan)

static struct BurnRomInfo zunkyouRomDesc[] = {
	{ "epr-16812.ic32",			0x80000, 0xeb088fb0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-16811.ic31",			0x80000, 0x9ac7035b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-16814.ic34",			0x80000, 0x821b3b77, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-16813.ic33",			0x80000, 0x3cba9e8f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-16810.ic4",			0x80000, 0xd542f0fe, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(zunkyou)
STD_ROM_FN(zunkyou)

static UINT8 zunkyou_protection_callback(UINT8 in)
{
	static const UINT8 prot_lut_table[0x100] = {
		0x0a, 0x00, 0x0a, 0x00, 0x06, 0x0c, 0x06, 0x0c, 0x08, 0x02, 0x08, 0x02, 0x00, 0x0a, 0x00, 0x0a,
		0x0e, 0x0c, 0x0e, 0x0c, 0x02, 0x00, 0x02, 0x00, 0x0e, 0x0c, 0x0e, 0x0c, 0x06, 0x04, 0x06, 0x04,
		0x0a, 0x02, 0x0a, 0x02, 0x06, 0x0e, 0x06, 0x0e, 0x08, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x08,
		0x0a, 0x0a, 0x0a, 0x0a, 0x06, 0x06, 0x06, 0x06, 0x0a, 0x0a, 0x0a, 0x0a, 0x02, 0x02, 0x02, 0x02,
		0x03, 0x09, 0x02, 0x08, 0x07, 0x0d, 0x06, 0x0c, 0x01, 0x0b, 0x00, 0x0a, 0x01, 0x0b, 0x00, 0x0a,
		0x07, 0x05, 0x06, 0x04, 0x03, 0x01, 0x02, 0x00, 0x07, 0x05, 0x06, 0x04, 0x07, 0x05, 0x06, 0x04,
		0x03, 0x0b, 0x02, 0x0a, 0x07, 0x0f, 0x06, 0x0e, 0x01, 0x09, 0x00, 0x08, 0x01, 0x09, 0x00, 0x08,
		0x03, 0x03, 0x02, 0x02, 0x07, 0x07, 0x06, 0x06, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03, 0x02, 0x02,
		0x0b, 0x01, 0x0b, 0x01, 0x07, 0x0d, 0x07, 0x0d, 0x09, 0x03, 0x09, 0x03, 0x01, 0x0b, 0x01, 0x0b,
		0x0f, 0x0d, 0x0f, 0x0d, 0x03, 0x01, 0x03, 0x01, 0x0f, 0x0d, 0x0f, 0x0d, 0x07, 0x05, 0x07, 0x05,
		0x0a, 0x02, 0x0a, 0x02, 0x06, 0x0e, 0x06, 0x0e, 0x08, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x08,
		0x0a, 0x0a, 0x0a, 0x0a, 0x06, 0x06, 0x06, 0x06, 0x0a, 0x0a, 0x0a, 0x0a, 0x02, 0x02, 0x02, 0x02,
		0x02, 0x08, 0x03, 0x09, 0x06, 0x0c, 0x07, 0x0d, 0x00, 0x0a, 0x01, 0x0b, 0x00, 0x0a, 0x01, 0x0b,
		0x06, 0x04, 0x07, 0x05, 0x02, 0x00, 0x03, 0x01, 0x06, 0x04, 0x07, 0x05, 0x06, 0x04, 0x07, 0x05,
		0x03, 0x0b, 0x02, 0x0a, 0x07, 0x0f, 0x06, 0x0e, 0x01, 0x09, 0x00, 0x08, 0x01, 0x09, 0x00, 0x08,
		0x03, 0x03, 0x02, 0x02, 0x07, 0x07, 0x06, 0x06, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03, 0x02, 0x02,
	};

	return prot_lut_table[in];
}

static INT32 ZunkyouInit()
{
	return SegaC2Init(zunkyou_protection_callback);
}

struct BurnDriver BurnDrvZunkyou = {
	"zunkyou", NULL, NULL, NULL, "1994",
	"Zunzunkyou no Yabou (Japan)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zunkyouRomInfo, zunkyouRomName, NULL, NULL, NULL, NULL, SegaC2_2ButtonInputInfo, ZunkyouDIPInfo,
	ZunkyouInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


// Monita to Rimoko no Head On Channel (prototype, hack)

static struct BurnRomInfo headonchRomDesc[] = {
	{ "headonch.ic32",			0x80000, 0x091cf538, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "headonch.ic31",			0x80000, 0x91f3b5f1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "headonch.ic34",			0x80000, 0xd8dc6323, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "headonch.ic33",			0x80000, 0x3268e38b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "headonch.ic4",			0x40000, 0x90af7301, 2 | BRF_SND },           //  4 upd
};

STD_ROM_PICK(headonch)
STD_ROM_FN(headonch)

struct BurnDriver BurnDrvHeadonch = {
	"headonch", NULL, NULL, NULL, "1994",
	"Monita to Rimoko no Head On Channel (prototype, hack)\0", NULL, "hack", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HACK, 2, HARDWARE_SEGA_MISC, GBF_MAZE | GBF_ACTION, 0,
	NULL, headonchRomInfo, headonchRomName, NULL, NULL, NULL, NULL, SegaC2_1ButtonInputInfo, HeadonchDIPInfo,
	NoProtectionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 224, 4, 3
};


/*
// Soreike! Anpanman Popcorn Koujou (Rev B)

static struct BurnRomInfo anpanmanRomDesc[] = {
	{ "epr-14804b.ic32",		0x40000, 0x7ce88c49, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-14803b.ic31",		0x40000, 0xeb3ca1b9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14806.ic34",			0x40000, 0x40f398db, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14805.ic33",			0x40000, 0xf27229ed, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14807.ic4",			0x40000, 0x9827549f, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(anpanman)
STD_ROM_FN(anpanman)

struct BurnDriverD BurnDrvAnpanman = {
	"anpanman", NULL, NULL, NULL, "1992",
	"Soreike! Anpanman Popcorn Koujou (Rev B)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, anpanmanRomInfo, anpanmanRomName, NULL, NULL, NULL, NULL, AnpanmanInputInfo, AnpanmanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Soreike! Anpanman Popcorn Koujou (Rev A)

static struct BurnRomInfo anpanmanaRomDesc[] = {
	{ "epr-14804a.ic32",		0x40000, 0xa80bd024, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-14803a.ic31",		0x40000, 0x32e1f248, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-14806.ic34",			0x40000, 0x40f398db, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-14805.ic33",			0x40000, 0xf27229ed, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-14807.ic4",			0x40000, 0x9827549f, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(anpanmana)
STD_ROM_FN(anpanmana)

struct BurnDriverD BurnDrvAnpanmana = {
	"anpanmana", "anpanman", NULL, NULL, "1992",
	"Soreike! Anpanman Popcorn Koujou (Rev A)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING| BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, anpanmanRomaInfo, anpanmanaRomName, NULL, NULL, NULL, NULL, AnpanmanInputInfo, AnpanmanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// SegaSonic Popcorn Shop (Rev B)

static struct BurnRomInfo sonicpopRomDesc[] = {
	{ "epr-14592b.ic32",		0x40000, 0xbac586a1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-15491b.ic31",		0x40000, 0x527106c3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-15494.ic34",			0x40000, 0x0520df5e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-15493.ic33",			0x40000, 0xd51b3b85, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-15495.ic4",			0x40000, 0xd3ee4c68, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(sonicpop)
STD_ROM_FN(sonicpop)

struct BurnDriverD BurnDrvSonicpop = {
	"sonicpop", NULL, NULL, NULL, "1993",
	"SegaSonic Popcorn Shop (Rev B)\0", NULL, "Sega", "C2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, sonicpopRomInfo, sonicpopRomName, NULL, NULL, NULL, NULL, SonicpopInputInfo, SonicpopDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Print Club (Japan Vol.1)

static struct BurnRomInfo pclubjRomDesc[] = {
	{ "epr18171.32",			0x80000, 0x6c8eb8e2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr18170.31",			0x80000, 0x72c631e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr18173.34",			0x80000, 0x9809dc72, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr18172.33",			0x80000, 0xc61d819b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr18169.4",				0x80000, 0x5c00ccfb, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(pclubj)
STD_ROM_FN(pclubj)

struct BurnDriver BurnDrvPclubj = {
	"pclubj", NULL, NULL, NULL, "1995",
	"Print Club (Japan Vol.1)\0", NULL, "Atlus", "C2",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, pclubjRomInfo, pclubjRomName, NULL, NULL, NULL, NULL, PclubInputInfo, PclubDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Print Club (Japan Vol.2)

static struct BurnRomInfo pclubjv2RomDesc[] = {
	{ "p2jwn.u32",				0x80000, 0xdfc0f7f1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p2jwn.u31",				0x80000, 0x6ab4c694, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p2jwn.u34",				0x80000, 0x854fd456, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p2jwn.u33",				0x80000, 0x64428a69, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr18169.4",				0x80000, 0x5c00ccfb, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(pclubjv2)
STD_ROM_FN(pclubjv2)

struct BurnDriverD BurnDrvPclubjv2 = {
	"pclubjv2", NULL, NULL, NULL, "1995",
	"Print Club (Japan Vol.2)\0", NULL, "Atlus", "C2",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, pclubjv2RomInfo, pclubjv2RomName, NULL, NULL, NULL, NULL, Pclubjv2InputInfo, Pclubjv2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Print Club (Japan Vol.4)

static struct BurnRomInfo pclubjv4RomDesc[] = {
	{ "p4jsm.u32",				0x80000, 0x36ff5f80, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p4jsm.u31",				0x80000, 0xf3c021ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p4jsm.u34",				0x80000, 0xd0fd4b33, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p4jsm.u33",				0x80000, 0xec667875, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-18169.ic4",			0x80000, 0x5c00ccfb, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(pclubjv4)
STD_ROM_FN(pclubjv4)

struct BurnDriverD BurnDrvPclubjv4 = {
	"pclubjv4", NULL, NULL, NULL, "1996",
	"Print Club (Japan Vol.4)\0", NULL, "Atlus", "C2",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, pclubjv4RomInfo, pclubjv4RomName, NULL, NULL, NULL, NULL, Pclubjv2InputInfo, Pclubjv2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Print Club (Japan Vol.5)

static struct BurnRomInfo pclubjv5RomDesc[] = {
	{ "p5jat.u32",				0x80000, 0x72220e69, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p5jat.u31",				0x80000, 0x06d83fde, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p5jat.u34",				0x80000, 0xb172ab58, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p5jat.u33",				0x80000, 0xba38ec50, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-18169.ic4",			0x80000, 0x5c00ccfb, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(pclubjv5)
STD_ROM_FN(pclubjv5)

struct BurnDriverD BurnDrvPclubjv5 = {
	"pclubjv5", NULL, NULL, NULL, "1996",
	"Print Club (Japan Vol.5)\0", NULL, "Atlus", "C2",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, pclubjv5RomInfo, pclubjv5RomName, NULL, NULL, NULL, NULL, Pclubjv2InputInfo, Pclubjv2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};


// Print Club (Japan Vol.1)

static struct BurnRomInfo pclubRomDesc[] = {
	{ "epr-ic32.32",			0x80000, 0x3fe9bda7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-ic31.31",			0x80000, 0x90f994d0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-ic34.34",			0x80000, 0x4d1ebb55, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-ic33.33",			0x80000, 0xbdfdc797, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-ic4.4",				0x80000, 0x84eed1c4, 2 | BRF_SND },           //  4 UPD Samples
};

STD_ROM_PICK(pclub)
STD_ROM_FN(pclub)

struct BurnDriverD BurnDrvPclub = {
	"pclub", NULL, NULL, NULL, "1995",
	"Print Club (Japan Vol.1)\0", NULL, "Atlus", "C2",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, pclubRomInfo, pclubRomName, NULL, NULL, NULL, NULL, PclubInputInfo, PclubDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0,
	320, 224, 4, 3
};
*/
