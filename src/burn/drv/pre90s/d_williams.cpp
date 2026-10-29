// FB Neo Williams Games driver module
// Based on MAME driver by Aaron Giles

// Works:
//  Defender
//  Stargate
//  Joust
//  Robotron 2084
//  Bubbles
//  Colony 7
//  Mayday
//  Jin
//  Splat
//  Alien Area - no sound (there is none)
//  Sinistar
//  Playball!
//  Blaster
//  Speed Ball
//  Lottofun
//  Inferno
//  Mystic Marathon
//  Turkey Shoot
//  Joust 2

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "dac.h"
#include "hc55516.h"
#include "midcvsd.h"
#include "burn_ym2151.h"
#include "timer.h"
#include "watchdog.h"
#include "6821pia.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6800ROM0;
static UINT8 *DrvM6800ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvTileROM;
static UINT8 *DrvBgROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6800RAM0;
static UINT8 *DrvM6800RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvBgRAM;

static UINT8 *DrvBlitRAM; // 8 bytes
static UINT8 *DrvPalWritten;
static UINT8 *blitter_remap; // 256*256

static UINT32 *Palette;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

// williams 2 config
static INT32 williams2 = 0;
static INT32 is_joust2 = 0;
static INT32 is_tshoot = 0;
static INT32 is_mysticm = 0;
static INT32 has_s11_bg = 0; // joust2 bg sound board
static INT32 w2_bg_color = 0;
static INT32 has_second_soundboard = 0;
static INT32 is_blasterkit = 0;
static INT32 w2_fg_color = 0;
static INT32 w2_bg_palette_offset = 0;
static INT32 w2_xscroll = 0;
static INT32 w2_numtiles = 0;
static INT32 w2_rom_view_active = 0;
static INT32 w2_palette_view_active = 0;
static INT32 w2_bank_entry = 0;
static UINT8 w2_bank_select_shadow = 0;
static UINT8 w2_fg_select_shadow = 0;
static UINT8 w2_bg_select_shadow = 0;
static UINT8 w2_xscroll_low_shadow = 0;
static UINT8 w2_xscroll_high_shadow = 0;
static UINT8 w2_video_control_shadow = 0;
static UINT8 w2_window_enable_shadow = 0;
static UINT8 w2_watchdog_shadow = 0;

static INT32 nExtraCycles[3];

static UINT8 blitter_xor = 0;
static INT32 blitter_window_enable = 0;
static INT32 blitter_clip_address = ~0;
static INT32 blitter_remap_index = 0;

static UINT8 blaster_video_control = 0;
static INT32 blaster_color0 = 0;

static UINT8 cocktail = 0;
static UINT8 bankselect = 0;
static UINT8 vram_select = 0;
static UINT8 port_select = 0;
static UINT8 rom_bank = 0;

static INT32 screen_x_adjust;
static INT32 scanline;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvInputs[7];
static UINT8 DrvDips[3] = { 0, 0, 0 };
static UINT8 DrvReset;
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static UINT8 lotto_ticket;
static UINT8 lotto_motor;
static INT32 TrackX[2] = { 0, 0 };
static INT32 TrackY[2] = { 0, 0 };
static INT32 conquest_dial = 0;

static ButtonToggle MemoryProtect; // lottofun

static INT32 defender_control_hack = 0;
static INT32 defender = 0;
static INT32 mayday = 0;
static INT32 splat = 0;
static INT32 blaster = 0;
static INT32 sinistarc = 0;
static INT32 playball = 0;
static INT32 spdball = 0;
static INT32 lottofun = 0;
static INT32 conquest = 0;

static INT32 uses_hc55516 = 0;
static INT32 uses_colprom = 0;

// raster update helpers
static INT32 lastline;
static void (*pStartDraw)() = NULL;
static void (*pDrawScanline)() = NULL;

static struct BurnInputInfo DefenderInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy7 + 6,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy7 + 1,	"p1 right"	},
	{"Fire",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Smart Bomb",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"Hyperspace",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"Thrust",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 4"	},
	{"Reverse",					BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 5"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Defender)

static struct BurnInputInfo MaydayInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"Fire",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Mayday",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"Back Fire",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Credit",				BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service3"	},
};

STDINPUTINFO(Mayday)

static struct BurnInputInfo Colony7InputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},

	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 5,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		}, // b
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",					BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Colony7)

static struct BurnInputInfo JinInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		}, // 9
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",					BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Jin)

static struct BurnInputInfo StargateInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy7 + 6,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy7 + 1,	"p1 right"	},
	{"Fire",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Smart Bomb",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"Inviso",					BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},
	{"Hyperspace",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"Thrust",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 5"	},
	{"Reverse",					BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 6"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Stargate)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ConquestInputList[] = {
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"Fire",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Thrust",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	A("Dial",					BIT_ANALOG_REL,	&DrvAnalogPort0,	"p1 dial"),

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
};
#undef A

STDINPUTINFO(Conquest)

static struct BurnInputInfo InfernoInputList[] = {
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy4 + 0,	"p1 up (left stick)"	},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy4 + 3,	"p1 down (left stick)"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy4 + 1,	"p1 left (left stick)"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy4 + 2,	"p1 right (left stick)"	},
	{"P1 Up 2",					BIT_DIGITAL,	DrvJoy4 + 4,	"p1 up (right stick)"	},
	{"P1 Down 2",				BIT_DIGITAL,	DrvJoy4 + 7,	"p1 down (right stick)"	},
	{"P1 Left 2",				BIT_DIGITAL,	DrvJoy4 + 5,	"p1 left (right stick)"	},
	{"P1 Right 2",				BIT_DIGITAL,	DrvJoy4 + 6,	"p1 right (right stick)"	},

	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up (left stick)"	},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy5 + 3,	"p2 down (left stick)"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy5 + 1,	"p2 left (left stick)"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy5 + 2,	"p2 right (left stick)"	},
	{"P2 Up 2",					BIT_DIGITAL,	DrvJoy5 + 4,	"p2 up (right stick)"	},
	{"P2 Down 2",				BIT_DIGITAL,	DrvJoy5 + 7,	"p2 down (right stick)"	},
	{"P2 Left 2",				BIT_DIGITAL,	DrvJoy5 + 5,	"p2 left (right stick)"	},
	{"P2 Right 2",				BIT_DIGITAL,	DrvJoy5 + 6,	"p2 right (right stick)"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy2 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 3,	"p1 coin"	},
	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P3 Coin",					BIT_DIGITAL,	DrvJoy2 + 5,	"p3 coin"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 6,	"tilt"		},
};

STDINPUTINFO(Inferno)

static struct BurnInputInfo Joust2InputList[] = {
	{"P1 Start/Transform",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Flap",					BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 1"	},

	{"P2 Start/Transform",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy5 + 0,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Flap",					BIT_DIGITAL,	DrvJoy5 + 2,	"p2 fire 1"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 2,	"service3"	},
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"Coin 3",					BIT_DIGITAL,	DrvJoy3 + 5,	"service2"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Joust2)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TshootInputList[] = {
	{"Start 1",					BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"Start 2",					BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"Grenade",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"Gobble",					BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},
	{"Fire",					BIT_DIGITAL,	DrvJoy4 + 7,	"p1 fire 1"	},

	A("Gun X",                  BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("Gun Y",                  BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 2,	"service3"	},
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"Coin 3",					BIT_DIGITAL,	DrvJoy3 + 5,	"service2"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};
#undef A

STDINPUTINFO(Tshoot)

static struct BurnInputInfo MysticmInputList[] = {
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 2,	"service3"	},
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"Coin 3",					BIT_DIGITAL,	DrvJoy3 + 5,	"service2"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Mysticm)

static struct BurnInputInfo RobotronInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"Move Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"Move Down",				BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"Move Left",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"Move Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"Fire Up",					BIT_DIGITAL,	DrvJoy1 + 6,	"p2 up"		},
	{"Fire Down",				BIT_DIGITAL,	DrvJoy1 + 7,	"p2 down"	},
	{"Fire Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"Fire Right",				BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Robotron)

static struct BurnInputInfo BubblesInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"Up",						BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"Right",					BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Bubbles)

static struct BurnInputInfo PlayballInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Playball)

static struct BurnInputInfo SplatInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},

	{"P1 Left Stick Up",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 up"		},
	{"P1 Left Stick Down",		BIT_DIGITAL,	DrvJoy5 + 1,	"p1 down"	},
	{"P1 Left Stick Left",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 left"	},
	{"P1 Left Stick Right",     BIT_DIGITAL,	DrvJoy5 + 3,	"p1 right"	},
	{"P1 Right Stick Up",		BIT_DIGITAL,	DrvJoy5 + 6,	"p3 up"		},
	{"P1 Right Stick Down",		BIT_DIGITAL,	DrvJoy5 + 7,	"p3 down"	},
	{"P1 Right Stick Left",		BIT_DIGITAL,	DrvJoy7 + 0,	"p3 left"	},
	{"P1 Right Stick Right",	BIT_DIGITAL,	DrvJoy7 + 1,	"p3 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Left Stick Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Left Stick Down",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 down"	},
	{"P2 Left Stick Left",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Left Stick Right",	    BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	{"P2 Right Stick Up",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 up"		},
	{"P2 Right Stick Down",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 down"	},
	{"P2 Right Stick Left",		BIT_DIGITAL,	DrvJoy6 + 0,	"p4 left"	},
	{"P2 Right Stick Right",	BIT_DIGITAL,	DrvJoy6 + 1,	"p4 right"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Splat)

static struct BurnInputInfo JoustInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy5 + 0,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy5 + 1,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy5 + 2,	"p1 fire 1"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy4 + 2,	"p2 fire 1"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Joust)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SpdballInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy5 + 6,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy4 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy4 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy5 + 0,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy5 + 1,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 1"	},

	A("P1 Stick X",             BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",             BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy5 + 7,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy5 + 2,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy5 + 3,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},

	A("P2 Stick X",             BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Stick Y",             BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};
#undef A
STDINPUTINFO(Spdball)

static struct BurnInputInfo AlienarInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy5 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy5 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy5 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy5 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy5 + 6,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy5 + 7,	"p1 fire 2"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy4 + 1,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Button 2",				BIT_DIGITAL,	DrvJoy4 + 7,	"p2 fire 2"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Alienar)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SinistarInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	A("P1 Stick X",             BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",             BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};
#undef A

STDINPUTINFO(Sinistar)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BlasterInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},

	A("P1 Stick X",             BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",             BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};
#undef A

STDINPUTINFO(Blaster)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BlasterkitInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"Coin 3",					BIT_DIGITAL,	DrvJoy3 + 2,	"service2"	},

	{"P1 Start",				BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 3"	},

	A("P1 Stick X",             BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",             BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Auto Up / Manual Down",	BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};
#undef A

STDINPUTINFO(Blasterkit)

static struct BurnInputInfo LottofunInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"Up",						BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"Right",					BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Memory Protect",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Advance",					BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"High Score Reset",		BIT_DIGITAL,	DrvJoy3 + 3,	"service3"	},
	{"Tilt",					BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
};

STDINPUTINFO(Lottofun)

static struct BurnDIPInfo Colony7DIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL					},
	{0x0c, 0xff, 0xff, 0x00, NULL					},
	{0x0d, 0xff, 0xff, 0x01, NULL					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x0d, 0x01, 0x01, 0x00, "2"					},
	{0x0d, 0x01, 0x01, 0x01, "3"					},

	{0   , 0xfe, 0   ,    4, "Bonus At"				},
	{0x0d, 0x01, 0x02, 0x00, "20k/40k"				},
	{0x0d, 0x01, 0x02, 0x02, "30k/50k"				},
	{0x0d, 0x01, 0x02, 0x00, "30k/50k"				},
	{0x0d, 0x01, 0x02, 0x02, "40k/70k"				},
};

STDDIPINFO(Colony7)

static struct BurnDIPInfo JinDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL					},
	{0x0a, 0xff, 0xff, 0x60, NULL					},
	{0x0b, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x0a, 0x01, 0x07, 0x07, "A 2C/1C B 1C/3C"		},
	{0x0a, 0x01, 0x07, 0x06, "A 2C/1C B 1C/1C"		},
	{0x0a, 0x01, 0x07, 0x05, "A 1C/3C B 1C/3C"		},
	{0x0a, 0x01, 0x07, 0x04, "A 1C/3C B 1C/1C"		},
	{0x0a, 0x01, 0x07, 0x03, "A 1C/2C B 1C/3C"		},
	{0x0a, 0x01, 0x07, 0x02, "A 1C/2C B 1C/1C"		},
	{0x0a, 0x01, 0x07, 0x01, "A 1C/1C B 1C/3C"		},
	{0x0a, 0x01, 0x07, 0x00, "A 1C/1C B 1C/1C"		},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x0a, 0x01, 0x08, 0x08, "4"					},
	{0x0a, 0x01, 0x08, 0x00, "3"					},

	{0   , 0xfe, 0   ,    4, "Level completed"		},
	{0x0a, 0x01, 0x60, 0x60, "85%"					},
	{0x0a, 0x01, 0x60, 0x40, "75%"					},
	{0x0a, 0x01, 0x60, 0x20, "65%"					},
	{0x0a, 0x01, 0x60, 0x00, "55%"					},
};

STDDIPINFO(Jin)

enum
{
	//controlbyte (0xCA00) bit definitions
	WMS_BLITTER_CONTROLBYTE_NO_EVEN = 0x80,
	WMS_BLITTER_CONTROLBYTE_NO_ODD = 0x40,
	WMS_BLITTER_CONTROLBYTE_SHIFT = 0x20,
	WMS_BLITTER_CONTROLBYTE_SOLID = 0x10,
	WMS_BLITTER_CONTROLBYTE_FOREGROUND_ONLY = 0x08,
	WMS_BLITTER_CONTROLBYTE_SLOW = 0x04, //2us blits instead of 1us
	WMS_BLITTER_CONTROLBYTE_DST_STRIDE_256 = 0x02,
	WMS_BLITTER_CONTROLBYTE_SRC_STRIDE_256 = 0x01
};

static void blit_pixel(INT32 dstaddr, INT32 srcdata, INT32 controlbyte)
{
	INT32 curpix = (dstaddr < 0xc000) ? DrvVidRAM[dstaddr] : M6809ReadByte(dstaddr);   //current pixel values at dest

	INT32 solid = DrvBlitRAM[1];
	UINT8 keepmask = 0xff;

	if((controlbyte & WMS_BLITTER_CONTROLBYTE_FOREGROUND_ONLY) && !(srcdata & 0xf0))
	{
		if(controlbyte & WMS_BLITTER_CONTROLBYTE_NO_EVEN)
			keepmask &= 0x0f;
	}
	else
	{
		if(!(controlbyte & WMS_BLITTER_CONTROLBYTE_NO_EVEN))
			keepmask &= 0x0f;
	}

	if((controlbyte & WMS_BLITTER_CONTROLBYTE_FOREGROUND_ONLY) && !(srcdata & 0x0f))
	{
		if(controlbyte & WMS_BLITTER_CONTROLBYTE_NO_ODD)
			keepmask &= 0xf0;
	}
	else
	{
		if(!(controlbyte & WMS_BLITTER_CONTROLBYTE_NO_ODD))
			keepmask &= 0xf0;
	}

	curpix &= keepmask;
	if(controlbyte & WMS_BLITTER_CONTROLBYTE_SOLID)
		curpix |= (solid & ~keepmask);
	else
		curpix |= (srcdata & ~keepmask);

	if (!blitter_window_enable || dstaddr < blitter_clip_address || dstaddr >= 0xc000)
	{
		M6809WriteByte(dstaddr, curpix);
	}
}

static INT32 blitter_core(UINT16 sstart, UINT16 dstart, UINT8 w, UINT8 h, UINT8 controlbyte)
{
	INT32 source, sxadv, syadv;
	INT32 dest, dxadv, dyadv;
	INT32 x, y;
	INT32 accesses = 0;
	UINT8 *remap_ptr = blitter_remap + blitter_remap_index * 256;

	sxadv = (controlbyte & WMS_BLITTER_CONTROLBYTE_SRC_STRIDE_256) ? 0x100 : 1;
	syadv = (controlbyte & WMS_BLITTER_CONTROLBYTE_SRC_STRIDE_256) ? 1 : w;
	dxadv = (controlbyte & WMS_BLITTER_CONTROLBYTE_DST_STRIDE_256) ? 0x100 : 1;
	dyadv = (controlbyte & WMS_BLITTER_CONTROLBYTE_DST_STRIDE_256) ? 1 : w;

	INT32 pixdata=0;

	for (y = 0; y < h; y++)
	{
		source = sstart & 0xffff;
		dest = dstart & 0xffff;

		for (x = 0; x < w; x++)
		{
			if (!(controlbyte & WMS_BLITTER_CONTROLBYTE_SHIFT)) //no shift
			{
				blit_pixel(dest, remap_ptr[M6809ReadByte(source)], controlbyte);
			}
			else
			{   //shift one pixel right
				pixdata = (pixdata << 8) | remap_ptr[M6809ReadByte(source)];
				blit_pixel(dest, (pixdata >> 4) & 0xff, controlbyte);
			}
			accesses += 2;

			source = (source + sxadv) & 0xffff;
			dest   = (dest + dxadv) & 0xffff;
		}

		if (controlbyte & WMS_BLITTER_CONTROLBYTE_DST_STRIDE_256)
			dstart = (dstart & 0xff00) | ((dstart + dyadv) & 0xff);
		else
			dstart += dyadv;

		if (controlbyte & WMS_BLITTER_CONTROLBYTE_SRC_STRIDE_256)
			sstart = (sstart & 0xff00) | ((sstart + syadv) & 0xff);
		else
			sstart += syadv;
	}
	return accesses;
}

static void williams_blitter_write(INT32 offset, UINT8 data)
{
	offset &= 7;

	INT32 sstart, dstart, w, h, accesses;
	INT32 estimated_clocks_at_4MHz;

	DrvBlitRAM[offset] = data;

	if (offset != 0)
		return;

	sstart = (DrvBlitRAM[2] << 8) + DrvBlitRAM[3];
	dstart = (DrvBlitRAM[4] << 8) + DrvBlitRAM[5];

	w = DrvBlitRAM[6] ^ blitter_xor;
	h = DrvBlitRAM[7] ^ blitter_xor;

	if (w == 0) w = 1;
	if (h == 0) h = 1;

	accesses = blitter_core(sstart, dstart, w, h, data);


	if(data & WMS_BLITTER_CONTROLBYTE_SLOW)
	{
		estimated_clocks_at_4MHz = 4 + 4 * (accesses + 2);
	}
	else
	{
		estimated_clocks_at_4MHz = 4 + 2 * (accesses + 3);
	}

	if (williams2) {
		M6809EatCycles((estimated_clocks_at_4MHz + 3) / 4); // the cpu is halted while the blitter runs
	}
}

static void defender_bank_write(UINT16 address, UINT8 data)
{
	//bprintf (0, _T("BW: %4.4x %2.2x\n"), address, data);

	if (address == 0x03ff) {
		if (data == 0x39) {
			BurnWatchdogWrite();
			bprintf(0, _T("Watchdog Write.    **\n"));
		}
		return;
	}
	
	if ((address & 0xfc10) == 0x0000) {
		DrvPalRAM[address & 0x0f] = data;
		return;
	}

	if ((address & 0xfc10) == 0x0010) {
		cocktail = data & 0x01;
		return;
	}

	if ((address & 0xfc00) == 0x0400) {
		DrvNVRAM[address & 0xff] = data | 0xf0;
		return;
	}

	if ((address & 0xfc1c) == 0x0c00) {
		pia_write(1, address & 0x3, data);
		return;
	}

	if ((address & 0xfc1c) == 0x0c04) {
		pia_write(0, address & 0x3, data);
		return;
	}

	bprintf (0, _T("BW: %4.4x %2.2x\n"), address, data);

	if (address >= 0xa000) return; // nop
}

static UINT8 defender_bank_read(UINT16 address)
{
//	bprintf (0, _T("BR: %4.4x\n"), address);
	
	if ((address & 0xfc00) == 0x0400) {
		return DrvNVRAM[address & 0xff];
	}

	if ((address & 0xfc00) == 0x0800) {
		return (scanline < 0x100) ? (scanline & 0xfc) : 0xfc;
	}

	if ((address & 0xfc1c) == 0x0c00) {
		return pia_read(1, address & 3);
	}
	
	if ((address & 0xfc1c) == 0x0c04) {
		return pia_read(0, address & 3);
	}

	if (address >= 0x1000 && address <= 0x9fff) {
		return DrvM6809ROM0[0x10000 + address];
	}

	if (address >= 0xa000) {
		return 0;
	}

	bprintf (0, _T("BR: %4.4x\n"), address);

	return 0;
}

static void bankswitch()
{
	if (bankselect >= 1 && bankselect <= 9) {
		M6809MapMemory(DrvM6809ROM0 + 0x10000 + (bankselect - 1) * 0x1000, 0xc000, 0xcfff, MAP_ROM);
	} else {
		M6809UnmapMemory(0xc000, 0xcfff, MAP_RAM);
	}
}

static void defender_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xc000) {
		defender_bank_write((address & 0xfff) + (bankselect * 0x1000), data);
		return;
	}

	if ((address & 0xf000) == 0xd000) {
		bankselect = data & 0xf;
		bankswitch();

		// mayday
		if (DrvM6809ROM0[0xf7b5] == 0xff && DrvM6809ROM0[0xf7b6] == 0xff) {
		   // bprintf (0, _T("Mayday prot: %3.3x, %2.2x\n"), address & 0xfff, data);
			//if (data == 0x01) {
			//	memset (DrvM6809ROM0 + 0xf7ad, 0, 6);
			//}
			// else {
			//	static const UINT8 test_data[8] = {
			//		0x31, 0x62, 0xE2, 0xA1, 0x91, 0x09, 0xC0, 0x00
			//	};
			//	memcpy (DrvM6809ROM0 + 0xf7ad, test_data, 2);
			//}
		}
		return;
	}
}

static UINT8 defender_main_read(UINT16 address)
{
	if (address >= 0x0000 && address <= 0xbfff) { // Mayday prot
		if (mayday && address >= 0xa190 && address <= 0xa191) { // Mayday prot
			bprintf(0, _T("read mayday prot: %X.\n"), address);
			return DrvVidRAM[address + 3];
		}
		return DrvVidRAM[address];
	}

	if ((address & 0xf000) == 0xc000) {
		return defender_bank_read((address & 0xfff) + (bankselect * 0x1000));
	}

	return 0;
}

