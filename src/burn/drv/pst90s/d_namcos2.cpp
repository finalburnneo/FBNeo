// FB Alpha Namco System 2 driver module
// Based on MAME driver by K.Wilkins

//tested / good:
// assault
// burnforc
// cosmogng
// dsaber
// mirninja
// valkyrie
// ordyne
// phelious
// rthun2
// marvland
// metlhawk
// kyukaidk
// sws92,93
// sgunner
// sgunner2 (needs old mcu)
// dirtfoxj
// finehour
// luckywld

//wip:
// bubbletr	- ok, missing artwork (flipped)
// gollygho	- ok, missing artwork (flipped)

//forget:
// finallap	- some bad gfx (title sprites), bad sound, no inputs
// finalap2	- "...all finallap-based games (incl. fourtrax & suzuka) are bugged and unplayable, even in mame"
// finalap3	- ""
// fourtrax	- ""
// suzuk8h	- ""
// suzuk8h2	- ""

//-timing notes- 240+16 fixes both
//vbl@240 sgunner after coin up, add more coins and coins# flickers
//vbl@240 sgunner2 attract mode before title screen, when its drawing the '2',
//        it will flicker a frame (w/wrong palette) from previous scene.
//        dsaber: all tmap glitches fixed with 240+16.
//        dirtfoxj, cosmogng, rthun2, phelios needs 240+8 otherwise sprite glitches
//vbl@224 rthun2 needs vbl@224 or flickery explosion in intro is wrong, also signs/sprites flicker sometimes when they shouldn't

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "m6805_intf.h"
#include "burn_ym2151.h"
#include "c140.h"
#include "c169.h"
#include "namco_c45.h"
#include "burn_gun.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM[2];
static UINT8 *Drv68KData;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvGfxROM5;
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT8 *DrvC45PROM;
static UINT8 *Drv68KRAM[2];
static UINT8 *DrvC123RAM;
static UINT8 *DrvC139RAM;
static UINT8 *DrvRozRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvDPRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvC123Ctrl;
static UINT8 *DrvRozCtrl;

static UINT8 *SpritePrio;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 gfx_ctrl;
// ...to keep track of spritebank changes & lines to draw them between
static UINT32 scanline, position; // current scanline, "pos-irq" position
static UINT32 sprite_bankSL[0x10][0x2];
static UINT32 sprite_bankL;
static UINT32 lastsprite_bank;

static UINT8 irq_reg[2];
static UINT8 irq_cpu[2];
static UINT8 irq_vblank[2];
static UINT8 irq_ex[2];
static UINT8 irq_pos[2];
static UINT8 irq_sci[2];
static UINT8 bus_reg[2];

static UINT16 c355_obj_position[4];

static INT32 audio_cpu_in_reset;
static INT32 sub_cpu_in_reset; // mcu & slave

static UINT16 sound_bank = 0;

static INT32 layer_color;
static UINT8 *roz_dirty_tile; // 0x10000
static UINT16 *roz_bitmap; // (256 * 8) * (256 * 8)
static INT32 roz_update_tiles = 0; // 

static void (*pDrvDrawBegin)() = NULL; // optional line-drawing
static void (*pDrvDrawLine)(INT32) = NULL;

static INT32 min_x = 0; // screen clipping
static INT32 max_x = 0;
static INT32 min_y = 0;
static INT32 max_y = 0;

static UINT8 mcu_analog_ctrl;
static UINT8 mcu_analog_complete;
static UINT8 mcu_analog_data;

static UINT32 maincpu_run_cycles = 0;
static UINT32 maincpu_run_ended = 0;

static INT32 key_sendval;
static UINT16 (*key_prot_read)(UINT8 offset) = NULL;
static void (*key_prot_write)(UINT8 offset, UINT16 data) = NULL;

static INT32 finallap_prot_count = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static INT32 DrvAnalogPort2 = 0;

static INT32 uses_gun = 0;
static INT32 DrvGun0 = 0;
static INT32 DrvGun1 = 0;
static INT32 DrvGun2 = 0;
static INT32 DrvGun3 = 0;

static INT32 is_dirtfoxj = 0;
static INT32 is_luckywld = 0;
static INT32 is_metlhawk = 0;

static INT32 weird_vbl = 0;

static INT32 nvramcheck = 0; // nvram init: 1 ordyne, 2 ordynej, 3 dirtfoxj.  0 after set!

static void FinallapDrawBegin(); // forwards
static void FinallapDrawLine(INT32 line); // ""
static void LuckywldDrawBegin(); // forwards
static void LuckywldDrawLine(INT32 line); // ""

static struct BurnInputInfo DefaultInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Default)

static struct BurnInputInfo AssaultInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy3 + 7,	"p1 right"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy3 + 3,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy4 + 3,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy4 + 1,	"p3 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 0,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy3 + 2,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"p4 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy4 + 2,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Assault)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo MetlhawkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 2"	},

	A("P1 X Axis",          BIT_ANALOG_REL, &DrvAnalogPort1 , "mouse x-axis"),
	A("P1 Y Axis",          BIT_ANALOG_REL, &DrvAnalogPort0 , "mouse y-axis"),
	A("P1 Up/Down Axis",      BIT_ANALOG_REL, &DrvAnalogPort2 , "p1 x-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Metlhawk)

static struct BurnInputInfo LuckywldInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	A("P1 Steering",	BIT_ANALOG_REL, &DrvAnalogPort0, "p1 x-axis"	),
	A("P1 Break",		BIT_ANALOG_REL, &DrvAnalogPort1, "p1 fire 5"	),
	A("P1 Accelerator",	BIT_ANALOG_REL, &DrvAnalogPort2, "p1 fire 6"	),

	A("P1 Gun X",    	BIT_ANALOG_REL, &DrvGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &DrvGun1,    "mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &DrvGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &DrvGun3,    "p2 y-axis"	),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Luckywld)

static struct BurnInputInfo SgunnerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &DrvGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &DrvGun1,    "mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &DrvGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &DrvGun3,    "p2 y-axis"	),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Sgunner)

static struct BurnInputInfo DirtfoxInputList[] = {
	{"Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},

	A("Steering",        BIT_ANALOG_REL, &DrvAnalogPort0 , "p1 x-axis"	),
	A("Break",           BIT_ANALOG_REL, &DrvAnalogPort1 , "p1 fire 5"	),
	A("Accelerator",     BIT_ANALOG_REL, &DrvAnalogPort2 , "p1 fire 6"	),

	{"Gear Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"Gear Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Debug",		BIT_DIPSWITCH,  DrvDips + 1,    "dip"           },
};

STDINPUTINFO(Dirtfox)

static struct BurnDIPInfo DefaultDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x14, 0x01, 0x01, 0x01, "Normal"		},
	{0x14, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Default)

static struct BurnDIPInfo AssaultDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x18, 0x01, 0x01, 0x01, "Normal"		},
	{0x18, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x40, 0x40, "Off"			},
	{0x19, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Assault)

static struct BurnDIPInfo MetlhawkDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL			},
	{0x0a, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x09, 0x01, 0x01, 0x01, "Normal"		},
	{0x09, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"			},
	{0x0a, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Metlhawk)

static struct BurnDIPInfo LuckywldDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	{0x10, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x0f, 0x01, 0x01, 0x01, "Normal"		},
	{0x0f, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x40, 0x40, "Off"			},
	{0x10, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Luckywld)

static struct BurnDIPInfo SgunnerDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL			},
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x0e, 0x01, 0x01, 0x01, "Normal"		},
	{0x0e, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x40, 0x40, "Off"			},
	{0x0f, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Sgunner)

static struct BurnDIPInfo DirtfoxDIPList[]=
{
	{0x08, 0xff, 0xff, 0xff, NULL			},
	{0x09, 0xff, 0xff, 0xff, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Video Display"	},
	{0x08, 0x01, 0x01, 0x01, "Normal"		},
	{0x08, 0x01, 0x01, 0x00, "Frozen"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x09, 0x01, 0x40, 0x40, "Off"			},
	{0x09, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Dirtfox)

static inline void palette_write(UINT16 offset)
{
	offset /= 2;

	INT32 ofst = (offset & 0x67ff);

	offset = (offset & 0x7ff) | ((offset & 0x6000) >> 2);

	UINT16 *ram = (UINT16*)DrvPalRAM;

	UINT8 r = ram[ofst + 0x0000];
	UINT8 g = ram[ofst + 0x0800];
	UINT8 b = ram[ofst + 0x1000];

	DrvPalette[offset] = BurnHighCol(r,g,b,0);

	DrvPalette[offset + 0x2000] = BurnHighCol(r/2,g/2,b/2,0); // shadow
}

static void clear_all_irqs()
{
	for (INT32 i = 0; i < 8; i++) {
		SekSetIRQLine(i, CPU_IRQSTATUS_NONE);
	}
}

static UINT16 c148_read_write(UINT32 offset, UINT16 data, INT32 w)
{
	INT32 a = SekGetActive();

	switch (offset & 0x3e000)
	{
		case 0x00000:
			return 0;

		case 0x04000:
			if (w) bus_reg[a] = data & 7;
			return bus_reg[a];

		case 0x06000:
			if (w) { irq_cpu[a] = data & 7; clear_all_irqs(); }
			return irq_cpu[a];

		case 0x08000:
			if (w) { irq_ex[a] = data & 7; clear_all_irqs(); }
			return irq_ex[a];

		case 0x0a000:
			if (w) { irq_pos[a] = data & 7; clear_all_irqs(); }
			return irq_pos[a];

		case 0x0c000:
			if (w) { irq_sci[a] = data & 7; clear_all_irqs(); }
			return irq_sci[a];

		case 0x0e000:
			if (w) { irq_vblank[a] = data & 7; clear_all_irqs(); }
			return irq_vblank[a];

		case 0x10000:
			if (w) {
				SekSetIRQLine(a^1, irq_cpu[(a) ? 0 : 1], CPU_IRQSTATUS_ACK);
			}
			return 0;

		case 0x16000:
			SekSetIRQLine(a^1, irq_cpu[(a) ? 0 : 1], CPU_IRQSTATUS_NONE);
			return 0;

		case 0x18000:
			SekSetIRQLine(irq_ex[a], CPU_IRQSTATUS_NONE);
			return 0;

		case 0x1a000:
			SekSetIRQLine(irq_pos[a], CPU_IRQSTATUS_NONE);
			return 0;

		case 0x1c000:
			SekSetIRQLine(irq_sci[a], CPU_IRQSTATUS_NONE);
			return 0;

		case 0x1e000:
			SekSetIRQLine(irq_vblank[a], CPU_IRQSTATUS_NONE);
			return 0;

		case 0x20000:
			return 0xffff; // eeprom ready

		case 0x22000:
			if (w && a == 0)
			{
				audio_cpu_in_reset = ~data & 1;
				if (audio_cpu_in_reset) M6809Reset();
				else {
					maincpu_run_cycles = SekTotalCycles();
					maincpu_run_ended = 1;
					SekRunEnd();
				}
			}
			return 0;

		case 0x24000:
			if (w && a == 0)
			{
				sub_cpu_in_reset = ~data & 1;
				if (sub_cpu_in_reset)
				{
					hd63705Reset();

					SekReset(1);
				}
				else
				{
					maincpu_run_cycles = SekTotalCycles();
					maincpu_run_ended = 1;
					SekRunEnd();
				}
			}
			return 0;

		case 0x26000:
			return 0; // watchdog
	}

	return 0;
}

static UINT16 __fastcall namcos2_68k_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x180000) {
		return DrvEEPROM[(address / 2) & 0x1fff];
	}

	if ((address & 0xfc0000) == 0x1c0000) {
		return c148_read_write(address, 0, 0);
	}

	if ((address & 0xffffc0) == 0x420000) {
		return *((UINT16*)(DrvC123Ctrl + (address & 0x3e)));
	}

	if ((address & 0xff0000) == 0x440000) {
		if ((address & 0x301e) > 0x3016) return 0xff;
		return *((UINT16*)(DrvPalRAM + (address & 0x301e)));
	}

	if ((address & 0xff0000) == 0x460000) {
		return DrvDPRAM[(address & 0xffe)/2];
	}

	if ((address & 0xfffff0) == 0xcc0000) {
		return *((UINT16*)(DrvRozCtrl + (address & 0x0e)));
	}

	if ((address & 0xfffff0) == 0xd00000) {
		if (key_prot_read != NULL)
			return key_prot_read(address/2);

		return BurnRandom();
	}

	switch (address)
	{
		case 0x4a0000:
			return 0x04; // c139 status

		case 0xc40000:
			return gfx_ctrl;
	}

	return 0;
}

static UINT8 __fastcall namcos2_68k_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x180000) {
		return DrvEEPROM[(address / 2) & 0x1fff];
	}

	if ((address & 0xffffc0) == 0x420000) {
		return DrvC123Ctrl[(address & 0x3f)^1];
	}

	if ((address & 0xff0000) == 0x440000) {
		if ((address & 0x301e) > 0x3016) return 0xff;
		return *((UINT16*)(DrvPalRAM + (address & 0x301e)));
	}

	if ((address & 0xff0000) == 0x460000) {
		return DrvDPRAM[(address & 0xffe)/2];
	}

	if ((address & 0xfc0000) == 0x1c0000) {
		return c148_read_write(address, 0, 0);
	}

	return 0;
}

static void __fastcall namcos2_68k_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x180000) {
		DrvEEPROM[(address / 2) & 0x1fff] = data;
		return;
	}

	if ((address & 0xfc0000) == 0x1c0000) {
		c148_read_write(address, data, 1);
		return;
	}

	if ((address & 0xffffc0) == 0x420000) {
		*((UINT16*)(DrvC123Ctrl + (address & 0x3e))) = data;
		return;
	}

	if ((address & 0xff0000) == 0x440000) {
		if ((address & 0x3000) >= 0x3000) {
			*((UINT16*)(DrvPalRAM + (address & 0x301e))) = data & 0xff;
		} else {
			*((UINT16*)(DrvPalRAM + (address & 0xfffe))) = data;
			palette_write(address);
		}
		return;
	}

	if ((address & 0xff0000) == 0x460000) {
		DrvDPRAM[(address & 0xffe)/2] = data & 0xff;
		return;
	}

	if ((address & 0xfffff0) == 0xcc0000) {
		*((UINT16*)(DrvRozCtrl + (address & 0x0e))) = data;
		return;
	}

	if ((address & 0xfffff0) == 0xd00000) {
		if (key_prot_write != NULL)
			return key_prot_write(address/2,data);
		return;
	}

	switch (address)
	{
		case 0xc40000:
			INT32 startpos = (scanline == position) ? scanline : 0;
			gfx_ctrl = data;

			// notes: finehour is the only game that takes advantage of this technique.
			// from game (finehour)   / what we do
			// scanline 188: 0xf        draw bank 0xf from 188 to end of screen. note: things will look terrible if 0xf is drawn above line 188!
			// scanline 251 & 261: 0    draw bank 0x0 from 0 to end of screen. note: really should draw from 0 - 188, but its not needed and the extra logic is cumbersome

			if ((gfx_ctrl & 0xf) != 0 && lastsprite_bank != (gfx_ctrl & 0xf))
			{
				// ..to help dink find other games that change the spritebank *keep*
				bprintf(0, _T("Spritebank change: %X @ %d. \n"), gfx_ctrl & 0xf, startpos);
				lastsprite_bank = gfx_ctrl & 0xf;
			}

			sprite_bankL |= 1 << (gfx_ctrl & 0xf);
			sprite_bankSL[gfx_ctrl & 0xf][0] = (startpos >= nScreenHeight) ? 0 : startpos; // spritebank set past nScreenHeight means to use for next frame.
			sprite_bankSL[gfx_ctrl & 0xf][1] = nScreenHeight;
		return;
	}
}

static void __fastcall namcos2_68k_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x180000) {
		DrvEEPROM[(address / 2) & 0x1fff] = data;
		return;
	}

	if ((address & 0xff0000) == 0x440000) {
		if ((address & 0x3000) >= 0x3000) {
			*((UINT16*)(DrvPalRAM + (address & 0x301e))) = data;
		} else {
			DrvPalRAM[(address & 0xffff)^1] = data;
			palette_write(address);
		}
		return;
	}

	if ((address & 0xff0000) == 0x460000) {
		DrvDPRAM[(address & 0xffe)/2] = data;
		return;
	}

	if (address == 0x0074) return; // NOP (phelios writes here in-game)
}


static UINT16 __fastcall sgunner_68k_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0xa00000) {
		if (key_prot_read != NULL)
			return key_prot_read(address/2);

		return BurnRandom();
	}

	return namcos2_68k_read_word(address);
}

static UINT8 __fastcall sgunner_68k_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0xa00000) {
		if (key_prot_read != NULL)
			return key_prot_read(address/2);

		return BurnRandom();
	}

	return namcos2_68k_read_byte(address);
}

static void __fastcall luckywld_68k_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe0) == 0xd00000) {
		*((UINT16*)(DrvRozCtrl + (address & 0x1e))) = data;
		return;
	}

	if ((address & 0xfffff8) == 0xf00000) {
		if (key_prot_write != NULL)
			key_prot_write(address/2,data);
		return;
	}

	if ((address & 0xfffff8) == 0x900000) {
		c355_obj_position[(address/2) & 3] = data;
		return;
	}

	switch (address)
	{
		case 0x818000:
		case 0x818001:
		case 0x81a000:
		case 0x81a001:
		return;
	}

	namcos2_68k_write_word(address, data);
}


static void __fastcall luckywld_68k_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0xd00000) {
		DrvRozCtrl[(address & 0x1f)^1] = data;
		return;
	}

	if ((address & 0xfffff8) == 0xf00000) {
		if (key_prot_write != NULL)
			key_prot_write(address/2,data);
		return;
	}

	switch (address)
	{
		case 0x818000:
		case 0x818001:
		case 0x81a000:
		case 0x81a001:
		return;
	}

	namcos2_68k_write_byte(address, data);
}

static UINT16 __fastcall luckywld_68k_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0xd00000) {
		return *((UINT16*)(DrvRozCtrl + (address & 0x1e)));
	}

	if ((address & 0xfffff8) == 0xf00000) {
		if (key_prot_read != NULL)
			return key_prot_read(address/2);

		return BurnRandom();
	}

	if ((address & 0xfffff8) == 0x900000) {
		return c355_obj_position[(address/2) & 3];
	}

	return namcos2_68k_read_word(address);
}

static UINT8 __fastcall luckywld_68k_read_byte(UINT32 address)
{
	return namcos2_68k_read_byte(address);
}

static void __fastcall metlhawk_68k_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe0) == 0xd00000) {

		*((UINT16*)(DrvRozCtrl + (address & 0x1e))) = data;
		return;
	}

	switch (address)
	{
		case 0xe00000:
			gfx_ctrl = data;
			sprite_bankL |= 1<<(gfx_ctrl&0xf);
		return;
	}

	namcos2_68k_write_word(address,data);
}

static void __fastcall metlhawk_68k_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0xd00000) {
		DrvRozCtrl[(address & 0x1f)^1] = data;
		return;
	}

	namcos2_68k_write_word(address,data);
}


static UINT16 __fastcall metlhawk_68k_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0xd00000) {
		return *((UINT16*)(DrvRozCtrl + (address & 0x0e)));
	}

	switch (address)
	{
		case 0xe00000:
		case 0xe00001:
			return gfx_ctrl;
	}

	return namcos2_68k_read_word(address);
}

static UINT8 __fastcall metlhawk_68k_read_byte(UINT32 address)
{
	if ((address & 0xffffe0) == 0xd00000) {
		return DrvRozCtrl[(address & 0x1f)^1];
	}

	return namcos2_68k_read_byte(address);
}

static UINT16 namcos2_finallap_prot_read(INT32 offset)
{
	static const UINT16 table0[8] = { 0x0000,0x0040,0x0440,0x2440,0x2480,0xa080,0x8081,0x8041 };
	static const UINT16 table1[8] = { 0x0040,0x0060,0x0060,0x0860,0x0864,0x08e4,0x08e5,0x08a5 };

	UINT16 data = 0;

	switch ((offset & 0x3ffff)/2)
	{
		case 0:
			data = 0x0101;
			break;

		case 1:
			data = 0x3e55;
			break;

		case 2:
			data = table1[finallap_prot_count&7];
			data = (data&0xff00)>>8;
			break;

		case 3:
			data = table1[finallap_prot_count&7];
			finallap_prot_count++;
			data = data&0x00ff;
			break;

		case 0x3fffc/2:
			data = table0[finallap_prot_count&7];
			data = data&0xff00;
			break;

		case 0x3fffe/2:
			data = table0[finallap_prot_count&7];
			finallap_prot_count++;
			data = (data&0x00ff)<<8;
			break;

		default:
			data = 0;
	}
	return data;
}

static void __fastcall finallap_68k_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x840000:
			gfx_ctrl = data;
			sprite_bankL |= 1<<(gfx_ctrl&0xf);
		return;
	}

	namcos2_68k_write_word(address,data);
}

static void __fastcall finallap_68k_write_byte(UINT32 address, UINT8 data)
{
	namcos2_68k_write_byte(address,data);
}

static UINT16 __fastcall finallap_68k_read_word(UINT32 address)
{
	if ((address & 0xfc0000) == 0x300000) {
		return namcos2_finallap_prot_read(address);
	}

	switch (address)
	{
		case 0x840000:
			return gfx_ctrl;
	}

	return namcos2_68k_read_word(address);
}

static UINT8 __fastcall finallap_68k_read_byte(UINT32 address)
{
	if ((address & 0xfc0000) == 0x300000) {
		return namcos2_finallap_prot_read(address) >> ((~address & 1) << 3);
	}

	return namcos2_68k_read_byte(address);
}

static INT32 AnalogClip(INT16 p)
{
	if (p > 1023) p = 1023;
	else if (p < -1023) p = -1023;
	return p;
}

static INT16 AnalogClipExDelay = 0;

static INT32 AnalogClipEx(INT16 p)
{
	if (is_metlhawk) // for the up/down axis & mouse wheel playability
	{
		if (p > 255) p = 1023;
		else if (p < -255) p = -1023;

		if (p) AnalogClipExDelay = p;

		if (!p) // no change on wheel - use last value and decrement it
		{
			if (AnalogClipExDelay > 0)
			{
				AnalogClipExDelay -= 50;
				if (AnalogClipExDelay < 0) AnalogClipExDelay = 0;

			}
			else if (AnalogClipExDelay < 0)
			{
				AnalogClipExDelay += 50;
				if (AnalogClipExDelay > 0) AnalogClipExDelay = 0;
			}

			p = AnalogClipExDelay;
		}

	} else {
		if (p > 1023) p = 1023;
		else if (p < -1023) p = -1023;
	}

	return p;
}

static UINT8 luckywldsteer()
{
	return ProcessAnalog(DrvAnalogPort0, 0, 0, 0x00, 0xff);
}

static void mcu_analog_ctrl_write(UINT8 data)
{
	mcu_analog_ctrl = data;

	if (data & 0x40)
	{
		mcu_analog_complete = 2;

		if (uses_gun && !is_luckywld) {
			switch ((data >> 2) & 7)
			{
				case 0: mcu_analog_data = 0; break; // an0
				case 1: mcu_analog_data = 0; break; // an1
				case 2: mcu_analog_data = 0; break; // an2
				case 3: mcu_analog_data = 0; break; // an3
				case 4: mcu_analog_data = BurnGunReturnX(0); break; // an4
				case 5: mcu_analog_data = BurnGunReturnX(1); break; // an5
				case 6: mcu_analog_data = BurnGunReturnY(0); break; // an6
				case 7: mcu_analog_data = BurnGunReturnY(1); break; // an7
			}
		} else if (is_dirtfoxj) {
			switch ((data >> 2) & 7)
			{
				case 0: mcu_analog_data = 0; break; // an0
				case 1: mcu_analog_data = 0; break; // an1
				case 2: mcu_analog_data = 0; break; // an2
				case 3: mcu_analog_data = 0; break; // an3
				case 4: mcu_analog_data = 0; break; // an4
				case 5: mcu_analog_data = 0x7f + (AnalogClip(DrvAnalogPort0) >> 4); break; // an5
				case 6: mcu_analog_data = (AnalogClip(DrvAnalogPort1) >> 4); break; // an6
				case 7: mcu_analog_data = (AnalogClip(DrvAnalogPort2) >> 4); break; // an7
			}
		} else if (is_luckywld) {
			switch ((data >> 2) & 7)
			{
				case 0: mcu_analog_data = 0; break; // an0
				case 1: mcu_analog_data = BurnGunReturnY(1); break; // an1
				case 2: mcu_analog_data = BurnGunReturnY(0); break; // an2
				case 3: mcu_analog_data = BurnGunReturnX(1); break; // an3
				case 4: mcu_analog_data = BurnGunReturnX(0); break; // an4
				case 5: mcu_analog_data = luckywldsteer(); break; // an5
				case 6: mcu_analog_data = (AnalogClip(DrvAnalogPort1) >> 4); break; // an6
				case 7: mcu_analog_data = (AnalogClip(DrvAnalogPort2) >> 4); break; // an7
			}
		} else {
			switch ((data >> 2) & 7)
			{
				case 0: mcu_analog_data = 0; break; // an0
				case 1: mcu_analog_data = 0; break; // an1
				case 2: mcu_analog_data = 0; break; // an2
				case 3: mcu_analog_data = 0; break; // an3
				case 4: mcu_analog_data = 0; break; // an4
				case 5: mcu_analog_data = 0x7f + (AnalogClip(DrvAnalogPort0) >> 4); break; // an5
				case 6: mcu_analog_data = 0x7f + (AnalogClip(DrvAnalogPort1) >> 4); break; // an6
				case 7: mcu_analog_data = 0x7f + (AnalogClipEx(DrvAnalogPort2) >> 4); break; // an7
			}
			//bprintf(0, _T("Port 0: %.06d  Port 1: %.06d  Port 2: %.06d       \n"), DrvAnalogPort0, DrvAnalogPort1, DrvAnalogPort2);
		}
	}

	if (data & 0x20)
	{
		hd63705SetIrqLine(7, CPU_IRQSTATUS_ACK);
		m6805Run(1);
		hd63705SetIrqLine(7, CPU_IRQSTATUS_NONE);
	}
}

static UINT8 mcu_analog_ctrl_read()
{
	INT32 data = 0;
	if (mcu_analog_complete == 2) mcu_analog_complete = 1;
	if (mcu_analog_complete) data |= 0x80;

	return data | (mcu_analog_ctrl & 0x3f);
}

static UINT8 mcu_port_d_r() // no games use this, but keep just in-case
{
	//INT32 threshold = 0x7f;
	INT32 data = 0;

	return data;
}

static UINT8 namcos2_mcu_analog_port_r()
{
	if (mcu_analog_complete==1) mcu_analog_complete=0;
	return mcu_analog_data;
}

static void namcos2_mcu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x5000) {
		DrvDPRAM[address & 0x7ff] = data;
		return;
	}

	switch (address)
	{
		case 0x0003:
			// namcos2_mcu_port_d_w (unused)
		return;

		case 0x0010:
			mcu_analog_ctrl_write(data);
		return;

		case 0x0011:
			// mcu_analog_port_w (unused)
		return;
	}

	if (address < 0x1c0) {
		DrvMCURAM[address] = data;
		return;
	}
}

static UINT8 namcos2_mcu_read(UINT16 address)
{
	if ((address & 0xf000) == 0x6000) {
		return 0; // watchdog
	}

	if ((address & 0xf800) == 0x5000) {
		return DrvDPRAM[address & 0x7ff];
	}

	switch (address)
	{
		case 0x0000:
			return 0;

		case 0x0001:
			return DrvInputs[0]; // mcub

		case 0x0002:
			return (DrvInputs[1] & ~0x40) | (DrvDips[1] & 0x40); // mcuc

		case 0x0003:
			return mcu_port_d_r();

		case 0x0007:
			return DrvInputs[2]; // mcuh

		case 0x0010:
			return mcu_analog_ctrl_read();

		case 0x0011:
			return namcos2_mcu_analog_port_r();

		case 0x2000:
			return DrvDips[0]; // dsw

		case 0x3000:
			return DrvInputs[3]; // MCUDI0

		case 0x3001:
			return 0xff; // MCUDI1

		case 0x3002:
			return 0xff; // MCUDI2

		case 0x3003:
			return 0xff; // MCUDI3
	}

	if (address < 0x1c0) {
		return DrvMCURAM[address];
	} else if (address < 0x2000) {
		return DrvMCUROM[address];
	}

	return 0;
}

static void sound_bankswitch(INT32 data)
{
	sound_bank = data;
	INT32 bank = (data >> 4);

	M6809MapMemory(DrvM6809ROM + (bank * 0x4000), 0x0000, 0x3fff, MAP_ROM);
}

static void namcos2_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x7000) {
		DrvDPRAM[address & 0x7ff] = data;
		return;
	}

	if (address >= 0x5000 && address <= 0x6fff) {
		c140_write(address,data);
		return;
	}

	if ((address & 0xe000) == 0xa000) {
		// amplifier enable (unemulated)
		return;
	}

	switch (address)
	{
		case 0x4000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x4001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xc000:
		case 0xc001:
			sound_bankswitch(data);
		return;

		case 0xd001: // watchdog
		return;

		case 0xe000: // nop
		return;
	}
}

static UINT8 namcos2_sound_read(UINT16 address)
{
	if ((address & 0xf000) == 0x7000) {
		return DrvDPRAM[address & 0x7ff];
	}

	if (address >= 0x5000 && address <= 0x6fff) {
		return c140_read(address);
	}

	switch (address)
	{
		case 0x4000:
		case 0x4001:
			return BurnYM2151Read();
	}

	return 0;
}

static void __fastcall roz_write_word(UINT32 address, UINT16 data)
{		
	UINT16 *ram = (UINT16*)DrvRozRAM;

	UINT16 offset = (address & 0x1ffff) / 2;

	if (ram[offset] != data) {
		roz_dirty_tile[offset] = 1;
		roz_update_tiles = 1;
		ram[offset] = data;
	}
}

static void __fastcall roz_write_byte(UINT32 address, UINT8 data)
{
	INT32 offset = (address & 0x1ffff)^1;

	if (DrvRozRAM[offset] != data) {
		roz_dirty_tile[offset/2] = 1;
		roz_update_tiles = 1;
		DrvRozRAM[offset] = data;
	}
}

static void default_68k_map(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);
	SekMapMemory(Drv68KROM[cpu],		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[cpu],		0x100000, 0x13ffff, MAP_RAM);
	SekMapMemory(Drv68KData,		0x200000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvC123RAM,		0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM + 0x0000,	0x440000, 0x442fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x4000,	0x444000, 0x446fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x8000,	0x448000, 0x44afff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0xc000,	0x44c000, 0x44efff, MAP_ROM);
	SekMapMemory(DrvC139RAM,		0x480000, 0x483fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvRozRAM,			0xc80000, 0xc9ffff, MAP_RAM);
	SekMapMemory(DrvRozRAM,			0xca0000, 0xcbffff, MAP_RAM); // mirror
	SekSetWriteWordHandler(0,		namcos2_68k_write_word);
	SekSetWriteByteHandler(0,		namcos2_68k_write_byte);
	SekSetReadWordHandler(0,		namcos2_68k_read_word);
	SekSetReadByteHandler(0,		namcos2_68k_read_byte);

	SekMapHandler(1,			0xc80000, 0xcbffff, MAP_WRITE);
	SekSetWriteByteHandler(1, roz_write_byte);
	SekSetWriteWordHandler(1, roz_write_word);

	SekClose();
}

static void sgunner_68k_map(INT32 cpu)
{
	default_68k_map(cpu);

	SekOpen(cpu);
	SekMapMemory(DrvSprRAM,			0x800000, 0x8143ff, MAP_RAM); // c355
	SekSetReadWordHandler(0,		sgunner_68k_read_word);
	SekSetReadByteHandler(0,		sgunner_68k_read_byte);
	SekClose();
}

static void metlhawk_68k_map(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);
	SekMapMemory(Drv68KROM[cpu],		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[cpu],		0x100000, 0x13ffff, MAP_RAM);
	SekMapMemory(Drv68KData,		0x200000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvC123RAM,		0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM + 0x0000,	0x440000, 0x442fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x4000,	0x444000, 0x446fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x8000,	0x448000, 0x44afff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0xc000,	0x44c000, 0x44efff, MAP_ROM);
	SekMapMemory(DrvC139RAM,		0x480000, 0x483fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvRozRAM,			0xc40000, 0xc4ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		metlhawk_68k_write_word);
	SekSetWriteByteHandler(0,		metlhawk_68k_write_byte);
	SekSetReadWordHandler(0,		metlhawk_68k_read_word);
	SekSetReadByteHandler(0,		metlhawk_68k_read_byte);
	SekClose();
}

static void luckywld_68k_map(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);
	SekMapMemory(Drv68KROM[cpu],		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[cpu],		0x100000, 0x13ffff, MAP_RAM);
	SekMapMemory(Drv68KData,		0x200000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvC123RAM,		0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM + 0x0000,	0x440000, 0x442fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x4000,	0x444000, 0x446fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x8000,	0x448000, 0x44afff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0xc000,	0x44c000, 0x44efff, MAP_ROM);
	SekMapMemory(DrvC139RAM,		0x480000, 0x483fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x800000, 0x8141ff, MAP_RAM);
	SekMapMemory(c45RoadRAM,		0xa00000, 0xa1ffff, MAP_ROM); // handler in c45roadmap68k
	SekMapMemory(DrvRozRAM,			0xc00000, 0xc0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		luckywld_68k_write_word);
	SekSetWriteByteHandler(0,		luckywld_68k_write_byte);
	SekSetReadWordHandler(0,		luckywld_68k_read_word);
	SekSetReadByteHandler(0,		luckywld_68k_read_byte);

	c45RoadMap68k(0xa00000);

	SekClose();
}

static void finallap_68k_map(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);
	SekMapMemory(Drv68KROM[cpu],		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[cpu],		0x100000, 0x13ffff, MAP_RAM);
//	SekMapMemory(Drv68KData,		0x200000, 0x2fffff, MAP_ROM);
//	SekMapMemory(Drv68KData + 0x140000,	0x340000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvC123RAM,		0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM + 0x0000,	0x440000, 0x442fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x4000,	0x444000, 0x446fff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0x8000,	0x448000, 0x44afff, MAP_ROM);
	SekMapMemory(DrvPalRAM + 0xc000,	0x44c000, 0x44efff, MAP_ROM);
	SekMapMemory(DrvC139RAM,		0x480000, 0x483fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(c45RoadRAM,		0x880000, 0x89ffff, MAP_ROM); // handler in c45roadmap68k
	SekSetWriteWordHandler(0,		finallap_68k_write_word);
	SekSetWriteByteHandler(0,		finallap_68k_write_byte);
	SekSetReadWordHandler(0,		finallap_68k_read_word);
	SekSetReadByteHandler(0,		finallap_68k_read_byte);

	c45RoadMap68k(0x880000);

	SekClose();
}

static void namcos2_sound_init()
{
	M6809Init(0);
	M6809Open(0);
//	M6809MapMemory(DrvDPRAM,	0x7000, 0x77ff, MAP_RAM);
//	M6809MapMemory(DrvDPRAM,	0x7800, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM,	0x8000, 0x9fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(namcos2_sound_write);
	M6809SetReadHandler(namcos2_sound_read);
	M6809Close();

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	c140_init(21333, C140_TYPE_SYSTEM2, DrvSndROM);
}

