// FB Alpha Irem M107 system driver
// Based on MAME driver by Bryan McPhail
// Based on port of M92 driver from MAME by OopsWare

#include "tiles_generic.h"
#include "burn_ym2151.h"
#include "nec_intf.h"
#include "irem_cpu.h"
#include "iremga20.h"

static UINT8 *Mem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT8 *DrvV33ROM;
static UINT8 *DrvV30ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSprTable;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvV33RAM;
static UINT8 *DrvV30RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *RamPrioBitmap;

static UINT32 *DrvPalette;
static UINT8 bRecalcPalette;

static UINT8 *pf_control[4];
static UINT8 *sound_status;
static UINT8 *sound_latch;

static INT32 sprite_enable;
static INT32 raster_irq_position;
static INT32 sound_cpu_reset;

static INT32 nBankswitchData;
static INT32 has_bankswitch = 0;

static UINT8 DrvButton[8];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInput[8];
static UINT8 DrvReset;

static INT32 config_cpu_speed;
static INT32 spritesystem;
static INT32 vblank;
static INT32 irq_vectorbase;
static INT32 graphics_mask[2];
static INT32 nPrevScreenPos = 0;

typedef struct _m107_layer m107_layer;
struct _m107_layer
{
	INT32 enable;
	INT32 wide;
	INT32 enable_rowscroll;

	UINT16 scrollx;
	UINT16 scrolly;

	UINT16 *scroll;
	UINT16 *vram;
};

static struct _m107_layer *m107_layers[4];

enum { VECTOR_INIT, YM2151_ASSERT, YM2151_CLEAR, V30_ASSERT, V30_CLEAR };

static struct BurnInputInfo FirebarrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvButton + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvButton + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvButton + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"		},
};

STDINPUTINFO(Firebarr)

static struct BurnInputInfo Dsoccr94InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvButton + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvButton + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvButton + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvButton + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	}	,
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 2"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvButton + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInput + 5,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInput + 6,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvInput + 7,	"dip"		},
};

STDINPUTINFO(Dsoccr94)

static struct BurnDIPInfo FirebarrDIPList[]=
{
	{0x14, 0xff, 0xff, 0xfb, NULL						},
	{0x15, 0xff, 0xff, 0xa3, NULL						},
	{0x16, 0xff, 0xff, 0xfd, NULL						},

	{0   , 0xfe, 0   ,    4, "Rapid Fire"					},
	{0x14, 0x01, 0x0c, 0x00, "Button 1 Normal, Button 3 Rapid Fire"		},
	{0x14, 0x01, 0x0c, 0x04, "Button 1 Rapid Fire, Button 3 No Function"	},
	{0x14, 0x01, 0x0c, 0x08, "Off"						},
	{0x14, 0x01, 0x0c, 0x0c, "Off"						},

