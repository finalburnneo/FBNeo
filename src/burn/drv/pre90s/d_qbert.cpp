// Q-Bert emu-layer for FB Alpha by dink, based on the MAME driver by Fabrice Frances & Nicola Salmoria.

#include "tiles_generic.h"
#include "driver.h"
#include "nec_intf.h"
#include "m6502_intf.h"
#include "bitswap.h"
#include "dac.h"
#include "samples.h"

//#define QBERT_SOUND_DEBUG

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvV20ROM;
static UINT8 *Drv6502ROM;
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

static UINT8 *riot_regs;
static UINT8 *riot_ram;

static UINT8 *background_prio;
static UINT8 *spritebank;
static UINT8 *soundlatch;
static UINT8 *soundcpu_do_nmi;

static char  *vtqueue;
static UINT8 *vtqueuepos;
static UINT32 *vtqueuetime;
static UINT8  *knocker_prev;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvFakeInput[8]; // fake inputs for rotate buttons

static UINT8 DrvDip[2] = { 0, 0  };
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static UINT8 game_type = 0; // 0 = qbert, 6 = qbertcub, 4 = mplanets

static UINT32 nRotateTime[2]  = { 0, 0 };

static struct BurnInputInfo QbertInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"   },
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"  },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"   },
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"  },
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"     },
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"   },
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"   },
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"  },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"     },
	{"Select",		BIT_DIGITAL,	DrvJoy1 + 7,    "select"    },
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Qbert)


static struct BurnDIPInfo QbertDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x02, NULL		        },
	{0x0f, 0xff, 0xff, 0x40, NULL	            },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x01, 0x01, "Off"		        },
	{0x0e, 0x01, 0x01, 0x00, "On"	            },

	{0   , 0xfe, 0   ,    2, "Kicker"		    },
	{0x0e, 0x01, 0x02, 0x00, "Off"		        },
	{0x0e, 0x01, 0x02, 0x02, "On"		        },

	{0   , 0xfe, 0   ,    2, "Cabinet"		    },
	{0x0e, 0x01, 0x04, 0x00, "Upright"		    },
	{0x0e, 0x01, 0x04, 0x04, "Cocktail"		    },

	{0   , 0xfe, 0   ,    2, "Auto Round Advance (Cheat)" },
	{0x0e, 0x01, 0x08, 0x00, "Off"		        },
	{0x0e, 0x01, 0x08, 0x08, "On"		        },

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0e, 0x01, 0x10, 0x00, "Off"		        },
	{0x0e, 0x01, 0x10, 0x10, "On"		        },

	{0   , 0xfe, 0   ,    2, "SW5"		        },
	{0x0e, 0x01, 0x20, 0x00, "Off"		        },
	{0x0e, 0x01, 0x20, 0x20, "On"		        },

	{0   , 0xfe, 0   ,    2, "SW7"		        },
	{0x0e, 0x01, 0x40, 0x00, "Off"		        },
	{0x0e, 0x01, 0x40, 0x40, "On"		        },

	{0   , 0xfe, 0   ,    2, "SW8"		        },
	{0x0e, 0x01, 0x80, 0x00, "Off"		        },
	{0x0e, 0x01, 0x80, 0x80, "On"		        },

	{0   , 0xfe, 0   ,    2, "Service"	        },
	{0x0f, 0x01, 0x40, 0x40, "Off"		        },
	{0x0f, 0x01, 0x40, 0x00, "On"		        },
};

STDDIPINFO(Qbert)

static struct BurnInputInfo MplanetsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"   },
	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"  },
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1" },
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2" },
	{"Rotate Left", BIT_DIGITAL,    DrvFakeInput + 0, "p1 rotate left" },
	{"Rotate Right",BIT_DIGITAL,    DrvFakeInput + 1, "p1 rotate right" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,      "reset"     },
	{"Select",		BIT_DIGITAL,	DrvJoy1 + 6,    "select"    },
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Mplanets)