static void williams_bank()
{
	if (vram_select == 0)
	{
		M6809MapMemory(DrvVidRAM,				0x0000, 0x8fff, MAP_RAM);
	}
	else
	{	
		M6809MapMemory(DrvM6809ROM0 + 0x10000,	0x0000, 0x8fff, MAP_ROM);
	}
}

static void williams_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xc000) {
		DrvPalRAM[address & 0xf] = data;
		return;
	}

	if ((address & 0xfc00) == 0xcc00) {
		DrvNVRAM[address & 0x3ff] = data | 0xf0;
		return;
	}

	if ((address & 0xff00) == 0xc900) {
		vram_select = data & 1;
		cocktail = data & 2;
		if (blitter_clip_address == 0x7400) blitter_window_enable = data & 4; // sinistar
		williams_bank();
		return;
	}

	if ((address & 0xff0c) == 0xc804) {
		pia_write(0, address & 0x3, data);
		return;
	}

	if ((address & 0xff0c) == 0xc808) { // spdball
		pia_write(3, address & 0x3, data);
		return;
	}

	if ((address & 0xff0c) == 0xc80c) {
		pia_write(1, address & 0x3, data);
		return;
	}

	if ((address & 0xff00) == 0xca00) {
		williams_blitter_write(address, data);
		return;
	}

	switch (address)
	{
		case 0xcbff:
			if (data == 0x39) {
				BurnWatchdogWrite();
			}
		return;
	}

	// sinistar debug derp spew writes @ 0xe000 - 0xffff
	if ((address & 0xe000) != 0xe000) bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);
}

static UINT8 williams_main_read(UINT16 address)
{
	if ((address & 0xfffc) == 0xc800) {
		switch (address & 0x03) {
			case 0: return TrackY[0]&0xff;
			case 1: return TrackX[0]&0xff;
			case 2: return TrackY[1]&0xff;
			case 3: return TrackX[1]&0xff;
		}

		return 0; // spdball analog input read
	}

	if ((address & 0xff0c) == 0xc804) {
		return pia_read(0, address & 3);
	}

	if ((address & 0xff0c) == 0xc808) { // spdball
		return pia_read(3, address & 3);
	}

	if ((address & 0xff0c) == 0xc80c) {
		return pia_read(1, address & 3);
	}

	if ((address & 0xff00) == 0xca00) {
		return DrvBlitRAM[address & 7];
	}

	if ((address & 0xff00) == 0xcb00) {
		return (scanline < 0x100) ? (scanline & 0xfc) : 0xfc;
	}

	if ((address & 0xfc00) == 0xc000) { // palette read, maybe? (sinistar)
		return DrvPalRAM[address & 0xf];
	}

	if ((address & 0xff00) == 0xc900) return 0; // NOP

	bprintf (0, _T("MR: %4.4x\n"), address);

	return 0;
}

static INT32 w2_tile_rom_len = 0;
static INT32 bg_rom_len = 0;

static INT32 DrvGfxDecode()
{
	INT32 thirdBits = (w2_tile_rom_len * 8) / 3;

	w2_numtiles = (thirdBits / 8) / 64;
	if (w2_numtiles > 256) w2_numtiles = 256;

	INT32 Plane[4] = { STEP4(0,1) };
	INT32 XOffs[24] = {
		0+0*8+0*thirdBits, 4+0*8+0*thirdBits, 0+0*8+1*thirdBits, 4+0*8+1*thirdBits, 0+0*8+2*thirdBits, 4+0*8+2*thirdBits,
		0+1*8+0*thirdBits, 4+1*8+0*thirdBits, 0+1*8+1*thirdBits, 4+1*8+1*thirdBits, 0+1*8+2*thirdBits, 4+1*8+2*thirdBits,
		0+2*8+0*thirdBits, 4+2*8+0*thirdBits, 0+2*8+1*thirdBits, 4+2*8+1*thirdBits, 0+2*8+2*thirdBits, 4+2*8+2*thirdBits,
		0+3*8+0*thirdBits, 4+3*8+0*thirdBits, 0+3*8+1*thirdBits, 4+3*8+1*thirdBits, 0+3*8+2*thirdBits, 4+3*8+2*thirdBits
	};
	INT32 YOffs[16] = { STEP16(0,4*8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}
	memcpy (tmp, DrvTileROM, 0x18000);

	GfxDecode(w2_numtiles, 4, 24, 16, Plane, XOffs, YOffs, 4*16*8, tmp, DrvTileROM);

	BurnFree(tmp);

	return 0;
}

static UINT32 w2_calc_col(INT32 lo, INT32 hi)
{
	static const float frgb[256] =
	{
		0.001889f, 0.002456f, 0.003196f, 0.004384f, 0.005272f, 0.007672f, 0.011345f, 0.017759f,
		0.018643f, 0.03103f,  0.051912f, 0.087251f, 0.1256f,   0.204967f, 0.331644f, 0.529628f,
		0.002524f, 0.003306f, 0.004371f, 0.006072f, 0.007386f, 0.010812f, 0.016096f, 0.02509f,
		0.02687f,  0.043747f, 0.070899f, 0.114042f, 0.159992f, 0.250108f, 0.391104f, 0.607433f,
		0.003522f, 0.004666f, 0.006254f, 0.008766f, 0.010795f, 0.015856f, 0.023585f, 0.036228f,
		0.039487f, 0.062206f, 0.096916f, 0.148811f, 0.20338f,  0.305447f, 0.462741f, 0.700199f,
		0.004941f, 0.006607f, 0.00895f,  0.012619f, 0.015683f, 0.022944f, 0.033832f, 0.050765f,
		0.056018f, 0.085033f, 0.127452f, 0.187886f, 0.25112f,  0.364998f, 0.53879f,  0.797876f,

		0.006857f, 0.009233f, 0.012592f, 0.017771f, 0.022172f, 0.03212f,  0.046615f, 0.06811f,
		0.075696f, 0.110906f, 0.160678f, 0.229149f, 0.300735f, 0.42593f,  0.61584f,  0.896241f,
		0.010113f, 0.01368f,  0.018726f, 0.02625f,  0.032735f, 0.046462f, 0.065809f, 0.092996f,
		0.103803f, 0.146292f, 0.204648f, 0.282479f, 0.364084f, 0.502781f, 0.712244f, 1.018759f,
		0.015744f, 0.021251f, 0.028944f, 0.039945f, 0.049446f, 0.068127f, 0.093445f, 0.127285f,
		0.14231f,  0.193004f, 0.261153f, 0.349656f, 0.443068f, 0.597642f, 0.830444f, 1.168267f,
		0.024223f, 0.032378f, 0.043467f, 0.058612f, 0.071646f, 0.09563f,  0.127147f, 0.167588f,
		0.187237f, 0.246001f, 0.32398f,  0.423282f, 0.528958f, 0.699986f, 0.957283f, 1.328146f,

		0.03429f,  0.045182f, 0.059626f, 0.078588f, 0.094882f, 0.123449f, 0.160233f, 0.206173f,
		0.230031f, 0.295596f, 0.382005f, 0.490617f, 0.607092f, 0.792615f, 1.071663f, 1.471949f,
		0.053106f, 0.068185f, 0.087712f, 0.112105f, 0.132967f, 0.167685f, 0.211656f, 0.265082f,
		0.295044f, 0.369837f, 0.467993f, 0.58972f,  0.721623f, 0.927771f, 1.23799f,  1.680648f,
		0.083576f, 0.103924f, 0.129726f, 0.160516f, 0.186838f, 0.228722f, 0.281144f, 0.343382f,
		0.381103f, 0.46708f,  0.5795f,   0.717377f, 0.86859f,  1.100548f, 1.449999f, 1.946101f,
		0.124032f, 0.149904f, 0.182177f, 0.219335f, 0.251408f, 0.300484f, 0.361692f, 0.433113f,
		0.479348f, 0.577076f, 0.704911f, 0.860274f, 1.032601f, 1.292736f, 1.685276f, 2.240223f,

		0.175706f, 0.207011f, 0.245689f, 0.289706f, 0.327912f, 0.384501f, 0.455038f, 0.536297f,
		0.592068f, 0.702442f, 0.847309f, 1.021943f, 1.217772f, 1.509243f, 1.949852f, 2.570528f,
		0.24927f,  0.287025f, 0.333877f, 0.385984f, 0.431717f, 0.497443f, 0.579727f, 0.673464f,
		0.741572f, 0.868017f, 1.034559f, 1.234123f, 1.460333f, 1.79207f,  2.295118f, 3.001228f,
		0.357357f, 0.403448f, 0.460931f, 0.52375f,  0.579557f, 0.656404f, 0.754715f, 0.865436f,
		0.950203f, 1.097887f, 1.293872f, 1.527752f, 1.795589f, 2.182008f, 2.770169f, 3.593798f,
		0.494255f, 0.549094f, 0.61884f,  0.693823f, 0.760527f, 0.851544f, 0.968276f, 1.098965f,
		1.203912f, 1.377026f, 1.608183f, 1.882244f, 2.200036f, 2.652345f, 3.342626f, 4.215708f
	};

	// default gain/offset per PORT_ADJUSTER defaults: mysticm uses its own monitor_controls_mysticm
	// (R=80/G=73/B=81 gain, R=73/G=100/B=78 offset); other games use the generic monitor_controls (25/25/25, 100/100/100)
	static const float gain_mysticm[3]   = { 0.80f, 0.73f, 0.81f };
	static const float offset_mysticm[3] = { -0.27f, 0.00f, -0.22f };
	static const float gain_generic[3]   = { 0.25f, 0.25f, 0.25f };
	static const float offset_generic[3] = { 0.00f, 0.00f, 0.00f };
	const float *gain   = is_mysticm ? gain_mysticm   : gain_generic;
	const float *offset = is_mysticm ? offset_mysticm : offset_generic;

	INT32 i  = (hi >> 4) & 15;
	INT32 ub = (hi >> 0) & 15;
	INT32 ug = (lo >> 4) & 15;
	INT32 ur = (lo >> 0) & 15;

	float r = frgb[i * 16 + ur] / 4.22f;
	float g = frgb[i * 16 + ug] / 4.22f;
	float b = frgb[i * 16 + ub] / 4.22f;

	r = (r + offset[0] > 0.0f) ? r + offset[0] : 0.0f;
	g = (g + offset[1] > 0.0f) ? g + offset[1] : 0.0f;
	b = (b + offset[2] > 0.0f) ? b + offset[2] : 0.0f;

	r = r * gain[0] / 0.25f; if (r > 1.0f) r = 1.0f;
	g = g * gain[1] / 0.25f; if (g > 1.0f) g = 1.0f;
	b = b * gain[2] / 0.25f; if (b > 1.0f) b = 1.0f;

	return BurnHighCol((INT32)(r * 255), (INT32)(g * 255), (INT32)(b * 255), 0);
}

static void w2_update_pen(INT32 entry)
{
	if (DrvPalWritten[entry]) {
		DrvPalette[0x10 + entry] = w2_calc_col(DrvPalRAM[entry * 2 + 0], DrvPalRAM[entry * 2 + 1]);
	} else {
		// never written since power on: placeholder colors
		DrvPalette[0x10 + entry] = BurnHighCol((entry & 1) ? 0xff : 0, (entry & 2) ? 0xff : 0, (entry & 4) ? 0xff : 0, 0);
	}
}

static void w2_paletteram_w(INT32 offset)
{
	DrvPalWritten[offset / 2] = 1;
	w2_update_pen(offset / 2);
}

static void w2_rebuild_palette()
{
	for (INT32 i = 0; i < 0x400; i++)
		w2_update_pen(i);
}

// williams2 bank select:
//   page 0    : 0x0000-0x7fff videoram
//   page 1, 2 : 0x0000-0x7fff rom bank (data & 6) >> 1
//   page 3    : 0x0000-0x7fff rom bank (data & 4) >> 1, 0x8000-0x87ff palette ram

static void w2_bank_select(UINT8 data)
{
	INT32 page = data & 3;

	switch (page)
	{
		case 0:
			w2_rom_view_active = 0;
			w2_palette_view_active = 0;
			break;

		case 1:
		case 2:
			w2_bank_entry = (data & 6) >> 1;
			w2_rom_view_active = 1;
			w2_palette_view_active = 0;
			break;

		case 3:
			w2_bank_entry = (data & 4) >> 1;
			w2_rom_view_active = 1;
			w2_palette_view_active = 1;
			break;
	}

	if (w2_rom_view_active) {
		M6809MapMemory(DrvM6809ROM0 + 0x10000 + w2_bank_entry * 0x8000, 0x0000, 0x7fff, MAP_ROM);
	} else {
		M6809MapMemory(DrvVidRAM, 0x0000, 0x7fff, MAP_RAM);
	}

	if (w2_palette_view_active) {
		M6809UnmapMemory(0x8000, 0x87ff, MAP_RAM); // palette ram goes through the handlers
	} else {
		M6809MapMemory(DrvVidRAM + 0x8000, 0x8000, 0x87ff, MAP_RAM);
	}
}

static void williams2_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x8000 && w2_palette_view_active) {
		INT32 offset = (address & 0x7ff);
		DrvPalRAM[offset] = data;
		w2_paletteram_w(offset);
		return;
	}

	// rom view only overlays reads - writes to 0x0000-0x7fff always land in videoram
	if (address < 0x8000) {
		DrvVidRAM[address] = data;
		return;
	}

	if ((address & 0xff80) == 0xc800) {
		w2_bank_select_shadow = data;
		w2_bank_select(data);
		return;
	}

	if ((address & 0xff80) == 0xc880) { // mirror(0x0078)
		williams_blitter_write(address & 7, data);
		return;
	}

	if ((address & 0xff80) == 0xc900) {
		w2_watchdog_shadow = data;
		if ((data & 0x3f) == 0x14) {
			BurnWatchdogWrite();
		}
		return;
	}

	if ((address & 0xff80) == 0xc980) { // mirror(0x0070)
		INT32 sel = address & 0xc;
		if (sel == 0x0) { pia_write(1, address & 0x3, data); return; }
		if (sel == 0x4) { pia_write(0, address & 0x3, data); return; }
		if (sel == 0xc) { return; } // this is a 7-segment led
		return;
	}

	if ((address & 0xffe0) == 0xcb00) {
		w2_fg_select_shadow = data;
		w2_fg_color = data & 0x3f;
		return;
	}

	if ((address & 0xffe0) == 0xcb20) {
		w2_bg_select_shadow = data;
		if (is_mysticm) {
			w2_bg_color = data;
		} else {
			w2_bg_palette_offset = is_joust2 ? ((data & 0x3f) << 4) : ((data & 0x38) << 4);
		}
		return;
	}

	if ((address & 0xffe0) == 0xcb40) {
		w2_xscroll_low_shadow = data;
		w2_xscroll = (w2_xscroll & ~0x00f) | ((data & 0x80) >> 4) | (data & 0x07);
		return;
	}

	if ((address & 0xffe0) == 0xcb60) {
		w2_xscroll_high_shadow = data;
		w2_xscroll = (w2_xscroll & 0x00f) | (data << 4);
		return;
	}

	if ((address & 0xffe0) == 0xcb80) {
		w2_video_control_shadow = data;
		return;
	}

	if ((address & 0xffe0) == 0xcba0) {
		w2_window_enable_shadow = data;
		blitter_window_enable = data & 1;
		return;
	}

	if ((address & 0xfc00) == 0xcc00) {
		DrvNVRAM[address & 0x3ff] = data | 0xf0;
		return;
	}

	if (address >= 0xd000 && address <= 0xdfff) {
		DrvM6809RAM0[address & 0xfff] = data;
		return;
	}
}

static UINT8 williams2_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0x8000 && w2_palette_view_active) {
		return DrvPalRAM[address & 0x7ff];
	}

	// c800-c97f and cb00-cbbf are write-only (unmapped reads)

	if ((address & 0xff80) == 0xc980) { // mirror(0x0070)
		INT32 sel = address & 0xc;
		if (sel == 0x0) return pia_read(1, address & 3);
		if (sel == 0x4) return pia_read(0, address & 3);
		return 0;
	}

	if ((address & 0xfff0) == 0xcbe0) {
		return scanline & 0xff;
	}

	if ((address & 0xfc00) == 0xcc00) {
		return DrvNVRAM[address & 0x3ff];
	}

	if (address >= 0xd000 && address <= 0xdfff) {
		return DrvM6809RAM0[address & 0xfff];
	}

	return 0;
}

static void blaster_bankswitch()
{
	if (vram_select == 0)
	{
		M6809MapMemory(DrvVidRAM,		0x0000, 0x8fff, MAP_RAM);
	}
	else
	{
		M6809MapMemory(DrvM6809ROM0 + 0x18000 + rom_bank * 0x4000, 0x0000, 0x3fff, MAP_ROM);
		M6809MapMemory(DrvM6809ROM0 + 0x10000, 0x4000, 0x8fff, MAP_ROM);
	}
}

static void blaster_main_write(UINT16 address, UINT8 data)
{
		if ((address & 0xffc0) == 0xc900) {
			vram_select = data & 0x01;
			cocktail = data & 0x02;
			blitter_window_enable = data & 0x04;
			blaster_bankswitch();
			return;
		}
		
		if ((address & 0xffc0) == 0xc940) {
			blitter_remap_index = data;
			return;
		}
		
		if ((address & 0xffc0) == 0xc980) {
			rom_bank = data & 0x0f;
			blaster_bankswitch();
			return;
		}
		
		if ((address & 0xffc0) == 0xc9c0) {
			blaster_video_control = data;
			return;
		}

		williams_main_write(address, data);
}

static void defender_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0x7ffc) == 0x0400) {
		pia_write(2, address & 0x3, data);
		return;
	}
}

static UINT8 defender_sound_read(UINT16 address)
{
	if ((address & 0x7ffc) == 0x0400) {
		return pia_read(2, address & 3);
	}

	return 0;
}

static void williams2_sound_write(UINT16 address, UINT8 data)
{
	address &= ~0x1ffc;
	if (address >= 0x2000 && address <= 0x2003) {
		pia_write(2, address & 0x3, data);
		return;
	}
}

static UINT8 williams2_sound_read(UINT16 address)
{
	address &= ~0x1ffc;
	if (address >= 0x2000 && address <= 0x2003) {
		return pia_read(2, address & 3);
	}
	return 0;
}

static void blaster_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0x7ffc) == 0x0400) {
		pia_write(4, address & 0x3, data);
		return;
	}
}

static UINT8 blaster_sound_read(UINT16 address)
{
	if ((address & 0x7ffc) == 0x0400) {
		return pia_read(4, address & 3);
	}

	return 0;
}

static UINT8 pia0_in_a(UINT16 )
{
	return DrvInputs[0];
}

static UINT8 pia0_in_b(UINT16 )
{
	return DrvInputs[1];
}

static UINT8 pia1_in_a(UINT16 )
{
	return DrvInputs[2];
}

static void pia0_muxed_out_b2(UINT16 , UINT8 data)
{
	port_select = data;
}

static UINT8 pia0_muxed_joust_in_a(UINT16 )
{
	return (DrvInputs[0] & 0x30) | ((port_select == 0) ? (DrvInputs[3] & ~0x30) : (DrvInputs[4] & ~0x30));
}

static UINT8 pia0_muxed_joust_in_b(UINT16 )
{
	return (DrvInputs[1] & 0xfc) | ((port_select == 0) ? (DrvInputs[5] & 0x3) : (DrvInputs[6] & 0x3));
}

static UINT8 pia0_49way_in_a(UINT16 )
{
	static const UINT8 translate49[7] = { 0x0, 0x4, 0x6, 0x7, 0xb, 0x9, 0x8 };
	INT16 x = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0x6f);
	INT16 y = ProcessAnalog(DrvAnalogPort1, 1, 1, 0x00, 0x6f);
	return (translate49[x >> 4] << 4) | translate49[y >> 4];
}

static void sync_sound(INT32 num)
{
	INT32 snd_clock = (williams2) ? 1000000 : 894886; // williams2: MASTER_CLOCK/3/4 = 4MHz/4; williams1: 3.579545MHz/4
	INT32 cyc = (INT32)(((double)((double)M6809TotalCycles() * snd_clock) / 1000000)+0.5);
	INT32 todo = cyc - M6800TotalCycles();
	//bprintf(0, _T("m6809 cyc: %d  m6800 cyc: %d.    cyc: %d  todo %d.\n"), M6809TotalCycles(), M6800TotalCycles(), cyc, todo);
	// Adding in a couple cycles to prevent lost soundcommands.  CPU_RUN_SYNCINT in DrvFrame() will compensate.
	if (todo < 1) todo = 15;
	M6800Run(todo + 10);
}

static void pia0_out_b(UINT16 , UINT8 data)
{
	if (lottofun) {
		lotto_motor = ~data & 0x80;
	}
}

static void pia1_out_b(UINT16 , UINT8 data)
{
	if (blaster || sinistarc) {
		// Blaster HW & Sinistar Cockpit
		UINT8 l_data = data | 0x80;
		UINT8 r_data = (data >> 1 & 0x40) | (data & 0x3f) | 0x80;

		if (sinistarc) {
			l_data = data | 0xc0;
			r_data = data | 0xc0;
		}

		M6800Open(0);
		sync_sound(0);
		pia_set_input_b(2, l_data);
		pia_set_input_cb1(2, (l_data == 0xff) ? 0 : 1);
		M6800Close();

		if (has_second_soundboard) {
			M6800Open(1);
			sync_sound(1);
			pia_set_input_b(4, r_data);
			pia_set_input_cb1(4, (r_data == 0xff) ? 0 : 1);
			M6800Close();
		}
	} else {
		// defender, williams HW (everything else)
		M6800Open(0);
		sync_sound(0);
		if (!playball) data |= 0xc0;
		pia_set_input_b(2, data);
		pia_set_input_cb1(2, (data == 0xff) ? 0 : 1);
		M6800Close();
	}
}

static void pia2_out_b(UINT16 , UINT8 data)
{
	DACWrite(0, data);
}

static void w2_dac_w(UINT16 , UINT8 data)
{
	DACWrite(0, data);
}

static void pia2_out_b2(UINT16 , UINT8 data)
{
	DACWrite(1, data);
}

static void hc55516_digit_out(UINT16 , UINT8 data)
{
	hc55516_digit_w(data);
}

static void hc55516_clock_out(UINT16 , UINT8 data)
{
	hc55516_clock_w(data);
}