static void namcos2_mcu_init()
{
	m6805Init(1, 1 << 16);
	m6805Open(0);
//	m6805MapMemory(DrvMCURAM + 0x0100,	0x0100, 0x01ff, MAP_RAM);
	m6805MapMemory(DrvMCUROM + 0x0200,	0x0200, 0x1fff, MAP_ROM);
//	m6805MapMemory(DrvDPRAM,		0x5000, 0x57ff, MAP_RAM);
	m6805MapMemory(DrvMCUROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	m6805SetWriteHandler(namcos2_mcu_write);
	m6805SetReadHandler(namcos2_mcu_read);
	m6805Close();
}

static void FreshEEPROMCheck()
{
	UINT8 ordyneeeprom[] = {
		0x96,0x44,0x01,0x01,0x00,0x18,0x00,0x80,0x4f,0x52,0x44,0x41,0x01,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0x04,0x9d,0x01,0x01,0x01,0x01,0x00,0x01,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

	UINT8 ordynejeeprom[] = {
		0x96,0x44,0x01,0x01,0x00,0x18,0x00,0x80,0x4f,0x52,0x44,0x41,0x01,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0x05,0xeb,0x01,0x01,0x01,0x01,0x00,0x01,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x10,0x00,0x3e };

	UINT8 dirtfoxjeeprom[] = {
		0x9a,0x49,0x01,0x01,0x00,0x18,0x00,0x80,0x44,0x41,0x52,0x54,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x01,0x04,0xff,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0x84,0xb8,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x01,0x04,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00 };

	if (DrvEEPROM[0] == 0xff && nvramcheck) {
		bprintf(0, _T("Setting default NVRAM for %S!\n"), BurnDrvGetTextA(DRV_NAME));

		memset(DrvEEPROM, 0xff, 0x2000);

		switch (nvramcheck) {
			case 1: memcpy(DrvEEPROM, ordyneeeprom, 6*16); break;
			case 2: memcpy(DrvEEPROM, ordynejeeprom, 6*16); break;
			case 3: memcpy(DrvEEPROM, dirtfoxjeeprom, 6*16); break;
		}
	}

	nvramcheck = 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memset (roz_dirty_tile, 1, 0x10000);
	roz_update_tiles = 1;

	SekReset(0);
	SekReset(1);

	M6809Open(0);
	M6809Reset();
	sound_bankswitch(0);
	BurnYM2151Reset();
	c140_reset();
	M6809Close();

	m6805Open(0);
	hd63705Reset();
	m6805Close();

	c45RoadReset();

	gfx_ctrl = 0;
	sprite_bankL = 0;
	lastsprite_bank = 0;

	irq_reg[1] = irq_reg[0] = 0;
	irq_cpu[1] = irq_cpu[0] = 0;
	irq_vblank[1] = irq_vblank[0] = 0;
	irq_ex[1] = irq_ex[0] = 0;
	irq_pos[1] = irq_pos[0] = 0;
	irq_sci[1] = irq_sci[0] = 0;
	bus_reg[1] = bus_reg[0] = 0;

	audio_cpu_in_reset = 1;
	sub_cpu_in_reset = 1; // mcu & slave

	key_sendval = 0;

	mcu_analog_ctrl = 0;
	mcu_analog_data = 0xaa;
	mcu_analog_complete = 0;

	finallap_prot_count = 0;

	c355_obj_position[0] = c355_obj_position[1] = 0;
	c355_obj_position[2] = c355_obj_position[3] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM[0]		= Next; Next += 0x040000;
	Drv68KROM[1]		= Next; Next += 0x040000;
	Drv68KData		= Next; Next += 0x200000;
	DrvM6809ROM		= Next; Next += 0x040000;
	DrvMCUROM		= Next; Next += 0x010000;

	DrvC45PROM		= Next; Next += 0x000100;

	DrvGfxROM0		= Next; Next += 0x400000; // sprite 32x32
	DrvGfxROM1		= Next; Next += 0x400000; // sprite 16x16
	DrvGfxROM2		= Next; Next += 0x400000; // char
	DrvGfxROM3		= Next; Next += 0x400000; // roz

	DrvGfxROM4		= Next; Next += 0x080000; // char mask
	DrvGfxROM5		= Next; Next += 0x080000; // roz mask

	DrvSndROM		= Next; Next += 0x100000;

	DrvEEPROM		= Next; Next += 0x002000;

	roz_dirty_tile		= Next; Next += 0x020000;
	roz_bitmap		= (UINT16*)Next; Next += ((256 * 16) * (256 * 16) * 2);

	SpritePrio      = Next; Next += (300 * 300);

	DrvPalette		= (UINT32*)Next; Next += 0x4001 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM[0]		= Next; Next += 0x040000;
	Drv68KRAM[1]		= Next; Next += 0x040000;
	DrvC123RAM		= Next; Next += 0x020000;
	DrvC139RAM		= Next; Next += 0x004000;
	DrvRozRAM		= Next; Next += 0x020000;
	DrvSprRAM		= Next; Next += 0x004000 + 0x10400;
	DrvPalRAM		= Next; Next += 0x010000;
	DrvDPRAM		= Next; Next += 0x000800;
	DrvMCURAM		= Next; Next += 0x000200;
	DrvM6809RAM		= Next; Next += 0x002000;

	DrvC123Ctrl		= Next; Next += 0x000040;
	DrvRozCtrl		= Next; Next += 0x000020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DoMirror(UINT8 *rom, INT32 size, INT32 mirror_size)
{
	for (INT32 i = mirror_size; i < size; i+=mirror_size)
	{
		memcpy (rom + i, rom, mirror_size);
	}
}

static INT32 Namcos2GetRoms(INT32 gfx0_offset)
{
	char* pRomName;
	struct BurnRomInfo ri;
	struct BurnRomInfo pi;

	UINT8 *m6809 = DrvM6809ROM;
	UINT8 *mcu = DrvMCUROM;
	UINT8 *gfx[5] = { DrvGfxROM0 + gfx0_offset, DrvGfxROM2, DrvGfxROM3, DrvGfxROM4 };
	UINT8 *snd = DrvSndROM;
	UINT8 *data = Drv68KData;
	INT32 sws92_ctr = 0;

	memset (DrvEEPROM,  0xff, 0x002000);
	memset (DrvGfxROM0, 0xff, 0x400000);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);
		BurnDrvGetRomInfo(&pi, i+1);

		INT32 use_size = ri.nType & 0x10;
		INT32 use_sws92 = ri.nType & 0x20;

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) < 3) {
			INT32 c = (ri.nType - 1) & 1;
			if (BurnLoadRom(Drv68KROM[c] + 0x000001, i + 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM[c] + 0x000000, i + 1, 2)) return 1;
			i++;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 3) {
			if (BurnLoadRom(m6809, i, 1)) return 1;
			m6809 += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 4) {
			if (BurnLoadRom(mcu, i, 1)) return 1;
			mcu += 0x8000;
			continue;
		}

		if ((ri.nType & BRF_GRA) && ((ri.nType & 0x0f) >= 5 && (ri.nType & 0x0f) < 9)) {
			if (BurnLoadRom(gfx[(ri.nType - 5) & 0xf], i, 1)) return 1;

			if (use_sws92) {
				INT32 inc_len = (sws92_ctr == 0) ? 0x300000 : 0x80000;
				gfx[(ri.nType - 5) & 0xf] += inc_len;
				sws92_ctr++;
			} else
			if (use_size) {
				gfx[(ri.nType - 5) & 0xf] += ri.nLen;
			} else {
				if (ri.nLen < 0x80000) DoMirror(gfx[(ri.nType - 5) & 0xf], 0x80000, ri.nLen);
				gfx[(ri.nType - 5) & 0xf] += 0x80000;
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 9) {
			if (BurnLoadRom(data + 0x00001, i + 0, 2)) return 1;

			if ((pi.nType & BRF_PRG) && (pi.nType & 0x0f) == 9) {
				if (BurnLoadRom(data + 0x00000, i + 1, 2)) return 1;
				i++;
			}

			if (ri.nLen < 0x80000) DoMirror(data, 0x100000, ri.nLen*2);
			data += 0x100000;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x0f) == 0x0a) {
			if (BurnLoadRom(snd, i, 1)) return 1;
			if (ri.nLen < 0x80000) DoMirror(snd, 0x80000, ri.nLen);
			snd += 0x80000;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 11) {
			if (BurnLoadRom(DrvEEPROM, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 12) {
			if (BurnLoadRom(DrvC45PROM, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & BRF_GRA) && ((ri.nType & 0x0f) == 13)) {
			if (BurnLoadRom(DrvGfxROM5, i, 1)) return 1;
			continue;
		}
	}

	if ((m6809 - DrvM6809ROM) == 0x20000) memcpy (DrvM6809ROM + 0x20000, DrvM6809ROM, 0x20000); // mirror

	if (strncmp(BurnDrvGetTextA(DRV_NAME), "ordyne", 6) == 0) {
		memmove (DrvGfxROM2 + 0x280000, DrvGfxROM2 + 0x180000, 0x180000);
		memmove (DrvGfxROM2 + 0x180000, DrvGfxROM2 + 0x100000, 0x080000);
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[8]  = {
		(0x400000*3),(0x400000*3)+4,(0x400000*2),(0x400000*2)+4,
		(0x400000*1),(0x400000*1)+4,(0x400000*0),(0x400000*0)+4
	};
	INT32 XOffs0[32] = {
		STEP4( 0,1), STEP4( 8,1), STEP4(16,1), STEP4(24,1),
		STEP4(32,1), STEP4(40,1), STEP4(48,1), STEP4(56,1)
	};
	INT32 YOffs0[32] = { STEP32(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x400000);

	GfxDecode(0x0800, 8, 32, 32, Plane0, XOffs0, YOffs0, 0x800, tmp + 0x0000000, DrvGfxROM0 + 0x0000000);
	GfxDecode(0x0800, 8, 32, 32, Plane0, XOffs0, YOffs0, 0x800, tmp + 0x0200000, DrvGfxROM0 + 0x0200000);

	BurnFree(tmp);

	for (INT32 i = 0; i < 0x400000; i++)
	{
		INT32 j = (i & 0xffffe0f) | ((i & 0x10) << 4) | ((i & 0x1e0) >> 1);

		DrvGfxROM1[j] = DrvGfxROM0[i];
	}

	return 0;
}

static void decode_layer_tiles()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);

	for (INT32 i = 0; i < 0x400000/0x40; i++)
	{
		INT32 j = (i & 0x07ff) | ((i & 0xc000) >> 3) | ((i & 0x3800) << 2);

		memcpy (tmp + i * 0x40, DrvGfxROM2 + j * 0x40, 0x40);
	}

	memcpy (DrvGfxROM2, tmp, 0x400000);

	BurnFree(tmp);
}

static void decode_layer_tiles_finalap2()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);

	for (INT32 i = 0; i < 0x200000/0x40; i++)
	{
		INT32 j = (i & 0x07ff) | ((i & 0x4000) >> 3) | ((i & 0x3800) << 1);

		memcpy (tmp + i * 0x40, DrvGfxROM2 + j * 0x40, 0x40);
	}

	memcpy (DrvGfxROM2, tmp, 0x400000);

	BurnFree(tmp);
}

static void LuckywldGfxDecode()
{
	for (INT32 i = 0 ; i < 0x400000; i++) {
		DrvGfxROM1[i] = DrvGfxROM0[(i / 4) | ((i & 3) << 20)];
	}
}

static void luckywld_roz_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);

	for (INT32 i = 0; i < 0x4000; i++)
	{
		INT32 j = i & 0x1ff;

		switch (i >> 9)
		{
			case 0x00: j |= 0x1c00; break;
			case 0x01: j |= 0x0800; break;
			case 0x02: j |= 0x0000; break;
			case 0x08: j |= 0x1e00; break;
			case 0x09: j |= 0x0a00; break;
			case 0x0a: j |= 0x0200; break;
			case 0x10: j |= 0x2000; break;
			case 0x11: j |= 0x0c00; break;
			case 0x12: j |= 0x0400; break;
			case 0x18: j |= 0x2200; break;
			case 0x19: j |= 0x0e00; break;
			case 0x1a: j |= 0x0600; break;
		}

		memcpy (tmp + i * 0x100, DrvGfxROM3 + j * 0x100, 0x100);
	}

	memcpy (DrvGfxROM3, tmp, 0x400000);

	BurnFree(tmp);
}

static void metal_hawk_sprite_decode()
{
	UINT8 *data = DrvGfxROM0;
	for (INT32 i=0; i<0x200000; i+=32*32)
	{
		for (INT32 j=0; j<32*32; j+=32*4)
		{
			for (INT32 k=0; k<32; k+=4)
			{
				INT32 a = i+j+k+32;
				UINT8 v = data[a];
				data[a]   = data[a+3];
				data[a+3] = data[a+2];
				data[a+2] = data[a+1];
				data[a+1] = v;

				a += 32;
				v = data[a];
				data[a]   = data[a+2];
				data[a+2] = v;
				v = data[a+1];
				data[a+1] = data[a+3];
				data[a+3] = v;

				a += 32;
				data[a]   = data[a+1];
				data[a+1] = data[a+2];
				data[a+2] = data[a+3];
				data[a+3] = v;

				a = i+j+k;
				for (INT32 l=0; l<4; l++)
				{
					v = data[a+l+32];
					data[a+l+32] = data[a+l+32*3];
					data[a+l+32*3] = v;
				}
			}
		} 
	}

	for (INT32 i=0; i<0x200000; i+=32*32)
	{
		for (INT32 j=0; j<32; j++)
		{
			for (INT32 k=0; k<32; k++)
			{
				data[0x200000+i+j*32+k] = data[i+j+k*32];
			}
		}
	}

	for (INT32 i = 0; i < 0x400000; i++)
	{
		INT32 j = (i & 0xffffe0f) | ((i & 0x10) << 4) | ((i & 0x1e0) >> 1);

		DrvGfxROM1[j] = DrvGfxROM0[i];
	}
}

static void metal_hawk_roz_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	for (INT32 i = 0; i < 0x200000/0x100; i++)
	{
		INT32 j = (i & 0x01ff) | ((i & 0xe00) << 1) | ((i & 0x1000) >> 3);

		memcpy (tmp + i * 0x100, DrvGfxROM3 + j * 0x100, 0x100);
	}

	memcpy (DrvGfxROM3, tmp, 0x200000);

	BurnFree(tmp);
}

static INT32 Namcos2Init(void (*key_write)(UINT8,UINT16), UINT16 (*key_read)(UINT8))
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		DrvGfxDecode(); // decode sprites
		decode_layer_tiles();
	}

	default_68k_map(0);
	default_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	key_prot_read = key_read;
	key_prot_write = key_write;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 SgunnerCommonInit(void (*key_write)(UINT8,UINT16), UINT16 (*key_read)(UINT8))
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		LuckywldGfxDecode();
		decode_layer_tiles();
	}

	sgunner_68k_map(0);
	sgunner_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	key_prot_read = key_read;
	key_prot_write = key_write;

	GenericTilesInit();

	uses_gun = 1;
	BurnGunInit(2, false);

	weird_vbl = 1;

	DrvDoReset();

	return 0;
}

static INT32 Suzuka8hCommonInit(void (*key_write)(UINT8,UINT16), UINT16 (*key_read)(UINT8))
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		LuckywldGfxDecode();
		decode_layer_tiles();
	}

	c45RoadInit(~0, DrvC45PROM);

	luckywld_68k_map(0);
	luckywld_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	key_prot_read = key_read;
	key_prot_write = key_write;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 LuckywldInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		LuckywldGfxDecode();
		decode_layer_tiles();

		for (INT32 i = 0; i < 0x80000; i++) {
			DrvGfxROM5[i] = BITSWAP08(DrvGfxROM5[i], 0,1,2,3,4,5,6,7);
		}

		memcpy (DrvGfxROM3 + 0x1c0000, DrvGfxROM3 + 0x100000, 0x080000);

		luckywld_roz_decode();
	}

	c45RoadInit(~0, DrvC45PROM);

	luckywld_68k_map(0);
	luckywld_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	c169_roz_init(DrvRozRAM, DrvRozCtrl, roz_bitmap);

	GenericTilesInit();

	is_luckywld = 1;

	uses_gun = 1;
	BurnGunInit(2, false);

	pDrvDrawBegin = LuckywldDrawBegin;
	pDrvDrawLine = LuckywldDrawLine;

	DrvDoReset();

	return 0;
}

static INT32 MetlhawkInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  9, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000003, 10, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100001, 12, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100002, 13, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100003, 14, 4)) return 1;


		for (INT32 i = 0; i < 8; i++) {
			BurnLoadRom(DrvGfxROM3 + i * 0x40000, 19+i, 1);
		}

		metal_hawk_sprite_decode();
		decode_layer_tiles();
		metal_hawk_roz_decode();
	}

	metlhawk_68k_map(0);
	metlhawk_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	c169_roz_init(DrvRozRAM, DrvRozCtrl, roz_bitmap);

	GenericTilesInit();

	is_metlhawk = 1;

	weird_vbl = 1; // fix incorrect flashing in continue screen

	DrvDoReset();

	return 0;
}

static INT32 FinallapInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0x200000)) return 1;

		DrvGfxDecode(); // decode sprites
		decode_layer_tiles();
	}

	c45RoadInit(~0, DrvC45PROM);

	finallap_68k_map(0);
	finallap_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	GenericTilesInit();

	DrvDoReset();

	//pDrvDrawBegin = FinallapDrawBegin;
	//pDrvDrawLine = FinallapDrawLine;

	return 0;
}

static INT32 Finalap2Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		DrvGfxDecode(); // decode sprites
		decode_layer_tiles_finalap2();
	}

	c45RoadInit(~0, DrvC45PROM);

	finallap_68k_map(0);
	finallap_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	SekOpen(0);
	SekMapMemory(Drv68KData,		0x200000, 0x2fffff, MAP_ROM);
	SekMapMemory(Drv68KData + 0x140000,	0x340000, 0x3fffff, MAP_ROM);

	SekClose();

	SekOpen(1);
	SekMapMemory(Drv68KData,		0x200000, 0x2fffff, MAP_ROM);
	SekMapMemory(Drv68KData + 0x140000,	0x340000, 0x3fffff, MAP_ROM);

	SekClose();

	GenericTilesInit();

	DrvDoReset();

	//pDrvDrawBegin = FinallapDrawBegin;
	//pDrvDrawLine = FinallapDrawLine;

	return 0;
}

static INT32 FourtraxInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (Namcos2GetRoms(0)) return 1;

		DrvGfxDecode(); // decode sprites
		decode_layer_tiles();
	}

	c45RoadInit(~0, DrvC45PROM);

	finallap_68k_map(0);
	finallap_68k_map(1);
	namcos2_sound_init();
	namcos2_mcu_init();

	SekOpen(0);
	SekMapMemory(Drv68KData,		0x200000, 0x3fffff, MAP_ROM);
	SekClose();

	SekOpen(1);
	SekMapMemory(Drv68KData,		0x200000, 0x3fffff, MAP_ROM);
	SekClose();

	GenericTilesInit();

	DrvDoReset();

	pDrvDrawBegin = FinallapDrawBegin;
	pDrvDrawLine = FinallapDrawLine;

	return 0;
}

static INT32 Namcos2Exit()
{
	GenericTilesExit();

	SekExit();
	M6809Exit();
	m6805Exit();

	BurnYM2151Exit();
	c140_exit();

	c45RoadExit();

	BurnFree (AllMem);

	if (uses_gun) {
		BurnGunExit();
		uses_gun = 0;
	}

	key_prot_read = NULL;
	key_prot_write = NULL;

	pDrvDrawBegin = NULL;
	pDrvDrawLine = NULL;

	nvramcheck = 0;
	is_dirtfoxj = 0;
	is_luckywld = 0;
	is_metlhawk = 0;

	weird_vbl = 0;

	return 0;
}

static inline UINT16 get_palette_register(INT32 reg)
{
	UINT16 *ctrl = (UINT16*)(DrvPalRAM + 0x3000);
	return ((ctrl[reg*2] & 0xff) * 256 + (ctrl[reg*2+1] & 0xff));
}

#define restore_XY_clip(); \
	min_x = oldmin_x; \
	max_x = oldmax_x; \
	min_y = oldmin_y; \
	max_y = oldmax_y;

static inline void adjust_clip()
{
	if (min_x > nScreenWidth) min_x = nScreenWidth-1;
	if (min_x < 0) min_x = 0;
	if (max_x > nScreenWidth) max_x = nScreenWidth-1;
	if (max_x < 0) max_x = 0;
	if (min_y > nScreenHeight) min_y = nScreenHeight-1;
	if (min_y < 0) min_y = 0;
	if (max_y > nScreenHeight) max_y = nScreenHeight-1;
	if (max_y < 0) max_y = 0;
}

static void apply_clip()
{
	min_x = get_palette_register(0) - 0x4a;
	max_x = get_palette_register(1) - 0x4a - 1;
	min_y = get_palette_register(2) - 0x21;
	max_y = get_palette_register(3) - 0x21 - 1;

	adjust_clip();

	GenericTilesSetClip(min_x, max_x, min_y, max_y);
}

static void DrvRecalcPalette()
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 bank = 0; bank < 0x20; bank++)
	{
		INT32 pen = bank * 256;
		INT32 offset = ((pen & 0x1800) << 2) | (pen & 0x0700);

		for (INT32 i = 0; i < 256; i++)
		{
			UINT8 r = ram[offset + i + 0x0000];
			UINT8 g = ram[offset + i + 0x0800];
			UINT8 b = ram[offset + i + 0x1000];

			DrvPalette[pen + i] = BurnHighCol(r,g,b,0);

			DrvPalette[pen + i + 0x2000] = BurnHighCol(r/2,g/2,b/2,0); // shadow
		}
	}
}

static void draw_layer_with_masking_by_line(INT32 layer, INT32 color, INT32 line, INT32 priority)
{
	if (line < min_y || line > max_y) return;

	if (layer >= 6) return;

	if ((nSpriteEnable & (1<<layer)) == 0) return;

	INT32 x_offset_table[6] = { 44+4, 44+2, 44+1, 44+0, 0, 0 };
	INT32 offset[6] = { 0, 0x2000, 0x4000, 0x6000, 0x8010, 0x8810 };

	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;
	UINT16 *ram = (UINT16*)(DrvC123RAM + offset[layer]);

	INT32 sizex = (layer < 4) ? 64 : 36;
	INT32 sizey = (layer < 4) ? 64 : 28;

	INT32 sizex_full = sizex * 8;
	INT32 sizey_full = sizey * 8;

	INT32 flipscreen = (ctrl[1] & 0x8000) ? 0xffff : 0;

	INT32 x_offset = x_offset_table[layer];
	INT32 y_offset = (layer < 4) ? 24 : 0;

	INT32 scrollx = ((ctrl[1 + layer * 4] + x_offset) ^ flipscreen) % sizex_full;
	INT32 scrolly = ((ctrl[3 + layer * 4] + y_offset) ^ flipscreen) % sizey_full;

	if (flipscreen) {
		scrolly += 256 + 16;
		scrollx += 256;

		scrollx %= sizex_full;
		scrolly %= sizey_full;
	}

	if (layer >= 4) {
		scrollx = scrolly = 0;
	}

	color = ((color & 7) * 256) + 0x1000;

	INT32 sy = (scrolly + line) % sizey_full;

	UINT16 *dst = pTransDraw + (line * nScreenWidth);
	UINT8 *pri = pPrioDraw + (line * nScreenWidth);

	for (INT32 x = 0; x < nScreenWidth + 7; x+=8)
	{
		INT32 sx = (scrollx + x) % sizex_full;

		INT32 offs = (sx / 8) + ((sy / 8) * sizex);

		INT32 code = ram[offs];
		UINT8 *gfx = DrvGfxROM2 + (code * 8 * 8);
		UINT8 *msk = DrvGfxROM4 + (code * 8);

		gfx += (sy & 7) * 8;
		msk += (sy & 7);

		INT32 zx = x - (sx & 7);

		for (INT32 xx = 0; xx < 8; xx++, zx++)
		{
			if (zx < min_x || zx > max_x) continue;
	
			if (*msk & (0x80 >> xx))
			{
				dst[zx] = gfx[xx] + color;
				pri[zx] = (priority & 0x1000) ? ((priority * 2) & 0xff) : (priority & 0xff);
			}
		}
	}
}

static void draw_layer_with_masking(INT32 layer, INT32 color, INT32 priority)
{
	if (layer >= 6) return;

	if ((nSpriteEnable & (1<<layer)) == 0) return;

	INT32 x_offset_table[6] = { 44+4, 44+2, 44+1, 44+0, 0, 0 };
	INT32 offset[6] = { 0, 0x2000, 0x4000, 0x6000, 0x8010, 0x8810 };

	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;
	UINT16 *ram = (UINT16*)(DrvC123RAM + offset[layer]);

	INT32 sizex = (layer < 4) ? 64 : 36;
	INT32 sizey = (layer < 4) ? 64 : 28;

	INT32 sizex_full = sizex * 8;
	INT32 sizey_full = sizey * 8;

	INT32 flipscreen = (ctrl[1] & 0x8000) ? 0xffff : 0;

	INT32 x_offset = x_offset_table[layer];
	INT32 y_offset = (layer < 4) ? 24 : 0;

	INT32 scrollx = ((ctrl[1 + layer * 4] + x_offset) ^ flipscreen) % sizex_full;
	INT32 scrolly = ((ctrl[3 + layer * 4] + y_offset) ^ flipscreen) % sizey_full;

	if (flipscreen) {
		scrolly += 256 + 16;
		scrollx += 256;

		scrollx %= sizex_full;
		scrolly %= sizey_full;
	}

	if (layer >= 4) {
		scrollx = scrolly = 0;
	}

	color = ((color & 7) * 256) + 0x1000;

	for (INT32 offs = 0; offs < sizex * sizey; offs++)
	{
		INT32 sx = (offs % sizex) * 8;
		INT32 sy = (offs / sizex) * 8;

		sx -= scrollx;
		sy -= scrolly;
		if (sx < -7) sx += sizex_full;
		if (sy < -7) sy += sizey_full;

		if (flipscreen) {
			sx = (nScreenWidth - 8) - sx;
			sy = (nScreenHeight - 8) - sy;
		}

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code = ram[offs];
		UINT8 *gfx = DrvGfxROM2 + (code * 8 * 8);
		UINT8 *msk = DrvGfxROM4 + (code * 8);

		if (flipscreen)
		{
			gfx += 8 * 7;
			msk += 7;

			for (INT32 y = 0; y < 8; y++, msk--, gfx-=8)
			{
				if ((sy+y) < min_y) continue;
				if ((sy+y) > max_y) break;

				for (INT32 x = 0; x < 8; x++)
				{
					if ((sx + x) < min_x || (sx + x) > max_x) continue;

					if (*msk & (0x01 << x))
					{
						pTransDraw[(sy + y) * nScreenWidth + (sx + x)] = gfx[7 - x] + color;
						pPrioDraw[(sy + y) * nScreenWidth + (sx + x)] = (priority & 0x1000) ? ((priority * 2) & 0xff) : (priority & 0xff);
					}
				}
			}
		} else {
			for (INT32 y = 0; y < 8; y++, msk++, gfx+=8)
			{
				if ((sy+y) < min_y) continue;
				if ((sy+y) > max_y) break;

				for (INT32 x = 0; x < 8; x++)
				{
					if ((sx + x) < min_x || (sx + x) > max_x) continue;

					if (*msk & (0x80 >> x))
					{
						pTransDraw[(sy + y) * nScreenWidth + (sx + x)] = gfx[x] + color;
						pPrioDraw[(sy + y) * nScreenWidth + (sx + x)] = (priority & 0x1000) ? ((priority * 2) & 0xff) : (priority & 0xff);
					}
				}
			}
		}
	}
}

static void draw_layer_line(INT32 line, INT32 pri)
{
	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;

	for (INT32 i = 0; i < 6; i++)
	{
		if((ctrl[0x10+i] & 0xf) == (pri & 0xf))
		{
			layer_color = ctrl[0x18 + i];
			draw_layer_with_masking_by_line(i, layer_color, line, pri);
		}
	}
}

static void draw_layer(INT32 pri)
{
	UINT16 *ctrl = (UINT16*)DrvC123Ctrl;

	if (!max_x && !max_y) return;

	for (INT32 i = 0; i < 6; i++)
	{
		if((ctrl[0x10+i] & 0xf) == (pri & 0xf))
		{
			layer_color = ctrl[0x18 + i];
			draw_layer_with_masking(i, layer_color, pri);
		}
	}
}

static void predraw_roz_layer()
{
	if (roz_update_tiles == 0) return; // tiles not updated

	UINT16 *ram = (UINT16*)DrvRozRAM;

	for (INT32 offs = 0; offs < 256 * 256; offs++)
	{
		if (roz_dirty_tile[offs] == 0) continue;
		roz_dirty_tile[offs] = 0;

		INT32 sx = (offs & 0xff) * 8;
		INT32 sy = (offs / 256) * 8;

		UINT8 *gfx = DrvGfxROM3 + (ram[offs] * 0x40);
		UINT16 *dst = roz_bitmap + (sy * 256 * 8) + sx;

		for (INT32 y = 0; y < 8; y++, dst += (256 * 8)) {
			for (INT32 x = 0; x < 8; x++, gfx++) {
				dst[x] = *gfx;
			}
		}
	}

	roz_update_tiles = 0;
}

static void zdrawgfxzoom(UINT8 *gfx, INT32 tile_size, UINT32 code, UINT32 color, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy, INT32 scalex, INT32 scaley, INT32 priority, INT32 priority2, INT32 is_c355)
{
	if (!scalex || !scaley) return;

	if (!max_x && !max_y) return; //nothing to draw (zdrawgfxzoom draws a 1x1 pixel at 0,0 if max_x and max_y are 0)

	INT32 shadow_offset = (1)?0x2000:0;
	const UINT8 *source_base = gfx + (code * tile_size * tile_size);
	INT32 sprite_screen_height = (scaley*tile_size+0x8000)>>16;
	INT32 sprite_screen_width = (scalex*tile_size+0x8000)>>16;
	if (sprite_screen_width && sprite_screen_height)
	{
		INT32 dx = (tile_size<<16)/sprite_screen_width;
		INT32 dy = (tile_size<<16)/sprite_screen_height;

		INT32 ex = sx+sprite_screen_width;
		INT32 ey = sy+sprite_screen_height;

		INT32 x_index_base;
		INT32 y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if( flipy )
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if( sx < min_x)
		{ // clip left
			INT32 pixels = min_x-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}
		if( sy < min_y )
		{ // clip top
			INT32 pixels = min_y-sy;
			sy += pixels;
			y_index += pixels*dy;
		}
		if( ex > max_x+1 )
		{ // clip right
			INT32 pixels = ex-max_x-1;
			ex -= pixels;
		}
		if( ey > max_y+1 )
		{ // clip bottom
			INT32 pixels = ey-max_y-1;
			ey -= pixels;
		}

		if (!(ex > sx)) return;

		if (priority2 == -1)
		{
			INT32 y;
			UINT8 *priority_bitmap = pPrioDraw;

			for( y=sy; y<ey; y++ )
			{
				const UINT8 *source = source_base + (y_index>>16) * tile_size;
				UINT16 *dest = pTransDraw + y * nScreenWidth;
				UINT8 *pri = priority_bitmap + y * nScreenWidth;
				INT32 x, x_index = x_index_base;

				for (x = sx; x < ex; x++)
				{
					INT32 c = source[x_index>>16];
					if (c != 0xff)
					{
						if (pri[x] <= (priority))
						{
							if (color == 0xf00 && c==0xfe)
							{
								dest[x] |= shadow_offset;
							}
							else
							{
								dest[x] = c | color;
							}
							pri[x] = priority;
						}
					}
					x_index += dx;
				}
				y_index += dy;
			}
		} else
		{ // 2-priority mode. "priority" sprite:tile, "priority2" sprite:sprite
			// Marvel Land needs this for things like the end-of-level door, which is a lower prio than the player sprite - but the player sprite needs to be ontop of the door.
			INT32 y;
			UINT8 *priority_bitmap = pPrioDraw;
			UINT8 *priority_bitmap2 = SpritePrio;

			for( y=sy; y<ey; y++ )
			{
				const UINT8 *source = source_base + (y_index>>16) * tile_size;
				UINT16 *dest = pTransDraw + y * nScreenWidth;
				UINT8 *pri = priority_bitmap + y * nScreenWidth;
				UINT8 *pri2 = priority_bitmap2 + y * nScreenWidth;
				INT32 x, x_index = x_index_base;

				for (x = sx; x < ex; x++)
				{
					INT32 c = source[x_index>>16];
					if (c != 0xff)
					{
						if (pri[x] <= priority && pri2[x] <= priority2)
						{
							if (color == 0xf00 && c==0xfe)
							{
								dest[x] |= shadow_offset;
							}
							else
							{
								dest[x] = c | color;
							}

							pri2[x] = priority2; // only write sprite:sprite prio bitmap
						} else {
							if (!is_c355)
								pri2[x] = 0xff; // for masking effects in rthun2 (enemy rides down wall-chute)
						}
					}
					x_index += dx;
				}
				y_index += dy;
			}
		}
	}
}

static void draw_sprites_metalhawk()
{
	const UINT16 *pSource = (UINT16*)DrvSprRAM;

	for (INT32 loop=0; loop < 128; loop++)
	{
		INT32 ypos  = pSource[0];
		INT32 tile  = pSource[1];
		INT32 xpos  = pSource[3];
		INT32 flags = pSource[6];
		INT32 attrs = pSource[7];
		INT32 sizey = ((ypos>>10)&0x3f)+1;
		INT32 sizex = (xpos>>10)&0x3f;
		INT32 sprn  = tile&0x1fff;

		if( tile&0x2000 )
		{
			sprn&=0xfff;
		}
		else
		{
			sprn|=0x1000;
		}

		if( (sizey-1) && sizex )
		{
			INT32 bBigSprite = (flags&8);
			INT32 color = (attrs>>4)&0xf;
			INT32 sx = (xpos&0x03ff)-0x50+0x07;
			INT32 sy = (0x1ff-(ypos&0x01ff))-0x50+0x02;
			INT32 flipx = flags&2;
			INT32 flipy = flags&4;
			INT32 scalex = (sizex<<16)/(bBigSprite?0x20:0x10);
			INT32 scaley = (sizey<<16)/(bBigSprite?0x20:0x10);

			if (flags&0x01)
			{
				sprn |= 0x2000;
			}

			if (bBigSprite)
			{
				if( sizex < 0x20 ) sx -= (0x20-sizex)/0x8;
				if( sizey < 0x20 ) sy += (0x20-sizey)/0xC;
			}

			UINT8 *gfx;

			if (!bBigSprite)
			{
				gfx = DrvGfxROM1;

				sizex = 16;
				sizey = 16;
				scalex = 1<<16;
				scaley = 1<<16;
			}
			else
			{
				gfx = DrvGfxROM0;
				sprn >>= 2;
			}

			zdrawgfxzoom(gfx, bBigSprite ? 32 : 16, sprn, color * 256, flipx, flipy, sx, sy, scalex, scaley, (attrs & 0xf), -1, 0);
		}
		pSource += 8;
	}
}

static void predraw_c169_roz_bitmap()
{
	UINT16 *ram = (UINT16*)DrvRozRAM;
	UINT16 *dirty = (UINT16*)roz_dirty_tile;

	for (INT32 offs = 0; offs < 256 * 256; offs++)
	{
		INT32 sx = (offs & 0xff);
		INT32 sy = (offs / 0x100);

		INT32 ofst;
		if (sx >= 128) {
			ofst = (sx & 0x7f) + ((sy + 0x100) * 0x80);
		} else {
			ofst = sx + (sy * 0x80);
		}

		INT32 code = ram[ofst] & 0x3fff;
		if (code == dirty[ofst] && roz_update_tiles == 0) {
			continue;
		}
		dirty[ofst] = code;

		sx *= 16;
		sy *= 16;

		UINT8 *gfx = DrvGfxROM3 + (code * 0x100);
		UINT8 *msk = DrvGfxROM5 + (code * 0x020);

		UINT16 *dst = roz_bitmap + (sy * (256 * 16)) + sx;

		for (INT32 y = 0; y < 16; y++, gfx+=16,msk+=2)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if (msk[x/8] & (0x80 >> (x & 7)))
				{
					dst[x] = gfx[x];
				} else {
					dst[x] = 0x8000;
				}
			}

			dst += (256 * 16);
		}
	}

	roz_update_tiles = 0;
}