static struct BurnDIPInfo MplanetsDIPList[]=
{
	{0x0C, 0xff, 0xff, 0x00, NULL		        },
	{0x0D, 0xff, 0xff, 0x80, NULL	            },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0C, 0x01, 0x01, 0x01, "Off"		        },
	{0x0C, 0x01, 0x01, 0x00, "On"		        },

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0C, 0x01, 0x02, 0x00, "10000"		    },
	{0x0C, 0x01, 0x02, 0x02, "12000"		    },

	{0   , 0xfe, 0   ,    2, "Allow Round Select" },
	{0x0C, 0x01, 0x08, 0x00, "No"		        },
	{0x0C, 0x01, 0x08, 0x08, "Yes"		        },

	{0   , 0xfe, 0   ,    4, "Coinage"		    },
	{0x0C, 0x01, 0x14, 0x04, "2 Coins 1 Credit"	},
	{0x0C, 0x01, 0x14, 0x00, "1 Coin  1 Credit"	},
	{0x0C, 0x01, 0x14, 0x10, "1 Coin  2 Credits" },
	{0x0C, 0x01, 0x14, 0x14, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Lives"		    },
	{0x0C, 0x01, 0x20, 0x00, "3"		        },
	{0x0C, 0x01, 0x20, 0x20, "5"		        },

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0C, 0x01, 0xc0, 0x40, "Easy"		        },
	{0x0C, 0x01, 0xc0, 0x00, "Medium"		    },
	{0x0C, 0x01, 0xc0, 0x80, "Hard"		        },
	{0x0C, 0x01, 0xc0, 0xc0, "Hardest"		    },

	{0   , 0xfe, 0   ,    2, "Service"	        },
	{0x0D, 0x01, 0x80, 0x80, "Off"		        },
	{0x0D, 0x01, 0x80, 0x00, "On"		        },
};

STDDIPINFO(Mplanets)

static struct BurnInputInfo QbertqubInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"   },
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"  },

	{"Reset",		BIT_DIGITAL,	&DrvReset,      "reset"     },
	{"Select",		BIT_DIGITAL,	DrvJoy1 + 7,    "select"    },
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,     "dip"       },
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,     "dip"       },
};

STDINPUTINFO(Qbertqub)

static struct BurnDIPInfo QbertqubDIPList[]=
{
	{0x08, 0xff, 0xff, 0x00, NULL		        },
	{0x09, 0xff, 0xff, 0x40, NULL	            },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x08, 0x01, 0x08, 0x08, "Off"		        },
	{0x08, 0x01, 0x08, 0x00, "On"		        },

	{0   , 0xfe, 0   ,    13, "Coinage"		    },
	{0x08, 0x01, 0x35, 0x24, "A 2/1 B 2/1"		},
	{0x08, 0x01, 0x35, 0x14, "A 1/1 B 4/1"		},
	{0x08, 0x01, 0x35, 0x30, "A 1/1 B 3/1"		},
	{0x08, 0x01, 0x35, 0x10, "A 1/1 B 2/1"		},
	{0x08, 0x01, 0x35, 0x00, "A 1/1 B 1/1"		},
	{0x08, 0x01, 0x35, 0x11, "A 2/3 B 2/1"		},
	{0x08, 0x01, 0x35, 0x15, "A 1/2 B 3/1"		},
	{0x08, 0x01, 0x35, 0x20, "A 1/2 B 2/1"		},
	{0x08, 0x01, 0x35, 0x21, "A 1/2 B 1/1"		},
	{0x08, 0x01, 0x35, 0x31, "A 1/2 B 1/5"		},
	{0x08, 0x01, 0x35, 0x04, "A 1/3 B 2/1"		},
	{0x08, 0x01, 0x35, 0x05, "A 1/3 B 1/1"		},
	{0x08, 0x01, 0x35, 0x35, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "1st Bonus Life"	},
	{0x08, 0x01, 0x02, 0x00, "10000"		    },
	{0x08, 0x01, 0x02, 0x02, "15000"		    },

	{0   , 0xfe, 0   ,    2, "Additional Bonus Life" },
	{0x08, 0x01, 0x40, 0x00, "20000"		    },
	{0x08, 0x01, 0x40, 0x40, "25000"		    },

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x08, 0x01, 0x80, 0x00, "Normal"		    },
	{0x08, 0x01, 0x80, 0x80, "Hard"		        },

	{0   , 0xfe, 0   ,    2, "Service"	        },
	{0x09, 0x01, 0x40, 0x40, "Off"		        },
	{0x09, 0x01, 0x40, 0x00, "On"		        },

};

STDDIPINFO(Qbertqub)

