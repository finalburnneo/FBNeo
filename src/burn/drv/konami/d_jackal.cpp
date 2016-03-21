// FB Alpha Jackal driver module
// Based on MAME driver by Kenneth Lin

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"

static UINT8 *AllMem;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvPalRAM;
static UINT32 *DrvPaletteTab;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;
static UINT8 *AllRam;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZRAM;
static UINT8 *DrvVORAM;
static UINT8 *DrvVidControl;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static INT32 DrvZRAMBank;
static INT32 DrvVORAMBank;
static INT32 DrvSprRAMBank;
static INT32 DrvROMBank;
static INT32 DrvIRQEnable;
static INT32 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static INT32 watchdog;
static INT32 layer_offset_x = 8;
static INT32 layer_offset_y = 16;

static INT32 bootleg = 0;

//static INT32 nRotate[2] = { 0, 0 };
//static UINT32 nRotateTime[2] = { 0, 0 };
//static UINT8 DrvFakeInput[4] = { 0, 0, 0, 0 };
// Rotation stuff! -dink
static UINT8  DrvFakeInput[6]       = {0, 0, 0, 0, 0, 0};
static UINT8  nRotateHoldInput[2]   = {0, 0};
static INT32  nRotate[2]            = {0, 0};
static INT32  nRotateTarget[2]      = {0, 0};
static INT32  nRotateTry[2]         = {0, 0};
static UINT32 nRotateTime[2]        = {0, 0};
static UINT8  game_rotates = 0;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo DrvrotateInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Rotate Left",     BIT_DIGITAL, DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right",    BIT_DIGITAL, DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 4,  "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Rotate Left",     BIT_DIGITAL, DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right",    BIT_DIGITAL, DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 5,  "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drvrotate)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0x20, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"			},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"			},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 1 Credit"			},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"			},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"			},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"			},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"			},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"			},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"			},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"			},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"			},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"			},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"			},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 1 Credit"			},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"			},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"			},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"			},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"			},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"			},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"			},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"			},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"			},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},	
	{0x13, 0x01, 0x03, 0x03, "2"					},
	{0x13, 0x01, 0x03, 0x02, "3"					},
	{0x13, 0x01, 0x03, 0x01, "4"					},
	{0x13, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x13, 0x01, 0x18, 0x18, "30k 150k"				},
	{0x13, 0x01, 0x18, 0x10, "50k 200k"				},
	{0x13, 0x01, 0x18, 0x08, "30k"					},
	{0x13, 0x01, 0x18, 0x00, "50k"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x60, 0x60, "Easy"					},
	{0x13, 0x01, 0x60, 0x40, "Normal"				},
	{0x13, 0x01, 0x60, 0x20, "Difficult"				},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x80, 0x80, "Off"					},
	{0x13, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x20, 0x20, "Off"					},
	{0x14, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Sound Adjustment"			},
	{0x14, 0x01, 0x40, 0x00, "Upright"				},
	{0x14, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Sound Mode"				},
	{0x14, 0x01, 0x80, 0x00, "Stereo"				},
	{0x14, 0x01, 0x80, 0x80, "Mono"					},
};

STDDIPINFO(Drv)

static struct BurnDIPInfo DrvrotateDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL					},
	{0x19, 0xff, 0xff, 0xff, NULL					},
	{0x1a, 0xff, 0xff, 0x20, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x18, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"			},
	{0x18, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"			},
	{0x18, 0x01, 0x0f, 0x06, "2 Coins 1 Credit"			},
	{0x18, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"			},
	{0x18, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"			},
	{0x18, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"			},
	{0x18, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"			},
	{0x18, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"			},
	{0x18, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"			},
	{0x18, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"			},
	{0x18, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"			},
	{0x18, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"			},
	{0x18, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"			},
	{0x18, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"			},
	{0x18, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"			},
	{0x18, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x18, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"			},
	{0x18, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"			},
	{0x18, 0x01, 0xf0, 0x60, "2 Coins 1 Credit"			},
	{0x18, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"			},
	{0x18, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"			},
	{0x18, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"			},
	{0x18, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"			},
	{0x18, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"			},
	{0x18, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"			},
	{0x18, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"			},
	{0x18, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"			},
	{0x18, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"			},
	{0x18, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"			},
	{0x18, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"			},
	{0x18, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"			},
	{0x18, 0x01, 0xf0, 0x00, "No Coin B"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},	
	{0x19, 0x01, 0x03, 0x03, "2"					},
	{0x19, 0x01, 0x03, 0x02, "3"					},
	{0x19, 0x01, 0x03, 0x01, "4"					},
	{0x19, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x19, 0x01, 0x18, 0x18, "30k 150k"				},
	{0x19, 0x01, 0x18, 0x10, "50k 200k"				},
	{0x19, 0x01, 0x18, 0x08, "30k"					},
	{0x19, 0x01, 0x18, 0x00, "50k"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x19, 0x01, 0x60, 0x60, "Easy"					},
	{0x19, 0x01, 0x60, 0x40, "Normal"				},
	{0x19, 0x01, 0x60, 0x20, "Difficult"				},
	{0x19, 0x01, 0x60, 0x00, "Very Difficult"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x19, 0x01, 0x80, 0x80, "Off"					},
	{0x19, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x1a, 0x01, 0x20, 0x20, "Off"					},
	{0x1a, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Sound Adjustment"			},
	{0x1a, 0x01, 0x40, 0x00, "Upright"				},
	{0x1a, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Sound Mode"				},
	{0x1a, 0x01, 0x80, 0x00, "Stereo"				},
	{0x1a, 0x01, 0x80, 0x80, "Mono"					},
};

STDDIPINFO(Drvrotate)

// Rotation-handler code

static void RotateReset() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotate[playernum] = 0; // start out pointing straight up (0=up)
		nRotateTarget[playernum] = -1;
		nRotateTime[playernum] = 0;
		nRotateHoldInput[0] = nRotateHoldInput[1] = 0;
	}
}

static UINT32 RotationTimer(void) {
    return nCurrentFrame;
}

static void RotateRight(INT32 *v) {
    (*v)++;
    if (*v > 7) *v = 0;
}

static void RotateLeft(INT32 *v) {
    (*v)--;
    if (*v < 0) *v = 7;
}

static UINT8 Joy2Rotate(UINT8 *joy) { // ugly code, but the effect is awesome. -dink
	if (joy[2] && joy[0]) return 7;    // up left
	if (joy[2] && joy[1]) return 1;    // up right

	if (joy[3] && joy[0]) return 5;    // down left
	if (joy[3] && joy[1]) return 3;    // down right

	if (joy[2]) return 0;    // up
	if (joy[3]) return 4;    // down
	if (joy[0]) return 6;    // left
	if (joy[1]) return 2;    // right

	return 0xff;
}

static int dialRotation(INT32 playernum) {
    // p1 = 0, p2 = 1
	UINT8 player[2] = { 0, 0 };
	static UINT8 lastplayer[2][2] = { { 0, 0 }, { 0, 0 } };

    if ((playernum != 0) && (playernum != 1)) {
        bprintf(PRINT_NORMAL, _T("Strange Rotation address => %06X\n"), playernum);
        return 0;
    }
    if (playernum == 0) {
        player[0] = DrvFakeInput[0]; player[1] = DrvFakeInput[1];
    }
    if (playernum == 1) {
        player[0] = DrvFakeInput[2]; player[1] = DrvFakeInput[3];
    }

    if (player[0] && (player[0] != lastplayer[playernum][0] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
		RotateLeft(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Left => %06X\n"), playernum+1, nRotate[playernum]);
		nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
    }

	if (player[1] && (player[1] != lastplayer[playernum][1] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
        RotateRight(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Right => %06X\n"), playernum+1, nRotate[playernum]);
        nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
	}

	lastplayer[playernum][0] = player[0];
	lastplayer[playernum][1] = player[1];

	return ~(1 << nRotate[playernum]);
}

static UINT8 *rotate_gunpos[2] = {NULL, NULL};
static UINT8 rotate_gunpos_multiplier = 1;

// Gun-rotation memory locations - do not remove this tag. - dink :)
// game     p1           p2           clockwise value in memory  multiplier
// jackal   0xbd8        0xc00        0  1  2  3  4  5  6  7     1
// topgunbl SAME

static void RotateSetGunPosRAM(UINT8 *p1, UINT8 *p2, UINT8 multiplier) {
	rotate_gunpos[0] = p1;
	rotate_gunpos[1] = p2;
	rotate_gunpos_multiplier = multiplier;
}

static INT32 get_distance(INT32 from, INT32 to) {
// this function finds the easiest way to get from "from" to "to", wrapping at 0 and 7
	INT32 countA = 0;
	INT32 countB = 0;
	INT32 fromtmp = from / rotate_gunpos_multiplier;
	INT32 totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp++;
		countA++;
		if(fromtmp>7) fromtmp = 0;
		if(fromtmp == totmp || countA > 32) break;
	}

	fromtmp = from / rotate_gunpos_multiplier;
	totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp--;
		countB++;
		if(fromtmp<0) fromtmp = 7;
		if(fromtmp == totmp || countB > 32) break;
	}

	if (countA > countB) {
		return 1; // go negative
	} else {
		return 0; // go positive
	}
}

static void RotateDoTick() {
	// since the game only allows for 1 rotation every other frame, we have to
	// do this.
	if (nCurrentFrame&1) return;

	for (INT32 i = 0; i < 2; i++) {
		if (rotate_gunpos[i] && (nRotateTarget[i] != -1) && (nRotateTarget[i] != (*rotate_gunpos[i] & 0xff))) {
			if (get_distance(nRotateTarget[i], *rotate_gunpos[i] & 0xff)) {
				RotateRight(&nRotate[i]); // --
			} else {
				RotateLeft(&nRotate[i]);  // ++
			}
			bprintf(0, _T("p%X target %X mempos %X nRotate %X.\n"), i, nRotateTarget[i], *rotate_gunpos[i] & 0xff, nRotate[i]);
			nRotateTry[i]++;
			if (nRotateTry[i] > 10) nRotateTarget[i] = -1; // don't get stuck in a loop if something goes horribly wrong here.
		} else {
			nRotateTarget[i] = -1;
		}
	}
}

static void SuperJoy2Rotate() {
	for (INT32 i = 0; i < 2; i++) { // p1 = 0, p2 = 1
		if (DrvFakeInput[4 + i]) { //  rotate-button had been pressed
			UINT8 rot = Joy2Rotate(((!i) ? &DrvJoy1[0] : &DrvJoy2[0]));
			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
			}
			//DrvInput[i] &= ~0xf; // cancel out directionals since they are used to rotate here.
			DrvInputs[i] = (DrvInputs[i] & ~0xf) | (nRotateHoldInput[i] & 0xf); // for midnight resistance! be able to duck + change direction of gun.
			nRotateTry[i] = 0;
		} else { // cache joystick UDLR if the rotate button isn't pressed.
			// This feature is for Midnight Resistance, if you are crawling on the
			// ground and need to rotate your gun WITHOUT getting up.
			nRotateHoldInput[i] = DrvInputs[i];
		}
	}

	RotateDoTick();
}

// end Rotation-handler

static void bankswitch()
{
	INT32 banks[3] = { DrvVORAMBank * 0x1000, DrvSprRAMBank * 0x1000, DrvROMBank * 0x8000 };

	M6809MapMemory(DrvVORAM + banks[0],			0x2000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM + banks[1],			0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0x10000 + banks[2],	0x4000, 0xbfff, MAP_ROM);
}

static void jackal_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0020 && address <= 0x005f) {
		DrvZRAM[(address - 0x20) + DrvZRAMBank] = data;
		return;
	}

	if (address >= 0x0060 && address <= 0x1fff) {
		DrvShareRAM[address] = data;
		return;
	}

	switch (address)
	{
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0003:
			DrvVidControl[address] = data;
		return;

		case 0x0004:
			flipscreen = data & 0x08;
			DrvIRQEnable = data & 0x02;
		return;

		case 0x0019:
			watchdog = 0;
		return;

		case 0x001c:
			DrvSprRAMBank = (data & 0x08) >> 3;
			DrvZRAMBank = (data & 0x10) << 2; // 0x40
			DrvROMBank = (data & 0x20) >> 5;
			DrvVORAMBank = (data & 0x10) >> 4;
			bankswitch();
		return;
	}
}

static UINT8 jackal_main_read(UINT16 address)
{
	if (address >= 0x0020 && address <= 0x005f) {
		return DrvZRAM[(address - 0x20) + DrvZRAMBank];
	}

	if (address >= 0x0060 && address <= 0x1fff) {
		return DrvShareRAM[address];
	}

	switch (address)
	{
		case 0x0000: // actually read?
		case 0x0001:
		case 0x0002:
		case 0x0003:
			return DrvVidControl[address];

		case 0x0010:
			return DrvDips[0];

		case 0x0011:
			return DrvInputs[0];

		case 0x0012:
			return DrvInputs[1];

		case 0x0013:
			return (DrvInputs[2] & 0x1f) | (DrvDips[2] & 0xe0);

		case 0x0014:
		case 0x0015:
			return dialRotation(address - 0x14); // rotary

		case 0x0018:
			return DrvDips[1];
	}

	return 0;
}

static void jackal_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x2001:
			BurnYM2151WriteRegister(data);
		return;
	}
}

static UINT8 jackal_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			return BurnYM2151ReadStatus();
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	DrvZRAMBank = 0;
	DrvVORAMBank = 0;
	DrvSprRAMBank = 0;
	DrvROMBank = 0;
	DrvIRQEnable = 0;
	flipscreen = 0;

	M6809Open(0);
	bankswitch();
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	BurnYM2151Reset();

	RotateReset();

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x020000;
	DrvM6809ROM1	= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x140000;
	DrvGfxROM1		= Next; Next += 0x180000;
	DrvGfxROM2		= Next; Next += 0x180000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPaletteTab   = (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);
	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x004000;
	DrvZRAM			= Next; Next += 0x000080;
	DrvVORAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x000400;

	DrvVidControl	= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[8] = { STEP4(0,1), STEP4(0x40000*8, 1) };
	INT32 XOffs[16] = {  0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4, 32*8+0*4, 32*8+1*4, 32*8+2*4, 32*8+3*4, 32*8+4*4, 32*8+5*4, 32*8+6*4, 32*8+7*4 };
	//STEP8(0,4), STEP8(32*8, ??)
	INT32 YOffs[16] = { STEP8(0,32), STEP8(16*32,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x1000, 8,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp + 0x00000, tmp + 0x20000, 0x20000);
	memcpy (tmp + 0x20000, tmp + 0x60000, 0x20000);

	GfxDecode(0x0800, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);
	GfxDecode(0x2000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++) {
		DrvPaletteTab[0x000 + i] = i + 0x100;
		DrvPaletteTab[0x100 + i] =  DrvColPROM[0x000 + i] & 0xf;
		DrvPaletteTab[0x200 + i] = (DrvColPROM[0x100 + i] & 0xf) + 0x10;
	}
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (!bootleg) {
		// Jackal
		if (BurnLoadRom(DrvM6809ROM0 + 0x10000,   0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0c000,   1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x08000,   2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,   3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x00001,   4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x40000,   5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x40001,   6, 2)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000,   7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100,   8, 1)) return 1;
	} else {
		// Bootleg
		if (BurnLoadRom(DrvM6809ROM0 + 0x10000,   0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x18000,   1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0c000,   2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x08000,   3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,   4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x08000,   5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x10000,   6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x18000,   7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x20000,   8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x28000,   9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x30000,  10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x38000,  11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x40000,  12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x48000,  13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x50000,  14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x58000,  15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x60000,  16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x68000,  17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x70000,  18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x78000,  19, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000,  20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100,  21, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x80000);
	}

	DrvGfxDecode();
	DrvPaletteInit();

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvShareRAM + 0x100,	0x0100, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvVORAM,		0x2000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0x0c000,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(jackal_main_write);
	M6809SetReadHandler(jackal_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvPalRAM,		0x4000, 0x43ff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x6000, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1 + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(jackal_sub_write);
	M6809SetReadHandler(jackal_sub_read);
	M6809Close();

	BurnYM2151Init(3580000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();
	if (game_rotates)
		RotateSetGunPosRAM(DrvShareRAM + 0xbd8, DrvShareRAM + 0xc00, 1);
	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	M6809Exit();

	BurnYM2151Exit();
	
	GenericTilesExit();
	
	BurnFree(AllMem);

	bootleg = 0;
	game_rotates = 1;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT32 pens[0x200];

	for (INT32 i = 0; i < 0x200; i++) {
		if (i >= 0x20 && i < 0x100) continue; // not used, save cycles

		UINT16 p = (DrvPalRAM[(i * 2)] | (DrvPalRAM[(i * 2) + 1] << 8));

		UINT8 r = (p >>  0) & 0x1f;
		UINT8 g = (p >>  5) & 0x1f;
		UINT8 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x300; i++) {
		DrvPalette[i] = pens[DrvPaletteTab[i]];
	}
}

static void draw_layer()
{
	INT32 layer_control = DrvVidControl[2];

	INT32 xscroll = DrvVidControl[1];
	INT32 yscroll = DrvVidControl[0];

	UINT8 *scrollram = DrvZRAM;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 attr  = DrvVORAM[0x0000 + offs];
		INT32 code  = DrvVORAM[0x0400 + offs] + ((attr & 0xc0) << 2) + ((attr & 0x30) << 6);

		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;

		INT32 sy = (((offs / 0x20) & 0x1f) * 8) - layer_offset_y;
		INT32 sx = ((offs & 0x1f) * 8) - layer_offset_x;

		if (layer_control & 0x02) {
			if (layer_control & 0x08) {
				sx -= scrollram[(sy + layer_offset_y)/8]; // maybe apply the offset here? I guess we'll know for sure after playtesting. -dink
				sy -= yscroll;
			}
			else if (layer_control & 0x04) {
				sy -= scrollram[(sx + layer_offset_x)/8]; // offset applied here fixes the konami logo on the titlescreen. -dink
				sx -= xscroll;
			}
		} else {
			sy -= yscroll;
			sx -= xscroll;
		}
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprite(INT32 bank, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	sx -= layer_offset_x;
	sy -= layer_offset_y;
	color += (bank & 2) ? 0x20 : 0x10;

	if (bank & 8) // 8x8
	{
		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM2);
			}
		}
	}
	else // 16x16
	{
		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites(INT32 rambase, INT32 length, INT32 bank)
{
	const UINT8 *sram = DrvSprRAM + rambase + ((DrvVidControl[3] & 0x08) ? 0x0800 : 0);

	for (INT32 offs = 0; offs < length; offs += 5)
	{
		INT32 attr  = sram[offs + 4];
		INT32 sn1   = sram[offs];
		INT32 sn2   = sram[offs + 1];
		INT32 sy    = sram[offs + 2];
		INT32 sx    = sram[offs + 3] - ((attr & 1) << 8);
		INT32 flipx = attr & 0x20;
		INT32 flipy = attr & 0x40;
		INT32 color = sn2 >> 4;

		if (sy > 0xf0)	sy -= 256;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (attr & 0x0c)
		{
			INT32 spritenum = sn1 * 4 + ((sn2 & 0x0c) >> 2) + ((sn2 & 3) << 10) + ((bank & 2) << 11); // was << 12 -dink

			INT32 mod = -8;

			if (flipscreen)
			{
				sx += 8;
				sy -= 8;
				mod = 8;
			}

			if ((attr & 0x0c) == 0x0C) // 1x1
			{
				if (flipscreen) sy += 16;
				draw_sprite(bank+8, spritenum, color, sx, sy, flipx, flipy);
			}

			if ((attr & 0x0c) == 0x08) // 1x2
			{
				sy += 8;
				draw_sprite(bank+8, spritenum,     color, sx, sy,       flipx, flipy);
				draw_sprite(bank+8, spritenum - 2, color, sx, sy + mod, flipx, flipy);
			}

			if ((attr & 0x0c) == 0x04) // 2x1
			{
				draw_sprite(bank+8, spritenum,     color, sx,       sy, flipx, flipy);
				draw_sprite(bank+8, spritenum + 1, color, sx + mod, sy, flipx, flipy);
			}
		}
		else
		{
			INT32 spritenum = sn1 + (((sn2 & 0x03) << 8) + ((bank & 2) << 9));

			if (attr & 0x10) // 2x2
			{
				if (flipscreen)
				{
					sx -= 16;
					sy -= 16;
				}

				draw_sprite(bank, spritenum,     color, flipx ? sx+16 : sx, flipy ? sy+16 : sy, flipx, flipy);
				draw_sprite(bank, spritenum + 1, color, flipx ? sx : sx+16, flipy ? sy+16 : sy, flipx, flipy);
				draw_sprite(bank, spritenum + 2, color, flipx ? sx+16 : sx, flipy ? sy : sy+16, flipx, flipy);
				draw_sprite(bank, spritenum + 3, color, flipx ? sx : sx+16, flipy ? sy : sy+16, flipx, flipy);
			}
			else // 1x1
			{
				draw_sprite(bank, spritenum, color, sx, sy, flipx, flipy);
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	//}
	BurnTransferClear();
	if (nBurnLayer & 2) draw_layer();

	if (nBurnLayer & 4) draw_sprites(0x1000, 0x0f5, 2);
	if (nBurnLayer & 8) draw_sprites(0x0000, 0x500, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 1536000 / 60, ((1536000 * 12) / 10) / 60 };
	INT32 nSoundBufferPos = 0;

	M6809NewFrame();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		M6809Run(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1) && DrvIRQEnable)
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_AUTO);
		M6809Close();

		M6809Open(1);
		M6809Run(nCyclesTotal[1] / nInterleave);
		if (i == (nInterleave - 1) && DrvIRQEnable)
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi
		M6809Close();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
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
		*pnMin = 0x029737;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);

		BurnYM2151Scan(nAction);

		SCAN_VAR(DrvZRAMBank);
		SCAN_VAR(DrvVORAMBank);
		SCAN_VAR(DrvSprRAMBank);
		SCAN_VAR(DrvROMBank);
		SCAN_VAR(DrvIRQEnable);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch();
		M6809Close();
	}

	return 0;
}

// Jackal (World, 8-way Joystick)

static struct BurnRomInfo jackalRomDesc[] = {
	{ "631_v02.15d",	0x10000, 0x0b7e0584, 0 | BRF_PRG | BRF_ESS }, // 0 - M6809 #0 Code
	{ "631_v03.16d",	0x04000, 0x3e0dfb83, 0 | BRF_PRG | BRF_ESS }, // 1

	{ "631_t01.11d",	0x08000, 0xb189af6a, 1 | BRF_PRG | BRF_ESS }, // 2 - M6809 #1 Code

	{ "631t04.7h",  	0x20000, 0x457f42f0, 2 | BRF_GRA },           // 3 - Graphics Tiles
	{ "631t05.8h",		0x20000, 0x732b3fc1, 2 | BRF_GRA },           // 4
	{ "631t06.12h",		0x20000, 0x2d10e56e, 2 | BRF_GRA },           // 5
	{ "631t07.13h",		0x20000, 0x4961c397, 2 | BRF_GRA },           // 6

	{ "631r08.9h",		0x00100, 0x7553a172, 3 | BRF_GRA },           // 7 - Color PROMs
	{ "631r09.14h",		0x00100, 0xa74dd86c, 3 | BRF_GRA },           // 8
};

STD_ROM_PICK(jackal)
STD_ROM_FN(jackal)

// Jackal (World, Rotary Joystick)

static struct BurnRomInfo jackalrRomDesc[] = {
	{ "631_q02.15d",	0x10000, 0xed2a7d66, 0 | BRF_PRG | BRF_ESS }, // 0 - M6809 #0 Code
	{ "631_q03.16d",	0x04000, 0xb9d34836, 0 | BRF_PRG | BRF_ESS }, // 1

	{ "631_q01.11d",	0x08000, 0x54aa2d29, 1 | BRF_PRG | BRF_ESS }, // 2 - M6809 #1 Code

	{ "631t04.7h",		0x20000, 0x457f42f0, 2 | BRF_GRA },           // 3 - Graphics Tiles
	{ "631t05.8h",		0x20000, 0x732b3fc1, 2 | BRF_GRA },           // 4
	{ "631t06.12h",		0x20000, 0x2d10e56e, 2 | BRF_GRA },           // 5
	{ "631t07.13h",		0x20000, 0x4961c397, 2 | BRF_GRA },           // 6

	{ "631r08.9h",		0x00100, 0x7553a172, 3 | BRF_GRA },           // 7 - Color PROMs
	{ "631r09.14h",		0x00100, 0xa74dd86c, 3 | BRF_GRA },           // 7 - Color PROMs
};

STD_ROM_PICK(jackalr)
STD_ROM_FN(jackalr)

// Tokushu Butai Jackal (Japan, 8-way Joystick)

static struct BurnRomInfo jackaljRomDesc[] = {
	{ "631_t02.15d",	0x10000, 0x14db6b1a, 0 | BRF_PRG | BRF_ESS }, // 0 - M6809 #0 Code
	{ "631_t03.16d",	0x04000, 0xfd5f9624, 0 | BRF_PRG | BRF_ESS }, // 1

	{ "631_t01.11d",	0x08000, 0xb189af6a, 1 | BRF_PRG | BRF_ESS }, // 2 - M6809 #1 Code

	{ "631t04.7h",  	0x20000, 0x457f42f0, 2 | BRF_GRA },           // 3 - Graphics Tiles
	{ "631t05.8h",		0x20000, 0x732b3fc1, 2 | BRF_GRA },           // 4
	{ "631t06.12h",		0x20000, 0x2d10e56e, 2 | BRF_GRA },           // 5
	{ "631t07.13h",		0x20000, 0x4961c397, 2 | BRF_GRA },           // 6

	{ "631r08.9h",		0x00100, 0x7553a172, 3 | BRF_GRA },           // 7 - Color PROMs
	{ "631r09.14h",		0x00100, 0xa74dd86c, 3 | BRF_GRA },           // 8
};

STD_ROM_PICK(jackalj)
STD_ROM_FN(jackalj)

// Top Gunner (US, 8-way Joystick)

static struct BurnRomInfo topgunrRomDesc[] = {
	{ "631_u02.15d",	0x10000, 0xf7e28426, 0 | BRF_PRG | BRF_ESS }, // 0 - M6809 #0 Code
	{ "631_u03.16d",	0x04000, 0xc086844e, 0 | BRF_PRG | BRF_ESS }, // 1

	{ "631_t01.11d",	0x08000, 0xb189af6a, 1 | BRF_PRG | BRF_ESS }, // 2 - M6809 #1 Code

	{ "631u04.7h",		0x20000, 0x50122a12, 2 | BRF_GRA },           // 3 - Graphics Tiles
	{ "631u05.8h",		0x20000, 0x6943b1a4, 2 | BRF_GRA },           // 4
	{ "631u06.12h",		0x20000, 0x37dbbdb0, 2 | BRF_GRA },           // 5
	{ "631u07.13h",		0x20000, 0x22effcc8, 2 | BRF_GRA },           // 6

	{ "631r08.9h",		0x00100, 0x7553a172, 3 | BRF_GRA },           // 7 - Color PROMs
	{ "631r09.14h",		0x00100, 0xa74dd86c, 3 | BRF_GRA },           // 8
};

STD_ROM_PICK(topgunr)
STD_ROM_FN(topgunr)

// Top Gunner (bootleg, Rotary Joystick)

static struct BurnRomInfo topgunblRomDesc[] = {
	{ "t-3.c5",	    0x8000, 0x7826ad38, 0 | BRF_PRG | BRF_ESS }, // 0 - M6809 #0 Code
	{ "t-4.c4",	    0x8000, 0x976c8431, 0 | BRF_PRG | BRF_ESS }, // 1
	{ "t-2.c6",	    0x4000, 0xd53172e5, 0 | BRF_PRG | BRF_ESS }, // 2

	{ "t-1.c14",	0x8000, 0x54aa2d29, 1 | BRF_PRG | BRF_ESS }, // 3 - M6809 #1 Code

	{ "t-17.n12",	0x8000, 0xe8875110, 2 | BRF_GRA },           // 4 - Graphics Tiles
	{ "t-18.n13",	0x8000, 0xcf14471d, 2 | BRF_GRA },           // 5
	{ "t-19.n14",	0x8000, 0x46ee5dd2, 2 | BRF_GRA },           // 6
	{ "t-20.n15",	0x8000, 0x3f472344, 2 | BRF_GRA },           // 7
	{ "t-6.n1",	    0x8000, 0x539cc48c, 2 | BRF_GRA },           // 8
	{ "t-5.m1",	    0x8000, 0xdbc26afe, 2 | BRF_GRA },           // 9
	{ "t-7.n2",	    0x8000, 0x0ecd31b1, 2 | BRF_GRA },           // 10
	{ "t-8.n3",	    0x8000, 0xf946ada7, 2 | BRF_GRA },           // 11
	{ "t-13.n8",	0x8000, 0x5d669abb, 2 | BRF_GRA },           // 12
	{ "t-14.n9",	0x8000, 0xf349369b, 2 | BRF_GRA },           // 13
	{ "t-15.n10",	0x8000, 0x7c5a91dd, 2 | BRF_GRA },           // 14
	{ "t-16.n11",	0x8000, 0x5ec46d8e, 2 | BRF_GRA },           // 15
	{ "t-9.n4",	    0x8000, 0x8269caca, 2 | BRF_GRA },           // 16
	{ "t-10.n5",	0x8000, 0x25393e4f, 2 | BRF_GRA },           // 17
	{ "t-11.n6",	0x8000, 0x7895c22d, 2 | BRF_GRA },           // 18
	{ "t-12.n7",	0x8000, 0x15606dfc, 2 | BRF_GRA },           // 19

	{ "631r08.bpr",	0x0100, 0x7553a172, 3 | BRF_GRA },           // 20 - Color PROMs
	{ "631r09.bpr",	0x0100, 0xa74dd86c, 3 | BRF_GRA },           // 21
};

STD_ROM_PICK(topgunbl)
STD_ROM_FN(topgunbl)

INT32 DrvInitbl()
{
	bootleg = 1;
	game_rotates = 1;

	return DrvInit();
}

INT32 DrvInitRo()
{
	game_rotates = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvJackal = {
	"jackal", NULL, NULL, NULL, "1986",
	"Jackal (World, 8-way Joystick)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, jackalRomInfo, jackalRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvJackalr = {
	"jackalr", "jackal", NULL, NULL, "1986",
	"Jackal (World, Rotary Joystick)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, jackalrRomInfo, jackalrRomName, NULL, NULL, DrvrotateInputInfo, DrvrotateDIPInfo,
	DrvInitRo, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvJackalj = {
	"jackalj", "jackal", NULL, NULL, "1986",
	"Tokushu Butai Jackal (Japan, 8-way Joystick)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, jackaljRomInfo, jackaljRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvTopgunr = {
	"topgunr", "jackal", NULL, NULL, "1986",
	"Top Gunner (US, 8-way Joystick)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, topgunrRomInfo, topgunrRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvTopgunbl = {
	"topgunbl", "jackal", NULL, NULL, "1986",
	"Top Gunner (bootleg, Rotary Joystick)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, topgunblRomInfo, topgunblRomName, NULL, NULL, DrvrotateInputInfo, DrvrotateDIPInfo,
	DrvInitbl, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 240, 3, 4
};
