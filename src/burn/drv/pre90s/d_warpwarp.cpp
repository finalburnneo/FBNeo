// Warp Warp emu-layer for FB Alpha by dink, based on Mirko & Chris Hardy's MAME driver.
// Notes:

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
#include "bitswap.h"
#include <math.h>

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvGFX1ROM;
static UINT8 *DrvCharGFX;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT32 m_ball_on;
static UINT32 m_ball_h;
static UINT32 m_ball_v;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDip[2] = {0, 0};
static UINT8 DrvInput[5];
static INT32 DrvAnalogPort0 = 0;
static UINT8 use_paddle = 0;
static UINT8 DrvReset;

static UINT32 rockola = 0;
static UINT32 navaronemode = 0;
static UINT32 bombbeemode = 0;
static UINT32 kaiteimode = 0;

static UINT8 ball_size_x;
static UINT8 ball_size_y;
static UINT32 ball_color;
static UINT8 geebee_bgw;

static struct BurnInputInfo WarpwarpInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"   },
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"  },
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1" },

	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"  },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"     },
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"   },
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"       },
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"       },
};

STDINPUTINFO(Warpwarp)


static struct BurnDIPInfo WarpwarpDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x85, NULL		        },
	{0x0b, 0xff, 0xff, 0x20, NULL		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x0a, 0x01, 0x03, 0x03, "2 Coins 1 Credit" },
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  1 Credit" },
	{0x0a, 0x01, 0x03, 0x02, "1 Coin  2 Credits"},
	{0x0a, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		    },
	{0x0a, 0x01, 0x0c, 0x00, "2"		        },
	{0x0a, 0x01, 0x0c, 0x04, "3"		        },
	{0x0a, 0x01, 0x0c, 0x08, "4"		        },
	{0x0a, 0x01, 0x0c, 0x0c, "5"		        },

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0x30, 0x00, "8k 30k 30k+"		},
	{0x0a, 0x01, 0x30, 0x10, "10k 40k 40k+"		},
	{0x0a, 0x01, 0x30, 0x20, "15k 60k 60k+"		},
	{0x0a, 0x01, 0x30, 0x30, "None"		        },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"		        },
	{0x0a, 0x01, 0x40, 0x00, "On"		        },

	{0   , 0xfe, 0   ,    2, "Level Selection"	},
	{0x0a, 0x01, 0x80, 0x80, "Off"		        },
	{0x0a, 0x01, 0x80, 0x00, "On"		        },

	{0   , 0xfe, 0   ,    2, "Service Mode"     },
	{0x0b, 0x01, 0x20, 0x20, "Off"		        },
	{0x0b, 0x01, 0x20, 0x00, "On"		        },
};

STDDIPINFO(Warpwarp)

static struct BurnDIPInfo WarpwarprDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x05, NULL		        },
	{0x0b, 0xff, 0xff, 0xa0, NULL		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x0a, 0x01, 0x03, 0x03, "2 Coins 1 Credit" },
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  1 Credit" },
	{0x0a, 0x01, 0x03, 0x02, "1 Coin  2 Credits"},
	{0x0a, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		    },
	{0x0a, 0x01, 0x0c, 0x00, "2"		        },
	{0x0a, 0x01, 0x0c, 0x04, "3"		        },
	{0x0a, 0x01, 0x0c, 0x08, "4"		        },
	{0x0a, 0x01, 0x0c, 0x0c, "5"		        },

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0x30, 0x00, "8k 30k 30k+"		},
	{0x0a, 0x01, 0x30, 0x10, "10k 40k 40k+"		},
	{0x0a, 0x01, 0x30, 0x20, "15k 60k 60k+"		},
	{0x0a, 0x01, 0x30, 0x30, "None"		        },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x40, 0x40, "Off"		        },
	{0x0a, 0x01, 0x40, 0x00, "On"		        },

	{0   , 0xfe, 0   ,    2, "High Score Name"	},
	{0x0a, 0x01, 0x80, 0x80, "No"		        },
	{0x0a, 0x01, 0x80, 0x00, "Yes"		        },

	{0   , 0xfe, 0   ,    2, "Service Mode"     },
	{0x0b, 0x01, 0x20, 0x20, "Off"		        },
	{0x0b, 0x01, 0x20, 0x00, "On"		        },
};

STDDIPINFO(Warpwarpr)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GeebeeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	A("P1 Paddle",  BIT_ANALOG_REL, &DrvAnalogPort0,"p1 z-axis"),
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
};

STDINPUTINFO(Geebee)


static struct BurnDIPInfo GeebeeDIPList[]=
{
	{0x05, 0xff, 0xff, 0xd0, NULL		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x05, 0x01, 0x02, 0x00, "3"		},
	{0x05, 0x01, 0x02, 0x02, "5"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x05, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x05, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x05, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x05, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Replay"		},
	{0x05, 0x01, 0x30, 0x10, "40k 80k"		},
	{0x05, 0x01, 0x30, 0x20, "70k 140k"		},
	{0x05, 0x01, 0x30, 0x30, "100k 200k"		},
	{0x05, 0x01, 0x30, 0x00, "None"		},
	{0x05, 0x01, 0x30, 0x10, "60k 120k"		},
	{0x05, 0x01, 0x30, 0x20, "100k 200k"		},
	{0x05, 0x01, 0x30, 0x30, "150k 300k"		},
	{0x05, 0x01, 0x30, 0x00, "None"		},
};

STDDIPINFO(Geebee)

static struct BurnInputInfo BombbeeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
   A("P1 Paddle",   BIT_ANALOG_REL, &DrvAnalogPort0,"p1 z-axis"),
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"},
};

STDINPUTINFO(Bombbee)
#undef A


static struct BurnDIPInfo BombbeeDIPList[]=
{
	{0x06, 0xff, 0xff, 0x03, NULL		            },
	{0x07, 0xff, 0xff, 0x20, NULL		            },

	{0   , 0xfe, 0   ,    4, "Coinage"		        },
	{0x06, 0x01, 0x03, 0x02, "2 Coins 1 Credit"		},
	{0x06, 0x01, 0x03, 0x03, "1 Coin 1 Credit"		},
	{0x06, 0x01, 0x03, 0x01, "1 Coin 2 Credits"		},
	{0x06, 0x01, 0x03, 0x00, "Free Play"		    },