static void pia1_main_irq(INT32 state)
{
	M6809SetIRQLine(M6809_IRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void mysticm_pia0_firq(INT32 state)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void pia2_sound_irq(INT32 state)
{
	M6800SetIRQLine(M6800_IRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void pia4_sound_irq(INT32 state)
{
	M6800SetIRQLine(M6800_IRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static pia6821_interface pia_0 = {
	pia0_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, pia0_out_b, NULL, NULL,
	NULL, NULL
};

static pia6821_interface pia_muxed_joust_0 = {
	pia0_muxed_joust_in_a, pia0_muxed_joust_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, pia0_muxed_out_b2,
	NULL, NULL
};

static pia6821_interface pia_49way_0 = {
	pia0_49way_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL
};

static UINT8 pia0_blasterkit_muxed_in_a(UINT16 )
{
	return port_select ? pia0_49way_in_a(0) : DrvInputs[3]; // port_select=1: joystick position, =0: IN3 (buttons/start)
}

static void blasterkit_out_cb2(UINT16 , UINT8 data)
{
	port_select = data;
}

static pia6821_interface pia_blasterkit_0 = {
	pia0_blasterkit_muxed_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, blasterkit_out_cb2,
	NULL, NULL
};

static pia6821_interface pia_1 = {
	pia1_in_a, NULL,
	NULL, NULL, NULL, NULL,
	NULL, pia1_out_b, NULL, NULL,
	pia1_main_irq, pia1_main_irq
};

static pia6821_interface pia_2 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia2_out_b, NULL, NULL, NULL,
	pia2_sound_irq, pia2_sound_irq
};

static pia6821_interface pia_4 = { // blaster 2nd soundchip
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia2_out_b2, NULL, NULL, NULL,
	pia4_sound_irq, pia4_sound_irq
};

static pia6821_interface pia_2_sinistar = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia2_out_b, NULL, hc55516_digit_out, hc55516_clock_out,
	pia2_sound_irq, pia2_sound_irq
};

// Williams 2
static UINT8 w2_mux_in_a(UINT16 )
{
	return (port_select == 0) ? DrvInputs[3] : DrvInputs[4];
}

static UINT8 tshoot_gun_r(INT32 axis)
{
	INT16 raw = (axis ? BurnGunReturnX(0) : BurnGunReturnY(0));
	INT32 data = (raw >> 2) & 0x3f; // scale 0-255 down to 0-0x3f
	UINT8 result = (data & 0x3f) ^ ((data & 0x3f) >> 1); // gray-code
	return result;
}

static UINT8 tshoot_mux_in_a(UINT16 )
{
	UINT8 ret = tshoot_gun_r(port_select & 1) | (DrvInputs[3] & 0xc0);
	return ret;
}

static UINT8 joust2_mux_in_a(UINT16 )
{
	return ((DrvInputs[0] & 0xf0) ^ 0xf0) | (w2_mux_in_a(0) & 0x0f);
}

static void w2_out_ca2(UINT16 , UINT8 data)
{
	port_select = data;
}

static void w2_snd_cmd_w(UINT16 , UINT8 data)
{
	M6800Open(0);
	sync_sound(0);
	pia_set_input_a(2, data);
	M6800Close();
}

static void w2_pia1_cb2_to_pia2_ca1(UINT16 , UINT8 data)
{
	M6800Open(0);
	sync_sound(0);
	pia_set_input_ca1(2, data);
	M6800Close();
}

static void w2_pia2_ca2_to_pia1_cb1(UINT16 , UINT8 data)
{
	pia_set_input_cb1(1, data);
}

#define BG_CB2_QUEUE	16
static UINT8 bg_cb2_queue[BG_CB2_QUEUE];
static INT32 bg_cb2_count = 0;
static INT32 bg_resetq_state = 1;

static void joust2_bg_cb2_w(INT32 state)
{
	if (bg_cb2_count < BG_CB2_QUEUE) bg_cb2_queue[bg_cb2_count++] = state;
}

static void joust2_bg_cb2_flush()
{
	for (INT32 i = 0; i < bg_cb2_count; i++) {
		pia_set_input_cb1(2, bg_cb2_queue[i]);
	}
	bg_cb2_count = 0;
}

static void joust2_bg_strobe_w(UINT16 , UINT8 state)
{
	cvsd_cb1_write(state);
}

static void joust2_bg_resetq_w(UINT16 , UINT8 state)
{
	if (bg_resetq_state && !state) {
		cvsd_cpu_reset();
	}
	bg_resetq_state = state;
}

static void pia2a_to_pia1_out_b(UINT16 , UINT8 data)
{
	pia_set_input_b(1, data);
}

static pia6821_interface pia_w2_0 = {
	w2_mux_in_a, pia0_in_a,
	NULL, NULL, NULL, NULL,
	NULL, NULL, w2_out_ca2, NULL,
	NULL, NULL
};

static pia6821_interface pia_tshoot_0 = {
	tshoot_mux_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, w2_out_ca2, NULL,
	pia1_main_irq, pia1_main_irq
};

static pia6821_interface pia_mysticm_0 = {
	pia0_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	mysticm_pia0_firq, pia1_main_irq
};

static pia6821_interface pia_joust2_0 = {
	joust2_mux_in_a, pia0_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, w2_out_ca2, NULL,
	NULL, NULL
};

static pia6821_interface pia_w2_1 = {
	pia0_in_b, NULL,
	NULL, NULL, NULL, NULL,
	NULL, w2_snd_cmd_w, NULL, w2_pia1_cb2_to_pia2_ca1,
	pia1_main_irq, pia1_main_irq
};

static pia6821_interface pia_tshoot_1 = {
	pia1_in_a, NULL,
	NULL, NULL, NULL, NULL,
	NULL, w2_snd_cmd_w, NULL, w2_pia1_cb2_to_pia2_ca1,
	pia1_main_irq, pia1_main_irq
};

static pia6821_interface pia_mysticm_1 = {
	pia1_in_a, NULL,
	NULL, NULL, NULL, NULL,
	NULL, w2_snd_cmd_w, NULL, w2_pia1_cb2_to_pia2_ca1,
	pia1_main_irq, pia1_main_irq
};

static void joust2_snd_cmd_w(UINT16 address, UINT8 data)
{
	w2_snd_cmd_w(address, data);
	cvsd_portb_write(data); // goes to the sound cpu and to the bg board
}

static pia6821_interface pia_joust2_1 = {
	pia1_in_a, NULL,
	NULL, NULL, NULL, NULL,
	NULL, joust2_snd_cmd_w, joust2_bg_strobe_w, w2_pia1_cb2_to_pia2_ca1,
	pia1_main_irq, pia1_main_irq
};

static pia6821_interface pia_w2_2 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia2a_to_pia1_out_b, w2_dac_w, w2_pia2_ca2_to_pia1_cb1, NULL,
	pia2_sound_irq, pia2_sound_irq
};

static pia6821_interface pia_joust2_2 = {
	NULL, NULL,
	NULL, NULL, NULL, NULL,
	pia2a_to_pia1_out_b, w2_dac_w, w2_pia2_ca2_to_pia1_cb1, joust2_bg_resetq_w,
	pia2_sound_irq, pia2_sound_irq
};

static UINT8 pia3_in_a(UINT16 )
{
	return DrvInputs[3];
}

static UINT8 pia3_in_b(UINT16 )
{
	return DrvInputs[4];
}

static pia6821_interface pia_3 = {
	pia3_in_a, pia3_in_b,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL
};

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
		if (williams2) w2_rebuild_palette();
	}

	M6809Open(0);
	M6809Reset();
	if (williams2) {
		w2_bank_select(0);
	}
	M6809Close();

	M6800Open(0);
	M6800Reset();
	M6800Close();

	if ((blaster || sinistarc) && has_second_soundboard) {
		M6800Open(1);
		M6800Reset();
		M6800Close();
	}

	pia_reset();

	BurnWatchdogReset();

	DACReset();

	if (uses_hc55516)
		hc55516_reset();

	if (has_s11_bg) {
		cvsd_reset();
		bg_cb2_count = 0;
		bg_resetq_state = 1;
	}

	cocktail = 0;
	bankselect = 0;
	vram_select = 0;
	port_select = 0;
	rom_bank = 0;
	blaster_video_control = 0;
	blaster_color0 = 0;

	lotto_motor = 0;
	lotto_ticket = 20;

	TrackX[0] = TrackX[1] = 0;
	TrackY[0] = TrackY[1] = 0;
	conquest_dial = 0;

	w2_fg_color = 0;
	w2_bg_palette_offset = 0;
	w2_xscroll = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x050000;
	DrvM6800ROM0		= Next; Next += 0x010000;
	DrvM6800ROM1		= Next; Next += 0x010000;

	DrvGfxROM			= Next; Next += 0x018000;

	DrvColPROM			= Next; Next += 0x001000;

	DrvTileROM			= Next; Next += 0x018000; // up to 256 tiles * 384 bytes (joust2 has double-size tile ROMs -> 256 tiles)

	DrvBgROM			= Next; Next += 0x080000; // bg board, laid out as U4 / U19 / U20 at 0x00000 / 0x20000 / 0x40000

	Palette				= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);
	DrvPalette			= (UINT32*)Next; Next += (0x0010 + 0x0400) * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x000400;

	blitter_remap		= Next; Next += 0x010000;

	AllRam				= Next;

	DrvM6809RAM0		= Next; Next += 0x004000;
	DrvM6800RAM0		= Next; Next += 0x000100;
	DrvM6800RAM1		= Next; Next += 0x000100;
	DrvVidRAM			= Next; Next += 0x00c000;
	DrvPalRAM			= Next; Next += 0x000800;
	DrvBlitRAM			= Next; Next += 0x000008;
	DrvTileRAM			= Next; Next += 0x000800;
	DrvPalWritten		= Next; Next += 0x000400;
	DrvBgRAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvRomLoad(INT32 type) // 1-defender, 2-mysticm, 3-tshoot, 4-sinistar, 0-blaster
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *mLoad = DrvM6809ROM0 + 0xd000;
	UINT8 *cLoad = DrvColPROM;
	UINT8 *gLoad = DrvGfxROM;

	has_second_soundboard = 0;
	bg_rom_len = 0;

	if (type == 4) mLoad += 0x1000;
	
	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			INT32 offset = mLoad - DrvM6809ROM0;

			// blaster
			if (ri.nLen == 0x4000 && offset == 0x15000) mLoad += 0x3000; // gap
			// conquest (incomplete prototype dump - 0xd000-0xdfff is unpopulated on PCB)
			if (type == 6 && offset == 0xd000) mLoad += 0x1000; // gap
			// williams2/inferno: fixed window starts at 0xe000 (not 0xd000), and bank 1 (0x18000-0x1ffff) is unpopulated
			if (type == 7 && offset == 0xd000) mLoad += 0x1000; // gap
			if (type == 7 && offset == 0x18000) mLoad += 0x8000; // gap (empty bank)
			// mysticm: fixed window starts at 0xe000 (like inferno); bank1 has only 3 of 4 chips
			if (type == 9 && offset == 0xd000) mLoad += 0x1000; // gap
			if (type == 9 && offset == 0x1e000) mLoad += 0x2000; // gap
			// joust2: 0xd000-0xdfff is real ROM (unlike inferno); bank2 (0x20000-0x27fff) only has 3 of 4 chips
			if (type == 8 && offset == 0x26000) mLoad += 0x2000; // gap
			if (type == 2) {
				if (offset == 0x18000) mLoad += 0x8000;
				if (offset == 0x24000) mLoad += 0xc000;
				if (offset == 0x38000) mLoad += 0x8000;
			}
			if (type == 3) {
				// tshoot: bank1 (0x18000-0x1ffff) only has 2 of 4 chips - second half is empty
				if (offset == 0x1c000) mLoad += 0x4000;
			}
			//bprintf(0, _T("loading main ROM @ %04X (%X bytes)\n"), offset, ri.nLen);
			if (BurnLoadRom(mLoad, i, 1)) return 1;
			mLoad += ri.nLen;

			// defender
			if (type == 1 && (offset + ri.nLen) == 0x13000) mLoad += 0x3000; // gap
			continue;
		}

		if ((ri.nType & 7) == 2) {
			memmove (DrvM6800ROM0, DrvM6800ROM0 + ri.nLen, 0x10000 - ri.nLen); // move back
			if (BurnLoadRom(DrvM6800ROM0 + 0x10000 - ri.nLen, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			bprintf(0, _T("loading second sound rom %x\n"), ri.nLen);
			has_second_soundboard = 1;
			memmove (DrvM6800ROM1, DrvM6800ROM1 + ri.nLen, 0x10000 - ri.nLen); // move back
			if (BurnLoadRom(DrvM6800ROM1 + 0x10000 - ri.nLen, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += ri.nLen;
			uses_colprom = 1;
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 6) {
			if (BurnLoadRom(DrvTileROM + w2_tile_rom_len, i, 1)) return 1;
			w2_tile_rom_len += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 7) { // bg board: each 27256 is mirrored over its 0x20000 slot
			UINT8 *dst = DrvBgROM + bg_rom_len * 0x20000;
			if (BurnLoadRom(dst, i, 1)) return 1;
			for (INT32 j = 1; j < 4; j++) memcpy(dst + j * 0x8000, dst, 0x8000);
			bg_rom_len++;
			continue;
		}
	}

	// colony
	if ((mLoad - DrvM6809ROM0) == 0x12800) memcpy (DrvM6809ROM0 + 0x12800, DrvM6809ROM0 + 0x12000, 0x0800);

	if (type == 7) memset(DrvM6809ROM0 + 0x18000, 0xff, 0x8000); // inferno: bank 1 (0x18000-0x1ffff) unpop.
	if (type == 3) memset(DrvM6809ROM0 + 0x1c000, 0xff, 0x4000); // tshoot: bank1 second half unpopulated
	if (type == 9) memset(DrvM6809ROM0 + 0x1e000, 0xff, 0x2000); // mysticm: bank1 last chip unpopulated

	return 0;
}

static void blitter_init(INT32 blitter_config, UINT8 *prom)
{
	static UINT8 dummy_table[16] = { STEP16(0,1) };

	if (prom) bprintf(0, _T(" ** Using DrvColPROM.\n"));

	blitter_window_enable = 0;
	blitter_xor = (blitter_config == 1) ? 4 : 0;
	blitter_remap_index = 0;

	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 *table = prom ? (prom + (i & 0x7f) * 16) : dummy_table;

		for (INT32 j = 0; j < 256; j++)
			blitter_remap[i * 256 + j] = (table[j >> 4] << 4) | table[j & 0x0f];
	}
}

static INT32 DrvInit(INT32 maptype, INT32 loadtype, INT32 x_adjust, INT32 blitter_config, INT32 blitter_clip_addr)
{
	BurnAllocMemIndex();

	{
		if (DrvRomLoad(loadtype)) return 1;
	}

	if (maptype != 3) {
		// pre-williams2 sound HW
		M6800Init(0);
		M6800Open(0);
		M6800MapMemory(DrvM6800RAM0,				0x0000, 0x00ff, MAP_RAM);
		M6800MapMemory(DrvM6800ROM0 + 0xb000,		0xb000, 0xffff, MAP_ROM);
		M6800SetWriteHandler(defender_sound_write);
		M6800SetReadHandler(defender_sound_read);
		M6800Close();
	}

	if (maptype == 2) // blaster
	{
		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(DrvVidRAM,				0x0000, 0xbfff, MAP_RAM); // banked
		M6809MapMemory(DrvNVRAM,				0xcc00, 0xcfff, MAP_ROM);
		M6809MapMemory(DrvM6809ROM0 + 0xd000,	0xd000, 0xffff, MAP_ROM);
		M6809SetWriteHandler(blaster_main_write);
		M6809SetReadHandler(williams_main_read);
		M6809Close();

		if (has_second_soundboard) {
			M6800Init(1);
			M6800Open(1);
			M6800MapMemory(DrvM6800RAM1,				0x0000, 0x00ff, MAP_RAM);
			M6800MapMemory(DrvM6800ROM1 + 0xb000,		0xb000, 0xffff, MAP_ROM);
			M6800SetWriteHandler(blaster_sound_write);
			M6800SetReadHandler(blaster_sound_read);
			M6800Close();
		}
	}
	else if (maptype == 1) // williams
	{
		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(DrvVidRAM,				0x0000, 0xbfff, MAP_RAM); // banked
		M6809MapMemory(DrvNVRAM,				0xcc00, 0xcfff, MAP_ROM); // handler
		M6809MapMemory(DrvM6809ROM0 + 0xd000,	0xd000, 0xffff, MAP_ROM);
		M6809SetWriteHandler(williams_main_write);
		M6809SetReadHandler(williams_main_read);
		M6809Close();
	}
	else if (maptype == 0) // defender
	{
		defender = 1;
		M6809Init(0);
		M6809Open(0);
		if (mayday) {
			M6809MapMemory(DrvVidRAM,				0x0000, 0xbfff, MAP_WRITE); // read -> handler
		} else {
			M6809MapMemory(DrvVidRAM,				0x0000, 0xbfff, MAP_RAM);
		}
		M6809MapMemory(DrvM6809ROM0 + 0xd000,	0xd000, 0xffff, MAP_ROM);
		M6809SetWriteHandler(defender_main_write);
		M6809SetReadHandler(defender_main_read);
		M6809Close();
	}
	else if (maptype == 3) // williams2 (inferno, joust2, tshoot, mysticm)
	{
		williams2 = 1;
		bprintf(0, _T("Williams 2 DrvInit()!\n"));
		DrvGfxDecode();

		M6800Init(0);
		M6800Open(0);
		M6800MapMemory(DrvM6800RAM0,			0x0000, 0x00ff, MAP_RAM);
		M6800MapMemory(DrvM6800ROM0 + 0xe000,	0xe000, 0xffff, MAP_ROM);
		M6800SetWriteHandler(williams2_sound_write);
		M6800SetReadHandler(williams2_sound_read);
		M6800Close();

		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(DrvVidRAM + 0x8800	,0x8800, 0xbfff, MAP_RAM);
		M6809MapMemory(DrvTileRAM			,0xc000, 0xc7ff, MAP_RAM);
		M6809MapMemory(DrvNVRAM				,0xcc00, 0xcfff, MAP_ROM);
		if (is_joust2 || is_tshoot) {
			M6809MapMemory(DrvM6809ROM0 + 0xd000,	0xd000, 0xdfff, MAP_ROM);
		} else {
			M6809MapMemory(DrvM6809RAM0			,0xd000, 0xdfff, MAP_RAM);
		}
		M6809MapMemory(DrvM6809ROM0 + 0xe000,0xe000, 0xffff, MAP_ROM);
		M6809SetWriteHandler(williams2_main_write);
		M6809SetReadHandler(williams2_main_read);
		M6809Close();

	}

	pia_init();
	if (williams2) {
		pia_config(0, 0, is_joust2 ? &pia_joust2_0 : is_tshoot ? &pia_tshoot_0 : is_mysticm ? &pia_mysticm_0 : &pia_w2_0);
		pia_config(1, 0, is_joust2 ? &pia_joust2_1 : is_tshoot ? &pia_tshoot_1 : is_mysticm ? &pia_mysticm_1 : &pia_w2_1);
		pia_config(2, 0, is_joust2 ? &pia_joust2_2 : &pia_w2_2);
		if (!is_joust2 && !is_tshoot && !is_mysticm) {
			pia_set_port_a_input_overrides_output_mask(2, 0xff); // inferno: sound commands can drive pia2 port a output pins
		}
	} else {
		pia_config(0, 0, &pia_0);
		pia_config(1, 0, &pia_1);
		pia_config(2, 0, &pia_2);
		pia_config(3, 0, &pia_3);
	}

	BurnWatchdogInit(DrvDoReset, 180);

	DACInit(0, 0, 0, M6800TotalCycles, (williams2) ? 1000000 : 894886);
	DACSetRoute(0, (williams2) ? 0.13 : 0.35, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	if (has_s11_bg) { // just joust2
		cvsd_init(1, 1, 3, DrvBgROM, DrvBgRAM);
		cvsd_set_cb2_callback(joust2_bg_cb2_w);
		cvsd_set_s11_bg_mode(1);
		cvsd_set_dac_signed(1);

		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
		DACSetRoute(1, 0.10, BURN_SND_ROUTE_BOTH);
		hc55516_volume(0.43); // in cvsd!
	}

	if (maptype == 2) // blaster, pia config & l+r dac
	{
		pia_init();
		pia_config(0, 0, is_blasterkit ? &pia_blasterkit_0 : &pia_49way_0);
		pia_config(1, 0, &pia_1);
		pia_config(2, 0, &pia_2);
		pia_config(3, 0, &pia_3);
		pia_config(4, 0, &pia_4); // 2nd soundboard

		if (has_second_soundboard) {
			DACSetRoute(0, 0.35, BURN_SND_ROUTE_LEFT);

			DACInit(1, 0, 0, M6800TotalCycles, 894886);
			DACSetRoute(1, 0.35, BURN_SND_ROUTE_RIGHT);
		} else {
			DACSetRoute(0, 0.35, BURN_SND_ROUTE_BOTH); // mono kit - single board feeds both channels
		}
	}

	blitter_clip_address = blitter_clip_addr;
	blitter_init(blitter_config, (uses_colprom) ? DrvColPROM : NULL);

	GenericTilesInit();

	screen_x_adjust = x_adjust;

	if (is_tshoot) BurnGunInit(1, true);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	if (is_tshoot) BurnGunExit();
	GenericTilesExit();

	M6809Exit();
	M6800Exit();
	pia_exit();

	DACExit();

	if (uses_hc55516)
		hc55516_exit();

	if (has_s11_bg)
		cvsd_exit();

	BurnFreeMemIndex();

	memset (DrvDips, 0, 3);

	mayday = 0;
	splat = 0;
	blaster = 0;
	sinistarc = 0;
	defender_control_hack = 0;
	defender = 0;
	spdball = 0;
	playball = 0;
	lottofun = 0;
	conquest = 0;

	williams2 = 0;
	is_joust2 = 0;
	is_tshoot = 0;
	is_mysticm = 0;
	has_s11_bg = 0;

	w2_tile_rom_len = 0;
	w2_rom_view_active = 0;
	w2_palette_view_active = 0;
	w2_bank_entry = 0;

	uses_hc55516 = 0;
	uses_colprom = 0;
	has_s11_bg = 0;

	pStartDraw = NULL;
	pDrawScanline = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (i & 0x01) >> 0;
		INT32 bit1 = (i & 0x02) >> 1;
		INT32 bit2 = (i & 0x04) >> 2;

		UINT8 r = ((bit2 * 1200 + bit1 * 560 + bit0 * 330) * 255) / 2090;

		bit0 = (i & 0x08) >> 3;
		bit1 = (i & 0x10) >> 4;
		bit2 = (i & 0x20) >> 5;

		UINT8 g = ((bit2 * 1200 + bit1 * 560 + bit0 * 330) * 255) / 2090;

		bit0 = (i & 0x40) >> 6;
		bit1 = (i & 0x80) >> 7;

		UINT8 b = ((bit1 * 560 + bit0 * 330) * 255) / 890;

		Palette[i] = BurnHighCol(r,g,b,0);
		DrvPalette[i + 0x10] = Palette[i];
	}
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 16; i++) {
		DrvPalette[i] = Palette[DrvPalRAM[i]];
	}
}