struct roz_param
{
	UINT32 size;
	UINT32 startx,starty;
	INT32 incxx,incxy,incyx,incyy;
	INT32 color;
	INT32 wrap;
};

static inline void draw_roz_helper_block(const struct roz_param *rozInfo, INT32 destx, INT32 desty, INT32 srcx, INT32 srcy, INT32 width, INT32 height, UINT32 size_mask, INT32 pri)
{
	INT32 desty_end = desty + height;

	INT32 end_incrx = rozInfo->incyx - (width * rozInfo->incxx);
	INT32 end_incry = rozInfo->incyy - (width * rozInfo->incxy);

	UINT16 *dest = pTransDraw + (desty * nScreenWidth) + destx;
	UINT8 *prio = pPrioDraw + (desty * nScreenWidth) + destx;
	INT32 dest_rowinc = nScreenWidth - width;

	while (desty < desty_end)
	{
		UINT16 *dest_end = dest + width;
		while (dest < dest_end)
		{
			UINT32 xpos = (srcx >> 16);
			UINT32 ypos = (srcy >> 16);

			if (rozInfo->wrap)
			{
				xpos &= size_mask;
				ypos &= size_mask;
			}
			else if ((xpos > rozInfo->size) || (ypos >= rozInfo->size))
			{
				srcx += rozInfo->incxx;
				srcy += rozInfo->incxy;
				dest++;
				prio++;
				continue;
			}

			INT32 pxl = roz_bitmap[(ypos * 256 * 8) + xpos];

			if (pxl != 0xff)
			{
				*dest = pxl + rozInfo->color;
				*prio = pri;
			}

			srcx += rozInfo->incxx;
			srcy += rozInfo->incxy;
			dest++;
			prio++;
		}
		srcx += end_incrx;
		srcy += end_incry;
		dest += dest_rowinc;
		prio += dest_rowinc;
		desty++;
	}
}

static void draw_roz_helper(const struct roz_param *rozInfo, INT32 pri)
{
	UINT32 size_mask = rozInfo->size - 1;
	UINT32 srcx = (rozInfo->startx + (min_x * rozInfo->incxx) + (min_y * rozInfo->incyx));
	UINT32 srcy = (rozInfo->starty + (min_x * rozInfo->incxy) + (min_y * rozInfo->incyy));
	INT32 destx = min_x;
	INT32 desty = min_y;

	INT32 row_count = (max_y - desty) + 1;
	INT32 row_block_count = row_count / 8;
	INT32 row_extra_count = row_count % 8;

	INT32 column_count = (max_x - destx) + 1;
	INT32 column_block_count = column_count / 8;
	INT32 column_extra_count = column_count % 8;

	INT32 row_block_size_incxx = 8 * rozInfo->incxx;
	INT32 row_block_size_incxy = 8 * rozInfo->incxy;
	INT32 row_block_size_incyx = 8 * rozInfo->incyx;
	INT32 row_block_size_incyy = 8 * rozInfo->incyy;

	for (INT32 i = 0; i < row_block_count; i++)
	{
		INT32 sx = srcx;
		INT32 sy = srcy;
		INT32 dx = destx;

		for (INT32 j = 0; j < column_block_count; j++)
		{
			draw_roz_helper_block(rozInfo, dx, desty, sx, sy, 8, 8, size_mask, pri);
			sx += row_block_size_incxx;
			sy += row_block_size_incxy;
			dx += 8;
		}

		if (column_extra_count)
		{
			draw_roz_helper_block(rozInfo, dx, desty, sx, sy, column_extra_count, 8,size_mask, pri);
		}
		srcx += row_block_size_incyx;
		srcy += row_block_size_incyy;
		desty += 8;
	}

	if (row_extra_count)
	{
		for (INT32 i = 0; i < column_block_count; i++)
		{
			draw_roz_helper_block(rozInfo, destx, desty, srcx, srcy, 8, row_extra_count, size_mask, pri);
			srcx += row_block_size_incxx;
			srcy += row_block_size_incxy;
			destx += 8;
		}

		if (column_extra_count)
		{
			draw_roz_helper_block(rozInfo, destx, desty, srcx, srcy, column_extra_count, row_extra_count, size_mask, pri);
		}
	}
}

static void draw_roz(INT32 pri)
{
	const INT32 xoffset = 38,yoffset = 0;
	struct roz_param rozParam;

	UINT16 *m_roz_ctrl = (UINT16*)DrvRozCtrl;

	if (!max_x && !max_y) return;

	rozParam.color = (gfx_ctrl & 0x0f00);
	rozParam.incxx  = (INT16)m_roz_ctrl[0];
	rozParam.incxy  = (INT16)m_roz_ctrl[1];
	rozParam.incyx  = (INT16)m_roz_ctrl[2];
	rozParam.incyy  = (INT16)m_roz_ctrl[3];
	rozParam.startx = (INT16)m_roz_ctrl[4];
	rozParam.starty = (INT16)m_roz_ctrl[5];
	rozParam.size = 2048;
	rozParam.wrap = 1;

	switch( m_roz_ctrl[7] )
	{
		case 0x4400: // (2048x2048)
		break;

		case 0x4488: // attract mode
			rozParam.wrap = 0;
		break;

		case 0x44cc: // stage1 demo
			rozParam.wrap = 0;
		break;

		case 0x44ee: // (256x256) used in Dragon Saber
			rozParam.wrap = 0;
			rozParam.size = 256;
		break;
	}

	rozParam.startx <<= 4;
	rozParam.starty <<= 4;
	rozParam.startx += xoffset * rozParam.incxx + yoffset * rozParam.incyx;
	rozParam.starty += xoffset * rozParam.incxy + yoffset * rozParam.incyy;

	rozParam.startx<<=8;
	rozParam.starty<<=8;
	rozParam.incxx<<=8;
	rozParam.incxy<<=8;
	rozParam.incyx<<=8;
	rozParam.incyy<<=8;

	draw_roz_helper(&rozParam, pri);
}

static void draw_sprites_bank(INT32 spritebank)
{
	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;
	INT32 offset = (spritebank & 0xf) * (128*4);

	//for (INT32 loop=0; loop < 128; loop++)
	for (INT32 loop=127; loop>=0; loop--) // fix rthun2 masking issue
	{
		INT32 word3 = m_spriteram[offset+(loop*4)+3];
		INT32 priority = word3 & 0xf;

		INT32 word0 = m_spriteram[offset+(loop*4)+0];
		INT32 word1 = m_spriteram[offset+(loop*4)+1];
		INT32 offset4 = m_spriteram[offset+(loop*4)+2];

		INT32 sizey=((word0>>10)&0x3f)+1;
		INT32 sizex=(word3>>10)&0x3f;

		if((word0&0x0200)==0) sizex>>=1;

		if((sizey-1) && sizex )
		{
			INT32 color  = (word3>>4)&0x000f;
			INT32 code   = word1 & 0x3fff;
			INT32 ypos   = (0x1ff-(word0&0x01ff))-0x50+0x02;
			INT32 xpos   = (offset4&0x03ff)-0x50+0x07;
			INT32 flipy  = word1&0x8000;
			INT32 flipx  = word1&0x4000;
			INT32 scalex = (sizex<<16)/((word0&0x0200)?0x20:0x10);
			INT32 scaley = (sizey<<16)/((word0&0x0200)?0x20:0x10);
			if(scalex && scaley)
			{
				INT32 size = (word0 >> 9) & 1; // 1 = 32x32, 0 = 16x16

				if (size == 1) code >>= 2;

				zdrawgfxzoom( size ? DrvGfxROM0 : DrvGfxROM1, size ? 32 : 16, code, color * 256, flipx,flipy, xpos,ypos, scalex,scaley, priority, loop, 0);
			}
		}
	}
}

static void DrvPrio2Clear()
{
	memset(SpritePrio, 0, 300 * 300);
}

static void draw_sprites()
{
	DrvPrio2Clear();

	if (!sprite_bankL) { // game didn't select sprite bank for frame(rare), default to first bank.
		sprite_bankL = 1<<0;
		sprite_bankSL[0][0] = 0;
		sprite_bankSL[0][1] = nScreenHeight;
	}

	for (INT32 i = 0; i < 0x10; i++)
	{
		if (sprite_bankL & 1<<i) {
			INT32 oldmin_y = min_y;
			INT32 oldmax_y = max_y;
			min_y = sprite_bankSL[i][0];
			max_y = sprite_bankSL[i][1];
			if (min_y < oldmin_y) min_y = oldmin_y;
			if (max_y > oldmax_y) max_y = oldmax_y;

			//bprintf(0, _T("draw spritebank %X, min_y %d max_y %d.\n"), i, min_y, max_y); /*keep!*/
			draw_sprites_bank(i);

			min_y = oldmin_y;
			max_y = oldmax_y;
		}
	}
	sprite_bankL = 0;
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	INT32 roz_enable = (gfx_ctrl & 0x7000) ? 1 : 0;

	if (roz_enable) predraw_roz_layer();

	BurnTransferClear(0x4000);
}

static void DrvDrawLine(INT32 line)
{
	INT32 roz_enable = (gfx_ctrl & 0x7000) ? 1 : 0;

	for (INT32 pri = 0; pri < 8; pri++)
	{
		draw_layer_line(line, pri);

		if (((gfx_ctrl & 0x7000) >> 12) == pri )
		{
			if (roz_enable) {
				INT32 oldmin_y = min_y;
				INT32 oldmax_y = max_y;
				min_y = (line >= min_y && line <= max_y) ? line : 255;
				max_y = (line+1 <= max_y) ? line+1 : 0;
				if (nBurnLayer & 1) draw_roz(pri);
				min_y = oldmin_y;
				max_y = oldmax_y;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (!pDrvDrawBegin) { // not line based, fall back to default
		if (DrvRecalc) {
			DrvRecalcPalette();
			DrvRecalc = 0;
		}

		apply_clip();

		INT32 roz_enable = (gfx_ctrl & 0x7000) ? 1 : 0;

		if (roz_enable) predraw_roz_layer();

		BurnTransferClear(0x4000);

		for (INT32 pri = 0; pri < 8; pri++)
		{
			draw_layer(pri);

			if (((gfx_ctrl & 0x7000) >> 12) == pri)
			{
				if (roz_enable) {
					if (nBurnLayer & 1) draw_roz(pri);
				}
			}
		}
	}

	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void c355_obj_draw_sprite(const UINT16 *pSource, INT32 zpos)
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;
	const UINT16 *spriteformat16 = &spriteram16[0x4000/2];
	const UINT16 *spritetile16   = &spriteram16[0x8000/2];

	UINT16 palette     = pSource[6];

	INT32 pri = (palette >> 4) & 0xf;

	UINT16 linkno      = pSource[0];
	UINT16 offset      = pSource[1];
	INT32 hpos        = pSource[2];
	INT32 vpos        = pSource[3];
	UINT16 hsize       = pSource[4];
	UINT16 vsize       = pSource[5];

	if (linkno*4>=0x4000/2) return;

	INT32 xscroll = (INT16)c355_obj_position[1];
	INT32 yscroll = (INT16)c355_obj_position[0];
	xscroll &= 0x1ff; if( xscroll & 0x100 ) xscroll |= ~0x1ff;
	yscroll &= 0x1ff; if( yscroll & 0x100 ) yscroll |= ~0x1ff;
	xscroll += 0x26;
	yscroll += 0x19;

	hpos -= xscroll;
	vpos -= yscroll;
	const UINT16 *pWinAttr = &spriteram16[0x2400/2+((palette>>8)&0xf)*4];

	INT32 oldmin_x = min_x;
	INT32 oldmax_x = max_x;
	INT32 oldmin_y = min_y;
	INT32 oldmax_y = max_y;
	min_x = pWinAttr[0] - xscroll;
	max_x = pWinAttr[1] - xscroll;
	min_y = pWinAttr[2] - yscroll;
	max_y = pWinAttr[3] - yscroll;
	adjust_clip(); // make sane

	if (min_x < oldmin_x) min_x = oldmin_x;
	if (max_x > oldmax_x) max_x = oldmax_x;
	if (min_y < oldmin_y) min_y = oldmin_y;
	if (max_y > oldmax_y) max_y = oldmax_y;

	hpos&=0x7ff; if( hpos&0x400 ) hpos |= ~0x7ff; /* sign extend */
	vpos&=0x7ff; if( vpos&0x400 ) vpos |= ~0x7ff; /* sign extend */

	INT32 tile_index      = spriteformat16[linkno*4+0];
	INT32 format          = spriteformat16[linkno*4+1];
	INT32 dx              = spriteformat16[linkno*4+2];
	INT32 dy              = spriteformat16[linkno*4+3];
	INT32 num_cols        = (format>>4)&0xf;
	INT32 num_rows        = (format)&0xf;

	if( num_cols == 0 ) num_cols = 0x10;
	INT32 flipx = (hsize&0x8000)?1:0;
	hsize &= 0x3ff;
	if( hsize == 0 ) { restore_XY_clip(); return; }
	UINT32 zoomx = (hsize<<16)/(num_cols*16);
	dx = (dx*zoomx+0x8000)>>16;
	if( flipx )
	{
		hpos += dx;
	}
	else
	{
		hpos -= dx;
	}

	if( num_rows == 0 ) num_rows = 0x10;
	INT32 flipy = (vsize&0x8000)?1:0;
	vsize &= 0x3ff;
	if( vsize == 0 ) { restore_XY_clip(); return; }
	UINT32 zoomy = (vsize<<16)/(num_rows*16);
	dy = (dy*zoomy+0x8000)>>16;
	if( flipy )
	{
		vpos += dy;
	}
	else
	{
		vpos -= dy;
	}

	INT32 color = palette&0xf;

	UINT32 source_height_remaining = num_rows*16;
	UINT32 screen_height_remaining = vsize;
	INT32 sy = vpos;
	for(INT32 row=0; row<num_rows; row++)
	{
		INT32 tile_screen_height = 16*screen_height_remaining/source_height_remaining;
		zoomy = (screen_height_remaining<<16)/source_height_remaining;
		if( flipy )
		{
			sy -= tile_screen_height;
		}
		UINT32 source_width_remaining = num_cols*16;
		UINT32 screen_width_remaining = hsize;
		INT32 sx = hpos;
		for(INT32 col=0; col<num_cols; col++)
		{
			INT32 tile_screen_width = 16*screen_width_remaining/source_width_remaining;
			zoomx = (screen_width_remaining<<16)/source_width_remaining;
			if( flipx )
			{
				sx -= tile_screen_width;
			}
			INT32 tile = spritetile16[tile_index++];
			if( (tile&0x8000)==0 )
			{
				INT32 size = 0;

				zdrawgfxzoom( size ? DrvGfxROM0 : DrvGfxROM1, size ? 32 : 16, tile + offset, color * 256, flipx,flipy, sx, sy, zoomx, zoomy, pri, zpos, 1);
			}
			if( !flipx )
			{
				sx += tile_screen_width;
			}
			screen_width_remaining -= tile_screen_width;
			source_width_remaining -= 16;
		}
		if( !flipy )
		{
			sy += tile_screen_height;
		}
		screen_height_remaining -= tile_screen_height;
		source_height_remaining -= 16;
	}

	restore_XY_clip();
}

static void c355_obj_draw_list(const UINT16 *pSpriteList16, const UINT16 *pSpriteTable)
{
	for (INT32 i = 0; i < 256; i++)
	{
		UINT16 which = pSpriteList16[i];
		c355_obj_draw_sprite(&pSpriteTable[(which&0xff)*8], i);
		if (which&0x100) break;
	}
}

static void c355_draw_sprites()
{
	UINT16 *m_c355_obj_ram = (UINT16*)DrvSprRAM;

	DrvPrio2Clear();

	c355_obj_draw_list(&m_c355_obj_ram[0x02000/2], &m_c355_obj_ram[0x00000/2]);
	c355_obj_draw_list(&m_c355_obj_ram[0x14000/2], &m_c355_obj_ram[0x10000/2]);
}

static INT32 SgunnerDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	BurnTransferClear(0x4000);

	for (INT32 pri = 0; pri < 8; pri++)
	{
		draw_layer(pri);
	}

	c355_draw_sprites();
	BurnTransferCopy(DrvPalette);

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}

	return 0;
}

static void FinallapDrawBegin()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	BurnTransferClear(0x4000);
}

static void FinallapDrawLine(INT32 line)
{
	for (INT32 pri=0; pri < 16; pri++)
	{
		if ((pri&1) == 0)
		{
			draw_layer_line(line, pri/2 | 0x1000);
		}
	}
}

static INT32 FinallapDraw()
{
	if (!pDrvDrawBegin) { // not line based, fall back to default
		if (DrvRecalc) {
			DrvRecalcPalette();
			DrvRecalc = 0;
		}

		apply_clip();

		BurnTransferClear(0x4000);

		for (INT32 pri = 0; pri < 16; pri++)
		{
			if ((pri&1) == 0)
			{
				draw_layer(pri/2 | 0x1000);
			}
		}
	}
	if (nBurnLayer & 1) c45RoadDraw();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void LuckywldDrawBegin()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	predraw_c169_roz_bitmap();

	BurnTransferClear(0x4000);
}

static void LuckywldDrawLine(INT32 line)
{
	for (INT32 pri = 0; pri < 16; pri++)
	{
		if ((pri&1) == 0)
		{
			draw_layer_line(line, pri/2 | 0x1000); // 0x1000 = write pri*2 to priobuf
		}

		if (nBurnLayer & 2) c169_roz_draw(pri, line); // guys in mirror
	}
}

static INT32 LuckywldDraw()
{
	if (!pDrvDrawBegin) { // not line based, fall back to default
		if (DrvRecalc) {
			DrvRecalcPalette();
			DrvRecalc = 0;
		}

		apply_clip();

		predraw_c169_roz_bitmap();

		BurnTransferClear(0x4000);

		for (INT32 pri = 0; pri < 16; pri++)
		{
			if ((pri&1) == 0)
			{
				draw_layer(pri/2 | 0x1000);
			}

			if (nBurnLayer & 2) c169_roz_draw(pri, -1);
		}
	}

	if (nBurnLayer & 1) c45RoadDraw();
	if (nBurnLayer & 4) c355_draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Suzuka8hDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	BurnTransferClear(0x4000);

	for (INT32 pri = 0; pri < 16; pri++)
	{
		if ((pri&1) == 0)
		{
			draw_layer(pri/2);
		}
	}

	if (nBurnLayer & 1) c45RoadDraw();
	if (nBurnLayer & 4) c355_draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 MetlhawkDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	apply_clip();

	predraw_c169_roz_bitmap();

	BurnTransferClear(0x4000);

	for (INT32 pri = 0; pri < 16; pri++)
	{
		if ((pri&1) == 0)
		{
			draw_layer(pri/2 | 0x1000);
		}

		if (nBurnLayer & 1) c169_roz_draw(pri, -1);
	}

	if (nBurnLayer & 2) draw_sprites_metalhawk();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (nvramcheck) {
		FreshEEPROMCheck();
	}

	SekNewFrame();
	M6809NewFrame();
	m6805NewFrame();

	{
		memset (DrvInputs, 0xff, 4);
		for (INT32 i = 0; i < 8; i++) { 
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (uses_gun) {
			BurnGunMakeInputs(0, (INT16)DrvGun0, (INT16)DrvGun1);
			BurnGunMakeInputs(1, (INT16)DrvGun2, (INT16)DrvGun3);
		}

	}

	INT32 nInterleave = 264*2;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[4] = { (INT32)((double)12288000 / 60.606061), (INT32)((double)12288000 / 60.606061), (INT32)((double)2048000 / 60.606061), (INT32)((double)2048000 / 1 / 60.606061) };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };
	INT32 vbloffs = 8;

	switch (weird_vbl) {
		case 1: vbloffs = 16; break;
		case 2: vbloffs = -16; break;
	}

	M6809Open(0);
	m6805Open(0);

	UINT16 *ctrl = (UINT16*)(DrvPalRAM + 0x3000);

	if (pBurnDraw && pDrvDrawBegin) {
		pDrvDrawBegin();
	}

	for (INT32 i = 0; i < nInterleave; i++) {
		scanline = i / 2;

		position = (((ctrl[0xa] & 0xff) * 256 + (ctrl[0xb] & 0xff)) - 35) & 0xff;

		SekOpen(0);
		if (i == (240+vbloffs)*2) SekSetIRQLine(irq_vblank[0], CPU_IRQSTATUS_AUTO); // should ack in c148
		if (i == position*2) SekSetIRQLine(irq_pos[0], CPU_IRQSTATUS_ACK);

		INT32 nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nNext - nCyclesDone[0]);
		INT32 segment = (maincpu_run_ended) ? maincpu_run_cycles : SekTotalCycles();
		maincpu_run_ended = maincpu_run_cycles = 0;
		SekClose();

		if (pBurnDraw && pDrvDrawLine && i&1)
			pDrvDrawLine(i/2);

		SekOpen(1);
		if (sub_cpu_in_reset) {
			nCyclesDone[1] += segment - SekTotalCycles();
			SekIdle(segment - SekTotalCycles());
		} else {
			if (i == (240+vbloffs)*2) SekSetIRQLine(irq_vblank[1], CPU_IRQSTATUS_AUTO); // should ack in c148
			if (i == position*2) SekSetIRQLine(irq_pos[1], CPU_IRQSTATUS_ACK);
			nCyclesDone[1] += SekRun(segment - SekTotalCycles());
		}
		SekClose();

		if (sub_cpu_in_reset) {
			nCyclesDone[3] += ((segment / 6) - m6805TotalCycles());
		} else {
			nCyclesDone[3] += m6805Run(((segment / 6) - m6805TotalCycles()));
			if (i == 240*2) {
				hd63705SetIrqLine(0, CPU_IRQSTATUS_ACK);
			}
			if (i == 16*2) {
				hd63705SetIrqLine(0, CPU_IRQSTATUS_NONE);
			}
		}

		if (audio_cpu_in_reset) {
			nCyclesDone[2] += segment / 6;
		} else {
			nCyclesDone[2] += M6809Run((segment / 6) - M6809TotalCycles());

			if (i == 1*2 || i == 133*2) {
				M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
				M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
			}
		}

		if (pBurnSoundOut && i%8 == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		c140_update(pBurnSoundOut, nBurnSoundLen);
	}

	m6805Close();
	M6809Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_ROM) {
		ba.Data		= Drv68KROM[0];
		ba.nLen		= 0x40000;
		ba.nAddress	= 0;
		ba.szName	= "68K #0 ROM";
		BurnAcb(&ba);

		ba.Data		= Drv68KROM[1];
		ba.nLen		= 0x040000;
		ba.nAddress	= 0x80000;
		ba.szName	= "68k #1 ROM";
		BurnAcb(&ba);

		ba.Data		= Drv68KData;
		ba.nLen		= 0x200000;
		ba.nAddress	= 0x200000;
		ba.szName	= "68K Shared ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= DrvC123RAM;
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x400000;
		ba.szName	= "Bg RAM";
		BurnAcb(&ba);

		ba.Data		= Drv68KRAM[0];
		ba.nLen		= 0x40000;
		ba.nAddress	= 0x100000;
		ba.szName	= "68k 0 RAM";
		BurnAcb(&ba);

		ba.Data		= Drv68KRAM[1];
		ba.nLen		= 0x40000;
		ba.nAddress	= 0x140000;
		ba.szName	= "68k 1 RAM";
		BurnAcb(&ba);

		ba.Data		= DrvDPRAM;
		ba.nLen		= 0x0000800;
		ba.nAddress	= 0x460000;
		ba.szName	= "Shared (DP) RAM";
		BurnAcb(&ba);

		ba.Data		= DrvC139RAM;
		ba.nLen		= 0x0004000;
		ba.nAddress	= 0x480000;
		ba.szName	= "C139 RAM";
		BurnAcb(&ba);

		ba.Data		= DrvPalRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x440000;
		ba.szName	= "Palette";
		BurnAcb(&ba);

		ba.Data		= DrvSprRAM;
		ba.nLen		= 0x0014400;
		ba.nAddress	= 0xc00000;
		ba.szName	= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data		= DrvRozRAM;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0xc20000;
		ba.szName	= "ROZ RAM";
		BurnAcb(&ba);

		ba.Data		= DrvMCURAM;
		ba.nLen		= 0x0000200;
		ba.nAddress	= 0xe00000;
		ba.szName	= "MCU RAM";
		BurnAcb(&ba);

		ba.Data		= DrvM6809RAM;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0xe10000;
		ba.szName	= "M6809 RAM";
		BurnAcb(&ba);

		ba.Data		= DrvC123Ctrl;
		ba.nLen		= 0x000040;
		ba.nAddress	= 0xe20000;
		ba.szName	= "C123 Control RAM";
		BurnAcb(&ba);

		ba.Data		= DrvRozCtrl;
		ba.nLen		= 0x000020;
		ba.nAddress	= 0xe30000;
		ba.szName	= "Roz Control RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvEEPROM;
		ba.nLen		= 0x02000;
		ba.nAddress	= 0x180000;
		ba.szName	= "EEPROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
	
		SekScan(nAction);
		m6805Scan(nAction);
		M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		c140_scan(nAction, pnMin);

		if (uses_gun) {
			BurnGunScan();
		}

		SCAN_VAR(gfx_ctrl);

		SCAN_VAR(irq_reg);
		SCAN_VAR(irq_cpu);
		SCAN_VAR(irq_vblank);
		SCAN_VAR(irq_ex);
		SCAN_VAR(irq_pos);
		SCAN_VAR(irq_sci);
		SCAN_VAR(bus_reg);

		SCAN_VAR(c355_obj_position);

		SCAN_VAR(audio_cpu_in_reset);
		SCAN_VAR(sub_cpu_in_reset);
		SCAN_VAR(sound_bank);

		SCAN_VAR(min_x);
		SCAN_VAR(max_x);
		SCAN_VAR(min_y);
		SCAN_VAR(max_y);

		SCAN_VAR(mcu_analog_ctrl);
		SCAN_VAR(mcu_analog_complete);
		SCAN_VAR(mcu_analog_data);

		SCAN_VAR(finallap_prot_count);

		SCAN_VAR(key_sendval);

		BurnRandomScan(nAction);

		c45RoadState(nAction); // here

		if (nAction & ACB_WRITE) {
			memset (roz_dirty_tile, 1, 0x10000);
			roz_update_tiles = 1;

			c45RoadState(nAction); // and here!

			M6809Open(0);
			sound_bankswitch(sound_bank);
			M6809Close();
		}
	}

 	return 0;
}


// Assault (Rev B)

static struct BurnRomInfo assaultRomDesc[] = {
	{ "at2mp0b.bin",	0x10000, 0x801f71c5, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "at2mp1b.bin",	0x10000, 0x72312d4f, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "at1sp0.bin",		0x10000, 0x0de2a0da, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "at1sp1.bin",		0x10000, 0x02d15fbe, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "at1snd0.bin",	0x20000, 0x1d1ffe12, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65b.bin",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "atobj0.bin",		0x20000, 0x22240076, 0x05 | BRF_GRA },           //  7 Sprites
	{ "atobj1.bin",		0x20000, 0x2284a8e8, 0x05 | BRF_GRA },           //  8
	{ "atobj2.bin",		0x20000, 0x51425476, 0x05 | BRF_GRA },           //  9
	{ "atobj3.bin",		0x20000, 0x791f42ce, 0x05 | BRF_GRA },           // 10
	{ "atobj4.bin",		0x20000, 0x4782e1b0, 0x05 | BRF_GRA },           // 11
	{ "atobj5.bin",		0x20000, 0xf5d158cf, 0x05 | BRF_GRA },           // 12
	{ "atobj6.bin",		0x20000, 0x12f6a569, 0x05 | BRF_GRA },           // 13
	{ "atobj7.bin",		0x20000, 0x06a929f2, 0x05 | BRF_GRA },           // 14

	{ "atchr0.bin",		0x20000, 0x6f8e968a, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "atchr1.bin",		0x20000, 0x88cf7cbe, 0x06 | BRF_GRA },           // 16

	{ "atroz0.bin",		0x20000, 0x8c247a97, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "atroz1.bin",		0x20000, 0xe44c475b, 0x07 | BRF_GRA },           // 18
	{ "atroz2.bin",		0x20000, 0x770f377f, 0x07 | BRF_GRA },           // 19
	{ "atroz3.bin",		0x20000, 0x01d93d0b, 0x07 | BRF_GRA },           // 20
	{ "atroz4.bin",		0x20000, 0xf96feab5, 0x07 | BRF_GRA },           // 21
	{ "atroz5.bin",		0x20000, 0xda2f0d9e, 0x07 | BRF_GRA },           // 22
	{ "atroz6.bin",		0x20000, 0x9089e477, 0x07 | BRF_GRA },           // 23
	{ "atroz7.bin",		0x20000, 0x62b2783a, 0x07 | BRF_GRA },           // 24

	{ "atshape.bin",	0x20000, 0xdfcad82b, 0x08 | BRF_GRA },           // 25 Layer Tiles Mask Data

	{ "at1dat0.13s",	0x20000, 0x844890f4, 0x09 | BRF_PRG | BRF_ESS }, // 26 Shared 68K Data
	{ "at1dat1.13p",	0x20000, 0x21715313, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "atvoi1.bin",		0x80000, 0xd36a649e, 0x0a | BRF_SND },           // 28 C140 Samples
};

STD_ROM_PICK(assault)
STD_ROM_FN(assault)

static INT32 AssaultInit()
{
	return Namcos2Init(NULL, NULL);
}

struct BurnDriver BurnDrvAssault = {
	"assault", NULL, NULL, NULL, "1988",
	"Assault (Rev B)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, assaultRomInfo, assaultRomName, NULL, NULL, NULL, NULL, AssaultInputInfo, AssaultDIPInfo,
	AssaultInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Assault (Japan)

static struct BurnRomInfo assaultjRomDesc[] = {
	{ "at1_mp0.bin",	0x10000, 0x2d3e5c8c, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "at1_mp1.bin",	0x10000, 0x851cec3a, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "at1sp0.bin",		0x10000, 0x0de2a0da, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "at1sp1.bin",		0x10000, 0x02d15fbe, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "at1snd0.bin",	0x20000, 0x1d1ffe12, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65b.bin",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "atobj0.bin",		0x20000, 0x22240076, 0x05 | BRF_GRA },           //  7 Sprites
	{ "atobj1.bin",		0x20000, 0x2284a8e8, 0x05 | BRF_GRA },           //  8
	{ "atobj2.bin",		0x20000, 0x51425476, 0x05 | BRF_GRA },           //  9
	{ "atobj3.bin",		0x20000, 0x791f42ce, 0x05 | BRF_GRA },           // 10
	{ "atobj4.bin",		0x20000, 0x4782e1b0, 0x05 | BRF_GRA },           // 11
	{ "atobj5.bin",		0x20000, 0xf5d158cf, 0x05 | BRF_GRA },           // 12
	{ "atobj6.bin",		0x20000, 0x12f6a569, 0x05 | BRF_GRA },           // 13
	{ "atobj7.bin",		0x20000, 0x06a929f2, 0x05 | BRF_GRA },           // 14

	{ "atchr0.bin",		0x20000, 0x6f8e968a, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "atchr1.bin",		0x20000, 0x88cf7cbe, 0x06 | BRF_GRA },           // 16

	{ "atroz0.bin",		0x20000, 0x8c247a97, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "atroz1.bin",		0x20000, 0xe44c475b, 0x07 | BRF_GRA },           // 18
	{ "atroz2.bin",		0x20000, 0x770f377f, 0x07 | BRF_GRA },           // 19
	{ "atroz3.bin",		0x20000, 0x01d93d0b, 0x07 | BRF_GRA },           // 20
	{ "atroz4.bin",		0x20000, 0xf96feab5, 0x07 | BRF_GRA },           // 21
	{ "atroz5.bin",		0x20000, 0xda2f0d9e, 0x07 | BRF_GRA },           // 22
	{ "atroz6.bin",		0x20000, 0x9089e477, 0x07 | BRF_GRA },           // 23
	{ "atroz7.bin",		0x20000, 0x62b2783a, 0x07 | BRF_GRA },           // 24

	{ "atshape.bin",	0x20000, 0xdfcad82b, 0x08 | BRF_GRA },           // 25 Layer Tiles Mask Data

	{ "at1dat0.13s",	0x20000, 0x844890f4, 0x09 | BRF_PRG | BRF_ESS }, // 26 Shared 68K Data
	{ "at1dat1.13p",	0x20000, 0x21715313, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "atvoi1.bin",		0x80000, 0xd36a649e, 0x0a | BRF_SND },           // 28 C140 Samples
};

STD_ROM_PICK(assaultj)
STD_ROM_FN(assaultj)

struct BurnDriver BurnDrvAssaultj = {
	"assaultj", "assault", NULL, NULL, "1988",
	"Assault (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, assaultjRomInfo, assaultjRomName, NULL, NULL, NULL, NULL, AssaultInputInfo, AssaultDIPInfo,
	AssaultInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Assault Plus (Japan)

static struct BurnRomInfo assaultpRomDesc[] = {
	{ "mpr0.bin",		0x10000, 0x97519f9f, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mpr1.bin",		0x10000, 0xc7f437c7, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "at1sp0.bin",		0x10000, 0x0de2a0da, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "at1sp1.bin",		0x10000, 0x02d15fbe, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "at1snd0.bin",	0x20000, 0x1d1ffe12, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65b.bin",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "atobj0.bin",		0x20000, 0x22240076, 0x05 | BRF_GRA },           //  7 Sprites
	{ "atobj1.bin",		0x20000, 0x2284a8e8, 0x05 | BRF_GRA },           //  8
	{ "atobj2.bin",		0x20000, 0x51425476, 0x05 | BRF_GRA },           //  9
	{ "atobj3.bin",		0x20000, 0x791f42ce, 0x05 | BRF_GRA },           // 10
	{ "atobj4.bin",		0x20000, 0x4782e1b0, 0x05 | BRF_GRA },           // 11
	{ "atobj5.bin",		0x20000, 0xf5d158cf, 0x05 | BRF_GRA },           // 12
	{ "atobj6.bin",		0x20000, 0x12f6a569, 0x05 | BRF_GRA },           // 13
	{ "atobj7.bin",		0x20000, 0x06a929f2, 0x05 | BRF_GRA },           // 14

	{ "atchr0.bin",		0x20000, 0x6f8e968a, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "atchr1.bin",		0x20000, 0x88cf7cbe, 0x06 | BRF_GRA },           // 16

	{ "atroz0.bin",		0x20000, 0x8c247a97, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "atroz1.bin",		0x20000, 0xe44c475b, 0x07 | BRF_GRA },           // 18
	{ "atroz2.bin",		0x20000, 0x770f377f, 0x07 | BRF_GRA },           // 19
	{ "atroz3.bin",		0x20000, 0x01d93d0b, 0x07 | BRF_GRA },           // 20
	{ "atroz4.bin",		0x20000, 0xf96feab5, 0x07 | BRF_GRA },           // 21
	{ "atroz5.bin",		0x20000, 0xda2f0d9e, 0x07 | BRF_GRA },           // 22
	{ "atroz6.bin",		0x20000, 0x9089e477, 0x07 | BRF_GRA },           // 23
	{ "atroz7.bin",		0x20000, 0x62b2783a, 0x07 | BRF_GRA },           // 24

	{ "atshape.bin",	0x20000, 0xdfcad82b, 0x08 | BRF_GRA },           // 25 Layer Tiles Mask Data