static UINT8 dialRotation(int addy) {
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

static void gottlieb_paletteram_w(UINT16 offset, UINT8 data)
{
	INT32 bit0, bit1, bit2, bit3;
	INT32 r, g, b, val;

	DrvPaletteRAM[offset] = data;

	/* red component */

	val = DrvPaletteRAM[offset | 1];

	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;

	r = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	/* green component */

	val = DrvPaletteRAM[offset & ~1];

	bit0 = (val >> 4) & 0x01;
	bit1 = (val >> 5) & 0x01;
	bit2 = (val >> 6) & 0x01;
	bit3 = (val >> 7) & 0x01;

	g = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	/* blue component */

	val = DrvPaletteRAM[offset & ~1];

	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;

	b = 0x10 * bit0 + 0x21 * bit1 + 0x46 * bit2 + 0x88 * bit3;

	DrvPalette[offset / 2] = BurnHighCol(r, g, b, 0);
}

static void qbert_palette()
{
	for (INT32 i = 0; i < 0x20; i++) {
		gottlieb_paletteram_w(i, DrvPaletteRAM[i]);
	}
}

static void gottlieb_sh_w(UINT16 offset, UINT8 data); //forward!
static void qbert_knocker(UINT8 knock);

static void __fastcall main_write(UINT32 address, UINT8 data)
{
	address &= 0xffff;

	if ((address & 0xff00) == 0x3700) address &= ~0x700; // mirror handling
	if ((address & 0xfc00) == 0x3c00) address &= ~0x400;
	if ((address & 0xffe0) == 0x57e0) address &= ~0x7e0;
	if ((address & 0xffff) == 0x5ff8) address &= ~0x7f8;

	if (/*address >= 0x0000 &&*/ address <= 0x0fff) {
		DrvNVRAM[address - 0x0000] = data;
		return;
	}

	if (address >= 0x1000 && address <= 0x2fff) {
		//bprintf(0, _T("drw."));
		DrvDummyROM[address - 0x1000] = data;
		return;
	}

	if (address >= 0x3000 && address <= 0x30ff) {
		DrvSpriteRAM[address - 0x3000] = data;
		return;
	}

	if (address >= 0x3800 && address <= 0x3bff) {
		DrvVideoRAM[address - 0x3800] = data;
		return;
	}

	if (address >= 0x4000 && address <= 0x4fff) {
		DrvCharRAM[address - 0x4000] = data;
		return;
	}

	if (address >= 0x5000 && address <= 0x501f) {
		gottlieb_paletteram_w(address - 0x5000, data);
		return;
	}

	switch (address)
	{
		case 0x5802: gottlieb_sh_w(address, data); return;
		case 0x5803: {
			*background_prio = data & 0x01;
			qbert_knocker(data >> 5 & 1);
			if (game_type == 6) // qbertqub only
				*spritebank = (data & 0x10) >> 4;
			return;
		}
	}
}

static UINT8 __fastcall main_read(UINT32 address)
{
	address &= 0xffff;

	if (address >= 0x6000 && address <= 0xffff) {
		return DrvV20ROM[address - 0x6000];
	}

	if (/*address >= 0x0000 &&*/ address <= 0x0fff) {
		return DrvNVRAM[address - 0x0000];
	}

	if (address >= 0x1000 && address <= 0x2fff) {
		//bprintf(0, _T("drr."));
		return DrvDummyROM[address - 0x1000];
	}

	if ((address & 0xff00) == 0x3700) address &= ~0x700;
	if ((address & 0xfc00) == 0x3c00) address &= ~0x400;
	if ((address & 0xffe0) == 0x57e0) address &= ~0x7e0;
	if ((address & 0xffff) == 0x5ff8) address &= ~0x7f8;

	if (address >= 0x3000 && address <= 0x30ff) {
		return DrvSpriteRAM[address - 0x3000];
	}

	if (address >= 0x3800 && address <= 0x3bff) {
		return DrvVideoRAM[address - 0x3800];
	}

	if (address >= 0x4000 && address <= 0x4fff) {
		return DrvCharRAM[address - 0x4000];
	}

	if (address >= 0x5000 && address <= 0x501f) {
		return DrvPaletteRAM[address - 0x5000];
	}


	switch (address)
	{
		case 0x5800: return DrvDip[0];
		case 0x5801: return DrvInput[0] | DrvDip[1]; // DrvDip[1] (fake-dip) for service mode.
		case 0x5803: return dialRotation(0);
		case 0x5804: return DrvInput[1];
	}

	return 0;
}

static void qbert_knocker(UINT8 knock)
{
	if (knock & ~*knocker_prev)
		BurnSamplePlay(44);
	*knocker_prev = knock;
}

static void gottlieb_sh_w(UINT16 /*offset*/, UINT8 data)
{
	static INT32 random_offset = rand()&7;
	data &= 0x3f;

	if ((data & 0x0f) != 0xf) {
#ifdef QBERT_SOUND_DEBUG
		bprintf(0, _T("data %X.."), data ^ 0x3f);
#endif
		switch (data ^ 0x3f) { // qbert sample player
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
				BurnSamplePlay(((data^0x3f)-17)*8+random_offset);
				random_offset = (random_offset+1)&7;
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

		*soundlatch = data;

		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6502Run(10); //CPU_IRQSTATUS_AUTO no workie
		M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
	}
}

static UINT8 gottlieb_riot_r(UINT16 offset)
{
    switch (offset & 0x1f) {
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
	if (*vtqueuepos == 24 && !strncmp("\xC1\xE4\xFF\xE7\xE8\xD2\xFC\xFC\xFC\xFC\xFC\xEA\xFF\xF6\xD6\xF3\xD5\xC5\xF5\xF2\xE1\xDB\xF2\xC0", vtqueue, 24)) {
		blank_queue(); // "Hello, I'm turned on."
		return 1;
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
			bprintf(0, _T("\\x%X"), data); //save
#endif
			switch (check_queue()) {
				case 1: BurnSamplePlay(42);	break; // Say Hello
			}
			*soundcpu_do_nmi = 1;
			return;
		}
	}
}

static UINT8 audio_read(UINT16 address)
{
	address &= 0x7fff; // 15bit addressing

	if (address >= 0x7000 && address <= 0x7fff) {
		return Drv6502ROM[address - 0x7000];
	}

	if (/*address >= 0x0000 &&*/ address <= 0x01ff) {
		return riot_ram[address&0x7f];
	}

	if (address >= 0x0200 && address <= 0x03ff) {
		return gottlieb_riot_r(address - 0x200);
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (M6502TotalCycles() / ((3579545.0000/4) / (nBurnFPS / 100.0000))));
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

	DACReset();
	BurnSampleReset();

	nRotateTime[0] = 0;
	nRotateTime[1] = 0;

	return 0;
}


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV20ROM		= Next; Next += 0x10000;
	Drv6502ROM		= Next; Next += 0x10000;

	DrvPalette		= (UINT32*)Next; Next += 0x10 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x40000;
	DrvSpriteGFX    = Next; Next += 0x40000;

	DrvNVRAM        = Next; Next += 0x01000; // Keep in ROM section.

	AllRam			= Next;

	DrvV20RAM		= Next; Next += 0x01000;
	Drv6502RAM		= Next; Next += 0x01000;

	DrvVideoRAM		= Next; Next += 0x00400;
	DrvCharRAM		= Next; Next += 0x01000;
	DrvSpriteRAM	= Next; Next += 0x00100;
	DrvPaletteRAM	= Next; Next += 0x00040;
	DrvDummyROM     = Next; Next += 0x02000; // it's RAM, too.

	riot_regs       = Next; Next += 0x00020;
	riot_ram        = Next; Next += 0x00200;

	vtqueuepos      = Next; Next += 0x00001;
	vtqueuetime     = (UINT32 *)Next; Next += 0x00004;
	vtqueue         = (char *)Next; Next += 0x00020;
	knocker_prev    = Next; Next += 0x00001;

	background_prio = Next; Next += 0x00001;
	spritebank      = Next; Next += 0x00001;
	soundlatch      = Next; Next += 0x00001;
	soundcpu_do_nmi = Next; Next += 0x00001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	INT32 c8PlaneOffsets[4] = { 0, 1, 2, 3 };
	INT32 c8XOffsets[8] = { 0, 4, 8, 12, 16, 20, 24, 28 };
	INT32 c8YOffsets[8] = { 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 };

	INT32 c16PlaneOffsets[4] = { RGN_FRAC(((game_type == 6) ? 0x10000 : 0x8000), 0, 4), RGN_FRAC(((game_type == 6) ? 0x10000 : 0x8000), 1, 4), RGN_FRAC(((game_type == 6) ? 0x10000 : 0x8000), 2, 4), RGN_FRAC(((game_type == 6) ? 0x10000 : 0x8000), 3, 4) };
	INT32 c16XOffsets[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	INT32 c16YOffsets[16] = { 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 };
	INT32 roffset = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{   // Load ROMS parse GFX
		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x40000);
		memset(DrvTempRom, 0, 0x40000);
		{
			if (game_type == 0) { // qbert
				if (BurnLoadRom(DrvV20ROM + 0x4000, 0, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x6000, 1, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x8000, 2, 1)) return 1;
			}
			if (game_type == 4) { // mplanets
				if (BurnLoadRom(DrvV20ROM + 0x0000, 0, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x2000, 1, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x4000, 2, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x6000, 3, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x8000, 4, 1)) return 1;
				roffset = 2;
			}
			if (game_type == 6) { // qbertqub
				if (BurnLoadRom(DrvV20ROM + 0x2000, 0, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x4000, 1, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x6000, 2, 1)) return 1;
				if (BurnLoadRom(DrvV20ROM + 0x8000, 3, 1)) return 1;
				roffset = 1;
			}

			if (BurnLoadRom(Drv6502ROM + 0x0000, 3 + roffset, 1)) return 1;
			if (BurnLoadRom(Drv6502ROM + 0x0800, 4 + roffset, 1)) return 1;

			// load & decode 8x8 tiles
			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom + 0x0000, 5 + roffset, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x1000, 6 + roffset, 1)) return 1;
			GfxDecode(0x100, 4, 8, 8, c8PlaneOffsets, c8XOffsets, c8YOffsets, 0x100, DrvTempRom, DrvCharGFX);

			// load & decode 16x16 tiles
			memset(DrvTempRom, 0, 0x40000);
			if (game_type == 6) { // qbertqub
				if (BurnLoadRom(DrvTempRom + 0x0000, 7 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0x4000, 8 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0x8000, 9 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0xc000, 10 + roffset, 1)) return 1;
				GfxDecode(0x200 /*((0x10000*8)/4)/(16*16))*/, 4, 16, 16, c16PlaneOffsets, c16XOffsets, c16YOffsets, 0x100, DrvTempRom, DrvSpriteGFX);
			} else {
				if (BurnLoadRom(DrvTempRom + 0x0000, 7 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0x2000, 8 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0x4000, 9 + roffset, 1)) return 1;
				if (BurnLoadRom(DrvTempRom + 0x6000, 10 + roffset, 1)) return 1;
				GfxDecode(0x100 /*((0x8000*8)/4)/(16*16))*/, 4, 16, 16, c16PlaneOffsets, c16XOffsets, c16YOffsets, 0x100, DrvTempRom, DrvSpriteGFX);
			}
		}
		BurnFree(DrvTempRom);
	}

	VezInit(0, V20_TYPE);
	VezOpen(0);

	memset(DrvNVRAM, 0xff, 0x1000); // Init NVRAM

	//VezMapArea(0x01000, 0x02fff, 0, DrvDummyROM); // ROM for reactor and 3stooges, used as RAM for all other games.
	//VezMapArea(0x01000, 0x02fff, 1, DrvDummyROM); // note: moved to main_read() / main_write()
	//VezMapArea(0x01000, 0x02fff, 2, DrvDummyROM);
	VezSetReadHandler(main_read);
	VezSetWriteHandler(main_write);

	VezClose();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetWriteHandler(audio_write);
	M6502SetReadHandler(audio_read);
	M6502SetReadOpArgHandler(audio_read);
	M6502SetReadOpHandler(audio_read);
	M6502Close();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.30, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();
	M6502Exit();
	DACExit();
	BurnSampleExit();

	BurnFree(AllMem);

	game_type = 0;

	return 0;
}