static void draw_bitmap(INT32 starty, INT32 endy)
{
	for (INT32 y = starty; y < endy; y++)
	{
		UINT16 *dst = pTransDraw + (y * nScreenWidth);
		UINT8 *src = DrvVidRAM + y + 7;

		if (y >= 240) return;

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			UINT8 pix = src[((x + screen_x_adjust) / 2) * 256];

			dst[x + 0] = pix >> 4;
			dst[x + 1] = pix & 0x0f;
		}
	}
}

static INT32 mysticm_color_decode(INT32 base_col, INT32 sig_J1, INT32 y)
{
	INT32 v = y << 6;
	INT32 sig_W11 = (v >> 11) & 1;
	INT32 sig_W12 = (v >> 12) & 1;
	INT32 sig_W13 = (v >> 13) & 1;

	INT32 a = 1 | ((base_col & 1) << 2) | ((base_col & 1) << 3);
	INT32 b = (sig_W12 << 0) | (sig_W13 << 1) | (0 << 2) | (sig_J1 << 3);
	INT32 color = (a > b) || ((a == b) && !sig_W11);

	return (base_col & 0x3e) | color;
}

static void w2_draw_bitmap(INT32 starty, INT32 endy)
{
	// visible area is x 8-283, y 8-247 of the hardware screen
	for (INT32 y = starty; y < endy; y++)
	{
		if (y >= nScreenHeight) return;

		UINT16 *dst = pTransDraw + (y * nScreenWidth);
		INT32 sy = y + 8;

		INT32 tile_row = (sy >> 4) & 15; // 16 rows of 16px-tall tiles
		INT32 fine_y   = sy & 15;

		// background: tilemap (128 cols x 16 rows, 24x16 tiles, TILEMAP_SCAN_COLS)
		INT32 w2_scrollx_px = (w2_xscroll & 7) + ((w2_xscroll >> 3) * 6);
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			INT32 scrolled_x = (x + 8) + w2_scrollx_px - 2;
			INT32 tile_col = (scrolled_x / 24) & 127;
			INT32 fine_x = scrolled_x % 24;

			INT32 tile_index = tile_col * 16 + tile_row;
			UINT8 tile_data = DrvTileRAM[tile_index & 0x7ff];
			INT32 tile_num, flipx;
			if (is_joust2) {
				tile_num = tile_data;
				flipx = 0;
			} else {
				tile_num = tile_data & 0x7f;
				flipx = tile_data & 0x80;
			}
			INT32 color, bg_off;

			if (is_mysticm) {
				color = mysticm_color_decode(w2_bg_color, 0, (tile_index << 4) & 0xff) & 0x3f;
				bg_off = 0;
			} else if (is_joust2) {
				color = 0;
				bg_off = w2_bg_palette_offset;
			} else {
				color = (tile_index >> 1) & 7;
				bg_off = w2_bg_palette_offset;
			}

			INT32 px = flipx ? (23 - fine_x) : fine_x;

			UINT8 pix = 0;
			if (tile_num < w2_numtiles)
				pix = DrvTileROM[tile_num * (24 * 16) + fine_y * 24 + px];

			INT32 idx = 0x10 + bg_off + color * 16 + pix;
			dst[x] = idx;

		}

		// foreground: mix in the blitter
		INT32 fg_col = is_mysticm ? (mysticm_color_decode(w2_fg_color, 1, sy) & 0x3f) : w2_fg_color;
		UINT8 *src = DrvVidRAM + sy;
		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			UINT8 pix = src[((x + 8) / 2) * 256];

			if (pix & 0xf0) dst[x + 0] = 0x10 + fg_col * 16 + (pix >> 4);
			if (pix & 0x0f) dst[x + 1] = 0x10 + fg_col * 16 + (pix & 0x0f);
		}
	}
}

static void blaster_draw_bitmap(INT32 starty, INT32 endy)
{
	const UINT8 YOFFS = 7;
	UINT8 *palette_control = DrvVidRAM + 0xbb00 + YOFFS;
	UINT8 *scanline_control = DrvVidRAM + 0xbc00 + YOFFS;

	if (starty == 0 || !(blaster_video_control & 1))
		blaster_color0 = 0x10 + (palette_control[0 - YOFFS] ^ 0xff);

	for (INT32 y = starty; y < endy; y++)
	{
		INT32 erase_behind = blaster_video_control & scanline_control[y] & 2;
		UINT8 *source = DrvVidRAM + y + YOFFS;
		UINT16 *dest = pTransDraw + (y * nScreenWidth);

		if (y >= 240) return;

		if (blaster_video_control & scanline_control[y] & 1)
			blaster_color0 = 0x10 + (palette_control[y] ^ 0xff);

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			INT32 pix = source[(x/2) * 256];

			if (erase_behind)
				source[(x/2) * 256] = 0;

			dest[x+0] = (pix & 0xf0) ? (pix >> 4) : blaster_color0;
			dest[x+1] = (pix & 0x0f) ? (pix & 0x0f) : blaster_color0;
		}
	}
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	lastline = 0;

	DrvPaletteUpdate();
}

static void DrvDrawLine()
{
	if (scanline > nScreenHeight || !pBurnDraw) return;

	DrvPaletteUpdate();

	draw_bitmap(lastline, scanline);

	lastline = scanline;
}

static void BlasterDrawLine()
{
	if (scanline > nScreenHeight || !pBurnDraw) return;

	DrvPaletteUpdate();

	blaster_draw_bitmap(lastline, scanline);

	lastline = scanline;
}

static void DrvDrawEnd()
{
	if (!pBurnDraw) return;
	BurnTransferCopy(DrvPalette);
}

