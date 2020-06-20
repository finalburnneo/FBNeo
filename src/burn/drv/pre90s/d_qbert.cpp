// Q-Bert emu-layer for FB Alpha by dink, based on the MAME driver by Fabrice Frances & Nicola Salmoria.

#include "tiles_generic.h"
#include "driver.h"
#include "nec_intf.h"
#include "m6502_intf.h"
#include "burn_gun.h" // trackball
#include "bitswap.h"
#include "dac.h"
#include "samples.h"
#include "ay8910.h"
#include "sp0250.h"
#include "timer.h"

//#define QBERT_SOUND_DEBUG

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvV20ROM;
static UINT8 *Drv6502ROM;
static UINT8 *Drv6502ROM1;
static UINT8 *DrvV20RAM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvPaletteRAM;
static UINT8 *DrvCharGFX;
static UINT8 *DrvSpriteGFX;
static UINT8 *DrvNVRAM;
static UINT8 *DrvDummyROM;
static UINT8 *DrvCharExp;

static UINT8 DummyRegion[2];

static UINT8 *riot_regs;
static UINT8 *riot_ram;

static UINT8 *background_prio;
static UINT8 *spritebank;
static UINT8 *soundlatch;
static UINT8 *soundcpu_do_nmi;

static UINT8  *vtqueue;
static UINT8  *vtqueuepos;
static UINT32 *vtqueuetime;
static UINT8  *knocker_prev;

static UINT8 flipscreenx;
static UINT8 flipscreeny;
static UINT8 joystick_select;

static INT32 speech_timer_counter = 0;
static UINT8 nmi_state = 0;
static UINT8 nmi_rate = 0;
static UINT8 psg_latch = 0;
static UINT8 sp0250_latch = 0;
static UINT8 soundlatch2 = 0;
static UINT8 speech_control = 0;
static UINT8 last_command = 0;
static UINT8 dac_data[2] = { 0xff, 0xff };

static INT32 qbert_random;
static INT32 reactor_score;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 tilemap_bank[4] = { 0, 0, 0, 0 };

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvFakeInput[8]; // fake inputs for rotate buttons

static UINT8 DrvDip[2] = { 0, 0  };
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static INT16 Analog[2];
static INT16 analog_last[2];

static UINT8 game_type = 0; // 0 = qbert, 6 = qbertcub, 4 = mplanets, 7 = curvebal, 8 = insector, 9 - argusg/krull, 10 - reactor
static INT32 type2_sound = 0;
static INT32 has_tball = 0; // wizwars, reactor, argusg

static UINT32 nRotateTime[2]  = { 0, 0 };

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo QbertInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"   },
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"  },
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"     },
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"   },
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"  },

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"   },
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"  },
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"     },
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"   },
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"   },
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"  },

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"     },
	{"Select",			BIT_DIGITAL,	DrvJoy1 + 7,    "select"    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Qbert)

static struct BurnInputInfo MplanetsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"   },
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"  },
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"     },
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"   },
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"  },
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1" },
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2" },
	{"Rotate Left", 	BIT_DIGITAL,    DrvFakeInput + 0, "p1 rotate left" },
	{"Rotate Right",	BIT_DIGITAL,    DrvFakeInput + 1, "p1 rotate right" },

	{"Reset",			BIT_DIGITAL,	&DrvReset,      "reset"     },
	{"Select",			BIT_DIGITAL,	DrvJoy1 + 6,    "select"    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Mplanets)

static struct BurnInputInfo QbertqubInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"   },
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"  },
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"     },
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"   },
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"   },
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"  },

	{"Reset",			BIT_DIGITAL,	&DrvReset,      "reset"     },
	{"Select",			BIT_DIGITAL,	DrvJoy1 + 7,    "select"    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Qbertqub)

static struct BurnInputInfo InsectorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Insector)

static struct BurnInputInfo ArgusgInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Select",			BIT_DIGITAL,	DrvJoy1 + 1,    "select"    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Argusg)

static struct BurnInputInfo KrullInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"Start 1",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"Start 2",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"		},
	{"Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"Left Stick Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},
	{"Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"Right Stick Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Krull)

static struct BurnInputInfo CurvebalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Curvebal)

static struct BurnInputInfo TylzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Tylz)

static struct BurnInputInfo KngtmareInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"Start 1",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"Start 2",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"Left Stick Right",BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"Right Stick Left",BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Kngtmare)

static struct BurnInputInfo ScrewlooInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"Left Stick Right",BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"Right Stick Down",BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"Right Stick Left",BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Screwloo)

static struct BurnInputInfo WizwarzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[1],		"p1 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Wizwarz)

static struct BurnInputInfo VidvinceInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Vidvince)

static struct BurnInputInfo Stooges3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy5 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy5 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy5 + 1,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Stooges3)

static struct BurnInputInfo ReactorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 2"	},

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Reactor)

static struct BurnDIPInfo QbertDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x02, NULL		        		},
	{0x0f, 0xff, 0xff, 0x40, NULL	                	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0e, 0x01, 0x01, 0x01, "Off"		        		},
	{0x0e, 0x01, 0x01, 0x00, "On"	                	},

	{0   , 0xfe, 0   ,    2, "Kicker"					},
	{0x0e, 0x01, 0x02, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x02, 0x02, "On"		        		},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0e, 0x01, 0x04, 0x00, "Upright"					},
	{0x0e, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Auto Round Advance (Cheat)" },
	{0x0e, 0x01, 0x08, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x08, 0x08, "On"		        		},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x0e, 0x01, 0x10, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x10, 0x10, "On"		        		},

	{0   , 0xfe, 0   ,    2, "SW5"		        		},
	{0x0e, 0x01, 0x20, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x20, 0x20, "On"		        		},

	{0   , 0xfe, 0   ,    2, "SW7"		        		},
	{0x0e, 0x01, 0x40, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x40, 0x40, "On"		        		},

	{0   , 0xfe, 0   ,    2, "SW8"		        		},
	{0x0e, 0x01, 0x80, 0x00, "Off"		        		},
	{0x0e, 0x01, 0x80, 0x80, "On"		        		},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x0f, 0x01, 0x40, 0x40, "Off"		        		},
	{0x0f, 0x01, 0x40, 0x00, "On"		        		},
};

STDDIPINFO(Qbert)

static struct BurnDIPInfo MplanetsDIPList[]=
{
	{0x0C, 0xff, 0xff, 0x00, NULL		        		},
	{0x0D, 0xff, 0xff, 0x80, NULL	                	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0C, 0x01, 0x01, 0x01, "Off"		        		},
	{0x0C, 0x01, 0x01, 0x00, "On"		        		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x0C, 0x01, 0x02, 0x00, "10000"					},
	{0x0C, 0x01, 0x02, 0x02, "12000"					},

	{0   , 0xfe, 0   ,    2, "Allow Round Select"   	},
	{0x0C, 0x01, 0x08, 0x00, "No"		        		},
	{0x0C, 0x01, 0x08, 0x08, "Yes"		        		},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x0C, 0x01, 0x14, 0x04, "2 Coins 1 Credit"			},
	{0x0C, 0x01, 0x14, 0x00, "1 Coin  1 Credit"			},
	{0x0C, 0x01, 0x14, 0x10, "1 Coin  2 Credits"    	},
	{0x0C, 0x01, 0x14, 0x14, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x0C, 0x01, 0x20, 0x00, "3"		        		},
	{0x0C, 0x01, 0x20, 0x20, "5"		        		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0C, 0x01, 0xc0, 0x40, "Easy"		        		},
	{0x0C, 0x01, 0xc0, 0x00, "Medium"					},
	{0x0C, 0x01, 0xc0, 0x80, "Hard"		        		},
	{0x0C, 0x01, 0xc0, 0xc0, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x0D, 0x01, 0x80, 0x80, "Off"		        		},
	{0x0D, 0x01, 0x80, 0x00, "On"		        		},
};

STDDIPINFO(Mplanets)

static struct BurnDIPInfo QbertqubDIPList[]=
{
	{0x08, 0xff, 0xff, 0x00, NULL		        		},
	{0x09, 0xff, 0xff, 0x40, NULL	                	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x08, 0x01, 0x08, 0x08, "Off"		        		},
	{0x08, 0x01, 0x08, 0x00, "On"		        		},

	{0   , 0xfe, 0   ,    13, "Coinage"					},
	{0x08, 0x01, 0x35, 0x24, "A 2/1 B 2/1"				},
	{0x08, 0x01, 0x35, 0x14, "A 1/1 B 4/1"				},
	{0x08, 0x01, 0x35, 0x30, "A 1/1 B 3/1"				},
	{0x08, 0x01, 0x35, 0x10, "A 1/1 B 2/1"				},
	{0x08, 0x01, 0x35, 0x00, "A 1/1 B 1/1"				},
	{0x08, 0x01, 0x35, 0x11, "A 2/3 B 2/1"				},
	{0x08, 0x01, 0x35, 0x15, "A 1/2 B 3/1"				},
	{0x08, 0x01, 0x35, 0x20, "A 1/2 B 2/1"				},
	{0x08, 0x01, 0x35, 0x21, "A 1/2 B 1/1"				},
	{0x08, 0x01, 0x35, 0x31, "A 1/2 B 1/5"				},
	{0x08, 0x01, 0x35, 0x04, "A 1/3 B 2/1"				},
	{0x08, 0x01, 0x35, 0x05, "A 1/3 B 1/1"				},
	{0x08, 0x01, 0x35, 0x35, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "1st Bonus Life"			},
	{0x08, 0x01, 0x02, 0x00, "10000"					},
	{0x08, 0x01, 0x02, 0x02, "15000"					},

	{0   , 0xfe, 0   ,    2, "Additional Bonus Life"	},
	{0x08, 0x01, 0x40, 0x00, "20000"					},
	{0x08, 0x01, 0x40, 0x40, "25000"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x08, 0x01, 0x80, 0x00, "Normal"					},
	{0x08, 0x01, 0x80, 0x80, "Hard"		        		},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x09, 0x01, 0x40, 0x40, "Off"		        		},
	{0x09, 0x01, 0x40, 0x00, "On"		        		},

};

STDDIPINFO(Qbertqub)