	{0   , 0xfe, 0   ,    3, "Lives"		        },
	{0x06, 0x01, 0x0c, 0x00, "3"		            },
	{0x06, 0x01, 0x0c, 0x04, "4"		            },
	{0x06, 0x01, 0x0c, 0x0c, "5"		            },

	{0   , 0xfe, 0   ,    2, "Unused"		        },
	{0x06, 0x01, 0x10, 0x10, "Off"		            },
	{0x06, 0x01, 0x10, 0x00, "On"		            },

	{0   , 0xfe, 0   ,    8, "Replay"		        },
	{0x06, 0x01, 0xe0, 0x00, "50000"		        },
	{0x06, 0x01, 0xe0, 0x20, "60000"		        },
	{0x06, 0x01, 0xe0, 0x40, "70000"		        },
	{0x06, 0x01, 0xe0, 0x60, "80000"		        },
	{0x06, 0x01, 0xe0, 0x80, "100000"		        },
	{0x06, 0x01, 0xe0, 0xa0, "120000"		        },
	{0x06, 0x01, 0xe0, 0xc0, "150000"		        },
	{0x06, 0x01, 0xe0, 0xe0, "None"		            },

	{0   , 0xfe, 0   ,    2, "Service Mode"     },
	{0x07, 0x01, 0x20, 0x20, "Off"		        },
	{0x07, 0x01, 0x20, 0x00, "On"		        },

};

STDDIPINFO(Bombbee)

static struct BurnInputInfo NavaroneInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
};

STDINPUTINFO(Navarone)


static struct BurnDIPInfo NavaroneDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x16, NULL		        },

	{0   , 0xfe, 0   ,    2, "Lives"		    },
	{0x0b, 0x01, 0x02, 0x00, "2"		        },
	{0x0b, 0x01, 0x02, 0x02, "3"		        },

	{0   , 0xfe, 0   ,    4, "Bonus Life"       },
	{0x0b, 0x01, 0x0c, 0x04, "5000/2, 6000/3"	},
	{0x0b, 0x01, 0x0c, 0x08, "6000/2, 7000/3"	},
	{0x0b, 0x01, 0x0c, 0x0c, "7000/2, 8000/3"	},
	{0x0b, 0x01, 0x0c, 0x00, "None"		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x0b, 0x01, 0x30, 0x30, "2 Coins 1 Credit"	},
	{0x0b, 0x01, 0x30, 0x10, "1 Coin 1 Credit"	},
	{0x0b, 0x01, 0x30, 0x20, "1 Coin 2 Credits" },
	{0x0b, 0x01, 0x30, 0x00, "Free Play"        },
};

STDDIPINFO(Navarone)

static struct BurnInputInfo KaiteinInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
};

STDINPUTINFO(Kaitein)


static struct BurnDIPInfo KaiteinDIPList[]=
{
	{0x07, 0xff, 0xff, 0x15, NULL		        },

	{0   , 0xfe, 0   ,    4, "Lives"		    },
	{0x07, 0x01, 0x03, 0x00, "2"		        },
	{0x07, 0x01, 0x03, 0x01, "3"		        },
	{0x07, 0x01, 0x03, 0x02, "4"		        },
	{0x07, 0x01, 0x03, 0x03, "5"		        },

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x07, 0x01, 0x0c, 0x04, "2000"		        },
	{0x07, 0x01, 0x0c, 0x08, "4000"		        },
	{0x07, 0x01, 0x0c, 0x0c, "6000"		        },
	{0x07, 0x01, 0x0c, 0x00, "None"		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x07, 0x01, 0x30, 0x30, "2 Coins 1 Credit"	},
	{0x07, 0x01, 0x30, 0x10, "1 Coin 1 Credit"	},
	{0x07, 0x01, 0x30, 0x20, "1 Coin 2 Credits"	},
	{0x07, 0x01, 0x30, 0x00, "Free Play"		},
};

STDDIPINFO(Kaitein)

static struct BurnInputInfo KaiteiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
};

STDINPUTINFO(Kaitei)


static struct BurnDIPInfo KaiteiDIPList[]=
{
	{0x06, 0xff, 0xff, 0x1e, NULL		        },

	{0   , 0xfe, 0   ,    4, "Lives"		    },
	{0x06, 0x01, 0x06, 0x06, "4"		        },
	{0x06, 0x01, 0x06, 0x04, "5"		        },
	{0x06, 0x01, 0x06, 0x02, "6"		        },
	{0x06, 0x01, 0x06, 0x00, "7"		        },

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x06, 0x01, 0x18, 0x18, "4000"		        },
	{0x06, 0x01, 0x18, 0x10, "6000"		        },
	{0x06, 0x01, 0x18, 0x08, "8000"		        },
	{0x06, 0x01, 0x18, 0x00, "10000"		    },

	{0   , 0xfe, 0   ,    2, "Unused"		    },
	{0x06, 0x01, 0x20, 0x00, "Off"		        },
	{0x06, 0x01, 0x20, 0x20, "On"		        },
};

STDDIPINFO(Kaitei)

static struct BurnInputInfo SosInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
};

STDINPUTINFO(Sos)


static struct BurnDIPInfo SosDIPList[]=
{
	{0x07, 0xff, 0xff, 0x2a, NULL		        },