static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, INT32 offset, INT32 /*mode*/, UINT8 *gfxrom)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = gfxrom;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue; // blank out the top and bottom 16 pixels for status

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (!pxl) continue; // transparency

			dest[sy * nScreenWidth + sx] = pxl | (color << 4) | offset;
		}
		sx -= width;
	}
}


static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100 - 8; offs += 4)	{
		INT32 sx = ((DrvSpriteRAM[offs + 1]) - 4) + ((game_type == 4) ? 7 : 0); // mplanets has weird sx/sy offsets
		INT32 sy = ((DrvSpriteRAM[offs]) - 13) - ((game_type == 4) ? 4 : 0);    // apparent in the hiscore table.
		INT32 code = (255 ^ DrvSpriteRAM[offs + 2]) + 256 * *spritebank;

		if (DrvSpriteRAM[offs] || DrvSpriteRAM[offs + 1])
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0, 0x00, DrvSpriteGFX);
	}
}

static void draw_bg()
{
	INT32 hflip = 0, vflip = 0;
	for (INT32 offs = 0x3ff; offs >= 0; offs--)
	{
		INT32 sx = offs % 32;
		INT32 sy = offs / 32;

		if (hflip) sx = 31 - sx;
		if (vflip) sy = 29 - sy;

		INT32 code = DrvVideoRAM[offs];
		INT32 color = 0;

		sx = 8 * sx;
		sy = 8 * sy;
		if (sx >= nScreenWidth) continue;
		if (sy >= nScreenHeight) continue;

		RenderTileCPMP(code, color, sx, sy, hflip, vflip, 8, 8, 0, 0, DrvCharGFX);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		qbert_palette();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2 && !*background_prio) draw_bg();
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 2 &&  *background_prio) draw_bg();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = 0;
	DrvInput[1] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
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
	INT32 nCyclesTotalVez = 5000000 / 60;
	INT32 nCyclesTotal6502 = (3579545 / 4) / 60;

	VezOpen(0);
	M6502Open(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		M6502Run(nCyclesTotal6502 / nInterleave);
		if (*soundcpu_do_nmi) {
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			*soundcpu_do_nmi = 0;
		}

		VezRun(nCyclesTotalVez / nInterleave);
		if (i == (nInterleave - 1))
			VezSetIRQLineAndVector(0x20, 0xff, CPU_IRQSTATUS_AUTO);
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

		VezScan(nAction);
		M6502Scan(nAction);

		DACScan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x1000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}