static struct BurnDIPInfo InsectorDIPList[]=
{
	{0x10, 0xff, 0xff, 0x08, NULL						},
	{0x11, 0xff, 0xff, 0x40, NULL						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x10, 0x01, 0x01, 0x00, "25k 75k and every 50k"	},
	{0x10, 0x01, 0x01, 0x01, "30k 90k and every 60k"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x10, 0x01, 0x02, 0x02, "Off"						},
	{0x10, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Mode (Unlim Lives, Start 2=Adv. (Cheat)"	},
	{0x10, 0x01, 0x04, 0x00, "Off"						},
	{0x10, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x10, 0x01, 0x08, 0x08, "3"						},
	{0x10, 0x01, 0x08, 0x00, "5"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x10, 0x01, 0x50, 0x40, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x50, 0x50, "2 Coins 2 Credits"		},
	{0x10, 0x01, 0x50, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x50, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x10, 0x01, 0x20, 0x00, "Off"						},
	{0x10, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x10, 0x01, 0x80, 0x00, "Upright"					},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x11, 0x01, 0x40, 0x40, "Off"		        		},
	{0x11, 0x01, 0x40, 0x00, "On"		        		},
};

STDDIPINFO(Insector)

static struct BurnDIPInfo ArgusgDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x12, NULL						},
	{0x0b, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0a, 0x01, 0x01, 0x01, "Off"						},
	{0x0a, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Bonus Human Every"		},
	{0x0a, 0x01, 0x22, 0x00, "15000"					},
	{0x0a, 0x01, 0x22, 0x02, "20000"					},
	{0x0a, 0x01, 0x22, 0x20, "25000"					},
	{0x0a, 0x01, 0x22, 0x22, "30000"					},

	{0   , 0xfe, 0   ,    4, "Initial Humans"			},
	{0x0a, 0x01, 0x14, 0x00, "4"						},
	{0x0a, 0x01, 0x14, 0x10, "6"						},
	{0x0a, 0x01, 0x14, 0x04, "8"						},
	{0x0a, 0x01, 0x14, 0x14, "10"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x0a, 0x01, 0x08, 0x00, "Off"						},
	{0x0a, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x0a, 0x01, 0x40, 0x00, "Normal"					},
	{0x0a, 0x01, 0x40, 0x40, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x0b, 0x01, 0x01, 0x01, "Off"		        		},
	{0x0b, 0x01, 0x01, 0x00, "On"		        		},
};

STDDIPINFO(Argusg)

static struct BurnDIPInfo KrullDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL						},
	{0x0e, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0d, 0x01, 0x01, 0x01, "Off"						},
	{0x0d, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x0d, 0x01, 0x02, 0x00, "Normal"					},
	{0x0d, 0x01, 0x02, 0x02, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x0d, 0x01, 0x08, 0x00, "3"						},
	{0x0d, 0x01, 0x08, 0x08, "5"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x0d, 0x01, 0x14, 0x04, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0x14, 0x00, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0x14, 0x10, "1 Coin  2 Credits"		},
	{0x0d, 0x01, 0x14, 0x14, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Hexagon"					},
	{0x0d, 0x01, 0x20, 0x00, "Roving"					},
	{0x0d, 0x01, 0x20, 0x20, "Stationary"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x0d, 0x01, 0xc0, 0x40, "30k 60k and every 30k"	},
	{0x0d, 0x01, 0xc0, 0x00, "30k 80k and every 50k"	},
	{0x0d, 0x01, 0xc0, 0x80, "40k 90k and every 50k"	},
	{0x0d, 0x01, 0xc0, 0xc0, "50k 125k and every 75k"	},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x0e, 0x01, 0x01, 0x01, "Off"		        		},
	{0x0e, 0x01, 0x01, 0x00, "On"		        		},
};

STDDIPINFO(Krull)

static struct BurnDIPInfo CurvebalDIPList[]=
{
	{0x09, 0xff, 0xff, 0x04, NULL						},
	{0x0a, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x04, 0x00, "Off"						},
	{0x09, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "2 Players Game"			},
	{0x09, 0x01, 0x08, 0x08, "1 Credit"					},
	{0x09, 0x01, 0x08, 0x00, "2 Credits"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x11, 0x00, "Easy"						},
	{0x09, 0x01, 0x11, 0x10, "Medium"					},
	{0x09, 0x01, 0x11, 0x01, "Hard"						},
	{0x09, 0x01, 0x11, 0x11, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Coins"					},
	{0x09, 0x01, 0x20, 0x00, "Normal"					},
	{0x09, 0x01, 0x20, 0x20, "French"					},

	{0   , 0xfe, 0   ,    13, "Coinage"					},
	{0x09, 0x01, 0xc2, 0x42, "A 3/1 B 1/2"				},
	{0x09, 0x01, 0xc2, 0x42, "A 4/1 B 1/1"				},
	{0x09, 0x01, 0xc2, 0x82, "A 1/5 B 1/2"				},
	{0x09, 0x01, 0xc2, 0x82, "A 3/1 B 1/1"				},
	{0x09, 0x01, 0xc2, 0x02, "A 2/1 B 2/3"				},
	{0x09, 0x01, 0xc2, 0x02, "A 2/1 B 1/1"				},
	{0x09, 0x01, 0xc2, 0xc0, "A 2/1 B 2/1"				},
	{0x09, 0x01, 0xc2, 0x80, "A 1/1 B 1/2"				},
	{0x09, 0x01, 0xc2, 0x80, "A 2/1 B 1/2"				},
	{0x09, 0x01, 0xc2, 0x40, "A 1/1 B 1/3"				},
	{0x09, 0x01, 0xc2, 0x40, "A 2/1 B 1/3"				},
	{0x09, 0x01, 0xc2, 0x00, "A 1/1 B 1/1"				},
	{0x09, 0x01, 0xc2, 0xc2, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x0a, 0x01, 0x01, 0x01, "Off"		        		},
	{0x0a, 0x01, 0x01, 0x00, "On"		        		},
};

STDDIPINFO(Curvebal)

static struct BurnDIPInfo TylzDIPList[]=
{
	{0x08, 0xff, 0xff, 0x08, NULL						},
	{0x09, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x08, 0x01, 0x04, 0x00, "3"						},
	{0x08, 0x01, 0x04, 0x04, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x08, 0x01, 0x08, 0x00, "Off"						},
	{0x08, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x08, 0x01, 0x11, 0x01, "2 Coins 1 Credits"		},
	{0x08, 0x01, 0x11, 0x00, "1 Coin  1 Credits"		},
	{0x08, 0x01, 0x11, 0x10, "1 Coin  2 Credits"		},
	{0x08, 0x01, 0x11, 0x11, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x08, 0x01, 0x22, 0x00, "15k 35k and every 20k"	},
	{0x08, 0x01, 0x22, 0x20, "15k 45k and every 30k"	},
	{0x08, 0x01, 0x22, 0x02, "20k 55k and every 35k"	},
	{0x08, 0x01, 0x22, 0x22, "20k 60k and every 40k"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x08, 0x01, 0xc0, 0x00, "Easy"						},
	{0x08, 0x01, 0xc0, 0x40, "Normal Easy"				},
	{0x08, 0x01, 0xc0, 0x80, "Normal Hard"				},
	{0x08, 0x01, 0xc0, 0xc0, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Service"	        		},
	{0x09, 0x01, 0x01, 0x01, "Off"		        		},
	{0x09, 0x01, 0x01, 0x00, "On"		        		},
};

STDDIPINFO(Tylz)

static struct BurnDIPInfo KngtmareDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xff, NULL						},
	{0x0c, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    3, "Coinage"					},
	{0x0b, 0x01, 0x11, 0x10, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x11, 0x11, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x11, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x0b, 0x01, 0x20, 0x00, "3"						},
	{0x0b, 0x01, 0x20, 0x20, "5"						},
};

STDDIPINFO(Kngtmare)

static struct BurnDIPInfo ScrewlooDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x40, NULL						},
	{0x0e, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0d, 0x01, 0x01, 0x01, "Off"						},
	{0x0d, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo mode"				},
	{0x0d, 0x01, 0x02, 0x00, "Off"						},
	{0x0d, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    2, "1st Bonus Atom at"		},
	{0x0d, 0x01, 0x04, 0x00, "5000"						},
	{0x0d, 0x01, 0x04, 0x04, "20000"					},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x0d, 0x01, 0x08, 0x00, "Off"						},
	{0x0d, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x0d, 0x01, 0x50, 0x00, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0x50, 0x10, "2 Coins 2 Credits"		},
	{0x0d, 0x01, 0x50, 0x40, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0x50, 0x50, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "1st Bonus Hand at"		},
	{0x0d, 0x01, 0x20, 0x00, "25000"					},
	{0x0d, 0x01, 0x20, 0x20, "50000"					},

	{0   , 0xfe, 0   ,    2, "Hands"					},
	{0x0d, 0x01, 0x80, 0x00, "3"						},
	{0x0d, 0x01, 0x80, 0x80, "5"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0e, 0x01, 0x01, 0x01, "Off"						},
	{0x0e, 0x01, 0x01, 0x00, "On"						},
};

STDDIPINFO(Screwloo)

static struct BurnDIPInfo WizwarzDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL						},
	{0x0a, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    4, "Bonuses"					},
	{0x09, 0x01, 0x09, 0x00, "Life 20k,50k every 30k / Mine 10k,25k every 15k"	},
	{0x09, 0x01, 0x09, 0x08, "Life 20k,55k every 35k / Mine 10k,30k every 20k"	},
	{0x09, 0x01, 0x09, 0x01, "Life 25k,60k every 35k / Mine 15k,35k every 20k"	},
	{0x09, 0x01, 0x09, 0x09, "Life 30k,40k every 40k / Mine 15k,40k every 25k"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x02, 0x02, "Off"						},
	{0x09, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x09, 0x01, 0x04, 0x00, "Normal"					},
	{0x09, 0x01, 0x04, 0x04, "Hard"						},

	{0   , 0xfe, 0   ,    0, "Lives"					},
	{0x09, 0x01, 0x20, 0x00, "3"						},
	{0x09, 0x01, 0x20, 0x20, "5"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x09, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0xc0, 0xc0, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0e, 0x01, 0x01, 0x01, "Off"						},
	{0x0e, 0x01, 0x01, 0x00, "On"						},
};

STDDIPINFO(Wizwarz)

static struct BurnDIPInfo VidvinceDIPList[]=
{
	{0x09, 0xff, 0xff, 0x45, NULL						},
	{0x0a, 0xff, 0xff, 0xf0, NULL						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x09, 0x01, 0x09, 0x09, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x09, 0x08, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x09, 0x01, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x09, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x09, 0x01, 0x02, 0x00, "3"						},
	{0x09, 0x01, 0x02, 0x02, "5"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x09, 0x01, 0x14, 0x00, "10000"					},
	{0x09, 0x01, 0x14, 0x04, "20000"					},
	{0x09, 0x01, 0x14, 0x10, "30000"					},
	{0x09, 0x01, 0x14, 0x14, "40000"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x09, 0x01, 0x20, 0x20, "Hard"						},
	{0x09, 0x01, 0x20, 0x00, "Normal"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x40, 0x00, "Off"						},
	{0x09, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0a, 0x01, 0x01, 0x00, "Off"						},
	{0x0a, 0x01, 0x91, 0x01, "On"						},
};

STDDIPINFO(Vidvince)

static struct BurnDIPInfo Stooges3DIPList[]=
{
	{0x13, 0xff, 0xff, 0xc0, NULL						},
	{0x14, 0xff, 0xff, 0x11, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x13, 0x01, 0x02, 0x00, "Normal"					},
	{0x13, 0x01, 0x02, 0x02, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x13, 0x01, 0x08, 0x00, "3"						},
	{0x13, 0x01, 0x08, 0x08, "5"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x13, 0x01, 0x14, 0x04, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x14, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x14, 0x10, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x14, 0x14, "Free Play"				},

	{0   , 0xfe, 0   ,    0, "1st Bonus Life at"		},
	{0x13, 0x01, 0xc0, 0xc0, "10k 20k and every 10k"	},
	{0x13, 0x01, 0xc0, 0x00, "20k 40k and every 20k"	},
	{0x13, 0x01, 0xc0, 0x40, "10k 30k and every 20k"	},
	{0x13, 0x01, 0xc0, 0x80, "20k 30k and every 10k"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x01, 0x01, "Off"						},
	{0x14, 0x01, 0x01, 0x00, "On"						},

};