	{0   , 0xfe, 0   ,    2, "Continuous Play"				},
	{0x14, 0x01, 0x10, 0x10, "Off"						},
	{0x14, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x15, 0x01, 0x03, 0x00, "2"						},
	{0x15, 0x01, 0x03, 0x03, "3"						},
	{0x15, 0x01, 0x03, 0x02, "4"						},
	{0x15, 0x01, 0x03, 0x01, "5"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x15, 0x01, 0x20, 0x00, "No"						},
	{0x15, 0x01, 0x20, 0x20, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x15, 0x01, 0x40, 0x40, "Off"						},
	{0x15, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x15, 0x01, 0x80, 0x80, "Off"						},
	{0x15, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x16, 0x01, 0x01, 0x01, "Off"						},
	{0x16, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Slots"					},
	{0x16, 0x01, 0x04, 0x04, "Common"					},
	{0x16, 0x01, 0x04, 0x00, "Separate"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x16, 0x01, 0x08, 0x08, "1"						},
	{0x16, 0x01, 0x08, 0x00, "2"						},


	{0   , 0xfe, 0   ,    16, "Coinage"					},
	{0x16, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"				},
	{0x16, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"				},
	{0x16, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"				},
	{0x16, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"				},
	{0x16, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"				},
	{0x16, 0x01, 0xf0, 0x10, "2 Coins to Start/1 to Continue"		},
	{0x16, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"				},
	{0x16, 0x01, 0xf0, 0x20, "4 Coins 3 Credits"				},
	{0x16, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"				},
	{0x16, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"				},
	{0x16, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"				},
	{0x16, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"				},
	{0x16, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"				},
	{0x16, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"				},
	{0x16, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"				},
	{0x16, 0x01, 0xf0, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x16, 0x01, 0x30, 0x00, "5 Coins 1 Credits"				},
	{0x16, 0x01, 0x30, 0x10, "3 Coins 1 Credits"				},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"				},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"				},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"				},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  5 Credits"				},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"				},
};

STDDIPINFO(Firebarr)

static struct BurnDIPInfo Dsoccr94DIPList[]=
{
	{0x24, 0xff, 0xff, 0xff, NULL						},
	{0x25, 0xff, 0xff, 0xbf, NULL						},
	{0x26, 0xff, 0xff, 0xfd, NULL						},

	{0   , 0xfe, 0   ,    4, "Time"						},
	{0x25, 0x01, 0x03, 0x00, "1:30"						},
	{0x25, 0x01, 0x03, 0x03, "2:00"						},
	{0x25, 0x01, 0x03, 0x02, "2:30"						},
	{0x25, 0x01, 0x03, 0x01, "3:00"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x25, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x25, 0x01, 0x0c, 0x08, "Easy"						},
	{0x25, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x25, 0x01, 0x0c, 0x04, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Game Mode"					},
	{0x25, 0x01, 0x10, 0x10, "Match Mode"					},
	{0x25, 0x01, 0x10, 0x00, "Power Mode"					},

	{0   , 0xfe, 0   ,    2, "Starting Button"				},
	{0x25, 0x01, 0x20, 0x00, "Button 1"					},
	{0x25, 0x01, 0x20, 0x20, "Start Button"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x25, 0x01, 0x40, 0x40, "Off"						},
	{0x25, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x25, 0x01, 0x80, 0x80, "Off"						},
	{0x25, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x26, 0x01, 0x01, 0x01, "Off"						},
	{0x26, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Slots"					},
	{0x26, 0x01, 0x04, 0x04, "Common"					},
	{0x26, 0x01, 0x04, 0x00, "Separate"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x26, 0x01, 0x08, 0x08, "1"						},
	{0x26, 0x01, 0x08, 0x00, "2"						},

	{0   , 0xfe, 0   ,    16, "Coinage"					},
	{0x26, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"				},
	{0x26, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"				},
	{0x26, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"				},
	{0x26, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"				},
	{0x26, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"				},
	{0x26, 0x01, 0xf0, 0x10, "2 Coins to Start/1 to Continue"		},
	{0x26, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"				},
	{0x26, 0x01, 0xf0, 0x20, "4 Coins 3 Credits"				},
	{0x26, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"				},
	{0x26, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"				},
	{0x26, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"				},
	{0x26, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"				},
	{0x26, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"				},
	{0x26, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"				},
	{0x26, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"				},
	{0x26, 0x01, 0xf0, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x26, 0x01, 0x30, 0x00, "5 Coins 1 Credits"				},
	{0x26, 0x01, 0x30, 0x10, "3 Coins 1 Credits"				},
	{0x26, 0x01, 0x30, 0x20, "2 Coins 1 Credits"				},
	{0x26, 0x01, 0x30, 0x30, "1 Coin  1 Credits"				},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x26, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"				},
	{0x26, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"				},
	{0x26, 0x01, 0xc0, 0x40, "1 Coin  5 Credits"				},
	{0x26, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"				},
};

STDDIPINFO(Dsoccr94)

inline static UINT32 CalcCol(INT32 offs)
{
	INT32 nColour = DrvPalRAM[offs + 0] | (DrvPalRAM[offs + 1] << 8);
	INT32 r, g, b;

	r = (nColour & 0x001F) << 3;
	r |= r >> 5;
	g = (nColour & 0x03E0) >> 2;
	g |= g >> 5;
	b = (nColour & 0x7C00) >> 7;
	b |= b >> 5;

	return BurnHighCol(r, g, b, 0);
}

static void m107Bankswitch(INT32 data)
{
	INT32 bank = 0x80000 + ((data >> 1) & 0x03) * 0x20000;

	nBankswitchData = data;

	VezMapArea(0xa0000, 0xbffff, 0, DrvV33ROM + bank);
	VezMapArea(0xa0000, 0xbffff, 2, DrvV33ROM + bank);
}

static void m107YM2151IRQHandler(INT32 nStatus)
{
	if (VezGetActive() == -1) return;

	VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP0, 0xff/*default*/, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	VezRun(100);
}

static UINT8 __fastcall m107ReadByte(UINT32 address)
{
	switch (address)
	{
		case 0xf0000:
		case 0xf0001:
		case 0xf0002:
		case 0xf0003:
		case 0xf0004:
		case 0xf0005:
			return 0x00; // wpksoc
	}

//	bprintf (0, _T("rb: %5.5x\n"), address);

	return 0;
}

static void __fastcall m107WriteByte(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0xf9000 ) {
		DrvPalRAM[ address - 0xf9000] = data;
		if (address & 1) {
			INT32 offs = (address - 0xf9000) >> 1;
			DrvPalette[offs] = CalcCol( offs << 1 );
		}
		return;
	}

//	bprintf (0, _T("wb: %5.5x, %2.2x\n"), address, data);
}

static UINT8 __fastcall m107ReadPort(UINT32 port)
{
//	bprintf (0, _T("Rp: %2.2x\n"), port & 0xff);

	switch (port)
	{
		case 0x00: return  DrvInput[0];
		case 0x01: return  DrvInput[1];
		case 0x02: return (DrvInput[4] & 0x7f) | vblank;
		case 0x03: return  DrvInput[7];
		case 0x04: return  DrvInput[5];
		case 0x05: return  DrvInput[6];
		case 0x06: return  DrvInput[2];
		case 0x07: return  DrvInput[3];

		case 0x08: VezSetIRQLineAndVector(0, (irq_vectorbase + 12)/4, CPU_IRQSTATUS_NONE); return sound_status[0]; 
		case 0x09: VezSetIRQLineAndVector(0, (irq_vectorbase + 12)/4, CPU_IRQSTATUS_NONE); return sound_status[1];

		case 0x18: return 0;

		case 0xc0:
			return 0x02; // wpksoc - required to boot?

		case 0xc1:
			return 0x02; //

		case 0xc2:
		case 0xc3:
			return 0x00; // wpksoc

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to read byte value of port %x\n"), port);
	}
	return 0;
}

static void set_pf_info(INT32 layer)
{
	struct _m107_layer *ptr = m107_layers[layer];

	INT32 data = (pf_control[layer][5] << 8) + pf_control[layer][4];

	ptr->enable		= (~data >> 7) & 1;
	ptr->vram		= (UINT16*)(DrvVidRAM + (((data >> 8) & 0x0f) * 0x1000));
	ptr->enable_rowscroll	= data & 0x03;
}

static void set_pf_scroll(INT32 layer)
{
	struct _m107_layer *ptr = m107_layers[layer];

	ptr->scrollx = (pf_control[layer][2] << 0) | (pf_control[layer][3] << 8);
	ptr->scrolly = (pf_control[layer][0] << 0) | (pf_control[layer][1] << 8);
}

static void __fastcall m107WritePort(UINT32 port, UINT8 data)
{
//	bprintf (0, _T("wp: %2.2x, %2.2x\n"), port & 0xff, data);

	switch (port)
	{
		case 0x00:
			sound_latch[0] = data;
			VezClose();
			VezOpen(1);
			VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP1, 0xff/*default*/, CPU_IRQSTATUS_ACK);
			VezRun(10);
			VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP1, 0xff/*default*/, CPU_IRQSTATUS_NONE);
			VezRun(10);
			VezClose();
			VezOpen(0);
			return;

		case 0x02:
		case 0x03:
			//m107_coincounter_w
			return;

		case 0x06:
			if (has_bankswitch) {
				m107Bankswitch(data);
			}
			return;

		case 0x80: pf_control[0][0] = data; set_pf_scroll(0); return;
		case 0x81: pf_control[0][1] = data; set_pf_scroll(0); return;
		case 0x82: pf_control[0][2] = data; set_pf_scroll(0); return;
		case 0x83: pf_control[0][3] = data; set_pf_scroll(0); return;
		case 0x84: pf_control[1][0] = data; set_pf_scroll(1); return;
		case 0x85: pf_control[1][1] = data; set_pf_scroll(1); return;
		case 0x86: pf_control[1][2] = data; set_pf_scroll(1); return;
		case 0x87: pf_control[1][3] = data; set_pf_scroll(1); return;
		case 0x88: pf_control[2][0] = data; set_pf_scroll(2); return;
		case 0x89: pf_control[2][1] = data; set_pf_scroll(2); return;
		case 0x8a: pf_control[2][2] = data; set_pf_scroll(2); return;
		case 0x8b: pf_control[2][3] = data; set_pf_scroll(2); return;
		case 0x8c: pf_control[3][0] = data; set_pf_scroll(3); return;
		case 0x8d: pf_control[3][1] = data; set_pf_scroll(3); return;
		case 0x8e: pf_control[3][2] = data; set_pf_scroll(3); return;
		case 0x8f: pf_control[3][3] = data; set_pf_scroll(3); return;
		case 0x90: pf_control[0][4] = data; return;
		case 0x91: pf_control[0][5] = data; set_pf_info(0); return;
		case 0x92: pf_control[1][4] = data; return;
		case 0x93: pf_control[1][5] = data; set_pf_info(1); return;
		case 0x94: pf_control[2][4] = data; return;
		case 0x95: pf_control[2][5] = data; set_pf_info(2); return;
		case 0x96: pf_control[3][4] = data; return;
		case 0x97: pf_control[3][5] = data; set_pf_info(3); return;
		case 0x98: pf_control[0][6] = data; return;
		case 0x99: pf_control[0][7] = data; return;
		case 0x9a: pf_control[1][6] = data; return;
		case 0x9b: pf_control[1][7] = data; return;
		case 0x9c: pf_control[2][6] = data; return;
		case 0x9d: pf_control[2][7] = data; return;
		case 0x9e: pf_control[3][6] = data;
			raster_irq_position = ((pf_control[3][7]<<8) | pf_control[3][6]) - 128;
			return;
		case 0x9f: pf_control[3][7] = data;
			raster_irq_position = ((pf_control[3][7]<<8) | pf_control[3][6]) - 128;
			return;

		case 0xb0:
			memcpy (DrvSprBuf, DrvSprRAM, 0x1000);
			return;

		case 0xb1:
			sprite_enable = ~data & 0x10;
			return;

		case 0xc0:
		case 0xc1:// sound reset
			VezClose();
			VezOpen(1);
			sound_cpu_reset = (data) ? 0 : 1;
			if (sound_cpu_reset) {
				VezReset();
			}
			VezClose();
			VezOpen(0);
			return;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to port %x\n"), data, port);
	}
}

static UINT8 __fastcall m107SndReadByte(UINT32 address)
{
	if ((address & 0xfffc0) == 0xa8000) {
		return iremga20_read( 0, (address & 0x0003f) / 2 );
	}

	switch (address)
	{
		case 0xa8042:
			return BurnYM2151ReadStatus();

		case 0xa8044:
			return sound_latch[0];

		case 0xa8045:
			return 0xff; // soundlatch high bits, always 0xff

	//	default:
	//		bprintf(PRINT_NORMAL, _T("V30 Attempt to read byte value of location %x\n"), address);
	}
	return 0;
}

static void __fastcall m107SndWriteByte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc0) == 0xa8000) {
		iremga20_write( 0, (address & 0x0003f) / 2, data );
		return;
	}

	switch (address)
	{
		case 0xa8040:
			BurnYM2151SelectRegister(data);
			return;

		case 0xa8042:
			BurnYM2151WriteRegister(data);
			return;

		case 0xa8044:
	//		VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP1, 0xff/*default*/, CPU_IRQSTATUS_NONE);
			return;

		case 0xa8046:
			sound_status[0] = data;
			VezClose();
			VezOpen(0);
			VezSetIRQLineAndVector(0, (irq_vectorbase + 12)/4, CPU_IRQSTATUS_ACK);
			VezClose();
			VezOpen(1);
			return;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("V30 Attempt to write byte value %x to location %x\n"), data, address);
	}
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);

	VezOpen(0);
	VezReset();
	if (has_bankswitch) m107Bankswitch(0);
	VezClose();

	VezOpen(1);
	VezReset();
	VezClose();

	BurnYM2151Reset();
	iremga20_reset(0);

	for (INT32 i = 0; i < 4; i++)
	{
		set_pf_info(i);
		set_pf_scroll(i);
	}

	sprite_enable = 0;
	raster_irq_position = -1;
	sound_cpu_reset = 0;

	return 0;
}

static INT32 MemIndex(INT32 gfxlen1, INT32 gfxlen2)
{
	UINT8 *Next; Next = Mem;
	DrvV33ROM 	= Next; Next += 0x100000;
	DrvV30ROM	= Next; Next += 0x020000;
	DrvGfxROM0	= Next; Next += gfxlen1 * 2;
	DrvGfxROM1	= Next; Next += gfxlen2 * 2;

	if (spritesystem == 1) {
		DrvSprTable	= Next; Next += 0x040000;
	}

	DrvSndROM	= Next; Next += 0x100000;

	RamPrioBitmap	= Next; Next += 320 * 240;

	RamStart	= Next;

	DrvSprRAM	= Next; Next += 0x001000;
	DrvSprBuf	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x010000;
	DrvV33RAM	= Next; Next += 0x010000;
	DrvV30RAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x001000;

	sound_status	= Next; Next += 0x000004; // 2
	sound_latch	= Next; Next += 0x000004; // 1

	pf_control[0]	= Next; Next += 0x000008;
	pf_control[1]	= Next; Next += 0x000008;
	pf_control[2]	= Next; Next += 0x000008;
	pf_control[3]	= Next; Next += 0x000008;

	m107_layers[0]	= (struct _m107_layer*)Next; Next += sizeof(struct _m107_layer);
	m107_layers[1]	= (struct _m107_layer*)Next; Next += sizeof(struct _m107_layer);
	m107_layers[2]	= (struct _m107_layer*)Next; Next += sizeof(struct _m107_layer);
	m107_layers[3]	= (struct _m107_layer*)Next; Next += sizeof(struct _m107_layer);

	RamEnd		= Next;

	DrvPalette	= (UINT32 *) Next; Next += 0x0800 * sizeof(UINT32);

	MemEnd		= Next;
	return 0;
}

static void DrvGfxExpand(UINT8 *rom, INT32 len, INT32 type)
{
	INT32 Plane0[4]  = { 8, 0, 24, 16 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 Plane1[4]  = { RGN_FRAC(len, 3,4), RGN_FRAC(len, 2,4), RGN_FRAC(len,1,4), RGN_FRAC(len,0,4) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };
	INT32 XOffs2[16] = { STEP8(8,1), STEP8(0,1) };
	INT32 YOffs2[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, rom, len);

	if (type == 0) GfxDecode((len * 2) / 0x040, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, rom);
	if (type == 1) GfxDecode((len * 2) / 0x100, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, rom);
	if (type == 2) GfxDecode((len * 2) / 0x100, 4, 16, 16, Plane1, XOffs2, YOffs2, 0x100, tmp, rom);

	BurnFree(tmp);
}

static INT32 DrvInit(INT32 (*pRomLoadCallback)(), const UINT8 *sound_decrypt_table, INT32 gfxdecode, INT32 vectorbase, INT32 gfxlen1, INT32 gfxlen2)
{
	Mem = NULL;
	MemIndex(gfxlen1, gfxlen2);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex(gfxlen1, gfxlen2);

	if (pRomLoadCallback)
	{
		if (pRomLoadCallback()) return 1; 

		DrvGfxExpand(DrvGfxROM0, gfxlen1, 0);
		DrvGfxExpand(DrvGfxROM1, gfxlen2, gfxdecode);
	}

	{
		VezInit(0, V33_TYPE);
		VezInit(1, V35_TYPE, 14318180 /*before divider*/);

		VezOpen(0);
		VezMapArea(0x00000, 0x9ffff, 0, DrvV33ROM);
		VezMapArea(0x00000, 0x9ffff, 2, DrvV33ROM);
		VezMapArea(0xa0000, 0xbffff, 0, DrvV33ROM + 0xa0000);
		VezMapArea(0xa0000, 0xbffff, 2, DrvV33ROM + 0xa0000);
		VezMapArea(0xd0000, 0xdffff, 0, DrvVidRAM);
		VezMapArea(0xd0000, 0xdffff, 1, DrvVidRAM);
		VezMapArea(0xd0000, 0xdffff, 2, DrvVidRAM);
		VezMapArea(0xe0000, 0xeffff, 0, DrvV33RAM);
		VezMapArea(0xe0000, 0xeffff, 1, DrvV33RAM);
		VezMapArea(0xe0000, 0xeffff, 2, DrvV33RAM);
		VezMapArea(0xf8000, 0xf8fff, 0, DrvSprRAM);
		VezMapArea(0xf8000, 0xf8fff, 1, DrvSprRAM);
		VezMapArea(0xf9000, 0xf9fff, 0, DrvPalRAM);
		VezMapArea(0xff800, 0xfffff, 0, DrvV33ROM + 0x7f800);
		VezMapArea(0xff800, 0xfffff, 2, DrvV33ROM + 0x7f800);
		VezSetReadHandler(m107ReadByte);
		VezSetWriteHandler(m107WriteByte);
		VezSetReadPort(m107ReadPort);
		VezSetWritePort(m107WritePort);
		VezClose();

		VezOpen(1);
		if (sound_decrypt_table != NULL) {
			VezSetDecode((UINT8*)sound_decrypt_table);
		}
		VezMapArea(0x00000, 0x1ffff, 0, DrvV30ROM);
		VezMapArea(0x00000, 0x1ffff, 2, DrvV30ROM);
		VezMapArea(0xa0000, 0xa3fff, 0, DrvV30RAM);
		VezMapArea(0xa0000, 0xa3fff, 1, DrvV30RAM);
		VezMapArea(0xa0000, 0xa3fff, 2, DrvV30RAM);
		VezMapArea(0xff800, 0xfffff, 0, DrvV30ROM + 0x1f800);
		VezMapArea(0xff800, 0xfffff, 2, DrvV30ROM + 0x1f800);
		VezSetReadHandler(m107SndReadByte);
		VezSetWriteHandler(m107SndWriteByte);
		VezClose();
	}

	graphics_mask[0] = ((gfxlen1 * 2) - 1) / (8 * 8);
	graphics_mask[1] = ((gfxlen2 * 2) - 1) / (16 * 16);

	irq_vectorbase = vectorbase;

	BurnYM2151Init(3579545);
	YM2151SetIrqHandler(0, &m107YM2151IRQHandler);
	BurnYM2151SetAllRoutes(0.40, BURN_SND_ROUTE_BOTH);

	iremga20_init(0, DrvSndROM, 0x100000, 3579545);
	itemga20_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	iremga20_exit();

	VezExit();

	BurnFree(Mem);
	
	nPrevScreenPos = 0;
	has_bankswitch = 0;

	return 0;
}

static void RenderTilePrio(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *pri, INT32 prio)
{
	if (sx <= (0-width) || sx >= nScreenWidth || sy <= (0-height) || sy >= nScreenHeight) return;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (pxl == 0) continue;

			if ((prio & (1 << pri[sy * nScreenWidth + sx])) == 0 && pri[sy * nScreenWidth + sx] < 0x80) {
				dest[sy * nScreenWidth + sx] = pxl | color;
				pri[sy * nScreenWidth + sx] |= 0x80;
			}
		}

		sx -= width;
	}
}

static void draw_sprites()
{
	if (sprite_enable == 0) return;

	UINT16 *ram = (UINT16*)DrvSprBuf;
	int offs;
	UINT8 *rom = DrvSprTable;

	for (offs = 0;offs < 0x800;offs += 4)
	{
		int x,y,sprite,colour,fx,fy,y_multi,i,s_ptr,pri_mask;

		pri_mask = (!(ram[offs+2]&0x80)) ? 2 : 0;

		y=ram[offs+0] + 8;
		x=ram[offs+3];
		x&=0x1ff;
		y&=0x1ff;

		if (x==0 || y==0) continue; /* offscreen */

		sprite=ram[offs+1]&0x7fff;

		x = x - 16;
		y = 384 - 16 - y;

		colour=ram[offs+2]&0x7f;
		fx=(ram[offs+2]>>8)&0x1;
		fy=(ram[offs+2]>>8)&0x2;
		y_multi=(ram[offs+0]>>11)&0x3;

		if (spritesystem == 0)
		{
			y_multi=1 << y_multi; /* 1, 2, 4 or 8 */

			s_ptr = 0;
			if (!fy) s_ptr+=y_multi-1;

			for (i=0; i<y_multi; i++)
			{
				RenderTilePrio(pTransDraw, DrvGfxROM1, (sprite + s_ptr) & graphics_mask[1], colour << 4, x - 80, y-i*16, fx, fy, 16, 16, RamPrioBitmap, pri_mask);
				RenderTilePrio(pTransDraw, DrvGfxROM1, (sprite + s_ptr) & graphics_mask[1], colour << 4, x - 80, y-i*16 - 0x200, fx, fy, 16, 16, RamPrioBitmap, pri_mask);

				if (fy) s_ptr++; else s_ptr--;
			}
		}
		else
		{
			int rom_offs = sprite*8;

			if (rom[rom_offs+1] || rom[rom_offs+3] || rom[rom_offs+5] || rom[rom_offs+7])
			{
				while (rom_offs < 0x40000)  /* safety check */
				{
					int xdisp = rom[rom_offs+6]+256*rom[rom_offs+7];
					int ydisp = rom[rom_offs+2]+256*rom[rom_offs+3];
					int ffx=fx^(rom[rom_offs+1]&1);
					int ffy=fy^(rom[rom_offs+1]&2);
					sprite=rom[rom_offs+4]+256*rom[rom_offs+5];
					y_multi=1<<((rom[rom_offs+3]>>1)&0x3);
					if (fx) xdisp = -xdisp-16;
					if (fy) ydisp = -ydisp - (16*y_multi-1);
					if (!ffy) sprite+=y_multi-1;
					for (i=0; i<y_multi; i++)
					{
						RenderTilePrio(pTransDraw, DrvGfxROM1, (sprite+(ffy?i:-i)) & graphics_mask[1], colour << 4, ((x+xdisp)&0x1ff)-80,(y-ydisp-16*i)&0x1ff, ffx, ffy, 16, 16, RamPrioBitmap, pri_mask);
						RenderTilePrio(pTransDraw, DrvGfxROM1, (sprite+(ffy?i:-i)) & graphics_mask[1], colour << 4, ((x+xdisp)&0x1ff)-80,((y-ydisp-16*i)&0x1ff)-0x200, ffx, ffy, 16, 16, RamPrioBitmap, pri_mask);
					}

					if (rom[rom_offs+1]&0x80) break;    /* end of block */

					rom_offs += 8;
				}
			}
		}
	}
}

static void draw_layer_byline(INT32 start, INT32 finish, INT32 layer, INT32 forcelayer)
{
	struct _m107_layer *ptr = m107_layers[layer];

	if (ptr->enable == 0) return;

	INT32 wide = 64;
	INT32 scrolly = (ptr->scrolly + 136) & 0x1ff;
	INT32 scrollx = ((ptr->scrollx) + 80) + (3 - 2 * layer);

	INT32 transparency = (forcelayer & 2) ? ~0 : 0;
	INT32 priority = forcelayer & 1;

	UINT16 *xscroll = (UINT16*)(DrvVidRAM + (0xe000 + 0x200 * layer));
	UINT16 *yscroll = (UINT16*)(DrvVidRAM + (0xe800 + 0x200 * layer));

	for (INT32 sy = start; sy < finish; sy++)
	{
		UINT16 *dest = pTransDraw + (sy * nScreenWidth);
		UINT8  *pri  = RamPrioBitmap + (sy * nScreenWidth);

		INT32 scrolly_1 = (scrolly + sy) & 0x1ff;
		if (ptr->enable_rowscroll & 2) scrolly_1 = (scrolly_1 + BURN_ENDIAN_SWAP_INT16(yscroll[sy+8])) & 0x1ff;

		INT32 scrollx_1 = scrollx; 
		if (ptr->enable_rowscroll & 1) scrollx_1 = (scrollx_1 + BURN_ENDIAN_SWAP_INT16(xscroll[((scrolly_1 + 136 + 0xff80)-(scrolly)) & 0x1ff])) & 0x1ff;

		INT32 romoff_1 = (scrolly_1 & 0x07) << 3;

		for (INT32 sx = 0; sx < nScreenWidth + 8; sx+=8)
		{
			INT32 sxx = (scrollx_1 + sx) & 0x1ff;

			INT32 offs  = ((scrolly_1 / 8) * wide) + (((sxx) / 8) & (wide - 1));

			INT32 attr  = BURN_ENDIAN_SWAP_INT16(ptr->vram[(offs * 2) + 1]);

			INT32 group = (attr >> 9) & 1;

			if (priority == group)
			{
				INT32 code  = BURN_ENDIAN_SWAP_INT16(ptr->vram[(offs * 2) + 0]) | ((attr & 0x1000) << 4);

				if (code == 0 && transparency == 0) continue; // skip blank tiles

				INT32 color = (attr & 0x007f) << 4;

				INT32 x_xor = 0;
				INT32 romoff = romoff_1;
				if (attr & 0x0800) romoff ^= 0x38;	// flipy
				if (attr & 0x0400) x_xor = 7;		// flipx

				UINT8 *rom = DrvGfxROM0 + ((code & graphics_mask[0]) * 0x40) + romoff;

				INT32 xx = sx - (scrollx_1&0x7);

				for (INT32 x = 0; x < 8; x++, xx++) {
					if (xx < 0 || xx >= nScreenWidth) continue;

					INT32 pxl = rom[x ^ x_xor];
					if (pxl == transparency) continue;

					dest[xx] = pxl | color;
					pri[xx] = group;
				}
			}
		}
	}
}

static void DrawLayers(INT32 start, INT32 finish)
{
	memset (RamPrioBitmap + (start * nScreenWidth), 0, nScreenWidth * (finish - start)); // clear priority

	if (~pf_control[3][4] & 0x80) {

		if (~nBurnLayer & 8) memset (pTransDraw + (start * nScreenWidth), 0, nScreenWidth * (finish - start) * sizeof(INT16));

		if (nBurnLayer & 8) draw_layer_byline(start, finish, 3, 0|2);
		if (nBurnLayer & 8) draw_layer_byline(start, finish, 3, 1|2);

		if (nBurnLayer & 4) draw_layer_byline(start, finish, 2, 0);
	} else {
		if (~nBurnLayer & 4) memset (pTransDraw + (start * nScreenWidth), 0, nScreenWidth * (finish - start) * sizeof(INT16));

		if (nBurnLayer & 4) draw_layer_byline(start, finish, 2, 0 | 2);
	}

	if (nBurnLayer & 2) draw_layer_byline(start, finish, 1, 0);
	if (nBurnLayer & 1) draw_layer_byline(start, finish, 0, 0);
	if (nSpriteEnable & 4) draw_layer_byline(start, finish, 2, 1);
	if (nSpriteEnable & 2) draw_layer_byline(start, finish, 1, 1);
	if (nSpriteEnable & 1) draw_layer_byline(start, finish, 0, 1);
}

static INT32 DrvDraw()
{
	if (bRecalcPalette) {
		for (INT32 i=0; i<0x800;i++)
			DrvPalette[i] = CalcCol(i<<1);
		bRecalcPalette = 0;
	}
//	DrawLayers(0, nScreenHeight);

	if (nBurnLayer & 8) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvReDraw()
{
	if (bRecalcPalette) {
		for (INT32 i=0; i<0x800;i++)
			DrvPalette[i] = CalcCol(i<<1);
		bRecalcPalette = 0;
	}

	BurnTransferClear();

	DrawLayers(0, nScreenHeight);

	if (nBurnLayer & 8) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void scanline_interrupts(INT32 scanline)
{
	if (scanline == raster_irq_position) {

		if (scanline>=8 && scanline < 248 && nPrevScreenPos != (scanline-8)+1) {
			if (nPrevScreenPos >= 0 && nPrevScreenPos <= 239)
				DrawLayers(nPrevScreenPos, (scanline-8)+1);
			nPrevScreenPos = (scanline-8)+1;
		}

		VezSetIRQLineAndVector(0, (irq_vectorbase + 8)/4, CPU_IRQSTATUS_ACK);
		VezRun(10);
		VezSetIRQLineAndVector(0, (irq_vectorbase + 8)/4, CPU_IRQSTATUS_NONE);

	}
	else if (scanline == 248) // vblank
	{
		vblank = 0;

		if (nPrevScreenPos != 240) {
			DrawLayers(nPrevScreenPos, 240);
		}
		nPrevScreenPos = 0;

		if (pBurnDraw) {
			DrvDraw();
	//		DrvReDraw();
		}

		VezSetIRQLineAndVector(0, (irq_vectorbase + 0)/4, CPU_IRQSTATUS_ACK);
		VezRun(10);
		VezSetIRQLineAndVector(0, (irq_vectorbase + 0)/4, CPU_IRQSTATUS_NONE);
	} else if (scanline == 8) {
		vblank = 0x80;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();

	{
		memset (DrvInput, 0xff, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInput[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInput[4] ^= (DrvButton[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256; // scanlines
	INT32 nCyclesTotal[2] = { 0, 0 };
	INT32 nCyclesDone[2] = { 0, 0 };

	INT32 nSoundBufferPos = 0;
	nCyclesTotal[0] = (INT32)((INT64)(config_cpu_speed / 60) * nBurnCPUSpeedAdjust / 0x0100);
	nCyclesTotal[1] = (INT32)((INT64)(7159090 / 60) * nBurnCPUSpeedAdjust / 0x0100);

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
	}

	nInterleave = 256 * 8; // * 8 for tight sync

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		VezOpen(0);
		INT32 segment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += VezRun(segment);
		if ((i&7)==7) scanline_interrupts(i/8); // update at hblank?

		segment = (VezTotalCycles() * 7159) / (config_cpu_speed / 1000);

		VezClose();

		VezOpen(1);

		{
			if (sound_cpu_reset) {
				VezIdle(segment - VezTotalCycles());
			} else {
				nCyclesDone[1] += VezRun(segment - VezTotalCycles());
			}
		}

		if (pBurnSoundOut && (i&7)==7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			iremga20_update(0, pSoundBuf, nSegmentLength);
			
			nSoundBufferPos += nSegmentLength;
		}

		VezClose();
	}

	VezOpen(1);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			iremga20_update(0, pSoundBuf, nSegmentLength);
		}
	}
	
	VezClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	if (pnMin)
	{
		*pnMin =  0x029671;
	}

	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM)
	{	
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		VezScan(nAction);

		iremga20_scan(0, nAction, pnMin);
		BurnYM2151Scan(nAction);

		SCAN_VAR(raster_irq_position);
		SCAN_VAR(sound_cpu_reset);
		SCAN_VAR(sprite_enable);
		SCAN_VAR(nBankswitchData);
	}

	if (nAction & ACB_WRITE) {
		bRecalcPalette = 1;

		for (INT32 i = 0; i < 4; i++) {
			set_pf_scroll(i);
			set_pf_info(i);
		}

		if (has_bankswitch)
		{
			VezOpen(0);
			m107Bankswitch(nBankswitchData);
			VezClose();
		}

		VezOpen(1);
		m107YM2151IRQHandler(0);
		VezClose();
	}

	return 0;
}



// Air Assault (World)

static struct BurnRomInfo airassRomDesc[] = {
	{ "f4-a-h0-etc.h0",	0x040000, 0x038f2cbd, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "f4-a-l0-etc.l0",	0x040000, 0xd3eb7842, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f4-a-h1-ss.h1",	0x020000, 0x4cb1c9ae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "f4-a-l1-ss.l1",	0x020000, 0x1ddd192d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "f4-b-sh0-c.sh0",	0x010000, 0x31c05c0d, 2 | BRF_PRG | BRF_ESS }, //  4 V35 Code
	{ "f4-b-sl0-c.sl0",	0x010000, 0x60a0d33a, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "w45.c00",		0x100000, 0x2aab419e, 3 | BRF_GRA },           //  6 Background Tiles
	{ "w46.c10",		0x100000, 0xd6e5c910, 3 | BRF_GRA },           //  7

	{ "w47.000",		0x100000, 0x72e1a253, 4 | BRF_GRA },           //  8 Sprite Tiles
	{ "w48.010",		0x100000, 0x1746b7f6, 4 | BRF_GRA },           //  9
	{ "w49.020",		0x100000, 0x17b5caf2, 4 | BRF_GRA },           // 10
	{ "w50.030",		0x100000, 0x63e4bec3, 4 | BRF_GRA },           // 11

	{ "f4-b-drh-.drh",	0x020000, 0x12001372, 5 | BRF_GRA },           // 12 Sprite Tables
	{ "f4-b-drl-.drl",	0x020000, 0x08cb7533, 5 | BRF_GRA },           // 13

	{ "w96.da0",		0x080000, 0x7a493e2e, 6 | BRF_SND },           // 14 Irem GA20 Samples
};

STD_ROM_PICK(airass)
STD_ROM_FN(airass)

static INT32 airassRomLoad()
{
	if (BurnLoadRom(DrvV33ROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080001,  2, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080000,  3, 2)) return 1;

	if (BurnLoadRom(DrvV30ROM  + 0x000001,  4, 2)) return 1;
	if (BurnLoadRom(DrvV30ROM  + 0x000000,  5, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 11, 1)) return 1;

	if (BurnLoadRom(DrvSprTable + 0x00001, 12, 2)) return 1;
	if (BurnLoadRom(DrvSprTable + 0x00000, 13, 2)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x000000, 14, 1)) return 1;

	return 0;
}

static INT32 airassInit()
{
	spritesystem = 1;
	config_cpu_speed = 14000000;
	return DrvInit(airassRomLoad, gunforce_decryption_table, 1, 0x20, 0x200000, 0x400000);
}

struct BurnDriver BurnDrvAirass = {
	"airass", NULL, NULL, NULL, "1993",
	"Air Assault (World)\0", NULL, "Irem", "M107",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_MISC, GBF_VERSHOOT, 0,
	NULL, airassRomInfo, airassRomName, NULL, NULL, FirebarrInputInfo, FirebarrDIPInfo,
	airassInit, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	240, 320, 3, 4
};


// Fire Barrel (Japan)

static struct BurnRomInfo firebarrRomDesc[] = {
	{ "f4-a-h0-c.h0",	0x40000, 0x2aa5676e, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "f4-a-l0-c.l0",	0x40000, 0x42f75d59, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f4-a-h1-c.h1",	0x20000, 0xbb7f6968, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "f4-a-l1-c.l1",	0x20000, 0x9d57edd6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "f4-b-sh0-b.sh0",	0x10000, 0x30a8e232, 2 | BRF_PRG | BRF_ESS }, //  4 V35 Code
	{ "f4-b-sl0-b.sl0",	0x10000, 0x204b5f1f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "f4-c00.c00",		0x80000, 0x50cab384, 3 | BRF_GRA },           //  6 Background Tiles
	{ "f4-c10.c10",		0x80000, 0x330c6df2, 3 | BRF_GRA },           //  7
	{ "f4-c01.c01",		0x80000, 0x12a698c8, 3 | BRF_GRA },           //  8
	{ "f4-c11.c11",		0x80000, 0x3f9add18, 3 | BRF_GRA },           //  9

	{ "f4-000.000",		0x80000, 0x920deee9, 4 | BRF_GRA },           // 10 Sprite Tiles
	{ "f4-001.001",		0x80000, 0xe5725eaf, 4 | BRF_GRA },           // 11
	{ "f4-010.010",		0x80000, 0x3505d185, 4 | BRF_GRA },           // 12
	{ "f4-011.011",		0x80000, 0x1912682f, 4 | BRF_GRA },           // 13
	{ "f4-020.020",		0x80000, 0xec130b8e, 4 | BRF_GRA },           // 14
	{ "f4-021.021",		0x80000, 0x8dd384dc, 4 | BRF_GRA },           // 15
	{ "f4-030.030",		0x80000, 0x7e7b30cd, 4 | BRF_GRA },           // 16
	{ "f4-031.031",		0x80000, 0x83ac56c5, 4 | BRF_GRA },           // 17

	{ "f4-b-drh-.drh",	0x20000, 0x12001372, 5 | BRF_GRA },           // 18 Sprite Tables
	{ "f4-b-drl-.drl",	0x20000, 0x08cb7533, 5 | BRF_GRA },           // 19

	{ "f4-b-da0.da0",	0x80000, 0x7a493e2e, 6 | BRF_SND },           // 20 Irem GA20 Samples
};

STD_ROM_PICK(firebarr)
STD_ROM_FN(firebarr)

static INT32 firebarrRomLoad()
{
	if (BurnLoadRom(DrvV33ROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080001,  2, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080000,  3, 2)) return 1;

	if (BurnLoadRom(DrvV30ROM  + 0x000001,  4, 2)) return 1;
	if (BurnLoadRom(DrvV30ROM  + 0x000000,  5, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x100000,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x100001,  9, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x000001, 11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100001, 13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200001, 15, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 16, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300001, 17, 2)) return 1;

	if (BurnLoadRom(DrvSprTable + 0x00001, 18, 2)) return 1;
	if (BurnLoadRom(DrvSprTable + 0x00000, 19, 2)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x000000, 20, 1)) return 1;

	return 0;
}

static INT32 firebarrInit()
{
	spritesystem = 1;
	config_cpu_speed = 14000000;
	return DrvInit(firebarrRomLoad, rtypeleo_decryption_table, 2, 0x20, 0x200000, 0x400000);
}

struct BurnDriver BurnDrvFirebarr = {
	"firebarr", "airass", NULL, NULL, "1993",
	"Fire Barrel (Japan)\0", NULL, "Irem", "M107",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_MISC, GBF_VERSHOOT, 0,
	NULL, firebarrRomInfo, firebarrRomName, NULL, NULL, FirebarrInputInfo, FirebarrDIPInfo,
	firebarrInit, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	240, 320, 3, 4
};


// Dream Soccer '94 (World, M107 hardware)

static struct BurnRomInfo dsoccr94RomDesc[] = {
	{ "a3-4p_h0-c-0.ic59",	0x040000, 0xd01d3fd7, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "a3-4p_l0-c-0.ic61",	0x040000, 0x8af0afe2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a3_h1-c-0.ic60",		0x040000, 0x6109041b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a3_l1-c-0.ic62",		0x040000, 0x97a01f6b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a3-sh0-c-0.ic31",	0x010000, 0x23fe6ffc, 2 | BRF_PRG | BRF_ESS }, //  4 V35 Code
	{ "a3-sl0-c-0.ic37",	0x010000, 0x768132e5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ds_c00.ic29",		0x100000, 0x2d31d418, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ds_c10.ic28",		0x100000, 0x57f7bcd3, 3 | BRF_GRA },           //  7
	{ "ds_c01.ic21",		0x100000, 0x9d31a464, 3 | BRF_GRA },           //  8
	{ "ds_c11.ic20",		0x100000, 0xa372e79f, 3 | BRF_GRA },           //  9

	{ "ds_000.ic11",		0x100000, 0x366b3e29, 4 | BRF_GRA },           // 10 Sprite Tiles
	{ "ds_010.ic12",		0x100000, 0x28a4cc40, 4 | BRF_GRA },           // 11
	{ "ds_020.ic13",		0x100000, 0x5a310f7f, 4 | BRF_GRA },           // 12
	{ "ds_030.ic14",		0x100000, 0x328b1f45, 4 | BRF_GRA },           // 13

	{ "ds_da0.ic24",		0x100000, 0x67fc52fd, 5 | BRF_SND },           // 14 Irem GA20 Samples
};

STD_ROM_PICK(dsoccr94)
STD_ROM_FN(dsoccr94)

static INT32 dsoccr94RomLoad()
{
	if (BurnLoadRom(DrvV33ROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080001,  2, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x080000,  3, 2)) return 1;

	if (BurnLoadRom(DrvV30ROM  + 0x000001,  4, 2)) return 1;
	if (BurnLoadRom(DrvV30ROM  + 0x000000,  5, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200000,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200001,  9, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 13, 1)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x000000, 14, 1)) return 1;

	return 0;
}

static INT32 dsoccr94Init()
{
	has_bankswitch = 1;
	spritesystem = 0;
	config_cpu_speed = 10000000;
	return DrvInit(dsoccr94RomLoad, dsoccr94_decryption_table, 1, 0x80, 0x400000, 0x400000);
}

struct BurnDriver BurnDrvDsoccr94 = {
	"dsoccr94", NULL, NULL, NULL, "1994",
	"Dream Soccer '94 (World, M107 hardware)\0", NULL, "Irem (Data East Corporation license)", "M107",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IREM_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, dsoccr94RomInfo, dsoccr94RomName, NULL, NULL, Dsoccr94InputInfo, Dsoccr94DIPInfo,
	dsoccr94Init, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	320, 240, 4, 3
};


// Dream Soccer '94 (Korea, M107 hardware)
// default team selected is Korea, so likely a Korean set

static struct BurnRomInfo dsoccr94kRomDesc[] = {
	{ "ic59_h0.bin",		0x040000, 0x7b26d8a3, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "ic61_l0.bin",		0x040000, 0xb13f0ff4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic60_h1.bin",		0x040000, 0x6109041b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic62_l1.bin",		0x040000, 0x97a01f6b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a3-sh0-c-0.ic31",	0x010000, 0x23fe6ffc, 2 | BRF_PRG | BRF_ESS }, //  4 V35 Code
	{ "a3-sl0-c-0.ic37",	0x010000, 0x768132e5, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ds_c00.ic29",		0x100000, 0x2d31d418, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ds_c10.ic28",		0x100000, 0x57f7bcd3, 3 | BRF_GRA },           //  7
	{ "ds_c01.ic21",		0x100000, 0x9d31a464, 3 | BRF_GRA },           //  8
	{ "ds_c11.ic20",		0x100000, 0xa372e79f, 3 | BRF_GRA },           //  9

	{ "ds_000.ic11",		0x100000, 0x366b3e29, 4 | BRF_GRA },           // 10 Sprite Tiles
	{ "ds_010.ic12",		0x100000, 0x28a4cc40, 4 | BRF_GRA },           // 11
	{ "ds_020.ic13",		0x100000, 0x5a310f7f, 4 | BRF_GRA },           // 12
	{ "ds_030.ic14",		0x100000, 0x328b1f45, 4 | BRF_GRA },           // 13

	{ "ds_da0.ic24",		0x100000, 0x67fc52fd, 5 | BRF_SND },           // 14 Irem GA20 Samples
};

STD_ROM_PICK(dsoccr94k)
STD_ROM_FN(dsoccr94k)

struct BurnDriver BurnDrvDsoccr94k = {
	"dsoccr94k", "dsoccr94", NULL, NULL, "1994",
	"Dream Soccer '94 (Korea, M107 hardware)\0", NULL, "Irem (Data East Corporation license)", "M107",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IREM_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, dsoccr94kRomInfo, dsoccr94kRomName, NULL, NULL, Dsoccr94InputInfo, Dsoccr94DIPInfo,
	dsoccr94Init, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	320, 240, 4, 3
};


// World PK Soccer

static struct BurnRomInfo wpksocRomDesc[] = {
	{ "pk-h0-eur-d.h0",	0x40000, 0xb4917788, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "pk-l0-eur-d.l0",	0x40000, 0x03816bae, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pk-sh0.sh0",		0x10000, 0x1145998c, 2 | BRF_PRG | BRF_ESS }, //  2 V35 Code
	{ "pk-sl0.sl0",		0x10000, 0x542ee1c7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "pk-c00-os.c00",	0x80000, 0x42ae3d73, 3 | BRF_GRA },           //  4 Background Tiles
	{ "pk-c10-os.c10",	0x80000, 0x86acf45c, 3 | BRF_GRA },           //  5
	{ "pk-c01-os.c01",	0x80000, 0xb0d33f87, 3 | BRF_GRA },           //  6
	{ "pk-c11-os.c11",	0x80000, 0x19de7d63, 3 | BRF_GRA },           //  7

	{ "pk-000.000",		0x80000, 0x165ce027, 4 | BRF_GRA },           //  8 Sprite Tiles
	{ "pk-001.001",		0x80000, 0xe2745147, 4 | BRF_GRA },           //  9
	{ "pk-010.010",		0x80000, 0x6c171b73, 4 | BRF_GRA },           // 10
	{ "pk-011.011",		0x80000, 0x471c0bf4, 4 | BRF_GRA },           // 11
	{ "pk-020.020",		0x80000, 0xc886dad1, 4 | BRF_GRA },           // 12
	{ "pk-021.021",		0x80000, 0x91e877ff, 4 | BRF_GRA },           // 13
	{ "pk-030.030",		0x80000, 0x3390621c, 4 | BRF_GRA },           // 14
	{ "pk-031.031",		0x80000, 0x4d322804, 4 | BRF_GRA },           // 15

	{ "pk-da0.da0",		0x80000, 0x26a34cf4, 6 | BRF_SND },           // 16 Irem GA20 Samples
};

STD_ROM_PICK(wpksoc)
STD_ROM_FN(wpksoc)

static INT32 wpksocRomLoad()
{
	if (BurnLoadRom(DrvV33ROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(DrvV33ROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvV30ROM  + 0x000001,  2, 2)) return 1;
	if (BurnLoadRom(DrvV30ROM  + 0x000000,  3, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  5, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x100000,  6, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x100001,  7, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x000001,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100001, 11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200001, 13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300001, 15, 2)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x000000, 16, 1)) return 1;

	return 0;
}

static INT32 wpksocInit()
{
	has_bankswitch = 0;
	spritesystem = 0;
	config_cpu_speed = 14000000;
	return DrvInit(wpksocRomLoad, leagueman_decryption_table, 1, 0x80, 0x200000, 0x400000);
}

struct BurnDriverD BurnDrvWpksoc = {
	"wpksoc", NULL, NULL, NULL, "1995",
	"World PK Soccer\0", NULL, "Jaleco", "M107",
	NULL, NULL, NULL, NULL,
	0, 4, HARDWARE_IREM_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, wpksocRomInfo, wpksocRomName, NULL, NULL, Dsoccr94InputInfo, Dsoccr94DIPInfo,
	wpksocInit, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	320, 240, 4, 3
};

// Kick for the Goal

static struct BurnRomInfo kftgoalRomDesc[] = {
	{ "pk-h0-usa-d.h0",	0x40000, 0xaed4cde0, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "pk-l0-usa-d.l0",	0x40000, 0x39fe30d2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pk-sh0.sh0",		0x10000, 0x1145998c, 2 | BRF_PRG | BRF_ESS }, //  2 V35 Code
	{ "pk-sl0.sl0",		0x10000, 0x542ee1c7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "pk-c00-os.c00",	0x80000, 0x42ae3d73, 3 | BRF_GRA },           //  4 Background Tiles
	{ "pk-c10-os.c10",	0x80000, 0x86acf45c, 3 | BRF_GRA },           //  5
	{ "pk-c01-os.c01",	0x80000, 0xb0d33f87, 3 | BRF_GRA },           //  6
	{ "pk-c11-os.c11",	0x80000, 0x19de7d63, 3 | BRF_GRA },           //  7

	{ "pk-000-usa.000",	0x80000, 0x72e905ab, 4 | BRF_GRA },           //  8 Sprite Tiles
	{ "pk-001-usa.001",	0x80000, 0xeec4f43c, 4 | BRF_GRA },           //  9
	{ "pk-010-usa.010",	0x80000, 0xb3339d73, 4 | BRF_GRA },           // 10
	{ "pk-011-usa.011",	0x80000, 0xbab2b7cf, 4 | BRF_GRA },           // 11
	{ "pk-020-usa.020",	0x80000, 0x740a0bef, 4 | BRF_GRA },           // 12
	{ "pk-021-usa.021",	0x80000, 0xf44208a6, 4 | BRF_GRA },           // 13
	{ "pk-030-usa.030",	0x80000, 0x8eceef50, 4 | BRF_GRA },           // 14
	{ "pk-031-usa.031",	0x80000, 0x8aa7dc04, 4 | BRF_GRA },           // 15

	{ "pk-da0.da0",		0x80000, 0x26a34cf4, 6 | BRF_SND },           // 16 Irem GA20 Samples

	{ "st-m28c64c.eeprom",	0x02000, 0x8e0c8b7c, 7 | BRF_PRG | BRF_ESS }, // 17 Eeprom data
};

STD_ROM_PICK(kftgoal)
STD_ROM_FN(kftgoal)

struct BurnDriverD BurnDrvKftgoal = {
	"kftgoal", "wpksoc", NULL, NULL, "1995",
	"Kick for the Goal\0", NULL, "Jaleco", "M107",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 4, HARDWARE_IREM_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, kftgoalRomInfo, kftgoalRomName, NULL, NULL, Dsoccr94InputInfo, Dsoccr94DIPInfo,
	wpksocInit, DrvExit, DrvFrame, DrvReDraw, DrvScan, &bRecalcPalette, 0x800,
	320, 240, 4, 3
};