static struct BurnSampleInfo qbertSampleDesc[] = {
#if !defined (ROM_VERIFY)
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
#endif
	{"knocker", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(qbert)
STD_SAMPLE_FN(qbert)

// Q*bert (US set 1)

static struct BurnRomInfo qbertRomDesc[] = {
	{ "qb-rom2.bin",	0x2000, 0xfe434526, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "qb-rom1.bin",	0x2000, 0x55635447, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb-rom0.bin",	0x2000, 0x8e318641, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",	0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "qb-snd2.bin",	0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",		0x1000, 0x7a9ba824, 3 | BRF_GRA }, //  5 bgtiles
	{ "qb-bg1.bin",		0x1000, 0x22e5b891, 3 | BRF_GRA }, //  6

	{ "qb-fg3.bin",		0x2000, 0xdd436d3a, 4 | BRF_GRA }, //  7 sprites
	{ "qb-fg2.bin",		0x2000, 0xf69b9483, 4 | BRF_GRA }, //  8
	{ "qb-fg1.bin",		0x2000, 0x224e8356, 4 | BRF_GRA }, //  9
	{ "qb-fg0.bin",		0x2000, 0x2f695b85, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(qbert)
STD_ROM_FN(qbert)

struct BurnDriver BurnDrvQbert = {
	"qbert", NULL, NULL, "qbert", "1982",
	"Q*bert (US set 1)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_16BIT_ONLY, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, qbertRomInfo, qbertRomName, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert (US set 2)

static struct BurnRomInfo qbertaRomDesc[] = {
	{ "qrom_2.bin",		0x2000, 0xb54a8ffc, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "qrom_1.bin",		0x2000, 0x19d924e3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qrom_0.bin",		0x2000, 0x2e7fad1b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",	0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "qb-snd2.bin",	0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",		0x1000, 0x7a9ba824, 3 | BRF_GRA }, //  5 bgtiles
	{ "qb-bg1.bin",		0x1000, 0x22e5b891, 3 | BRF_GRA }, //  6

	{ "qb-fg3.bin",		0x2000, 0xdd436d3a, 4 | BRF_GRA }, //  7 sprites
	{ "qb-fg2.bin",		0x2000, 0xf69b9483, 4 | BRF_GRA }, //  8
	{ "qb-fg1.bin",		0x2000, 0x224e8356, 4 | BRF_GRA }, //  9
	{ "qb-fg0.bin",		0x2000, 0x2f695b85, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(qberta)
STD_ROM_FN(qberta)

struct BurnDriver BurnDrvQberta = {
	"qberta", "qbert", NULL, "qbert", "1982",
	"Q*bert (US set 2)\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, qbertaRomInfo, qbertaRomName, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Q*bert (Japan)

static struct BurnRomInfo qbertjRomDesc[] = {
	{ "qbj-rom2.bin",	0x2000, 0x67bb1cb2, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "qbj-rom1.bin",	0x2000, 0xc61216e7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qbj-rom0.bin",	0x2000, 0x69679d5c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",	0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "qb-snd2.bin",	0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",		0x1000, 0x7a9ba824, 3 | BRF_GRA }, //  5 bgtiles
	{ "qb-bg1.bin",		0x1000, 0x22e5b891, 3 | BRF_GRA }, //  6

	{ "qb-fg3.bin",		0x2000, 0xdd436d3a, 4 | BRF_GRA }, //  7 sprites
	{ "qb-fg2.bin",		0x2000, 0xf69b9483, 4 | BRF_GRA }, //  8
	{ "qb-fg1.bin",		0x2000, 0x224e8356, 4 | BRF_GRA }, //  9
	{ "qb-fg0.bin",		0x2000, 0x2f695b85, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(qbertj)
STD_ROM_FN(qbertj)

struct BurnDriver BurnDrvQbertj = {
	"qbertj", "qbert", NULL, "qbert", "1982",
	"Q*bert (Japan)\0", NULL, "Gottlieb (Konami license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, qbertjRomInfo, qbertjRomName, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Mello Yello Q*bert

static struct BurnRomInfo myqbertRomDesc[] = {
	{ "mqb-rom2.bin",	0x2000, 0x6860f957, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mqb-rom1.bin",	0x2000, 0x11f0a4e4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mqb-rom0.bin",	0x2000, 0x12a90cb2, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mqb-snd1.bin",	0x0800, 0x495ffcd2, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "mqb-snd2.bin",	0x0800, 0x9bbaa945, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",		0x1000, 0x7a9ba824, 3 | BRF_GRA }, //  5 bgtiles
	{ "qb-bg1.bin",		0x1000, 0x22e5b891, 3 | BRF_GRA }, //  6

	{ "mqb-fg3.bin",	0x2000, 0x8b5d0852, 4 | BRF_GRA }, //  7 sprites
	{ "mqb-fg2.bin",	0x2000, 0x823f1e57, 4 | BRF_GRA }, //  8
	{ "mqb-fg1.bin",	0x2000, 0x05343ae6, 4 | BRF_GRA }, //  9
	{ "mqb-fg0.bin",	0x2000, 0xabc71bdd, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(myqbert)
STD_ROM_FN(myqbert)

struct BurnDriver BurnDrvMyqbert = {
	"myqbert", "qbert", NULL, "qbert", "1982",
	"Mello Yello Q*bert\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, myqbertRomInfo, myqbertRomName, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};

static INT32 DrvInitMplanets()
{
	game_type = 4;

	return DrvInit();
}


// Faster, Harder, More Challenging Q*bert (prototype)

static struct BurnRomInfo sqbertRomDesc[] = {
	{ "qb-rom2.bin",	0x2000, 0x1e3d4038, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "qb-rom1.bin",	0x2000, 0xeaf3076c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb-rom0.bin",	0x2000, 0x61260a7e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "qb-snd1.bin",	0x0800, 0x15787c07, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "qb-snd2.bin",	0x0800, 0x58437508, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "qb-bg0.bin",		0x1000, 0xc3118eef, 3 | BRF_GRA }, //  5 bgtiles
	{ "qb-bg1.bin",		0x1000, 0x4f6d8075, 3 | BRF_GRA }, //  6

	{ "qb-fg3.bin",		0x2000, 0xee595eda, 4 | BRF_GRA }, //  7 sprites
	{ "qb-fg2.bin",		0x2000, 0x59884c78, 4 | BRF_GRA }, //  8
	{ "qb-fg1.bin",		0x2000, 0x2a60e3ad, 4 | BRF_GRA }, //  9
	{ "qb-fg0.bin",		0x2000, 0xb11ad9d8, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(sqbert)
STD_ROM_FN(sqbert)

struct BurnDriver BurnDrvSqbert = {
	"sqbert", NULL, NULL, "qbert", "1983",
	"Faster, Harder, More Challenging Q*bert (prototype)\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, sqbertRomInfo, sqbertRomName, qbertSampleInfo, qbertSampleName, QbertInputInfo, QbertDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};

// Mad Planets

static struct BurnRomInfo mplanetsRomDesc[] = {
	{ "rom4.c16",		0x2000, 0x5402077f, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rom3.c14-15",	0x2000, 0x5d18d740, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom2.c13-14",	0x2000, 0x960c3bb1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom1.c12-13",	0x2000, 0xeb515f10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom0.c11-12",	0x2000, 0x74de78aa, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "snd1",		    0x0800, 0x453193a1, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu
	{ "snd2",		    0x0800, 0xf5ffc98f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bg0.e11-12",		0x1000, 0x709aa24c, 3 | BRF_GRA }, //  7 bgtiles
	{ "bg1.e13",		0x1000, 0x4921e345, 3 | BRF_GRA }, //  8

	{ "fg3.k7-8",		0x2000, 0xc990b39f, 4 | BRF_GRA }, //  9 sprites
	{ "fg2.k6",		    0x2000, 0x735e2522, 4 | BRF_GRA }, // 10
	{ "fg1.k5",		    0x2000, 0x6456cc1c, 4 | BRF_GRA }, // 11
	{ "fg0.k4",		    0x2000, 0xa920e325, 4 | BRF_GRA }, // 12
};

STD_ROM_PICK(mplanets)
STD_ROM_FN(mplanets)

struct BurnDriver BurnDrvMplanets = {
	"mplanets", NULL, NULL, NULL, "1983",
	"Mad Planets\0", NULL, "Gottlieb", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mplanetsRomInfo, mplanetsRomName, NULL, NULL, MplanetsInputInfo, MplanetsDIPInfo,
	DrvInitMplanets, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};


// Mad Planets (UK)

static struct BurnRomInfo mplanetsukRomDesc[] = {
	{ "mpt_rom4.bin",	0x2000, 0xcd88e23c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mpt_rom3.bin",	0x2000, 0xdc355b2d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpt_rom2.bin",	0x2000, 0x846ddc23, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpt_rom1.bin",	0x2000, 0x94d67b87, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mpt_rom0.bin",	0x2000, 0xa9e30ad2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "mpt_snd1.bin",	0x0800, 0x453193a1, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu
	{ "mpt_snd2.bin",	0x0800, 0xf5ffc98f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "mpt_bg0.bin",	0x1000, 0x709aa24c, 3 | BRF_GRA }, //  7 bgtiles
	{ "mpt_bg1.bin",	0x1000, 0x4921e345, 3 | BRF_GRA }, //  8

	{ "mpt_fg3.bin",	0x2000, 0xc990b39f, 4 | BRF_GRA }, //  9 sprites
	{ "mpt_fg2.bin",	0x2000, 0x735e2522, 4 | BRF_GRA }, // 10
	{ "mpt_fg1.bin",	0x2000, 0x6456cc1c, 4 | BRF_GRA }, // 11
	{ "mpt_fg0.bin",	0x2000, 0xa920e325, 4 | BRF_GRA }, // 12
};

STD_ROM_PICK(mplanetsuk)
STD_ROM_FN(mplanetsuk)

struct BurnDriver BurnDrvMplanetsuk = {
	"mplanetsuk", "mplanets", NULL, NULL, "1983",
	"Mad Planets (UK)\0", NULL, "Gottlieb (Taitel license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mplanetsukRomInfo, mplanetsukRomName, NULL, NULL, MplanetsInputInfo, MplanetsDIPInfo,
	DrvInitMplanets, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};

static INT32 DrvInitCube()
{
	game_type = 6;

	return DrvInit();
}

// Q*bert's Qubes

static struct BurnRomInfo qbertqubRomDesc[] = {
	{ "qq-rom3.bin",	0x2000, 0xc4dbdcd7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "qq-rom2.bin",	0x2000, 0x21a6c6cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qq-rom1.bin",	0x2000, 0x63e6c43d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qq-rom0.bin",	0x2000, 0x8ddbe438, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "qq-snd1.bin",	0x0800, 0xe704b450, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu
	{ "qq-snd2.bin",	0x0800, 0xc6a98bf8, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "qq-bg0.bin",		0x1000, 0x050badde, 3 | BRF_GRA }, //  6 bgtiles
	{ "qq-bg1.bin",		0x1000, 0x8875902f, 3 | BRF_GRA }, //  7

	{ "qq-fg3.bin",		0x4000, 0x91a949cc, 4 | BRF_GRA }, //  8 sprites
	{ "qq-fg2.bin",		0x4000, 0x782d9431, 4 | BRF_GRA }, //  9
	{ "qq-fg1.bin",		0x4000, 0x71c3ac4c, 4 | BRF_GRA }, // 10
	{ "qq-fg0.bin",		0x4000, 0x6192853f, 4 | BRF_GRA }, // 11
};

STD_ROM_PICK(qbertqub)
STD_ROM_FN(qbertqub)

struct BurnDriver BurnDrvQbertqub = {
	"qbertqub", NULL, NULL, "qbert", "1983",
	"Q*bert's Qubes\0", NULL, "Mylstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, qbertqubRomInfo, qbertqubRomName, qbertSampleInfo, qbertSampleName, QbertqubInputInfo, QbertqubDIPInfo,
	DrvInitCube, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	240, 256, 3, 4
};