STDDIPINFO(Stooges3)

static struct BurnDIPInfo ReactorDIPList[]=
{
	{0x08, 0xff, 0xff, 0xff, NULL						},
	{0x09, 0xff, 0xff, 0x02, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x08, 0x01, 0x01, 0x00, "Off"						},
	{0x08, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    2, "Bounce Chambers Points"	},
	{0x08, 0x01, 0x02, 0x00, "10"						},
	{0x08, 0x01, 0x02, 0x02, "15"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x08, 0x01, 0x04, 0x04, "Off"						},
	{0x08, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound with Instructions"	},
	{0x08, 0x01, 0x08, 0x00, "Off"						},
	{0x08, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x08, 0x01, 0x10, 0x10, "Upright"					},
	{0x08, 0x01, 0x10, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x08, 0x01, 0x20, 0x00, "2 Coins 1 Credits"		},
	{0x08, 0x01, 0x20, 0x20, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x08, 0x01, 0xc0, 0x00, "10000"					},
	{0x08, 0x01, 0xc0, 0x40, "12000"					},
	{0x08, 0x01, 0xc0, 0xc0, "15000"					},
	{0x08, 0x01, 0xc0, 0x80, "20000"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x09, 0x01, 0x02, 0x02, "Off"						},
	{0x09, 0x01, 0x02, 0x00, "On"						},
};

STDDIPINFO(Reactor)


static UINT8 dialRotation(int addy) // mplanets
{
	if (nRotateTime[addy] > nCurrentFrame) nRotateTime[addy] = 0; // Bugfix: no rotate after savestate

	if (DrvFakeInput[0] && (nCurrentFrame > nRotateTime[addy]+2)) {
		nRotateTime[addy] = nCurrentFrame;
		return 0xfe;
	}

	if (DrvFakeInput[1] && (nCurrentFrame > nRotateTime[addy]+2)) {
		nRotateTime[addy] = nCurrentFrame;
		return 2;
	}

	return 0;
}

static void palette_write(UINT16 offset, UINT8 data)
{
	INT32 bit0, bit1, bit2, bit3;
	INT32 r, g, b, val;

	DrvPaletteRAM[offset] = data;

	val = DrvPaletteRAM[offset | 1];

	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;

	r = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	val = DrvPaletteRAM[offset & ~1];

	bit0 = (val >> 4) & 0x01;
	bit1 = (val >> 5) & 0x01;
	bit2 = (val >> 6) & 0x01;
	bit3 = (val >> 7) & 0x01;

	g = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	val = DrvPaletteRAM[offset & ~1];

	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;

	b = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	DrvPalette[offset / 2] = BurnHighCol(r, g, b, 0);
}

static void qbert_knocker(UINT8 knock)
{
	if (knock & ~*knocker_prev && game_type == 0)
		BurnSamplePlay(44);

	*knocker_prev = knock;
}

static void soundlatch_r1(UINT16 /*offset*/, UINT8 data)
{
	data &= 0x3f;

	if ((data & 0x0f) != 0xf) {
#ifdef QBERT_SOUND_DEBUG
		bprintf(0, _T("data %X.."), data ^ 0x3f);
#endif
		switch (game_type) {
			case 0: { // qbert
				switch (data ^ 0x3f) { // qbert sample player
					case 17:
					case 18:
					case 19:
					case 20:
					case 21:
						BurnSamplePlay(((data ^ 0x3f) - 17) * 8 + qbert_random);
						qbert_random = (qbert_random + 1) & 7;
						break;
					case 22:
						BurnSamplePlay(40);
						break;
					case 23:
						BurnSamplePlay(41);
						break;
					case 28:
						BurnSamplePlay(42); // Hello, I'm turned on.
						break;
					case 36:
						BurnSamplePlay(43); // Bye-Bye
						break;
				}
			}
			break;

			case 10: { // reactor
				switch (data ^ 0x3f) {
					case 31:
						BurnSamplePlay(7);
						reactor_score = 0;
						break;
					case 39:
						if (++reactor_score < 13)
							BurnSamplePlay(reactor_score + 7);
						break;
					case 53:
					case 54:
					case 55:
					case 56:
					case 57:
					case 58:
					case 59:
						BurnSamplePlay((data ^ 0x3f) - 53);
						break;
				}
			}
			break;
		}

		*soundlatch = data;

		M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
}

static void soundlatch_r2(UINT8 data)
{
	if (data != 0xff)
	{
		*soundlatch = data;
		soundlatch2 = data;

		if (last_command == 0xff)
		{
			M6502SetIRQLine(0, 0, CPU_IRQSTATUS_ACK);
			M6502SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		}
	}

	last_command = data;
}


static inline void expand_chars(UINT16 offset)
{
	offset &= 0xfff;

	DrvCharExp[offset * 2 + 1] = DrvCharRAM[offset] & 0xf;
	DrvCharExp[offset * 2 + 0] = DrvCharRAM[offset] >> 4;
}

static void __fastcall main_write(UINT32 address, UINT8 data)
{
	if (address >= 0x10000) {
		cpu_writemem20(address & 0xffff, data);
		return;
	}

	if ((address & 0xf800) == 0x3000) {
		DrvSpriteRAM[address & 0xff] = data;
		return;
	}

	if ((address & 0xf000) == 0x4000) {
		DrvCharRAM[address & 0xfff] = data;
		expand_chars(address);
		return;
	}

	if ((address & 0xf800) == 0x5000) { // mirrored here
		palette_write(address & 0x1f, data);
		return;
	}

	switch (address & ~0x7f8)
	{
		case 0x5800:
			// watchdog
		return;

		case 0x5801:
			//	analog reset
			if (has_tball) {
				analog_last[0] = BurnTrackballRead(0, 0);
				analog_last[1] = BurnTrackballRead(0, 1);
			}
		return;

		case 0x5802:
			if (type2_sound) {
				soundlatch_r2(data);
			} else {
				soundlatch_r1(address, data);
			}
		return;

		case 0x5803: {
			*background_prio = data & 0x01;
			if (type2_sound == 0) qbert_knocker(data >> 5 & 1);
			if (game_type == 6) // qbertqub only
				*spritebank = (data & 0x10) >> 4;
			flipscreenx = data & 0x02;
			flipscreeny = data & 0x04;
			joystick_select = (data >> 5) & 0x03; // 3stooges
		}
		return;
	}

//	bprintf (0, _T("MW: %4.4x %2.2x\n"), address, data);
}

static UINT8 __fastcall main_read(UINT32 address)
{
	if (address >= 0x10000) {
		return cpu_readmem20(address & 0xffff);
	}

	if ((address & 0xf800) == 0x3000) {
		return DrvSpriteRAM[address & 0xff];
	}

	if ((address & 0xf800) == 0x5000) { // mirrored here
		return DrvPaletteRAM[address & 0x1f];
	}

	switch (address & ~0x7f8)
	{
		case 0x5800: return DrvDip[0];
		case 0x5801: return DrvInput[0]; // DrvDip[1] (fake-dip) for service mode.
		case 0x5802: return (has_tball) ? (BurnTrackballRead(0, 0) - analog_last[0]) : 0xff;
		case 0x5803: return (has_tball) ? (BurnTrackballRead(0, 1) - analog_last[1]) : dialRotation(0);
		case 0x5804: {
			if (game_type == 14) return (DrvInput[1] & 0xf0) | (DrvInput[(joystick_select % 3) + 2] & 0xf);
			return DrvInput[1];
		}
	}

	bprintf (0, _T("MR: %4.4x\n"), address);

	return 0;
}


static void __fastcall reactor_write(UINT32 address, UINT8 data)
{
	if ((address & 0xf000) == 0x2000) {
		DrvSpriteRAM[address & 0xff] = data;
		return;
	}

	if ((address & 0xf000) == 0x4000) {
		DrvCharRAM[address & 0xfff] = data;
		expand_chars(address);
		return;
	}

	if ((address & 0xf000) == 0x6000) { // mirrored here
		palette_write(address & 0x1f, data);
		return;
	}

	switch (address & ~0xff8)
	{
		case 0x7000:
		//	watchdog
		return;

		case 0x7001:
			//	analog reset
			analog_last[0] = BurnTrackballRead(0, 0);
			analog_last[1] = BurnTrackballRead(0, 1);
		return;

		case 0x7002:
			soundlatch_r1(address, data);
		return;

		case 0x7003: {
			*background_prio = data & 0x01;
			flipscreenx = data & 0x02;
			flipscreeny = data & 0x04;
			return;
		}
	}

	if (address >= 0x10000) {
		cpu_writemem20(address & 0xffff, data);
		return;
	}
}

static UINT8 __fastcall reactor_read(UINT32 address)
{
	if ((address & 0xf000) == 0x6000) { // mirrored here
		return DrvPaletteRAM[address & 0x1f];
	}

	switch (address & ~0xff8)
	{
		case 0x7000: return DrvDip[0];
		case 0x7001: return DrvInput[0]; // DrvDip[1] (fake-dip) for service mode.
		case 0x7002: return BurnTrackballRead(0, 0) - analog_last[0];
		case 0x7003: return BurnTrackballRead(0, 1) - analog_last[1];
		case 0x7004: return DrvInput[3];
	}

	if (address >= 0x10000) {
		return cpu_readmem20(address & 0xffff);
	}

	return 0;
}

static UINT8 gottlieb_riot_r(UINT16 offset)
{
	switch (offset & 0x1f)
	{
		case 0: /* port A */
			return *soundlatch ^ 0xff; /* invert command */

		case 2: /* port B */
			return 0x40; /* say that PB6 is 1 (test SW1 not pressed) */

		case 5: /* interrupt register */
			return 0x40; /* say that edge detected on PA7 */

		default:
			return riot_regs[offset & 0x1f];
	}
}

static void blank_queue()
{
#ifdef QBERT_SOUND_DEBUG
	bprintf(0, _T("BLANK!{%X}.."), *vtqueuetime);
#endif
	*vtqueuepos = 0;
	memset(vtqueue, 0, 0x20);
	*vtqueuetime = GetCurrentFrame();
}

static void add_to_queue(UINT8 data)
{
	if (*vtqueuepos > 0x20-1 || (UINT32)GetCurrentFrame() > *vtqueuetime+2)
		blank_queue();
	vtqueue[(*vtqueuepos)++] = data;
}

static UINT8 check_queue()
{
	if (*vtqueuepos == 24 && !strncmp("\xC1\xE4\xFF\xE7\xE8\xD2\xFC\xFC\xFC\xFC\xFC\xEA\xFF\xF6\xD6\xF3\xD5\xC5\xF5\xF2\xE1\xDB\xF2\xC0", (char *)vtqueue, 24)) {
		blank_queue(); // "Hello, I'm turned on."
		return 1;
	}
	if (*vtqueuepos == 26 && !strncmp("\x00\xFF\xD5\xFE\xD5\xC0\xD2\xCB\xD4\xF2\xD6\xEB\xC1\xE6\xCB\xD4\xC1\xCD\xF2\xE0\xFB\xDF\xF1\xCD\xE7\xC0", (char *)vtqueue, 26)) {
		blank_queue(); // "Warning, Core Unstable." (reactor)
		return 11;
	}


	return 0;
}

static void audio_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff; // 15bit addressing

	if (address >= 0x7000 && address <= 0x7fff) {
		bprintf(0, _T("write to audio ROM @ %X."), address);
		Drv6502ROM[address - 0x7000] = data;
	}

	if (/*address >= 0x0000 &&*/ address <= 0x01ff) {
		riot_ram[address & 0x7f] = data;
	}

	if (address >= 0x0200 && address <= 0x03ff) {
		riot_regs[address & 0x1f] = data;
	}

	switch (address)
	{
		case 0x1fff:
		case 0x1000: {
			DACWrite(0, data);
			return;
		}
		case 0x2000: {
			add_to_queue(data);
#ifdef QBERT_SOUND_DEBUG
			bprintf(0, _T("\\x%02X"), data); //save
#endif
			switch (check_queue()) {
				case 1: BurnSamplePlay(42);	break; // Say Hello
				case 11: BurnSamplePlay(5);	break; // Warning Core Unstable
			}
			*soundcpu_do_nmi = 1;
			M6502RunEnd();
			return;
		}
	}
}

static UINT8 audio_read(UINT16 address)
{
	address &= 0x7fff; // 15bit addressing

	if (/*address >= 0x0000 &&*/ address <= 0x01ff) {
		return riot_ram[address&0x7f];
	}

	if (address >= 0x0200 && address <= 0x03ff) {
		return gottlieb_riot_r(address - 0x200);
	}

	return 0;
}

static void sound_r2_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x3ffe)
	{
		case 0x4000:
		case 0x4001:
			dac_data[address & 1] = data;
			DACWrite16(0, dac_data[0] * dac_data[1]);
		return;
	}
}