	{ "at1dat0.13s",	0x20000, 0x844890f4, 0x09 | BRF_PRG | BRF_ESS }, // 26 Shared 68K Data
	{ "at1dat1.13p",	0x20000, 0x21715313, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "atvoi1.bin",		0x80000, 0xd36a649e, 0x0a | BRF_SND },           // 28 C140 Samples Samples
};

STD_ROM_PICK(assaultp)
STD_ROM_FN(assaultp)

static INT32 AssaultpInit()
{
	return Namcos2Init(NULL, NULL);
}

struct BurnDriver BurnDrvAssaultp = {
	"assaultp", "assault", NULL, NULL, "1988",
	"Assault Plus (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, assaultpRomInfo, assaultpRomName, NULL, NULL, NULL, NULL, AssaultInputInfo, AssaultDIPInfo,
	AssaultpInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Ordyne (World)

static struct BurnRomInfo ordyneRomDesc[] = {
	{ "or2_mp0.mpr0",	0x20000, 0x31a1742b, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "or2_mp1.mpr1",	0x20000, 0xc80c6b73, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "or1_sp0.spr0",	0x10000, 0x01ef6638, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "or1_sp1.spr1",	0x10000, 0xb632adc3, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "or1_sd.snd0",	0x20000, 0xc41e5d22, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2_c65b.3f",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "or_obj-0.obj0",	0x20000, 0x67b2b9e4, 0x05 | BRF_GRA },           //  7 Sprites
	{ "or_obj-1.obj1",	0x20000, 0x8a54fa5e, 0x05 | BRF_GRA },           //  8
	{ "or_obj-2.obj2",	0x20000, 0xa2c1cca0, 0x05 | BRF_GRA },           //  9
	{ "or_obj-3.obj3",	0x20000, 0xe0ad292c, 0x05 | BRF_GRA },           // 10
	{ "or_obj-4.obj4",	0x20000, 0x7aefba59, 0x05 | BRF_GRA },           // 11
	{ "or_obj-5.obj5",	0x20000, 0xe4025be9, 0x05 | BRF_GRA },           // 12
	{ "or_obj-6.obj6",	0x20000, 0xe284c30c, 0x05 | BRF_GRA },           // 13
	{ "or_obj-7.obj7",	0x20000, 0x262b7112, 0x05 | BRF_GRA },           // 14

	{ "or_chr-0.chr0",	0x20000, 0xe7c47934, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "or_chr-1.chr1",	0x20000, 0x874b332d, 0x06 | BRF_GRA },           // 16
	{ "or_chr-3.chr3",	0x20000, 0x5471a834, 0x06 | BRF_GRA },           // 17
	{ "or_chr-5.chr5",	0x20000, 0xa7d3a296, 0x06 | BRF_GRA },           // 18
	{ "or_chr-6.chr6",	0x20000, 0x3adc09c8, 0x06 | BRF_GRA },           // 19
	{ "or2_chr7.chr7",	0x20000, 0x8c0d2ab7, 0x06 | BRF_GRA },           // 20

	{ "or_roz-0.roz0",	0x20000, 0xc88a9e6b, 0x07 | BRF_GRA },           // 21 Roz Layer Tiles
	{ "or_roz-1.roz1",	0x20000, 0xc20cc749, 0x07 | BRF_GRA },           // 22
	{ "or_roz-2.roz2",	0x20000, 0x148c9866, 0x07 | BRF_GRA },           // 23
	{ "or_roz-3.roz3",	0x20000, 0x4e727b7e, 0x07 | BRF_GRA },           // 24
	{ "or_roz-4.roz4",	0x20000, 0x17b04396, 0x07 | BRF_GRA },           // 25

	{ "or_shape.shape",	0x20000, 0x7aec9dee, 0x08 | BRF_GRA },           // 26 Layer Tiles Mask Data

	{ "or1_d0.13s",		0x20000, 0xde214f7a, 0x09 | BRF_PRG | BRF_ESS }, // 27 Shared 68K Data
	{ "or1_d1.13p",		0x20000, 0x25e3e6c8, 0x09 | BRF_PRG | BRF_ESS }, // 28

	{ "or_voi1.voice1",	0x80000, 0x369e0bca, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "or_voi2.voice2",	0x80000, 0x9f4cd7b5, 0x0a | BRF_SND },           // 30
	
	{ "lh5762.6n",		0x02000, 0x90db1bf6, 0x00 | BRF_OPT },
};

STD_ROM_PICK(ordyne)
STD_ROM_FN(ordyne)

static UINT16 ordyne_key_read(UINT8 offset)
{
	switch (offset)
	{
		case 2: return 0x1001;
		case 3: return 0x0001;
		case 4: return 0x0110;
		case 5: return 0x0010;
		case 6: return 0x00B0;
		case 7: return 0x00B0;
	}

	return BurnRandom();
}

static INT32 OrdyneInit()
{
	INT32 rc = Namcos2Init(NULL, ordyne_key_read);

	if (!rc) {
		nvramcheck = 1;
	}

	return rc;
}

static INT32 OrdynejInit()
{
	INT32 rc = Namcos2Init(NULL, ordyne_key_read);

	if (!rc) {
		nvramcheck = 2;
	}

	return rc;
}

struct BurnDriver BurnDrvOrdyne = {
	"ordyne", NULL, NULL, NULL, "1988",
	"Ordyne (World)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, ordyneRomInfo, ordyneRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	OrdyneInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Ordyne (Japan)

static struct BurnRomInfo ordynejRomDesc[] = {
	{ "or1_mp0.mpr0",	0x20000, 0xf5929ed3, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "or1_mp1.mpr1",	0x20000, 0xc1c8c1e2, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "or1_sp0.spr0",	0x10000, 0x01ef6638, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "or1_sp1.spr1",	0x10000, 0xb632adc3, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "or1_sd.snd0",	0x20000, 0xc41e5d22, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2_c65b.3f",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "or_obj-0.obj0",	0x20000, 0x67b2b9e4, 0x05 | BRF_GRA },           //  7 Sprites
	{ "or_obj-1.obj1",	0x20000, 0x8a54fa5e, 0x05 | BRF_GRA },           //  8
	{ "or_obj-2.obj2",	0x20000, 0xa2c1cca0, 0x05 | BRF_GRA },           //  9
	{ "or_obj-3.obj3",	0x20000, 0xe0ad292c, 0x05 | BRF_GRA },           // 10
	{ "or_obj-4.obj4",	0x20000, 0x7aefba59, 0x05 | BRF_GRA },           // 11
	{ "or_obj-5.obj5",	0x20000, 0xe4025be9, 0x05 | BRF_GRA },           // 12
	{ "or_obj-6.obj6",	0x20000, 0xe284c30c, 0x05 | BRF_GRA },           // 13
	{ "or_obj-7.obj7",	0x20000, 0x262b7112, 0x05 | BRF_GRA },           // 14

	{ "or_chr-0.chr0",	0x20000, 0xe7c47934, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "or_chr-1.chr1",	0x20000, 0x874b332d, 0x06 | BRF_GRA },           // 16
	{ "or_chr-3.chr3",	0x20000, 0x5471a834, 0x06 | BRF_GRA },           // 17
	{ "or_chr-5.chr5",	0x20000, 0xa7d3a296, 0x06 | BRF_GRA },           // 18
	{ "or_chr-6.chr6",	0x20000, 0x3adc09c8, 0x06 | BRF_GRA },           // 19
	{ "or_chr-7.chr7",	0x20000, 0xf050a152, 0x06 | BRF_GRA },           // 20

	{ "or_roz-0.roz0",	0x20000, 0xc88a9e6b, 0x07 | BRF_GRA },           // 21 Roz Layer Tiles
	{ "or_roz-1.roz1",	0x20000, 0xc20cc749, 0x07 | BRF_GRA },           // 22
	{ "or_roz-2.roz2",	0x20000, 0x148c9866, 0x07 | BRF_GRA },           // 23
	{ "or_roz-3.roz3",	0x20000, 0x4e727b7e, 0x07 | BRF_GRA },           // 24
	{ "or_roz-4.roz4",	0x20000, 0x17b04396, 0x07 | BRF_GRA },           // 25

	{ "or_shape.shape",	0x20000, 0x7aec9dee, 0x08 | BRF_GRA },           // 26 Layer Tiles Mask Data

	{ "or1_d0.13s",		0x20000, 0xde214f7a, 0x09 | BRF_PRG | BRF_ESS }, // 27 Shared 68K Data
	{ "or1_d1.13p",		0x20000, 0x25e3e6c8, 0x09 | BRF_PRG | BRF_ESS }, // 28

	{ "or_voi1.voice1",	0x80000, 0x369e0bca, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "or_voi2.voice2",	0x80000, 0x9f4cd7b5, 0x0a | BRF_SND },           // 30
	
	{ "lh5762.6n",		0x02000, 0x90db1bf6, 0x00 | BRF_OPT },
};

STD_ROM_PICK(ordynej)
STD_ROM_FN(ordynej)

struct BurnDriver BurnDrvOrdynej = {
	"ordynej", "ordyne", NULL, NULL, "1988",
	"Ordyne (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, ordynejRomInfo, ordynejRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	OrdynejInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Ordyne (Japan, English Version)

static struct BurnRomInfo ordynejeRomDesc[] = {
	{ "or1_mp0e.mpr0",	0x20000, 0x5e2f9052, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "or1_mp1e.mpr1",	0x20000, 0x367a8fcf, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "or1_sp0.spr0",	0x10000, 0x01ef6638, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "or1_sp1.spr1",	0x10000, 0xb632adc3, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "or1_sd.snd0",	0x20000, 0xc41e5d22, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2_c65b.3f",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "or_obj-0.obj0",	0x20000, 0x67b2b9e4, 0x05 | BRF_GRA },           //  7 Sprites
	{ "or_obj-1.obj1",	0x20000, 0x8a54fa5e, 0x05 | BRF_GRA },           //  8
	{ "or_obj-2.obj2",	0x20000, 0xa2c1cca0, 0x05 | BRF_GRA },           //  9
	{ "or_obj-3.obj3",	0x20000, 0xe0ad292c, 0x05 | BRF_GRA },           // 10
	{ "or_obj-4.obj4",	0x20000, 0x7aefba59, 0x05 | BRF_GRA },           // 11
	{ "or_obj-5.obj5",	0x20000, 0xe4025be9, 0x05 | BRF_GRA },           // 12
	{ "or_obj-6.obj6",	0x20000, 0xe284c30c, 0x05 | BRF_GRA },           // 13
	{ "or_obj-7.obj7",	0x20000, 0x262b7112, 0x05 | BRF_GRA },           // 14

	{ "or_chr-0.chr0",	0x20000, 0xe7c47934, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "or_chr-1.chr1",	0x20000, 0x874b332d, 0x06 | BRF_GRA },           // 16
	{ "or_chr-3.chr3",	0x20000, 0x5471a834, 0x06 | BRF_GRA },           // 17
	{ "or_chr-5.chr5",	0x20000, 0xa7d3a296, 0x06 | BRF_GRA },           // 18
	{ "or_chr-6.chr6",	0x20000, 0x3adc09c8, 0x06 | BRF_GRA },           // 19
	{ "or1_ch7e.chr7",	0x20000, 0x8c0d2ab7, 0x06 | BRF_GRA },           // 20

	{ "or_roz-0.roz0",	0x20000, 0xc88a9e6b, 0x07 | BRF_GRA },           // 21 Roz Layer Tiles
	{ "or_roz-1.roz1",	0x20000, 0xc20cc749, 0x07 | BRF_GRA },           // 22
	{ "or_roz-2.roz2",	0x20000, 0x148c9866, 0x07 | BRF_GRA },           // 23
	{ "or_roz-3.roz3",	0x20000, 0x4e727b7e, 0x07 | BRF_GRA },           // 24
	{ "or_roz-4.roz4",	0x20000, 0x17b04396, 0x07 | BRF_GRA },           // 25

	{ "or_shape.shape",	0x20000, 0x7aec9dee, 0x08 | BRF_GRA },           // 26 Layer Tiles Mask Data

	{ "or1_d0.13s",		0x20000, 0xde214f7a, 0x09 | BRF_PRG | BRF_ESS }, // 27 Shared 68K Data
	{ "or1_d1.13p",		0x20000, 0x25e3e6c8, 0x09 | BRF_PRG | BRF_ESS }, // 28

	{ "or_voi1.voice1",	0x80000, 0x369e0bca, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "or_voi2.voice2",	0x80000, 0x9f4cd7b5, 0x0a | BRF_SND },           // 30
	
	{ "lh5762.6n",		0x02000, 0x90db1bf6, 0x00 | BRF_OPT },
};

STD_ROM_PICK(ordyneje)
STD_ROM_FN(ordyneje)

struct BurnDriver BurnDrvOrdyneje = {
	"ordyneje", "ordyne", NULL, NULL, "1988",
	"Ordyne (Japan, English Version)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, ordynejeRomInfo, ordynejeRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	OrdyneInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Cosmo Gang the Video (US)

static struct BurnRomInfo cosmogngRomDesc[] = {
	{ "co2mpr0.bin",	0x20000, 0x2632c209, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "co2mpr1.bin",	0x20000, 0x65840104, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "co1spr0.bin",	0x20000, 0xbba2c28f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "co1spr1.bin",	0x20000, 0xc029b459, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "co2_s0",		0x20000, 0x4ca59338, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "co1obj0.bin",	0x80000, 0x5df8ce0c, 0x05 | BRF_GRA },           //  7 Sprites
	{ "co1obj1.bin",	0x80000, 0x3d152497, 0x05 | BRF_GRA },           //  8
	{ "co1obj2.bin",	0x80000, 0x4e50b6ee, 0x05 | BRF_GRA },           //  9
	{ "co1obj3.bin",	0x80000, 0x7beed669, 0x05 | BRF_GRA },           // 10

	{ "co1chr0.bin",	0x80000, 0xee375b3e, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "co1chr1.bin",	0x80000, 0x0149de65, 0x06 | BRF_GRA },           // 12
	{ "co1chr2.bin",	0x80000, 0x93d565a0, 0x06 | BRF_GRA },           // 13
	{ "co1chr3.bin",	0x80000, 0x4d971364, 0x06 | BRF_GRA },           // 14

	{ "co1roz0.bin",	0x80000, 0x2bea6951, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles

	{ "co1sha0.bin",	0x80000, 0x063a70cc, 0x08 | BRF_GRA },           // 16 Layer Tiles Mask Data

	{ "co1dat0.13s",	0x20000, 0xb53da2ae, 0x09 | BRF_PRG | BRF_ESS }, // 17 Shared 68K Data
	{ "co1dat1.13p",	0x20000, 0xd21ad10b, 0x09 | BRF_PRG | BRF_ESS }, // 18

	{ "co2voi1.bin",	0x80000, 0x5a301349, 0x0a | BRF_SND },           // 19 C140 Samples
	{ "co2voi2.bin",	0x80000, 0xa27cb45a, 0x0a | BRF_SND },           // 20
};

STD_ROM_PICK(cosmogng)
STD_ROM_FN(cosmogng)

static UINT16 cosmogng_key_read(UINT8 offset)
{
	if (offset == 3) return 0x014a;

	return BurnRandom();
}

static INT32 CosmogngInit()
{
	return Namcos2Init(NULL, cosmogng_key_read);
}

struct BurnDriver BurnDrvCosmogng = {
	"cosmogng", NULL, NULL, NULL, "1991",
	"Cosmo Gang the Video (US)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, cosmogngRomInfo, cosmogngRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	CosmogngInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Mirai Ninja (Japan)

static struct BurnRomInfo mirninjaRomDesc[] = {
	{ "mn_mpr0e.bin",	0x10000, 0xfa75f977, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mn_mpr1e.bin",	0x10000, 0x58ddd464, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mn1_spr0.bin",	0x10000, 0x3f1a17be, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "mn1_spr1.bin",	0x10000, 0x2bc66f60, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "mn_snd0.bin",	0x20000, 0x6aa1ae84, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65b.bin",	0x08000, 0xe9f2922a, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "mn_obj0.bin",	0x20000, 0x6bd1e290, 0x05 | BRF_GRA },           //  7 Sprites
	{ "mn_obj1.bin",	0x20000, 0x5e8503be, 0x05 | BRF_GRA },           //  8
	{ "mn_obj2.bin",	0x20000, 0xa96d9b45, 0x05 | BRF_GRA },           //  9
	{ "mn_obj3.bin",	0x20000, 0x0086ef8b, 0x05 | BRF_GRA },           // 10
	{ "mn_obj4.bin",	0x20000, 0xb3f48755, 0x05 | BRF_GRA },           // 11
	{ "mn_obj5.bin",	0x20000, 0xc21e995b, 0x05 | BRF_GRA },           // 12
	{ "mn_obj6.bin",	0x20000, 0xa052c582, 0x05 | BRF_GRA },           // 13
	{ "mn_obj7.bin",	0x20000, 0x1854c6f5, 0x05 | BRF_GRA },           // 14

	{ "mn_chr0.bin",	0x20000, 0x4f66df26, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "mn_chr1.bin",	0x20000, 0xf5de5ea7, 0x06 | BRF_GRA },           // 16
	{ "mn_chr2.bin",	0x20000, 0x9ff61924, 0x06 | BRF_GRA },           // 17
	{ "mn_chr3.bin",	0x20000, 0xba208bf5, 0x06 | BRF_GRA },           // 18
	{ "mn_chr4.bin",	0x20000, 0x0ef00aff, 0x06 | BRF_GRA },           // 19
	{ "mn_chr5.bin",	0x20000, 0x4cd9d377, 0x06 | BRF_GRA },           // 20
	{ "mn_chr6.bin",	0x20000, 0x114aca76, 0x06 | BRF_GRA },           // 21
	{ "mn_chr7.bin",	0x20000, 0x2d5705d3, 0x06 | BRF_GRA },           // 22

	{ "mn_roz0.bin",	0x20000, 0x677a4f25, 0x07 | BRF_GRA },           // 23 Roz Layer Tiles
	{ "mn_roz1.bin",	0x20000, 0xf00ae572, 0x07 | BRF_GRA },           // 24

	{ "mn_sha.bin",		0x20000, 0xc28af90f, 0x08 | BRF_GRA },           // 25 Layer Tiles Mask Data

	{ "mn1_dat0.13s",	0x20000, 0x104bcca8, 0x09 | BRF_PRG | BRF_ESS }, // 26 Shared 68K Data
	{ "mn1_dat1.13p",	0x20000, 0xd2a918fb, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "mn_voi1.bin",	0x80000, 0x2ca3573c, 0x0a | BRF_SND },           // 28 C140 Samples
	{ "mn_voi2.bin",	0x80000, 0x466c3b47, 0x0a | BRF_SND },           // 29
};

STD_ROM_PICK(mirninja)
STD_ROM_FN(mirninja)

static UINT16 mirninja_key_read(UINT8 offset)
{
	if (offset == 7) return 0x00b1;

	return BurnRandom();
}

static INT32 MirninjaInit()
{
	return Namcos2Init(NULL, mirninja_key_read);
}

struct BurnDriver BurnDrvMirninja = {
	"mirninja", NULL, NULL, NULL, "1988",
	"Mirai Ninja (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, mirninjaRomInfo, mirninjaRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	MirninjaInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Phelios

static struct BurnRomInfo pheliosRomDesc[] = {
	{ "ps2_mpr0.mpr0",	0x20000, 0x28f04e1a, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ps2_mpr1.mpr1",	0x20000, 0x2546501a, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ps2_spr0.spr0",	0x10000, 0xe9c6987e, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ps2_spr1.spr1",	0x10000, 0x02b074fb, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ps2_snd0.snd0",	0x20000, 0xda694838, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2_c65c.3f",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "ps_obj-0.obj0",	0x40000, 0xf323db2b, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ps_obj-1.obj1",	0x40000, 0xfaf7c2f5, 0x05 | BRF_GRA },           //  8
	{ "ps_obj-2.obj2",	0x40000, 0x828178ba, 0x05 | BRF_GRA },           //  9
	{ "ps_obj-3.obj3",	0x40000, 0xe84771c8, 0x05 | BRF_GRA },           // 10
	{ "ps_obj-4.obj4",	0x40000, 0x81ea86c6, 0x05 | BRF_GRA },           // 11
	{ "ps_obj-5.obj5",	0x40000, 0xaaebd51a, 0x05 | BRF_GRA },           // 12
	{ "ps_obj-6.obj6",	0x40000, 0x032ea497, 0x05 | BRF_GRA },           // 13
	{ "ps_obj-7.obj7",	0x40000, 0xf6183b36, 0x05 | BRF_GRA },           // 14

	{ "ps_chr-0.chr0",	0x20000, 0x668b6670, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "ps_chr-1.chr1",	0x20000, 0x80c30742, 0x06 | BRF_GRA },           // 16
	{ "ps_chr-2.chr2",	0x20000, 0xf4e11bf7, 0x06 | BRF_GRA },           // 17
	{ "ps_chr-3.chr3",	0x20000, 0x97a93dc5, 0x06 | BRF_GRA },           // 18
	{ "ps_chr-4.chr4",	0x20000, 0x81d965bf, 0x06 | BRF_GRA },           // 19
	{ "ps_chr-5.chr5",	0x20000, 0x8ca72d35, 0x06 | BRF_GRA },           // 20
	{ "ps_chr-6.chr6",	0x20000, 0xda3543a9, 0x06 | BRF_GRA },           // 21

	{ "ps_roz-0.roz0",	0x20000, 0x68043d7e, 0x07 | BRF_GRA },           // 22 Roz Layer Tiles
	{ "ps_roz-1.roz1",	0x20000, 0x029802b4, 0x07 | BRF_GRA },           // 23
	{ "ps_roz-2.roz2",	0x20000, 0xbf0b76dc, 0x07 | BRF_GRA },           // 24
	{ "ps_roz-3.roz3",	0x20000, 0x9c821455, 0x07 | BRF_GRA },           // 25
	{ "ps_roz-4.roz4",	0x20000, 0x63a39b7a, 0x07 | BRF_GRA },           // 26
	{ "ps_roz-5.roz5",	0x20000, 0xfc5a99d0, 0x07 | BRF_GRA },           // 27
	{ "ps_roz-6.roz6",	0x20000, 0xa2a17587, 0x07 | BRF_GRA },           // 28

	{ "ps_shape.shape",	0x20000, 0x58e26fcf, 0x08 | BRF_GRA },           // 29 Layer Tiles Mask Data

	{ "ps2_dat0.13s",	0x20000, 0xee4194b0, 0x09 | BRF_PRG | BRF_ESS }, // 30 Shared 68K Data
	{ "ps2_dat1.13p",	0x20000, 0x5b22d714, 0x09 | BRF_PRG | BRF_ESS }, // 31

	{ "ps_voi-1.voice1",	0x80000, 0xf67376ed, 0x0a | BRF_SND },           // 32 C140 Samples
};

STD_ROM_PICK(phelios)
STD_ROM_FN(phelios)

static UINT16 phelios_key_read(UINT8 offset)
{
	switch (offset)
	{
		case 0: return 0x00f0;
		case 1: return 0x0ff0;
		case 2: return 0x00b2;
		case 3: return 0x00b2;
		case 4: return 0x000f;
		case 5: return 0xf00f;
		case 7: return 0x00b2;
	}

	return BurnRandom();
}

static INT32 PheliosInit()
{
	return Namcos2Init(NULL, phelios_key_read);
}

struct BurnDriver BurnDrvPhelios = {
	"phelios", NULL, NULL, NULL, "1988",
	"Phelios\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, pheliosRomInfo, pheliosRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	PheliosInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Phelios (Japan)

static struct BurnRomInfo pheliosjRomDesc[] = {
	{ "ps1_mpr0.mpr0",	0x20000, 0xbfbe96c6, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ps1_mpr1.mpr1",	0x20000, 0xf5c0f883, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ps1_spr0.spr0",	0x10000, 0xe9c6987e, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ps1_spr1.spr1",	0x10000, 0x02b074fb, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ps1_snd0.snd0",	0x20000, 0xda694838, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2_c65c.3f",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "ps_obj-0.obj0",	0x40000, 0xf323db2b, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ps_obj-1.obj1",	0x40000, 0xfaf7c2f5, 0x05 | BRF_GRA },           //  8
	{ "ps_obj-2.obj2",	0x40000, 0x828178ba, 0x05 | BRF_GRA },           //  9
	{ "ps_obj-3.obj3",	0x40000, 0xe84771c8, 0x05 | BRF_GRA },           // 10
	{ "ps_obj-4.obj4",	0x40000, 0x81ea86c6, 0x05 | BRF_GRA },           // 11
	{ "ps_obj-5.obj5",	0x40000, 0xaaebd51a, 0x05 | BRF_GRA },           // 12
	{ "ps_obj-6.obj6",	0x40000, 0x032ea497, 0x05 | BRF_GRA },           // 13
	{ "ps_obj-7.obj7",	0x40000, 0xf6183b36, 0x05 | BRF_GRA },           // 14

	{ "ps_chr-0.chr0",	0x20000, 0x668b6670, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "ps_chr-1.chr1",	0x20000, 0x80c30742, 0x06 | BRF_GRA },           // 16
	{ "ps_chr-2.chr2",	0x20000, 0xf4e11bf7, 0x06 | BRF_GRA },           // 17
	{ "ps_chr-3.chr3",	0x20000, 0x97a93dc5, 0x06 | BRF_GRA },           // 18
	{ "ps_chr-4.chr4",	0x20000, 0x81d965bf, 0x06 | BRF_GRA },           // 19
	{ "ps_chr-5.chr5",	0x20000, 0x8ca72d35, 0x06 | BRF_GRA },           // 20
	{ "ps_chr-6.chr6",	0x20000, 0xda3543a9, 0x06 | BRF_GRA },           // 21

	{ "ps_roz-0.roz0",	0x20000, 0x68043d7e, 0x07 | BRF_GRA },           // 22 Roz Layer Tiles
	{ "ps_roz-1.roz1",	0x20000, 0x029802b4, 0x07 | BRF_GRA },           // 23
	{ "ps_roz-2.roz2",	0x20000, 0xbf0b76dc, 0x07 | BRF_GRA },           // 24
	{ "ps_roz-3.roz3",	0x20000, 0x9c821455, 0x07 | BRF_GRA },           // 25
	{ "ps_roz-4.roz4",	0x20000, 0x63a39b7a, 0x07 | BRF_GRA },           // 26
	{ "ps_roz-5.roz5",	0x20000, 0xfc5a99d0, 0x07 | BRF_GRA },           // 27
	{ "ps_roz-6.roz6",	0x20000, 0xa2a17587, 0x07 | BRF_GRA },           // 28

	{ "ps_shape.shape",	0x20000, 0x58e26fcf, 0x08 | BRF_GRA },           // 29 Layer Tiles Mask Data

	{ "ps1_dat0.13s",	0x20000, 0xee4194b0, 0x09 | BRF_PRG | BRF_ESS }, // 30 Shared 68K Data
	{ "ps1_dat1.13p",	0x20000, 0x5b22d714, 0x09 | BRF_PRG | BRF_ESS }, // 31

	{ "ps_voi-1.voice1",	0x80000, 0xf67376ed, 0x0a | BRF_SND },           // 32 C140 Samples
};

STD_ROM_PICK(pheliosj)
STD_ROM_FN(pheliosj)

struct BurnDriver BurnDrvPheliosj = {
	"pheliosj", "phelios", NULL, NULL, "1988",
	"Phelios (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, pheliosjRomInfo, pheliosjRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	PheliosInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};



// Marvel Land (US)

static struct BurnRomInfo marvlandRomDesc[] = {
	{ "mv2_mpr0",		0x20000, 0xd8b14fee, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mv2_mpr1",		0x20000, 0x29ff2738, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mv2_spr0",		0x10000, 0xaa418f29, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "mv2_spr1",		0x10000, 0xdbd94def, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "mv2_snd0",		0x20000, 0xa5b99162, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "mv1-obj0.bin",	0x40000, 0x73a29361, 0x05 | BRF_GRA },           //  7 Sprites
	{ "mv1-obj1.bin",	0x40000, 0xabbe4a99, 0x05 | BRF_GRA },           //  8
	{ "mv1-obj2.bin",	0x40000, 0x753659e0, 0x05 | BRF_GRA },           //  9
	{ "mv1-obj3.bin",	0x40000, 0xd1ce7339, 0x05 | BRF_GRA },           // 10

	{ "mv1-chr0.bin",	0x40000, 0x1c7e8b4f, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "mv1-chr1.bin",	0x40000, 0x01e4cafd, 0x06 | BRF_GRA },           // 12
	{ "mv1-chr2.bin",	0x40000, 0x198fcc6f, 0x06 | BRF_GRA },           // 13
	{ "mv1-chr3.bin",	0x40000, 0xed6f22a5, 0x06 | BRF_GRA },           // 14

	{ "mv1-roz0.bin",	0x20000, 0x7381a5a9, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles
	{ "mv1-roz1.bin",	0x20000, 0xe899482e, 0x07 | BRF_GRA },           // 16
	{ "mv1-roz2.bin",	0x20000, 0xde141290, 0x07 | BRF_GRA },           // 17
	{ "mv1-roz3.bin",	0x20000, 0xe310324d, 0x07 | BRF_GRA },           // 18
	{ "mv1-roz4.bin",	0x20000, 0x48ddc5a9, 0x07 | BRF_GRA },           // 19

	{ "mv1-sha.bin",	0x40000, 0xa47db5d3, 0x08 | BRF_GRA },           // 20 Layer Tiles Mask Data

	{ "mv2_dat0.13s",	0x20000, 0x62e6318b, 0x09 | BRF_PRG | BRF_ESS }, // 21 Shared 68K Data
	{ "mv2_dat1.13p",	0x20000, 0x8a6902ca, 0x09 | BRF_PRG | BRF_ESS }, // 22
	{ "mv2_dat2.13r",	0x20000, 0xf5c6408c, 0x09 | BRF_PRG | BRF_ESS }, // 23
	{ "mv2_dat3.13n",	0x20000, 0x6df76955, 0x09 | BRF_PRG | BRF_ESS }, // 24

	{ "mv1-voi1.bin",	0x80000, 0xde5cac09, 0x0a | BRF_SND },           // 25 C140 Samples Samples
};

STD_ROM_PICK(marvland)
STD_ROM_FN(marvland)

static void marvland_key_write(UINT8 offset, UINT16 data)
{
	if ((offset == 5 && data == 0x615e) || (offset == 6 && data == 0x1001)) {
		key_sendval = offset & 1;
	}
}

static UINT16 marvland_key_read(UINT8 offset)
{
	switch (offset)
	{
		case 0: return 0x0010;
		case 1: return 0x0110;
		case 4: return 0x00BE;
		case 6: return 0x1001;
		case 7: return (key_sendval == 1) ? 0xbe : 1;
	}

	return BurnRandom();
}

static INT32 MarvlandInit()
{
	return Namcos2Init(marvland_key_write, marvland_key_read);
}

struct BurnDriver BurnDrvMarvland = {
	"marvland", NULL, NULL, NULL, "1989",
	"Marvel Land (US)\0", "Bad music - use the Japan version", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, marvlandRomInfo, marvlandRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	MarvlandInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Marvel Land (Japan)

static struct BurnRomInfo marvlandjRomDesc[] = {
	{ "mv1-mpr0.bin",	0x10000, 0x8369120f, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mv1-mpr1.bin",	0x10000, 0x6d5442cc, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mv1-spr0.bin",	0x10000, 0xc3909925, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "mv1-spr1.bin",	0x10000, 0x1c5599f5, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "mv1-snd0.bin",	0x20000, 0x51b8ccd7, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "mv1-obj0.bin",	0x40000, 0x73a29361, 0x05 | BRF_GRA },           //  7 Sprites
	{ "mv1-obj1.bin",	0x40000, 0xabbe4a99, 0x05 | BRF_GRA },           //  8
	{ "mv1-obj2.bin",	0x40000, 0x753659e0, 0x05 | BRF_GRA },           //  9
	{ "mv1-obj3.bin",	0x40000, 0xd1ce7339, 0x05 | BRF_GRA },           // 10

	{ "mv1-chr0.bin",	0x40000, 0x1c7e8b4f, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "mv1-chr1.bin",	0x40000, 0x01e4cafd, 0x06 | BRF_GRA },           // 12
	{ "mv1-chr2.bin",	0x40000, 0x198fcc6f, 0x06 | BRF_GRA },           // 13
	{ "mv1-chr3.bin",	0x40000, 0xed6f22a5, 0x06 | BRF_GRA },           // 14

	{ "mv1-roz0.bin",	0x20000, 0x7381a5a9, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles
	{ "mv1-roz1.bin",	0x20000, 0xe899482e, 0x07 | BRF_GRA },           // 16
	{ "mv1-roz2.bin",	0x20000, 0xde141290, 0x07 | BRF_GRA },           // 17
	{ "mv1-roz3.bin",	0x20000, 0xe310324d, 0x07 | BRF_GRA },           // 18
	{ "mv1-roz4.bin",	0x20000, 0x48ddc5a9, 0x07 | BRF_GRA },           // 19

	{ "mv1-sha.bin",	0x40000, 0xa47db5d3, 0x08 | BRF_GRA },           // 20 Layer Tiles Mask Data

	{ "mv1-dat0.13s",	0x20000, 0xe15f412e, 0x09 | BRF_PRG | BRF_ESS }, // 21 Shared 68K Data
	{ "mv1-dat1.13p",	0x20000, 0x73e1545a, 0x09 | BRF_PRG | BRF_ESS }, // 22

	{ "mv1-voi1.bin",	0x80000, 0xde5cac09, 0x0a | BRF_SND },           // 23 C140 Samples Samples
};

STD_ROM_PICK(marvlandj)
STD_ROM_FN(marvlandj)

struct BurnDriver BurnDrvMarvlandj = {
	"marvlandj", "marvland", NULL, NULL, "1989",
	"Marvel Land (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, marvlandjRomInfo, marvlandjRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	MarvlandInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Valkyrie No Densetsu (Japan)

static struct BurnRomInfo valkyrieRomDesc[] = {
	{ "wd1mpr0.bin",	0x20000, 0x94111a2e, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "wd1mpr1.bin",	0x20000, 0x57b5051c, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "wd1spr0.bin",	0x10000, 0xb2398321, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "wd1spr1.bin",	0x10000, 0x38dba897, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "wd1snd0.bin",	0x20000, 0xd0fbf58b, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "wdobj0.bin",		0x40000, 0xe8089451, 0x05 | BRF_GRA },           //  7 Sprites
	{ "wdobj1.bin",		0x40000, 0x7ca65666, 0x05 | BRF_GRA },           //  8
	{ "wdobj2.bin",		0x40000, 0x7c159407, 0x05 | BRF_GRA },           //  9
	{ "wdobj3.bin",		0x40000, 0x649f8760, 0x05 | BRF_GRA },           // 10
	{ "wdobj4.bin",		0x40000, 0x7ca39ae7, 0x05 | BRF_GRA },           // 11
	{ "wdobj5.bin",		0x40000, 0x9ead2444, 0x05 | BRF_GRA },           // 12
	{ "wdobj6.bin",		0x40000, 0x9fa2ea21, 0x05 | BRF_GRA },           // 13
	{ "wdobj7.bin",		0x40000, 0x66e07a36, 0x05 | BRF_GRA },           // 14

	{ "wdchr0.bin",		0x20000, 0xdebb0116, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "wdchr1.bin",		0x20000, 0x8a1431e8, 0x06 | BRF_GRA },           // 16
	{ "wdchr2.bin",		0x20000, 0x62f75f69, 0x06 | BRF_GRA },           // 17
	{ "wdchr3.bin",		0x20000, 0xcc43bbe7, 0x06 | BRF_GRA },           // 18
	{ "wdchr4.bin",		0x20000, 0x2f73d05e, 0x06 | BRF_GRA },           // 19
	{ "wdchr5.bin",		0x20000, 0xb632b2ec, 0x06 | BRF_GRA },           // 20

	{ "wdroz0.bin",		0x20000, 0xf776bf66, 0x07 | BRF_GRA },           // 21 Roz Layer Tiles
	{ "wdroz1.bin",		0x20000, 0xc1a345c3, 0x07 | BRF_GRA },           // 22
	{ "wdroz2.bin",		0x20000, 0x28ffb44a, 0x07 | BRF_GRA },           // 23
	{ "wdroz3.bin",		0x20000, 0x7e77b46d, 0x07 | BRF_GRA },           // 24