	{0   , 0xfe, 0   ,    4, "Lives"		    },
	{0x07, 0x01, 0x06, 0x00, "2"		        },
	{0x07, 0x01, 0x06, 0x02, "3"		        },
	{0x07, 0x01, 0x06, 0x04, "4"		        },
	{0x07, 0x01, 0x06, 0x06, "5"		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x07, 0x01, 0x18, 0x18, "2 Coins 1 Credit"	},
	{0x07, 0x01, 0x18, 0x08, "1 Coin 1 Credit"	},
	{0x07, 0x01, 0x18, 0x10, "1 Coin 2 Credits"	},
	{0x07, 0x01, 0x18, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Nudity"		    },
	{0x07, 0x01, 0x20, 0x00, "Off"		        },
	{0x07, 0x01, 0x20, 0x20, "On"		        },
};

STDDIPINFO(Sos)


// Warp Warp soundchip emulation
static INT16 *decay = NULL;
static INT32 sound_latch = 0;
static INT32 music1_latch = 0;
static INT32 music2_latch = 0;
static INT32 sound_signal = 0;
static INT32 sound_volume = 0;
static INT32 sound_volume_timer = 0;
static INT32 music_signal = 0;
static INT32 music_volume = 0;
static INT32 music_volume_timer = 0;
static INT32 noise = 0;

static void warpwarp_sound_reset()
{
	sound_latch = 0;
	music1_latch = 0;
	music2_latch = 0;
	sound_signal = 0;
	sound_volume = 0;
	sound_volume_timer = 0;
	music_signal = 0;
	music_volume = 0;
	music_volume_timer = 0;
	noise = 0;
}

static void warpwarp_sound_scan()
{
	SCAN_VAR(sound_latch);
	SCAN_VAR(music1_latch);
	SCAN_VAR(music2_latch);
	SCAN_VAR(sound_signal);
	SCAN_VAR(sound_volume);
	SCAN_VAR(sound_volume_timer);
	SCAN_VAR(music_signal);
	SCAN_VAR(music_volume);
	SCAN_VAR(music_volume_timer);
	SCAN_VAR(noise);
}

static void sound_volume_decay()
{
	if( --sound_volume < 0 )
		sound_volume = 0;
}

static void warpwarp_sound_w(UINT8 data)
{
	sound_latch = data & 0x0f;
	sound_volume = 0x7fff; /* set sound_volume */
	noise = 0x0000;  /* reset noise shifter */

	if( sound_latch & 8 ) {
		sound_volume_timer = 1;
	} else {
		sound_volume_timer = 2;
	}
}

static void warpwarp_music1_w(UINT8 data)
{
	music1_latch = data & 0x3f;
}

static void music_volume_decay()
{
	if( --music_volume < 0 )
        music_volume = 0;
}

static void warpwarp_music2_w(UINT8 data)
{
	music2_latch = data & 0x3f;
	music_volume = 0x7fff;

	if( music2_latch & 0x10 ) {
		music_volume_timer = 1;
	} else {
		music_volume_timer = 3;
	}
}

static void warpwarp_timer_tiktiktik(INT32 /*ticks*/)
{// warpwarp_timer_tiktiktik(); must be called 256 times per frame
	switch (music_volume_timer) {
		case 1: music_volume_decay();  // no breaks!
		case 2: music_volume_decay();
		case 3: music_volume_decay();
	}
	switch (sound_volume_timer) {
		case 1: sound_volume_decay();
		case 2: sound_volume_decay();
		case 3: sound_volume_decay();
	}
}

static void warpwarp_sound_update(INT16 *buffer, INT32 length)
{
    static int vcarry = 0;
    static int vcount = 0;
    static int mcarry = 0;
	static int mcount = 0;

	memset(buffer, 0, length * 2 * sizeof(INT16));

    while (length--)
    {
		*buffer++ = (sound_signal + music_signal) / 4;
		*buffer++ = (sound_signal + music_signal) / 4;

		mcarry -= (18432000/3/2/16) / (4 * (64 - music1_latch));
		while( mcarry < 0 )
		{
			mcarry += nBurnSoundRate;
			mcount++;
			music_signal = (mcount & ~music2_latch & 15) ? decay[music_volume] : 0;
			/* override by noise gate? */
			if( (music2_latch & 32) && (noise & 0x8000) )
				music_signal = decay[music_volume];
		}

		/* clock 1V = 8kHz */
		vcarry -= (18432000/3/2/384);
        while (vcarry < 0)
        {
            vcarry += nBurnSoundRate;
            vcount++;

            /* noise is clocked with raising edge of 2V */
			if ((vcount & 3) == 2)
			{
				/* bit0 = bit0 ^ !bit10 */
				if ((noise & 1) == ((noise >> 10) & 1))
					noise = (noise << 1) | 1;
				else
					noise = noise << 1;
			}

            switch (sound_latch & 7)
            {
            case 0: /* 4V */
				sound_signal = (vcount & 0x04) ? decay[sound_volume] : 0;
                break;
            case 1: /* 8V */
				sound_signal = (vcount & 0x08) ? decay[sound_volume] : 0;
                break;
            case 2: /* 16V */
				sound_signal = (vcount & 0x10) ? decay[sound_volume] : 0;
                break;
            case 3: /* 32V */
				sound_signal = (vcount & 0x20) ? decay[sound_volume] : 0;
                break;
            case 4: /* TONE1 */
				sound_signal = !(vcount & 0x01) && !(vcount & 0x10) ? decay[sound_volume] : 0;
                break;
            case 5: /* TONE2 */
				sound_signal = !(vcount & 0x02) && !(vcount & 0x20) ? decay[sound_volume] : 0;
                break;
            case 6: /* TONE3 */
				sound_signal = !(vcount & 0x04) && !(vcount & 0x40) ? decay[sound_volume] : 0;
                break;
			default: /* NOISE */
				sound_signal = (noise & 0x8000) ? decay[sound_volume] : 0;
            }
        }
    }
}

static void warpwarp_sound_init()
{
	decay = (INT16 *) BurnMalloc(32768 * sizeof(INT16));

    for(INT32 i = 0; i < 0x8000; i++ )
		decay[0x7fff-i] = (INT16) (0x7fff/exp(1.0*i/4096));

	sound_volume_timer = 0;
	music_volume_timer = 0;
}

static void warpwarp_sound_deinit()
{
	if (decay)
		BurnFree(decay);
	decay = NULL;
}

#define MAX_NETS        3
#define MAX_RES_PER_NET 18
#define Combine2Weights(tab,w0,w1)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1)) + 0.5))
#define Combine3Weights(tab,w0,w1,w2)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1) + (tab)[2]*(w2)) + 0.5))

static double ComputeResistorWeights(INT32 MinVal, INT32 MaxVal, double Scaler, INT32 Count1, const INT32 *Resistances1, double *Weights1, INT32 PullDown1, INT32 PullUp1,	INT32 Count2, const INT32 *Resistances2, double *Weights2, INT32 PullDown2, INT32 PullUp2, INT32 Count3, const INT32 *Resistances3, double *Weights3, INT32 PullDown3, INT32 PullUp3)
{
	INT32 NetworksNum;

	INT32 ResCount[MAX_NETS];
	double r[MAX_NETS][MAX_RES_PER_NET];
	double w[MAX_NETS][MAX_RES_PER_NET];
	double ws[MAX_NETS][MAX_RES_PER_NET];
	INT32 r_pd[MAX_NETS];
	INT32 r_pu[MAX_NETS];

	double MaxOut[MAX_NETS];
	double *Out[MAX_NETS];

	INT32 i, j, n;
	double Scale;
	double Max;

	NetworksNum = 0;
	for (n = 0; n < MAX_NETS; n++) {
		INT32 Count, pd, pu;
		const INT32 *Resistances;
		double *Weights;

		switch (n) {
			case 0: {
				Count = Count1;
				Resistances = Resistances1;
				Weights = Weights1;
				pd = PullDown1;
				pu = PullUp1;
				break;
			}
			
			case 1: {
				Count = Count2;
				Resistances = Resistances2;
				Weights = Weights2;
				pd = PullDown2;
				pu = PullUp2;
				break;
			}
		
			case 2:
			default: {
				Count = Count3;
				Resistances = Resistances3;
				Weights = Weights3;
				pd = PullDown3;
				pu = PullUp3;
				break;
			}
		}

		if (Count > 0) {
			ResCount[NetworksNum] = Count;
			for (i = 0; i < Count; i++) {
				r[NetworksNum][i] = 1.0 * Resistances[i];
			}
			Out[NetworksNum] = Weights;
			r_pd[NetworksNum] = pd;
			r_pu[NetworksNum] = pu;
			NetworksNum++;
		}
	}

	for (i = 0; i < NetworksNum; i++) {
		double R0, R1, Vout, Dst;

		for (n = 0; n < ResCount[i]; n++) {
			R0 = (r_pd[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pd[i];
			R1 = (r_pu[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pu[i];

			for (j = 0; j < ResCount[i]; j++) {
				if (j == n) {
					if (r[i][j] != 0.0) R1 += 1.0 / r[i][j];
				} else {
					if (r[i][j] != 0.0) R0 += 1.0 / r[i][j];
				}
			}

			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (MaxVal - MinVal) * R0 / (R1 + R0) + MinVal;

			Dst = (Vout < MinVal) ? MinVal : (Vout > MaxVal) ? MaxVal : Vout;

			w[i][n] = Dst;
		}
	}

	j = 0;
	Max = 0.0;
	for (i = 0; i < NetworksNum; i++) {
		double Sum = 0.0;

		for (n = 0; n < ResCount[i]; n++) Sum += w[i][n];

		MaxOut[i] = Sum;
		if (Max < Sum) {
			Max = Sum;
			j = i;
		}
	}

	if (Scaler < 0.0) {
		Scale = ((double)MaxVal) / MaxOut[j];
	} else {
		Scale = Scaler;
	}

	for (i = 0; i < NetworksNum; i++) {
		for (n = 0; n < ResCount[i]; n++) {
			ws[i][n] = w[i][n] * Scale;
			(Out[i])[n] = ws[i][n];
		}
	}

	return Scale;
}


static void warpwarp_palette_init()
{
	static const int resistances_tiles_rg[3] = { 1600, 820, 390 };
	static const int resistances_tiles_b[2]  = { 820, 390 };
	static const int resistance_ball[1]      = { 220 };

	double weights_tiles_rg[3], weights_tiles_b[2], weight_ball[1];

	ComputeResistorWeights(0, 0xff, -1.0,
								3, &resistances_tiles_rg[0], weights_tiles_rg, 150, 0,
								2, &resistances_tiles_b[0],  weights_tiles_b,  150, 0,
								1, &resistance_ball[0],      weight_ball,      150, 0);

	for (INT32 i = 0; i < 0x100; i++)
	{
		int bit0, bit1, bit2;
		int r,g,b;

		/* red component */
		bit0 = (i >> 0) & 0x01;
		bit1 = (i >> 1) & 0x01;
		bit2 = (i >> 2) & 0x01;
		r = Combine3Weights(weights_tiles_rg, bit0, bit1, bit2);

		/* green component */
		bit0 = (i >> 3) & 0x01;
		bit1 = (i >> 4) & 0x01;
		bit2 = (i >> 5) & 0x01;
		g = Combine3Weights(weights_tiles_rg, bit0, bit1, bit2);

		/* blue component */
		bit0 = (i >> 6) & 0x01;
		bit1 = (i >> 7) & 0x01;
		b = Combine2Weights(weights_tiles_b, bit0, bit1);

		DrvPalette[(i * 2) + 0] = BurnHighCol(0, 0, 0, 0);;
		DrvPalette[(i * 2) + 1] = BurnHighCol(r, g, b, 0);;
	}

	DrvPalette[0x200] = BurnHighCol((INT32)weight_ball[0], (INT32)weight_ball[0], (INT32)weight_ball[0], 0);
}

static UINT32 geebee_palette[3];

static void geebee_palette_common()
{
	geebee_palette[0] = BurnHighCol(0x00, 0x00, 0x00, 0); /* black */
	geebee_palette[1] = BurnHighCol(0xff, 0xff, 0xff, 0); /* white */
	geebee_palette[2] = BurnHighCol(0x7f, 0x7f, 0x7f, 0); /* grey  */
}

#if 0
static void geebee_palette_init()
{
	geebee_palette_common();
	DrvPalette[0] = geebee_palette[0];
	DrvPalette[1] = geebee_palette[1];
	DrvPalette[2] = geebee_palette[1];
	DrvPalette[3] = geebee_palette[0];
	DrvPalette[4] = geebee_palette[0];
	DrvPalette[5] = geebee_palette[2];
	DrvPalette[6] = geebee_palette[2];
	DrvPalette[7] = geebee_palette[0];
}
#endif

static void navarone_palette_init()
{
	geebee_palette_common();
	DrvPalette[0] = geebee_palette[0];
	DrvPalette[1] = geebee_palette[1];
	DrvPalette[2] = geebee_palette[0];
	DrvPalette[3] = geebee_palette[2];
	DrvPalette[4] = geebee_palette[0];
	DrvPalette[5] = geebee_palette[1];
	DrvPalette[6] = geebee_palette[0];
	DrvPalette[7] = geebee_palette[2];
	DrvPalette[8] = geebee_palette[1];
}

#undef MAX_NETS
#undef MAX_RES_PER_NET
#undef Combine2Weights
#undef Combine3Weights

static void DrvMakeInputs()
{
	UINT8 *DrvJoy[4] = { DrvJoy1, DrvJoy2, DrvJoy3, DrvJoy4 };
	UINT32 DrvJoyInit[4] = { 0xff, 0xff, 0x00, 0x00 };

	CompileInput(DrvJoy, (void*)DrvInput, 4, 8, DrvJoyInit); // first two are active low

	ProcessJoystick(&DrvInput[2], 0, 3,2,1,0, INPUT_4WAY);
	ProcessJoystick(&DrvInput[3], 1, 3,2,1,0, INPUT_4WAY);
}

static UINT32 scalerange_skns(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static UINT8 in_paddle()
{
	UINT8 Temp = 0x7f - (DrvAnalogPort0 >> 4);
	if (Temp < 0x01) Temp = 0x01;
	if (Temp > 0xfe) Temp = 0xfe;
	return scalerange_skns(Temp, 0x3f, 0xbe, 0x10, 0xa0);
}

#if 0      // kaitei protection needs fixing.
static UINT8 geebee_in_r(UINT8 offset)
{
	INT32 res;
	//static const char *const portnames[] = { "SW0", "SW1", "DSW2", "PLACEHOLDER" };	// "IN1" & "IN2" are read separately when offset==3

	offset &= 3;
	switch (offset) {
		case 0: res = DrvInput[0] | 0x20 | 0xa0; break;
		case 1: res = ((kaiteimode) ? DrvInput[2] | 0x80 : 0xff); break;
		case 2: res = ((kaiteimode) ? DrvDip[0] | 0x80 : DrvDip[0]); break;
	}
	
//	bprintf(0, _T("%X: res = %X di0 %X \n"), offset, res, DrvInput[0]);
	if (offset == 3)
	{
		res = (use_paddle) ? in_paddle() : DrvInput[2];
		if (kaiteimode) { return 0xc0; }
		//if (res) bprintf(0, _T("res = %X.."), res);
		if (!use_paddle) // joystick-mode
		{
			/* map digital two-way joystick to two fixed VOLIN values */
			if (res & 2) return 0x9f;
			if (res & 1) return 0x0f;
			return 0x60;
		}
	}
	return res;
}
#endif

static UINT8 geebee_in_r(UINT8 offset)
{
	INT32 res = 0;
	//static const char *const portnames[] = { "SW0", "SW1", "DSW2", "PLACEHOLDER" };	// "IN1" & "IN2" are read separately when offset==3

	offset &= 3;
	switch (offset) {
		case 0: res = DrvInput[0] | 0x20; break;
		case 1: res = 0xff; break;
		case 2: res = DrvDip[0]; break;
	}
	
//	bprintf(0, _T("%X: res = %X di0 %X \n"), offset, res, DrvInput[0]);
	if (offset == 3)
	{
		res = (use_paddle) ? in_paddle() : DrvInput[2];

		//if (res) bprintf(0, _T("res = %X.."), res);
		if (!use_paddle) // joystick-mode
		{
			/* map digital two-way joystick to two fixed VOLIN values */
			if (res & 2) return 0x9f;
			if (res & 1) return 0x0f;
			return 0x60;
		}
	}
	return res;
}

static void geebee_out6_w(UINT16 offset, UINT8 data)
{
	switch (offset & 3)
	{
		case 0:
			m_ball_h = data;
			break;
		case 1:
			m_ball_v = data;
			break;
		case 2:
			/* n.c. */
			break;
		case 3:
			warpwarp_sound_w(data); // need to port geebee sound core!
			break;
	}
}

static void geebee_out7_w(UINT16 offset, UINT8 data)
{
	switch (offset & 7)
	{
		case 5:
			geebee_bgw = data & 1;
			break;
		case 6:
			m_ball_on = data & 1;
			break;
		case 7:
			//flip_screen_set(space->machine, data & 1);
			break;
	}
}

static UINT8 warpwarp_sw_r(UINT8 offset)
{
	return (DrvInput[0] >> (offset & 7)) & 1;
}

static UINT8 warpwarp_dsw1_r(UINT8 offset)
{
	return (DrvDip[0] >> (offset & 7)) & 1;
}

static UINT8 warpwarp_vol_r(UINT8 /*offset*/)
{
	INT32 res;

	res = (use_paddle) ? in_paddle() : DrvInput[2];
	if (!use_paddle) {
		if (res & 1) return 0x0f;
		if (res & 2) return 0x3f;
		if (res & 4) return 0x6f;
		if (res & 8) return 0x9f;
		return 0xff;
	}
	//if (res) bprintf(0, _T("res = %X.."), res);
	return res;
}

static void warpwarp_out0_w(UINT16 offset, UINT8 data)
{
	switch (offset & 3)
	{
		case 0:
			m_ball_h = data;
			break;
		case 1:
			m_ball_v = data;
			break;
		case 2:
			warpwarp_sound_w(data);
			break;
		case 3:
			// watchdog reset
			break;
	}
}

static void warpwarp_out3_w(UINT16 offset, UINT8 data)
{
	switch (offset & 7)
	{
		case 6:
			m_ball_on = data & 1;
			if (~data & 1) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
			break;
		case 7:
			// flipscreen = data & 1;
			break;
	}
}

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x6000) address = (address&0xfff) + 0xc000; // bombee/cutieq map

	if (address >= 0xc000 && address <= 0xc00f) {
		warpwarp_out0_w(address - 0xc000, data);
		return;
	}
	if (address >= 0xc010 && address <= 0xc01f) {
		warpwarp_music1_w(data);
		return;
	}
	if (address >= 0xc020 && address <= 0xc02f) {
		warpwarp_music2_w(data);
		return;
	}
	if (address >= 0xc030 && address <= 0xc03f) {
		warpwarp_out3_w(address - 0xc030, data);
		return;
	}
}

static UINT8 __fastcall main_read(UINT16 address)
{
	if ((address&0xf000) == 0x6000)	address = (address&0xfff) + 0xc000; // bombee/cutieq map

	//bprintf(0, _T("new: %X\n"), address);
	if (address >= 0xc000 && address <= 0xc00f) {
		//bprintf(0, _T("sw_r %X  "), address - 0xc000);
		return warpwarp_sw_r(address - 0xc000);
	}
	if (address >= 0xc010 && address <= 0xc01f) {
		return warpwarp_vol_r(address - 0xc010);
	}
	if (address >= 0xc020 && address <= 0xc02f) {
		return warpwarp_dsw1_r(address - 0xc020);
	}

	return 0;
}

static void __fastcall geebee_write(UINT16 address, UINT8 data)
{
	if (address >= 0x6000 && address <= 0x6fff) {
		geebee_out6_w(address - 0x6000, data);
		return;
	}
	if (address >= 0x7000 && address <= 0x7fff) {
		geebee_out7_w(address - 0x7000, data);
		return;
	}
}

static UINT8 __fastcall geebee_read(UINT16 address)
{
	if (address >= 0x5000 && address <= 0x53ff) {
		return geebee_in_r(address - 0x5000);
	}

	return 0;
}

static void __fastcall geebee_out(UINT16 address, UINT8 data)
{
	address &= 0xff;

	if (address >= 0x60 && address <= 0x6f) {
		geebee_out6_w(address, data);
		return;
	}

	if (address >= 0x70 && address <= 0x7f) {
		geebee_out7_w(address, data);
		return;
	}
}

static UINT8 __fastcall geebee_in(UINT16 address)
{
	address &= 0xff;

	if (address >= 0x50 && address <= 0x53) {
		return geebee_in_r(address);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	m_ball_on = 0;
	m_ball_h = 0;
	m_ball_v = 0;

	warpwarp_sound_reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x08000;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x40000;
	DrvGFX1ROM      = Next; Next += 0x01000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x01000;
	DrvVidRAM		= Next; Next += 0x01000;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 CharPlaneOffsets[1] = { 0 };
static INT32 CharXOffsets[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 CharYOffsets[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (!strncmp(BurnDrvGetTextA(DRV_NAME), "geebee", 6)) {
		bprintf(0, _T("geebee mode"));
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvGFX1ROM        , 1, 1)) return 1;
		if (BurnLoadRom(DrvGFX1ROM + 0x0400, 2, 1)) return 1;
		GfxDecode(0x100, 1, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvGFX1ROM, DrvCharGFX);
	} else
	if (bombbeemode) {
		bprintf(0, _T("bombbee/cutieq mode\n"));
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvGFX1ROM        , 1, 1)) return 1;
		GfxDecode(0x100, 1, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvGFX1ROM, DrvCharGFX);

	} else
	if (navaronemode) {
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "kaitei")) {
			bprintf(0, _T("original kaitei mode.\n"));
			if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x0800, 0, 1)) return 1; // re-load
			if (BurnLoadRom(DrvZ80ROM + 0x1000, 1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x1400, 2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x1800, 3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x1c00, 4, 1)) return 1;
			if (BurnLoadRom(DrvGFX1ROM        , 5, 1)) return 1;
			if (BurnLoadRom(DrvGFX1ROM+ 0x0400, 6, 1)) return 1;
		} else {
			bprintf(0, _T("original navalone/kaitein mode.\n"));
			if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x0800, 1, 1)) return 1;
			if (BurnLoadRom(DrvGFX1ROM        , 2 + rockola, 1)) return 1;
		}
		GfxDecode(0x100, 1, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvGFX1ROM, DrvCharGFX);

	} else {   // Load ROMS parse GFX
		bprintf(0, _T("load roms: warpwarp mode\n"));
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000, 2, 1)) return 1;
		if (rockola)
			if (BurnLoadRom(DrvZ80ROM + 0x3000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGFX1ROM        , 3 + rockola, 1)) return 1;
		GfxDecode(0x100, 1, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvGFX1ROM, DrvCharGFX);
	}

	ZetInit(0);
	ZetOpen(0);
	if (bombbeemode) {
		bprintf(0, _T("mapping: bombbee/cutieq mode\n"));
		ZetMapMemory(DrvZ80ROM,		0x0000, 0x1fff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,		0x2000, 0x23ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,		0x4000, 0x47ff, MAP_RAM);
		ZetMapMemory(DrvGFX1ROM,    0x4800, 0x4fff, MAP_RAM);
		ZetSetWriteHandler(main_write);
		ZetSetReadHandler(main_read);
		ball_color = 0x200;
		ball_size_x = 4;
		ball_size_y = 4;
	} else
	if (navaronemode) {
		bprintf(0, _T("mapping: navarone mode\n"));
		ZetMapMemory(DrvZ80ROM,		0x0000, 0x1fff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,		0x4000, 0x40ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,		0x2000, 0x23ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,		0x2400, 0x27ff, MAP_RAM);
		ZetMapMemory(DrvGFX1ROM,    0x3000, 0x37ff, MAP_RAM);
		ZetSetInHandler(geebee_in);
		ZetSetOutHandler(geebee_out);
		ZetSetWriteHandler(geebee_write);
		ZetSetReadHandler(geebee_read);
		ball_color = 7;
	} else { //warpwarp
		bprintf(0, _T("mapping: warpwarp mode\n"));
		ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM,		0x8000, 0x83ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,		0x4000, 0x47ff, MAP_RAM);
		ZetMapMemory(DrvGFX1ROM,    0x4800, 0x4fff, MAP_RAM);
		ZetSetWriteHandler(main_write);
		ZetSetReadHandler(main_read);
		ball_color = 0x200;
		ball_size_x = 4;
		ball_size_y = 4;
	}
	ZetClose();

	GenericTilesInit();

	warpwarp_sound_init();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnFree(AllMem);

	warpwarp_sound_deinit();

	rockola = 0;
	navaronemode = 0;
	bombbeemode = 0;
	kaiteimode = 0;
	use_paddle = 0;

	return 0;
}