static UINT8 sound_r2_read(UINT16 address)
{
	switch (address & ~0x3fff)
	{
		case 0x8000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	return 0;
}

static void speech_control_write(UINT8 data)
{
	UINT8 previous = speech_control;
	speech_control = data;

	if ((previous & 0x04) && (~data & 0x04))
	{
		AY8910Write((data & 8) >> 3, (~data & 0x10) >> 4, psg_latch);
	}

	if ((~previous & 0x40) && (data & 0x40)) {
		sp0250_write(sp0250_latch);
	}

	if ((previous ^ data) & 0x80) {
		sp0250_reset();
	}
}

static void sound_r2_speech_write(UINT16 address, UINT8 data)
{
	if (address < 0xa000) address &= ~0x1fff;
	if (address > 0xa000) address &= ~0x07ff;

	switch (address)
	{
		case 0x2000:
			sp0250_latch = data;
		return;

		case 0x4000: {
			double period = 0.0;

			if (data & 1) {
				period = ((1.0 / 1000000) * (250000.0/256/(256-nmi_rate)));
			}

			BurnTimerSetRetrig(0, period);
			//bprintf (0, _T("nmi rate: %d  period %g\n"), nmi_rate, period);

			speech_control_write(data);
		}
		return;

		case 0x8000:
			psg_latch = data;
		return;

		case 0xa000: {
			nmi_rate = data;
		}
		return;

		case 0xb000:
			M6502SetIRQLine(0, 0x20/*nmi*/, CPU_IRQSTATUS_AUTO);
		return;
	}
}

static UINT8 sound_r2_speech_read(UINT16 address)
{
	if (address < 0xa000) address &= ~0x1fff;
	if (address > 0xa000) address &= ~0x07ff;

	switch (address)
	{
		case 0x6000:
			return 0x7f | (sp0250_drq_read() ? 0x80 : 0); // sp status

		case 0xa800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch2;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = DrvVideoRAM[offs];

	TILE_SET_INFO(tilemap_bank[code >> 6], code, 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	VezOpen(0);
	VezReset();
	VezClose();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	if (type2_sound)
	{
		M6502Open(1);
		M6502Reset();
		M6502Close();

		AY8910Reset(0);
		AY8910Reset(1);
		AY8910Reset(2);

		sp0250_reset();

		speech_timer_counter = 0;
		nmi_state = 0;
		nmi_rate = 0;
		psg_latch = 0;
		sp0250_latch = 0;
		soundlatch2 = 0;
		speech_control = 0;
		last_command = 0;
		dac_data[0] = 0xff;
		dac_data[1] = 0xff;

		BurnTimerReset();
	}
	else
	{
		BurnSampleReset();
		qbert_random = BurnRandom() & 7;
		reactor_score = 0;
	}

	DACReset();

	nRotateTime[0] = 0;
	nRotateTime[1] = 0;
	flipscreenx = 0;
	flipscreeny = 0;
	joystick_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV20ROM		= Next; Next += 0x10000;
	Drv6502ROM		= Next; Next += 0x02000;
	Drv6502ROM1		= Next; Next += 0x04000;

	DrvPalette		= (UINT32*)Next; Next += 0x10 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x04000;
	DrvSpriteGFX    = Next; Next += 0x20000;

	DrvCharExp		= Next; Next += 0x02000;

	DrvNVRAM        = Next; Next += 0x01000; // Keep in ROM section.
	DrvDummyROM     = Next; Next += 0x02000; // RAM or ROM depending on conf.

	AllRam			= Next;

	DrvSpriteRAM	= Next; Next += 0x00100;
	DrvV20RAM		= Next; Next += 0x01000;
	Drv6502RAM		= Next; Next += 0x01000;

	DrvVideoRAM		= Next; Next += 0x00400;
	DrvCharRAM		= Next; Next += 0x01000;
	DrvPaletteRAM	= Next; Next += 0x00020;

	riot_regs       = Next; Next += 0x00020;
	riot_ram        = Next; Next += 0x00200;

	vtqueuepos      = Next; Next += 0x00001;
	vtqueuetime     = (UINT32 *)Next; Next += 0x00004;
	vtqueue         = (UINT8 *)Next;  Next += 0x00020;
	knocker_prev    = Next; Next += 0x00001;

	background_prio = Next; Next += 0x00001;
	spritebank      = Next; Next += 0x00001;
	soundlatch      = Next; Next += 0x00001;
	soundcpu_do_nmi = Next; Next += 0x00001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(INT32 len0, INT32 len1)
{
	INT32 c8PlaneOffsets[4]  = { STEP4(0,1) };
	INT32 c8XOffsets[8]      = { STEP8(0,4) };
	INT32 c8YOffsets[8]      = { STEP8(0,32) };
	INT32 c16PlaneOffsets[4] = { RGN_FRAC(len1, 0, 4), RGN_FRAC(len1, 1, 4), RGN_FRAC(len1, 2, 4), RGN_FRAC(len1, 3, 4) };
	INT32 c16XOffsets[16]    = { STEP16(0,1) };
	INT32 c16YOffsets[16]    = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) return;

	if (len0)
	{
		memcpy (tmp, DrvCharGFX, len0);

		GfxDecode((len0 * 2) / (8 * 8), 4, 8, 8, c8PlaneOffsets, c8XOffsets, c8YOffsets, 0x100, tmp, DrvCharGFX);
	}

	memcpy (tmp, DrvSpriteGFX, 0x10000);

	GfxDecode((len1 * 2) / (16 * 16), 4, 16, 16, c16PlaneOffsets, c16XOffsets, c16YOffsets, 0x100, tmp, DrvSpriteGFX);

	BurnFree (tmp);
}

static INT32 RomLoad()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad = DrvV20ROM;
	UINT8 *sLoad = Drv6502ROM;
	UINT8 *mLoad = Drv6502ROM1;
	UINT8 *gLoad = DrvCharGFX;
	UINT8 *rLoad = DrvSpriteGFX;
	UINT8 *dLoad = DrvDummyROM;

	// calculate load offsets for program roms
	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) {
			pLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 2) {
			sLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 6) {
			mLoad += ri.nLen;
			continue;
		}
	}

	pLoad = DrvV20ROM + (0x10000 - (pLoad - DrvV20ROM));
	sLoad = Drv6502ROM + (0x2000 - (sLoad - Drv6502ROM));
	mLoad = Drv6502ROM1 + (0x4000 - (mLoad - Drv6502ROM1));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) { // i8088
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 2) { // m6502
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 6) { // m6502 (speech)
			type2_sound = 1;
			if (BurnLoadRom(mLoad, i, 1)) return 1;
			mLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 3) { // background tiles
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 4) { // sprites
			if (game_type == 10) rLoad += 0x1000;
			if (BurnLoadRom(rLoad, i, 1)) return 1;
			rLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 9) { // extra i8088 roms
			if (BurnLoadRom(dLoad, i, 1)) return 1;
			dLoad += ri.nLen;
			switch (dLoad - DrvDummyROM) {
				case 0x1000: DummyRegion[1] = MAP_ROM; break;
				case 0x2000: DummyRegion[0] = MAP_ROM; break;
			}
			continue;
		}
	}

	INT32 tilelen = gLoad - DrvCharGFX;
	if (tilelen == 0) memset (tilemap_bank, 1, 4);

	bprintf (0, _T("tiles: %x, sprite: %x\n"), tilelen, rLoad - DrvSpriteGFX);

	bprintf(0, _T("DummyRegion[0] = "));
	switch (DummyRegion[0]) {
		case MAP_ROM: bprintf(0, _T("MAP_ROM\n")); break;
		case MAP_RAM: bprintf(0, _T("MAP_RAM\n")); break;
	}
	bprintf(0, _T("DummyRegion[1] = "));
	switch (DummyRegion[1]) {
		case MAP_ROM: bprintf(0, _T("MAP_ROM\n")); break;
		case MAP_RAM: bprintf(0, _T("MAP_RAM\n")); break;
	}

	DrvGfxDecode(tilelen, rLoad - DrvSpriteGFX);

	if ((dLoad - DrvDummyROM) == 0x1000) { // single-rom loads @ 0x2000
		memcpy (DrvDummyROM + 0x1000, DrvDummyROM, 0x1000);
		memset (DrvDummyROM + 0x0000, 0x00, 0x1000);
	}

	return 0;
}