	{ "wdshape.bin",	0x20000, 0x3b5e0249, 0x08 | BRF_GRA },           // 25 Layer Tiles Mask Data

	{ "wd1dat0.13s",	0x20000, 0xea209f48, 0x09 | BRF_PRG | BRF_ESS }, // 26 Shared 68K Data
	{ "wd1dat1.13p",	0x20000, 0x04b48ada, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "wd1voi1.bin",	0x40000, 0xf1ace193, 0x0a | BRF_SND },           // 28 C140 Samples Samples
	{ "wd1voi2.bin",	0x20000, 0xe95c5cf3, 0x0a | BRF_SND },           // 29
};

STD_ROM_PICK(valkyrie)
STD_ROM_FN(valkyrie)

static INT32 ValkyrieInit()
{
	INT32 rc = Namcos2Init(NULL, NULL);

	if (!rc) {
		weird_vbl = 0; // was 1, but some roz issues with last line (16px) of screen (f.ex namco logo spin-out on title)

		pDrvDrawBegin = DrvDrawBegin; // needs linedraw for some effects (end of boss fight / fall through floor)
		pDrvDrawLine = DrvDrawLine;
	}

	return rc;
}

struct BurnDriver BurnDrvValkyrie = {
	"valkyrie", NULL, NULL, NULL, "1989",
	"Valkyrie No Densetsu (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, valkyrieRomInfo, valkyrieRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	ValkyrieInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Rolling Thunder 2

static struct BurnRomInfo rthun2RomDesc[] = {
	{ "rts2_mpr0.bin",		0x20000, 0xe09a3549, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "rts2_mpr1.bin",		0x20000, 0x09573bff, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rts2_spr0.bin",		0x10000, 0x54c22ac5, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "rts2_spr1.bin",		0x10000, 0x060eb393, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "rst1_snd0.bin",		0x20000, 0x55b7562a, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "rst1_snd1.bin",		0x20000, 0x00445a4f, 0x03 | BRF_PRG | BRF_ESS }, //  5

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
#endif
	{ "sys2c65c.bin",		0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7

	{ "rst1_obj0.bin",		0x80000, 0xe5cb82c1, 0x05 | BRF_GRA },           //  8 Sprites
	{ "rst1_obj1.bin",		0x80000, 0x19ebe9fd, 0x05 | BRF_GRA },           //  9
	{ "rst1_obj2.bin",		0x80000, 0x455c4a2f, 0x05 | BRF_GRA },           // 10
	{ "rst1_obj3.bin",		0x80000, 0xfdcae8a9, 0x05 | BRF_GRA },           // 11

	{ "rst1_chr0.bin",		0x80000, 0x6f0e9a68, 0x06 | BRF_GRA },           // 12 Layer Tiles
	{ "rst1_chr1.bin",		0x80000, 0x15e44adc, 0x06 | BRF_GRA },           // 13

	{ "rst1_roz0.bin",		0x80000, 0x482d0554, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles

	{ "shape.bin",			0x80000, 0xcf58fbbe, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "rst1_data0.13s",		0x20000, 0x0baf44ee, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "rst1_data1.13p",		0x20000, 0x58a8daac, 0x09 | BRF_PRG | BRF_ESS }, // 17
	{ "rst1_data2.13r",		0x20000, 0x8e850a2a, 0x09 | BRF_PRG | BRF_ESS }, // 18

	{ "rst1_voi1.bin",		0x80000, 0xe42027cd, 0x0a | BRF_SND },           // 19 C140 Samples Samples
	{ "rst1_voi2.bin",		0x80000, 0x0c4c2b66, 0x0a | BRF_SND },           // 20
};
STD_ROM_PICK(rthun2)
STD_ROM_FN(rthun2)


static void rthun2_key_write(UINT8 offset, UINT16 data)
{
	if (data == 0x13ec && (offset == 4 || offset == 7)) {
		key_sendval = 1;
	}
}

static UINT16 rthun2_key_read(UINT8 offset)
{
	if (key_sendval) {
		if (offset == 4 || offset == 7) {
			key_sendval = 0;
			return 0x013f;
		}
	}

	if (offset == 2) return 0;

	return BurnRandom();
}

static INT32 Rthun2Init()
{
	weird_vbl = 2;

	return Namcos2Init(rthun2_key_write, rthun2_key_read);
}

struct BurnDriver BurnDrvRthun2 = {
	"rthun2", NULL, NULL, NULL, "1990",
	"Rolling Thunder 2\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, rthun2RomInfo, rthun2RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	Rthun2Init, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Rolling Thunder 2 (Japan)

static struct BurnRomInfo rthun2jRomDesc[] = {
	{ "rst1_mpr0.bin",		0x20000, 0x2563b9ee, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "rst1_mpr1.bin",		0x20000, 0x14c4c564, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rst1_spr0.bin",		0x10000, 0xf8ef5150, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "rst1_spr1.bin",		0x10000, 0x52ed3a48, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "rst1_snd0.bin",		0x20000, 0x55b7562a, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "rst1_snd1.bin",		0x20000, 0x00445a4f, 0x03 | BRF_PRG | BRF_ESS }, //  5

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7

	{ "rst1_obj0.bin",		0x80000, 0xe5cb82c1, 0x05 | BRF_GRA },           //  8 Sprites
	{ "rst1_obj1.bin",		0x80000, 0x19ebe9fd, 0x05 | BRF_GRA },           //  9
	{ "rst1_obj2.bin",		0x80000, 0x455c4a2f, 0x05 | BRF_GRA },           // 10
	{ "rst1_obj3.bin",		0x80000, 0xfdcae8a9, 0x05 | BRF_GRA },           // 11

	{ "rst1_chr0.bin",		0x80000, 0x6f0e9a68, 0x06 | BRF_GRA },           // 12 Layer Tiles
	{ "rst1_chr1.bin",		0x80000, 0x15e44adc, 0x06 | BRF_GRA },           // 13

	{ "rst1_roz0.bin",		0x80000, 0x482d0554, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles

	{ "shape.bin",		0x80000, 0xcf58fbbe, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "rst1_data0.13s",		0x20000, 0x0baf44ee, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "rst1_data1.13p",		0x20000, 0x58a8daac, 0x09 | BRF_PRG | BRF_ESS }, // 17
	{ "rst1_data2.13r",		0x20000, 0x8e850a2a, 0x09 | BRF_PRG | BRF_ESS }, // 18

	{ "rst1_voi1.bin",		0x80000, 0xe42027cd, 0x0a | BRF_SND },           // 19 C140 Samples Samples
	{ "rst1_voi2.bin",		0x80000, 0x0c4c2b66, 0x0a | BRF_SND },           // 20

	/* stuff below isn't used but loaded because it was on the board .. */
	{ "pal12l10.8d",		0x00040, 0xd3ae64a6, 0x00 | BRF_OPT },			 // 21 plds
	{ "plhs18p8a.2p",		0x00149, 0x28c634a4, 0x00 | BRF_OPT },
	{ "plhs18p8a.4g",		0x00149, 0x1932dd5e, 0x00 | BRF_OPT },
	{ "plhs18p8a.5f",		0x00149, 0xab2fd9c2, 0x00 | BRF_OPT },
	{ "pal16l8.9d",			0x00104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP},
};

STD_ROM_PICK(rthun2j)
STD_ROM_FN(rthun2j)

struct BurnDriver BurnDrvRthun2j = {
	"rthun2j", "rthun2", NULL, NULL, "1990",
	"Rolling Thunder 2 (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, rthun2jRomInfo, rthun2jRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	Rthun2Init, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Dragon Saber (World, DO2)

static struct BurnRomInfo dsaberRomDesc[] = {
	{ "do2 mpr0.mpr0",	0x20000, 0xa4c9ff34, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "do2 mpr1.mpr1",	0x20000, 0x2a89e794, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "do1 spr0.spr0",	0x10000, 0x013faf80, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "do1 spr1.spr1",	0x10000, 0xc36242bb, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "do1 snd0.snd0",	0x20000, 0xaf5b1ff8, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "do1 snd1.snd1",	0x20000, 0xc4ca6f3f, 0x03 | BRF_PRG | BRF_ESS }, //  5

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7

	{ "do obj-0a.obj0",	0x80000, 0xf08c6648, 0x05 | BRF_GRA },           //  8 Sprites
	{ "do obj-1a.obj1",	0x80000, 0x34e0810d, 0x05 | BRF_GRA },           //  9
	{ "do obj-2a.obj2",	0x80000, 0xbccdabf3, 0x05 | BRF_GRA },           // 10
	{ "do obj-3a.obj3",	0x80000, 0x2a60a4b8, 0x05 | BRF_GRA },           // 11

	{ "do chr-0a.chr0",	0x80000, 0xc6058df6, 0x06 | BRF_GRA },           // 12 Layer Tiles
	{ "do chr-1a.chr1",	0x80000, 0x67aaab36, 0x06 | BRF_GRA },           // 13

	{ "roz0.bin",		0x80000, 0x32aab758, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles

	{ "shape.bin",		0x80000, 0x698e7a3e, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "do1 dat0.13s",	0x20000, 0x3e53331f, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "do1 dat1.13p",	0x20000, 0xd5427f11, 0x09 | BRF_PRG | BRF_ESS }, // 17

	{ "do voi-1a.voice1",	0x80000, 0xdadf6a57, 0x0a | BRF_SND },           // 18 C140 Samples Samples
	{ "do voi-2a.voice2",	0x80000, 0x81078e01, 0x0a | BRF_SND },           // 19

	{ "pal16l8a.4g",	0x00104, 0x660e1655, 0x00 | BRF_OPT },           // 20 plds
	{ "pal16l8a.5f",	0x00104, 0x18f43c22, 0x00 | BRF_OPT },           // 21
	{ "pal12l10.8d",	0x00040, 0xe2379249, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(dsaber)
STD_ROM_FN(dsaber)

static UINT16 dsaber_key_read(UINT8 offset)
{
	if (offset == 2) return 0x00c0;

	return BurnRandom();
}

static INT32 DsaberInit()
{
	weird_vbl = 1;

	return Namcos2Init(NULL, dsaber_key_read);
}

struct BurnDriver BurnDrvDsaber = {
	"dsaber", NULL, NULL, NULL, "1990",
	"Dragon Saber (World, DO2)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, dsaberRomInfo, dsaberRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	DsaberInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Dragon Saber (World, older?)

static struct BurnRomInfo dsaberaRomDesc[] = {
	{ "mpr0.bin",		0x20000, 0x45309ddc, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mpr1.bin",		0x20000, 0xcbfc4cba, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "do1 spr0.spr0",	0x10000, 0x013faf80, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "do1 spr1.spr1",	0x10000, 0xc36242bb, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "do1 snd0.snd0",	0x20000, 0xaf5b1ff8, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "do1 snd1.snd1",	0x20000, 0xc4ca6f3f, 0x03 | BRF_PRG | BRF_ESS }, //  5

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7

	{ "do obj-0a.obj0",	0x80000, 0xf08c6648, 0x05 | BRF_GRA },           //  8 Sprites
	{ "do obj-1a.obj1",	0x80000, 0x34e0810d, 0x05 | BRF_GRA },           //  9
	{ "do obj-2a.obj2",	0x80000, 0xbccdabf3, 0x05 | BRF_GRA },           // 10
	{ "do obj-3a.obj3",	0x80000, 0x2a60a4b8, 0x05 | BRF_GRA },           // 11

	{ "do chr-0a.chr0",	0x80000, 0xc6058df6, 0x06 | BRF_GRA },           // 12 Layer Tiles
	{ "do chr-1a.chr1",	0x80000, 0x67aaab36, 0x06 | BRF_GRA },           // 13

	{ "roz0.bin",		0x80000, 0x32aab758, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles

	{ "shape.bin",		0x80000, 0x698e7a3e, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "do1 dat0.13s",	0x20000, 0x3e53331f, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "do1 dat1.13p",	0x20000, 0xd5427f11, 0x09 | BRF_PRG | BRF_ESS }, // 17

	{ "do voi-1a.voice1",	0x80000, 0xdadf6a57, 0x0a | BRF_SND },           // 18 C140 Samples Samples
	{ "do voi-2a.voice2",	0x80000, 0x81078e01, 0x0a | BRF_SND },           // 19

	{ "pal16l8a.4g",	0x00104, 0x660e1655, 0x00 | BRF_OPT },              // 20 plds
	{ "pal16l8a.5f",	0x00104, 0x18f43c22, 0x00 | BRF_OPT },           // 21
	{ "pal12l10.8d",	0x00040, 0xe2379249, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(dsabera)
STD_ROM_FN(dsabera)

struct BurnDriver BurnDrvDsabera = {
	"dsabera", "dsaber", NULL, NULL, "1990",
	"Dragon Saber (World, older?)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, dsaberaRomInfo, dsaberaRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	DsaberInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Dragon Saber (Japan, Rev B)

static struct BurnRomInfo dsaberjRomDesc[] = {
	{ "do1 mpr0b.mor0",	0x20000, 0x2898e791, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "do1 mpr1b.mpr1",	0x20000, 0x5fa9778e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "do1 spr0.spr0",	0x10000, 0x013faf80, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "do1 spr1.spr1",	0x10000, 0xc36242bb, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "do1 snd0.snd0",	0x20000, 0xaf5b1ff8, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "do1 snd1.snd1",	0x20000, 0xc4ca6f3f, 0x03 | BRF_PRG | BRF_ESS }, //  5

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7

	{ "do obj-0a.obj0",	0x80000, 0xf08c6648, 0x05 | BRF_GRA },           //  8 Sprites
	{ "do obj-1a.obj1",	0x80000, 0x34e0810d, 0x05 | BRF_GRA },           //  9
	{ "do obj-2a.obj2",	0x80000, 0xbccdabf3, 0x05 | BRF_GRA },           // 10
	{ "do obj-3a.obj3",	0x80000, 0x2a60a4b8, 0x05 | BRF_GRA },           // 11

	{ "do chr-0a.chr0",	0x80000, 0xc6058df6, 0x06 | BRF_GRA },           // 12 Layer Tiles
	{ "do chr-1a.chr1",	0x80000, 0x67aaab36, 0x06 | BRF_GRA },           // 13

	{ "roz0.bin",		0x80000, 0x32aab758, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles

	{ "shape.bin",		0x80000, 0x698e7a3e, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "do1 dat0.13s",	0x20000, 0x3e53331f, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "do1 dat1.13p",	0x20000, 0xd5427f11, 0x09 | BRF_PRG | BRF_ESS }, // 17

	{ "do voi-1a.voice1",	0x80000, 0xdadf6a57, 0x0a | BRF_SND },           // 18 C140 Samples Samples
	{ "do voi-2a.voice2",	0x80000, 0x81078e01, 0x0a | BRF_SND },           // 19
 
	{ "pal16l8a.4g",	0x00104, 0x660e1655, 0x00 | BRF_OPT },           // 20 plds
	{ "pal16l8a.5f",	0x00104, 0x18f43c22, 0x00 | BRF_OPT },           // 21
	{ "pal12l10.8d",	0x00040, 0xe2379249, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(dsaberj)
STD_ROM_FN(dsaberj)

struct BurnDriver BurnDrvDsaberj = {
	"dsaberj", "dsaber", NULL, NULL, "1990",
	"Dragon Saber (Japan, Rev B)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, dsaberjRomInfo, dsaberjRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	DsaberInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Burning Force (Japan, new version (Rev C))

static struct BurnRomInfo burnforcRomDesc[] = {
	{ "bu1_mpr0c.bin",	0x20000, 0xcc5864c6, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "bu1_mpr1c.bin",	0x20000, 0x3e6b4b1b, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bu1_spr0.bin",	0x10000, 0x17022a21, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "bu1_spr1.bin",	0x10000, 0x5255f8a5, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "bu1_snd0.bin",	0x20000, 0xfabb1150, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "bu_obj-0.bin",	0x80000, 0x24c919a1, 0x05 | BRF_GRA },           //  7 Sprites
	{ "bu_obj-1.bin",	0x80000, 0x5bcb519b, 0x05 | BRF_GRA },           //  8
	{ "bu_obj-2.bin",	0x80000, 0x509dd5d0, 0x05 | BRF_GRA },           //  9
	{ "bu_obj-3.bin",	0x80000, 0x270a161e, 0x05 | BRF_GRA },           // 10

	{ "bu_chr-0.bin",	0x20000, 0xc2109f73, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "bu_chr-1.bin",	0x20000, 0x67d6aa67, 0x06 | BRF_GRA },           // 12
	{ "bu_chr-2.bin",	0x20000, 0x52846eff, 0x06 | BRF_GRA },           // 13
	{ "bu_chr-3.bin",	0x20000, 0xd1326d7f, 0x06 | BRF_GRA },           // 14
	{ "bu_chr-4.bin",	0x20000, 0x81a66286, 0x06 | BRF_GRA },           // 15
	{ "bu_chr-5.bin",	0x20000, 0x629aa67f, 0x06 | BRF_GRA },           // 16

	{ "bu_roz-0.bin",	0x20000, 0x65fefc83, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "bu_roz-1.bin",	0x20000, 0x979580c2, 0x07 | BRF_GRA },           // 18
	{ "bu_roz-2.bin",	0x20000, 0x548b6ad8, 0x07 | BRF_GRA },           // 19
	{ "bu_roz-3.bin",	0x20000, 0xa633cea0, 0x07 | BRF_GRA },           // 20
	{ "bu_roz-4.bin",	0x20000, 0x1b1f56a6, 0x07 | BRF_GRA },           // 21
	{ "bu_roz-5.bin",	0x20000, 0x4b864b0e, 0x07 | BRF_GRA },           // 22
	{ "bu_roz-6.bin",	0x20000, 0x38bd25ba, 0x07 | BRF_GRA },           // 23

	{ "bu_shape.bin",	0x20000, 0x80a6b722, 0x08 | BRF_GRA },           // 24 Layer Tiles Mask Data

	{ "bu1_dat0.13s",	0x20000, 0xe0a9d92f, 0x09 | BRF_PRG | BRF_ESS }, // 25 Shared 68K Data
	{ "bu1_dat1.13p",	0x20000, 0x5fe54b73, 0x09 | BRF_PRG | BRF_ESS }, // 26

	{ "bu_voi-1.bin",	0x80000, 0x99d8a239, 0x0a | BRF_SND },           // 27 C140 Samples Samples
};

STD_ROM_PICK(burnforc)
STD_ROM_FN(burnforc)

static UINT16 burnforc_key_read(UINT8 offset)
{
	if (offset == 1) return 0xbd;

	return BurnRandom();
}

static INT32 BurnforcInit()
{
	INT32 rc = Namcos2Init(NULL, burnforc_key_read);
	if (!rc) {
		pDrvDrawBegin = DrvDrawBegin;
		pDrvDrawLine = DrvDrawLine;
	}

	return rc;
}

struct BurnDriver BurnDrvBurnforc = {
	"burnforc", NULL, NULL, NULL, "1989",
	"Burning Force (Japan, new version (Rev C))\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, burnforcRomInfo, burnforcRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	BurnforcInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Burning Force (Japan, old version)

static struct BurnRomInfo burnforcoRomDesc[] = {
	{ "bu1_mpr0.bin",	0x20000, 0x096b73e2, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "bu1_mpr1.bin",	0x20000, 0x7ead4cbf, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bu1_spr0.bin",	0x10000, 0x17022a21, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "bu1_spr1.bin",	0x10000, 0x5255f8a5, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "bu1_snd0.bin",	0x20000, 0xfabb1150, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "bu_obj-0.bin",	0x80000, 0x24c919a1, 0x05 | BRF_GRA },           //  7 Sprites
	{ "bu_obj-1.bin",	0x80000, 0x5bcb519b, 0x05 | BRF_GRA },           //  8
	{ "bu_obj-2.bin",	0x80000, 0x509dd5d0, 0x05 | BRF_GRA },           //  9
	{ "bu_obj-3.bin",	0x80000, 0x270a161e, 0x05 | BRF_GRA },           // 10

	{ "bu_chr-0.bin",	0x20000, 0xc2109f73, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "bu_chr-1.bin",	0x20000, 0x67d6aa67, 0x06 | BRF_GRA },           // 12
	{ "bu_chr-2.bin",	0x20000, 0x52846eff, 0x06 | BRF_GRA },           // 13
	{ "bu_chr-3.bin",	0x20000, 0xd1326d7f, 0x06 | BRF_GRA },           // 14
	{ "bu_chr-4.bin",	0x20000, 0x81a66286, 0x06 | BRF_GRA },           // 15
	{ "bu_chr-5.bin",	0x20000, 0x629aa67f, 0x06 | BRF_GRA },           // 16

	{ "bu_roz-0.bin",	0x20000, 0x65fefc83, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "bu_roz-1.bin",	0x20000, 0x979580c2, 0x07 | BRF_GRA },           // 18
	{ "bu_roz-2.bin",	0x20000, 0x548b6ad8, 0x07 | BRF_GRA },           // 19
	{ "bu_roz-3.bin",	0x20000, 0xa633cea0, 0x07 | BRF_GRA },           // 20
	{ "bu_roz-4.bin",	0x20000, 0x1b1f56a6, 0x07 | BRF_GRA },           // 21
	{ "bu_roz-5.bin",	0x20000, 0x4b864b0e, 0x07 | BRF_GRA },           // 22
	{ "bu_roz-6.bin",	0x20000, 0x38bd25ba, 0x07 | BRF_GRA },           // 23

	{ "bu_shape.bin",	0x20000, 0x80a6b722, 0x08 | BRF_GRA },           // 24 Layer Tiles Mask Data

	{ "bu1_dat0.13s",	0x20000, 0xe0a9d92f, 0x09 | BRF_PRG | BRF_ESS }, // 25 Shared 68K Data
	{ "bu1_dat1.13p",	0x20000, 0x5fe54b73, 0x09 | BRF_PRG | BRF_ESS }, // 26

	{ "bu_voi-1.bin",	0x80000, 0x99d8a239, 0x0a | BRF_SND },           // 27 C140 Samples Samples
};

STD_ROM_PICK(burnforco)
STD_ROM_FN(burnforco)

struct BurnDriver BurnDrvBurnforco = {
	"burnforco", "burnforc", NULL, NULL, "1989",
	"Burning Force (Japan, old version)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, burnforcoRomInfo, burnforcoRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	BurnforcInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};



// Finest Hour (Japan)

static struct BurnRomInfo finehourRomDesc[] = {
	{ "fh1_mp0.bin",	0x20000, 0x355d9119, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fh1_mp1.bin",	0x20000, 0x647eb621, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fh1_sp0.bin",	0x20000, 0xaa6289e9, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fh1_sp1.bin",	0x20000, 0x8532d5c7, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fh1_sd0.bin",	0x20000, 0x059a9cfd, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "fh1_ob0.bin",	0x80000, 0xb1fd86f1, 0x05 | BRF_GRA },           //  7 Sprites
	{ "fh1_ob1.bin",	0x80000, 0x519c44ce, 0x05 | BRF_GRA },           //  8
	{ "fh1_ob2.bin",	0x80000, 0x9c5de4fa, 0x05 | BRF_GRA },           //  9
	{ "fh1_ob3.bin",	0x80000, 0x54d4edce, 0x05 | BRF_GRA },           // 10

	{ "fh1_ch0.bin",	0x40000, 0x516900d1, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fh1_ch1.bin",	0x40000, 0x964d06bd, 0x06 | BRF_GRA },           // 12
	{ "fh1_ch2.bin",	0x40000, 0xfbb9449e, 0x06 | BRF_GRA },           // 13
	{ "fh1_ch3.bin",	0x40000, 0xc18eda8a, 0x06 | BRF_GRA },           // 14
	{ "fh1_ch4.bin",	0x40000, 0x80dd188a, 0x06 | BRF_GRA },           // 15
	{ "fh1_ch5.bin",	0x40000, 0x40969876, 0x06 | BRF_GRA },           // 16

	{ "fh1_rz0.bin",	0x20000, 0x6c96c5c1, 0x07 | BRF_GRA },           // 17 Roz Layer Tiles
	{ "fh1_rz1.bin",	0x20000, 0x44699eb9, 0x07 | BRF_GRA },           // 18
	{ "fh1_rz2.bin",	0x20000, 0x5ec14abf, 0x07 | BRF_GRA },           // 19
	{ "fh1_rz3.bin",	0x20000, 0x9f5a91b2, 0x07 | BRF_GRA },           // 20
	{ "fh1_rz4.bin",	0x20000, 0x0b4379e6, 0x07 | BRF_GRA },           // 21
	{ "fh1_rz5.bin",	0x20000, 0xe034e560, 0x07 | BRF_GRA },           // 22

	{ "fh1_sha.bin",	0x40000, 0x15875eb0, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "fh1_dt0.13s",	0x20000, 0x2441c26f, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "fh1_dt1.13p",	0x20000, 0x48154deb, 0x09 | BRF_PRG | BRF_ESS }, // 25
	{ "fh1_dt2.13r",	0x20000, 0x12453ba4, 0x09 | BRF_PRG | BRF_ESS }, // 26
	{ "fh1_dt3.13n",	0x20000, 0x50bab9da, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "fh1_vo1.bin",	0x80000, 0x07560fc7, 0x0a | BRF_SND },           // 28 C140 Samples Samples
};

STD_ROM_PICK(finehour)
STD_ROM_FN(finehour)

static UINT16 finehour_key_read(UINT8 offset)
{
	if (offset == 7) return 0xbc;

	return BurnRandom();
}

static INT32 FinehourInit()
{
	INT32 rc = Namcos2Init(NULL, finehour_key_read);

	weird_vbl = 0;

	if (!rc) {
		pDrvDrawBegin = DrvDrawBegin;
		pDrvDrawLine = DrvDrawLine;
	}

	return rc;
}

struct BurnDriver BurnDrvFinehour = {
	"finehour", NULL, NULL, NULL, "1989",
	"Finest Hour (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, finehourRomInfo, finehourRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	FinehourInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Super World Stadium (Japan)

static struct BurnRomInfo swsRomDesc[] = {
	{ "ss1_mpr0.11d",	0x20000, 0xd12bd020, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ss1_mpr1.13d",	0x20000, 0xe9ae14ce, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sst1spr0.bin",	0x20000, 0x9777ee2f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sst1spr1.bin",	0x20000, 0x27a35c69, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sst1snd0.bin",	0x20000, 0x8fc45114, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xca64550a, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "ss1_obj0.5b",	0x80000, 0x9bd6add1, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ss1_obj1.4b",	0x80000, 0xa9db3d02, 0x05 | BRF_GRA },           //  8
	{ "ss1_obj2.5d",	0x80000, 0xb4a73ced, 0x05 | BRF_GRA },           //  9
	{ "ss1_obj3.4d",	0x80000, 0x0a832b36, 0x05 | BRF_GRA },           // 10

	{ "ss1_chr0.11n",	0x80000, 0xab0141de, 0x06 | BRF_GRA },           // 11 Layer Tiles

	{ "ss_roz0.bin",	0x80000, 0x40ce9a58, 0x07 | BRF_GRA },           // 12 Roz Layer Tiles
	{ "ss_roz1.bin",	0x80000, 0xc98902ff, 0x07 | BRF_GRA },           // 13
	{ "ss1_roz2.1c",	0x80000, 0xb603e1a1, 0x07 | BRF_GRA },           // 14

	{ "ss1_sha0.7n",	0x80000, 0xfea6952c, 0x08 | BRF_GRA },           // 15 Layer Tiles Mask Data

	{ "ss1_dat0.13s",	0x40000, 0x6a360f91, 0x09 | BRF_PRG | BRF_ESS }, // 16 Shared 68K Data
	{ "ss1_dat1.13p",	0x40000, 0xab1e487d, 0x09 | BRF_PRG | BRF_ESS }, // 17

	{ "ss_voi1.bin",	0x80000, 0x503e51b7, 0x0a | BRF_SND },           // 18 C140 Samples Samples
};

STD_ROM_PICK(sws)
STD_ROM_FN(sws)

static UINT16 sws_key_read(UINT8 offset)
{
	if (offset == 4) return 0x0142;

	return BurnRandom();
}

static INT32 SwsInit()
{
	return Namcos2Init(NULL, sws_key_read);
}

struct BurnDriver BurnDrvSws = {
	"sws", NULL, NULL, NULL, "1992",
	"Super World Stadium (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, swsRomInfo, swsRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	SwsInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Super World Stadium '92 (Japan)

static struct BurnRomInfo sws92RomDesc[] = {
	{ "sss1mpr0.bin",	0x20000, 0xdbea0210, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sss1mpr1.bin",	0x20000, 0xb5e6469a, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sst1spr0.bin",	0x20000, 0x9777ee2f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sst1spr1.bin",	0x20000, 0x27a35c69, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sst1snd0.bin",	0x20000, 0x8fc45114, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xca64550a, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "sss_obj0.bin",	0x80000, 0x375e8f1f, 0x05 | BRF_GRA },           //  7 Sprites
	{ "sss_obj1.bin",	0x80000, 0x675c1014, 0x05 | BRF_GRA },           //  8
	{ "sss_obj2.bin",	0x80000, 0xbdc55f1c, 0x05 | BRF_GRA },           //  9
	{ "sss_obj3.bin",	0x80000, 0xe32ac432, 0x05 | BRF_GRA },           // 10

	{ "sss_chr0.bin",	0x80000, 0x1d2876f2, 0x26 | BRF_GRA },           // 11 Layer Tiles
	{ "sss_chr6.bin",	0x80000, 0x354f0ed2, 0x26 | BRF_GRA },           // 12
	{ "sss_chr7.bin",	0x80000, 0x4032f4c1, 0x26 | BRF_GRA },           // 13

	{ "ss_roz0.bin",	0x80000, 0x40ce9a58, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles
	{ "ss_roz1.bin",	0x80000, 0xc98902ff, 0x07 | BRF_GRA },           // 15
	{ "sss_roz2.bin",	0x80000, 0xc9855c10, 0x07 | BRF_GRA },           // 16

	{ "sss_sha0.bin",	0x80000, 0xb71a731a, 0x08 | BRF_GRA },           // 17 Layer Tiles Mask Data

	{ "sss1dat0.13s",	0x40000, 0xdb3e6aec, 0x09 | BRF_PRG | BRF_ESS }, // 18 Shared 68K Data
	{ "sss1dat1.13p",	0x40000, 0x463b5ba8, 0x09 | BRF_PRG | BRF_ESS }, // 19

	{ "ss_voi1.bin",	0x80000, 0x503e51b7, 0x0a | BRF_SND },           // 20 C140 Samples Samples
};

STD_ROM_PICK(sws92)
STD_ROM_FN(sws92)

static UINT16 sws92_key_read(UINT8 offset)
{
	if (offset == 3) return 0x014b;

	return BurnRandom();
}

static INT32 Sws92Init()
{
	return Namcos2Init(NULL, sws92_key_read);
}

struct BurnDriver BurnDrvSws92 = {
	"sws92", NULL, NULL, NULL, "1992",
	"Super World Stadium '92 (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws92RomInfo, sws92RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	Sws92Init, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Super World Stadium '92 Gekitouban (Japan)

static struct BurnRomInfo sws92gRomDesc[] = {
	{ "ssg1mpr0.bin",	0x20000, 0x5596c535, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ssg1mpr1.bin",	0x20000, 0x3289ef0c, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sst1spr0.bin",	0x20000, 0x9777ee2f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sst1spr1.bin",	0x20000, 0x27a35c69, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sst1snd0.bin",	0x20000, 0x8fc45114, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xca64550a, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "sss_obj0.bin",	0x80000, 0x375e8f1f, 0x05 | BRF_GRA },           //  7 Sprites
	{ "sss_obj1.bin",	0x80000, 0x675c1014, 0x05 | BRF_GRA },           //  8
	{ "sss_obj2.bin",	0x80000, 0xbdc55f1c, 0x05 | BRF_GRA },           //  9
	{ "sss_obj3.bin",	0x80000, 0xe32ac432, 0x05 | BRF_GRA },           // 10

	{ "sss_chr0.bin",	0x80000, 0x1d2876f2, 0x26 | BRF_GRA },           // 11 Layer Tiles
	{ "sss_chr6.bin",	0x80000, 0x354f0ed2, 0x26 | BRF_GRA },           // 12
	{ "sss_chr7.bin",	0x80000, 0x4032f4c1, 0x26 | BRF_GRA },           // 13

	{ "ss_roz0.bin",	0x80000, 0x40ce9a58, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles
	{ "ss_roz1.bin",	0x80000, 0xc98902ff, 0x07 | BRF_GRA },           // 15
	{ "sss_roz2.bin",	0x80000, 0xc9855c10, 0x07 | BRF_GRA },           // 16

	{ "sss_sha0.bin",	0x80000, 0xb71a731a, 0x08 | BRF_GRA },           // 17 Layer Tiles Mask Data

	{ "sss1dat0.13s",	0x40000, 0xdb3e6aec, 0x09 | BRF_PRG | BRF_ESS }, // 18 Shared 68K Data
	{ "sss1dat1.13p",	0x40000, 0x463b5ba8, 0x09 | BRF_PRG | BRF_ESS }, // 19
	{ "ssg1dat2.13r",	0x40000, 0x754128aa, 0x09 | BRF_PRG | BRF_ESS }, // 20
	{ "ssg1dat3.13n",	0x40000, 0xcb3fed01, 0x09 | BRF_PRG | BRF_ESS }, // 21

	{ "ss_voi1.bin",	0x80000, 0x503e51b7, 0x0a | BRF_SND },           // 22 C140 Samples Samples
};

STD_ROM_PICK(sws92g)
STD_ROM_FN(sws92g)

static UINT16 sws92g_key_read(UINT8 offset)
{
	if (offset == 3) return 0x014c;

	return BurnRandom();
}

static INT32 Sws92gInit()
{
	return Namcos2Init(NULL, sws92g_key_read);
}

struct BurnDriver BurnDrvSws92g = {
	"sws92g", "sws92", NULL, NULL, "1992",
	"Super World Stadium '92 Gekitouban (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws92gRomInfo, sws92gRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	Sws92gInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Super World Stadium '93 (Japan)

static struct BurnRomInfo sws93RomDesc[] = {
	{ "sst1mpr0.bin",	0x20000, 0xbd2679bc, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sst1mpr1.bin",	0x20000, 0x9132e220, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sst1spr0.bin",	0x20000, 0x9777ee2f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sst1spr1.bin",	0x20000, 0x27a35c69, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sst1snd0.bin",	0x20000, 0x8fc45114, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "sst_obj0.bin",	0x80000, 0x4089dfd7, 0x05 | BRF_GRA },           //  7 Sprites
	{ "sst_obj1.bin",	0x80000, 0xcfbc25c7, 0x05 | BRF_GRA },           //  8
	{ "sst_obj2.bin",	0x80000, 0x61ed3558, 0x05 | BRF_GRA },           //  9
	{ "sst_obj3.bin",	0x80000, 0x0e3bc05d, 0x05 | BRF_GRA },           // 10

	{ "sst_chr0.bin",	0x80000, 0x3397850d, 0x26 | BRF_GRA },           // 11 Layer Tiles
	{ "sss_chr6.bin",	0x80000, 0x354f0ed2, 0x26 | BRF_GRA },           // 12
	{ "sst_chr7.bin",	0x80000, 0xe0abb763, 0x26 | BRF_GRA },           // 13

	{ "ss_roz0.bin",	0x80000, 0x40ce9a58, 0x07 | BRF_GRA },           // 14 Roz Layer Tiles
	{ "ss_roz1.bin",	0x80000, 0xc98902ff, 0x07 | BRF_GRA },           // 15
	{ "sss_roz2.bin",	0x80000, 0xc9855c10, 0x07 | BRF_GRA },           // 16

	{ "sst_sha0.bin",	0x80000, 0x4f64d4bd, 0x08 | BRF_GRA },           // 17 Layer Tiles Mask Data

	{ "sst1dat0.13s",	0x80000, 0xb99c9656, 0x09 | BRF_PRG | BRF_ESS }, // 18 Shared 68K Data
	{ "sst1dat1.13p",	0x80000, 0x60cf6281, 0x09 | BRF_PRG | BRF_ESS }, // 19

	{ "ss_voi1.bin",	0x80000, 0x503e51b7, 0x0a | BRF_SND },           // 20 C140 Samples Samples
};

STD_ROM_PICK(sws93)
STD_ROM_FN(sws93)

static UINT16 sws93_key_read(UINT8 offset)
{
	if (offset == 3) return 0x014e;

	return BurnRandom();
}

static INT32 Sws93Init()
{
	return Namcos2Init(NULL, sws93_key_read);
}

struct BurnDriver BurnDrvSws93 = {
	"sws93", NULL, NULL, NULL, "1993",
	"Super World Stadium '93 (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, sws93RomInfo, sws93RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	Sws93Init, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Kyuukai Douchuuki (Japan, new version (Rev B))

static struct BurnRomInfo kyukaidkRomDesc[] = {
	{ "ky1_mp0b.bin",	0x10000, 0xd1c992c8, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ky1_mp1b.bin",	0x10000, 0x723553af, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ky1_sp0.bin",	0x10000, 0x4b4d2385, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ky1_sp1.bin",	0x10000, 0xbd3368cd, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ky1_s0.bin",		0x20000, 0x27aea3e9, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "ky1_o0.bin",		0x80000, 0xebec5132, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ky1_o1.bin",		0x80000, 0xfde7e5ae, 0x05 | BRF_GRA },           //  8
	{ "ky1_o2.bin",		0x80000, 0x2a181698, 0x05 | BRF_GRA },           //  9
	{ "ky1_o3.bin",		0x80000, 0x71fcd3a6, 0x05 | BRF_GRA },           // 10

	{ "ky1_c0.bin",		0x20000, 0x7bd69a2d, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "ky1_c1.bin",		0x20000, 0x66a623fe, 0x06 | BRF_GRA },           // 12
	{ "ky1_c2.bin",		0x20000, 0xe84b3dfd, 0x06 | BRF_GRA },           // 13
	{ "ky1_c3.bin",		0x20000, 0x69e67c86, 0x06 | BRF_GRA },           // 14

	{ "ky1_r0.bin",		0x40000, 0x9213e8c4, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles
	{ "ky1_r1.bin",		0x40000, 0x97d1a641, 0x07 | BRF_GRA },           // 16
	{ "ky1_r2.bin",		0x40000, 0x39b58792, 0x07 | BRF_GRA },           // 17
	{ "ky1_r3.bin",		0x40000, 0x90c60d92, 0x07 | BRF_GRA },           // 18

	{ "ky1_sha.bin",	0x20000, 0x380a20d7, 0x08 | BRF_GRA },           // 19 Layer Tiles Mask Data

	{ "ky1_d0.13s",		0x20000, 0xc9cf399d, 0x09 | BRF_PRG | BRF_ESS }, // 20 Shared 68K Data
	{ "ky1_d1.13p",		0x20000, 0x6d4f21b9, 0x09 | BRF_PRG | BRF_ESS }, // 21
	{ "ky1_d2.13r",		0x20000, 0xeb6d19c8, 0x09 | BRF_PRG | BRF_ESS }, // 22
	{ "ky1_d3.13n",		0x20000, 0x95674701, 0x09 | BRF_PRG | BRF_ESS }, // 23

	{ "ky1_v1.bin",		0x80000, 0x5ff81aec, 0x0a | BRF_SND },           // 24 C140 Samples Samples
};

STD_ROM_PICK(kyukaidk)
STD_ROM_FN(kyukaidk)

static INT32 KyukaidkInit()
{
	return Namcos2Init(NULL, NULL);
}

struct BurnDriver BurnDrvKyukaidk = {
	"kyukaidk", NULL, NULL, NULL, "1990",
	"Kyuukai Douchuuki (Japan, new version (Rev B))\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, kyukaidkRomInfo, kyukaidkRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	KyukaidkInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Kyuukai Douchuuki (Japan, old version)

static struct BurnRomInfo kyukaidkoRomDesc[] = {
	{ "ky1_mp0.bin",	0x10000, 0x01978a19, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ky1_mp1.bin",	0x10000, 0xb40717a7, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ky1_sp0.bin",	0x10000, 0x4b4d2385, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ky1_sp1.bin",	0x10000, 0xbd3368cd, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ky1_s0.bin",		0x20000, 0x27aea3e9, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "ky1_o0.bin",		0x80000, 0xebec5132, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ky1_o1.bin",		0x80000, 0xfde7e5ae, 0x05 | BRF_GRA },           //  8
	{ "ky1_o2.bin",		0x80000, 0x2a181698, 0x05 | BRF_GRA },           //  9
	{ "ky1_o3.bin",		0x80000, 0x71fcd3a6, 0x05 | BRF_GRA },           // 10

	{ "ky1_c0.bin",		0x20000, 0x7bd69a2d, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "ky1_c1.bin",		0x20000, 0x66a623fe, 0x06 | BRF_GRA },           // 12
	{ "ky1_c2.bin",		0x20000, 0xe84b3dfd, 0x06 | BRF_GRA },           // 13
	{ "ky1_c3.bin",		0x20000, 0x69e67c86, 0x06 | BRF_GRA },           // 14

	{ "ky1_r0.bin",		0x40000, 0x9213e8c4, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles
	{ "ky1_r1.bin",		0x40000, 0x97d1a641, 0x07 | BRF_GRA },           // 16
	{ "ky1_r2.bin",		0x40000, 0x39b58792, 0x07 | BRF_GRA },           // 17
	{ "ky1_r3.bin",		0x40000, 0x90c60d92, 0x07 | BRF_GRA },           // 18

	{ "ky1_sha.bin",	0x20000, 0x380a20d7, 0x08 | BRF_GRA },           // 19 Layer Tiles Mask Data

	{ "ky1_d0.13s",		0x20000, 0xc9cf399d, 0x09 | BRF_PRG | BRF_ESS }, // 20 Shared 68K Data
	{ "ky1_d1.13p",		0x20000, 0x6d4f21b9, 0x09 | BRF_PRG | BRF_ESS }, // 21
	{ "ky1_d2.13r",		0x20000, 0xeb6d19c8, 0x09 | BRF_PRG | BRF_ESS }, // 22
	{ "ky1_d3.13n",		0x20000, 0x95674701, 0x09 | BRF_PRG | BRF_ESS }, // 23

	{ "ky1_v1.bin",		0x80000, 0x5ff81aec, 0x0a | BRF_SND },           // 24 C140 Samples Samples
};

STD_ROM_PICK(kyukaidko)
STD_ROM_FN(kyukaidko)

struct BurnDriver BurnDrvKyukaidko = {
	"kyukaidko", "kyukaidk", NULL, NULL, "1990",
	"Kyuukai Douchuuki (Japan, old version)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, kyukaidkoRomInfo, kyukaidkoRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo,
	KyukaidkInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Steel Gunner (Rev B)

static struct BurnRomInfo sgunnerRomDesc[] = {
	{ "sn2mpr0b.11d",	0x20000, 0x4bb33394, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sn2mpr1b.13d",	0x20000, 0xd8b47334, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sn1spr0.11k",	0x10000, 0x4638b512, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sn1spr1.13k",	0x10000, 0xe8b1ee73, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sn1_snd0.8j",	0x20000, 0xbdf36d44, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "sn_obj0.8c",		0x80000, 0xbbae38f7, 0x05 | BRF_GRA },           //  7 Sprites
	{ "sn_obj4.9c",		0x80000, 0x82fdaa06, 0x05 | BRF_GRA },           //  8
	{ "sn_obj1.12c",	0x80000, 0x4dfacb51, 0x05 | BRF_GRA },           //  9
	{ "sn_obj5.13c",	0x80000, 0x8700a8a4, 0x05 | BRF_GRA },           // 10
	{ "sn_obj2.10c",	0x80000, 0x313a308f, 0x05 | BRF_GRA },           // 11
	{ "sn_obj6.11c",	0x80000, 0x9c6504f7, 0x05 | BRF_GRA },           // 12
	{ "sn_obj3.14c",	0x80000, 0xd7c340f6, 0x05 | BRF_GRA },           // 13
	{ "sn_obj7.15c",	0x80000, 0xcd1356c0, 0x05 | BRF_GRA },           // 14

	{ "sn_chr0.11n",	0x80000, 0xb433c37b, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "sn_chr1.11p",	0x80000, 0xb7dd41f9, 0x06 | BRF_GRA },           // 16

	{ "sn_sha0.8n",		0x80000, 0x01e20999, 0x08 | BRF_GRA },           // 17 Layer Tiles Mask Data

	{ "sn1_dat0.13s",	0x20000, 0x72bfeca8, 0x09 | BRF_PRG | BRF_ESS }, // 18 Shared 68K Data
	{ "sn1_dat1.13p",	0x20000, 0x99b3e653, 0x09 | BRF_PRG | BRF_ESS }, // 19

	{ "sn_voi1.3m",		0x80000, 0x464e616d, 0x0a | BRF_SND },           // 20 C140 Samples Samples
	{ "sn_voi2.3l",		0x80000, 0x8c3251b5, 0x0a | BRF_SND },           // 21

	{ "sgunner.nv",		0x02000, 0x106026f8, 0x0b | BRF_PRG | BRF_ESS }, // 22 Default NV RAM
};

STD_ROM_PICK(sgunner)
STD_ROM_FN(sgunner)

static INT32 SgunnerInit()
{
	return SgunnerCommonInit(NULL, NULL);
}

struct BurnDriver BurnDrvSgunner = {
	"sgunner", NULL, NULL, NULL, "1990",
	"Steel Gunner (Rev B)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, sgunnerRomInfo, sgunnerRomName, NULL, NULL, NULL, NULL, SgunnerInputInfo, SgunnerDIPInfo,
	SgunnerInit, Namcos2Exit, DrvFrame, SgunnerDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Steel Gunner (Japan)

static struct BurnRomInfo sgunnerjRomDesc[] = {
	{ "sn1mpr0.11d",	0x20000, 0xf60116d7, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sn1mpr1.13d",	0x20000, 0x23942fc9, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sn1spr0.11k",	0x10000, 0x4638b512, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sn1spr1.13k",	0x10000, 0xe8b1ee73, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sn1_snd0.8j",	0x20000, 0xbdf36d44, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "sn_obj0.8c",		0x80000, 0xbbae38f7, 0x05 | BRF_GRA },           //  7 Sprites
	{ "sn_obj4.9c",		0x80000, 0x82fdaa06, 0x05 | BRF_GRA },           //  8
	{ "sn_obj1.12c",	0x80000, 0x4dfacb51, 0x05 | BRF_GRA },           //  9
	{ "sn_obj5.13c",	0x80000, 0x8700a8a4, 0x05 | BRF_GRA },           // 10
	{ "sn_obj2.10c",	0x80000, 0x313a308f, 0x05 | BRF_GRA },           // 11
	{ "sn_obj6.11c",	0x80000, 0x9c6504f7, 0x05 | BRF_GRA },           // 12
	{ "sn_obj3.14c",	0x80000, 0xd7c340f6, 0x05 | BRF_GRA },           // 13
	{ "sn_obj7.15c",	0x80000, 0xcd1356c0, 0x05 | BRF_GRA },           // 14

	{ "sn_chr0.11n",	0x80000, 0xb433c37b, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "sn_chr1.11p",	0x80000, 0xb7dd41f9, 0x06 | BRF_GRA },           // 16

	{ "sn_sha0.8n",		0x80000, 0x01e20999, 0x08 | BRF_GRA },           // 17 Layer Tiles Mask Data

	{ "sn1_dat0.13s",	0x20000, 0x72bfeca8, 0x09 | BRF_PRG | BRF_ESS }, // 18 Shared 68K Data
	{ "sn1_dat1.13p",	0x20000, 0x99b3e653, 0x09 | BRF_PRG | BRF_ESS }, // 19

	{ "sn_voi1.3m",		0x80000, 0x464e616d, 0x0a | BRF_SND },           // 20 C140 Samples Samples
	{ "sn_voi2.3l",		0x80000, 0x8c3251b5, 0x0a | BRF_SND },           // 21

	{ "sgunner.nv",		0x02000, 0x106026f8, 0x0b | BRF_PRG | BRF_ESS }, // 22 Default NV RAM
};

STD_ROM_PICK(sgunnerj)
STD_ROM_FN(sgunnerj)

struct BurnDriver BurnDrvSgunnerj = {
	"sgunnerj", "sgunner", NULL, NULL, "1990",
	"Steel Gunner (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, sgunnerjRomInfo, sgunnerjRomName, NULL, NULL, NULL, NULL, SgunnerInputInfo, SgunnerDIPInfo,
	SgunnerInit, Namcos2Exit, DrvFrame, SgunnerDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Steel Gunner 2 (US)

static struct BurnRomInfo sgunner2RomDesc[] = {
	{ "sns2mpr0.bin",	0x20000, 0xf1a44039, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sns2mpr1.bin",	0x20000, 0x9184c4db, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sns_spr0.bin",	0x10000, 0xe5e40ed0, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sns_spr1.bin",	0x10000, 0x3a85a5e9, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sns_snd0.bin",	0x20000, 0xf079cd32, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	//	not actually on this hw, but works
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7
#endif

	{ "sns_obj0.bin",	0x80000, 0xc762445c, 0x05 | BRF_GRA },           //  8 Sprites
	{ "sns_obj4.bin",	0x80000, 0x0b1be894, 0x05 | BRF_GRA },           //  9
	{ "sns_obj1.bin",	0x80000, 0xe9e379d8, 0x05 | BRF_GRA },           // 10
	{ "sns_obj5.bin",	0x80000, 0x416b14e1, 0x05 | BRF_GRA },           // 11
	{ "sns_obj2.bin",	0x80000, 0x0d076f6c, 0x05 | BRF_GRA },           // 12
	{ "sns_obj6.bin",	0x80000, 0xc2e94ed2, 0x05 | BRF_GRA },           // 13
	{ "sns_obj3.bin",	0x80000, 0x0fb01e8b, 0x05 | BRF_GRA },           // 14
	{ "sns_obj7.bin",	0x80000, 0xfc1f26af, 0x05 | BRF_GRA },           // 15

	{ "sns_chr0.bin",	0x80000, 0xcdc42b61, 0x06 | BRF_GRA },           // 16 Layer Tiles
	{ "sns_chr1.bin",	0x80000, 0x42d4cbb7, 0x06 | BRF_GRA },           // 17
	{ "sns_chr2.bin",	0x80000, 0x7dbaa14e, 0x06 | BRF_GRA },           // 18
	{ "sns_chr3.bin",	0x80000, 0xb562ff72, 0x06 | BRF_GRA },           // 19

	{ "sns_sha0.bin",	0x80000, 0x0374fd67, 0x08 | BRF_GRA },           // 20 Layer Tiles Mask Data

	{ "sns_dat0.13s",	0x20000, 0x48295d93, 0x09 | BRF_PRG | BRF_ESS }, // 21 Shared 68K Data
	{ "sns_dat1.13p",	0x20000, 0xb44cc656, 0x09 | BRF_PRG | BRF_ESS }, // 22
	{ "sns_dat2.13r",	0x20000, 0xca2ae645, 0x09 | BRF_PRG | BRF_ESS }, // 23
	{ "sns_dat3.13n",	0x20000, 0x203bb018, 0x09 | BRF_PRG | BRF_ESS }, // 24

	{ "sns_voi1.bin",	0x80000, 0x219c97f7, 0x0a | BRF_SND },           // 25 C140 Samples
	{ "sns_voi2.bin",	0x80000, 0x562ec86b, 0x0a | BRF_SND },           // 26

	{ "sgunner2.nv",	0x02000, 0x57a521c6, 0x0b | BRF_PRG | BRF_ESS }, // 27 Default NV RAM
};

STD_ROM_PICK(sgunner2)
STD_ROM_FN(sgunner2)

static UINT16 sgunner2_key_read(UINT8 offset)
{
	if (offset == 0x04) return 0x15a;

	return BurnRandom();
}

static INT32 Sgunner2Init()
{
	return SgunnerCommonInit(NULL, sgunner2_key_read);
}

struct BurnDriver BurnDrvSgunner2 = {
	"sgunner2", NULL, NULL, NULL, "1991",
	"Steel Gunner 2 (US)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, sgunner2RomInfo, sgunner2RomName, NULL, NULL, NULL, NULL, SgunnerInputInfo, SgunnerDIPInfo,
	Sgunner2Init, Namcos2Exit, DrvFrame, SgunnerDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Steel Gunner 2 (Japan, Rev A)

static struct BurnRomInfo sgunner2jRomDesc[] = {
	{ "sns1mpr0a.bin",	0x20000, 0xe7216ad7, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sns1mpr1a.bin",	0x20000, 0x6caef2ee, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "sns_spr0.bin",	0x10000, 0xe5e40ed0, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "sns_spr1.bin",	0x10000, 0x3a85a5e9, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "sns_snd0.bin",	0x20000, 0xf079cd32, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2_c68.3f",	0x08000, 0xca64550a, 0x00 | BRF_PRG | BRF_ESS }, //	5 c68 Code (unused for now)

	//	not actually on this hw, but works
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  6 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  7
#endif

	{ "sns_obj0.bin",	0x80000, 0xc762445c, 0x05 | BRF_GRA },           //  8 Sprites
	{ "sns_obj4.bin",	0x80000, 0x0b1be894, 0x05 | BRF_GRA },           //  9
	{ "sns_obj1.bin",	0x80000, 0xe9e379d8, 0x05 | BRF_GRA },           // 10
	{ "sns_obj5.bin",	0x80000, 0x416b14e1, 0x05 | BRF_GRA },           // 11
	{ "sns_obj2.bin",	0x80000, 0x0d076f6c, 0x05 | BRF_GRA },           // 12
	{ "sns_obj6.bin",	0x80000, 0xc2e94ed2, 0x05 | BRF_GRA },           // 13
	{ "sns_obj3.bin",	0x80000, 0x0fb01e8b, 0x05 | BRF_GRA },           // 14
	{ "sns_obj7.bin",	0x80000, 0xfc1f26af, 0x05 | BRF_GRA },           // 15

	{ "sns_chr0.bin",	0x80000, 0xcdc42b61, 0x06 | BRF_GRA },           // 16 Layer Tiles
	{ "sns_chr1.bin",	0x80000, 0x42d4cbb7, 0x06 | BRF_GRA },           // 17
	{ "sns_chr2.bin",	0x80000, 0x7dbaa14e, 0x06 | BRF_GRA },           // 18
	{ "sns_chr3.bin",	0x80000, 0xb562ff72, 0x06 | BRF_GRA },           // 19

	{ "sns_sha0.bin",	0x80000, 0x0374fd67, 0x08 | BRF_GRA },           // 20 Layer Tiles Mask Data

	{ "sns_dat0.13s",	0x20000, 0x48295d93, 0x09 | BRF_PRG | BRF_ESS }, // 21 Shared 68K Data
	{ "sns_dat1.13p",	0x20000, 0xb44cc656, 0x09 | BRF_PRG | BRF_ESS }, // 22
	{ "sns_dat2.13r",	0x20000, 0xca2ae645, 0x09 | BRF_PRG | BRF_ESS }, // 23
	{ "sns_dat3.13n",	0x20000, 0x203bb018, 0x09 | BRF_PRG | BRF_ESS }, // 24

	{ "sns_voi1.bin",	0x80000, 0x219c97f7, 0x0a | BRF_SND },           // 25 C140 Samples
	{ "sns_voi2.bin",	0x80000, 0x562ec86b, 0x0a | BRF_SND },           // 26

	{ "sgunner2j.nv",	0x02000, 0x014bccf9, 0x0b | BRF_PRG | BRF_ESS }, // 27 Default NV RAM
};

STD_ROM_PICK(sgunner2j)
STD_ROM_FN(sgunner2j)

struct BurnDriver BurnDrvSgunner2j = {
	"sgunner2j", "sgunner2", NULL, NULL, "1991",
	"Steel Gunner 2 (Japan, Rev A)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, sgunner2jRomInfo, sgunner2jRomName, NULL, NULL, NULL, NULL, SgunnerInputInfo, SgunnerDIPInfo,
	Sgunner2Init, Namcos2Exit, DrvFrame, SgunnerDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Dirt Fox (Japan)

static struct BurnRomInfo dirtfoxjRomDesc[] = {
	{ "df1_mpr0.bin",	0x20000, 0x8386c820, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "df1_mpr1.bin",	0x20000, 0x51085728, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "df1_spr0.bin",	0x20000, 0xd4906585, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "df1_spr1.bin",	0x20000, 0x7d76cf57, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "df1_snd0.bin",	0x20000, 0x66b4f3ab, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "df1_obj0.bin",	0x80000, 0xb6bd1a68, 0x05 | BRF_GRA },           //  7 Sprites
	{ "df1_obj1.bin",	0x80000, 0x05421dc1, 0x05 | BRF_GRA },           //  8
	{ "df1_obj2.bin",	0x80000, 0x9390633e, 0x05 | BRF_GRA },           //  9
	{ "df1_obj3.bin",	0x80000, 0xc8447b33, 0x05 | BRF_GRA },           // 10

	{ "df1_chr0.bin",	0x20000, 0x4b10e4ed, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "df1_chr1.bin",	0x20000, 0x8f63f3d6, 0x06 | BRF_GRA },           // 12
	{ "df1_chr2.bin",	0x20000, 0x5a1b852a, 0x06 | BRF_GRA },           // 13
	{ "df1_chr3.bin",	0x20000, 0x28570676, 0x06 | BRF_GRA },           // 14

	{ "df1_roz0.bin",	0x40000, 0xa6129f94, 0x07 | BRF_GRA },           // 15 Roz Layer Tiles
	{ "df1_roz1.bin",	0x40000, 0xc8e7ce73, 0x07 | BRF_GRA },           // 16
	{ "df1_roz2.bin",	0x40000, 0xc598e923, 0x07 | BRF_GRA },           // 17
	{ "df1_roz3.bin",	0x40000, 0x5a38b062, 0x07 | BRF_GRA },           // 18
	{ "df1_roz4.bin",	0x40000, 0xe196d2e8, 0x07 | BRF_GRA },           // 19
	{ "df1_roz5.bin",	0x40000, 0x1f8a1a3c, 0x07 | BRF_GRA },           // 20
	{ "df1_roz6.bin",	0x40000, 0x7f3a1ed9, 0x07 | BRF_GRA },           // 21
	{ "df1_roz7.bin",	0x40000, 0xdd546ae8, 0x07 | BRF_GRA },           // 22

	{ "df1_sha.bin",	0x20000, 0x9a7c9a9b, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "df1_dat0.13s",	0x40000, 0xf5851c85, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "df1_dat1.13p",	0x40000, 0x1a31e46b, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "df1_voi1.bin",	0x80000, 0x15053904, 0x0a | BRF_SND },           // 26 C140 Samples Samples
	
	{ "nvram",			0x02000, 0x4b9f7b06, 0x00 | BRF_OPT },
};

STD_ROM_PICK(dirtfoxj)
STD_ROM_FN(dirtfoxj)

static UINT16 dirtfoxj_key_read(UINT8 offset)
{
	if (offset == 0x01) return 0x00b4;

	return BurnRandom();
}

static INT32 DirtfoxjInit()
{
	is_dirtfoxj = 1;

	INT32 rc = Namcos2Init(NULL, dirtfoxj_key_read);

	if (!rc) {
		nvramcheck = 3;
	}

	return rc;

}

struct BurnDriver BurnDrvDirtfoxj = {
	"dirtfoxj", NULL, NULL, NULL, "1989",
	"Dirt Fox (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, dirtfoxjRomInfo, dirtfoxjRomName, NULL, NULL, NULL, NULL, DirtfoxInputInfo, DirtfoxDIPInfo,
	DirtfoxjInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Golly! Ghost!

static struct BurnRomInfo gollyghoRomDesc[] = {
	{ "gl2mpr0.11d",	0x10000, 0xe5d48bb9, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "gl2mpr1.13d",	0x10000, 0x584ef971, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "gl1spr0.11k",	0x10000, 0xa108136f, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "gl1spr1.13k",	0x10000, 0xda8443b7, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "gl1snd0.7j",		0x20000, 0x008bce72, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "gl1edr0c.ic7",	0x08000, 0xdb60886f, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "gl1obj0.5b",		0x40000, 0x6809d267, 0x05 | BRF_GRA },           //  7 Sprites
	{ "gl1obj1.4b",		0x40000, 0xae4304d4, 0x05 | BRF_GRA },           //  8
	{ "gl1obj2.5d",		0x40000, 0x9f2e9eb0, 0x05 | BRF_GRA },           //  9
	{ "gl1obj3.4d",		0x40000, 0x3a85f3c2, 0x05 | BRF_GRA },           // 10

	{ "gl1chr0.11n",	0x20000, 0x1a7c8abd, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "gl1chr1.11p",	0x20000, 0x36aa0fbc, 0x06 | BRF_GRA },           // 12
	{ "gl1chr2.11r",	0x10000, 0x6c1964ba, 0x06 | BRF_GRA },           // 13

	{ "gl1sha0.7n",		0x20000, 0x8886f6f5, 0x08 | BRF_GRA },           // 14 Layer Tiles Mask Data

	{ "04544191.6n",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 15 Sprite Zoom (nused)

	{ "gl1voi1.3m",		0x80000, 0x0eca0efb, 0x0a | BRF_SND },           // 16 C140 Samples

	{ "gollygho.nv",	0x02000, 0xb7e67b9d, 0x0b | BRF_PRG | BRF_ESS }, // 17 Default NV RAM
};

STD_ROM_PICK(gollygho)
STD_ROM_FN(gollygho)

static UINT16 gollygho_key_read(UINT8 offset)
{
	switch (offset)
	{
		case 0:
		case 1: return 0x0002;
		case 2: return 0x0000;
		case 4: return 0x0143;
	}

	return BurnRandom();
}

static INT32 GollyghoInit()
{
	return Namcos2Init(NULL, gollygho_key_read);
}

struct BurnDriver BurnDrvGollygho = {
	"gollygho", NULL, NULL, NULL, "1990",
	"Golly! Ghost!\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, gollyghoRomInfo, gollyghoRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //GollyghoInputInfo, GollyghoDIPInfo,
	GollyghoInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Bubble Trouble (World, Rev B)

static struct BurnRomInfo bubbletrRomDesc[] = {
	{ "bt2-mpr0b.bin",	0x20000, 0x26fbfce3, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "bt2-mpr1b.bin",	0x20000, 0x21f42ab2, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bt1-spr0.11k",	0x10000, 0xb507b00a, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "bt1-spr1.13k",	0x10000, 0x4f35540f, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "bt1-snd0.7j",	0x20000, 0x46a5c625, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "bt1edr0a.ic7",	0x08000, 0x155b02fc, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "bt1-obj0.5b",	0x80000, 0x16b5dc04, 0x05 | BRF_GRA },           //  7 Sprites
	{ "bt1-obj1.4b",	0x80000, 0xae37a969, 0x05 | BRF_GRA },           //  8
	{ "bt1-obj2.5d",	0x80000, 0x75f74871, 0x05 | BRF_GRA },           //  9
	{ "bt1-obj3.4d",	0x80000, 0x7fb23c05, 0x05 | BRF_GRA },           // 10

	{ "bt1-chr0.11n",	0x80000, 0x11574c30, 0x06 | BRF_GRA },           // 11 Layer Tiles

	{ "bt1-sha0.7n",	0x80000, 0xdc4664df, 0x07 | BRF_GRA },           // 12 Layer Tiles Mask Data

	{ "bt1_dat0.13s",	0x20000, 0x1001a14e, 0x09 | BRF_PRG | BRF_ESS }, // 13 Shared 68K Data
	{ "bt1_dat1.13p",	0x20000, 0x7de6a839, 0x09 | BRF_PRG | BRF_ESS }, // 14

	{ "04544191.6n",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 15 Sprite Zoom (unused)

	{ "bt1-voi1.3m",	0x80000, 0x08b3a089, 0x0a | BRF_SND },           // 16 C140 Samples

	{ "bubbletr.nv",	0x02000, 0x75ace624, 0x0b | BRF_PRG | BRF_ESS }, // 17 Default NV RAM
};

STD_ROM_PICK(bubbletr)
STD_ROM_FN(bubbletr)

static UINT16 bubbletr_key_read(UINT8 offset)
{
	switch (offset)
	{
		case 0:
		case 1: return 0x0002;
		case 2: return 0x0000;
		case 4: return 0x0141;
	}

	return BurnRandom();
}

static INT32 BubbletrInit()
{
	return Namcos2Init(NULL, bubbletr_key_read);
}

struct BurnDriver BurnDrvBubbletr = {
	"bubbletr", NULL, NULL, NULL, "1992",
	"Bubble Trouble (World, Rev B)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bubbletrRomInfo, bubbletrRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //BubbletrInputInfo, BubbletrDIPInfo,
	BubbletrInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Bubble Trouble (Japan, Rev C)

static struct BurnRomInfo bubbletrjRomDesc[] = {
	{ "bt1-mpr0c.11d",	0x20000, 0x64eb3496, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "bt1-mpr1c.13d",	0x20000, 0x26785bce, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bt1-spr0.11k",	0x10000, 0xb507b00a, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "bt1-spr1.13k",	0x10000, 0x4f35540f, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "bt1-snd0.7j",	0x20000, 0x46a5c625, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "bt1edr0a.ic7",	0x08000, 0x155b02fc, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "bt1-obj0.5b",	0x80000, 0x16b5dc04, 0x05 | BRF_GRA },           //  7 Sprites
	{ "bt1-obj1.4b",	0x80000, 0xae37a969, 0x05 | BRF_GRA },           //  8
	{ "bt1-obj2.5d",	0x80000, 0x75f74871, 0x05 | BRF_GRA },           //  9
	{ "bt1-obj3.4d",	0x80000, 0x7fb23c05, 0x05 | BRF_GRA },           // 10

	{ "bt1-chr0.11n",	0x80000, 0x11574c30, 0x06 | BRF_GRA },           // 11 Layer Tiles

	{ "bt1-sha0.7n",	0x80000, 0xdc4664df, 0x08 | BRF_GRA },           // 12 Layer Tiles Mask Data

	{ "bt1_dat0.13s",	0x20000, 0x1001a14e, 0x09 | BRF_PRG | BRF_ESS }, // 13 Shared 68K Data
	{ "bt1_dat1.13p",	0x20000, 0x7de6a839, 0x09 | BRF_PRG | BRF_ESS }, // 14

	{ "04544191.6n",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 15 Sprite Zoom (unused)

	{ "bt1-voi1.3m",	0x80000, 0x08b3a089, 0x0a | BRF_SND },           // 16 C140 Samples

	{ "bubbletr.nv",	0x02000, 0x75ace624, 0x0b | BRF_PRG | BRF_ESS }, // 17 Default NV RAM
};

STD_ROM_PICK(bubbletrj)
STD_ROM_FN(bubbletrj)

struct BurnDriver BurnDrvBubbletrj = {
	"bubbletrj", "bubbletr", NULL, NULL, "1992",
	"Bubble Trouble (Japan, Rev C)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bubbletrjRomInfo, bubbletrjRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //BubbletrInputInfo, BubbletrDIPInfo,
	BubbletrInit, Namcos2Exit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Metal Hawk (Rev C)

static struct BurnRomInfo metlhawkRomDesc[] = {
	{ "mh2mp0c.11d",	0x20000, 0xcd7dae6e, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mh2mp1c.13d",	0x20000, 0xe52199fd, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mh1sp0f.11k",	0x10000, 0x2c141fea, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "mh1sp1f.13k",	0x10000, 0x8ccf98e0, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "mh1s0.7j",		0x20000, 0x79e054cf, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "mhobj-4.5c",		0x40000, 0xe3590e1a, 0x05 | BRF_GRA },           //  7 Sprites
	{ "mhobj-5.5a",		0x40000, 0xb85c0d07, 0x05 | BRF_GRA },           //  8
	{ "mhobj-6.6c",		0x40000, 0x90c4523d, 0x05 | BRF_GRA },           //  9
	{ "mhobj-7.6a",		0x40000, 0xf00edb39, 0x05 | BRF_GRA },           // 10
	{ "mhobj-0.5d",		0x40000, 0x52ae6620, 0x05 | BRF_GRA },           // 11
	{ "mhobj-1.5b",		0x40000, 0x2c2a1291, 0x05 | BRF_GRA },           // 12
	{ "mhobj-2.6d",		0x40000, 0x6221b927, 0x05 | BRF_GRA },           // 13
	{ "mhobj-3.6b",		0x40000, 0xfd09f2f1, 0x05 | BRF_GRA },           // 14

	{ "mhchr-0.11n",	0x20000, 0xe2da1b14, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "mhchr-1.11p",	0x20000, 0x023c78f9, 0x06 | BRF_GRA },           // 16
	{ "mhchr-2.11r",	0x20000, 0xece47e91, 0x06 | BRF_GRA },           // 17
	{ "mh1c3.11s",		0x20000, 0x9303aa7f, 0x06 | BRF_GRA },           // 18

	{ "mhr0z-0.2d",		0x40000, 0x30ade98f, 0x07 | BRF_GRA },           // 19 Roz Layer Tiles
	{ "mhr0z-1.2c",		0x40000, 0xa7fff42a, 0x07 | BRF_GRA },           // 20
	{ "mhr0z-2.2b",		0x40000, 0x6abec820, 0x07 | BRF_GRA },           // 21
	{ "mhr0z-3.2a",		0x40000, 0xd53cec6d, 0x07 | BRF_GRA },           // 22
	{ "mhr0z-4.3d",		0x40000, 0x922213e2, 0x07 | BRF_GRA },           // 23
	{ "mhr0z-5.3c",		0x40000, 0x78418a54, 0x07 | BRF_GRA },           // 24
	{ "mhr0z-6.3b",		0x40000, 0x6c74977e, 0x07 | BRF_GRA },           // 25
	{ "mhr0z-7.3a",		0x40000, 0x68a19cbd, 0x07 | BRF_GRA },           // 26

	{ "mh1sha.7n",		0x20000, 0x6ac22294, 0x08 | BRF_GRA },           // 27 Layer Tiles Mask Data

	{ "mh-rzsh.bin",	0x40000, 0x5090b1cf, 0x0d | BRF_GRA },           // 28 Roz Layer Tiles Mask Data

	{ "mh1d0.13s",		0x20000, 0x8b178ac7, 0x09 | BRF_PRG | BRF_ESS }, // 29 Shared 68K Data
	{ "mh1d1.13p",		0x20000, 0x10684fd6, 0x09 | BRF_PRG | BRF_ESS }, // 30

	{ "mhvoi-1.bin",	0x80000, 0x2723d137, 0x0a | BRF_SND },           // 31 C140 Samples Samples
	{ "mhvoi-2.bin",	0x80000, 0xdbc92d91, 0x0a | BRF_SND },           // 32

	{ "mh5762.7p",		0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 33 Sprite zoom (unused)

	{ "ampal16l8a-sys87b-1.4g",	0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 34 plds
	{ "ampal16l8a-sys87b-2.5e",	0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 35
	{ "pal12l10-sys87b-3.8d",	0x00040, 0xd3ae64a6, 0 | BRF_OPT },      // 36

	{ "metlhawk.nv",	0x02000, 0x547cb0dc, 0x0b | BRF_PRG | BRF_ESS }, // 37 Default NV RAM
};

STD_ROM_PICK(metlhawk)
STD_ROM_FN(metlhawk)

struct BurnDriver BurnDrvMetlhawk = {
	"metlhawk", NULL, NULL, NULL, "1988",
	"Metal Hawk (Rev C)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, metlhawkRomInfo, metlhawkRomName, NULL, NULL, NULL, NULL, MetlhawkInputInfo, MetlhawkDIPInfo,
	MetlhawkInit, Namcos2Exit, DrvFrame, MetlhawkDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Metal Hawk (Japan, Rev F)

static struct BurnRomInfo metlhawkjRomDesc[] = {
	{ "mh1mpr0f.11d",	0x20000, 0xe22dfb6e, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "mh1mpr1f.13d",	0x20000, 0xd139a7b7, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "mh1sp0f.11k",	0x10000, 0x2c141fea, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "mh1sp1f.13k",	0x10000, 0x8ccf98e0, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "mh1s0.7j",		0x20000, 0x79e054cf, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "mhobj-4.5c",		0x40000, 0xe3590e1a, 0x05 | BRF_GRA },           //  7 Sprites
	{ "mhobj-5.5a",		0x40000, 0xb85c0d07, 0x05 | BRF_GRA },           //  8
	{ "mhobj-6.6c",		0x40000, 0x90c4523d, 0x05 | BRF_GRA },           //  9
	{ "mhobj-7.6a",		0x40000, 0xf00edb39, 0x05 | BRF_GRA },           // 10
	{ "mhobj-0.5d",		0x40000, 0x52ae6620, 0x05 | BRF_GRA },           // 11
	{ "mhobj-1.5b",		0x40000, 0x2c2a1291, 0x05 | BRF_GRA },           // 12
	{ "mhobj-2.6d",		0x40000, 0x6221b927, 0x05 | BRF_GRA },           // 13
	{ "mhobj-3.6b",		0x40000, 0xfd09f2f1, 0x05 | BRF_GRA },           // 14

	{ "mhchr-0.11n",	0x20000, 0xe2da1b14, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "mhchr-1.11p",	0x20000, 0x023c78f9, 0x06 | BRF_GRA },           // 16
	{ "mhchr-2.11r",	0x20000, 0xece47e91, 0x06 | BRF_GRA },           // 17
	{ "mh1c3.11s",		0x20000, 0x9303aa7f, 0x06 | BRF_GRA },           // 18

	{ "mhr0z-0.2d",		0x40000, 0x30ade98f, 0x06 | BRF_GRA },           // 19 Roz Layer Tiles
	{ "mhr0z-1.2c",		0x40000, 0xa7fff42a, 0x06 | BRF_GRA },           // 20
	{ "mhr0z-2.2b",		0x40000, 0x6abec820, 0x06 | BRF_GRA },           // 21
	{ "mhr0z-3.2a",		0x40000, 0xd53cec6d, 0x06 | BRF_GRA },           // 22
	{ "mhr0z-4.3d",		0x40000, 0x922213e2, 0x06 | BRF_GRA },           // 23
	{ "mhr0z-5.3c",		0x40000, 0x78418a54, 0x06 | BRF_GRA },           // 24
	{ "mhr0z-6.3b",		0x40000, 0x6c74977e, 0x06 | BRF_GRA },           // 25
	{ "mhr0z-7.3a",		0x40000, 0x68a19cbd, 0x06 | BRF_GRA },           // 26

	{ "mh1sha.7n",		0x20000, 0x6ac22294, 0x08 | BRF_GRA },           // 27 Layer Tiles Mask Data

	{ "mh-rzsh.bin",	0x40000, 0x5090b1cf, 0x0d | BRF_GRA },           // 28 Roz Layer Tiles Mask Data

	{ "mh1d0.13s",		0x20000, 0x8b178ac7, 0x09 | BRF_PRG | BRF_ESS }, // 29 Shared 68K Data
	{ "mh1d1.13p",		0x20000, 0x10684fd6, 0x09 | BRF_PRG | BRF_ESS }, // 30

	{ "mhvoi-1.bin",	0x80000, 0x2723d137, 0x0a | BRF_SND },           // 31 C140 Samples
	{ "mhvoi-2.bin",	0x80000, 0xdbc92d91, 0x0a | BRF_SND },           // 32

	{ "metlhawk.nv",	0x02000, 0x547cb0dc, 0x0b | BRF_PRG | BRF_ESS }, // 33 Default NV RAM

	{ "mh5762.7p",		0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 34 Sprite Zoom (unused)

	{ "ampal16l8a-sys87b-1.4g",	0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 35 plds
	{ "ampal16l8a-sys87b-2.5e",	0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 36
	{ "pal12l10-sys87b-3.8d",	0x00040, 0xd3ae64a6, 0 | BRF_OPT },           // 37
};

STD_ROM_PICK(metlhawkj)
STD_ROM_FN(metlhawkj)

struct BurnDriver BurnDrvMetlhawkj = {
	"metlhawkj", "metlhawk", NULL, NULL, "1988",
	"Metal Hawk (Japan, Rev F)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, metlhawkjRomInfo, metlhawkjRomName, NULL, NULL, NULL, NULL, MetlhawkInputInfo, MetlhawkDIPInfo,
	MetlhawkInit, Namcos2Exit, DrvFrame, MetlhawkDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 288, 3, 4
};


// Lucky & Wild

static struct BurnRomInfo luckywldRomDesc[] = {
	{ "lw2mp0.11d",		0x20000, 0x368306bb, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "lw2mp1.13d",		0x20000, 0x9be3a4b8, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "lw1sp0.11k",		0x20000, 0x1eed12cb, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "lw1sp1.13k",		0x20000, 0x535033bc, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "lw1snd0.7j",		0x20000, 0xcc83c6b6, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif

	{ "lw1obj0.3p",		0x80000, 0x21485830, 0x05 | BRF_GRA },           //  6 Sprites
	{ "lw1obj4.3s",		0x80000, 0x0050458a, 0x05 | BRF_GRA },           // 10
	{ "lw1obj1.3w",		0x80000, 0xd6437a82, 0x05 | BRF_GRA },           //  7
	{ "lw1obj5.3x",		0x80000, 0xcbc08f46, 0x05 | BRF_GRA },           // 11
	{ "lw1obj2.3t",		0x80000, 0xceb6f516, 0x05 | BRF_GRA },           //  8
	{ "lw1obj6.3u",		0x80000, 0x29740c88, 0x05 | BRF_GRA },           // 12
	{ "lw1obj3.3y",		0x80000, 0x5d32c7e9, 0x05 | BRF_GRA },           //  9
	{ "lw1obj7.3z",		0x80000, 0x8cbd62b4, 0x05 | BRF_GRA },           // 13

	{ "lw1chr0.11n",	0x80000, 0xa0da15fd, 0x06 | BRF_GRA },           // 14 Layer Tiles
	{ "lw1chr1.11p",	0x80000, 0x89102445, 0x06 | BRF_GRA },           // 15
	{ "lw1chr2.11r",	0x80000, 0xc749b778, 0x06 | BRF_GRA },           // 16
	{ "lw1chr3.11s",	0x80000, 0xd76f9578, 0x06 | BRF_GRA },           // 17
	{ "lw1chr4.9n",		0x80000, 0x2f8ab45e, 0x06 | BRF_GRA },           // 18
	{ "lw1chr5.9p",		0x80000, 0xc9acbe61, 0x06 | BRF_GRA },           // 19

	{ "lw1roz2.23e",	0x80000, 0x1ef46d82, 0x07 | BRF_GRA },           // 20 Roz Layer Tiles
	{ "lw1roz1.23c",	0x80000, 0x74e98793, 0x07 | BRF_GRA },           // 21
	{ "lw1roz0.23b",	0x80000, 0xa14079c9, 0x07 | BRF_GRA },           // 22

	{ "lw1sha0.7n",		0x80000, 0xe3a65196, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "lw1rzs0.20z",	0x80000, 0xa1071537, 0x0d | BRF_GRA },           // 24 Roz Layer Tiles Mask data

	{ "lw1dat0.13s",	0x80000, 0x5d387d47, 0x09 | BRF_PRG | BRF_ESS }, // 25 Shared 68K Data
	{ "lw1dat1.13p",	0x80000, 0x7ba94476, 0x09 | BRF_PRG | BRF_ESS }, // 26
	{ "lw1dat2.13r",	0x80000, 0xeeba7c62, 0x09 | BRF_PRG | BRF_ESS }, // 27
	{ "lw1dat3.13n",	0x80000, 0xec3b36ea, 0x09 | BRF_PRG | BRF_ESS }, // 28

	{ "lw1voi1.3m",		0x80000, 0xb3e57993, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "lw1voi2.3l",		0x80000, 0xcd8b86a2, 0x0a | BRF_SND },           // 30

	{ "lw1ld8.10w",		0x00100, 0x29058c73, 0x0c | BRF_GRA },           // 31 C45 Color Look-up

	{ "luckywld.nv",	0x02000, 0x0c185d2a, 0x0b | BRF_PRG | BRF_ESS }, // 32 Default NV RAM
};

STD_ROM_PICK(luckywld)
STD_ROM_FN(luckywld)

struct BurnDriver BurnDrvLuckywld = {
	"luckywld", NULL, NULL, NULL, "1992",
	"Lucky & Wild\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, luckywldRomInfo, luckywldRomName, NULL, NULL, NULL, NULL, LuckywldInputInfo, LuckywldDIPInfo,
	LuckywldInit, Namcos2Exit, DrvFrame, LuckywldDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Lucky & Wild (Japan)

static struct BurnRomInfo luckywldjRomDesc[] = {
	{ "lw1mpr0.11d",	0x20000, 0x7dce8ba6, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "lw1mpr1.13d",	0x20000, 0xce3b0f37, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "lw1sp0.11k",		0x20000, 0x1eed12cb, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "lw1sp1.13k",		0x20000, 0x535033bc, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "lw1snd0.7j",		0x20000, 0xcc83c6b6, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif

	{ "lw1obj0.3p",		0x80000, 0x21485830, 0x05 | BRF_GRA },           //  6 Sprites
	{ "lw1obj4.3s",		0x80000, 0x0050458a, 0x05 | BRF_GRA },           //  8
	{ "lw1obj1.3w",		0x80000, 0xd6437a82, 0x05 | BRF_GRA },           // 10
	{ "lw1obj5.3x",		0x80000, 0xcbc08f46, 0x05 | BRF_GRA },           // 12
	{ "lw1obj2.3t",		0x80000, 0xceb6f516, 0x05 | BRF_GRA },           //  7
	{ "lw1obj6.3u",		0x80000, 0x29740c88, 0x05 | BRF_GRA },           //  9
	{ "lw1obj3.3y",		0x80000, 0x5d32c7e9, 0x05 | BRF_GRA },           // 11
	{ "lw1obj7.3z",		0x80000, 0x8cbd62b4, 0x05 | BRF_GRA },           // 13

	{ "lw1chr0.11n",	0x80000, 0xa0da15fd, 0x06 | BRF_GRA },           // 14 Layer Tiles
	{ "lw1chr1.11p",	0x80000, 0x89102445, 0x06 | BRF_GRA },           // 15
	{ "lw1chr2.11r",	0x80000, 0xc749b778, 0x06 | BRF_GRA },           // 16
	{ "lw1chr3.11s",	0x80000, 0xd76f9578, 0x06 | BRF_GRA },           // 17
	{ "lw1chr4.9n",		0x80000, 0x2f8ab45e, 0x06 | BRF_GRA },           // 18
	{ "lw1chr5.9p",		0x80000, 0xc9acbe61, 0x06 | BRF_GRA },           // 19

	{ "lw1roz2.23e",	0x80000, 0x1ef46d82, 0x07 | BRF_GRA },           // 20 Roz Layer Tiles
	{ "lw1roz1.23c",	0x80000, 0x74e98793, 0x07 | BRF_GRA },           // 21
	{ "lw1roz0.23b",	0x80000, 0xa14079c9, 0x07 | BRF_GRA },           // 22

	{ "lw1sha0.7n",		0x80000, 0xe3a65196, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "lw1rzs0.20z",	0x80000, 0xa1071537, 0x0d | BRF_GRA },           // 24 Roz Layer Tiles Mask data

	{ "lw1dat0.13s",	0x80000, 0x5d387d47, 0x09 | BRF_PRG | BRF_ESS }, // 25 Shared 68K Data
	{ "lw1dat1.13p",	0x80000, 0x7ba94476, 0x09 | BRF_PRG | BRF_ESS }, // 26
	{ "lw1dat2.13r",	0x80000, 0xeeba7c62, 0x09 | BRF_PRG | BRF_ESS }, // 27
	{ "lw1dat3.13n",	0x80000, 0xec3b36ea, 0x09 | BRF_PRG | BRF_ESS }, // 28

	{ "lw1voi1.3m",		0x80000, 0xb3e57993, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "lw1voi2.3l",		0x80000, 0xcd8b86a2, 0x0a | BRF_SND },           // 30

	{ "lw1ld8.10w",		0x00100, 0x29058c73, 0x0c | BRF_GRA },           // 31 C45 Color Look-up

	{ "luckywld.nv",	0x02000, 0x0c185d2a, 0x0b | BRF_PRG | BRF_ESS }, // 32 Default NV RAM
};

STD_ROM_PICK(luckywldj)
STD_ROM_FN(luckywldj)

struct BurnDriver BurnDrvLuckywldj = {
	"luckywldj", "luckywld", NULL, NULL, "1992",
	"Lucky & Wild (Japan)\0", NULL, "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, luckywldjRomInfo, luckywldjRomName, NULL, NULL, NULL, NULL, LuckywldInputInfo, LuckywldDIPInfo,
	LuckywldInit, Namcos2Exit, DrvFrame, LuckywldDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};



// Suzuka 8 Hours (World, Rev C)

static struct BurnRomInfo suzuka8hRomDesc[] = {
	{ "eh2-mp0c.bin",	0x20000, 0x9b9271ac, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "eh2-mp1c.bin",	0x20000, 0x24fdd4bc, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "eh1-sp0.bin",	0x20000, 0x4a8c4709, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "eh1-sp1.bin",	0x20000, 0x2256b14e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "eh1-snd0.bin",	0x20000, 0x36748d3c, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "eh1-obj0.bin",	0x80000, 0x864b6816, 0x05 | BRF_GRA },           //  7 Sprites
	{ "eh1-obj4.bin",	0x80000, 0xcde13867, 0x05 | BRF_GRA },           //  8
	{ "eh1-obj1.bin",	0x80000, 0xd4921c35, 0x05 | BRF_GRA },           //  9
	{ "eh1-obj5.bin",	0x80000, 0x9f210546, 0x05 | BRF_GRA },           // 10
	{ "eh1-obj2.bin",	0x80000, 0x966d3f19, 0x05 | BRF_GRA },           // 11
	{ "eh1-obj6.bin",	0x80000, 0x6019fc8c, 0x05 | BRF_GRA },           // 12
	{ "eh1-obj3.bin",	0x80000, 0x7d253cbe, 0x05 | BRF_GRA },           // 13
	{ "eh1-obj7.bin",	0x80000, 0x0bd966b8, 0x05 | BRF_GRA },           // 14

	{ "eh2-chr0.bin",	0x80000, 0xb2450fd2, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "eh2-chr1.bin",	0x80000, 0x57204651, 0x06 | BRF_GRA },           // 16
	{ "eh1-chr2.bin",	0x80000, 0x8150f644, 0x06 | BRF_GRA },           // 17

	{ "eh2-sha0.bin",	0x80000, 0x7f24619c, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "eh1-d0.bin",		0x40000, 0xb43e5dfa, 0x09 | BRF_PRG | BRF_ESS }, // 19 Shared 68K Data
	{ "eh1-d1.bin",		0x40000, 0x9825d5bf, 0x09 | BRF_PRG | BRF_ESS }, // 20
	{ "eh1-d3.bin",		0x40000, 0xf46d301f, 0x09 | BRF_PRG | BRF_ESS }, // 21

	{ "ehs1_landdt.10w",	0x00100, 0xcde7e8a6, 0x0c | BRF_GRA },           // 22 c45_road:clut

	{ "eh1-voi1.bin",	0x80000, 0x71e534d3, 0x0a | BRF_SND },           // 23 C140 Samples
	{ "eh1-voi2.bin",	0x80000, 0x3e20df8e, 0x0a | BRF_SND },           // 24
};

STD_ROM_PICK(suzuka8h)
STD_ROM_FN(suzuka8h)

static INT32 Suzuka8hInit()
{
	INT32 nRet = Suzuka8hCommonInit(NULL, NULL);

	if (nRet == 0)
	{
		BurnByteswap(Drv68KData + 0x100000, 0x100000); // 3rd data rom loaded odd
	}

	return nRet;
}

struct BurnDriverD BurnDrvSuzuka8h = {
	"suzuka8h", NULL, NULL, NULL, "1992",
	"Suzuka 8 Hours (World, Rev C)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, suzuka8hRomInfo, suzuka8hRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //SuzukaInputInfo, SuzukaDIPInfo,
	Suzuka8hInit, Namcos2Exit, DrvFrame, Suzuka8hDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Suzuka 8 Hours (Japan, Rev B)

static struct BurnRomInfo suzuka8hjRomDesc[] = {
	{ "eh1-mp0b.bin",	0x20000, 0x2850f469, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "eh1-mp1b.bin",	0x20000, 0xbe83eb2c, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "eh1-sp0.bin",	0x20000, 0x4a8c4709, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "eh1-sp1.bin",	0x20000, 0x2256b14e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "eh1-snd0.bin",	0x20000, 0x36748d3c, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "eh1-obj0.bin",	0x80000, 0x864b6816, 0x05 | BRF_GRA },           //  7 Sprites
	{ "eh1-obj4.bin",	0x80000, 0xcde13867, 0x05 | BRF_GRA },           //  8
	{ "eh1-obj1.bin",	0x80000, 0xd4921c35, 0x05 | BRF_GRA },           //  9
	{ "eh1-obj5.bin",	0x80000, 0x9f210546, 0x05 | BRF_GRA },           // 10
	{ "eh1-obj2.bin",	0x80000, 0x966d3f19, 0x05 | BRF_GRA },           // 11
	{ "eh1-obj6.bin",	0x80000, 0x6019fc8c, 0x05 | BRF_GRA },           // 12
	{ "eh1-obj3.bin",	0x80000, 0x7d253cbe, 0x05 | BRF_GRA },           // 13
	{ "eh1-obj7.bin",	0x80000, 0x0bd966b8, 0x05 | BRF_GRA },           // 14

	{ "eh1-chr0.bin",	0x80000, 0xbc90ebef, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "eh1-chr1.bin",	0x80000, 0x61395018, 0x06 | BRF_GRA },           // 16
	{ "eh1-chr2.bin",	0x80000, 0x8150f644, 0x06 | BRF_GRA },           // 17

	{ "eh1-sha0.bin",	0x80000, 0x39585cf9, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "eh1-d0.bin",		0x40000, 0xb43e5dfa, 0x09 | BRF_PRG | BRF_ESS }, // 19 Shared 68K Data
	{ "eh1-d1.bin",		0x40000, 0x9825d5bf, 0x09 | BRF_PRG | BRF_ESS }, // 20
	{ "eh1-d3.bin",		0x40000, 0xf46d301f, 0x09 | BRF_PRG | BRF_ESS }, // 21

	{ "ehs1_landdt.10w",	0x00100, 0xcde7e8a6, 0x0c | BRF_GRA },           // 22 C45 Color Look-up

	{ "eh1-voi1.bin",	0x80000, 0x71e534d3, 0x0a | BRF_SND },           // 23 C140 Samples
	{ "eh1-voi2.bin",	0x80000, 0x3e20df8e, 0x0a | BRF_SND },           // 24
};

STD_ROM_PICK(suzuka8hj)
STD_ROM_FN(suzuka8hj)

struct BurnDriverD BurnDrvSuzuka8hj = {
	"suzuka8hj", "suzuka8h", NULL, NULL, "1992",
	"Suzuka 8 Hours (Japan, Rev B)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, suzuka8hjRomInfo, suzuka8hjRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //SuzukaInputInfo, SuzukaDIPInfo,
	Suzuka8hInit, Namcos2Exit, DrvFrame, Suzuka8hDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Suzuka 8 Hours 2 (World, Rev B)

static struct BurnRomInfo suzuk8h2RomDesc[] = {
	{ "ehs2_mp0b.11d",	0x20000, 0xade97f90, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ehs2_mp1b.13d",	0x20000, 0x19744a66, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ehs1-sp0.11k",	0x20000, 0x9ca967bc, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ehs1-sp1.13k",	0x20000, 0xf25bfaaa, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ehs1-snd0.7j",	0x20000, 0xfc95993b, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "ehs1-obj0.3p",	0x80000, 0xa0acf307, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ehs1-obj4.3s",	0x80000, 0x4e503ca5, 0x05 | BRF_GRA },           //  8
	{ "ehs1-obj1.3w",	0x80000, 0xca780b44, 0x05 | BRF_GRA },           //  9
	{ "ehs1-obj5.3x",	0x80000, 0x5405f2d9, 0x05 | BRF_GRA },           // 10
	{ "ehs1-obj2.3t",	0x80000, 0x83b45afe, 0x05 | BRF_GRA },           // 11
	{ "ehs1-obj6.3j",	0x80000, 0xf5fc8b23, 0x05 | BRF_GRA },           // 12
	{ "ehs1-obj3.3y",	0x80000, 0x360c03a8, 0x05 | BRF_GRA },           // 13
	{ "ehs1-obj7.3z",	0x80000, 0xda6bf51b, 0x05 | BRF_GRA },           // 14

	{ "ehs1-chr0.11n",	0x80000, 0x844efe0d, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "ehs1-chr1.11p",	0x80000, 0xe8480a6d, 0x06 | BRF_GRA },           // 16
	{ "ehs1-chr2.11r",	0x80000, 0xace2d871, 0x06 | BRF_GRA },           // 17
	{ "ehs1-chr3.11s",	0x80000, 0xc1680818, 0x06 | BRF_GRA },           // 18
	{ "ehs1-chr4.9n",	0x80000, 0x82e8c1d5, 0x06 | BRF_GRA },           // 19
	{ "ehs1-chr5.9p",	0x80000, 0x9448537c, 0x06 | BRF_GRA },           // 20
	{ "ehs1-chr6.9r",	0x80000, 0x2d1c01ad, 0x06 | BRF_GRA },           // 21
	{ "ehs1-chr7.9s",	0x80000, 0x18dd8676, 0x06 | BRF_GRA },           // 22

	{ "ehs1-sha0.7n",	0x80000, 0x0f0e2dbf, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "ehs1-dat0.13s",	0x80000, 0x12a202fb, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "ehs1-dat1.13r",	0x80000, 0x91790905, 0x09 | BRF_PRG | BRF_ESS }, // 25
	{ "ehs1-dat2.13p",	0x80000, 0x087da1f3, 0x09 | BRF_PRG | BRF_ESS }, // 26
	{ "ehs1-dat3.13n",	0x80000, 0x85aecb3f, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "ehs1-landdt.10w",	0x00100, 0xcde7e8a6, 0x0c | BRF_GRA },           // 28 C45 Color Look-up

	{ "ehs1-voi1.3m",	0x80000, 0xbf94eb42, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "ehs1-voi2.3l",	0x80000, 0x0e427604, 0x0a | BRF_SND },           // 30
};

STD_ROM_PICK(suzuk8h2)
STD_ROM_FN(suzuk8h2)

static UINT16 suzuka8h2_key_read(UINT8 offset)
{
	switch(offset)
	{
		case 3: return 0x014d;
		case 2: return 0x0000;
	}

	return BurnRandom();
}

static INT32 Suzuka8h2Init()
{
	return Suzuka8hCommonInit(NULL, suzuka8h2_key_read);
}

struct BurnDriverD BurnDrvSuzuk8h2 = {
	"suzuk8h2", NULL, NULL, NULL, "1993",
	"Suzuka 8 Hours 2 (World, Rev B)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, suzuk8h2RomInfo, suzuk8h2RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //SuzukaInputInfo, SuzukaDIPInfo,
	Suzuka8h2Init, Namcos2Exit, DrvFrame, Suzuka8hDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Suzuka 8 Hours 2 (Japan, Rev B)

static struct BurnRomInfo suzuk8h2jRomDesc[] = {
	{ "ehs1_mp0b.11d",	0x20000, 0xae40d445, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "ehs1_mp1b.13d",	0x20000, 0x9d5b0d43, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ehs1-sp0.11k",	0x20000, 0x9ca967bc, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "ehs1-sp1.13k",	0x20000, 0xf25bfaaa, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "ehs1-snd0.7j",	0x20000, 0xfc95993b, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "ehs1-obj0.3p",	0x80000, 0xa0acf307, 0x05 | BRF_GRA },           //  7 Sprites
	{ "ehs1-obj4.3s",	0x80000, 0x4e503ca5, 0x05 | BRF_GRA },           // 11
	{ "ehs1-obj1.3w",	0x80000, 0xca780b44, 0x05 | BRF_GRA },           //  8
	{ "ehs1-obj5.3x",	0x80000, 0x5405f2d9, 0x05 | BRF_GRA },           // 12
	{ "ehs1-obj2.3t",	0x80000, 0x83b45afe, 0x05 | BRF_GRA },           //  9
	{ "ehs1-obj6.3j",	0x80000, 0xf5fc8b23, 0x05 | BRF_GRA },           // 13
	{ "ehs1-obj3.3y",	0x80000, 0x360c03a8, 0x05 | BRF_GRA },           // 10
	{ "ehs1-obj7.3z",	0x80000, 0xda6bf51b, 0x05 | BRF_GRA },           // 14

	{ "ehs1-chr0.11n",	0x80000, 0x844efe0d, 0x06 | BRF_GRA },           // 15 Layer Tiles
	{ "ehs1-chr1.11p",	0x80000, 0xe8480a6d, 0x06 | BRF_GRA },           // 16
	{ "ehs1-chr2.11r",	0x80000, 0xace2d871, 0x06 | BRF_GRA },           // 17
	{ "ehs1-chr3.11s",	0x80000, 0xc1680818, 0x06 | BRF_GRA },           // 18
	{ "ehs1-chr4.9n",	0x80000, 0x82e8c1d5, 0x06 | BRF_GRA },           // 19
	{ "ehs1-chr5.9p",	0x80000, 0x9448537c, 0x06 | BRF_GRA },           // 20
	{ "ehs1-chr6.9r",	0x80000, 0x2d1c01ad, 0x06 | BRF_GRA },           // 21
	{ "ehs1-chr7.9s",	0x80000, 0x18dd8676, 0x06 | BRF_GRA },           // 22

	{ "ehs1-sha0.7n",	0x80000, 0x0f0e2dbf, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "ehs1-dat0.13s",	0x80000, 0x12a202fb, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "ehs1-dat1.13r",	0x80000, 0x91790905, 0x09 | BRF_PRG | BRF_ESS }, // 25
	{ "ehs1-dat2.13p",	0x80000, 0x087da1f3, 0x09 | BRF_PRG | BRF_ESS }, // 26
	{ "ehs1-dat3.13n",	0x80000, 0x85aecb3f, 0x09 | BRF_PRG | BRF_ESS }, // 27

	{ "ehs1-landdt.10w",	0x00100, 0xcde7e8a6, 0x0c | BRF_GRA },           // 28 C45 Color Look-up

	{ "ehs1-voi1.3m",	0x80000, 0xbf94eb42, 0x0a | BRF_SND },           // 29 C140 Samples
	{ "ehs1-voi2.3l",	0x80000, 0x0e427604, 0x0a | BRF_SND },           // 30
};

STD_ROM_PICK(suzuk8h2j)
STD_ROM_FN(suzuk8h2j)

struct BurnDriverD BurnDrvSuzuk8h2j = {
	"suzuk8h2j", "suzuk8h2", NULL, NULL, "1993",
	"Suzuka 8 Hours 2 (Japan, Rev B)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, suzuk8h2jRomInfo, suzuk8h2jRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //SuzukaInputInfo, SuzukaDIPInfo,
	Suzuka8h2Init, Namcos2Exit, DrvFrame, Suzuka8hDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap (Rev E)

static struct BurnRomInfo finallapRomDesc[] = {
	{ "fl2mp0e",		0x10000, 0xed805674, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl2mp1e",		0x10000, 0x4c1d523b, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fl1-sp0",		0x10000, 0x2c5ff15d, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fl1-sp1",		0x10000, 0xea9d1a2e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fl1-s0b",		0x20000, 0xf5d76989, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "obj-0b",		0x80000, 0xc6986523, 0x05 | BRF_GRA },           //  7 Sprites
	{ "obj-1b",		0x80000, 0x6af7d284, 0x05 | BRF_GRA },           //  8
	{ "obj-2b",		0x80000, 0xde45ca8d, 0x05 | BRF_GRA },           //  9
	{ "obj-3b",		0x80000, 0xdba830a2, 0x05 | BRF_GRA },           // 10

	{ "fl1-c0",		0x20000, 0xcd9d2966, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fl1-c1",		0x20000, 0xb0efec87, 0x06 | BRF_GRA },           // 12
	{ "fl1-c2",		0x20000, 0x263b8e31, 0x06 | BRF_GRA },           // 13
	{ "fl1-c3",		0x20000, 0xc2c56743, 0x06 | BRF_GRA },           // 14
	{ "fl2-c4",		0x20000, 0x83c77a50, 0x06 | BRF_GRA },           // 15
	{ "fl1-c5",		0x20000, 0xab89da77, 0x06 | BRF_GRA },           // 16
	{ "fl2-c6",		0x20000, 0x239bd9a0, 0x06 | BRF_GRA },           // 17

	{ "fl2-sha",		0x20000, 0x5fda0b6d, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 19 C45 Color Look-up

	{ "fl1-v1",		0x20000, 0x86b21996, 0x0a | BRF_SND },           // 20 C140 Samples
	{ "fl1-v2",		0x20000, 0x6a164647, 0x0a | BRF_SND },           // 21
};

STD_ROM_PICK(finallap)
STD_ROM_FN(finallap)

struct BurnDriverD BurnDrvFinallap = {
	"finallap", NULL, NULL, NULL, "1987",
	"Final Lap (Rev E)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, finallapRomInfo, finallapRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinallapInputInfo, FinallapDIPInfo,
	FinallapInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap (Rev D)

static struct BurnRomInfo finallapdRomDesc[] = {
	{ "fl2-mp0d",		0x10000, 0x3576d3aa, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl2-mp1d",		0x10000, 0x22d3906d, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fl1-sp0",		0x10000, 0x2c5ff15d, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fl1-sp1",		0x10000, 0xea9d1a2e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fl1-s0b",		0x20000, 0xf5d76989, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "obj-0b",		0x80000, 0xc6986523, 0x05 | BRF_GRA },           //  7 Sprites
	{ "obj-1b",		0x80000, 0x6af7d284, 0x05 | BRF_GRA },           //  8
	{ "obj-2b",		0x80000, 0xde45ca8d, 0x05 | BRF_GRA },           //  9
	{ "obj-3b",		0x80000, 0xdba830a2, 0x05 | BRF_GRA },           // 10

	{ "fl1-c0",		0x20000, 0xcd9d2966, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fl1-c1",		0x20000, 0xb0efec87, 0x06 | BRF_GRA },           // 12
	{ "fl1-c2",		0x20000, 0x263b8e31, 0x06 | BRF_GRA },           // 13
	{ "fl1-c3",		0x20000, 0xc2c56743, 0x06 | BRF_GRA },           // 14
	{ "fl2-c4",		0x20000, 0x83c77a50, 0x06 | BRF_GRA },           // 15
	{ "fl1-c5",		0x20000, 0xab89da77, 0x06 | BRF_GRA },           // 16
	{ "fl2-c6",		0x20000, 0x239bd9a0, 0x06 | BRF_GRA },           // 17

	{ "fl2-sha",		0x20000, 0x5fda0b6d, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 19 C45 Color Look-up

	{ "fl1-v1",		0x20000, 0x86b21996, 0x0a | BRF_SND },           // 20 C140 Samples
	{ "fl1-v2",		0x20000, 0x6a164647, 0x0a | BRF_SND },           // 21
};

STD_ROM_PICK(finallapd)
STD_ROM_FN(finallapd)

struct BurnDriverD BurnDrvFinallapd = {
	"finallapd", "finallap", NULL, NULL, "1987",
	"Final Lap (Rev D)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, finallapdRomInfo, finallapdRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinallapInputInfo, FinallapDIPInfo,
	FinallapInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap (Rev C)

static struct BurnRomInfo finallapcRomDesc[] = {
	{ "fl2-mp0c",		0x10000, 0xf667f2c9, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl2-mp1c",		0x10000, 0xb8615d33, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fl1-sp0",		0x10000, 0x2c5ff15d, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fl1-sp1",		0x10000, 0xea9d1a2e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fl1-s0",		0x20000, 0x1f8ff494, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "obj-0b",		0x80000, 0xc6986523, 0x05 | BRF_GRA },           //  7 Sprites
	{ "obj-1b",		0x80000, 0x6af7d284, 0x05 | BRF_GRA },           //  8
	{ "obj-2b",		0x80000, 0xde45ca8d, 0x05 | BRF_GRA },           //  9
	{ "obj-3b",		0x80000, 0xdba830a2, 0x05 | BRF_GRA },           // 10

	{ "fl1-c0",		0x20000, 0xcd9d2966, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fl1-c1",		0x20000, 0xb0efec87, 0x06 | BRF_GRA },           // 12
	{ "fl1-c2",		0x20000, 0x263b8e31, 0x06 | BRF_GRA },           // 13
	{ "fl1-c3",		0x20000, 0xc2c56743, 0x06 | BRF_GRA },           // 14
	{ "fl2-c4",		0x20000, 0x83c77a50, 0x06 | BRF_GRA },           // 15
	{ "fl1-c5",		0x20000, 0xab89da77, 0x06 | BRF_GRA },           // 16
	{ "fl2-c6",		0x20000, 0x239bd9a0, 0x06 | BRF_GRA },           // 17

	{ "fl2-sha",		0x20000, 0x5fda0b6d, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 19 C45 Color Look-up

	{ "fl1-v1",		0x20000, 0x86b21996, 0x0a | BRF_SND },           // 20 C140 Samples
	{ "fl1-v2",		0x20000, 0x6a164647, 0x0a | BRF_SND },           // 21
};

STD_ROM_PICK(finallapc)
STD_ROM_FN(finallapc)

struct BurnDriverD BurnDrvFinallapc = {
	"finallapc", "finallap", NULL, NULL, "1987",
	"Final Lap (Rev C)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, finallapcRomInfo, finallapcRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinallapInputInfo, FinallapDIPInfo,
	FinallapInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap (Japan, Rev C)

static struct BurnRomInfo finallapjcRomDesc[] = {
	{ "fl1_mp0c.bin",	0x10000, 0x63cd7304, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl1_mp1c.bin",	0x10000, 0xcc9c5fb6, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fl1-sp0",		0x10000, 0x2c5ff15d, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fl1-sp1",		0x10000, 0xea9d1a2e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fl1-s0b",		0x20000, 0xf5d76989, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "obj-0b",		0x80000, 0xc6986523, 0x05 | BRF_GRA },           //  7 Sprites
	{ "obj-1b",		0x80000, 0x6af7d284, 0x05 | BRF_GRA },           //  8
	{ "obj-2b",		0x80000, 0xde45ca8d, 0x05 | BRF_GRA },           //  9
	{ "obj-3b",		0x80000, 0xdba830a2, 0x05 | BRF_GRA },           // 10

	{ "fl1-c0",		0x20000, 0xcd9d2966, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fl1-c1",		0x20000, 0xb0efec87, 0x06 | BRF_GRA },           // 12
	{ "fl1-c2",		0x20000, 0x263b8e31, 0x06 | BRF_GRA },           // 13
	{ "fl1-c3",		0x20000, 0xc2c56743, 0x06 | BRF_GRA },           // 14
	{ "fl1-c4",		0x20000, 0xcdc1de2e, 0x06 | BRF_GRA },           // 15
	{ "fl1-c5",		0x20000, 0xab89da77, 0x06 | BRF_GRA },           // 16
	{ "fl1-c6",		0x20000, 0x8e78a3c3, 0x06 | BRF_GRA },           // 17

	{ "fl1_sha.bin",	0x20000, 0xb7e1c7a3, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 19 C45 Color Look-up

	{ "fl1-v1",		0x20000, 0x86b21996, 0x0a | BRF_SND },           // 20 C140 Samples
	{ "fl1-v2",		0x20000, 0x6a164647, 0x0a | BRF_SND },           // 21
};

STD_ROM_PICK(finallapjc)
STD_ROM_FN(finallapjc)

struct BurnDriverD BurnDrvFinallapjc = {
	"finallapjc", "finallap", NULL, NULL, "1987",
	"Final Lap (Japan, Rev C)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, finallapjcRomInfo, finallapjcRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinallapInputInfo, FinallapDIPInfo,
	FinallapInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap (Japan, Rev B)

static struct BurnRomInfo finallapjbRomDesc[] = {
	{ "fl1_mp0b.bin",	0x10000, 0x870a482a, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl1_mp1b.bin",	0x10000, 0xaf52c991, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fl1-sp0",		0x10000, 0x2c5ff15d, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fl1-sp1",		0x10000, 0xea9d1a2e, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fl1-s0",			0x20000, 0x1f8ff494, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "obj-0b",		0x80000, 0xc6986523, 0x05 | BRF_GRA },           //  7 Sprites
	{ "obj-1b",		0x80000, 0x6af7d284, 0x05 | BRF_GRA },           //  8
	{ "obj-2b",		0x80000, 0xde45ca8d, 0x05 | BRF_GRA },           //  9
	{ "obj-3b",		0x80000, 0xdba830a2, 0x05 | BRF_GRA },           // 10

	{ "fl1-c0",		0x20000, 0xcd9d2966, 0x06 | BRF_GRA },           // 11 Layer Tiles
	{ "fl1-c1",		0x20000, 0xb0efec87, 0x06 | BRF_GRA },           // 12
	{ "fl1-c2",		0x20000, 0x263b8e31, 0x06 | BRF_GRA },           // 13
	{ "fl1-c3",		0x20000, 0xc2c56743, 0x06 | BRF_GRA },           // 14
	{ "fl1-c4",		0x20000, 0xcdc1de2e, 0x06 | BRF_GRA },           // 15
	{ "fl1-c5",		0x20000, 0xab89da77, 0x06 | BRF_GRA },           // 16
	{ "fl1-c6",		0x20000, 0x8e78a3c3, 0x06 | BRF_GRA },           // 17

	{ "fl1_sha.bin",	0x20000, 0xb7e1c7a3, 0x08 | BRF_GRA },           // 18 Layer Tiles Mask Data

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 19 C45 Color Look-up

	{ "fl1-v1",		0x20000, 0x86b21996, 0x0a | BRF_SND },           // 20 C140 Samples
	{ "fl1-v2",		0x20000, 0x6a164647, 0x0a | BRF_SND },           // 21
};

STD_ROM_PICK(finallapjb)
STD_ROM_FN(finallapjb)

struct BurnDriverD BurnDrvFinallapjb = {
	"finallapjb", "finallap", NULL, NULL, "1987",
	"Final Lap (Japan, Rev B)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, finallapjbRomInfo, finallapjbRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinallapInputInfo, FinallapDIPInfo,
	FinallapInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 2

static struct BurnRomInfo finalap2RomDesc[] = {
	{ "fls2mp0b",		0x20000, 0x97b48aae, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fls2mp1b",		0x20000, 0xc9f3e0e7, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fls2sp0b",		0x20000, 0x8bf15d9c, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fls2sp1b",		0x20000, 0xc1a31086, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flss0",		0x20000, 0xc07cc10a, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "fl2obj0",		0x80000, 0x3657dd7a, 0x05 | BRF_GRA },           //  7 Sprites
	{ "fl2obj2",		0x80000, 0x8ac933fd, 0x05 | BRF_GRA },           //  8
	{ "fl2obj4",		0x80000, 0xe7b989e6, 0x05 | BRF_GRA },           //  9
	{ "fl2obj6",		0x80000, 0x4936583d, 0x05 | BRF_GRA },           // 10
	{ "fl2obj1",		0x80000, 0x3cebf419, 0x05 | BRF_GRA },           // 11
	{ "fl2obj3",		0x80000, 0x0959ed55, 0x05 | BRF_GRA },           // 12
	{ "fl2obj5",		0x80000, 0xd74ae0d3, 0x05 | BRF_GRA },           // 13
	{ "fl2obj7",		0x80000, 0x5ca68c93, 0x05 | BRF_GRA },           // 14

	{ "fls2chr0",		0x40000, 0x7bbda499, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fls2chr1",		0x40000, 0xac8940e5, 0x16 | BRF_GRA },           // 16
	{ "fls2chr2",		0x40000, 0x1756173d, 0x16 | BRF_GRA },           // 17
	{ "fls2chr3",		0x40000, 0x69032785, 0x16 | BRF_GRA },           // 18
	{ "fls2chr4",		0x40000, 0x8216cf42, 0x16 | BRF_GRA },           // 19
	{ "fls2chr5",		0x40000, 0xdc3e8e1c, 0x16 | BRF_GRA },           // 20
	{ "fls2chr6",		0x40000, 0x1ef4bdde, 0x16 | BRF_GRA },           // 21
	{ "fls2chr7",		0x40000, 0x53dafcde, 0x16 | BRF_GRA },           // 22

	{ "fls2sha",		0x40000, 0xf7b40a85, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "fls2dat0",		0x40000, 0xf1af432c, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "fls2dat1",		0x40000, 0x8719533e, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flsvoi1",		0x80000, 0x590be52f, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flsvoi2",		0x80000, 0x204b3c27, 0x0a | BRF_SND },           // 28

	{ "finalap2.nv",	0x02000, 0xc7ae5d0a, 0x0b | BRF_PRG | BRF_ESS},  // 29 nvram
};

STD_ROM_PICK(finalap2)
STD_ROM_FN(finalap2)

struct BurnDriverD BurnDrvFinalap2 = {
	"finalap2", NULL, NULL, NULL, "1990",
	"Final Lap 2\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap2RomInfo, finalap2RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinalapInputInfo, FinallapDIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 2 (Japan)

static struct BurnRomInfo finalap2jRomDesc[] = {
	{ "fls1_mp0.bin",	0x20000, 0x05ea8090, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fls1_mp1.bin",	0x20000, 0xfb189f50, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fls2sp0b",		0x20000, 0x8bf15d9c, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fls2sp1b",		0x20000, 0xc1a31086, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flss0",		0x20000, 0xc07cc10a, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "fl2obj0",		0x80000, 0x3657dd7a, 0x05 | BRF_GRA },           //  7 Sprites
	{ "fl2obj2",		0x80000, 0x8ac933fd, 0x05 | BRF_GRA },           //  8
	{ "fl2obj4",		0x80000, 0xe7b989e6, 0x05 | BRF_GRA },           //  9
	{ "fl2obj6",		0x80000, 0x4936583d, 0x05 | BRF_GRA },           // 10
	{ "fl2obj1",		0x80000, 0x3cebf419, 0x05 | BRF_GRA },           // 11
	{ "fl2obj3",		0x80000, 0x0959ed55, 0x05 | BRF_GRA },           // 12
	{ "fl2obj5",		0x80000, 0xd74ae0d3, 0x05 | BRF_GRA },           // 13
	{ "fl2obj7",		0x80000, 0x5ca68c93, 0x05 | BRF_GRA },           // 14

	{ "fls2chr0",		0x40000, 0x7bbda499, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fls2chr1",		0x40000, 0xac8940e5, 0x16 | BRF_GRA },           // 16
	{ "fls2chr2",		0x40000, 0x1756173d, 0x16 | BRF_GRA },           // 17
	{ "fls2chr3",		0x40000, 0x69032785, 0x16 | BRF_GRA },           // 18
	{ "fls2chr4",		0x40000, 0x8216cf42, 0x16 | BRF_GRA },           // 19
	{ "fls2chr5",		0x40000, 0xdc3e8e1c, 0x16 | BRF_GRA },           // 20
	{ "fls2chr6",		0x40000, 0x1ef4bdde, 0x16 | BRF_GRA },           // 21
	{ "fls2chr7",		0x40000, 0x53dafcde, 0x16 | BRF_GRA },           // 22

	{ "fls2sha",		0x40000, 0xf7b40a85, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "fls2dat0",		0x40000, 0xf1af432c, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "fls2dat1",		0x40000, 0x8719533e, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flsvoi1",		0x80000, 0x590be52f, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flsvoi2",		0x80000, 0x204b3c27, 0x0a | BRF_SND },           // 28

	{ "finalap2.nv",	0x02000, 0xc7ae5d0a, 0x0b | BRF_PRG | BRF_ESS},  // 29 nvram
};

STD_ROM_PICK(finalap2j)
STD_ROM_FN(finalap2j)

struct BurnDriverD BurnDrvFinalap2j = {
	"finalap2j", "finalap2", NULL, NULL, "1990",
	"Final Lap 2 (Japan)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap2jRomInfo, finalap2jRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinalapInputInfo, FinallapDIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 3 (World, set 1)

static struct BurnRomInfo finalap3RomDesc[] = {
	{ "flt2_mpr0c.11d",	0x20000, 0x9ff361ff, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "flt2_mpr1c.13d",	0x20000, 0x17efb7f2, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "flt1_sp0.11k",	0x20000, 0xe804ced1, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "flt1_sp1.13k",	0x20000, 0x3a2b24ee, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flt1_snd0.7j",	0x20000, 0x60b72aed, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "flt_obj-0.4c",	0x80000, 0xeab19ec6, 0x05 | BRF_GRA },           //  7 Sprites
	{ "flt_obj-2.4a",	0x80000, 0x2a3b7ded, 0x05 | BRF_GRA },           //  8
	{ "flt_obj-4.8c",	0x80000, 0x84aa500c, 0x05 | BRF_GRA },           //  9
	{ "flt_obj-6.8a",	0x80000, 0x33118e63, 0x05 | BRF_GRA },           // 10
	{ "flt_obj-1.2c",	0x80000, 0x4ef37a51, 0x05 | BRF_GRA },           // 11
	{ "flt_obj-3.2a",	0x80000, 0xb86dc7cd, 0x05 | BRF_GRA },           // 12
	{ "flt_obj-5.5c",	0x80000, 0x6a53e603, 0x05 | BRF_GRA },           // 13
	{ "flt_obj-7.6a",	0x80000, 0xb52a85e2, 0x05 | BRF_GRA },           // 14

	{ "flt2_chr-0.11n",	0x40000, 0x5954f270, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fltchr-1.11p",	0x40000, 0x2e68d13c, 0x16 | BRF_GRA },           // 16
	{ "flt2_chr-2.11r",	0x40000, 0x98f3b190, 0x16 | BRF_GRA },           // 17
	{ "fltchr-3.11s",	0x40000, 0xe9b05a1f, 0x16 | BRF_GRA },           // 18
	{ "fltchr-4.9n",	0x40000, 0x5ae43767, 0x16 | BRF_GRA },           // 19
	{ "fltchr-5.9p",	0x40000, 0xb5f4e780, 0x16 | BRF_GRA },           // 20
	{ "fltchr-6.9r",	0x40000, 0x4b0baea2, 0x16 | BRF_GRA },           // 21
	{ "fltchr-7.9s",	0x40000, 0x85db9e94, 0x16 | BRF_GRA },           // 22

	{ "flt2_sha.7n",	0x40000, 0x6986565b, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "flt1_d0.13s",	0x20000, 0x80004966, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "flt1_d1.13p",	0x20000, 0xa2e93e8c, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1_3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flt_voi-1.3m",	0x80000, 0x4fc7c0ba, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flt_voi-2.3l",	0x80000, 0x409c62df, 0x0a | BRF_SND },           // 28

	{ "04544191.6r",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 29 Sprite zoom (Unused)

	{ "finalap3.nv",	0x02000, 0xefbc6274, 0x0b | BRF_PRG | BRF_ESS }, // 30 nvram
};

STD_ROM_PICK(finalap3)
STD_ROM_FN(finalap3)

struct BurnDriverD BurnDrvFinalap3 = {
	"finalap3", NULL, NULL, NULL, "1992",
	"Final Lap 3 (World, set 1)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap3RomInfo, finalap3RomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FinalapInputInfo, FinallapDIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 3 (World, set 2)

static struct BurnRomInfo finalap3aRomDesc[] = {
	{ "flt2-mp0.11d",	0x20000, 0x22082168, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "flt2-mp1.13d",	0x20000, 0x2ec21977, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "flt1_sp0.11k",	0x20000, 0xe804ced1, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "flt1_sp1.13k",	0x20000, 0x3a2b24ee, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flt1_snd0.7j",	0x20000, 0x60b72aed, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "flt_obj-0.4c",	0x80000, 0xeab19ec6, 0x05 | BRF_GRA },           //  7 Sprites
	{ "flt_obj-2.4a",	0x80000, 0x2a3b7ded, 0x05 | BRF_GRA },           //  8
	{ "flt_obj-4.8c",	0x80000, 0x84aa500c, 0x05 | BRF_GRA },           //  9
	{ "flt_obj-6.8a",	0x80000, 0x33118e63, 0x05 | BRF_GRA },           // 10
	{ "flt_obj-1.2c",	0x80000, 0x4ef37a51, 0x05 | BRF_GRA },           // 11
	{ "flt_obj-3.2a",	0x80000, 0xb86dc7cd, 0x05 | BRF_GRA },           // 12
	{ "flt_obj-5.5c",	0x80000, 0x6a53e603, 0x05 | BRF_GRA },           // 13
	{ "flt_obj-7.6a",	0x80000, 0xb52a85e2, 0x05 | BRF_GRA },           // 14

	{ "flt2_chr-0.11n",	0x40000, 0x5954f270, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fltchr-1.11p",	0x40000, 0x2e68d13c, 0x16 | BRF_GRA },           // 16
	{ "flt2_chr-2.11r",	0x40000, 0x98f3b190, 0x16 | BRF_GRA },           // 17
	{ "fltchr-3.11s",	0x40000, 0xe9b05a1f, 0x16 | BRF_GRA },           // 18
	{ "fltchr-4.9n",	0x40000, 0x5ae43767, 0x16 | BRF_GRA },           // 19
	{ "fltchr-5.9p",	0x40000, 0xb5f4e780, 0x16 | BRF_GRA },           // 20
	{ "fltchr-6.9r",	0x40000, 0x4b0baea2, 0x16 | BRF_GRA },           // 21
	{ "fltchr-7.9s",	0x40000, 0x85db9e94, 0x16 | BRF_GRA },           // 22

	{ "flt2_sha.7n",	0x40000, 0x6986565b, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "flt1_d0.13s",	0x20000, 0x80004966, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "flt1_d1.13p",	0x20000, 0xa2e93e8c, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1_3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flt_voi-1.3m",	0x80000, 0x4fc7c0ba, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flt_voi-2.3l",	0x80000, 0x409c62df, 0x0a | BRF_SND },           // 28

	{ "04544191.6r",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 29 Sprite zoom (Unused)

	{ "341.bin",		0x20000, 0x8c90ca97, 0x00 | BRF_PRG | BRF_OPT }, // 30 Unknown

	{ "finalap3.nv",	0x02000, 0xefbc6274, 0x0b | BRF_GRA },           // 31 Default NV RAM
};

STD_ROM_PICK(finalap3a)
STD_ROM_FN(finalap3a)

struct BurnDriverD BurnDrvFinalap3a = {
	"finalap3a", "finalap3", NULL, NULL, "1992",
	"Final Lap 3 (World, set 2)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap3aRomInfo, finalap3aRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //Finalap3InputInfo, Finalap3DIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 3 (Japan)

static struct BurnRomInfo finalap3jRomDesc[] = {
	{ "flt_mp0.11d",	0x20000, 0x2f2a997a, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "flt_mp1.13d",	0x20000, 0xb505ca0b, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "flt1_sp0.11k",	0x20000, 0xe804ced1, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "flt1_sp1.13k",	0x20000, 0x3a2b24ee, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flt1_snd0.7j",	0x20000, 0x60b72aed, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "flt_obj-0.4c",	0x80000, 0xeab19ec6, 0x05 | BRF_GRA },           //  7 Sprites
	{ "flt_obj-2.4a",	0x80000, 0x2a3b7ded, 0x05 | BRF_GRA },           //  8
	{ "flt_obj-4.8c",	0x80000, 0x84aa500c, 0x05 | BRF_GRA },           //  9
	{ "flt_obj-6.8a",	0x80000, 0x33118e63, 0x05 | BRF_GRA },           // 10
	{ "flt_obj-1.2c",	0x80000, 0x4ef37a51, 0x05 | BRF_GRA },           // 11
	{ "flt_obj-3.2a",	0x80000, 0xb86dc7cd, 0x05 | BRF_GRA },           // 12
	{ "flt_obj-5.5c",	0x80000, 0x6a53e603, 0x05 | BRF_GRA },           // 13
	{ "flt_obj-7.6a",	0x80000, 0xb52a85e2, 0x05 | BRF_GRA },           // 14

	{ "fltchr-0.11n",	0x40000, 0x97ed5b62, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fltchr-1.11p",	0x40000, 0x2e68d13c, 0x16 | BRF_GRA },           // 16
	{ "fltchr-2.11r",	0x40000, 0x43c3abf8, 0x16 | BRF_GRA },           // 17
	{ "fltchr-3.11s",	0x40000, 0xe9b05a1f, 0x16 | BRF_GRA },           // 18
	{ "fltchr-4.9n",	0x40000, 0x5ae43767, 0x16 | BRF_GRA },           // 19
	{ "fltchr-5.9p",	0x40000, 0xb5f4e780, 0x16 | BRF_GRA },           // 20
	{ "fltchr-6.9r",	0x40000, 0x4b0baea2, 0x16 | BRF_GRA },           // 21
	{ "fltchr-7.9s",	0x40000, 0x85db9e94, 0x16 | BRF_GRA },           // 22

	{ "flt_sha.7n",		0x40000, 0x211bbd83, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "flt1_d0.13s",	0x20000, 0x80004966, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "flt1_d1.13p",	0x20000, 0xa2e93e8c, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1_3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flt_voi-1.3m",	0x80000, 0x4fc7c0ba, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flt_voi-2.3l",	0x80000, 0x409c62df, 0x0a | BRF_SND },           // 28

	{ "04544191.6r",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 29 Sprite zoom (Unused)

	{ "finalap3.nv",	0x02000, 0xefbc6274, 0x0b | BRF_PRG | BRF_ESS }, // 30 nDefault NV RAM
};

STD_ROM_PICK(finalap3j)
STD_ROM_FN(finalap3j)

struct BurnDriverD BurnDrvFinalap3j = {
	"finalap3j", "finalap3", NULL, NULL, "1992",
	"Final Lap 3 (Japan)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap3jRomInfo, finalap3jRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //Finalap3InputInfo, Finalap3DIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 3 (Japan - Rev C)

static struct BurnRomInfo finalap3jcRomDesc[] = {
	{ "flt1_mp0c.11d",	0x20000, 0xebe1bff8, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "flt1_mp1c.13d",	0x20000, 0x61099bb8, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "flt1_sp0.11k",	0x20000, 0xe804ced1, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "flt1_sp1.13k",	0x20000, 0x3a2b24ee, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flt1_snd0.7j",	0x20000, 0x60b72aed, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "c68.3d",		0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c68.3f",		0x08000, 0xca64550a, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "flt_obj-0.4c",	0x80000, 0xeab19ec6, 0x05 | BRF_GRA },           //  7 Sprites
	{ "flt_obj-2.4a",	0x80000, 0x2a3b7ded, 0x05 | BRF_GRA },           //  8
	{ "flt_obj-4.8c",	0x80000, 0x84aa500c, 0x05 | BRF_GRA },           //  9
	{ "flt_obj-6.8a",	0x80000, 0x33118e63, 0x05 | BRF_GRA },           // 10
	{ "flt_obj-1.2c",	0x80000, 0x4ef37a51, 0x05 | BRF_GRA },           // 11
	{ "flt_obj-3.2a",	0x80000, 0xb86dc7cd, 0x05 | BRF_GRA },           // 12
	{ "flt_obj-5.5c",	0x80000, 0x6a53e603, 0x05 | BRF_GRA },           // 13
	{ "flt_obj-7.6a",	0x80000, 0xb52a85e2, 0x05 | BRF_GRA },           // 14

	{ "flt_chr-0.11n",	0x40000, 0x97ed5b62, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "flt_chr-1.11p",	0x40000, 0x2e68d13c, 0x16 | BRF_GRA },           // 16
	{ "flt_chr-2.11r",	0x40000, 0x43c3abf8, 0x16 | BRF_GRA },           // 17
	{ "flt_chr-3.11s",	0x40000, 0xe9b05a1f, 0x16 | BRF_GRA },           // 18
	{ "flt_chr-4.9n",	0x40000, 0x5ae43767, 0x16 | BRF_GRA },           // 19
	{ "flt_chr-5.9p",	0x40000, 0xb5f4e780, 0x16 | BRF_GRA },           // 20
	{ "flt_chr-6.9r",	0x40000, 0x4b0baea2, 0x16 | BRF_GRA },           // 21
	{ "flt_chr-7.9s",	0x40000, 0x85db9e94, 0x16 | BRF_GRA },           // 22

	{ "flt_sha.7n",		0x40000, 0x211bbd83, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "flt1_d0.13s",	0x20000, 0x80004966, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "flt1_d1.13p",	0x20000, 0xa2e93e8c, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1_3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "flt_voi-1.3m",	0x80000, 0x4fc7c0ba, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "flt_voi-2.3l",	0x80000, 0x409c62df, 0x0a | BRF_SND },           // 28

	{ "04544191.6r",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 29 Zoom Look-up (Unused)

	{ "finalap3.nv",	0x02000, 0xefbc6274, 0x0b | BRF_PRG | BRF_ESS }, // 30 Default NV RAM
};

STD_ROM_PICK(finalap3jc)
STD_ROM_FN(finalap3jc)

struct BurnDriverD BurnDrvFinalap3jc = {
	"finalap3jc", "finalap3", NULL, NULL, "1992",
	"Final Lap 3 (Japan - Rev C)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap3jcRomInfo, finalap3jcRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //Finalap3InputInfo, Finalap3DIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Final Lap 3 (bootleg)

static struct BurnRomInfo finalap3blRomDesc[] = {
	{ "fl3-mp0.11d",	0x20000, 0xaf11f52e, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fl3-mp1.13d",	0x20000, 0xda9b1b48, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "flt1sp0",		0x20000, 0xe804ced1, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "flt1sp1",		0x20000, 0x3a2b24ee, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "flt1_snd0.7j",	0x20000, 0x60b72aed, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6
#endif

	{ "fltobj0",		0x80000, 0xeab19ec6, 0x05 | BRF_GRA },           //  7 Sprites
	{ "fltobj2",		0x80000, 0x2a3b7ded, 0x05 | BRF_GRA },           //  8
	{ "fltobj4",		0x80000, 0x84aa500c, 0x05 | BRF_GRA },           //  9
	{ "fltobj6",		0x80000, 0x33118e63, 0x05 | BRF_GRA },           // 10
	{ "fltobj1",		0x80000, 0x4ef37a51, 0x05 | BRF_GRA },           // 11
	{ "fltobj3",		0x80000, 0xb86dc7cd, 0x05 | BRF_GRA },           // 12
	{ "fltobj5",		0x80000, 0x6a53e603, 0x05 | BRF_GRA },           // 13
	{ "fltobj7",		0x80000, 0xb52a85e2, 0x05 | BRF_GRA },           // 14

	{ "flt2_chr-0.bin",	0x40000, 0x5954f270, 0x16 | BRF_GRA },           // 15 Layer Tiles
	{ "fltchr-1.11p",	0x40000, 0x2e68d13c, 0x16 | BRF_GRA },           // 16
	{ "flt2_chr-2.bin",	0x40000, 0x98f3b190, 0x16 | BRF_GRA },           // 17
	{ "fltchr-3.11s",	0x40000, 0xe9b05a1f, 0x16 | BRF_GRA },           // 18
	{ "fltchr-4.9n",	0x40000, 0x5ae43767, 0x16 | BRF_GRA },           // 19
	{ "fltchr-5.9p",	0x40000, 0xb5f4e780, 0x16 | BRF_GRA },           // 20
	{ "fltchr-6.9r",	0x40000, 0x4b0baea2, 0x16 | BRF_GRA },           // 21
	{ "fltchr-7.9s",	0x40000, 0x85db9e94, 0x16 | BRF_GRA },           // 22

	{ "flt2_sha.bin",	0x40000, 0x6986565b, 0x08 | BRF_GRA },           // 23 Layer Tiles Mask Data

	{ "flt1d0",		0x20000, 0x80004966, 0x09 | BRF_PRG | BRF_ESS }, // 24 Shared 68K Data
	{ "flt1d1",		0x20000, 0xa2e93e8c, 0x09 | BRF_PRG | BRF_ESS }, // 25

	{ "fl1-3.5b",		0x00100, 0xd179d99a, 0x0c | BRF_GRA },           // 26 C45 Color Look-up

	{ "fltvoi1",		0x80000, 0x4fc7c0ba, 0x0a | BRF_SND },           // 27 C140 Samples
	{ "fltvoi2",		0x80000, 0x409c62df, 0x0a | BRF_SND },           // 28

	{ "04544191.6r",	0x02000, 0x90db1bf6, 0x00 | BRF_GRA | BRF_OPT }, // 29 Sprite Zoom (Unused)

	{ "finalap3.nv",	0x02000, 0xefbc6274, 0x0b | BRF_PRG | BRF_ESS }, // 30 Default NV RAM
};

STD_ROM_PICK(finalap3bl)
STD_ROM_FN(finalap3bl)

struct BurnDriverD BurnDrvFinalap3bl = {
	"finalap3bl", "finalap3", NULL, NULL, "1992",
	"Final Lap 3 (bootleg)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, finalap3blRomInfo, finalap3blRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //Finalap3InputInfo, Finalap3DIPInfo,
	Finalap2Init, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Four Trax (World)

static struct BurnRomInfo fourtraxRomDesc[] = {
	{ "fx2_mp0.11d",	0x20000, 0xf147cd6b, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fx2_mp1.13d",	0x20000, 0x8af4a309, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fx2_sp0.11k",	0x20000, 0x48548e78, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fx2_sp1.13k",	0x20000, 0xd2861383, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fx1_sd0.7j",		0x20000, 0xacccc934, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "fx_obj-0.4c",	0x40000, 0x1aa60ffa, 0x15 | BRF_GRA },           //  7 Sprites
	{ "fx_obj-1.3c",	0x40000, 0x7509bc09, 0x15 | BRF_GRA },           //  8
	{ "fx_obj-4.4a",	0x40000, 0x30add52a, 0x15 | BRF_GRA },           //  9
	{ "fx_obj-5.3a",	0x40000, 0xe3cd2776, 0x15 | BRF_GRA },           // 10
	{ "fx_obj-8.8c",	0x40000, 0xb165acab, 0x15 | BRF_GRA },           // 11
	{ "fx_obj-9.7c",	0x40000, 0x90f0735b, 0x15 | BRF_GRA },           // 12
	{ "fx_obj-12.8a",	0x40000, 0xf5e23b78, 0x15 | BRF_GRA },           // 13
	{ "fx_obj-13.7a",	0x40000, 0x04a25007, 0x15 | BRF_GRA },           // 14
	{ "fx_obj-2.2c",	0x40000, 0x243affc7, 0x15 | BRF_GRA },           // 15
	{ "fx_obj-3.1c",	0x40000, 0xb7e5d17d, 0x15 | BRF_GRA },           // 16
	{ "fx_obj-6.2a",	0x40000, 0xa2d5ce4a, 0x15 | BRF_GRA },           // 17
	{ "fx_obj-7.1a",	0x40000, 0x4d91c929, 0x15 | BRF_GRA },           // 18
	{ "fx_obj-10.6c",	0x40000, 0x7a01e86f, 0x15 | BRF_GRA },           // 19
	{ "fx_obj-11.5c",	0x40000, 0x514b3fe5, 0x15 | BRF_GRA },           // 20
	{ "fx_obj-14.6a",	0x40000, 0xc1658c77, 0x15 | BRF_GRA },           // 21
	{ "fx_obj-15.5a",	0x40000, 0x2bc909b3, 0x15 | BRF_GRA },           // 22

	{ "fx_chr-0.11n",	0x20000, 0x6658c1c3, 0x06 | BRF_GRA },           // 23 Layer Tiles
	{ "fx_chr-1.11p",	0x20000, 0x3a888943, 0x06 | BRF_GRA },           // 24
	{ "fx2_chr-2.11r",	0x20000, 0xfdf1e86b, 0x06 | BRF_GRA },           // 25
	{ "fx_chr-3.11s",	0x20000, 0x47fa7e61, 0x06 | BRF_GRA },           // 26
	{ "fx_chr-4.9n",	0x20000, 0xc720c5f5, 0x06 | BRF_GRA },           // 27
	{ "fx_chr-5.9p",	0x20000, 0x9eacdbc8, 0x06 | BRF_GRA },           // 28
	{ "fx_chr-6.9r",	0x20000, 0xc3dba42e, 0x06 | BRF_GRA },           // 29
	{ "fx_chr-7.9s",	0x20000, 0xc009f3ae, 0x06 | BRF_GRA },           // 30

	{ "fx_sha.7n",		0x20000, 0xf7aa4af7, 0x08 | BRF_GRA },           // 31 Layer Tiles Mask Data

	{ "fx_dat0.13s",	0x40000, 0x63abf69b, 0x09 | BRF_PRG | BRF_ESS }, // 32 Shared 68K Data
	{ "fx_dat1.13r",	0x40000, 0x725bed14, 0x09 | BRF_PRG | BRF_ESS }, // 33
	{ "fx_dat2.13p",	0x40000, 0x71e4a5a0, 0x09 | BRF_PRG | BRF_ESS }, // 34
	{ "fx_dat3.13n",	0x40000, 0x605725f7, 0x09 | BRF_PRG | BRF_ESS }, // 35

	{ "fx1_1.5b",		0x00100, 0x85ffd753, 0x0c | BRF_GRA },           // 36 C45 Color Look-up

	{ "fx_voi-1.3m",	0x80000, 0x6173364f, 0x0a | BRF_SND },           // 37 C140 Samples
};

STD_ROM_PICK(fourtrax)
STD_ROM_FN(fourtrax)

struct BurnDriverD BurnDrvFourtrax = {
	"fourtrax", NULL, NULL, NULL, "1989",
	"Four Trax (World)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, fourtraxRomInfo, fourtraxRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FourtraxInputInfo, Fourtrax3DIPInfo,
	FourtraxInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};


// Four Trax (Asia)

static struct BurnRomInfo fourtraxaRomDesc[] = {
	{ "fx4_mpr-0a.11d",	0x20000, 0xf147cd6b, 0x01 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "fx4_mpr-1a.13d",	0x20000, 0xd1138c85, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fx1_sp0.11k",	0x20000, 0x48548e78, 0x02 | BRF_PRG | BRF_ESS }, //  2 Sub 68K Code
	{ "fx1_sp1.13k",	0x20000, 0xd2861383, 0x02 | BRF_PRG | BRF_ESS }, //  3

	{ "fx1_sd0.7j",		0x20000, 0xacccc934, 0x03 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

#if !defined ROM_VERIFY
	{ "sys2mcpu.bin",	0x02000, 0xa342a97e, 0x04 | BRF_PRG | BRF_ESS }, //  5 HD68705 Code
#endif
	{ "sys2c65c.bin",	0x08000, 0xa5b2a4ff, 0x04 | BRF_PRG | BRF_ESS }, //  6

	{ "fx_obj-0.4c",	0x40000, 0x1aa60ffa, 0x15 | BRF_GRA },           //  7 Sprites
	{ "fx_obj-1.3c",	0x40000, 0x7509bc09, 0x15 | BRF_GRA },           //  8
	{ "fx_obj-4.4a",	0x40000, 0x30add52a, 0x15 | BRF_GRA },           //  9
	{ "fx_obj-5.3a",	0x40000, 0xe3cd2776, 0x15 | BRF_GRA },           // 10
	{ "fx_obj-8.8c",	0x40000, 0xb165acab, 0x15 | BRF_GRA },           // 11
	{ "fx_obj-9.7c",	0x40000, 0x90f0735b, 0x15 | BRF_GRA },           // 12
	{ "fx_obj-12.8a",	0x40000, 0xf5e23b78, 0x15 | BRF_GRA },           // 13
	{ "fx_obj-13.7a",	0x40000, 0x04a25007, 0x15 | BRF_GRA },           // 14
	{ "fx_obj-2.2c",	0x40000, 0x243affc7, 0x15 | BRF_GRA },           // 15
	{ "fx_obj-3.1c",	0x40000, 0xb7e5d17d, 0x15 | BRF_GRA },           // 16
	{ "fx_obj-6.2a",	0x40000, 0xa2d5ce4a, 0x15 | BRF_GRA },           // 17
	{ "fx_obj-7.1a",	0x40000, 0x4d91c929, 0x15 | BRF_GRA },           // 18
	{ "fx_obj-10.6c",	0x40000, 0x7a01e86f, 0x15 | BRF_GRA },           // 19
	{ "fx_obj-11.5c",	0x40000, 0x514b3fe5, 0x15 | BRF_GRA },           // 20
	{ "fx_obj-14.6a",	0x40000, 0xc1658c77, 0x15 | BRF_GRA },           // 21
	{ "fx_obj-15.5a",	0x40000, 0x2bc909b3, 0x15 | BRF_GRA },           // 22

	{ "fx_chr-0.11n",	0x20000, 0x6658c1c3, 0x06 | BRF_GRA },           // 23 Layer Tiles
	{ "fx_chr-1.11p",	0x20000, 0x3a888943, 0x06 | BRF_GRA },           // 24
	{ "fx4_chr-2a.11r",	0x20000, 0xa5d1ab10, 0x06 | BRF_GRA },           // 25
	{ "fx_chr-3.11s",	0x20000, 0x47fa7e61, 0x06 | BRF_GRA },           // 26
	{ "fx_chr-4.9n",	0x20000, 0xc720c5f5, 0x06 | BRF_GRA },           // 27
	{ "fx_chr-5.9p",	0x20000, 0x9eacdbc8, 0x06 | BRF_GRA },           // 28
	{ "fx_chr-6.9r",	0x20000, 0xc3dba42e, 0x06 | BRF_GRA },           // 29
	{ "fx_chr-7.9s",	0x20000, 0xc009f3ae, 0x06 | BRF_GRA },           // 30

	{ "fx_sha.7n",		0x20000, 0xf7aa4af7, 0x08 | BRF_GRA },           // 31 Layer Tiles Mask Data

	{ "fx_dat0.13s",	0x40000, 0x63abf69b, 0x09 | BRF_PRG | BRF_ESS }, // 32 Shared 68K Data
	{ "fx_dat1.13r",	0x40000, 0x725bed14, 0x09 | BRF_PRG | BRF_ESS }, // 33
	{ "fx_dat2.13p",	0x40000, 0x71e4a5a0, 0x09 | BRF_PRG | BRF_ESS }, // 34
	{ "fx_dat3.13n",	0x40000, 0x605725f7, 0x09 | BRF_PRG | BRF_ESS }, // 35

	{ "fx1_1.5b",		0x00100, 0x85ffd753, 0x0c | BRF_GRA },           // 36 C45 Color Look-up

	{ "fx_voi-1.3m",	0x80000, 0x6173364f, 0x0a | BRF_SND },           // 37 C140 Samples
};

STD_ROM_PICK(fourtraxa)
STD_ROM_FN(fourtraxa)

struct BurnDriverD BurnDrvFourtraxa = {
	"fourtraxa", "fourtrax", NULL, NULL, "1989",
	"Four Trax (Asia)\0", "Imperfect graphics, sound and inputs", "Namco", "System 2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, fourtraxaRomInfo, fourtraxaRomName, NULL, NULL, NULL, NULL, DefaultInputInfo, DefaultDIPInfo, //FourtraxInputInfo, Fourtrax3DIPInfo,
	FourtraxInit, Namcos2Exit, DrvFrame, FinallapDraw, DrvScan, &DrvRecalc, 0x4000,
	288, 224, 4, 3
};