static INT32 Williams2DrawEnd()
{
	if (DrvRecalc) {
		w2_rebuild_palette();
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Williams2Draw()
{
	if (DrvRecalc) {
		w2_rebuild_palette();
		DrvRecalc = 0;
	}

	w2_draw_bitmap(0, nScreenHeight);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	DrvPaletteUpdate();

	draw_bitmap(0, nScreenHeight);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BlasterDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	DrvPaletteUpdate();

	blaster_draw_bitmap(0, nScreenHeight);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	M6800NewFrame();

	{
		DrvInputs[0] = DrvDips[0];
		DrvInputs[1] = DrvDips[1];
		DrvInputs[2] = DrvDips[2];

		memset (DrvInputs + 3, 0, 7-3);

		if (williams2) {
			DrvInputs[3] = 0xff; // active low
			DrvInputs[4] = 0xff;
		}

		if (is_tshoot) BurnGunMakeInputs(0, DrvAnalogPort0, DrvAnalogPort1);

		if (lottofun) {
			DrvInputs[2] |= 1; // Memory Protect
			MemoryProtect.Toggle(DrvJoy3[0]);
		}

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
		}

		if ( defender_control_hack && (DrvInputs[6] & 0x42) ) {
			// This kludge gives Defender and Stargate proper L/R joystick ability
			DrvInputs[0] |= DrvInputs[6] & 0x42;

			M6809Open(0);
			if (defender_control_hack && M6809ReadByte(defender_control_hack) == 0xfd) {
				if (DrvInputs[0] & 0x02)
					DrvInputs[0] = (DrvInputs[0] & 0xfd) | 0x40;
				else if (DrvInputs[0] & 0x40)
					DrvInputs[0] = (DrvInputs[0] & 0xbf) | 0x02;
			}
			M6809Close();
		}
		if (lottofun && lotto_motor) {
			if (lotto_ticket) lotto_ticket--;
			if (!lotto_ticket) {
				lotto_ticket = 20;
				bprintf(0, _T("ticket!\n"));
				DrvInputs[0] |= 0x80;
			}
		}

		if (spdball) {
			UINT8 xy = 0;

			xy = ProcessAnalog(DrvAnalogPort1, 1, 1, 0x00, 0xff);
			if (xy > (0x80+0x10)) TrackY[0]+=12;
			if (xy < (0x80-0x10)) TrackY[0]-=12;
			xy = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0xff);
			if (xy > (0x80+0x10)) TrackX[0]+=12;
			if (xy < (0x80-0x10)) TrackX[0]-=12;

			xy = ProcessAnalog(DrvAnalogPort3, 1, 1, 0x00, 0xff);
			if (xy > (0x80+0x10)) TrackY[1]+=12;
			if (xy < (0x80-0x10)) TrackY[1]-=12;
			xy = ProcessAnalog(DrvAnalogPort2, 0, 1, 0x00, 0xff);
			if (xy > (0x80+0x10)) TrackX[1]+=12;
			if (xy < (0x80-0x10)) TrackX[1]-=12;

		}

		if (conquest) {
			UINT8 xy = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0xff);
			if (xy > (0x80+0x10)) conquest_dial--;
			if (xy < (0x80-0x10)) conquest_dial++;
			conquest_dial &= 0x0f;

			DrvInputs[0] = (DrvInputs[0] & ~0x0c) | (((conquest_dial >> 2) & 3) << 2);
			DrvInputs[1] = (DrvInputs[1] & ~0x03) | (conquest_dial & 3);
		}
	}

	INT32 nInterleave = (williams2 && !is_mysticm) ? 260 : 256; // williams2: 260 lines per frame (mysticm: 256)
	INT32 nCyclesTotal[4] = { 1000000 / 60, (williams2 ? 1000000 : 894886) / 60, 894886 / 60, 2000000 / 60 /*s11bg*/ };
	INT32 nCyclesDone[4] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2], 0 };

	M6809Open(0);

	if (pStartDraw) pStartDraw();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (williams2) {
			if ((scanline & 0x1f) == 0 && scanline < 256) { // va11
				pia_set_input_cb1(0, scanline & 0x20);
				pia_set_input_ca1(1, scanline & 0x20);
			}
			if (scanline == 8 || scanline == 254) {
				pia_set_input_ca1(0, (scanline >= 254) ? 0 : 1);
			}

			if (pBurnDraw && scanline >= 8 && scanline < 8 + nScreenHeight) {
				w2_draw_bitmap(scanline - 8, scanline - 7);
			}
		} else {
			if (scanline % 8 == 0) {
				pia_set_input_cb1(1, scanline & 0x20);
				if (pDrawScanline) pDrawScanline();
			}

			if (scanline == 0 || scanline == 240)
				pia_set_input_ca1(1, scanline >= 240 ? 1 : 0);
		}
		CPU_RUN(0, M6809);

		M6800Open(0);
		if (has_s11_bg) joust2_bg_cb2_flush(); // slightly delayed message(s) from s11bg -> main sound
		CPU_RUN_SYNCINT(1, M6800);
		M6800Close();

		if (has_s11_bg) {
			M6809CPUPush(1);
			CPU_RUN_TIMER(3);
			M6809CPUPop();
		}

		if ((blaster || sinistarc) && has_second_soundboard) {
			M6800Open(1);
			CPU_RUN_SYNCINT(2, M6800);
			M6800Close();
		}
	}

	M6809Close();

	if (pBurnSoundOut) {
		if (has_s11_bg) {
			cvsd_update(pBurnSoundOut, nBurnSoundLen); // ym2151, dac(main, cvsd board), hc55516
		} else {
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			if (uses_hc55516)
				hc55516_update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	M6800Open(0);
	nExtraCycles[1] = M6800TotalCycles() - nCyclesTotal[1];
	M6800Close();
	if ((blaster || sinistarc) && has_second_soundboard) {
		M6800Open(1);
		nExtraCycles[2] = M6800TotalCycles() - nCyclesTotal[2];
		M6800Close();
	}

	if (pBurnDraw) {
		if (williams2)
			Williams2DrawEnd();
		else if (pStartDraw)
			DrvDrawEnd();
		else
			BurnDrvRedraw();
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

		M6809Scan(nAction);
		M6800Scan(nAction);

		pia_scan(nAction, pnMin);

		DACScan(nAction, pnMin);
		if (uses_hc55516)
			hc55516_scan(nAction, pnMin);

		SCAN_VAR(cocktail);
		SCAN_VAR(bankselect);
		SCAN_VAR(vram_select);
		SCAN_VAR(rom_bank);
		SCAN_VAR(blaster_video_control);
		SCAN_VAR(blaster_color0);

		if (lottofun) {
			SCAN_VAR(lotto_motor);
			SCAN_VAR(lotto_ticket);
		}

		SCAN_VAR(nExtraCycles);

		if (spdball) {
			SCAN_VAR(TrackX);
			SCAN_VAR(TrackY);
		}

		if (has_s11_bg) {
			cvsd_scan(nAction, pnMin);
			SCAN_VAR(bg_resetq_state);
		}

		if (is_tshoot) BurnGunScan();

		if (williams2) {
			SCAN_VAR(port_select);
			SCAN_VAR(blitter_window_enable);
			SCAN_VAR(w2_fg_color);
			SCAN_VAR(w2_bg_color);
			SCAN_VAR(w2_bg_palette_offset);
			SCAN_VAR(w2_xscroll);
			SCAN_VAR(w2_bank_select_shadow);
			SCAN_VAR(w2_fg_select_shadow);
			SCAN_VAR(w2_bg_select_shadow);
			SCAN_VAR(w2_xscroll_low_shadow);
			SCAN_VAR(w2_xscroll_high_shadow);
			SCAN_VAR(w2_video_control_shadow);
			SCAN_VAR(w2_window_enable_shadow);
			SCAN_VAR(w2_watchdog_shadow);
		}
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(DrvNVRAM, 0x400, "NVRAM");
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		if (williams2) {
			w2_bank_select(w2_bank_select_shadow);
			DrvRecalc = 1;
		}
		else if (blaster) {
			blaster_bankswitch();
		}
		else if (defender) {
			bankswitch();
		} else {
			williams_bank();
		}
		M6809Close();
	}

	return 0;
}


// Defender (Red label)

static struct BurnRomInfo defenderRomDesc[] = {
	{ "defend.1",			0x0800, 0xc3e52d7e, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "defend.4",			0x0800, 0x9a72348b, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "defend.2",			0x1000, 0x89b75984, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "defend.3",			0x1000, 0x94f51e9b, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "defend.9",			0x0800, 0x6870e8a5, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "defend.12",			0x0800, 0xf1f88938, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "defend.8",			0x0800, 0xb649e306, 1 | BRF_PRG | BRF_ESS }, 	 //  6
	{ "defend.11",			0x0800, 0x9deaf6d9, 1 | BRF_PRG | BRF_ESS }, 	 //  7
	{ "defend.7",			0x0800, 0x339e092e, 1 | BRF_PRG | BRF_ESS }, 	 //  8
	{ "defend.10",			0x0800, 0xa543b167, 1 | BRF_PRG | BRF_ESS }, 	 //  9
	{ "defend.6",			0x0800, 0x65f4efd1, 1 | BRF_PRG | BRF_ESS }, 	 // 10

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 11 M6808 Code

	{ "decoder.2",			0x0200, 0x8dd98da5, 0 | BRF_OPT },           	 // 12 Address Decoder
	{ "decoder.3",			0x0200, 0xc3f45f70, 0 | BRF_OPT },           	 // 13
};

STD_ROM_PICK(defender)
STD_ROM_FN(defender)

static INT32 DefenderCommInit()
{
	return DrvInit(0, 1, 12, -1, 0);
}

static INT32 DefenderInit()
{
	defender_control_hack = 0xa0bb;

	return DefenderCommInit();
}

static INT32 DefenderBGInit()
{
	defender_control_hack = 0xa096;

	return DefenderCommInit();
}

static INT32 DefenderWhiteInit()
{
	defender_control_hack = 0xa093;

	return DefenderCommInit();
}

static INT32 DefenceInit()
{
	defender_control_hack = 0xa0d0;

	return DefenderCommInit();
}

struct BurnDriver BurnDrvDefender = {
	"defender", NULL, NULL, NULL, "1980",
	"Defender (Red label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenderRomInfo, defenderRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defender (Green label)

static struct BurnRomInfo defendergRomDesc[] = {
	{ "defeng01.bin",		0x0800, 0x6111d74d, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "defeng04.bin",		0x0800, 0x3cfc04ce, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "defeng02.bin",		0x1000, 0xd184ab6b, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "defeng03.bin",		0x1000, 0x788b76d7, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "defeng09.bin",		0x0800, 0xf57caa62, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "defeng12.bin",		0x0800, 0x33db686f, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "defeng08.bin",		0x0800, 0x9a9eb3d2, 1 | BRF_PRG | BRF_ESS }, 	 //  6
	{ "defeng11.bin",		0x0800, 0x5ca4e860, 1 | BRF_PRG | BRF_ESS }, 	 //  7
	{ "defeng07.bin",		0x0800, 0x545c3326, 1 | BRF_PRG | BRF_ESS }, 	 //  8
	{ "defeng10.bin",		0x0800, 0x941cf34e, 1 | BRF_PRG | BRF_ESS }, 	 //  9
	{ "defeng06.bin",		0x0800, 0x3af34c05, 1 | BRF_PRG | BRF_ESS }, 	 // 10

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 11 M6808 Code

	{ "decoder.1",			0x0200, 0x8dd98da5, 0 | BRF_OPT },           	 // 12 Address Decoder
};

STD_ROM_PICK(defenderg)
STD_ROM_FN(defenderg)

struct BurnDriver BurnDrvDefenderg = {
	"defenderg", "defender", NULL, NULL, "1980",
	"Defender (Green label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defendergRomInfo, defendergRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderBGInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defender (Blue label)

static struct BurnRomInfo defenderbRomDesc[] = {
	{ "wb01.bin",			0x1000, 0x0ee1019d, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "defeng02.bin",		0x1000, 0xd184ab6b, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "wb03.bin",			0x1000, 0xa732d649, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "defeng09.bin",		0x0800, 0xf57caa62, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "defeng12.bin",		0x0800, 0x33db686f, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "defeng08.bin",		0x0800, 0x9a9eb3d2, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "defeng11.bin",		0x0800, 0x5ca4e860, 1 | BRF_PRG | BRF_ESS }, 	 //  6
	{ "defeng07.bin",		0x0800, 0x545c3326, 1 | BRF_PRG | BRF_ESS }, 	 //  7
	{ "defeng10.bin",		0x0800, 0x941cf34e, 1 | BRF_PRG | BRF_ESS }, 	 //  8
	{ "defeng06.bin",		0x0800, 0x3af34c05, 1 | BRF_PRG | BRF_ESS }, 	 //  9

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "decoder.1",			0x0200, 0x8dd98da5, 0 | BRF_OPT },           	 // 11 Address Decoder
};

STD_ROM_PICK(defenderb)
STD_ROM_FN(defenderb)

struct BurnDriver BurnDrvDefenderb = {
	"defenderb", "defender", NULL, NULL, "1980",
	"Defender (Blue label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenderbRomInfo, defenderbRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderBGInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defender (White label)

static struct BurnRomInfo defenderwRomDesc[] = {
	{ "rom1.bin",			0x1000, 0x5af871e3, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "rom2.bin",			0x1000, 0x1126adc9, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "rom3.bin",			0x1000, 0x4097b46b, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "rom9.bin",			0x0800, 0x93012991, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "rom12.bin",			0x0800, 0x4bdd8dc4, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "rom8.bin",			0x0800, 0x5227fc0b, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "rom11.bin",			0x0800, 0xd068f0c5, 1 | BRF_PRG | BRF_ESS }, 	 //  6
	{ "rom7.bin",			0x0800, 0xfef4cb77, 1 | BRF_PRG | BRF_ESS }, 	 //  7
	{ "rom10.bin",			0x0800, 0x49b50b40, 1 | BRF_PRG | BRF_ESS }, 	 //  8
	{ "rom6.bin",			0x0800, 0x43d42a1b, 1 | BRF_PRG | BRF_ESS }, 	 //  9

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "decoder.1",			0x0200, 0x8dd98da5, 0 | BRF_OPT },           	 // 11 Address Decoder
};

STD_ROM_PICK(defenderw)
STD_ROM_FN(defenderw)

struct BurnDriver BurnDrvDefenderw = {
	"defenderw", "defender", NULL, NULL, "1980",
	"Defender (White label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenderwRomInfo, defenderwRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// T.T Defender

static struct BurnRomInfo defenderjRomDesc[] = {
	{ "df1-1.e3",			0x1000, 0x8c04602b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "df2-1.e2",			0x1000, 0x89b75984, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "df3-1.e1",			0x1000, 0x94f51e9b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "df10-1.a1",			0x0800, 0x12e2bd1c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "df7-1.b1",			0x0800, 0xf1f88938, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "df9-1.a2",			0x0800, 0xb649e306, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "df6-1.b2",			0x0800, 0x9deaf6d9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "df8-1.a3",			0x0800, 0x339e092e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "df5-1.b3",			0x0800, 0xa543b167, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "df4-1.c1",			0x0800, 0x65f4efd1, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "dr12.i3",			0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "df11.k3",			0x0200, 0x25de5d85, 0 | BRF_OPT },           // 11 Address Decoder
	{ "df12.f3",			0x0200, 0xc3f45f70, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(defenderj)
STD_ROM_FN(defenderj)

struct BurnDriver BurnDrvDefenderj = {
	"defenderj", "defender", NULL, NULL, "1980",
	"T.T Defender\0", NULL, "Williams (Taito Corporation license)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenderjRomInfo, defenderjRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defender (bootleg)

static struct BurnRomInfo defndjeuRomDesc[] = {
	{ "15",					0x1000, 0x706a24bd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "16",					0x1000, 0x03201532, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "17",					0x1000, 0x25287eca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "21",					0x1000, 0xbddb71a3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "20",					0x1000, 0x12fa0788, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "19",					0x1000, 0x769f5984, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "18",					0x1000, 0xe99d5679, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "s",					0x0800, 0xcb79ae42, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(defndjeu)
STD_ROM_FN(defndjeu)

struct BurnDriverD BurnDrvDefndjeu = {
	"defndjeu", "defender", NULL, NULL, "1980",
	"Defender (bootleg)\0", NULL, "bootleg (Jeutel)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defndjeuRomInfo, defndjeuRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Tornado (set 1, Defender bootleg)

static struct BurnRomInfo tornado1RomDesc[] = {
	{ "torna1.bin",			0x1000, 0x706a24bd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "torna3.bin",			0x1000, 0xa79213b2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "torna4.bin",			0x1000, 0xf96d3d26, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tornc4.bin",			0x1000, 0xe30f4c00, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tornb3.bin",			0x1000, 0x0e3fef55, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tornb1.bin",			0x1000, 0xf2bef850, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tornb4.bin",			0x1000, 0xe99d5679, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "tornb6.bin",			0x1000, 0x3685e033, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(tornado1)
STD_ROM_FN(tornado1)

struct BurnDriver BurnDrvTornado1 = {
	"tornado1", "defender", NULL, NULL, "1980",
	"Tornado (bootleg of Defender, set 1)\0", NULL, "bootleg (Jeutel)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, tornado1RomInfo, tornado1RomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Tornado (set 2, Defender bootleg)

static struct BurnRomInfo tornado2RomDesc[] = {
	{ "tto15.bin",			0x1000, 0x910ac603, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "to16.bin",			0x1000, 0x46ccd582, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tto17.bin",			0x1000, 0xfaa3613c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "to21.bin",			0x1000, 0xe30f4c00, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "to20.bin",			0x1000, 0xe90bdcb2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "to19.bin",			0x1000, 0x42885b4f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "to18.bin",			0x1000, 0xc15ffc03, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "to_s.bin",			0x0800, 0xcb79ae42, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(tornado2)
STD_ROM_FN(tornado2)

struct BurnDriverD BurnDrvTornado2 = {
	"tornado2", "defender", NULL, NULL, "1980",
	"Tornado (bootleg of Defender, set 2)\0", NULL, "bootleg (Jeutel)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, tornado2RomInfo, tornado2RomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Zero (set 1, Defender bootleg)

static struct BurnRomInfo zeroRomDesc[] = {
	{ "zero-15",			0x1000, 0x706a24bd, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "zero-16",			0x1000, 0xa79213b2, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "zero-17",			0x1000, 0x25287eca, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "zero-21",			0x1000, 0x7ca35cfd, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "zero-20",			0x1000, 0x0757967f, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "zero-19",			0x1000, 0xf2bef850, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "zero-18",			0x1000, 0xe99d5679, 1 | BRF_PRG | BRF_ESS }, 	 //  6

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(zero)
STD_ROM_FN(zero)

struct BurnDriver BurnDrvZero = {
	"zero", "defender", NULL, NULL, "1980",
	"Zero (bootleg of Defender, set 1)\0", NULL, "bootleg (Jeutel)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, zeroRomInfo, zeroRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Zero (set 2, Defender bootleg)

static struct BurnRomInfo zero2RomDesc[] = {
	{ "15me.1a",			0x1000, 0x9323eee5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "to16.3a",			0x1000, 0xa79213b2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "17m5.4a",			0x1000, 0x16a3c0dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "21.4c",				0x1000, 0x7ca35cfd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "20m5.3b",			0x1000, 0x7473955b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "to19.1b",			0x1000, 0xf2bef850, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "18m5.4b",			0x1000, 0x7e4afe43, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "to4.6b",				0x0800, 0xcb79ae42, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(zero2)
STD_ROM_FN(zero2)

struct BurnDriver BurnDrvZero2 = {
	"zero2", "defender", NULL, NULL, "1980",
	"Zero (bootleg of Defender, set 2)\0", NULL, "bootleg (Amtec)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, zero2RomInfo, zero2RomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Operacion Defender (bootleg of Defender)

static struct BurnRomInfo defenderomRomDesc[] = {
	{ "rom1.bin",		0x1000, 0x8c04602b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rom2.bin",		0x1000, 0x89b75984, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom3.bin",		0x1000, 0x94f51e9b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom10.bin",		0x0800, 0x12e2bd1c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom7.bin",		0x0800, 0x19e1ac79, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom9.bin",		0x0800, 0xb8ac5966, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom6.bin",		0x0800, 0x9deaf6d9, 1 | BRF_PRG | BRF_ESS }, //  6 BAD_DUMP, borrowed from defenseb
	{ "rom8.bin",		0x0800, 0x339e092e, 1 | BRF_PRG | BRF_ESS }, //  7 BAD_DUMP, borrowed from defenseb
	{ "rom5.bin",		0x0800, 0x871f75a0, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "rom4.bin",		0x0800, 0x65f4efd1, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "rom12.bin",		0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "7641-1.bin",		0x0200, 0x25de5d85, 0 | BRF_OPT },           // 11 Address Decoder
	{ "7641-2.bin",		0x0200, 0xc3f45f70, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(defenderom)
STD_ROM_FN(defenderom)

struct BurnDriver BurnDrvDefenderom = {
	"defenderom", "defender", NULL, NULL, "1981",
	"Operacion Defender (bootleg of Defender)\0", NULL, "bootleg (Operamatic)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenderomRomInfo, defenderomRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defense Command (bootleg of Defender)

static struct BurnRomInfo defcmndRomDesc[] = {
	{ "defcmnda.1",			0x1000, 0x68effc1d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "defcmnda.2",			0x1000, 0x1126adc9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "defcmnda.3",			0x1000, 0x7340209d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "defcmnda.10",		0x0800, 0x3dddae75, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "defcmnda.7",			0x0800, 0x3f1e7cf8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "defcmnda.9",			0x0800, 0x8882e1ff, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "defcmnda.6",			0x0800, 0xd068f0c5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "defcmnda.8",			0x0800, 0xfef4cb77, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "defcmnda.5",			0x0800, 0x49b50b40, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "defcmnda.4",			0x0800, 0x43d42a1b, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "defcmnda.snd",		0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code
};

STD_ROM_PICK(defcmnd)
STD_ROM_FN(defcmnd)

struct BurnDriver BurnDrvDefcmnd = {
	"defcmnd", "defender", NULL, NULL, "1980",
	"Defense Command (bootleg of Defender)\0", NULL, "bootleg", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defcmndRomInfo, defcmndRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Next Century (bootleg of Defender)

static struct BurnRomInfo nextcentRomDesc[] = {
	{ "nextcentury_2732.ic29",			0x1000, 0x68effc1d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "nextcentury_2732.ic28",			0x1000, 0x1126adc9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nextcentury_2732.ic27",			0x1000, 0x7340209d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nextcentury_2732.ic30",			0x1000, 0x78b33590, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nextcentury_2732.ic31",			0x1000, 0x23bddfaf, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "nextcentury_2732.ic32",			0x1000, 0x03721aa7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "nextcentury_2732.ic33",			0x1000, 0xe4a27e2f, 1 | BRF_PRG | BRF_ESS }, //  6
	
	{ "nextcentury_2732.ic24",			0x1000, 0x4c2236cc, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code
	
	{ "nextcentury_7343_82s137.ic34",	0x0400, 0x00b0b5b5, 0 | BRF_OPT },           // 11 Address Decoder
	{ "nextcentury_7343_82s137.ic41",	0x0400, 0xba1d88cd, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(nextcent)
STD_ROM_FN(nextcent)

struct BurnDriver BurnDrvNextcent = {
	"nextcent", "defender", NULL, NULL, "1981",
	"Next Century (bootleg of Defender)\0", NULL, "bootleg (Petaco)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, nextcentRomInfo, nextcentRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defense (bootleg of Defender)

static struct BurnRomInfo defensebRomDesc[] = {
	{ "1.d9",			0x1000, 0x8c04602b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "2.c9",			0x1000, 0x89b75984, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.d8",			0x1000, 0x94f51e9b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.c4",			0x0800, 0x12e2bd1c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "7.d5",			0x0800, 0x88881cc6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9.d4",			0x0800, 0x252605c9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "6.c6",			0x0800, 0x9deaf6d9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.c5",			0x0800, 0x339e092e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "5.d6",			0x0800, 0xa543b167, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "4.c7",			0x0800, 0x65f4efd1, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "12.f3.snd",		0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "mmi6341.a1",		0x0200, 0x25de5d85, 0 | BRF_OPT },           // 11 Address Decoder
	{ "mmi6341.l1",		0x0200, 0xc3f45f70, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(defenseb)
STD_ROM_FN(defenseb)

struct BurnDriver BurnDrvDefenseb = {
	"defenseb", "defender", NULL, NULL, "198?",
	"Defense (bootleg of Defender)\0", NULL, "bootleg", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defensebRomInfo, defensebRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Star Trek (bootleg of Defender)

static struct BurnRomInfo startrkdRomDesc[] = {
	{ "st_rom8.bin",		0x1000, 0x5af871e3, 1 | BRF_PRG | BRF_ESS }, 	 //  0 M6809 Code
	{ "st_rom9.bin",		0x1000, 0x1126adc9, 1 | BRF_PRG | BRF_ESS }, 	 //  1
	{ "st_rom10.bin",		0x1000, 0x4097b46b, 1 | BRF_PRG | BRF_ESS }, 	 //  2
	{ "st_rom6.bin",		0x0800, 0x93012991, 1 | BRF_PRG | BRF_ESS }, 	 //  3
	{ "st_rom5.bin",		0x0800, 0xc6f0c004, 1 | BRF_PRG | BRF_ESS }, 	 //  4
	{ "st_rom4.bin",		0x0800, 0xb48430bf, 1 | BRF_PRG | BRF_ESS }, 	 //  5
	{ "st_rom3.bin",		0x0800, 0xd068f0c5, 1 | BRF_PRG | BRF_ESS }, 	 //  6
	{ "st_rom2.bin",		0x0800, 0xfef4cb77, 1 | BRF_PRG | BRF_ESS }, 	 //  7
	{ "st_rom1.bin",		0x0800, 0xd23d6cdb, 1 | BRF_PRG | BRF_ESS }, 	 //  8
	{ "st_rom7.bin",		0x0800, 0x43d42a1b, 1 | BRF_PRG | BRF_ESS }, 	 //  9

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code
};

STD_ROM_PICK(startrkd)
STD_ROM_FN(startrkd)

struct BurnDriver BurnDrvStartrkd = {
	"startrkd", "defender", NULL, NULL, "1981",
	"Star Trek (bootleg of Defender)\0", NULL, "bootleg", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, startrkdRomInfo, startrkdRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderWhiteInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Defence Command (bootleg of Defender)

static struct BurnRomInfo defenceRomDesc[] = {
	{ "1",					0x1000, 0xebc93622, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "2",					0x1000, 0x2a4f4f44, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",					0x1000, 0xa4112f91, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "0",					0x0800, 0x7a1e5998, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "7",					0x0800, 0x4c2616a3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9",					0x0800, 0x7b146003, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "6",					0x0800, 0x6d748030, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8",					0x0800, 0x52d5438b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "5",					0x0800, 0x4a270340, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "4",					0x0800, 0xe13f457c, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "defcmnda.snd",		0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code
};

STD_ROM_PICK(defence)
STD_ROM_FN(defence)

struct BurnDriver BurnDrvDefence = {
	"defence", "defender", NULL, NULL, "1981",
	"Defence Command (bootleg of Defender)\0", NULL, "bootleg (Outer Limits)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, defenceRomInfo, defenceRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Attack (bootleg of Defender)

static struct BurnRomInfo attackfRomDesc[] = {
	{ "002-1.ic1",		0x1000, 0x0ee1019d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "002-2.ic2",		0x1000, 0xd184ab6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "002-3.ic3",		0x1000, 0xa732d649, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "002-9.ic12",		0x0800, 0xf57caa62, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "002-12.ic9",		0x0800, 0xeb73d8a1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "002-8.ic11",		0x0800, 0x17f7abde, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "002-11.ic8",		0x0800, 0x5ca4e860, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "002-7.ic10",		0x0800, 0x545c3326, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "002-10.ic7",		0x0800, 0x3940d731, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "002-6.ic6",		0x0800, 0x3af34c05, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "003-13.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "001-14.g1",		0x0200, 0x8dd98da5, 0 | BRF_OPT },           // 11 Address Decoder
};

STD_ROM_PICK(attackf)
STD_ROM_FN(attackf)

struct BurnDriver BurnDrvAttackf = {
	"attackf", "defender", NULL, NULL, "1980",
	"Attack (bootleg of Defender)\0", NULL, "bootleg (Famaresa)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, attackfRomInfo, attackfRomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Galaxy Wars II (bootleg of Defender)

static struct BurnRomInfo galwars2RomDesc[] = {
	{ "9d-1-2532.bin",		0x1000, 0xebc93622, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "9c-2-2532.bin",		0x1000, 0x2a4f4f44, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8d-3-2532.bin",		0x1000, 0xa4112f91, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4c-10-2716.bin",		0x0800, 0x7a1e5998, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5d-7-2716.bin",		0x0800, 0xa9bdacdc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "4d-9-2716.bin",		0x0800, 0x906dca8f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "6c-6-2716.bin",		0x0800, 0x6d748030, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "5c-8-2716.bin",		0x0800, 0x52d5438b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "6d-5-2716.bin",		0x0800, 0x4a270340, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "7c-4-2716.bin",		0x0800, 0xe13f457c, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "3f-11-2716.bin",		0x0800, 0xf122d9c9, 2 | BRF_PRG | BRF_ESS }, // 10 M6808 Code

	{ "1l-13-8516.bin",		0x0800, 0x7e113979, 0 | BRF_OPT },           // 11 Unknown
	{ "1a-12-8516.bin",		0x0800, 0xa562c506, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(galwars2)
STD_ROM_FN(galwars2)

struct BurnDriver BurnDrvGalwars2 = {
	"galwars2", "defender", NULL, NULL, "1981",
	"Galaxy Wars II (bootleg of Defender)\0", NULL, "bootleg (Sonic)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, galwars2RomInfo, galwars2RomName, NULL, NULL, NULL, NULL, DefenderInputInfo, NULL,
	DefenceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Mayday (set 1)

static struct BurnRomInfo maydayRomDesc[] = {
	{ "ic03-3.bin",			0x1000, 0xa1ff6e62, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "ic02-2.bin",			0x1000, 0x62183aea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic01-1.bin",			0x1000, 0x5dcb113f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic04-4.bin",			0x1000, 0xea6a4ec8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic05-5.bin",			0x1000, 0x0d797a3e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic06-6.bin",			0x1000, 0xee8bfcd6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic07-7d.bin",		0x1000, 0xd9c065e7, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "ic28-8.bin",			0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(mayday)
STD_ROM_FN(mayday)

static INT32 MaydayInit()
{
	mayday = 1;

	return DrvInit(0, 1, 12, -1, 0);
}

struct BurnDriver BurnDrvMayday = {
	"mayday", NULL, NULL, NULL, "1980",
	"Mayday (set 1)\0", NULL, "Hoei", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, maydayRomInfo, maydayRomName, NULL, NULL, NULL, NULL, MaydayInputInfo, NULL,
	MaydayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Mayday (set 2)

static struct BurnRomInfo maydayaRomDesc[] = {
	{ "mayday.c",			0x1000, 0x872a2f2d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "mayday.b",			0x1000, 0xc4ab5e22, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mayday.a",			0x1000, 0x329a1318, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mayday.d",			0x1000, 0xc2ae4716, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mayday.e",			0x1000, 0x41225666, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mayday.f",			0x1000, 0xc39be3c0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "mayday.g",			0x1000, 0x2bd0f106, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "ic28-8.bin",			0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(maydaya)
STD_ROM_FN(maydaya)

struct BurnDriver BurnDrvMaydaya = {
	"maydaya", "mayday", NULL, NULL, "1980",
	"Mayday (set 2)\0", NULL, "Hoei", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, maydayaRomInfo, maydayaRomName, NULL, NULL, NULL, NULL, MaydayInputInfo, NULL,
	MaydayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Mayday (set 3)

static struct BurnRomInfo maydaybRomDesc[] = {
	{ "ic03-3.bin",			0x1000, 0xa1ff6e62, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "ic02-2.bin",			0x1000, 0x62183aea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic01-1.bin",			0x1000, 0x5dcb113f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom7.bin",			0x1000, 0x0c3ca687, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic05-5.bin",			0x1000, 0x0d797a3e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic06-6.bin",			0x1000, 0xee8bfcd6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic07-7d.bin",		0x1000, 0xd9c065e7, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "ic28-8.bin",			0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code

	{ "rom11.bin",			0x0800, 0x7e113979, 0 | BRF_OPT },           //  8 user1
	{ "rom12.bin",			0x0800, 0xa562c506, 0 | BRF_OPT },           //  9
	{ "rom6a.bin",			0x0800, 0x8e4e981f, 0 | BRF_OPT },           // 10
	{ "rom8-sos.bin",		0x0800, 0x6a9b383f, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(maydayb)
STD_ROM_FN(maydayb)

struct BurnDriver BurnDrvMaydayb = {
	"maydayb", "mayday", NULL, NULL, "1980",
	"Mayday (set 3)\0", NULL, "Hoei", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, maydaybRomInfo, maydaybRomName, NULL, NULL, NULL, NULL, MaydayInputInfo, NULL,
	MaydayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Battle Zone (bootleg of Mayday)

static struct BurnRomInfo batlzoneRomDesc[] = {
	{ "43-2732.rom.bin",	0x1000, 0x244334f8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "42-2732.rom.bin",	0x1000, 0x62183aea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "41-2732.rom.bin",	0x1000, 0xa7e9093e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "44-8532.rom.bin",	0x1000, 0xbba3e626, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "45-8532.rom.bin",	0x1000, 0x43b3a0de, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "46-8532.rom.bin",	0x1000, 0x3df9b901, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "47-8532.rom.bin",	0x1000, 0x55a27e02, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "48-2716.rom.bin",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  7 M6808 Code
};

STD_ROM_PICK(batlzone)
STD_ROM_FN(batlzone)

struct BurnDriver BurnDrvBatlzone = {
	"batlzone", "mayday", NULL, NULL, "1980",
	"Battle Zone (bootleg of Mayday)\0", NULL, "bootleg (Video Game)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, batlzoneRomInfo, batlzoneRomName, NULL, NULL, NULL, NULL, MaydayInputInfo, NULL,
	MaydayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Colony 7 (set 1)

static struct BurnRomInfo colony7RomDesc[] = {
	{ "cs03.bin",			0x1000, 0x7ee75ae5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "cs02.bin",			0x1000, 0xc60b08cb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cs01.bin",			0x1000, 0x1bc97436, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cs06.bin",			0x0800, 0x318b95af, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cs04.bin",			0x0800, 0xd740faee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cs07.bin",			0x0800, 0x0b23638b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cs05.bin",			0x0800, 0x59e406a8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cs08.bin",			0x0800, 0x3bfde87a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "cs11.bin",			0x0800, 0x6032293c, 2 | BRF_PRG | BRF_ESS }, //  8 M6808 Code

	{ "cs10.bin",			0x0200, 0x25de5d85, 0 | BRF_OPT },           //  9 Address Decoder?
	{ "decoder.3",			0x0200, 0xc3f45f70, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(colony7)
STD_ROM_FN(colony7)

static INT32 Colony7Init()
{
	return DrvInit(0, 0, 14, -1, 0);
}

struct BurnDriver BurnDrvColony7 = {
	"colony7", NULL, NULL, NULL, "1981",
	"Colony 7 (set 1)\0", NULL, "Taito", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, colony7RomInfo, colony7RomName, NULL, NULL, NULL, NULL, Colony7InputInfo, Colony7DIPInfo,
	Colony7Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Colony 7 (set 2)

static struct BurnRomInfo colony7aRomDesc[] = {
	{ "cs03a.bin",			0x1000, 0xe0b0d23b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "cs02a.bin",			0x1000, 0x370c6f41, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cs01a.bin",			0x1000, 0xba299946, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cs06.bin",			0x0800, 0x318b95af, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cs04.bin",			0x0800, 0xd740faee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cs07.bin",			0x0800, 0x0b23638b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cs05.bin",			0x0800, 0x59e406a8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cs08.bin",			0x0800, 0x3bfde87a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "cs11.bin",			0x0800, 0x6032293c, 2 | BRF_PRG | BRF_ESS }, //  8 M6808 Code

	{ "cs10.bin",			0x0200, 0x25de5d85, 0 | BRF_OPT },           //  9 Address Decoder?
	{ "decoder.3",			0x0200, 0xc3f45f70, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(colony7a)
STD_ROM_FN(colony7a)

struct BurnDriver BurnDrvColony7a = {
	"colony7a", "colony7", NULL, NULL, "1981",
	"Colony 7 (set 2)\0", NULL, "Taito", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, colony7aRomInfo, colony7aRomName, NULL, NULL, NULL, NULL, Colony7InputInfo, Colony7DIPInfo,
	Colony7Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Jin

static struct BurnRomInfo jinRomDesc[] = {
	{ "jin11.6c",			0x1000, 0xc4b9e93f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "jin12.7c",			0x1000, 0xa8bc9fdd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jin13.6d",			0x1000, 0x79779b85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jin14.4c",			0x1000, 0x6a4df97e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jin15.3f",			0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  4 M6808 Code

	{ "jin.1a",				0x0200, 0x8dd98da5, 0 | BRF_OPT },           //  5 Address Decoder?
	{ "jin.1l",				0x0200, 0xc3f45f70, 0 | BRF_OPT },           //  6
};

STD_ROM_PICK(jin)
STD_ROM_FN(jin)

static INT32 JinInit()
{
	return DrvInit(0, 0, 0, -1, 0);
}

struct BurnDriver BurnDrvJin = {
	"jin", NULL, NULL, NULL, "1982",
	"Jin\0", NULL, "Falcon", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, jinRomInfo, jinRomName, NULL, NULL, NULL, NULL, JinInputInfo, JinDIPInfo,
	JinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 316, 3, 4
};


// Stargate
/* "B" ROMs labeled 3002-13 through 3002-24, identical data */

static struct BurnRomInfo stargateRomDesc[] = {
	{ "stargate_rom_10-a_3002-10.a7",	0x1000, 0x60b07ff7, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "stargate_rom_11-a_3002-11.c7",	0x1000, 0x7d2c5daf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "stargate_rom_12-a_3002-12.e7",	0x1000, 0xa0396670, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "stargate_rom_1-a_3002-1.e4",		0x1000, 0x88824d18, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "stargate_rom_2-a_3002-2.c4",		0x1000, 0xafc614c5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "stargate_rom_3-a_3002-3.a4",		0x1000, 0x15077a9d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "stargate_rom_4-a_3002-4.e5",		0x1000, 0xa8b4bf0f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "stargate_rom_5-a_3002-5.c5",		0x1000, 0x2d306074, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "stargate_rom_6-a_3002-6.a5",		0x1000, 0x53598dde, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "stargate_rom_7-a_3002-7.e6",		0x1000, 0x23606060, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "stargate_rom_8-a_3002-8.c6",		0x1000, 0x4ec490c7, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "stargate_rom_9-a_3002-9.a6",		0x1000, 0x88187b64, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_2_std_744.ic12",	0x0800, 0x2fcf6c4d, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_5.3c",				0x0200, 0xf921c5fe, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(stargate)
STD_ROM_FN(stargate)

static INT32 StargateInit()
{
	defender_control_hack = 0x9c92;

	return DrvInit(1, 0, 6, -1, 0);
}

struct BurnDriver BurnDrvStargate = {
	"stargate", NULL, NULL, NULL, "1981",
	"Stargate\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, stargateRomInfo, stargateRomName, NULL, NULL, NULL, NULL, StargateInputInfo, NULL,
	StargateInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Conquest (prototype)

static struct BurnRomInfo conquestRomDesc[] = {
	{ "conquest_b.bin",		0x1000, 0x9aab5516, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (fixed window, mirrored)
	{ "conquest_b.bin",		0x1000, 0x9aab5516, 1 | BRF_PRG | BRF_ESS }, //  1 (mirror of above)
	{ "conquest_a.bin",		0x4000, 0xa384f4a2, 1 | BRF_PRG | BRF_ESS }, //  2 (banked)

	{ "video_sound_rom_1.ic12",	0x0800, 0xfefd5b48, 2 | BRF_PRG | BRF_ESS }, //  3 M6800 Code

	{ "decoder.2",			0x0200, 0x8dd98da5, 0 | BRF_OPT },           //  4 Address Decoder
	{ "decoder.3",			0x0200, 0xc3f45f70, 0 | BRF_OPT },           //  5
};

STD_ROM_PICK(conquest)
STD_ROM_FN(conquest)

static INT32 ConquestInit()
{
	conquest = 1;

	return DrvInit(1, 6, 6, -1, 0);
}

struct BurnDriverD BurnDrvConquest = {
	"conquest", NULL, NULL, NULL, "1982",
	"Conquest (prototype)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, conquestRomInfo, conquestRomName, NULL, NULL, NULL, NULL, ConquestInputInfo, NULL,
	ConquestInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Inferno

static struct BurnRomInfo infernoRomDesc[] = {
	{ "ic9.inf",		0x1000, 0x1a013185, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (fixed window)
	{ "ic10.inf",		0x1000, 0xdbf64a36, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic18.inf",		0x2000, 0x95bcf7b1, 1 | BRF_PRG | BRF_ESS }, //  2 (bank 0)
	{ "ic16.inf",		0x2000, 0x8bc4f935, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic14.inf",		0x2000, 0xa70508a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic12.inf",		0x2000, 0x7ffb87f9, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ic17.inf",		0x2000, 0xb4684139, 1 | BRF_PRG | BRF_ESS }, //  6 (bank 2)
	{ "ic15.inf",		0x2000, 0x128a6ad6, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "ic13.inf",		0x2000, 0x83a9e4d6, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "ic11.inf",		0x2000, 0xc2e9c909, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "ic25.inf",		0x2000, 0x103a5951, 1 | BRF_PRG | BRF_ESS }, // 10 (bank 3)
	{ "ic23.inf",		0x2000, 0xc04749a0, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "ic21.inf",		0x2000, 0xc405f853, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "ic19.inf",		0x2000, 0xade7645a, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "ic8.inf",		0x2000, 0x4e3123b8, 2 | BRF_PRG | BRF_ESS }, // 14 M6800 Code

	{ "ic57.inf",		0x2000, 0x65a4ef79, 6 | BRF_GRA },           // 15 Tilemap Tiles
	{ "ic58.inf",		0x2000, 0x4bb1c2a0, 6 | BRF_GRA },           // 16
	{ "ic41.inf",		0x2000, 0xf3f7238f, 6 | BRF_GRA },           // 17

	{ "prom1",			0x0020, 0x85057e40, 0 | BRF_OPT },           // 18 n/c
	{ "prom2",			0x0100, 0xefb03024, 0 | BRF_OPT },           // 19
	{ "prom3",			0x0200, 0x0ea3f7fb, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(inferno)
STD_ROM_FN(inferno)

static struct BurnRomInfo joust2RomDesc[] = {
	{ "cpu_2732_ic55_rom2_rev1.4c",			0x1000, 0x08b0d5bd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (0xd000-0xdfff)
	{ "cpu_2732_ic9_rom3_rev2.4d",			0x1000, 0x951175ce, 1 | BRF_PRG | BRF_ESS }, //  1 (fixed window)
	{ "cpu_2732_ic10_rom4_rev2.4f",			0x1000, 0xba6e0f6c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cpu_2732_ic18_rom11_rev1.8j",		0x2000, 0x9dc986f9, 1 | BRF_PRG | BRF_ESS }, //  3 (bank 0)
	{ "cpu_2732_ic16_rom9_rev2.8h",			0x2000, 0x56e2b550, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu_2732_ic14_rom7_rev2.6j",			0x2000, 0xf3bce576, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cpu_2732_ic12_rom5_rev2.6h",			0x2000, 0x5f8b4919, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "cpu_2732_ic26_rom19_rev1.10j",		0x2000, 0x4ef5e805, 1 | BRF_PRG | BRF_ESS }, //  7 (bank 1)
	{ "cpu_2732_ic24_rom17_rev1.10h",		0x2000, 0x4861f063, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "cpu_2732_ic22_rom15_rev1.9j",		0x2000, 0x421aafa8, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "cpu_2732_ic20_rom13_rev1.9h",		0x2000, 0x3432ff55, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "cpu_2732_ic17_rom10_rev1.8i",		0x2000, 0x3e01b597, 1 | BRF_PRG | BRF_ESS }, // 11 (bank 2, 3 of 4 chips)
	{ "cpu_2732_ic15_rom8_rev1.8g",			0x2000, 0xff26fb29, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "cpu_2732_ic13_rom6_rev2.6i",			0x2000, 0x5f107db5, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "cpu_2732_ic25_rom18_rev1.10i",		0x2000, 0x47580af5, 1 | BRF_PRG | BRF_ESS }, // 14 (bank 3)
	{ "cpu_2732_ic23_rom16_rev1.10g",		0x2000, 0x869b5942, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "cpu_2732_ic21_rom14_rev1.9i",		0x2000, 0x0bbd867c, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "cpu_2732_ic19_rom12_rev1.9g",		0x2000, 0xb9221ed1, 1 | BRF_PRG | BRF_ESS }, // 17

	{ "cpu_2764_ic8_rom1_rev1.0f",			0x2000, 0x84517c3c, 2 | BRF_PRG | BRF_ESS }, // 18 M6800 Code

	{ "vid_27128_ic57_rom20_rev1.8f",		0x4000, 0x572c6b01, 6 | BRF_GRA },           // 19 Tilemap Tiles
	{ "vid_27128_ic58_rom21_rev1.9f",		0x4000, 0xaa94bf05, 6 | BRF_GRA },           // 20
	{ "vid_27128_ic41_rom22_rev1.9d",		0x4000, 0xc41e3daa, 6 | BRF_GRA },           // 21

	{ "vid_82s123_ic14_a-5282-10295.2b",	0x0020, 0x85057e40, 0 | BRF_OPT },           // 22 n/c
	{ "vid_82s129_ic47_a-5282-10294.15d",	0x0100, 0xefb03024, 0 | BRF_OPT },           // 23
	{ "vid_82s147a_ic60_a-5282-10292.12f",	0x0200, 0x0ea3f7fb, 0 | BRF_OPT },           // 24

	{ "snd_27256_rom23_rev1.u4",			0x8000, 0x3af6b47d, 7 | BRF_SND | BRF_ESS }, // 25 BG sound board M6809 Code
	{ "snd_27256_rom24_rev1.u19",			0x8000, 0xe7f9ed2e, 7 | BRF_SND | BRF_ESS }, // 26
	{ "snd_27256_rom25_rev1.u20",			0x8000, 0xc85b29f7, 7 | BRF_SND | BRF_ESS }, // 27
};

STD_ROM_PICK(joust2)
STD_ROM_FN(joust2)

static struct BurnRomInfo joust2r1RomDesc[] = {
	{ "cpu_2732_ic55_rom2_rev1.4c",		0x1000, 0x08b0d5bd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (0xd000-0xdfff)
	{ "cpu_2732_ic9_rom3_rev1.4d",			0x1000, 0x6f319644, 1 | BRF_PRG | BRF_ESS }, //  1 (fixed window)
	{ "cpu_2732_ic10_rom4_rev1.4f",			0x1000, 0x027b9f0c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cpu_2732_ic18_rom11_rev1.8j",		0x2000, 0x9dc986f9, 1 | BRF_PRG | BRF_ESS }, //  3 (bank 0)
	{ "cpu_2732_ic16_rom9_rev1.8h",			0x2000, 0x0c77e22f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu_2732_ic14_rom7_rev1.6j",			0x2000, 0xfb9455ca, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cpu_2732_ic12_rom5_rev1.6h",			0x2000, 0x31248a0d, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "cpu_2732_ic26_rom19_rev1.10j",		0x2000, 0x4ef5e805, 1 | BRF_PRG | BRF_ESS }, //  7 (bank 1)
	{ "cpu_2732_ic24_rom17_rev1.10h",		0x2000, 0x4861f063, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "cpu_2732_ic22_rom15_rev1.9j",		0x2000, 0x421aafa8, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "cpu_2732_ic20_rom13_rev1.9h",		0x2000, 0x3432ff55, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "cpu_2732_ic17_rom10_rev1.8i",		0x2000, 0x3e01b597, 1 | BRF_PRG | BRF_ESS }, // 11 (bank 2, 3 of 4 chips)
	{ "cpu_2732_ic15_rom8_rev1.8g",			0x2000, 0xff26fb29, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "cpu_2732_ic13_rom6_rev1.6i",			0x2000, 0x6a8c87d7, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "cpu_2732_ic25_rom18_rev1.10i",		0x2000, 0x47580af5, 1 | BRF_PRG | BRF_ESS }, // 14 (bank 3)
	{ "cpu_2732_ic23_rom16_rev1.10g",		0x2000, 0x869b5942, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "cpu_2732_ic21_rom14_rev1.9i",		0x2000, 0x0bbd867c, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "cpu_2732_ic19_rom12_rev1.9g",		0x2000, 0xb9221ed1, 1 | BRF_PRG | BRF_ESS }, // 17

	{ "cpu_2764_ic8_rom1_rev1.0f",			0x2000, 0x84517c3c, 2 | BRF_PRG | BRF_ESS }, // 18 M6800 Code

	{ "vid_27128_ic57_rom20_rev1.8f",		0x4000, 0x572c6b01, 6 | BRF_GRA },           // 19 Tilemap Tiles
	{ "vid_27128_ic58_rom21_rev1.9f",		0x4000, 0xaa94bf05, 6 | BRF_GRA },           // 20
	{ "vid_27128_ic41_rom22_rev1.9d",		0x4000, 0xc41e3daa, 6 | BRF_GRA },           // 21

	{ "vid_82s123_ic14_a-5282-10295.2b",	0x0020, 0x85057e40, 0 | BRF_OPT },           // 22 n/c
	{ "vid_82s129_ic47_a-5282-10294.15d",	0x0100, 0xefb03024, 0 | BRF_OPT },           // 23
	{ "vid_82s147a_ic60_a-5282-10292.12f",	0x0200, 0x0ea3f7fb, 0 | BRF_OPT },           // 24

	{ "snd_27256_rom23_rev1.u4",			0x8000, 0x3af6b47d, 7 | BRF_SND | BRF_ESS }, // 25 BG sound board M6809 Code
	{ "snd_27256_rom24_rev1.u19",			0x8000, 0xe7f9ed2e, 7 | BRF_SND | BRF_ESS }, // 26
	{ "snd_27256_rom25_rev1.u20",			0x8000, 0xc85b29f7, 7 | BRF_SND | BRF_ESS }, // 27
};

STD_ROM_PICK(joust2r1)
STD_ROM_FN(joust2r1)

static struct BurnRomInfo tshootRomDesc[] = {
	{ "rom18.ic55",		0x1000, 0xeffc33f1, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (0xd000-0xdfff)
	{ "rom2.ic9",		0x1000, 0xfd982687, 1 | BRF_PRG | BRF_ESS }, //  1 (fixed window)
	{ "rom3.ic10",		0x1000, 0x9617054d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "rom11.ic18",		0x2000, 0x60d5fab8, 1 | BRF_PRG | BRF_ESS }, //  3 (bank 0)
	{ "rom9.ic16",		0x2000, 0xa4dd4a0e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom7.ic14",		0x2000, 0xf25505e6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom5.ic12",		0x2000, 0x94a7c0ed, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "rom17.ic26",		0x2000, 0xb02d1ccd, 1 | BRF_PRG | BRF_ESS }, //  7 (bank 1, 2 of 4 chips)
	{ "rom15.ic24",		0x2000, 0x11709935, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "rom10.ic17",		0x2000, 0x0f32bad8, 1 | BRF_PRG | BRF_ESS }, //  9 (bank 2)
	{ "rom8.ic15",		0x2000, 0xe9b6cbf7, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "rom6.ic13",		0x2000, 0xa49f617f, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "rom4.ic11",		0x2000, 0xb026dc00, 1 | BRF_PRG | BRF_ESS }, // 12

	{ "rom16.ic25",		0x2000, 0x69ce38f8, 1 | BRF_PRG | BRF_ESS }, // 13 (bank 3)
	{ "rom14.ic23",		0x2000, 0x769a4ae5, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "rom13.ic21",		0x2000, 0xec016c9b, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "rom12.ic19",		0x2000, 0x98ae7afa, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "rom1.ic8",		0x2000, 0x011a94a7, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 Code

	{ "rom20.ic57",		0x2000, 0xc6e1d253, 6 | BRF_GRA },           // 18 Tilemap Tiles
	{ "rom21.ic58",		0x2000, 0x9874e90f, 6 | BRF_GRA },           // 19
	{ "rom19.ic41",		0x2000, 0xb9ce4d2a, 6 | BRF_GRA },           // 20

	{ "82s123.ic14a",	0x0020, 0x27a6d555, 0 | BRF_OPT },           // 21 n/c
	{ "82s129.ic47",	0x0100, 0xefb03024, 0 | BRF_OPT },           // 22
	{ "7649.ic60",		0x0200, 0x0ea3f7fb, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(tshoot)
STD_ROM_FN(tshoot)

static INT32 TshootInit()
{
	is_tshoot = 1;

	return DrvInit(3, 3, 0, -1, 0x9000);
}

struct BurnDriver BurnDrvTshoot = {
	"tshoot", NULL, NULL, NULL, "1984",
	"Turkey Shoot\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, tshootRomInfo, tshootRomName, NULL, NULL, NULL, NULL, TshootInputInfo, NULL,
	TshootInit, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	276, 240, 4, 3
};

static struct BurnRomInfo mysticmRomDesc[] = {
	{ "mm02_2.a09",		0x1000, 0x3a776ea8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (fixed window)
	{ "mm03_2.a10",		0x1000, 0x6e247c75, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mm11_1.a18",		0x2000, 0xf537968e, 1 | BRF_PRG | BRF_ESS }, //  2 (bank 0)
	{ "mm09_1.a16",		0x2000, 0x3bd12f6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mm07_1.a14",		0x2000, 0xea2a2a68, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mm05_1.a12",		0x2000, 0xb514eef3, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mm18_1.a26",		0x2000, 0x9b391a81, 1 | BRF_PRG | BRF_ESS }, //  6 (bank 1, 3 of 4 chips)
	{ "mm16_1.a24",		0x2000, 0x399e175d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "mm14_1.a22",		0x2000, 0x191153b1, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "mm10_1.a17",		0x2000, 0xd6a37509, 1 | BRF_PRG | BRF_ESS }, //  9 (bank 2)
	{ "mm08_1.a15",		0x2000, 0x6f1a64f2, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "mm06_1.a13",		0x2000, 0x2e6795d4, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "mm04_1.a11",		0x2000, 0xc222fb64, 1 | BRF_PRG | BRF_ESS }, // 12

	{ "mm17_1.a25",		0x2000, 0xd36f0a96, 1 | BRF_PRG | BRF_ESS }, // 13 (bank 3)
	{ "mm15_1.a23",		0x2000, 0xcd5d99da, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "mm13_1.a21",		0x2000, 0xef4b79db, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "mm12_1.a19",		0x2000, 0xa1f04bf0, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "mm01_1.a08",		0x2000, 0x65339512, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 Code

	{ "mm20_1.b57",		0x2000, 0x5c0f4f46, 6 | BRF_GRA },           // 18 Tilemap Tiles
	{ "mm21_1.b58",		0x2000, 0xcb90b3c5, 6 | BRF_GRA },           // 19
	{ "mm19_1.b41",		0x2000, 0xe274df86, 6 | BRF_GRA },           // 20

	{ "ic14.bpr",		0x0020, 0x27a6d555, 0 | BRF_OPT },           // 21 n/c
	{ "ic47.bpr",		0x0100, 0xefb03024, 0 | BRF_OPT },           // 22
	{ "ic60.bpr",		0x0200, 0x0ea3f7fb, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(mysticm)
STD_ROM_FN(mysticm)

static struct BurnRomInfo mysticmpRomDesc[] = {
	{ "cpu_2732_ic9_rom2_proto6.d4",	0x1000, 0x3e4f53dd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (fixed window)
	{ "cpu_2732_ic10_rom3_proto6.f4",	0x1000, 0x6a25ee4b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cpu_2764_ic18_rom11_proto5.j8",	0x2000, 0xf537968e, 1 | BRF_PRG | BRF_ESS }, //  2 (bank 0)
	{ "cpu_2764_ic16_rom9_proto5.h8",	0x2000, 0x3bd12f6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu_2764_ic14_rom7_proto5.j6",	0x2000, 0xea2a2a68, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu_2764_ic12_rom5_proto5.h6",	0x2000, 0xb514eef3, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "cpu_2764_ic26_rom18_proto5.j10",	0x2000, 0x9b391a81, 1 | BRF_PRG | BRF_ESS }, //  6 (bank 1, 3 of 4 chips)
	{ "cpu_2764_ic24_rom16_proto5.h10",	0x2000, 0x399e175d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "cpu_2764_ic22_rom14_proto5.j9",	0x2000, 0x191153b1, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "cpu_2764_ic17_rom10_proto4.i8",	0x2000, 0xd6a37509, 1 | BRF_PRG | BRF_ESS }, //  9 (bank 2)
	{ "cpu_2764_ic15_rom8_proto4.g8",	0x2000, 0x6f1a64f2, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "cpu_2764_ic13_rom6_proto4.i6",	0x2000, 0x2e6795d4, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "cpu_2764_ic11_rom4_proto4.g6",	0x2000, 0xc222fb64, 1 | BRF_PRG | BRF_ESS }, // 12

	{ "cpu_2764_ic25_rom17_proto6.i10",	0x2000, 0x7acc9995, 1 | BRF_PRG | BRF_ESS }, // 13 (bank 3)
	{ "cpu_2764_ic23_rom15_proto6.g10",	0x2000, 0xc32d1ce5, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "cpu_2764_ic21_rom13_proto6.i9",	0x2000, 0xe387a785, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "cpu_2764_ic19_rom12_proto6.g9",	0x2000, 0xa1f04bf0, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "cpu_2764_ic8_rom1_proto4.f0",	0x2000, 0x65339512, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 Code

	{ "ram_2764_ic57_rom20_rev1.f9",	0x2000, 0x5c0f4f46, 6 | BRF_GRA },           // 18 Tilemap Tiles
	{ "ram_2764_ic58_rom21_rev1.f10",	0x2000, 0xcb90b3c5, 6 | BRF_GRA },           // 19
	{ "ram_2764_ic41_rom19_rev1.d10",	0x2000, 0xe274df86, 6 | BRF_GRA },           // 20
};

STD_ROM_PICK(mysticmp)
STD_ROM_FN(mysticmp)

static INT32 MysticmInit()
{
	is_mysticm = 1;

	return DrvInit(3, 9, 0, -1, 0x9000);
}

struct BurnDriver BurnDrvMysticm = {
	"mysticm", NULL, NULL, NULL, "1983",
	"Mystic Marathon\0", "Bad Colors", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mysticmRomInfo, mysticmRomName, NULL, NULL, NULL, NULL, MysticmInputInfo, NULL,
	MysticmInit, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	276, 240, 4, 3
};

struct BurnDriver BurnDrvMysticmp = {
	"mysticmp", "mysticm", NULL, NULL, "1983",
	"Mystic Marathon (prototype)\0", "Bad Colors", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mysticmpRomInfo, mysticmpRomName, NULL, NULL, NULL, NULL, MysticmInputInfo, NULL,
	MysticmInit, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	276, 240, 4, 3
};

static INT32 Joust2Init()
{
	is_joust2 = 1;
	has_s11_bg = 1;

	return DrvInit(3, 8, 0, -1, 0x9000);
}

struct BurnDriver BurnDrvJoust2 = {
	"joust2", NULL, NULL, NULL, "1986",
	"Joust 2 - Survival of the Fittest (revision 2)\0", "S11 background sound board not emulated", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, joust2RomInfo, joust2RomName, NULL, NULL, NULL, NULL, Joust2InputInfo, NULL,
	Joust2Init, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	240, 276, 3, 4
};

struct BurnDriver BurnDrvJoust2r1 = {
	"joust2r1", "joust2", NULL, NULL, "1986",
	"Joust 2 - Survival of the Fittest (revision 1)\0", "S11 background sound board not emulated", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, joust2r1RomInfo, joust2r1RomName, NULL, NULL, NULL, NULL, Joust2InputInfo, NULL,
	Joust2Init, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	240, 276, 3, 4
};

static INT32 InfernoInit()
{
	return DrvInit(3, 7, 0, -1, 0x9000);
}

struct BurnDriver BurnDrvInferno = {
	"inferno", NULL, NULL, NULL, "1984",
	"Inferno (Williams)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, infernoRomInfo, infernoRomName, NULL, NULL, NULL, NULL, InfernoInputInfo, NULL,
	InfernoInit, DrvExit, DrvFrame, Williams2Draw, DrvScan, &DrvRecalc, 0x410,
	276, 240, 4, 3
};


// Robotron: 2084 (Solid Blue label)
/* "Release 5" Solid Blue labels, "B" type ROMs labeled 3005-13 through 3005-24 */

static struct BurnRomInfo robotronRomDesc[] = {
	{ "2084_rom_10b_3005-22.a7",		0x1000, 0x13797024, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "2084_rom_11b_3005-23.c7",		0x1000, 0x7e3c1b87, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2084_rom_12b_3005-24.e7",		0x1000, 0x645d543e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2084_rom_1b_3005-13.e4",			0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2084_rom_2b_3005-14.c4",			0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2084_rom_3b_3005-15.a4",			0x1000, 0xe99a82be, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "2084_rom_4b_3005-16.e5",			0x1000, 0xafb1c561, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "2084_rom_5b_3005-17.c5",			0x1000, 0x62691e77, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2084_rom_6b_3005-18.a5",			0x1000, 0xbd2c853d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "2084_rom_7b_3005-19.e6",			0x1000, 0x49ac400c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "2084_rom_8b_3005-20.c6",			0x1000, 0x3a96e88c, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "2084_rom_9b_3005-21.a6",			0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotron)
STD_ROM_FN(robotron)

static INT32 RobotronInit()
{
	return DrvInit(1, 0, 6, 1, 0xc000);
}

struct BurnDriver BurnDrvRobotron = {
	"robotron", NULL, NULL, NULL, "1982",
	"Robotron: 2084 (Release 5, solid blue label)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, robotronRomInfo, robotronRomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (Yellow/Orange label)
/* "Release 4" Yellow label / Red stripe & Black print or Yellow label / Red stripe & Green print "B" type ROMs numbered 3005-13 through 3005-24 */

static struct BurnRomInfo robotronyoRomDesc[] = {
	{ "2084_rom_10b_3005-10.a7",		0x1000, 0x4a9d5f52, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "2084_rom_11b_3005-11.c7",		0x1000, 0x2afc5e7f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2084_rom_12b_3005-12.e7",		0x1000, 0x45da9202, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2084_rom_1b_3005-1.e4",			0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2084_rom_2b_3005-2.c4",			0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2084_rom_3b_3005-3.a4",			0x1000, 0x67a369bc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "2084_rom_4b_3005-4.e5",			0x1000, 0xb0de677a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "2084_rom_5b_3005-5.c5",			0x1000, 0x24726007, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2084_rom_6b_3005-6.a5",			0x1000, 0x028181a6, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "2084_rom_7b_3005-7.e6",			0x1000, 0x4dfcceae, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "2084_rom_8b_3005-8.c6",			0x1000, 0x3a96e88c, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "2084_rom_9b_3005-9.a6",			0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotronyo)
STD_ROM_FN(robotronyo)

struct BurnDriver BurnDrvRobotronyo = {
	"robotronyo", "robotron", NULL, NULL, "1982",
	"Robotron: 2084 (Release 4, yellow/orange label)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, robotronyoRomInfo, robotronyoRomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (Release 3)
/* "Release 3" Censored prototype seen in the Robotron promo videotape, recovered from the Vid Kidz dev system */

static struct BurnRomInfo robotronr3RomDesc[] = {
	{ "r3_10.a7",						0x1000, 0x1fb75828, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "r3_11.c7",						0x1000, 0xccfaf7da, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3_12.e7",						0x1000, 0xcfdc9990, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "r3_1.e4",						0x1000, 0x2e5433ee, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "r3_2.c4",						0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "r3_3.a4",						0x1000, 0x37962af2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "r3_4.e5",						0x1000, 0x9c140843, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "r3_5.c5",						0x1000, 0x24726007, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "r3_6.a5",						0x1000, 0x097e174d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "r3_7.e6",						0x1000, 0x17a7ce34, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "r3_8.c6",						0x1000, 0xb6e8dbb9, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "r3_9.a6",						0x1000, 0xe7b53345, 1 | BRF_PRG | BRF_ESS }, // 11
	
	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotronr3)
STD_ROM_FN(robotronr3)

struct BurnDriver BurnDrvRobotronr3 = {
	"robotronr3", "robotron", NULL, NULL, "1982",
	"Robotron: 2084 (Release 3, prototype)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, robotronr3RomInfo, robotronr3RomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (Unidesa license)

static struct BurnRomInfo robotronunRomDesc[] = {
	{ "roboun11.10b",		0x1000, 0x13797024, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "roboun11.11b",		0x1000, 0x7e3c1b87, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "roboun11.12b",		0x1000, 0x645d543e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "roboun11.1b",		0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "roboun11.2b",		0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "roboun11.3b",		0x1000, 0xe99a82be, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "roboun11.4b",		0x1000, 0xafb1c561, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "roboun11.5b",		0x1000, 0x62691e77, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "roboun11.6b",		0x1000, 0xbd2c853d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "roboun11.7b",		0x1000, 0x8981a43b, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "roboun11.8b",		0x1000, 0x3a96e88c, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "roboun11.9b",		0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",	0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",	0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotronun)
STD_ROM_FN(robotronun)

struct BurnDriver BurnDrvRobotronun = {
	"robotronun", "robotron", NULL, NULL, "1982",
	"Robotron: 2084 (Unidesa license)\0", NULL, "Williams / Vid Kidz (Unidesa license)", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, robotronunRomInfo, robotronunRomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (1987 'shot-in-the-corner' bugfix)
/* Patch by Christian Gingras in 1987 fixing 7 bugs, AKA "Shot in the corner" bug fix */

static struct BurnRomInfo robotron87RomDesc[] = {
	{ "2084_rom_10b_3005-22.a7",		0x1000, 0x13797024, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "fixrobo_rom_11b.c7",				0x1000, 0xe83a2eda, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2084_rom_12b_3005-24.e7",		0x1000, 0x645d543e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2084_rom_1b_3005-13.e4",			0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2084_rom_2b_3005-14.c4",			0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2084_rom_3b_3005-15.a4",			0x1000, 0xe99a82be, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "2084_rom_4b_3005-16.e5",			0x1000, 0xafb1c561, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fixrobo_rom_5b.c5",				0x1000, 0x827cb5c9, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2084_rom_6b_3005-18.a5",			0x1000, 0xbd2c853d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "2084_rom_7b_3005-19.e6",			0x1000, 0x49ac400c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "2084_rom_8b_3005-20.c6",			0x1000, 0x3a96e88c, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "2084_rom_9b_3005-21.a6",			0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotron87)
STD_ROM_FN(robotron87)

struct BurnDriver BurnDrvRobotron87 = {
	"robotron87", "robotron", NULL, NULL, "1987",
	"Robotron: 2084 (1987 'shot-in-the-corner' bug fix)\0", NULL, "hack", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, robotron87RomInfo, robotron87RomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (2012 'wave 201 start' hack)

static struct BurnRomInfo robotron12RomDesc[] = {
	{ "2084_rom_10b_3005-22.a7",		0x1000, 0x13797024, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "fixrobo_rom_11b.c7",				0x1000, 0xe83a2eda, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2084_rom_12b_3005-24.e7",		0x1000, 0x645d543e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2084_rom_1b_3005-13.e4",			0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2084_rom_2b_3005-14.c4",			0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "wave201.a4",						0x1000, 0x85eb583e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "2084_rom_4b_3005-16.e5",			0x1000, 0xafb1c561, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fixrobo_rom_5b.c5",				0x1000, 0x827cb5c9, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2084_rom_6b_3005-18.a5",			0x1000, 0xbd2c853d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "2084_rom_7b_3005-19.e6",			0x1000, 0x49ac400c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "2084_rom_8b_3005-20.c6",			0x1000, 0x3a96e88c, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "2084_rom_9b_3005-21.a6",			0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotron12)
STD_ROM_FN(robotron12)

struct BurnDriver BurnDrvRobotron12 = {
	"robotron12", "robotron", NULL, NULL, "2012",
	"Robotron: 2084 (2012 'wave 201 start' hack)\0", NULL, "hack", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, robotron12RomInfo, robotron12RomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Robotron: 2084 (2015 'tie-die V2' hack)
/* Tie-Die version starts with a "Solid Blue label" set */

static struct BurnRomInfo robotrontdRomDesc[] = {
	{ "tiedie_rom_10b.a7",				0x1000, 0x952bea55, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "tiedie_rom_11b.c7",				0x1000, 0x4c05fd3c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2084_rom_12b_3005-24.e7",		0x1000, 0x645d543e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2084_rom_1b_3005-13.e4",			0x1000, 0x66c7d3ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2084_rom_2b_3005-14.c4",			0x1000, 0x5bc6c614, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2084_rom_3b_3005-15.a4",			0x1000, 0xe99a82be, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tiedie_rom_4b.e5",				0x1000, 0xe8238019, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fixrobo_rom_5b.c5",				0x1000, 0x827cb5c9, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2084_rom_6b_3005-18.a5",			0x1000, 0xbd2c853d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "tiedie_rom_7b.e6",				0x1000, 0x3ecf4620, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "tiedie_rom_8b.c6",				0x1000, 0x752d7a46, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "2084_rom_9b_3005-21.a6",			0x1000, 0xb124367b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_3_std_767.ic12",	0x1000, 0xc56c1d28, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(robotrontd)
STD_ROM_FN(robotrontd)

struct BurnDriver BurnDrvRobotrontd = {
	"robotrontd", "robotron", NULL, NULL, "2015",
	"Robotron: 2084 (2015 'tie-die V2' hack)\0", NULL, "hack", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, robotrontdRomInfo, robotrontdRomName, NULL, NULL, NULL, NULL, RobotronInputInfo, NULL,
	RobotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Joust (Green label)
/* Solid green labels - contains the same data as the white label with green stripe 3006-52 through 3006-63 set */

static struct BurnRomInfo joustRomDesc[] = {
	{ "joust_rom_10b_3006-22.a7",		0x1000, 0x3f1c4f89, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "joust_rom_11b_3006-23.c7",		0x1000, 0xea48b359, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "joust_rom_12b_3006-24.e7",		0x1000, 0xc710717b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "joust_rom_1b_3006-13.e4",		0x1000, 0xfe41b2af, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "joust_rom_2b_3006-14.c4",		0x1000, 0x501c143c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "joust_rom_3b_3006-15.a4",		0x1000, 0x43f7161d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "joust_rom_4b_3006-16.e5",		0x1000, 0xdb5571b6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "joust_rom_5b_3006-17.c5",		0x1000, 0xc686bb6b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "joust_rom_6b_3006-18.a5",		0x1000, 0xfac5f2cf, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "joust_rom_7b_3006-19.e6",		0x1000, 0x81418240, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "joust_rom_8b_3006-20.c6",		0x1000, 0xba5359ba, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "joust_rom_9b_3006-21.a6",		0x1000, 0x39643147, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_4_std_780.ic12",	0x1000, 0xf1835bdd, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(joust)
STD_ROM_FN(joust)

static INT32 JoustInit()
{
	INT32 nRet = DrvInit(1, 0, 6, 1, 0xc000);

	if (nRet == 0)
	{
		pia_config(0, 0, &pia_muxed_joust_0);
		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;
	}

	return nRet;
}

struct BurnDriver BurnDrvJoust = {
	"joust", NULL, NULL, NULL, "1982",
	"Joust (Green label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, joustRomInfo, joustRomName, NULL, NULL, NULL, NULL, JoustInputInfo, NULL,
	JoustInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Joust (Yellow label)
/* Solid yellow labels */

static struct BurnRomInfo joustyRomDesc[] = {
	{ "joust_rom_10a_3006-10.a7",		0x1000, 0x2039014a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "joust_rom_11a_3006-11.c7",		0x1000, 0xea48b359, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "joust_rom_12a_3006-12.e7",		0x1000, 0xc710717b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "joust_rom_1a_3006-1.e4",			0x1000, 0xfe41b2af, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "joust_rom_2a_3006-2.c4",			0x1000, 0x501c143c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "joust_rom_3a_3006-3.a4",			0x1000, 0x43f7161d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "joust_rom_4a_3006-4.e5",			0x1000, 0xdb5571b6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "joust_rom_5a_3006-5.c5",			0x1000, 0xc686bb6b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "joust_rom_6a_3006-6.a5",			0x1000, 0xfac5f2cf, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "joust_rom_7a_3006-7.e6",			0x1000, 0xe6f439c4, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "joust_rom_8a_3006-8.c6",			0x1000, 0xba5359ba, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "joust_rom_9a_3006-9.a6",			0x1000, 0x39643147, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_4_std_780.ic12",	0x1000, 0xf1835bdd, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(jousty)
STD_ROM_FN(jousty)

struct BurnDriver BurnDrvJousty = {
	"jousty", "joust", NULL, NULL, "1982",
	"Joust (Yellow label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, joustyRomInfo, joustyRomName, NULL, NULL, NULL, NULL, JoustInputInfo, NULL,
	JoustInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Joust (Red label)
/* Solid red labels */

static struct BurnRomInfo joustrRomDesc[] = {
	{ "joust_rom_10a_3006-37.a7",		0x1000, 0xc0c6e52a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "joust_rom_11a_3006-38.c7",		0x1000, 0xab11bcf9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "joust_rom_12a_3006-39.e7",		0x1000, 0xea14574b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "joust_rom_1a_3006-28.e4",		0x1000, 0xfe41b2af, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "joust_rom_2a_3006-29.c4",		0x1000, 0x501c143c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "joust_rom_3a_3006-30.a4",		0x1000, 0x43f7161d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "joust_rom_4a_3006-31.e5",		0x1000, 0xab347170, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "joust_rom_5a_3006-32.c5",		0x1000, 0xc686bb6b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "joust_rom_6a_3006-33.a5",		0x1000, 0x3d9a6fac, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "joust_rom_7a_3006-34.e6",		0x1000, 0x0a70b3d1, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "joust_rom_8a_3006-35.c6",		0x1000, 0xa7f01504, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "joust_rom_9a_3006-36.a6",		0x1000, 0x978687ad, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_4_std_780.ic12",	0x1000, 0xf1835bdd, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(joustr)
STD_ROM_FN(joustr)

struct BurnDriver BurnDrvJoustr = {
	"joustr", "joust", NULL, NULL, "1982",
	"Joust (Red label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, joustrRomInfo, joustrRomName, NULL, NULL, NULL, NULL, JoustInputInfo, NULL,
	JoustInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Bubbles

static struct BurnRomInfo bubblesRomDesc[] = {
	{ "bubbles_rom_10b_16-3012-49.a7",	0x1000, 0x26e7869b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "bubbles_rom_11b_16-3012-50.c7",	0x1000, 0x5a5b572f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bubbles_rom_12b_16-3012-51.e7",	0x1000, 0xce22d2e2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bubbles_rom_1b_16-3012-40.4e",	0x1000, 0x8234f55c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bubbles_rom_2b_16-3012-41.4c",	0x1000, 0x4a188d6a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bubbles_rom_3b_16-3012-42.4a",	0x1000, 0x7728f07f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bubbles_rom_4b_16-3012-43.5e",	0x1000, 0x040be7f9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "bubbles_rom_5b_16-3012-44.5c",	0x1000, 0x0b5f29e0, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "bubbles_rom_6b_16-3012-45.5a",	0x1000, 0x4dd0450d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "bubbles_rom_7b_16-3012-46.6e",	0x1000, 0xe0a26ec0, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "bubbles_rom_8b_16-3012-47.6c",	0x1000, 0x4fd23d8d, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "bubbles_rom_9b_16-3012-48.6a",	0x1000, 0xb48559fb, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_5_std_771.ic12",	0x1000, 0x689ce2aa, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(bubbles)
STD_ROM_FN(bubbles)

static INT32 BubblesInit()
{
	INT32 nRet = DrvInit(1, 0, 6, 1, 0xc000);
	
	if (nRet == 0) {
		M6809Open(0);
		M6809MapMemory(DrvNVRAM,		0xcc00, 0xcfff, MAP_RAM); // 8 bit
		M6809Close();
	}
	
	return nRet;
}

struct BurnDriver BurnDrvBubbles = {
	"bubbles", NULL, NULL, NULL, "1982",
	"Bubbles\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, bubblesRomInfo, bubblesRomName, NULL, NULL, NULL, NULL, BubblesInputInfo, NULL,
	BubblesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Bubbles (Solid Red label)

static struct BurnRomInfo bubblesrRomDesc[] = {
	{ "bubblesr.10b",		0x1000, 0x8b396db0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "bubblesr.11b",		0x1000, 0x096af43e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bubblesr.12b",		0x1000, 0x5c1244ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bubblesr.1b",		0x1000, 0xdda4e782, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bubblesr.2b",		0x1000, 0x3c8fa7f5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bubblesr.3b",		0x1000, 0xf869bb9c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bubblesr.4b",		0x1000, 0x0c65eaab, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "bubblesr.5b",		0x1000, 0x7ece4e13, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "bubbles.6b",			0x1000, 0x4dd0450d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "bubbles.7b",			0x1000, 0xe0a26ec0, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "bubblesr.8b",		0x1000, 0x598b9bd6, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "bubbles.9b",			0x1000, 0xb48559fb, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_5_std_771.ic12",		0x1000, 0x689ce2aa, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",	0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",	0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(bubblesr)
STD_ROM_FN(bubblesr)

struct BurnDriver BurnDrvBubblesr = {
	"bubblesr", "bubbles", NULL, NULL, "1982",
	"Bubbles (Solid Red label)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, bubblesrRomInfo, bubblesrRomName, NULL, NULL, NULL, NULL, BubblesInputInfo, NULL,
	BubblesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Bubbles (prototype)

static struct BurnRomInfo bubblespRomDesc[] = {
	{ "bub_prot.10b",		0x1000, 0x89a565df, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "bub_prot.11b",		0x1000, 0x5a0c36a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bub_prot.12b",		0x1000, 0x2bfd3438, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bub_prot.1b",		0x1000, 0x6466a746, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bub_prot.2b",		0x1000, 0xcca04357, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bub_prot.3b",		0x1000, 0x7aaff9e5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bub_prot.4b",		0x1000, 0x4e264f01, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "bub_prot.5b",		0x1000, 0x121b0be6, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "bub_prot.6b",		0x1000, 0x80e90b25, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "bub_prot.7b",		0x1000, 0xe0a26ec0, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "bub_prot.8b",		0x1000, 0x96fb19c8, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "bub_prot.9b",		0x1000, 0xbe7e1028, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_5_std_771.ic12",		0x1000, 0x689ce2aa, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",	0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",	0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(bubblesp)
STD_ROM_FN(bubblesp)

struct BurnDriver BurnDrvBubblesp = {
	"bubblesp", "bubbles", NULL, NULL, "1982",
	"Bubbles (prototype)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, bubblespRomInfo, bubblespRomName, NULL, NULL, NULL, NULL, BubblesInputInfo, NULL,
	BubblesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Splat!

static struct BurnRomInfo splatRomDesc[] = {
	/* Solid Brown labels */
	{ "splat_rom_10b_16-3011-10.a7",	0x1000, 0xd1a1f632, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "splat_rom_11b_16-3011-11.c7",	0x1000, 0xca8cde95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "splat_rom_12b_16-3011-12.e7",	0x1000, 0x5bee3e60, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "splat_rom_1b_16-3011-1.e4",		0x1000, 0x1cf26e48, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "splat_rom_2b_16-3011-2.c4",		0x1000, 0xac0d4276, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "splat_rom_3b_16-3011-3.a4",		0x1000, 0x74873e59, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "splat_rom_4b_16-3011-4.e5",		0x1000, 0x70a7064e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "splat_rom_5b_16-3011-5.c5",		0x1000, 0xc6895221, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "splat_rom_6b_16-3011-6.a5",		0x1000, 0xea4ab7fd, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "splat_rom_7b_16-3011-7.e6",		0x1000, 0x82fd8713, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "splat_rom_8b_16-3011-8.c6",		0x1000, 0x7dded1b4, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "splat_rom_9b_16-3011-9.a6",		0x1000, 0x71cbfe5a, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "video_sound_rom_13_std.ic12",	0x1000, 0xa878d5f3, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder_rom_4.3g",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder_rom_6.3c",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(splat)
STD_ROM_FN(splat)

static INT32 SplatInit()
{
	splat = 1;

	INT32 nRet = DrvInit(1, 0, 6, 2, 0xc000);

	if (nRet == 0)
	{
		pia_config(0, 0, &pia_muxed_joust_0);
		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;
	}

	return nRet;
}

struct BurnDriver BurnDrvSplat = {
	"splat", NULL, NULL, NULL, "1982",
	"Splat!\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, splatRomInfo, splatRomName, NULL, NULL, NULL, NULL, SplatInputInfo, NULL,
	SplatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Speed Ball - Contest at Neonworld (prototype)

static struct BurnRomInfo spdballRomDesc[] = {
	{ "speedbal.10",		0x1000, 0x4a3add93, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "speedbal.11",		0x1000, 0x1fbcfaa5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "speedbal.12",		0x1000, 0xf3458f41, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "speedbal.01",		0x1000, 0x7f4801bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "speedbal.02",		0x1000, 0x5cd5e489, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "speedbal.03",		0x1000, 0x280e11a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "speedbal.04",		0x1000, 0x3469cbbf, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "speedbal.05",		0x1000, 0x87373c89, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "speedbal.06",		0x1000, 0x48779a0d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "speedbal.07",		0x1000, 0x2e5d8db6, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "speedbal.08",		0x1000, 0xc173cedf, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "speedbal.09",		0x1000, 0x415f424b, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "speedbal.snd",		0x1000, 0x78de20e2, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "mystery.rom",		0x1000, 0xdcb6a070, 0 | BRF_OPT },           // 13 ??
};

STD_ROM_PICK(spdball)
STD_ROM_FN(spdball)

static INT32 SpdballInit()
{
	INT32 nRet = DrvInit(1, 0, 6, 1, 0xc000);

	if (nRet == 0) // raster draw
	{
		spdball = 1;
		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;
	}

	return nRet;
}

struct BurnDriver BurnDrvSpdball = {
	"spdball", NULL, NULL, NULL, "1985",
	"Speed Ball - Contest at Neonworld (prototype)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, spdballRomInfo, spdballRomName, NULL, NULL, NULL, NULL, SpdballInputInfo, NULL,
	SpdballInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Alien Arena

static struct BurnRomInfo alienarRomDesc[] = {
	{ "aarom10",			0x1000, 0x6feb0314, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code 
	{ "aarom11",			0x1000, 0xae3a270e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aarom12",			0x1000, 0x6be9f09e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aarom01",			0x1000, 0xbb0c21be, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "aarom02",			0x1000, 0x165acd37, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "aarom03",			0x1000, 0xe5d51d92, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "aarom04",			0x1000, 0x24f6feb8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "aarom05",			0x1000, 0x5b1ac59b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "aarom06",			0x1000, 0xda7195a2, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "aarom07",			0x1000, 0xf9812be4, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "aarom08",			0x1000, 0xcd7f3a87, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "aarom09",			0x1000, 0xe6ce77b4, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "sg.snd",				0x0800, 0x2fcf6c4d, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code

	{ "decoder.4",			0x0200, 0xe6631c23, 0 | BRF_OPT },           // 13 Address Decoder
	{ "decoder.6",			0x0200, 0x83faf25e, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(alienar)
STD_ROM_FN(alienar)

static INT32 AlienarInit()
{
	INT32 nRet = DrvInit(1, 0, 6, 2, 0xc000);

	if (nRet == 0)
	{
		pia_config(0, 0, &pia_muxed_joust_0);
	}

	return nRet;
}

struct BurnDriver BurnDrvAlienar = {
	"alienar", NULL, NULL, NULL, "1985",
	"Alien Arena\0", NULL, "Duncan Brown", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, alienarRomInfo, alienarRomName, NULL, NULL, NULL, NULL, AlienarInputInfo, NULL,
	AlienarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	292, 240, 4, 3
};


// Sinistar (revision 3, upright)
// solid RED labels with final production part numbers

static struct BurnRomInfo sinistarRomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-62.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-63.4a",	0x1000, 0x3162bc50, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-53.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-54.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-55.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-56.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-57.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-58.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-59.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-60.3c",		0x1000, 0x4785a787, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-61.3a",		0x1000, 0x50cb63ad, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistar)
STD_ROM_FN(sinistar)

static INT32 SinistarInit()
{
	INT32 nRet = DrvInit(1, 4, 6, 1, 0x7400);

	if (nRet == 0)
	{
		hc55516_init(M6800TotalCycles, 894886);
		uses_hc55516 = 1;

		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;

		pia_init();
		pia_config(0, 0, &pia_49way_0);
		pia_config(1, 0, &pia_1);
		pia_config(2, 0, &pia_2_sinistar);
		pia_config(3, 0, &pia_3);

		M6809Open(0);
		M6809MapMemory(DrvM6809RAM0,		0xd000, 0xdfff, MAP_RAM);
		M6809Close();
	}

	return nRet;
}

struct BurnDriver BurnDrvSinistar = {
	"sinistar", NULL, NULL, NULL, "1982",
	"Sinistar (revision 3, upright)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarRomInfo, sinistarRomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Sinistar (revision 3, cockpit)

static struct BurnRomInfo sinistarcRomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-62.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sinistar_rom_11-b_16-3004-63.4a",	0x1000, 0x3162bc50, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-53.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-54.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-55.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-56.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-57.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-58.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-59.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-60.3c",		0x1000, 0x4785a787, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-61.3a",		0x1000, 0x50cb63ad, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 soundcpu
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "video_sound_rom_10_std.ic12",		0x1000, 0xb5c70082, 3 | BRF_PRG | BRF_ESS }, // 16 soundcpu_b

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 17 proms
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(sinistarc)
STD_ROM_FN(sinistarc)

static INT32 SinistarCockpitInit()
{
	INT32 nRet = SinistarInit();

	if (nRet == 0) {
		M6800Init(1);
		M6800Open(1);
		M6800MapMemory(DrvM6800RAM1,				0x0000, 0x00ff, MAP_RAM);
		M6800MapMemory(DrvM6800ROM1 + 0xb000,		0xb000, 0xffff, MAP_ROM);
		M6800SetWriteHandler(blaster_sound_write);
		M6800SetReadHandler(blaster_sound_read);
		M6800Close();

		pia_config(4, 0, &pia_4); // 2nd soundboard

		DACSetRoute(0, 0.35, BURN_SND_ROUTE_LEFT);

		DACInit(1, 0, 0, M6800TotalCycles, 894886);
		DACSetRoute(1, 0.35, BURN_SND_ROUTE_RIGHT);

		sinistarc = 1;
	}

	return nRet;
}

struct BurnDriver BurnDrvSinistarc = {
	"sinistarc", "sinistar", NULL, NULL, "1982",
	"Sinistar (revision 3, cockpit)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarcRomInfo, sinistarcRomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarCockpitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Sinistar (revision 2, cockpit)

static struct BurnRomInfo sinistarc2RomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-47.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sinistar_rom_11-b_16-3004-48.4a",	0x1000, 0x792c8b00, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-38.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-39.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-40.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-41.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-42.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-43.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-44.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-45.3c",		0x1000, 0xd7ecee45, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-46.3a",		0x1000, 0x50cb63ad, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 soundcpu
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "video_sound_rom_10_std.ic12",		0x1000, 0xb5c70082, 3 | BRF_PRG | BRF_ESS }, // 16 soundcpu_b

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 17 proms
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(sinistarc2)
STD_ROM_FN(sinistarc2)

struct BurnDriver BurnDrvSinistarc2 = {
	"sinistarc2", "sinistar", NULL, NULL, "1982",
	"Sinistar (revision 2, cockpit)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarc2RomInfo, sinistarc2RomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarCockpitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};

// Sinistar (SynaMax difficulty/titlescreen hack, revision 3, upright)

static struct BurnRomInfo sinistarsmRomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-62.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-63.4a.mod",0x1000, 0xfe83bce2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-53.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-54.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-55.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-56.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-57.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-58.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-59.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-60.3c.mod",	0x1000, 0x9b693243, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-61.3a.mod",	0x1000, 0x5ed44beb, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistarsm)
STD_ROM_FN(sinistarsm)

struct BurnDriver BurnDrvSinistarsm = {
	"sinistarsm", "sinistar", NULL, NULL, "2023",
	"Sinistar (SynaMax difficulty/titlescreen hack)\0", "initial boot will black screen, hit reset to fix", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarsmRomInfo, sinistarsmRomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Sinistar (SynaMax difficulty/titlescreen hack, revision 3, upright)

static struct BurnRomInfo sinistarcsmRomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-62.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-63.4a.mod",0x1000, 0xfe83bce2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-53.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-54.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-55.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-56.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-57.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-58.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-59.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-60.3c.mod",	0x1000, 0x9b693243, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-61.3a.mod",	0x1000, 0x5ed44beb, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "video_sound_rom_10_std.ic12",		0x1000, 0xb5c70082, 3 | BRF_PRG | BRF_ESS }, // 16 soundcpu_b

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistarcsm)
STD_ROM_FN(sinistarcsm)

struct BurnDriver BurnDrvSinistarcsm = {
	"sinistarcsm", "sinistar", NULL, NULL, "2023",
	"Sinistar (Cockpit, SynaMax difficulty/titlescreen hack)\0", "initial boot will black screen, hit reset to fix", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarcsmRomInfo, sinistarcsmRomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarCockpitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};

// Sinistar (SynaMax 2025 difficulty/titlescreen hack, revision 3, upright)

static struct BurnRomInfo sinistarsm25RomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-62.4c.mod25",0x1000, 0xfbd8eaf5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-63.4a.mod25",0x1000, 0xbd8c4f3f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-53.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-54.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-55.1a.mod25",0x1000, 0x704ed653, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-56.2d.mod25",0x1000, 0x421a47eb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-57.2c.mod25",0x1000, 0x81251407, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-58.2a.mod25",0x1000, 0x3bc54ce4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-59.3d.mod25",0x1000, 0xe965401d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-60.3c.mod25",0x1000, 0xeec28cb8, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-61.3a.mod25",0x1000, 0x551ba9d4, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistarsm25)
STD_ROM_FN(sinistarsm25)

struct BurnDriver BurnDrvSinistarsm25 = {
	"sinistarsm25", "sinistar", NULL, NULL, "2025",
	"Sinistar (SynaMax 2025 difficulty/titlescreen hack)\0", "initial boot will black screen, hit reset to fix", "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarsm25RomInfo, sinistarsm25RomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};

// Sinistar (revision 2, upright)
// solid RED labels with final production part numbers

static struct BurnRomInfo sinistar2RomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-47.4c",	0x1000, 0x3d670417, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-48.4a",	0x1000, 0x792c8b00, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-38.1d",		0x1000, 0xf6f3a22c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-39.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-40.1a",		0x1000, 0x1ce1b3cc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-41.2d",		0x1000, 0x6da632ba, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-42.2c",		0x1000, 0xb662e8fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-43.2a",		0x1000, 0x2306183d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-44.3d",		0x1000, 0xe5dd918e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-45.3c",		0x1000, 0xd7ecee45, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-46.3a",		0x1000, 0x50cb63ad, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1_16-3004-52.ic7",	0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1_16-3004-50.ic5",	0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1_16-3004-51.ic6",	0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1_16-3004-49.ic4",	0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808.ic12",		0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistar2)
STD_ROM_FN(sinistar2)

struct BurnDriver BurnDrvSinistar2 = {
	"sinistar2", "sinistar", NULL, NULL, "1982",
	"Sinistar (revision 2, upright)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistar2RomInfo, sinistar2RomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Sinistar (AMOA-82 prototype)
// solid pink labels - 1982 AMOA prototype

static struct BurnRomInfo sinistarpRomDesc[] = {
	{ "sinistar_rom_10-b_16-3004-21.4c",	0x1000, 0xea87a53f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "sinistar_rom_11-b_16-3004-22.4a",	0x1000, 0x88d36e80, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sinistar_rom_1-b_16-3004-12.1d",		0x1000, 0x3810d7b8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sinistar_rom_2-b_16-3004-13.1c",		0x1000, 0xcab3185c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sinistar_rom_3-b_16-3004-14.1a",		0x1000, 0x7c984ca9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sinistar_rom_4-b_16-3004-15.2d",		0x1000, 0xcc6c4f24, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sinistar_rom_5-b_16-3004-16.2c",		0x1000, 0x12285bfe, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sinistar_rom_6-b_16-3004-17.2a",		0x1000, 0x7a675f35, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sinistar_rom_7-b_16-3004-18.3d",		0x1000, 0xb0463243, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sinistar_rom_8-b_16-3004-19.3c",		0x1000, 0x909040d4, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sinistar_rom_9-b_16-3004-20.3a",		0x1000, 0xcc949810, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "3004_speech_ic7_r1.ic7",				0x1000, 0xe1019568, 2 | BRF_PRG | BRF_ESS }, // 11 M6800 Code
	{ "3004_speech_ic5_r1.ic5",				0x1000, 0xcf3b5ffd, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "3004_speech_ic6_r1.ic6",				0x1000, 0xff8d2645, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "3004_speech_ic4_r1.ic4",				0x1000, 0x4b56a626, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "video_sound_rom_9_std.808",			0x1000, 0xb82f4ddb, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "decoder_rom_4.3g",					0x0200, 0xe6631c23, 0 | BRF_OPT },           // 16 Address Decoder
	{ "decoder_rom_6.3c",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(sinistarp)
STD_ROM_FN(sinistarp)

struct BurnDriver BurnDrvSinistarp = {
	"sinistarp", "sinistar", NULL, NULL, "1982",
	"Sinistar (AMOA-82 prototype)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinistarpRomInfo, sinistarpRomName, NULL, NULL, NULL, NULL, SinistarInputInfo, NULL,
	SinistarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// PlayBall! (prototype)

static struct BurnRomInfo playballRomDesc[] = {
	{ "playball.10",	0x1000, 0x18787b52, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "playball.11",	0x1000, 0x1dd5c8f2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "playball.12",	0x1000, 0xa700597b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "playball.01",	0x1000, 0x7ba8fd71, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "playball.02",	0x1000, 0x2387c3d4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "playball.03",	0x1000, 0xd34cc5fd, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "playball.04",	0x1000, 0xf68c3a8e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "playball.05",	0x1000, 0xa3f20810, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "playball.06",	0x1000, 0xf213e48e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "playball.07",	0x1000, 0x9b5574e9, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "playball.08",	0x1000, 0xb2d2074a, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "playball.09",	0x1000, 0xc4566d0f, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "speech.ic4",		0x1000, 0x7e4fc798, 2 | BRF_PRG | BRF_ESS }, // 12 soundcpu
	{ "speech.ic5",		0x1000, 0xddfe860c, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "speech.ic6",		0x1000, 0x8bfebf87, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "speech.ic7",		0x1000, 0xdb351db6, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "playball.snd",	0x1000, 0xf3076f9f, 2 | BRF_PRG | BRF_ESS }, // 16
};

STD_ROM_PICK(playball)
STD_ROM_FN(playball)

static INT32 PlayballInit()
{
	playball = 1;

	INT32 nRet = DrvInit(1, 5, 6, 1, 0xc000);

	if (nRet == 0)
	{
		hc55516_init(M6800TotalCycles, 894886);
		uses_hc55516 = 1;

		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;

		pia_init();
		pia_config(0, 0, &pia_0);
		pia_config(1, 0, &pia_1);
		pia_config(2, 0, &pia_2_sinistar);
		pia_config(3, 0, &pia_3);
	}

	return nRet;
}

struct BurnDriver BurnDrvPlayball = {
	"playball", NULL, NULL, NULL, "1983",
	"PlayBall! (prototype)\0", NULL, "Williams", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, playballRomInfo, playballRomName, NULL, NULL, NULL, NULL, PlayballInputInfo, NULL,
	PlayballInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 292, 3, 4
};


// Lotto Fun

static struct BurnRomInfo lottofunRomDesc[] = {
	{ "vl7a.dat",			0x1000, 0xfb2aec2c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "vl7c.dat",			0x1000, 0x9a496519, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vl7e.dat",			0x1000, 0x032cab4b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vl4e.dat",			0x1000, 0x5e9af236, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vl4c.dat",			0x1000, 0x4b134ae2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vl4a.dat",			0x1000, 0xb2f1f95a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vl5e.dat",			0x1000, 0xc8681c55, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vl5c.dat",			0x1000, 0xeb9351e0, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "vl5a.dat",			0x1000, 0x534f2fa1, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "vl6e.dat",			0x1000, 0xbefac592, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "vl6c.dat",			0x1000, 0xa73d7f13, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "vl6a.dat",			0x1000, 0x5730a43d, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "vl2532.snd",			0x1000, 0x214b8a04, 2 | BRF_PRG | BRF_ESS }, // 12 M6800 Code
};

STD_ROM_PICK(lottofun)
STD_ROM_FN(lottofun)

static INT32 LottofunInit()
{
	lottofun = 1;

	INT32 rc = DrvInit(1, 0, 6, 1, 0xc000);
	if (!rc) {
		pStartDraw = DrvDrawBegin;
		pDrawScanline = DrvDrawLine;
	}
	return rc;
}

struct BurnDriver BurnDrvLottofun = {
	"lottofun", NULL, NULL, NULL, "1987",
	"Lotto Fun\0", NULL, "HAR Management", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, lottofunRomInfo, lottofunRomName, NULL, NULL, NULL, NULL, LottofunInputInfo, NULL,
	LottofunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	286, 240, 4, 3
};


// Blaster
// 20 Level version - Each ROM label had an additional "PROTO5" or "PROTO6" sticker attached (verified on multiple PCBs)

static struct BurnRomInfo blasterRomDesc[] = {
	{ "proto6_blaster_3021_rom_16.ic39",		0x1000, 0x54a40b21, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "proto6_blaster_3021_rom_13.ic27",		0x2000, 0xf4dae4c8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "proto6_blaster_3021_rom_11.ic25",		0x2000, 0x6371e62f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "proto6_blaster_3021_rom_12.ic26",		0x2000, 0x9804faac, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "proto6_blaster_3021_rom_17.ic41",		0x1000, 0xbf96182f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "proto5_blaster_3021_rom_15.ic38",		0x4000, 0x1ad146a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "proto5_blaster_3021_rom_8.ic20",			0x4000, 0xf110bbb0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "proto5_blaster_3021_rom_9.ic22",			0x4000, 0x5c5b0f8a, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "proto5_blaster_3021_rom_10.ic24",		0x4000, 0xd47eb67f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "proto5_blaster_3021_rom_6.ic13",			0x4000, 0x47fc007e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "proto5_blaster_3021_rom_5.ic11",			0x4000, 0x15c1b94d, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "proto5_blaster_3021_rom_14.ic35",		0x4000, 0xaea6b846, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "proto6_blaster_3021_rom_7.ic15",			0x4000, 0x7a101181, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "proto5_blaster_3021_rom_1.ic1",			0x4000, 0x8d0ea9e7, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "proto5_blaster_3021_rom_2.ic3",			0x4000, 0x03c4012c, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "proto6_blaster_3021_rom_4.ic7",			0x4000, 0xfc9d39fb, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "proto6_blaster_3021_rom_3.ic6",			0x4000, 0x253690fb, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "proto5_blaster_3021_rom_18.sb13",		0x1000, 0xc33a3145, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 #0 Code

	{ "proto5_blaster_3021_rom_18.sb10",		0x1000, 0xc33a3145, 3 | BRF_PRG | BRF_ESS }, // 18 M6800 #1 Code

	{ "decoder_rom_4.ic42",						0x0200, 0xe6631c23, 0 | BRF_OPT },           // 19 proms
	{ "video_decoder_rom_6.ic23",				0x0200, 0x83faf25e, 0 | BRF_OPT },           // 20
	{ "blaster.col",							0x0800, 0xbac50bc4, 4 | BRF_GRA },           // 21
};

STD_ROM_PICK(blaster)
STD_ROM_FN(blaster)

static INT32 BlasterInit()
{
	blaster = 1;
	is_blasterkit = 0;

	INT32 nRet = DrvInit(2, 0, 6, 2, 0x9700);

	if (nRet == 0)
	{
		pStartDraw = DrvDrawBegin;
		pDrawScanline = BlasterDrawLine;
	}

	return nRet;
}

static INT32 BlasterkitInit()
{
	blaster = 1;
	is_blasterkit = 1;

	INT32 nRet = DrvInit(2, 0, 6, 2, 0x9700);

	if (nRet == 0)
	{
		pStartDraw = DrvDrawBegin;
		pDrawScanline = BlasterDrawLine;
	}

	return nRet;
}

struct BurnDriver BurnDrvBlaster = {
	"blaster", NULL, NULL, NULL, "1983",
	"Blaster\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blasterRomInfo, blasterRomName, NULL, NULL, NULL, NULL, BlasterInputInfo, NULL,
	BlasterInit, DrvExit, DrvFrame, BlasterDraw, DrvScan, &DrvRecalc, 0x110,
	292, 240, 4, 3
};


// Blaster (conversion kit)
// 20 Level version with single sound board & mono sound

static struct BurnRomInfo blasterkitRomDesc[] = {
	{ "blastkit_rom_16.ic39",			0x1000, 0x414b2abf, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "blastkit_rom_13.ic27",			0x2000, 0x9c64db76, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "blastkit_rom_11.ic25",			0x2000, 0xb7df4914, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "blastkit_rom_12.ic26",			0x2000, 0x8b1e26ab, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "blastkit_rom_17.ic41",			0x1000, 0x577d1e9a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "blastkit_rom_15.ic38",			0x4000, 0x1ad146a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "blastkit_rom_8.ic20",			0x4000, 0xf110bbb0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "blastkit_rom_9.ic22",			0x4000, 0x5c5b0f8a, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "blastkit_rom_10.ic24",			0x4000, 0xd47eb67f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "blastkit_rom_6.ic13",			0x4000, 0x47fc007e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "blastkit_rom_5.ic11",			0x4000, 0x15c1b94d, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "blastkit_rom_14.ic35",			0x4000, 0xaea6b846, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "blastkit_rom_7.ic15",			0x4000, 0x6fcc2153, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "blastkit_rom_1.ic1",				0x4000, 0x8d0ea9e7, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "blastkit_rom_2.ic3",				0x4000, 0x03c4012c, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "blastkit_rom_4.ic7",				0x4000, 0xf80e9ff5, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "blastkit_rom_3.ic6",				0x4000, 0x20e851f9, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "blastkit_rom_18",				0x1000, 0xc33a3145, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 Code (single mono sound board)

	{ "decoder_rom_4.ic42",				0x0200, 0xe6631c23, 0 | BRF_OPT },           // 18 proms
	{ "video_decoder_rom_6.ic23",		0x0200, 0x83faf25e, 0 | BRF_OPT },           // 19
	{ "blaster.col",					0x0800, 0xbac50bc4, 4 | BRF_GRA },           // 20
};

STD_ROM_PICK(blasterkit)
STD_ROM_FN(blasterkit)

struct BurnDriver BurnDrvBlasterkit = {
	"blasterkit", "blaster", NULL, NULL, "1983",
	"Blaster (conversion kit)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blasterkitRomInfo, blasterkitRomName, NULL, NULL, NULL, NULL, BlasterkitInputInfo, NULL,
	BlasterkitInit, DrvExit, DrvFrame, BlasterDraw, DrvScan, &DrvRecalc, 0x110,
	292, 240, 4, 3
};


// Blaster (location test)
// 30 Level version

static struct BurnRomInfo blasteroRomDesc[] = {
	{ "proto5_blaster_3021_rom_16.ic39",			0x1000, 0x2db032d2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "proto5_blaster_3021_rom_13.ic27",			0x2000, 0xc99213c7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "proto5_blaster_3021_rom_11.ic25",			0x2000, 0xbc2d7eda, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "proto5_blaster_3021_rom_12.ic26",			0x2000, 0x8a215017, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "proto5_blaster_3021_rom_17.ic41",			0x1000, 0xb308f0e5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "proto5_blaster_3021_rom_15.ic38",			0x4000, 0x1ad146a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "proto5_blaster_3021_rom_8.ic20",				0x4000, 0xf110bbb0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "proto5_blaster_3021_rom_9.ic22",				0x4000, 0x5c5b0f8a, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "proto5_blaster_3021_rom_10.ic24",			0x4000, 0xd47eb67f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "proto5_blaster_3021_rom_6.ic13",				0x4000, 0x47fc007e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "proto5_blaster_3021_rom_5.ic11",				0x4000, 0x15c1b94d, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "proto5_blaster_3021_rom_14.ic35",			0x4000, 0xaea6b846, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "proto5_blaster_3021_rom_7.ic15",				0x4000, 0xa1c4db77, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "proto5_blaster_3021_rom_1.ic1",				0x4000, 0x8d0ea9e7, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "proto5_blaster_3021_rom_2.ic3",				0x4000, 0x03c4012c, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "proto5_blaster_3021_rom_4.ic7",				0x4000, 0x39d2a32c, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "proto5_blaster_3021_rom_3.ic6",				0x4000, 0x054c9f1c, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "proto5_blaster_3021_rom_18.sb13",			0x1000, 0xc33a3145, 2 | BRF_PRG | BRF_ESS }, // 17 M6800 #0 Code

	{ "proto5_blaster_3021_rom_18.sb10",			0x1000, 0xc33a3145, 3 | BRF_PRG | BRF_ESS }, // 18 M6800 #1 Code

	{ "decoder_rom_4.ic42",							0x0200, 0xe6631c23, 0 | BRF_OPT },           // 19 proms
	{ "video_decoder_rom_6.ic23",					0x0200, 0x83faf25e, 0 | BRF_OPT },           // 20
	{ "blaster.col",								0x0800, 0xbac50bc4, 4 | BRF_GRA },           // 21
};

STD_ROM_PICK(blastero)
STD_ROM_FN(blastero)

struct BurnDriver BurnDrvBlastero = {
	"blastero", "blaster", NULL, NULL, "1983",
	"Blaster (location test)\0", NULL, "Williams / Vid Kidz", "6809 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blasteroRomInfo, blasteroRomName, NULL, NULL, NULL, NULL, BlasterInputInfo, NULL,
	BlasterInit, DrvExit, DrvFrame, BlasterDraw, DrvScan, &DrvRecalc, 0x110,
	292, 240, 4, 3
};