INT32 t2_timer_cb(INT32 n, INT32 c)
{
	if (nmi_state)
	{
		nmi_state = 0;
		M6502SetIRQLine(0x20/*nmi*/, CPU_IRQSTATUS_NONE);
	}

	if (speech_timer_counter == 0xff)
	{
		nmi_state = 1;
		M6502SetIRQLine(0x20/*nmi*/, (speech_control & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}

	speech_timer_counter = (speech_timer_counter + 1) & 0xff;

	return 0;
}

static void type2_sound_init()
{
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	for (INT32 i = 0; i < 0x4000; i+= 0x400) {
		M6502MapMemory(Drv6502RAM,		0x0000 + i, 0x03ff + i, MAP_RAM);
	}

	M6502MapMemory(Drv6502ROM,			0xe000, 0xffff, MAP_ROM);
	M6502MapMemory(Drv6502ROM,			0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(sound_r2_write);
	M6502SetReadHandler(sound_r2_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	for (INT32 i = 0; i < 0x2000; i+= 0x400) {
		M6502MapMemory(Drv6502RAM + 0x400,	0x0000 + i, 0x03ff + i, MAP_RAM);
	}

	M6502MapMemory(Drv6502ROM1,			0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(sound_r2_speech_write);
	M6502SetReadHandler(sound_r2_speech_read);
	M6502Close();

	BurnTimerInit(&t2_timer_cb, NULL); // for high-freq dac&speech timer
	BurnTimerAttachM6502(1000000);

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 0);
	AY8910Init(2, 2000000, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.15, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6502TotalCycles, 1000000);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	sp0250_init(3120000, NULL, M6502TotalCycles, 1000000);
	sp0250_volume(1.00);
}

static void type1_sound_init()
{
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502ROM,		0x6000, 0x7fff, MAP_ROM);
	M6502MapMemory(Drv6502ROM,		0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(audio_write);
	M6502SetReadHandler(audio_read);
	M6502SetReadOpArgHandler(audio_read);
	M6502SetReadOpHandler(audio_read);
	M6502Close();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.30, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6502TotalCycles, 3579545 / 4);
	DACSetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{   // Load ROMS parse GFX
		DummyRegion[0] = DummyRegion[1] = MAP_RAM; // start out as RAM. change to ROM if they load in region(s)

		if (RomLoad()) return 1;
	}

	if (game_type == 10) // reactor
	{
		VezInit(0, V20_TYPE); // really i8088, but v20 is compatible
		VezOpen(0);
		VezMapMemory(DrvDummyROM,		0x0000, 0x1fff, MAP_RAM); // RAM (reactor)
		VezMapMemory(DrvVideoRAM,		0x3000, 0x33ff, MAP_RAM);
		VezMapMemory(DrvVideoRAM,		0x3400, 0x37ff, MAP_RAM); // mirror
		VezMapMemory(DrvVideoRAM,		0x3800, 0x3bff, MAP_RAM); // mirror
		VezMapMemory(DrvVideoRAM,		0x3c00, 0x3fff, MAP_RAM); // mirror
		VezMapMemory(DrvCharRAM,		0x4000, 0x4fff, MAP_ROM); // write through handler
		VezMapMemory(DrvV20ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
		VezSetReadHandler(reactor_read);
		VezSetWriteHandler(reactor_write);
		VezClose();
	}
	else // standard
	{
		VezInit(0, V20_TYPE); // really i8088, but v20 is compatible
		VezOpen(0);
		VezMapMemory(DrvNVRAM,			0x0000, 0x0fff, MAP_RAM);
		VezMapMemory(DrvDummyROM + 0x0000,		0x1000, 0x1fff, DummyRegion[0]); // ROM for argus, krull, vidvince & 3stooges, RAM for all others
		VezMapMemory(DrvDummyROM + 0x1000,		0x2000, 0x2fff, DummyRegion[1]);
		VezMapMemory(DrvVideoRAM,		0x3800, 0x3bff, MAP_RAM);
		VezMapMemory(DrvVideoRAM,		0x3c00, 0x3fff, MAP_RAM); // mirror
		VezMapMemory(DrvCharRAM,		0x4000, 0x4fff, MAP_ROM); // write through handler
		VezMapMemory(DrvV20ROM + 0x6000,	0x6000, 0xffff, MAP_ROM);
		VezSetReadHandler(main_read);
		VezSetWriteHandler(main_write);
		VezClose();
	}

	if (has_tball) {
		BurnTrackballInit(2);
	}

	if (type2_sound) {
		type2_sound_init();
	} else {
		type1_sound_init();
	}

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvCharGFX, 4, 8, 8, 0x4000, 0, 0);
	GenericTilemapSetGfx(1, DrvCharExp, 4, 8, 8, 0x2000, 0, 0);
	GenericTilemapSetTransparent(0, 0);

	memset(DrvNVRAM, 0xff, 0x1000); // Init NVRAM

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();
	M6502Exit();
	DACExit();

	if (type2_sound == 0)
	{
		BurnSampleExit();
	}
	else
	{
		AY8910Exit(0);
		AY8910Exit(1);
		AY8910Exit(2);

		sp0250_exit();

		BurnTimerExit();
	}

	if (has_tball) { // reactor, argusg, wizwarz
		BurnTrackballExit();
		has_tball = 0;
	}

	BurnFree(AllMem);

	game_type = 0;
	type2_sound = 0;

	tilemap_bank[0] = 0;
	tilemap_bank[1] = 0;
	tilemap_bank[2] = 0;
	tilemap_bank[3] = 0;

	return 0;
}

static void draw_sprites()
{
	GenericTilesSetClip(8, -1, -1, -1);

	for (INT32 offs = 0; offs < 0x100 - 8; offs += 4)
	{
		INT32 sx = ((DrvSpriteRAM[offs + 1]) - 4) + ((game_type == 4) ? 7 : 0); // mplanets has weird sx/sy offsets
		INT32 sy = ((DrvSpriteRAM[offs]) - 13) - ((game_type == 4) ? 4 : 0);    // apparent in the hiscore table.
		INT32 code = (255 ^ DrvSpriteRAM[offs + 2]) + (256 * *spritebank);

		INT32 flipy = 0;
		INT32 flipx = 0;

		if (flipscreenx) {
			sx = (256 - 8) - sx;
			flipx = 1;
		}

		if (flipscreeny) {
			sy = (240 - 8) - sy;
			flipy = 1;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, 0, 4, 0, 0, DrvSpriteGFX);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x20; i++) {
			palette_write(i, DrvPaletteRAM[i]);
		}
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreeny ? TMAP_FLIPY : 0) | (flipscreenx ? TMAP_FLIPX : 0));

	if (nBurnLayer & 1 && !*background_prio) {
		GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);
	} else {
		BurnTransferClear();
	}

	if (nBurnLayer & 4) draw_sprites();

	if (nBurnLayer & 2 &&  *background_prio) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = DrvDip[1];
	DrvInput[1] = 0;

	DrvInput[2] = 0; // 3stooges
	DrvInput[3] = 0; // & reactor
	DrvInput[4] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInput[3] ^= (DrvJoy4[i] & 1) << i;
		DrvInput[4] ^= (DrvJoy5[i] & 1) << i;
	}

	if (has_tball) { // reactor, argusg, wizwarz
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x04);
		BurnTrackballUpdate(0);
	}

}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();
	M6502NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 5000000 / 60, (3579545 / 4) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	VezOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Vez);
		if (i == (nInterleave - 1))
			VezSetIRQLineAndVector(0x20, 0xff, CPU_IRQSTATUS_AUTO);

		CPU_RUN_SYNCINT(1, M6502);
		if (*soundcpu_do_nmi) {
			M6502Run(44); // 50usec later..
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			*soundcpu_do_nmi = 0;
		}

		if ((i%64) == 63 && has_tball && game_type != 9) { // reactor, wizwarz, not argus
			BurnTrackballUpdate(0);
		}
	}

	VezClose();
	M6502Close();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 Drv2Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();
	M6502NewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 5000000 / 60, 1000000 / 60, 1000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0 };

	VezOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Vez);
		if (i == (nInterleave - 1))
			VezSetIRQLineAndVector(0x20, 0xff, CPU_IRQSTATUS_AUTO);

		M6502Open(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		if (i == nInterleave - 1) BurnTimerEndFrame(nCyclesTotal[1]);
		sp0250_tick();
		M6502Close();

		M6502Open(0);
		CPU_RUN(2, M6502);
		M6502Close();
	}

	VezClose();

	M6502Open(0);
	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		sp0250_update(pBurnSoundOut, nBurnSoundLen);
	}
	M6502Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));     // savestates get f*cked up, because NVRAM is also used
		ba.Data		= DrvNVRAM;         // as regular memory, to fix that we will scan it both here
		ba.nLen		= 0x1000;           // and in the NVRAM section.
		ba.szName	= "SSNVRAM";        // note: this is separate from "All Ram" so it doesn't get trashed in DrvDoReset();
		BurnAcb(&ba);

		if (DummyRegion[0] == MAP_RAM) {
			ScanVar(DrvDummyROM + 0x0000, 0x1000, "DummyRAM0");
		}
		if (DummyRegion[1] == MAP_RAM) {
			ScanVar(DrvDummyROM + 0x1000, 0x1000, "DummyRAM1");
		}

		VezScan(nAction);
		M6502Scan(nAction);

		if (type2_sound) {
			AY8910Scan(nAction, pnMin);
			sp0250_scan(nAction, pnMin);
			BurnTimerScan(nAction, pnMin);
		} else {
			BurnSampleScan(nAction, pnMin);
		}

		DACScan(nAction, pnMin);

		if (has_tball) {
			BurnTrackballScan(); // reactor
		}

		SCAN_VAR(flipscreenx);
		SCAN_VAR(flipscreeny);
		SCAN_VAR(joystick_select);

		SCAN_VAR(speech_timer_counter);
		SCAN_VAR(nmi_state);
		SCAN_VAR(nmi_rate);
		SCAN_VAR(psg_latch);
		SCAN_VAR(sp0250_latch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(speech_control);
		SCAN_VAR(last_command);
		SCAN_VAR(dac_data);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x1000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		for (INT32 i = 0; i < 0x1000; i++) {
			expand_chars(i);
		}
	}

	return 0;
}