static void plot_pixel(INT32 x, INT32 y, INT32 pen)
{
	if (x < 0 || x > nScreenWidth) return;
	if (y < 0 || y > nScreenHeight) return;

	UINT16 *pPixel = pTransDraw + (y * nScreenWidth) + x;
	*pPixel = pen;
}

static void draw_bullet()
{
	if (m_ball_on) {
		INT32 x = 264 - m_ball_h;
		INT32 y = 240 - m_ball_v;

		for (INT32 i = ball_size_y; i > 0; i--)
			for (INT32 j = ball_size_x; j > 0; j--) {
				plot_pixel(x - j, y - i, ball_color);
			}
	}
}

static void draw_chars()
{
	INT32 mx, my, code, color, x, y, offs, row, col;

	for (mx = 0; mx < 28; mx++) {
		for (my = 0; my < 34; my++) {
			row = mx + 2;
			col = my - 1;

			if (col & 0x20)
				offs = row + ((col & 1) << 5);
			else
				offs = col + (row << 5);

			code = DrvVidRAM[offs];
			if (navaronemode) {
				color = ((geebee_bgw & 1) << 1) | ((code & 0x80) >> 7);
			} else { //warpwarp
				color = DrvVidRAM[offs + 0x400];
			}

			y = 8 * mx;
			x = 8 * my;
			Render8x8Tile_Clip(pTransDraw, code, x, y, color, 1, 0, DrvCharGFX);
		}
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		if (navaronemode) {
			navarone_palette_init();
		} else {
			warpwarp_palette_init();
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	draw_chars();
	draw_bullet();

	BurnTransferCopy(DrvPalette);

	return 0;
}


static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 2048000 / 60;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(nCyclesTotal / nInterleave);

		if (i == nInterleave - 1 && m_ball_on)
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);

		if (navaronemode && i == 0 && bombbeemode == 0) // weird timing.
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

		warpwarp_timer_tiktiktik(nCyclesTotal / nInterleave);
	}
	ZetClose();

	if (pBurnSoundOut) {
		warpwarp_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

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

		ZetScan(nAction);

		warpwarp_sound_scan();

		SCAN_VAR(m_ball_on);
		SCAN_VAR(m_ball_h);
		SCAN_VAR(m_ball_v);
		SCAN_VAR(geebee_bgw);
	}

	return 0;
}