static struct BurnSampleInfo qbertSampleDesc[] = {
	{"fx_17a", SAMPLE_NOLOOP },
	{"fx_17b", SAMPLE_NOLOOP },
	{"fx_17c", SAMPLE_NOLOOP },
	{"fx_17d", SAMPLE_NOLOOP },
	{"fx_17e", SAMPLE_NOLOOP },
	{"fx_17f", SAMPLE_NOLOOP },
	{"fx_17g", SAMPLE_NOLOOP },
	{"fx_17h", SAMPLE_NOLOOP },
	{"fx_18a", SAMPLE_NOLOOP },
	{"fx_18b", SAMPLE_NOLOOP },
	{"fx_18c", SAMPLE_NOLOOP },
	{"fx_18d", SAMPLE_NOLOOP },
	{"fx_18e", SAMPLE_NOLOOP },
	{"fx_18f", SAMPLE_NOLOOP },
	{"fx_18g", SAMPLE_NOLOOP },
	{"fx_18h", SAMPLE_NOLOOP },
	{"fx_19a", SAMPLE_NOLOOP },
	{"fx_19b", SAMPLE_NOLOOP },
	{"fx_19c", SAMPLE_NOLOOP },
	{"fx_19d", SAMPLE_NOLOOP },
	{"fx_19e", SAMPLE_NOLOOP },
	{"fx_19f", SAMPLE_NOLOOP },
	{"fx_19g", SAMPLE_NOLOOP },
	{"fx_19h", SAMPLE_NOLOOP },
	{"fx_20a", SAMPLE_NOLOOP },
	{"fx_20b", SAMPLE_NOLOOP },
	{"fx_20c", SAMPLE_NOLOOP },
	{"fx_20d", SAMPLE_NOLOOP },
	{"fx_20e", SAMPLE_NOLOOP },
	{"fx_20f", SAMPLE_NOLOOP },
	{"fx_20g", SAMPLE_NOLOOP },
	{"fx_20h", SAMPLE_NOLOOP },
	{"fx_21a", SAMPLE_NOLOOP },
	{"fx_21b", SAMPLE_NOLOOP },
	{"fx_21c", SAMPLE_NOLOOP },
	{"fx_21d", SAMPLE_NOLOOP },
	{"fx_21e", SAMPLE_NOLOOP },
	{"fx_21f", SAMPLE_NOLOOP },
	{"fx_21g", SAMPLE_NOLOOP },
	{"fx_21h", SAMPLE_NOLOOP },
	{"fx_22",  SAMPLE_NOLOOP },
	{"fx_23",  SAMPLE_NOLOOP },
	{"fx_28",  SAMPLE_NOLOOP },
	{"fx_36",  SAMPLE_NOLOOP },
	{"knocker", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(qbert)
STD_SAMPLE_FN(qbert)

static struct BurnSampleInfo reactorSampleDesc[] = {
	{"fx_53", SAMPLE_NOLOOP },
	{"fx_54", SAMPLE_NOLOOP },
	{"fx_55", SAMPLE_NOLOOP },
	{"fx_56", SAMPLE_NOLOOP },
	{"fx_57", SAMPLE_NOLOOP },
	{"fx_58", SAMPLE_NOLOOP },
	{"fx_59", SAMPLE_NOLOOP },
	{"fx_31", SAMPLE_NOLOOP },
	{"fx_39a", SAMPLE_NOLOOP },
	{"fx_39b", SAMPLE_NOLOOP },
	{"fx_39c", SAMPLE_NOLOOP },
	{"fx_39d", SAMPLE_NOLOOP },
	{"fx_39e", SAMPLE_NOLOOP },
	{"fx_39f", SAMPLE_NOLOOP },
	{"fx_39g", SAMPLE_NOLOOP },
	{"fx_39h", SAMPLE_NOLOOP },
	{"fx_39i", SAMPLE_NOLOOP },
	{"fx_39j", SAMPLE_NOLOOP },
	{"fx_39k", SAMPLE_NOLOOP },
	{"fx_39l", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(reactor)
STD_SAMPLE_FN(reactor)


// Q*bert (US set 1)

static struct BurnRomInfo qbertRomDesc[] = {
	{ "qb-rom2.bin",		0x2000, 0xfe434526, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qb-rom1.bin",		0x2000, 0x55635447, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb-rom0.bin",		0x2000, 0x8e318641, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xdd436d3a, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0xf69b9483, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x224e8356, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0x2f695b85, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(qbert)
STD_ROM_FN(qbert)

struct BurnDriver BurnDrvQbert = {
	"qbert", NULL, NULL, "qbert", "1982",
	"Q*bert (US set 1)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, qbertRomInfo, qbertRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert (US set 2)

static struct BurnRomInfo qbertaRomDesc[] = {
	{ "qrom_2.bin",			0x2000, 0xb54a8ffc, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qrom_1.bin",			0x2000, 0x19d924e3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qrom_0.bin",			0x2000, 0x2e7fad1b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xdd436d3a, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0xf69b9483, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x224e8356, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0x2f695b85, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(qberta)
STD_ROM_FN(qberta)

struct BurnDriver BurnDrvQberta = {
	"qberta", "qbert", NULL, "qbert", "1982",
	"Q*bert (US set 2)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, qbertaRomInfo, qbertaRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert (Japan)

static struct BurnRomInfo qbertjRomDesc[] = {
	{ "qbj-rom2.bin",		0x2000, 0x67bb1cb2, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qbj-rom1.bin",		0x2000, 0xc61216e7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qbj-rom0.bin",		0x2000, 0x69679d5c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xdd436d3a, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0xf69b9483, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x224e8356, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0x2f695b85, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(qbertj)
STD_ROM_FN(qbertj)

struct BurnDriver BurnDrvQbertj = {
	"qbertj", "qbert", NULL, "qbert", "1982",
	"Q*bert (Japan)\0", NULL, "Gottlieb (Konami license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, qbertjRomInfo, qbertjRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert (early test version)

static struct BurnRomInfo qberttstRomDesc[] = {
	{ "qbtst2.bin",			0x2000, 0x55307b02, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qbtst1.bin",			0x2000, 0xe97fdd78, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qbtst0.bin",			0x2000, 0x94c9f588, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xdd436d3a, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0xf69b9483, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x224e8356, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0x2f695b85, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(qberttst)
STD_ROM_FN(qberttst)

struct BurnDriver BurnDrvQberttst = {
	"qberttst", "qbert", NULL, NULL, "1982",
	"Q*bert (early test version)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, qberttstRomInfo, qberttstRomName, NULL, NULL, NULL, NULL, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert Board Input Test Rom

static struct BurnRomInfo qbtrktstRomDesc[] = {
	{ "qb-rom2.bin",		0x2000, 0xfe434526, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qb-rom1.bin",		0x2000, 0x55635447, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv103_t-ball-test_rom0_2764.c11c12",	0x2000, 0x5d390cd2, 1 | BRF_PRG | BRF_ESS }, //  2 - longest rom name ever!

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xdd436d3a, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0xf69b9483, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x224e8356, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0x2f695b85, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(qbtrktst)
STD_ROM_FN(qbtrktst)

// mark as debug only to avoid confusion
struct BurnDriverD BurnDrvQbtrktst = {
	"qbtrktst", "qbert", NULL, NULL, "1982",
	"Q*bert Board Input Test Rom\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, qbtrktstRomInfo, qbtrktstRomName, NULL, NULL, NULL, NULL, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Mello Yello Q*bert

static struct BurnRomInfo myqbertRomDesc[] = {
	{ "mqb-rom2.bin",		0x2000, 0x6860f957, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "mqb-rom1.bin",		0x2000, 0x11f0a4e4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mqb-rom0.bin",		0x2000, 0x12a90cb2, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mqb-snd1.bin",		0x0800, 0x495ffcd2, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "mqb-snd2.bin",		0x0800, 0x9bbaa945, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0x7a9ba824, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x22e5b891, 3 | BRF_GRA },           //  6

	{ "mqb-fg3.bin",		0x2000, 0x8b5d0852, 4 | BRF_GRA },           //  7 Sprites
	{ "mqb-fg2.bin",		0x2000, 0x823f1e57, 4 | BRF_GRA },           //  8
	{ "mqb-fg1.bin",		0x2000, 0x05343ae6, 4 | BRF_GRA },           //  9
	{ "mqb-fg0.bin",		0x2000, 0xabc71bdd, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(myqbert)
STD_ROM_FN(myqbert)

struct BurnDriver BurnDrvMyqbert = {
	"myqbert", "qbert", NULL, "qbert", "1982",
	"Mello Yello Q*bert\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, myqbertRomInfo, myqbertRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Faster, Harder, More Challenging Q*bert (prototype)

static struct BurnRomInfo sqbertRomDesc[] = {
	{ "qb-rom2.bin",		0x2000, 0x1e3d4038, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qb-rom1.bin",		0x2000, 0xeaf3076c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb-rom0.bin",		0x2000, 0x61260a7e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",		0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 m6502 Code
	{ "qb-snd2.bin",		0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",			0x1000, 0xc3118eef, 3 | BRF_GRA },           //  5 Background Tiles
	{ "qb-bg1.bin",			0x1000, 0x4f6d8075, 3 | BRF_GRA },           //  6

	{ "qb-fg3.bin",			0x2000, 0xee595eda, 4 | BRF_GRA },           //  7 Sprites
	{ "qb-fg2.bin",			0x2000, 0x59884c78, 4 | BRF_GRA },           //  8
	{ "qb-fg1.bin",			0x2000, 0x2a60e3ad, 4 | BRF_GRA },           //  9
	{ "qb-fg0.bin",			0x2000, 0xb11ad9d8, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(sqbert)
STD_ROM_FN(sqbert)

static INT32 DrvInitMplanets()
{
	game_type = 4;

	return DrvInit();
}

struct BurnDriver BurnDrvSqbert = {
	"sqbert", NULL, NULL, "qbert", "1983",
	"Faster, Harder, More Challenging Q*bert (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sqbertRomInfo, sqbertRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};

// Mad Planets

static struct BurnRomInfo mplanetsRomDesc[] = {
	{ "rom4.c16",			0x2000, 0x5402077f, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "rom3.c14-15",		0x2000, 0x5d18d740, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom2.c13-14",		0x2000, 0x960c3bb1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom1.c12-13",		0x2000, 0xeb515f10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom0.c11-12",		0x2000, 0x74de78aa, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "snd1",			0x0800, 0x453193a1, 2 | BRF_PRG | BRF_ESS }, //  5 m6502 Code
	{ "snd2",			0x0800, 0xf5ffc98f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bg0.e11-12",			0x1000, 0x709aa24c, 3 | BRF_GRA },           //  7 Background Tiles
	{ "bg1.e13",			0x1000, 0x4921e345, 3 | BRF_GRA },           //  8

	{ "fg3.k7-8",			0x2000, 0xc990b39f, 4 | BRF_GRA },           //  9 Sprites
	{ "fg2.k6",			0x2000, 0x735e2522, 4 | BRF_GRA },           // 10
	{ "fg1.k5",			0x2000, 0x6456cc1c, 4 | BRF_GRA },           // 11
	{ "fg0.k4",			0x2000, 0xa920e325, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(mplanets)
STD_ROM_FN(mplanets)

struct BurnDriver BurnDrvMplanets = {
	"mplanets", NULL, NULL, NULL, "1983",
	"Mad Planets\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mplanetsRomInfo, mplanetsRomName, NULL, NULL, NULL, NULL, MplanetsInputInfo, MplanetsDIPInfo,
	DrvInitMplanets, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Mad Planets (UK)

static struct BurnRomInfo mplanetsukRomDesc[] = {
	{ "mpt_rom4.bin",		0x2000, 0xcd88e23c, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "mpt_rom3.bin",		0x2000, 0xdc355b2d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpt_rom2.bin",		0x2000, 0x846ddc23, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpt_rom1.bin",		0x2000, 0x94d67b87, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mpt_rom0.bin",		0x2000, 0xa9e30ad2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "mpt_snd1.bin",		0x0800, 0x453193a1, 2 | BRF_PRG | BRF_ESS }, //  5 m6502 Code
	{ "mpt_snd2.bin",		0x0800, 0xf5ffc98f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "mpt_bg0.bin",		0x1000, 0x709aa24c, 3 | BRF_GRA },           //  7 Background Tiles
	{ "mpt_bg1.bin",		0x1000, 0x4921e345, 3 | BRF_GRA },           //  8

	{ "mpt_fg3.bin",		0x2000, 0xc990b39f, 4 | BRF_GRA },           //  9 Sprites
	{ "mpt_fg2.bin",		0x2000, 0x735e2522, 4 | BRF_GRA },           // 10
	{ "mpt_fg1.bin",		0x2000, 0x6456cc1c, 4 | BRF_GRA },           // 11
	{ "mpt_fg0.bin",		0x2000, 0xa920e325, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(mplanetsuk)
STD_ROM_FN(mplanetsuk)

struct BurnDriver BurnDrvMplanetsuk = {
	"mplanetsuk", "mplanets", NULL, NULL, "1983",
	"Mad Planets (UK)\0", NULL, "Gottlieb (Taitel license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mplanetsukRomInfo, mplanetsukRomName, NULL, NULL, NULL, NULL, MplanetsInputInfo, MplanetsDIPInfo,
	DrvInitMplanets, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert's Qubes

static struct BurnRomInfo qbertqubRomDesc[] = {
	{ "qq-rom3.bin",		0x2000, 0xc4dbdcd7, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "qq-rom2.bin",		0x2000, 0x21a6c6cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qq-rom1.bin",		0x2000, 0x63e6c43d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qq-rom0.bin",		0x2000, 0x8ddbe438, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "qq-snd1.bin",		0x0800, 0xe704b450, 2 | BRF_PRG | BRF_ESS }, //  4 m6502 Code
	{ "qq-snd2.bin",		0x0800, 0xc6a98bf8, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "qq-bg0.bin",			0x1000, 0x050badde, 3 | BRF_GRA },           //  6 Background Tiles
	{ "qq-bg1.bin",			0x1000, 0x8875902f, 3 | BRF_GRA },           //  7

	{ "qq-fg3.bin",			0x4000, 0x91a949cc, 4 | BRF_GRA },           //  8 Sprites
	{ "qq-fg2.bin",			0x4000, 0x782d9431, 4 | BRF_GRA },           //  9
	{ "qq-fg1.bin",			0x4000, 0x71c3ac4c, 4 | BRF_GRA },           // 10
	{ "qq-fg0.bin",			0x4000, 0x6192853f, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(qbertqub)
STD_ROM_FN(qbertqub)

static INT32 DrvInitCube()
{
	game_type = 6;

	return DrvInit();
}

struct BurnDriver BurnDrvQbertqub = {
	"qbertqub", NULL, NULL, "qbert", "1983",
	"Q*bert's Qubes\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, qbertqubRomInfo, qbertqubRomName, NULL, NULL, qbertSampleInfo, qbertSampleName, QbertqubInputInfo, QbertqubDIPInfo,
	DrvInitCube, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Insector (prototype)

static struct BurnRomInfo insectorRomDesc[] = {
	{ "rom3",			0x2000, 0x640881fd, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "rom2",			0x2000, 0x456bc3f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom1",			0x2000, 0x706962af, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom0",			0x2000, 0x31cee24b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gv106s.bin",			0x1000, 0x25bcc8bc, 2 | BRF_PRG | BRF_ESS }, //  4 m6502 Code

	{ "bg0",			0x1000, 0x0dc2037e, 3 | BRF_GRA },           //  5 Background Tiles
	{ "bg1",			0x1000, 0x3dd73b94, 3 | BRF_GRA },           //  6

	{ "fg3",			0x2000, 0x9bbf5b6b, 4 | BRF_GRA },           //  7 Sprites
	{ "fg2",			0x2000, 0x5adf9986, 4 | BRF_GRA },           //  8
	{ "fg1",			0x2000, 0x4bb16111, 4 | BRF_GRA },           //  9
	{ "fg0",			0x2000, 0x965f6b76, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(insector)
STD_ROM_FN(insector)

static INT32 DrvInitInsector()
{
	game_type = 8;

	return DrvInit();
}

struct BurnDriver BurnDrvInsector = {
	"insector", NULL, NULL, NULL, "1982",
	"Insector (prototype)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, insectorRomInfo, insectorRomName, NULL, NULL, NULL, NULL, InsectorInputInfo, InsectorDIPInfo,
	DrvInitInsector, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Argus (Gottlieb, prototype)

static struct BurnRomInfo argusgRomDesc[] = {
	{ "arg_ram2_2732.c7",		0x1000, 0x5d35b83e, 9 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "arg_ram4_2732.c9c10",	0x1000, 0x7180e823, 9 | BRF_PRG | BRF_ESS }, //  1

	{ "arg_rom4_2764.c16",		0x2000, 0x2f48bd78, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "arg_rom3_2764.c14c15",	0x2000, 0x4dc2914c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "arg_rom2_2764.c13c14",	0x2000, 0xb5e9ee77, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "arg_rom1_2764.c12c13",	0x2000, 0x733d3d44, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "arg_rom0_2764.c11c12",	0x2000, 0xe1906355, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "arg_snd1_2716.u5",		0x0800, 0x3a6cf455, 2 | BRF_PRG | BRF_ESS }, //  7 m6502 Code
	{ "arg_snd2_2716.u6",		0x0800, 0xddf32040, 2 | BRF_PRG | BRF_ESS }, //  8

	// ram-based background tiles

	{ "arg_fg3_2764.k7k8",		0x2000, 0xcdb6e25c, 4 | BRF_GRA },           //  9 Sprites
	{ "arg_fg2_2764.k6",		0x2000, 0xf10af1be, 4 | BRF_GRA },           // 10
	{ "arg_fg1_2764.k5",		0x2000, 0x5add96e5, 4 | BRF_GRA },           // 11
	{ "arg_fg0_2764.k4",		0x2000, 0x5b7bd588, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(argusg)
STD_ROM_FN(argusg)

static INT32 DrvInitArgusg()
{
	game_type = 9;
	has_tball = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvArgusg = {
	"argusg", NULL, NULL, NULL, "1984",
	"Argus (Gottlieb, prototype)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, argusgRomInfo, argusgRomName, NULL, NULL, NULL, NULL, ArgusgInputInfo, ArgusgDIPInfo,
	DrvInitArgusg, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Krull

static struct BurnRomInfo krullRomDesc[] = {
	{ "gv-105_ram_2.c7",		0x1000, 0x302feadf, 9 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv-105_ram_4.c9-10",		0x1000, 0x79355a60, 9 | BRF_PRG | BRF_ESS }, //  1
	{ "gv-105_rom_4.c16",		0x2000, 0x2b696394, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv-105_rom_3.c14-15",	0x2000, 0x14b0ee42, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gv-105_rom_2.c13-14",	0x2000, 0xb5fad94a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gv-105_rom_1.c12-13",	0x2000, 0x1ad956a3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "gv-105_rom_0.c11-12",	0x2000, 0xa466afae, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "snd1.bin",			0x1000, 0xdd2b30b4, 2 | BRF_PRG | BRF_ESS }, //  7 m6502 Code
	{ "snd2.bin",			0x1000, 0x8cab901b, 2 | BRF_PRG | BRF_ESS }, //  8

	// ram-based background tiles

	{ "gv-105_fg_3.k7-8",		0x2000, 0x82d77a45, 4 | BRF_GRA },           //  9 Sprites
	{ "gv-105_fg_2.k6",		0x2000, 0x25a24317, 4 | BRF_GRA },           // 10
	{ "gv-105_fg_1.k5",		0x2000, 0x7e3ad7b0, 4 | BRF_GRA },           // 11
	{ "gv-105_fg_0.k4",		0x2000, 0x7402dc19, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(krull)
STD_ROM_FN(krull)

struct BurnDriver BurnDrvKrull = {
	"krull", NULL, NULL, NULL, "1983",
	"Krull\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, krullRomInfo, krullRomName, NULL, NULL, NULL, NULL, KrullInputInfo, KrullDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Curve Ball

static struct BurnRomInfo curvebalRomDesc[] = {
	{ "gv-134_rom_3.rom3_c14-15",	0x2000, 0x72ad4d45, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv-134_rom_2.rom2_c13-14",	0x2000, 0xd46c3db5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv-134_rom_1.rom1_c12-13",	0x2000, 0xeb1e08bd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv-134_rom_0.rom0_c11-12",	0x2000, 0x401fc7e3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "yrom.sbd",			0x1000, 0x4c313d9b, 2 | BRF_PRG | BRF_ESS }, //  4 m6502 Code
	{ "drom.sbd",			0x1000, 0xcecece88, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "gv-134_bg_0.bg0_e11-12",	0x1000, 0xd666a179, 3 | BRF_GRA },           //  6 Background Tiles
	{ "gv-134_bg_1.bg1_e13",	0x1000, 0x5e34ff4e, 3 | BRF_GRA },           //  7

	{ "gv-134_fg_3.fg3_k7-8",	0x2000, 0x9c9452fe, 4 | BRF_GRA },           //  8 Sprites
	{ "gv-134_fg_2.fg2_k6",		0x2000, 0x065131af, 4 | BRF_GRA },           //  9
	{ "gv-134_fg_1.fg1_k5",		0x2000, 0x1b7b7f94, 4 | BRF_GRA },           // 10
	{ "gv-134_fg_0.fg0_k4",		0x2000, 0xe3a8230e, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(curvebal)
STD_ROM_FN(curvebal)

static INT32 DrvInitCurvebal()
{
	game_type = 7;

	return DrvInit();
}

struct BurnDriver BurnDrvCurvebal = {
	"curvebal", NULL, NULL, NULL, "1984",
	"Curve Ball\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, curvebalRomInfo, curvebalRomName, NULL, NULL, NULL, NULL, CurvebalInputInfo, CurvebalDIPInfo,
	DrvInitCurvebal, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Tylz (prototype)

static struct BurnRomInfo tylzRomDesc[] = {
	{ "tylz.s4t",			0x2000, 0x28ed146d, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "tylz.s4b",			0x2000, 0x18ee09f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tylz.r4",			0x2000, 0x657c3d2e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tylz.n4",			0x2000, 0xb2a15510, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tylz.f2",			0x1000, 0xebcedba9, 2 | BRF_PRG | BRF_ESS }, //  4 m6502 Code

	{ "tylz.m6",			0x1000, 0x5e300b9b, 3 | BRF_GRA },           //  5 Background Tiles
	{ "tylz.n6",			0x1000, 0xaf56292e, 3 | BRF_GRA },           //  6

	{ "tylz.f12",			0x2000, 0x6d2c5ad8, 4 | BRF_GRA },           //  7 Sprites
	{ "tylz.g12",			0x2000, 0x1eb26e6f, 4 | BRF_GRA },           //  8
	{ "tylz.j12",			0x2000, 0xbc319067, 4 | BRF_GRA },           //  9
	{ "tylz.k12",			0x2000, 0xff62bc4b, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(tylz)
STD_ROM_FN(tylz)

static INT32 DrvInitTylz()
{
	game_type = 5;

	return DrvInit();
}

struct BurnDriver BurnDrvTylz = {
	"tylz", NULL, NULL, NULL, "1982",
	"Tylz (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tylzRomInfo, tylzRomName, NULL, NULL, NULL, NULL, TylzInputInfo, TylzDIPInfo,
	DrvInitTylz, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Knightmare (prototype)

static struct BurnRomInfo kngtmareRomDesc[] = {
	{ "gv112_rom3_2764.c14c15",	0x2000, 0x47351270, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv112_rom2_2764.c13c14",	0x2000, 0x53e01f97, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv112_rom1_2764.c12c13",	0x2000, 0x5b340640, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv112_rom0_2764.c11c12",	0x2000, 0x620dc629, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gv112_snd",			0x1000, 0, 0 | BRF_NODUMP }, //  4 m6502 Code

	{ "gv112_bg0_2732.e11e12",	0x1000, 0xa74591fd, 3 | BRF_GRA },           //  5 Background Tiles
	{ "gv112_bg1_2732.e13",		0x1000, 0x5a226e6a, 3 | BRF_GRA },           //  6

	{ "gv112_fg3_2764.k7k8",	0x2000, 0xd1886658, 4 | BRF_GRA },           //  7 Sprites
	{ "gv112_fg2_2764.k6",		0x2000, 0xe1c73f0c, 4 | BRF_GRA },           //  8
	{ "gv112_fg1_2764.k5",		0x2000, 0x724bc3ea, 4 | BRF_GRA },           //  9
	{ "gv112_fg0_2764.k4",		0x2000, 0x0311bbd9, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(kngtmare)
STD_ROM_FN(kngtmare)

struct BurnDriver BurnDrvKngtmare = {
	"kngtmare", NULL, NULL, NULL, "1983",
	"Knightmare (prototype)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, kngtmareRomInfo, kngtmareRomName, NULL, NULL, NULL, NULL, KngtmareInputInfo, KngtmareDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Reactor

static struct BurnRomInfo reactorRomDesc[] = {
	{ "rom7",		0x1000, 0xa62d86fd, 1 | BRF_PRG | BRF_ESS }, //  0 i8080 Code
	{ "rom6",		0x1000, 0x6ed841f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom5",		0x1000, 0xd90576a3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom4",		0x1000, 0x0155daae, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom3",		0x1000, 0xf8881385, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom2",		0x1000, 0x3caba35b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom1",		0x1000, 0x944e1ddf, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rom0",		0x1000, 0x55930aed, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "snd1",		0x0800, 0xd958a0fd, 2 | BRF_PRG | BRF_ESS }, //  8 m6502 Code
	{ "snd2",		0x0800, 0x5dc86942, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "fg3",		0x1000, 0x8416ad53, 4 | BRF_GRA },           // 10 Sprites
	{ "fg2",		0x1000, 0x5489605a, 4 | BRF_GRA },           // 11
	{ "fg1",		0x1000, 0x18396c57, 4 | BRF_GRA },           // 12
	{ "fg0",		0x1000, 0xd1f20e15, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(reactor)
STD_ROM_FN(reactor)

static INT32 DrvInitReactor()
{
	game_type = 10;
	has_tball = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvReactor = {
	"reactor", NULL, NULL, "reactor", "1982",
	"Reactor\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, reactorRomInfo, reactorRomName, NULL, NULL, reactorSampleInfo, reactorSampleName, ReactorInputInfo, ReactorDIPInfo,
	DrvInitReactor, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Screw Loose (prototype)

static struct BurnRomInfo screwlooRomDesc[] = {
	{ "rom4",			0x2000, 0x744a2513, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "rom3",			0x2000, 0xffde5b5d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom2",			0x2000, 0x97932b05, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom1",			0x2000, 0x571b65ca, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom0",			0x2000, 0x6447fe54, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "drom1",			0x2000, 0xae965ade, 2 | BRF_PRG | BRF_ESS }, //  5 m6502 Code (audio)

	{ "yrom1",			0x2000, 0x3719b0b5, 6 | BRF_PRG | BRF_ESS }, //  6 m6502 Code (speech)

	{ "bg0",			0x1000, 0x1fd5b649, 3 | BRF_GRA },           //  7 Background Tiles
	{ "bg1",			0x1000, 0xc8ddb8ba, 3 | BRF_GRA },           //  8

	{ "fg3",			0x2000, 0x97d4e63b, 4 | BRF_GRA },           //  9 Sprites
	{ "fg2",			0x2000, 0xf76e56ca, 4 | BRF_GRA },           // 10
	{ "fg1",			0x2000, 0x698c395f, 4 | BRF_GRA },           // 11
	{ "fg0",			0x2000, 0xf23269fb, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(screwloo)
STD_ROM_FN(screwloo)

static INT32 DrvInitScrewloo()
{
	game_type = 11;

	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		tilemap_bank[0] = 1; // ram
	}

	return nRet;
}

struct BurnDriver BurnDrvScrewloo = {
	"screwloo", NULL, NULL, NULL, "1983",
	"Screw Loose (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, screwlooRomInfo, screwlooRomName, NULL, NULL, NULL, NULL, ScrewlooInputInfo, ScrewlooDIPInfo,
	DrvInitScrewloo, DrvExit, Drv2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Wiz Warz (prototype)

static struct BurnRomInfo wizwarzRomDesc[] = {
	{ "gv110_rom4_2764.c16",	0x2000, 0xe4e6c29b, 1 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv110_rom3_2764.c14c15",	0x2000, 0xaa8e0fc4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv110_rom2_2764.c13c14",	0x2000, 0x16c7d8ba, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv110_rom1_2764.c12c13",	0x2000, 0x358895b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gv110_rom0_2764.c11c12",	0x2000, 0xf7157e17, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "gv110_drom1_snd_2732.k2",	0x1000, 0x05ca79da, 2 | BRF_PRG | BRF_ESS }, //  5 m6502 Code (audio)

	{ "gv110_yrom1_snd_2732.n3",	0x1000, 0x1e3de643, 6 | BRF_PRG | BRF_ESS }, //  6 m6502 Code (speech)

	{ "gv110_bg0_2732.e11e12",	0x1000, 0x7437813c, 3 | BRF_GRA },           //  7 Background Tiles
	{ "gv110_bg1_2732.e13",		0x1000, 0x70a54cc5, 3 | BRF_GRA },           //  8

	{ "gv110_fg3_2764.k7k8",	0x2000, 0xce0c3e8b, 4 | BRF_GRA },           //  9 Sprites
	{ "gv110_fg2_2764.k6",		0x2000, 0xe42a166f, 4 | BRF_GRA },           // 10
	{ "gv110_fg1_2764.k5",		0x2000, 0xb947cf84, 4 | BRF_GRA },           // 11
	{ "gv110_fg0_2764.k4",		0x2000, 0xf7ba0fcb, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(wizwarz)
STD_ROM_FN(wizwarz)

static INT32 DrvInitWizwarz()
{
	has_tball = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvWizwarz = {
	"wizwarz", NULL, NULL, NULL, "1984",
	"Wiz Warz (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wizwarzRomInfo, wizwarzRomName, NULL, NULL, NULL, NULL, WizwarzInputInfo, WizwarzDIPInfo,
	DrvInitWizwarz, DrvExit, Drv2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// Video Vince and the Game Factory (prototype)

static struct BurnRomInfo vidvinceRomDesc[] = {
	{ "gv132_ram4_2732.c9c10",	0x1000, 0x67a4927b, 9 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv132_rom4_2764.c16",	0x2000, 0x3c5f39f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv132_rom3_2764.c14c15",	0x2000, 0x3983cb79, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv132_rom2_2764.c13c14",	0x2000, 0x0f5ebab9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gv132_rom1_2764.c12c13",	0x2000, 0xa5bf40b7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gv132_rom0_2764.c11c12",	0x2000, 0x2c02b598, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "gv132_drom_snd_2764.k2",	0x2000, 0x18d9d72f, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code (audio)

	{ "gv132_yrom2_snd_2764.k3",	0x2000, 0xff59f618, 6 | BRF_PRG | BRF_ESS }, //  7 m6502 Code (speech)
	{ "gv132_yrom1_snd_2764.n3",	0x2000, 0xbefa4b97, 6 | BRF_PRG | BRF_ESS }, //  8

	{ "gv132_bg0_2732.e11e12",	0x1000, 0x1521bb4a, 3 | BRF_GRA },           //  9 Background Tiles

	{ "gv132_fg3_2764.k7k8",	0x2000, 0x42a78a52, 4 | BRF_GRA },           // 10 Sprites
	{ "gv132_fg2_2764.k6",		0x2000, 0x8ae428ba, 4 | BRF_GRA },           // 11
	{ "gv132_fg1_2764.k5",		0x2000, 0xea423550, 4 | BRF_GRA },           // 12
	{ "gv132_fg0_2764.k4",		0x2000, 0x74c996a6, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(vidvince)
STD_ROM_FN(vidvince)

static INT32 DrvInitVidvince()
{
	game_type = 13;

	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		tilemap_bank[0] = 0; // tiles
		tilemap_bank[1] = 0; // tiles
		tilemap_bank[2] = 1; // ram
		tilemap_bank[3] = 1; // ram
	}

	return nRet;
}

struct BurnDriver BurnDrvVidvince = {
	"vidvince", NULL, NULL, NULL, "1984",
	"Video Vince and the Game Factory (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vidvinceRomInfo, vidvinceRomName, NULL, NULL, NULL, NULL, VidvinceInputInfo, VidvinceDIPInfo,
	DrvInitVidvince, DrvExit, Drv2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};

// The Three Stooges In Brides Is Brides (set 1)

static struct BurnRomInfo stooges3RomDesc[] = {
	{ "gv113ram.4",			0x1000, 0x533bff2a, 9 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv113rom.4",			0x2000, 0x8b6e52b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv113rom.3",			0x2000, 0xb816d8c4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv113rom.2",			0x2000, 0xb45b2a79, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gv113rom.1",			0x2000, 0x34ab051e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gv113rom.0",			0x2000, 0xab124329, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "drom1",			0x2000, 0x87a9fa10, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code (audio)

	{ "yrom2",			0x2000, 0x90f9c940, 6 | BRF_PRG | BRF_ESS }, //  7 m6502 Code (speech)
	{ "yrom1",			0x2000, 0x55f8ab30, 6 | BRF_PRG | BRF_ESS }, //  8

	// ram-based tiles

	{ "gv113fg3",			0x2000, 0x28071212, 4 | BRF_GRA },           //  9 Sprites
	{ "gv113fg2",			0x2000, 0x9fa3dfde, 4 | BRF_GRA },           // 10
	{ "gv113fg1",			0x2000, 0xfb223854, 4 | BRF_GRA },           // 11
	{ "gv113fg0",			0x2000, 0x95762c53, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(stooges3)
STD_ROM_FN(stooges3)

static INT32 DrvInitStooges3()
{
	game_type = 14;

	return DrvInit();
}

struct BurnDriver BurnDrvStooges3 = {
	"3stooges", NULL, NULL, NULL, "1984",
	"The Three Stooges In Brides Is Brides (set 1)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, stooges3RomInfo, stooges3RomName, NULL, NULL, NULL, NULL, Stooges3InputInfo, Stooges3DIPInfo,
	DrvInitStooges3, DrvExit, Drv2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};


// The Three Stooges In Brides Is Brides (set 2)

static struct BurnRomInfo stooges3aRomDesc[] = {
	{ "gv113ram4.bin",		0x1000, 0xa00365be, 9 | BRF_PRG | BRF_ESS }, //  0 i8088 Code
	{ "gv113rom4.bin",		0x2000, 0xa8f9d51d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gv113rom3.bin",		0x2000, 0x60bda7b6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gv113rom2.bin",		0x2000, 0x9bb95798, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gv113rom1.bin",		0x2000, 0x0a8ce58d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gv113rom0.bin",		0x2000, 0xf245fe18, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "drom1",			0x2000, 0x87a9fa10, 2 | BRF_PRG | BRF_ESS }, //  6 m6502 Code (audio)

	{ "yrom2",			0x2000, 0x90f9c940, 6 | BRF_PRG | BRF_ESS }, //  7 m6502 Code (speech)
	{ "yrom1",			0x2000, 0x55f8ab30, 6 | BRF_PRG | BRF_ESS }, //  8

	// ram-based tiles

	{ "gv113fg3",			0x2000, 0x28071212, 4 | BRF_GRA },           //  9 Sprites
	{ "gv113fg2",			0x2000, 0x9fa3dfde, 4 | BRF_GRA },           // 10
	{ "gv113fg1",			0x2000, 0xfb223854, 4 | BRF_GRA },           // 11
	{ "gv113fg0",			0x2000, 0x95762c53, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(stooges3a)
STD_ROM_FN(stooges3a)

struct BurnDriver BurnDrvStooges3a = {
	"3stoogesa", "3stooges", NULL, NULL, "1984",
	"The Three Stooges In Brides Is Brides (set 2)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, stooges3aRomInfo, stooges3aRomName, NULL, NULL, NULL, NULL, Stooges3InputInfo, Stooges3DIPInfo,
	DrvInitStooges3, DrvExit, Drv2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 240, 4, 3
};