// Warp & Warp

static struct BurnRomInfo warpwarpRomDesc[] = {
	{ "ww1_prg1.s10",	0x1000, 0xf5262f38, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ww1_prg2.s8",	0x1000, 0xde8355dd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ww1_prg3.s4",	0x1000, 0xbdd1dec5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ww1_chg1.s12",	0x0800, 0x380994c8, 2 | BRF_GRA },           //  3 gfx1
};

STD_ROM_PICK(warpwarp)
STD_ROM_FN(warpwarp)

struct BurnDriver BurnDrvWarpwarp = {
	"warpwarp", NULL, NULL, NULL, "1981",
	"Warp & Warp\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, warpwarpRomInfo, warpwarpRomName, NULL, NULL, WarpwarpInputInfo, WarpwarpDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 warpwarprDrvInit()
{
	rockola = 1;

	return DrvInit();
}

// Warp Warp (Rock-Ola set 1)

static struct BurnRomInfo warpwarprRomDesc[] = {
	{ "g-09601.2r",	0x1000, 0x916ffa35, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "g-09602.2m",	0x1000, 0x398bb87b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g-09603.1p",	0x1000, 0x6b962fc4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g-09613.1t",	0x0800, 0x60a67e76, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "g-9611.4c",	0x0800, 0x00e6a326, 2 | BRF_GRA },           //  4 gfx1
};

STD_ROM_PICK(warpwarpr)
STD_ROM_FN(warpwarpr)

struct BurnDriver BurnDrvWarpwarpr = {
	"warpwarpr", "warpwarp", NULL, NULL, "1981",
	"Warp Warp (Rock-Ola set 1)\0", NULL, "Namco (Rock-Ola license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, warpwarprRomInfo, warpwarprRomName, NULL, NULL, WarpwarpInputInfo, WarpwarprDIPInfo,
	warpwarprDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};


// Warp Warp (Rock-Ola set 2)

static struct BurnRomInfo warpwarpr2RomDesc[] = {
	{ "g-09601.2r",	0x1000, 0x916ffa35, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "g-09602.2m",	0x1000, 0x398bb87b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g-09603.1p",	0x1000, 0x6b962fc4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g-09612.1t",	0x0800, 0xb91e9e79, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "g-9611.4c",	0x0800, 0x00e6a326, 2 | BRF_GRA },           //  4 gfx1
};

STD_ROM_PICK(warpwarpr2)
STD_ROM_FN(warpwarpr2)

struct BurnDriver BurnDrvWarpwarpr2 = {
	"warpwarpr2", "warpwarp", NULL, NULL, "1981",
	"Warp Warp (Rock-Ola set 2)\0", NULL, "Namco (Rock-Ola license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, warpwarpr2RomInfo, warpwarpr2RomName, NULL, NULL, WarpwarpInputInfo, WarpwarprDIPInfo,
	warpwarprDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 GeebeeInit()
{
	navaronemode = 1;
	ball_size_x = 4;
	ball_size_y = 4;
	use_paddle = 1;

	return DrvInit();
}

// Gee Bee

static struct BurnRomInfo geebeeRomDesc[] = {
	{ "geebee.1k",	0x1000, 0x8a5577e0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "geebee.3a",	0x0400, 0xf257b21b, 1 | BRF_GRA }, //  1 gfx1
};

STD_ROM_PICK(geebee)
STD_ROM_FN(geebee)

struct BurnDriver BurnDrvGeebee = {
	"geebee", NULL, NULL, NULL, "1978",
	"Gee Bee\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, geebeeRomInfo, geebeeRomName, NULL, NULL, GeebeeInputInfo, GeebeeDIPInfo,
	GeebeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};


// Gee Bee (Gremlin)

static struct BurnRomInfo geebeegRomDesc[] = {
	{ "geebee.1k",	0x1000, 0x8a5577e0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "geebeeg.3a",	0x0400, 0xa45932ba, 1 | BRF_GRA }, //  1 gfx1
};

STD_ROM_PICK(geebeeg)
STD_ROM_FN(geebeeg)

struct BurnDriver BurnDrvGeebeeg = {
	"geebeeg", "geebee", NULL, NULL, "1978",
	"Gee Bee (Gremlin)\0", NULL, "[Namco] (Gremlin license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, geebeegRomInfo, geebeegRomName, NULL, NULL, GeebeeInputInfo, GeebeeDIPInfo,
	GeebeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 NavaroneInit()
{
	navaronemode = 1;
	ball_size_x = 4;
	ball_size_y = 4;

	return DrvInit();
}

// Navarone

static struct BurnRomInfo navaroneRomDesc[] = {
	{ "navalone.p1",	0x0800, 0x5a32016b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "navalone.p2",	0x0800, 0xb1c86fe3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "navalone.chr",	0x0800, 0xb26c6170, 2 | BRF_GRA }, //  2 gfx1
};

STD_ROM_PICK(navarone)
STD_ROM_FN(navarone)

struct BurnDriver BurnDrvNavarone = {
	"navarone", NULL, NULL, NULL, "1980",
	"Navarone\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_16BIT_ONLY, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, navaroneRomInfo, navaroneRomName, NULL, NULL, NavaroneInputInfo, NavaroneDIPInfo,
	NavaroneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 KaiteinInit()
{
	navaronemode = 1;
	ball_size_x = 1;
	ball_size_y = 16;

	return DrvInit();
}

// Kaitei Takara Sagashi (Namco license)

static struct BurnRomInfo kaiteinRomDesc[] = {
	{ "kaitein.p1",		0x0800, 0xd88e10ae, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "kaitein.p2",		0x0800, 0xaa9b5763, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kaitein.chr",	0x0800, 0x3125af4d, 2 | BRF_GRA }, //  2 gfx1
};

STD_ROM_PICK(kaitein)
STD_ROM_FN(kaitein)

struct BurnDriver BurnDrvKaitein = {
	"kaitein", "kaitei", NULL, NULL, "1980",
	"Kaitei Takara Sagashi (Namco license)\0", NULL, "K.K. Tokki (Namco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, kaiteinRomInfo, kaiteinRomName, NULL, NULL, KaiteinInputInfo, KaiteinDIPInfo,
	KaiteinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 KaiteiInit()
{
	kaiteimode = 1;

	return KaiteinInit();
}

// Kaitei Takara Sagashi

static struct BurnRomInfo kaiteiRomDesc[] = {
	{ "kaitei_7.1k",	0x0800, 0x32f70d48, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "kaitei_1.1m",	0x0400, 0x9a7ab3b9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kaitei_2.1p",	0x0400, 0x5eeb0fff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kaitei_3.1s",	0x0400, 0x5dff4df7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kaitei_4.1t",	0x0400, 0xe5f303d6, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "kaitei_5.bin",	0x0400, 0x60fdb795, 2 | BRF_GRA }, //  5 gfx1
	{ "kaitei_6.bin",	0x0400, 0x21399ace, 2 | BRF_GRA }, //  6
};

STD_ROM_PICK(kaitei)
STD_ROM_FN(kaitei)

struct BurnDriver BurnDrvKaitei = {
	"kaitei", NULL, NULL, NULL, "1980",
	"Kaitei Takara Sagashi\0", "Freezes up, use kaitein instead!", "K.K. Tokki", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, kaiteiRomInfo, kaiteiRomName, NULL, NULL, KaiteiInputInfo, KaiteiDIPInfo,
	KaiteiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 SOSInit()
{
	navaronemode = 1;

	ball_size_x = 4;
	ball_size_y = 2;

	return DrvInit();
}


// SOS

static struct BurnRomInfo sosRomDesc[] = {
	{ "sos.p1",	0x0800, 0xf70bdafb, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sos.p2",	0x0800, 0x58e9c480, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sos.chr",0x0800, 0x66f983e4, 2 | BRF_GRA }, //  2 gfx1
};

STD_ROM_PICK(sos)
STD_ROM_FN(sos)

struct BurnDriver BurnDrvSos = {
	"sos", NULL, NULL, NULL, "1980",
	"SOS\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sosRomInfo, sosRomName, NULL, NULL, SosInputInfo, SosDIPInfo,
	SOSInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};

INT32 BombbeeInit()
{
	navaronemode = 0;
	bombbeemode = 1;
	ball_size_x = 4;
	ball_size_y = 4;
	use_paddle = 1;

	return DrvInit();
}

// Bomb Bee

static struct BurnRomInfo bombbeeRomDesc[] = {
	{ "bombbee.1k",	0x2000, 0x9f8cd7af, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "bombbee.4c",	0x0800, 0x5f37d569, 1 | BRF_GRA }, //  1 gfx1
};

STD_ROM_PICK(bombbee)
STD_ROM_FN(bombbee)

struct BurnDriver BurnDrvBombbee = {
	"bombbee", NULL, NULL, NULL, "1979",
	"Bomb Bee\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bombbeeRomInfo, bombbeeRomName, NULL, NULL, BombbeeInputInfo, BombbeeDIPInfo,
	BombbeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};


// Cutie Q

static struct BurnRomInfo cutieqRomDesc[] = {
	{ "cutieq.1k",	0x2000, 0x6486cdca, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "cutieq.4c",	0x0800, 0x0e1618c9, 1 | BRF_GRA }, //  1 gfx1
};

STD_ROM_PICK(cutieq)
STD_ROM_FN(cutieq)

struct BurnDriver BurnDrvCutieq = {
	"cutieq", NULL, NULL, NULL, "1979",
	"Cutie Q\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cutieqRomInfo, cutieqRomName, NULL, NULL, BombbeeInputInfo, BombbeeDIPInfo,
	BombbeeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 272, 3, 4
};
