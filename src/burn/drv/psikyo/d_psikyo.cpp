// Psikyo MC68EC020 based hardware
// Driver and Emulation by Jan Klaassen
#include "psikyo.h"
#include "burn_ym2610.h"
#include "burn_ymf278b.h"
#include "msm6295.h"
#include "bitswap.h"

INT32 PsikyoHardwareVersion;

static UINT8 DrvJoy1[8] = {0, };
static UINT8 DrvJoy2[8] = {0, };
static UINT8 DrvInp1[8] = {0, };
static UINT8 DrvInp2[8] = {0, };
static UINT16 DrvInput[4] = {0, };

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Psikyo68KROM, *PsikyoZ80ROM;
static UINT8 *Psikyo68KRAM, *PsikyoZ80RAM;
static UINT8 *PsikyoSampleROM01, *PsikyoSampleROM02;
static UINT8 *PsikyoBootSpriteBuf;

static INT32 PsikyoSampleROM01Size, PsikyoSampleROM02Size;
static INT32 PsikyoTileROMSize, PsikyoSpriteROMSize;

static UINT8 DrvReset = 0;
static UINT16 bVBlank;

static INT32 nPsikyoZ80Bank;

static INT32 nSoundlatch, nSoundlatchAck;

static INT32 nCyclesDone[2];
static INT32 nCyclesTotal[2];
static INT32 nCyclesSegment;
static INT32 nCycles68KSync;

static INT32 nPrevBurnCPUSpeedAdjust;

// ----------------------------------------------------------------------------
// Input definitions

static struct BurnInputInfo gunbirdInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInp1 + 0,	"p1 coin"			},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"			},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7, 	"p1 up"				},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6, 	"p1 down"			},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4, 	"p1 left"			},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5, 	"p1 right"			},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"			},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"			},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"			},

	{"P2 Coin",			BIT_DIGITAL,	DrvInp1 + 1,	"p2 coin"			},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"			},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7, 	"p2 up"				},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6, 	"p2 down"			},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4, 	"p2 left"			},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5, 	"p2 right"			},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"			},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"			},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"			},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"				},
	{"Test",	  		BIT_DIGITAL,	DrvInp1 + 5,	"diag"				},
	{"Service",			BIT_DIGITAL,	DrvInp1 + 4,	"service"			},
//	{"Tilt",			BIT_DIGITAL,	DrvInp1 + 6,	"tilt"				},

	{"Dip 1",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 2)) + 1, "dip"	},
	{"Dip 2",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 2)) + 0, "dip"	},
	{"Dip 3",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 3)) + 0, "dip"	},
};

STDINPUTINFO(gunbird)

static struct BurnInputInfo btlkroadInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInp1 + 0,	"p1 coin"			},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"			},

	{"P1 Up",		  	BIT_DIGITAL,	DrvJoy1 + 7, 	"p1 up"				},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6, 	"p1 down"			},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4, 	"p1 left"			},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5, 	"p1 right"			},
	{"P1 Weak punch",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"			},
	{"P1 Medium punch",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"			},
	{"P1 Strong punch",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"			},
	{"P1 Weak kick",	BIT_DIGITAL,	DrvInp2 + 7,	"p1 fire 4"			},
	{"P1 Medium kick",	BIT_DIGITAL,	DrvInp2 + 6,	"p1 fire 5"			},
	{"P1 Strong kick",	BIT_DIGITAL,	DrvInp2 + 5,	"p1 fire 6"			},

	{"P2 Coin",			BIT_DIGITAL,	DrvInp1 + 1,	"p2 coin"			},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"			},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7, 	"p2 up"				},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6, 	"p2 down"			},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4, 	"p2 left"			},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5, 	"p2 right"			},
	{"P2 Weak punch",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"			},
	{"P2 Medium punch",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"			},
	{"P2 Strong punch",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"			},
	{"P2 Weak kick",	BIT_DIGITAL,	DrvInp2 + 3,	"p2 fire 4"			},
	{"P2 Medium kick",	BIT_DIGITAL,	DrvInp2 + 2,	"p2 fire 5"			},
	{"P2 Strong kick",	BIT_DIGITAL,	DrvInp2 + 1,	"p2 fire 6"			},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"				},
	{"Test",			BIT_DIGITAL,	DrvInp1 + 5,	"diag"				},
	{"Service",			BIT_DIGITAL,	DrvInp1 + 4,	"service"			},
//	{"Tilt",			BIT_DIGITAL,	DrvInp1 + 6,	"tilt"				},

	{"Dip 1",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 2)) + 1, "dip"	},
	{"Dip 2",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 2)) + 0, "dip"	},
	{"Region",			BIT_DIPSWITCH,	((UINT8*)(DrvInput + 3)) + 0, "dip"	},
	{"Debug Dip",		BIT_DIPSWITCH,	((UINT8*)(DrvInput + 3)) + 1, "dip"	},
};

STDINPUTINFO(btlkroad)

static struct BurnDIPInfo samuraiaDIPList[] = {
	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL							},
	{0x16,	0xFF, 0xFF,	0x02, NULL							},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x15,	0x82, 0x01,	0x00, "Same"						},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x01,	0x01, "Individual"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	9,	  "Coin 1"						},
	{0x15,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x15,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x15,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x15,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x15,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x01, 0xFF,	0xFF, "Free play"					},

	{0,		0xFE, 0,	9,	  "Coin 2"						},
	{0x15,	0x82, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x70, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0xFF,	0xFF, "Free play"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x15,	0x82, 0x80,	0x00, "Normal mode"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x80,	0x80, "Continue mode"				},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen reverse"				},
	{0x16,	0x01, 0x01,	0x00, "Invert"						},
	{0x16,	0x01, 0x01,	0x01, "Normal"						},

	{0,		0xFE, 0,	2,	  "Demo sound"},
	{0x16,	0x01, 0x02,	0x00, "Off"							},
	{0x16,	0x01, 0x02,	0x02, "On"							},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x16,	0x01, 0x0C,	0x00, "Normal"						},
	{0x16,	0x01, 0x0C,	0x04, "Easy"						},
	{0x16,	0x01, 0x0C,	0x08, "Hard"						},
	{0x16,	0x01, 0x0C,	0x0C, "Super hard"					},

	{0,		0xFE, 0,	4,	  "Fighters"					},
	{0x16,	0x01, 0x30,	0x00, "3"							},
	{0x16,	0x01, 0x30,	0x10, "1"							},
	{0x16,	0x01, 0x30,	0x20, "2"							},
	{0x16,	0x01, 0x30,	0x30, "4"							},

	{0,		0xFE, 0,	2,	  "Extend player"				},
	{0x16,	0x01, 0x40,	0x00, "Every 400,000 points"		},
	{0x16,	0x01, 0x40,	0x40, "Every 600,000 points"		},

	{0,		0xFE, 0,	2,	  "Test mode"					},
	{0x16,	0x01, 0x80,	0x00, "Off"							},
	{0x16,	0x01, 0x80,	0x80, "On"							},
};

static struct BurnDIPInfo gunbirdDIPList[] = {
	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL							},
	{0x16,	0xFF, 0xFF,	0x02, NULL							},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x15,	0x82, 0x01,	0x00, "Same"						},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x01,	0x01, "Individual"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	9,	  "Coin 1"						},
	{0x15,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x15,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x15,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x15,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x15,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x01, 0xFF,	0xFF, "Free play"					},

	{0,		0xFE, 0,	9,	  "Coin 2"						},
	{0x15,	0x82, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x70, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0xFF,	0xFF, "Free play"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x15,	0x82, 0x80,	0x00, "Normal mode"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x80,	0x80, "Continue mode"				},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen reverse"				},
	{0x16,	0x01, 0x01,	0x00, "Normal"						},
	{0x16,	0x01, 0x01,	0x01, "Reverse"						},

	{0,		0xFE, 0,	2,	  "Demo sound"					},
	{0x16,	0x01, 0x02,	0x00, "Off"							},
	{0x16,	0x01, 0x02,	0x02, "On"							},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x16,	0x01, 0x0C,	0x00, "Normal"						},
	{0x16,	0x01, 0x0C,	0x04, "Easy"						},
	{0x16,	0x01, 0x0C,	0x08, "Difficult"					},
	{0x16,	0x01, 0x0C,	0x0C, "More Difficult"				},

	{0,		0xFE, 0,	4,	  "Number of fighters"			},
	{0x16,	0x01, 0x30,	0x00, "3"							},
	{0x16,	0x01, 0x30,	0x10, "1"							},
	{0x16,	0x01, 0x30,	0x20, "2"							},
	{0x16,	0x01, 0x30,	0x30, "4"							},

	{0,		0xFE, 0,	2,	  "Extend fighters"				},
	{0x16,	0x01, 0x40,	0x00, "Every 400,000 points"		},
	{0x16,	0x01, 0x40,	0x40, "Every 600,000 points"		},

	{0,		0xFE, 0,	2,	  "Test mode"					},
	{0x16,	0x01, 0x80,	0x00, "Off"							},
	{0x16,	0x01, 0x80,	0x80, "On"							},
};

static struct BurnDIPInfo btlkroadDIPList[] = {
	// Defaults
	{0x1B,	0xFF, 0xFF,	0x00, NULL							},
	{0x1C,	0xFF, 0xFF,	0x02, NULL							},
	{0x1D,	0xFF, 0xFF,	0x0F, NULL							},
	{0x1E,	0xFF, 0xFF,	0x00, NULL							},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x1B,	0x82, 0x01,	0x00, "Same"						},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x01,	0x01, "Individual"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	9,	  "Coin 1"						},
	{0x1B,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x1B,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x1B,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x1B,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x1B,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x01, 0xFF,	0xFF, "Free play"					},

	{0,		0xFE, 0,	9,	  "Coin 2"						},
	{0x1B,	0x82, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x70, "1 coin = 6 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0xFF,	0xFF, "Free play"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x1B,	0x82, 0x80,	0x00, "Normal mode"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x80,	0x80, "Continue mode"				},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen reverse"				},
	{0x1C,	0x01, 0x01,	0x00, "Normal screen"				},
	{0x1C,	0x01, 0x01,	0x01, "Invert screen"				},

	{0,		0xFE, 0,	2,	  "Demo sounds"					},
	{0x1C,	0x01, 0x02,	0x00, "Disabled"					},
	{0x1C,	0x01, 0x02,	0x02, "Enabled"						},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x1C,	0x01, 0x0C,	0x00, "Normal"						},
	{0x1C,	0x01, 0x0C,	0x04, "Easy"						},
	{0x1C,	0x01, 0x0C,	0x08, "Hard"						},
	{0x1C,	0x01, 0x0C,	0x0C, "Hardest"						},

	{0,		0xFE, 0,	2,	  "Enable debug dip"			},
	{0x1C,	0x01, 0x40,	0x00, "Off"							},
	{0x1C,	0x01, 0x40,	0x40, "On"							},

	{0,		0xFE, 0,	2,	  "Test mode"					},
	{0x1C,	0x01, 0x80,	0x00, "Off"							},
	{0x1C,	0x01, 0x80,	0x80, "On"							},

	// Region
	{0,		0xFE, 0,	6,	  "Region"						},
	{0x1D,	0x01, 0xFF,	0x00, "Japan"						},
	{0x1D,	0x01, 0xFF,	0x01, "USA/Canada (Jaleco license)"	},
	{0x1D,	0x01, 0xFF,	0x03, "Korea"						},
	{0x1D,	0x01, 0xFF,	0x05, "Hong Kong"					},
	{0x1D,	0x01, 0xFF,	0x09, "Taiwan"						},
	{0x1D,	0x01, 0xFF,	0x0F, "World"						},

	// Debug Dip
	{0,		0xFE, 0,	2,	  "Debug test menu"				},
	{0x1E,	0x82, 0x80,	0x00, "Off"							},
	{0x1C,	0x00, 0x40, 0x00, NULL							},
	{0x1E,	0x82, 0x80,	0x80, "On"							},
	{0x1C,	0x00, 0x40, 0x00, NULL							},
};

STDDIPINFO(btlkroad)

static struct BurnDIPInfo btlkroadkDIPList[] = {
	// Defaults
	{0x1B,	0xFF, 0xFF,	0x00, NULL							},
	{0x1C,	0xFF, 0xFF,	0x02, NULL							},
	{0x1D,	0xFF, 0xFF,	0x03, NULL							},
	{0x1E,	0xFF, 0xFF,	0x00, NULL							},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x1B,	0x82, 0x01,	0x00, "Same"						},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x01,	0x01, "Individual"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	9,	  "Coin 1"						},
	{0x1B,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x1B,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x1B,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x1B,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x1B,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x1B,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x01, 0xFF,	0xFF, "Free play"					},

	{0,		0xFE, 0,	9,	  "Coin 2"						},
	{0x1B,	0x82, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x70,	0x70, "1 coin = 6 credits"			},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0xFF,	0xFF, "Free play"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x1B,	0x82, 0x80,	0x00, "Normal mode"					},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},
	{0x1B,	0x82, 0x80,	0x80, "Continue mode"				},
	{0x1B,	0x00, 0xFF, 0xFF, NULL							},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen reverse"				},
	{0x1C,	0x01, 0x01,	0x00, "Normal screen"				},
	{0x1C,	0x01, 0x01,	0x01, "Invert screen"				},

	{0,		0xFE, 0,	2,	  "Demo sounds"					},
	{0x1C,	0x01, 0x02,	0x00, "Disabled"					},
	{0x1C,	0x01, 0x02,	0x02, "Enabled"						},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x1C,	0x01, 0x0C,	0x00, "Normal"						},
	{0x1C,	0x01, 0x0C,	0x04, "Easy"						},
	{0x1C,	0x01, 0x0C,	0x08, "Hard"						},
	{0x1C,	0x01, 0x0C,	0x0C, "Hardest"						},

	{0,		0xFE, 0,	2,	  "Enable debug dip"			},
	{0x1C,	0x01, 0x40,	0x00, "Off"							},
	{0x1C,	0x01, 0x40,	0x40, "On"							},

	{0,		0xFE, 0,	2,	  "Test mode"					},
	{0x1C,	0x01, 0x80,	0x00, "Off"							},
	{0x1C,	0x01, 0x80,	0x80, "On"							},

	// Region
	{0,		0xFE, 0,	6,	  "Region"						},
	{0x1D,	0x01, 0xFF,	0x00, "Japan"						},
	{0x1D,	0x01, 0xFF,	0x01, "USA/Canada (Jaleco license)"	},
	{0x1D,	0x01, 0xFF,	0x03, "Korea"						},
	{0x1D,	0x01, 0xFF,	0x05, "Hong Kong"					},
	{0x1D,	0x01, 0xFF,	0x09, "Taiwan"						},
	{0x1D,	0x01, 0xFF,	0x0F, "World"						},

	// Debug Dip
	{0,		0xFE, 0,	2,	  "Debug test menu"				},
	{0x1E,	0x82, 0x80,	0x00, "Off"							},
	{0x1C,	0x00, 0x40, 0x00, NULL							},
	{0x1E,	0x82, 0x80,	0x80, "On"							},
	{0x1C,	0x00, 0x40, 0x00, NULL							},
};

STDDIPINFO(btlkroadk)

static struct BurnDIPInfo s1945DIPList[] = {
	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL							},
	{0x16,	0xFF, 0xFF,	0x02, NULL							},

	// DIP 1
	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x15,	0x01, 0x01,	0x00, "Same"						},
	{0x15,	0x01, 0x01,	0x01, "Individual"			 		},

	{0,		0xFE, 0,	8,	  "Coin 1"						},
	{0x15,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x15,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x15,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x15,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x15,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},

	{0,		0xFE, 0,	8,	  "Coin 2"						},
	{0x15,	0x01, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x01, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x15,	0x01, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x15,	0x01, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x15,	0x01, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x15,	0x01, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x15,	0x01, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x15,	0x01, 0x70,	0x70, "1 coin = 6 credits"			},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x15,	0x01, 0x80,	0x00, "Normal mode"					},
	{0x15,	0x01, 0x80,	0x80, "Continue mode"				},

	// DIP 2
	{0,		0xFE, 0,	2,	  "Screen Reverse"				},
	{0x16,	0x01, 0x01,	0x00, "Normal"						},
	{0x16,	0x01, 0x01,	0x01, "Reverse"						},

	{0,		0xFE, 0,	2,	  "Demo sound"					},
	{0x16,	0x01, 0x02,	0x00, "Off"							},
	{0x16,	0x01, 0x02,	0x02, "On"							},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x16,	0x01, 0x0C,	0x00, "Normal"						},
	{0x16,	0x01, 0x0C,	0x04, "Easy"						},
	{0x16,	0x01, 0x0C,	0x08, "Difficult"					},
	{0x16,	0x01, 0x0C,	0x0C, "More Difficult"				},

	{0,		0xFE, 0,	4,	  "Number of fighters"			},
	{0x16,	0x01, 0x30,	0x00, "3"							},
	{0x16,	0x01, 0x30,	0x10, "1"							},
	{0x16,	0x01, 0x30,	0x20, "2"							},
	{0x16,	0x01, 0x30,	0x30, "4"							},

	{0,		0xFE, 0,	2,	  "Extend fighters"				},
	{0x16,	0x01, 0x40,	0x00, "Every 600,000 points"		},
	{0x16,	0x01, 0x40,	0x40, "Every 800,000 points"		},

	{0,		0xFE, 0,	2,	  "Test mode"					},
	{0x16,	0x01, 0x80,	0x00, "Off"							},
	{0x16,	0x01, 0x80,	0x80, "On"							},
};

static struct BurnDIPInfo tengaiDIPList[] = {
	// Defaults
	{0x15,	0xFF, 0xFF,	0x00, NULL							},
	{0x16,	0xFF, 0xFF,	0x02, NULL							},
	{0x17,	0xFF, 0xFF,	0x0F, NULL							},

	{0,		0xFE, 0,	2,	  "Coin slot"					},
	{0x15,	0x82, 0x01,	0x00, "Same"						},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x01,	0x01, "Individual"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	9,	  "Coin 1"						},
	{0x15,	0x01, 0x0E,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x02, "2 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x04, "3 coins = 1 credit"			},
	{0x15,	0x01, 0x0E,	0x08, "1 coin = 2 credits"			},
	{0x15,	0x01, 0x0E,	0x06, "1 coin = 3 credits"			},
	{0x15,	0x01, 0x0E,	0x0A, "1 coin = 4 credits"			},
	{0x15,	0x01, 0x0E,	0x0C, "1 coin = 5 credits"			},
	{0x15,	0x82, 0x0E,	0x0E, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x01, 0xFF,	0xFF, "Free play"					},

	{0,		0xFE, 0,	9,	  "Coin 2"						},
	{0x15,	0x82, 0x70,	0x00, "1 coin = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x10, "2 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x20, "3 coins = 1 credit"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x40, "1 coin = 2 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x30, "1 coin = 3 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x50, "1 coin = 4 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x60, "1 coin = 5 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x70,	0x70, "1 coin = 6 credits"			},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0xFF,	0xFF, "Free play"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	{0,		0xFE, 0,	2,	  "Continue coin"				},
	{0x15,	0x82, 0x80,	0x00, "Normal mode"					},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},
	{0x15,	0x82, 0x80,	0x80, "Continue mode"				},
	{0x15,	0x00, 0xFF, 0xFF, NULL							},

	// DIP 2
	{0,		0xFE, 0,	2, 	  "Screen Reverse"				},
	{0x16,	0x01, 0x01,	0x00, "Normal screen"				},
	{0x16,	0x01, 0x01,	0x01, "Invert screen"				},

	{0,		0xFE, 0,	2,	  "Demo sounds"					},
	{0x16,	0x01, 0x02,	0x00, "Disabled"					},
	{0x16,	0x01, 0x02,	0x02, "Enabled"						},

	{0,		0xFE, 0,	4,	  "Difficulty"					},
	{0x16,	0x01, 0x0C,	0x00, "Normal"						},
	{0x16,	0x01, 0x0C,	0x04, "Easy"						},
	{0x16,	0x01, 0x0C,	0x08, "Hard"						},
	{0x16,	0x01, 0x0C,	0x0C, "Hardest"						},

	{0,		0xFE, 0,	4,	  "Lives"						},
	{0x16,	0x01, 0x30,	0x00, "3"							},
	{0x16,	0x01, 0x30,	0x10, "1"							},
	{0x16,	0x01, 0x30,	0x20, "2"							},
	{0x16,	0x01, 0x30,	0x30, "4"							},

	{0,		0xFE, 0,	2,	  "Bonus life"					},
	{0x16,	0x01, 0x40,	0x00, "600K"						},
	{0x16,	0x01, 0x40,	0x40, "800K"						},
};

static struct BurnDIPInfo NoRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL							},
};

STDDIPINFOEXT(gunbird, gunbird, NoRegion)
STDDIPINFOEXT(s1945, s1945, NoRegion)
STDDIPINFOEXT(sngkace, samuraia, NoRegion)

static struct BurnDIPInfo gunbirdRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL							},

	// Region
	{0,		0xFE, 0,	5,	  "Region"						},
	{0x17,	0x01, 0xFF,	0x00, "World"						},
	{0x17,	0x01, 0xFF,	0x01, "USA / Canada"				},
	{0x17,	0x01, 0xFF,	0x02, "Korea"						},
	{0x17,	0x01, 0xFF,	0x04, "Hong Kong"					},
	{0x17,	0x01, 0xFF,	0x08, "Taiwan"						},
};

STDDIPINFOEXT(gunbirdWorld, gunbird, gunbirdRegion)
STDDIPINFOEXT(s1945World, s1945, gunbirdRegion)

static struct BurnDIPInfo s1945aRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x01, NULL							},

	// Region
	{0,		0xFE, 0,	2,	  "Region"						},
	{0x17,	0x01, 0xFF,	0x00, "Japan"						},
	{0x17,	0x01, 0xFF,	0x01, "World"						},
};

STDDIPINFOEXT(s1945a, s1945, s1945aRegion)

static struct BurnDIPInfo samuraiaRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL							},

	// Region
	{0,		0xFE, 0,	5,	  "Region"						},
	{0x17,	0x01, 0xFF,	0x00, "World"						},
	{0x17,	0x01, 0xFF,	0x10, "USA / Canada"				},
	{0x17,	0x01, 0xFF,	0x20, "Korea"						},
	{0x17,	0x01, 0xFF,	0x40, "Hong Kong"					},
	{0x17,	0x01, 0xFF,	0x80, "Taiwan"						},
};

STDDIPINFOEXT(samuraia, samuraia, samuraiaRegion)

static struct BurnDIPInfo tengaiRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL							},

	// Region
	{0,		0xFE, 0,	5,	  "Region"						},
	{0x17,	0x01, 0xFF,	0x01, "U.S.A. & Canada"				},
	{0x17,	0x01, 0xFF,	0x02, "Korea"						},
	{0x17,	0x01, 0xFF,	0x04, "Hong Kong"					},
	{0x17,	0x01, 0xFF,	0x08, "Taiwan"						},
	{0x17,	0x01, 0xFF,	0x00, "World"						},
};

STDDIPINFOEXT(tengai, tengai, tengaiRegion)

static struct BurnDIPInfo tengaijRegionDIPList[] = {
	// Defaults
	{0x17,	0xFF, 0xFF,	0x00, NULL							},

	// Region
	{0,		0xFE, 0,	2,	  "Region"						},
	{0x17,	0x01, 0xFF,	0x00, "Japan"						},
	{0x17,	0x01, 0xFF,	0x0F, "World"						},
};

STDDIPINFOEXT(tengaij, tengai, tengaijRegion)

// ----------------------------------------------------------------------------
// CPU synchronisation

static inline void PsikyoSynchroniseZ80(INT32 nExtraCycles)
{
	INT32 nCycles = ((INT64)SekTotalCycles() * nCyclesTotal[1] / nCyclesTotal[0]) + nExtraCycles;

	if (nCycles <= ZetTotalCycles()) {
		return;
	}

	nCycles68KSync = nCycles - nExtraCycles;

	BurnTimerUpdate(nCycles);
}

// ----------------------------------------------------------------------------
// 68K memory handlers

static inline void SendSoundCommand(const INT8 nCommand)
{
//	bprintf(PRINT_NORMAL, _T("  - Sound command sent (0x%02X).\n"), nCommand);

	PsikyoSynchroniseZ80(0);

	nSoundlatch = nCommand;
	nSoundlatchAck = 0;

	ZetNmi();
}

// ----------------------------------------------------------------------------

static UINT8 __fastcall samuraiaReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0] >> 8;
		case 0xC00001:
			return ~DrvInput[0] & 0xFF;
		case 0xC00004:							// DIPs
			return ~DrvInput[2] >> 8;
		case 0xC00005:
			return ~DrvInput[2] & 0xFF;
		case 0xC00006:							// Region
			return ~DrvInput[3] >> 8;
		case 0xC00007:
			return ~DrvInput[3] & 0xFF;
		case 0xC00008: {						// Inputs / Sound CPU status
			return ~DrvInput[1] >> 8;
		case 0xC80009:
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			PsikyoSynchroniseZ80(0);
			if (!nSoundlatchAck) {
				return ~DrvInput[1] & 0xFF;
			}
			return ~(DrvInput[1] | 0x80) & 0xFF;
		}
		case 0xC0000B:							// VBlank
			return ~bVBlank;

//		default: {
//			bprintf(PRINT_NORMAL, "Attempt to read word value of location %x\n", sekAddress);
//		}
	}
	return 0;
}

static UINT16 __fastcall samuraiaReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0];
		case 0xC00004:							// DIPs
			return ~DrvInput[2];
		case 0xC00006:							//
			return ~DrvInput[3];
		case 0xC00008: {						// Inputs / Sound CPU status
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			PsikyoSynchroniseZ80(0);
			if (!nSoundlatchAck) {
				return ~DrvInput[1];
			}
			return ~(DrvInput[1] | 0x80);
		}
		case 0xC0000A:							// VBlank
			return ~bVBlank;

//		default: {
//			bprintf(PRINT_NORMAL, "Attempt to read word value of location %x\n", sekAddress);
//		}
	}
	return 0;
}

// ----------------------------------------------------------------------------

static UINT8 __fastcall gunbirdReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0] >> 8;
		case 0xC00001:
			return ~DrvInput[0] & 0xFF;
		case 0xC00002:							// Inputs / Sound CPU status
			return ~DrvInput[1] >> 8;
		case 0xC00003:
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) {
				PsikyoSynchroniseZ80(0);
			}
			if (!nSoundlatchAck) {
				return ~DrvInput[1] & 0xFF;
			}
			return ~(DrvInput[1] | 0x80) & 0xFF;
		case 0xC00004:							// DIPs
			return ~DrvInput[2] >> 8;
		case 0xC00005:
			return ~DrvInput[2] & 0xFF;
		case 0xC00006:							// Region / VBlank
			return ~DrvInput[3] >> 8;
		case 0xC00007:
			return ~(DrvInput[3] | (bVBlank << 7)) & 0xFF;

		case 0xC00018:
			if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL) return MSM6295Read(0);
			return 0;

		default: {
//			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static UINT16 __fastcall gunbirdReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0];
		case 0xC00002: {						// Inputs / Sound CPU status
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) {
				PsikyoSynchroniseZ80(0);
			}
			if (!nSoundlatchAck) {
				return ~DrvInput[1];
			}
			return ~(DrvInput[1] | 0x80);
		}
		case 0xC00004:							// DIPs
			return ~DrvInput[2];
		case 0xC00006:							// Region / VBlank
			return ~(DrvInput[3] | (bVBlank << 7));

		default: {
//			bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static void __fastcall gunbirdWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {
		case 0xC00011:							// Sound latch
			SendSoundCommand(byteValue);
			break;

		case 0xC00018:
			if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL) MSM6295Write(0, byteValue);
			break;

		case 0xC00019:
			if ((byteValue & 7) >= 5) return;
			if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL) MSM6295SetBank(0, PsikyoSampleROM01 + 0x30000 + (byteValue & 7) * 0x10000, 0x30000, 0x3FFFF);
			break;

		default: {
//			bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
		}
	}
}

static void __fastcall gunbirdWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0xC00012:							// Sound latch
			SendSoundCommand(wordValue & 0xFF);
			break;

		case 0xC00018:
			if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL) MSM6295Write(0, wordValue >> 8);
			if ((wordValue & 7) >= 5) return;
			if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL) MSM6295SetBank(0, PsikyoSampleROM01 + 0x30000 + (wordValue & 7) * 0x10000, 0x30000, 0x3FFFF);
			break;

		default: {
//			bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
		}
	}
}

// ----------------------------------------------------------------------------

static UINT8 s1945_table[256] = {
	0x00, 0x00, 0x64, 0xae, 0x00, 0x00, 0x26, 0x2c, 0x00, 0x00, 0x2c, 0xda, 0x00, 0x00, 0x2c, 0xbc,
	0x00, 0x00, 0x2c, 0x9e, 0x00, 0x00, 0x2f, 0x0e, 0x00, 0x00, 0x31, 0x10, 0x00, 0x00, 0xc5, 0x1e,
	0x00, 0x00, 0x32, 0x90, 0x00, 0x00, 0xac, 0x5c, 0x00, 0x00, 0x2b, 0xc0
};

static UINT8 s1945j_table[256] = {
	0x00, 0x00, 0x64, 0xb6, 0x00, 0x00, 0x26, 0x2c, 0x00, 0x00, 0x2c, 0xda, 0x00, 0x00, 0x2c, 0xbc,
	0x00, 0x00, 0x2c, 0x9e, 0x00, 0x00, 0x2f, 0x0e, 0x00, 0x00, 0x31, 0x10, 0x00, 0x00, 0xc5, 0x92,
	0x00, 0x00, 0x32, 0x90, 0x00, 0x00, 0xac, 0x64, 0x00, 0x00, 0x2b, 0xc0
};

static UINT8 s1945a_table[256] = {
	0x00, 0x00, 0x64, 0xbe, 0x00, 0x00, 0x26, 0x2c, 0x00, 0x00, 0x2c, 0xda, 0x00, 0x00, 0x2c, 0xbc,
	0x00, 0x00, 0x2c, 0x9e, 0x00, 0x00, 0x2f, 0x0e, 0x00, 0x00, 0x31, 0x10, 0x00, 0x00, 0xc7, 0x2a,
	0x00, 0x00, 0x32, 0x90, 0x00, 0x00, 0xad, 0x4c, 0x00, 0x00, 0x2b, 0xc0
};

static UINT8 s1945_mcu_direction, s1945_mcu_latch1, s1945_mcu_latch2, s1945_mcu_inlatch, s1945_mcu_index;
static UINT8 s1945_mcu_latching, s1945_mcu_mode, s1945_mcu_control, s1945_mcu_bctrl;
static const UINT8 *s1945_mcu_table;

static INT32 TengaiMCUScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {						// Return minimum compatible version
		*pnMin = 0x020998;
	}

	SCAN_VAR(s1945_mcu_direction);
	SCAN_VAR(s1945_mcu_latch1);
	SCAN_VAR(s1945_mcu_latch2);
	SCAN_VAR(s1945_mcu_inlatch);
	SCAN_VAR(s1945_mcu_index);
	SCAN_VAR(s1945_mcu_latching);
	SCAN_VAR(s1945_mcu_mode);
	SCAN_VAR(s1945_mcu_control);
	SCAN_VAR(s1945_mcu_bctrl);

	if (nAction & ACB_WRITE) {
		PsikyoSetTileBank(1, (s1945_mcu_bctrl >> 6) & 3);
		PsikyoSetTileBank(0, (s1945_mcu_bctrl >> 4) & 3);
	}

	return 0;
}

static void TengaiMCUInit(const UINT8 *mcu_table)
{
	s1945_mcu_direction = 0x00;
	s1945_mcu_inlatch = 0xff;
	s1945_mcu_latch1 = 0xff;
	s1945_mcu_latch2 = 0xff;
	s1945_mcu_latching = 0x5;
	s1945_mcu_control = 0xff;
	s1945_mcu_index = 0;
	s1945_mcu_mode = 0;
	s1945_mcu_table = mcu_table;
	s1945_mcu_bctrl = 0x00;
}

static void tengaiMCUWrite(UINT32 offset, UINT8 data)
{
	switch (offset) {
		case 0x06:
			s1945_mcu_inlatch = data;
			break;
		case 0x07:
			PsikyoSetTileBank(1, (data >> 6) & 3);
			PsikyoSetTileBank(0, (data >> 4) & 3);
			s1945_mcu_bctrl = data;
			break;
		case 0x08:
			s1945_mcu_control = data;
			break;
		case 0x09:
			s1945_mcu_direction = data;
			break;
		case 0x0b:
			switch (data | (s1945_mcu_direction ? 0x100 : 0)) {
				case 0x11c:
					s1945_mcu_latching = 5;
					s1945_mcu_index = s1945_mcu_inlatch;
					break;
				case 0x013:
//					logerror("MCU: Table read index %02x\n", s1945_mcu_index);
					s1945_mcu_latching = 1;
					s1945_mcu_latch1 = s1945_mcu_table[s1945_mcu_index];
					break;
				case 0x113:
					s1945_mcu_mode = s1945_mcu_inlatch;
					if(s1945_mcu_mode == 1) {
						s1945_mcu_latching &= ~1;
						s1945_mcu_latch2 = 0x55;
					} else {
						// Go figure.
						s1945_mcu_latching &= ~1;
						s1945_mcu_latching |= 2;
					}
					s1945_mcu_latching &= ~4;
					s1945_mcu_latch1 = s1945_mcu_inlatch;
					break;
				case 0x010:
				case 0x110:
					s1945_mcu_latching |= 4;
					break;
				default:
//					logerror("MCU: function %02x, direction %02x, latch1 %02x, latch2 %02x (%x)\n", data, s1945_mcu_direction, s1945_mcu_latch1, s1945_mcu_latch2, activecpu_get_pc());
					break;
			}
			break;
//		default:
//			logerror("MCU.w %x, %02x (%x)\n", offset, data, activecpu_get_pc());
	}
}

static UINT16 tengaiMCURead(UINT32 offset)
{
	switch (offset) {
		case 0: {
			UINT16 res;
			if (s1945_mcu_control & 16) {
				res = (s1945_mcu_latching & 4) ? 0xff00 : s1945_mcu_latch1 << 8;
				s1945_mcu_latching |= 4;
			} else {
				res = (s1945_mcu_latching & 1) ? 0xff00 : s1945_mcu_latch2 << 8;
				s1945_mcu_latching |= 1;
			}
			res |= s1945_mcu_bctrl & 0x00f0;
			return res;
		}
		case 1:
			return (s1945_mcu_latching << 8) | 0x0800;
	}

	return 0;
}

static UINT8 __fastcall tengaiReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0] >> 8;
		case 0xC00001:
			return ~DrvInput[0] & 0xFF;
		case 0xC00002:							// Inputs / Sound CPU status
			return ~DrvInput[1] >> 8;
		case 0xC00003:
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			PsikyoSynchroniseZ80(0);
			if (!nSoundlatchAck) {
				return ~(DrvInput[1] | 0x04) & 0xFF;
			}
			return ~(DrvInput[1] | 0x84) & 0xFF;
		case 0xC00004:							// DIPs
			return ~DrvInput[2] >> 8;
		case 0xC00005:
			return ~DrvInput[2] & 0xFF;
		case 0xC00006:							// Region / MCU
			return tengaiMCURead(0) >> 8;
		case 0xC00007:
			return ((~DrvInput[3] & 0x0F) | tengaiMCURead(0)) & 0xFF;
		case 0xC00008:							// MCU
			return tengaiMCURead(1) >> 8;
		case 0xC00009:
			return tengaiMCURead(1) & 0xFF;

//		default: {
//			bprintf(PRINT_NORMAL, "Attempt to read byte value of location %x\n", sekAddress);
//		}
	}
	return 0;
}

static UINT16 __fastcall tengaiReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {
		case 0xC00000:							// Joysticks
			return ~DrvInput[0];
		case 0xC00002: {						// Inputs / Sound CPU status
//			bprintf(PRINT_NORMAL, _T("  - Sound reply read.\n"));
			PsikyoSynchroniseZ80(0);
			if (!nSoundlatchAck) {
				return ~(DrvInput[1] | 0x04);
			}
			return ~(DrvInput[1] | 0x84);
		}
		case 0xC00004:							// DIPs
			return ~DrvInput[2];
		case 0xC00006:							// Region / MCU
			return (~DrvInput[3] & 0x0F) | tengaiMCURead(0);
		case 0xC00008:							// MCU
			return tengaiMCURead(1);

//		default: {
//			bprintf(PRINT_NORMAL, "Attempt to read word value of location %x\n", sekAddress);
//		}
	}
	return 0;
}

static void __fastcall tengaiWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {
		case 0xC00004:
		case 0xC00005:
		case 0xC00006:
		case 0xC00007:
		case 0xC00008:
		case 0xC00009:
		case 0xC0000A:
		case 0xC0000B:
			tengaiMCUWrite(sekAddress & 0x0F, byteValue);
			break;

		case 0xC00011:							// Sound latch
			SendSoundCommand(byteValue);
			break;

		default: {
//			bprintf(PRINT_NORMAL, "Attempt to write byte value %x to location %x\n", byteValue, sekAddress);
		}
	}
}

static void __fastcall tengaiWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0xC00004:
		case 0xC00005:
		case 0xC00006:
		case 0xC00007:
		case 0xC00008:
		case 0xC00009:
		case 0xC0000A:
		case 0xC0000B:
			tengaiMCUWrite((sekAddress & 0x0F), wordValue >> 8);
			tengaiMCUWrite((sekAddress & 0x0F) + 1, wordValue & 0xFF);
			break;

		case 0xC00010:							// Sound latch
			SendSoundCommand(wordValue & 0xFF);
			break;

//		default: {
//			bprintf(PRINT_NORMAL, "Attempt to write word value %x to location %x\n", wordValue, sekAddress);
//		}
	}
}

// ----------------------------------------------------------------------------
// Z80 banswitch

static void samuraiaZ80SetBank(INT32 nBank)
{
	nBank &= 0x03;
	if (nBank != nPsikyoZ80Bank) {
		UINT8* nStartAddress = PsikyoZ80ROM + (nBank << 15);
		ZetMapMemory(nStartAddress, 0x8000, 0xFFFF, MAP_ROM);
		nPsikyoZ80Bank = nBank;
	}

	return;
}

static void gunbirdZ80SetBank(INT32 nBank)
{
	nBank &= 0x03;
	if (nBank != nPsikyoZ80Bank) {
		UINT8* nStartAddress = PsikyoZ80ROM + 0x00200 + (nBank << 15);
		ZetMapMemory(nStartAddress, 0x8200, 0xFFFF, MAP_ROM);
		nPsikyoZ80Bank = nBank;
	}

	return;
}

// ----------------------------------------------------------------------------
// Z80 I/O handlers

static UINT8 __fastcall samuraiaZ80In(UINT16 nAddress)
{
	switch (nAddress & 0xFF) {
		case 0x00:
			return BurnYM2610Read(0);
		case 0x02:
			return BurnYM2610Read(2);
		case 0x08:									// Read sound command
//			bprintf(PRINT_NORMAL, _T("  - Sound command received (0x%02X).\n"), nSoundlatch);
			return nSoundlatch;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 read port 0x%04X.\n"), nAddress);
//		}
	}

	return 0;
}

static void __fastcall samuraiaZ80Out(UINT16 nAddress, UINT8 nValue)
{
	switch (nAddress & 0x0FF) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			BurnYM2610Write(nAddress & 3, nValue);
			break;

		case 0x04:
			samuraiaZ80SetBank(nValue);
			break;

		case 0x0C:									// Write reply to sound commands
//			bprintf(PRINT_NORMAL, _T("  - Sound reply sent (0x%02X).\n"), nValue);

			nSoundlatchAck = 1;

			break;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 port 0x%04X -> 0x%02X.\n"), nAddress, nValue);
//		}
	}
}

static UINT8 __fastcall gunbirdZ80In(UINT16 nAddress)
{
	switch (nAddress & 0xFF) {
		case 0x04:
//			bprintf(PRINT_NORMAL, _T("    read 0 %6i\n"), ZetTotalCycles());
			return BurnYM2610Read(0);
		case 0x06:
//			bprintf(PRINT_NORMAL, _T("    read 2 %6i\n"), ZetTotalCycles());
			return BurnYM2610Read(2);
		case 0x08:									// Read sound command
//			bprintf(PRINT_NORMAL, _T("  - Sound command received (0x%02X).\n"), nSoundlatch);
			return nSoundlatch;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 read port 0x%04X.\n"), nAddress);
//		}
	}

	return 0;
}

static void __fastcall gunbirdZ80Out(UINT16 nAddress, UINT8 nValue)
{
	switch (nAddress & 0x0FF) {
		case 0x00:
			gunbirdZ80SetBank(nValue >> 4);
			break;
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07: {
			BurnYM2610Write(nAddress & 3, nValue);
			break;
		}
		case 0x0C:									// Write reply to sound commands
//			bprintf(PRINT_NORMAL, _T("  - Sound reply sent (0x%02X).\n"), nValue);

			nSoundlatchAck = 1;

			break;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 port 0x%04X -> 0x%02X.\n"), nAddress, nValue);
//		}
	}
}

static UINT8 __fastcall tengaiZ80In(UINT16 nAddress)
{
	switch (nAddress & 0xFF) {
		case 0x08:
//			bprintf(PRINT_NORMAL, _T("    read 0 %6i\n"), ZetTotalCycles());
			return BurnYMF278BReadStatus();
		case 0x10:									// Read sound command
//			bprintf(PRINT_NORMAL, _T("  - Sound command received (0x%02X).\n"), nSoundlatch);
			return nSoundlatch;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 read port 0x%04X.\n"), nAddress);
//		}
	}

	return 0;
}

static void __fastcall tengaiZ80Out(UINT16 nAddress, UINT8 nValue)
{
	switch (nAddress & 0x0FF) {
		case 0x00:
			gunbirdZ80SetBank(nValue >> 4);
			break;
		case 0x08:
		case 0x0A:
		case 0x0C: {
			BurnYMF278BSelectRegister((nAddress >> 1) & 3, nValue);
			break;
		}
		case 0x09:
		case 0x0B:
		case 0x0D: {
			BurnYMF278BWriteRegister((nAddress >> 1) & 3, nValue);
			break;
		}
		case 0x18:									// Write reply to sound commands
//			bprintf(PRINT_NORMAL, _T("  - Sound reply sent (0x%02X).\n"), nValue);

			nSoundlatchAck = 1;

			break;

//		default: {
//			bprintf(PRINT_NORMAL, _T("  - Z80 port 0x%04X -> 0x%02X.\n"), nAddress, nValue);
//		}
	}
}

// Callbacks for the FM chip

static void PsikyoFMIRQHandler(INT32, INT32 nStatus)
{
//	bprintf(PRINT_NORMAL, _T("  - IRQ -> %i.\n"), nStatus);

	ZetSetIRQLine(0xFF, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

// ----------------------------------------------------------------------------

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();

	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL)
	{
		ZetOpen(0);

		nPsikyoZ80Bank = -1;
		switch (PsikyoHardwareVersion) {
			case PSIKYO_HW_SAMURAIA: {
				samuraiaZ80SetBank(0);
				break;
			}
			case PSIKYO_HW_GUNBIRD:
			case PSIKYO_HW_S1945:
			case PSIKYO_HW_TENGAI: {
				gunbirdZ80SetBank(0);
				break;
			}
		}

		ZetReset();
		ZetClose();
	}

	switch (PsikyoHardwareVersion) {
		case PSIKYO_HW_SAMURAIA:
		case PSIKYO_HW_GUNBIRD: {
			BurnYM2610Reset();
			break;
		}
		case PSIKYO_HW_S1945:
		case PSIKYO_HW_TENGAI: {
			BurnYMF278BReset();
			break;
		}
		case PSIKYO_HW_S1945BL: {
			MSM6295SetBank(0, PsikyoSampleROM01, 0x00000, 0x3FFFF);
			MSM6295Reset();
			break;
		}
	}

	nSoundlatch = 0;
	nSoundlatchAck = 1;

	nCyclesDone[0] = nCyclesDone[1] = 0;

	HiscoreReset();

	return 0;
}

// This routine is called first to determine how much memory is needed (MemEnd-(UINT8 *)0),
// and then afterwards to set up all the pointers

static INT32 MemIndex()
{
	UINT8* Next; Next = Mem;

	Psikyo68KROM		= Next; Next += 0x100000;		// 68K program
	PsikyoZ80ROM		= Next; Next += 0x020000;		// Z80 program

	PsikyoSpriteROM		= Next; Next += PsikyoSpriteROMSize;
	PsikyoSpriteLUT		= Next; Next += 0x040000;
	PsikyoTileROM		= Next; Next += PsikyoTileROMSize;

	MSM6295ROM			= Next;
	PsikyoSampleROM01	= Next; Next += PsikyoSampleROM01Size;
	PsikyoSampleROM02	= Next; Next += PsikyoSampleROM02Size;

	RamStart			= Next;

	Psikyo68KRAM		= Next; Next += 0x020000;		// 68K work RAM
	PsikyoZ80RAM		= Next; Next += 0x000800;		// Z80 work RAM
	PsikyoTileRAM[0]	= Next; Next += 0x002000;		// Tile layer 0
	PsikyoTileRAM[1]	= Next; Next += 0x002000;		// Tile layer 1
	PsikyoTileRAM[2]	= Next; Next += 0x004000;		// Tile attribute RAM
	PsikyoSpriteRAM		= Next; Next += 0x002000;
	PsikyoBootSpriteBuf = Next; Next += 0x001000;		// Bootleg Sprite Buffer
	PsikyoPalSrc		= Next; Next += 0x002000;		// palette

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 PsikyoLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[7] = { Psikyo68KROM, PsikyoZ80ROM, PsikyoSpriteROM, PsikyoTileROM, PsikyoSampleROM01, PsikyoSampleROM02, PsikyoSpriteLUT };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) // 68k rom loading
		{
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[0] + 0, i + 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(pLoad[0] + 2, i + 1, 4, LD_GROUP(2))) return 1;
			}
			i++;
			continue;
		}

		if ((ri.nType & 0x0f) >= 2 && (ri.nType & 0x0f) <= 7) // everything else
		{
			if (bLoad) {
				if (BurnLoadRom(pLoad[(ri.nType & 0xf) - 1], i, 1)) return 1;
			}
			pLoad[(ri.nType & 0xf) - 1] += ri.nLen;
			continue;
		}
	}

	if (!bLoad) {
		PsikyoSampleROM01Size = pLoad[4] - PsikyoSampleROM01;
		PsikyoSampleROM02Size = pLoad[5] - PsikyoSampleROM02;
		PsikyoSpriteROMSize = pLoad[2] - PsikyoSpriteROM;
		PsikyoSpriteROMSize += (PsikyoSpriteROMSize & 0x100000);
		PsikyoSpriteROMSize <<= 1;
		PsikyoTileROMSize = (pLoad[3] - PsikyoTileROM) << 1;
	} else {
		if (PsikyoHardwareVersion != PSIKYO_HW_TENGAI) {
			if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) {
				BurnByteswap(PsikyoSpriteROM, PsikyoSpriteROMSize / 2);
			}
			BurnByteswap(PsikyoTileROM, PsikyoTileROMSize / 2);
		}
			
		BurnNibbleExpand(PsikyoSpriteROM, NULL, PsikyoSpriteROMSize / 2, 0, 0);
		BurnNibbleExpand(PsikyoTileROM, NULL, PsikyoTileROMSize / 2, 0, 0);
	}

	return 0;
}

static INT32 CommonInit(INT32 HardwareVersion, const UINT8 *mcu_table, void (*pCallback)())
{
	BurnSetRefreshRate(15625.0 / 263.5);

	PsikyoHardwareVersion = HardwareVersion;

	TengaiMCUInit(mcu_table);

	PsikyoLoadRoms(false);

	// Find out how much memory is needed
	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex();													// Index the allocated memory

	if (PsikyoLoadRoms(true)) return 1;

	if (pCallback) pCallback();

	{
		// 68EC020 setup
		SekInit(0, 0x68EC020);												// Allocate 68000
	    SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Psikyo68KROM,			0x000000, 0x0FFFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(PsikyoBootSpriteBuf,	0x200000, 0x200FFF, MAP_RAM);	// Bootleg only
		SekMapMemory(PsikyoSpriteRAM,		0x400000, 0x401FFF, MAP_RAM);
		SekMapMemory(PsikyoTileRAM[0],		0x800000, 0x801FFF, MAP_RAM);
		SekMapMemory(PsikyoTileRAM[1],		0x802000, 0x803FFF, MAP_RAM);
		SekMapMemory(PsikyoTileRAM[2],		0x804000, 0x807FFF, MAP_RAM);
		SekMapMemory(Psikyo68KRAM,			0xFE0000, 0xFFFFFF, MAP_RAM);

		SekMapMemory(PsikyoPalSrc,			0x600000, 0x601FFF, MAP_ROM);	// Palette RAM (write goes through handler)
		SekMapHandler(1,					0x600000, 0x601FFF, MAP_WRITE);	//

		switch (PsikyoHardwareVersion) {
			case PSIKYO_HW_SAMURAIA: {
				SekSetReadWordHandler(0, samuraiaReadWord);
				SekSetReadByteHandler(0, samuraiaReadByte);
				SekSetWriteWordHandler(0, gunbirdWriteWord);
				SekSetWriteByteHandler(0, gunbirdWriteByte);
				break;
			}
			case PSIKYO_HW_GUNBIRD:
			case PSIKYO_HW_S1945BL: {
				SekSetReadWordHandler(0, gunbirdReadWord);
				SekSetReadByteHandler(0, gunbirdReadByte);
				SekSetWriteWordHandler(0, gunbirdWriteWord);
				SekSetWriteByteHandler(0, gunbirdWriteByte);
				break;
			}
			case PSIKYO_HW_S1945:
			case PSIKYO_HW_TENGAI: {
				SekSetReadWordHandler(0, tengaiReadWord);
				SekSetReadByteHandler(0, tengaiReadByte);
				SekSetWriteWordHandler(0, tengaiWriteWord);
				SekSetWriteByteHandler(0, tengaiWriteByte);
				break;
			}
		}

		SekSetWriteWordHandler(1, PsikyoPalWriteWord);
		SekSetWriteByteHandler(1, PsikyoPalWriteByte);

		SekClose();
	}

	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL)
	{
		// Z80 setup
		ZetInit(0);
		ZetOpen(0);

		switch (PsikyoHardwareVersion) {
			case PSIKYO_HW_SAMURAIA: {
				ZetMapMemory(PsikyoZ80ROM, 0x0000, 0x77ff, MAP_ROM);
				ZetMapMemory(PsikyoZ80RAM, 0x7800, 0x7fff, MAP_RAM);
				ZetSetInHandler(samuraiaZ80In);
				ZetSetOutHandler(samuraiaZ80Out);
				break;
			}
			case PSIKYO_HW_GUNBIRD: {
				ZetMapMemory(PsikyoZ80ROM, 0x0000, 0x7fff, MAP_ROM);
				ZetMapMemory(PsikyoZ80RAM, 0x8000, 0x81ff, MAP_RAM);
				ZetSetInHandler(gunbirdZ80In);
				ZetSetOutHandler(gunbirdZ80Out);
				break;
			}
			case PSIKYO_HW_S1945:
			case PSIKYO_HW_TENGAI: {
				ZetMapMemory(PsikyoZ80ROM, 0x0000, 0x7fff, MAP_ROM);
				ZetMapMemory(PsikyoZ80RAM, 0x8000, 0x81ff, MAP_RAM);
				ZetSetInHandler(tengaiZ80In);
				ZetSetOutHandler(tengaiZ80Out);
				break;
			}
		}

		ZetClose();
	}

	switch (PsikyoHardwareVersion) {
		case PSIKYO_HW_SAMURAIA:
		case PSIKYO_HW_GUNBIRD: {
			BurnYM2610Init(8000000, PsikyoSampleROM02, &PsikyoSampleROM02Size, PsikyoSampleROM01, &PsikyoSampleROM01Size, &PsikyoFMIRQHandler, 0);
			BurnTimerAttachZet(4000000);
			BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
			BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
			BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 1.20, BURN_SND_ROUTE_BOTH);
			break;
		}
		case PSIKYO_HW_S1945:
		case PSIKYO_HW_TENGAI: {
			BurnYMF278BInit(0, PsikyoSampleROM02, PsikyoSampleROM02Size, &PsikyoFMIRQHandler);
			BurnYMF278BSetRoute(BURN_SND_YMF278B_YMF278B_ROUTE_1, 2.80, BURN_SND_ROUTE_LEFT);
			BurnYMF278BSetRoute(BURN_SND_YMF278B_YMF278B_ROUTE_2, 2.80, BURN_SND_ROUTE_RIGHT);
			BurnTimerAttachZet(4000000);
			break;
		}
		case PSIKYO_HW_S1945BL: {
			MSM6295Init(0, 1122000 / 132, 0);
			MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);
			break;
		}
	}

	GenericTilesInit();
	PsikyoPalInit();
	PsikyoTileInit(PsikyoTileROMSize);
	PsikyoSpriteInit(PsikyoSpriteROMSize);

	bprintf (0, _T("past init!\n"));

	nPrevBurnCPUSpeedAdjust = -1;

	DrvDoReset(); // Reset machine

	return 0;
}

static INT32 DrvExit()
{
	switch (PsikyoHardwareVersion) {
		case PSIKYO_HW_SAMURAIA:
		case PSIKYO_HW_GUNBIRD: {
			BurnYM2610Exit();
			break;
		}
		case PSIKYO_HW_S1945:
		case PSIKYO_HW_TENGAI: {
			BurnYMF278BExit();
			break;
		}
		case PSIKYO_HW_S1945BL: {
			MSM6295Exit();
			MSM6295ROM = NULL;
			break;
		}
	}

	GenericTilesExit();

	PsikyoSpriteExit();
	PsikyoTileExit();
	PsikyoPalExit();

	SekExit();
	ZetExit();

	BurnFree(Mem);

	return 0;
}

static INT32 DrvDraw()
{
	PsikyoPalUpdate();

	PsikyoTileRender();

	BurnTransferCopy(PsikyoPalette);
	
	return 0;
}

static INT32 DrvFrame()
{
	INT32 nCyclesVBlank;
	INT32 nInterleave = 16;

	if (DrvReset) {														// Reset machine
		DrvDoReset();
	}

	// Compile digital inputs
	DrvInput[0] = 0x0000;  												// Joysticks
	DrvInput[1] = 0x0000;
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << (i + 8);
		DrvInput[0] |= (DrvJoy2[i] & 1) << (i + 0);

		DrvInput[1] |= (DrvInp1[i] & 1) << (i + 0);
		DrvInput[1] |= (DrvInp2[i] & 1) << (i + 8);
	}

	SekNewFrame();
	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) ZetNewFrame();
	
	SekOpen(0);

	if (nPrevBurnCPUSpeedAdjust != nBurnCPUSpeedAdjust) {
		// 68K CPU clock is 16MHz, modified by nBurnCPUSpeedAdjust
		nCyclesTotal[0] = (INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (INT32)(256.0 * 15625.0 / 263.5));
		// Z80 CPU clock is always 4MHz
		nCyclesTotal[1] =  (INT32)(4000000.0 / (15625.0 / 263.5));

		// 68K cycles executed each scanline
		SekSetCyclesScanline((INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (256 * 15625)));

		nPrevBurnCPUSpeedAdjust = nBurnCPUSpeedAdjust;
	}

	nCyclesVBlank = nCyclesTotal[0] * (INT32)(224.0 * 2.0) / (INT32)(263.5 * 2.0);
	bVBlank = 0x01;

	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) ZetOpen(0);

	SekIdle(nCyclesDone[0]);
	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) ZetIdle(nCyclesDone[1]);

	for (INT32 i = 1; i <= nInterleave; i++) {

		// Run 68000
		INT32 nNext = i * nCyclesTotal[0] / nInterleave;

		// See if we need to trigger the VBlank interrupt
		if (bVBlank && nNext >= nCyclesVBlank) {
			if (nCyclesDone[0] < nCyclesVBlank) {
				nCyclesSegment = nCyclesVBlank - nCyclesDone[0];
				nCyclesDone[0] += SekRun(nCyclesSegment);
			}

			if (pBurnDraw) {
				DrvDraw();												// Draw screen if needed
			}
			PsikyoSpriteBuffer();

			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
			bVBlank = 0x00;
		}

		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesDone[0] += SekRun(nCyclesSegment);
	}

	switch (PsikyoHardwareVersion) {
		case PSIKYO_HW_SAMURAIA:
		case PSIKYO_HW_GUNBIRD: {
			nCycles68KSync = SekTotalCycles();
			BurnTimerEndFrame(nCyclesTotal[1]);
			if (pBurnSoundOut) BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
			break;
		}
		case PSIKYO_HW_S1945:
		case PSIKYO_HW_TENGAI: {
			nCycles68KSync = SekTotalCycles();
			BurnTimerEndFrame(nCyclesTotal[1]);
			if (pBurnSoundOut) BurnYMF278BUpdate(nBurnSoundLen);
			break;
		}
		case PSIKYO_HW_S1945BL: {
			MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
			break;
		}
		default: {
			ZetIdle(nCyclesTotal[1] - ZetTotalCycles());
		}
	}

	nCyclesDone[0] = SekTotalCycles() - nCyclesTotal[0];
	if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL) {
		nCyclesDone[1] = ZetTotalCycles() - nCyclesTotal[1];
		ZetClose();
	}
	SekClose();

	return 0;
}

// ----------------------------------------------------------------------------
// Savestates / scanning

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {						// Return minimum compatible version
		*pnMin =  0x029521;
	}

	if (nAction & ACB_MEMORY_ROM) {								// Scan all memory, devices & variables
		ba.Data		= Psikyo68KROM;
		ba.nLen		= 0x00100000;
		ba.nAddress = 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);

		ba.Data		= PsikyoZ80ROM;
		ba.nLen		= 0x00020000;
		ba.nAddress = 0x100000;
		ba.szName	= "Z80 ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {								// Scan all memory, devices & variables
		ba.Data		= Psikyo68KRAM;
		ba.nLen		= 0x00020000;
		ba.nAddress = 0xFE0000;
		ba.szName	= "68K RAM";
		BurnAcb(&ba);

		ba.Data		= PsikyoZ80RAM;
		if (PsikyoHardwareVersion == PSIKYO_HW_SAMURAIA) {
			ba.nLen		= 0x0000800;
		} else {
			ba.nLen		= 0x0000200;
		}
		ba.nAddress = 0x180000;
		ba.szName	= "Z80 RAM";
		BurnAcb(&ba);

		ba.Data		= PsikyoTileRAM[0];
		ba.nLen		= 0x00002000;
		ba.nAddress = 0x800000;
		ba.szName	= "Tilemap 0";
		BurnAcb(&ba);

		ba.Data		= PsikyoTileRAM[1];
		ba.nLen		= 0x00002000;
		ba.nAddress = 0x802000;
		ba.szName	= "Tilemap 1";
		BurnAcb(&ba);

		ba.Data		= PsikyoTileRAM[2];
		ba.nLen		= 0x00004000;
		ba.nAddress = 0x804000;
		ba.szName	= "Tilemap attributes";
		BurnAcb(&ba);

		if (PsikyoHardwareVersion == PSIKYO_HW_S1945BL)
		{
			ba.Data		= PsikyoBootSpriteBuf;
			ba.nLen		= 0x00001000;
			ba.nAddress = 0x200000;
			ba.szName	= "Sprite Buffer Bootleg";
			BurnAcb(&ba);
		}

		ba.Data		= PsikyoSpriteRAM;
		ba.nLen		= 0x00002000;
		ba.nAddress = 0x400000;
		ba.szName	= "Sprite tables";
		BurnAcb(&ba);

		ba.Data		= PsikyoPalSrc;
		ba.nLen		= 0x00002000;
		ba.nAddress = 0x600000;
		ba.szName	= "Palette";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		SekScan(nAction);										// Scan 68000 state

		if (PsikyoHardwareVersion != PSIKYO_HW_S1945BL)
			ZetScan(nAction);										// Scan Z80 state

		SCAN_VAR(nCyclesDone);

		SCAN_VAR(bVBlank);

		switch (PsikyoHardwareVersion) {
			case PSIKYO_HW_SAMURAIA:
				case PSIKYO_HW_GUNBIRD: {
					BurnYM2610Scan(nAction, pnMin);
					break;
				}
				case PSIKYO_HW_S1945:
				case PSIKYO_HW_TENGAI: {
					BurnYMF278BScan(nAction, pnMin);
					break;
				}
				case PSIKYO_HW_S1945BL: {
					MSM6295Scan(nAction, pnMin);
					break;
				}
		}

		SCAN_VAR(nSoundlatch); SCAN_VAR(nSoundlatchAck);

		SCAN_VAR(nPsikyoZ80Bank);

		if (PsikyoHardwareVersion == PSIKYO_HW_TENGAI || PsikyoHardwareVersion == PSIKYO_HW_S1945) {
			TengaiMCUScan(nAction, pnMin);
		}

		if (nAction & ACB_WRITE) {
			int nBank = nPsikyoZ80Bank;
			nPsikyoZ80Bank = -1;

			switch (PsikyoHardwareVersion) {
				case PSIKYO_HW_SAMURAIA: {
					ZetOpen(0);
					samuraiaZ80SetBank(nBank);
					ZetClose();
					break;
				}
				case PSIKYO_HW_GUNBIRD:
				case PSIKYO_HW_S1945:
				case PSIKYO_HW_TENGAI: {
					ZetOpen(0);
					gunbirdZ80SetBank(nBank);
					ZetClose();
					break;
				}
			}

			PsikyoRecalcPalette = 1;
		}
	}

	return 0;
}

// ----------------------------------------------------------------------------
// Rom information


// Samurai Aces (World)

static struct BurnRomInfo samuraiaRomDesc[] = {
	{ "4-u127.bin",   0x040000, 0x8c9911ca, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5-u126.bin",   0x040000, 0xd20c3ef0, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x00a546cb, 3 | BRF_GRA },			 //  2 Sprite data

	{ "u11.bin",      0x040000, 0x11a04d91, 7 | BRF_GRA },			 //  3 Sprite LUT

	{ "u34.bin",      0x100000, 0xe6a75bd8, 4 | BRF_GRA },			 //  4 Tile data
	{ "u35.bin",      0x100000, 0xc4ca0164, 4 | BRF_GRA },			 //  5

	{ "3-u58.bin",    0x020000, 0x310f5c76, 2 | BRF_ESS | BRF_PRG }, //  6 CPU #1 code

	{ "u68.bin",      0x100000, 0x9a7f6c34, 6 | BRF_SND },			 //  7 YM2610 (delta-t) ADPCM data
};

STD_ROM_PICK(samuraia)
STD_ROM_FN(samuraia)

static void SamuraiaCallback()
{
	for (INT32 i = 0; i < 0x100000; i++) {
		PsikyoSampleROM02[i] = BITSWAP08(PsikyoSampleROM02[i], 6,7, 5,4,3,2,1,0);
	}
}

static INT32 SamuraiaInit() // sngkace, sngkacea
{
	return CommonInit(PSIKYO_HW_SAMURAIA, NULL, SamuraiaCallback);
}

struct BurnDriver BurnDrvSamuraia = {
	"samuraia", NULL, NULL, NULL, "1993",
	"Samurai Aces (World)\0", NULL, "Psikyo (Banpresto license)", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, samuraiaRomInfo, samuraiaRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, samuraiaDIPInfo,
	SamuraiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Samurai Aces (Korea?)
// PCB was found in Korea. Banpresto copyright removed (still shows the Psikyo splash screen)

static struct BurnRomInfo samuraiakRomDesc[] = {
	{ "u127.bin",     0x080000, 0xed4a3626, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "u126.bin",     0x080000, 0x21803147, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x00a546cb, 3 | BRF_GRA },			 //  2 Sprite data

	{ "u11.bin",      0x040000, 0x11a04d91, 7 | BRF_GRA },			 //  3 Sprite LUT

	{ "u34.bin",      0x100000, 0xe6a75bd8, 4 | BRF_GRA },			 //  4 Tile data
	{ "u35.bin",      0x100000, 0xc4ca0164, 4 | BRF_GRA },			 //  5

	{ "3-u58.bin",    0x020000, 0x310f5c76, 2 | BRF_ESS | BRF_PRG }, //  6 CPU #1 code

	{ "u68.bin",      0x100000, 0x9a7f6c34, 6 | BRF_SND },			 //  7 YM2610 (delta-t) ADPCM data
};

STD_ROM_PICK(samuraiak)
STD_ROM_FN(samuraiak)

struct BurnDriver BurnDrvSamuraiak = {
	"samuraiak", "samuraia", NULL, NULL, "1993",
	"Samurai Aces (Korea?)\0", NULL, "Psikyo (Banpresto license)", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, samuraiakRomInfo, samuraiakRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, samuraiaDIPInfo,
	SamuraiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Sengoku Ace (Japan, set 1)

static struct BurnRomInfo sngkaceRomDesc[] = {
	{ "1-u127.bin",   0x040000, 0x6c45b2f8, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2-u126.bin",   0x040000, 0x845a6760, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x00a546cb, 3 | BRF_GRA },			 //  2 Sprite data

	{ "u11.bin",      0x040000, 0x11a04d91, 7 | BRF_GRA },			 //  3 Sprite LUT

	{ "u34.bin",      0x100000, 0xe6a75bd8, 4 | BRF_GRA },			 //  4 Tile data
	{ "u35.bin",      0x100000, 0xc4ca0164, 4 | BRF_GRA },			 //  5

	{ "3-u58.bin",    0x020000, 0x310f5c76, 2 | BRF_ESS | BRF_PRG }, //  6 CPU #1 code

	{ "u68.bin",      0x100000, 0x9a7f6c34, 6 | BRF_SND },			 //  7 YM2610 (delta-t) ADPCM data
};

STD_ROM_PICK(sngkace)
STD_ROM_FN(sngkace)

struct BurnDriver BurnDrvSngkace = {
	"sngkace", "samuraia", NULL, NULL, "1993",
	"Sengoku Ace (Japan, set 1)\0", NULL, "Psikyo (Banpresto license)", "Psikyo 68EC020",
	L"\u6226\u56FD\u30A8\u30FC\u30B9 (Japan, set 1)\0Sengoku Ace\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, sngkaceRomInfo, sngkaceRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, sngkaceDIPInfo,
	SamuraiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Sengoku Ace (Japan, set 2)

static struct BurnRomInfo sngkaceaRomDesc[] = {
	// the roms have a very visible "." symbol after the number, it might indicate a newer revision.
	{ "1.-u127.bin",  0x040000, 0x3a43708d, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2.-u126.bin",  0x040000, 0x7aa50c46, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x00a546cb, 3 | BRF_GRA },			 //  2 Sprite data

	{ "u11.bin",      0x040000, 0x11a04d91, 7 | BRF_GRA },			 //  3 Sprite LUT

	{ "u34.bin",      0x100000, 0xe6a75bd8, 4 | BRF_GRA },			 //  4 Tile data
	{ "u35.bin",      0x100000, 0xc4ca0164, 4 | BRF_GRA },			 //  5

	{ "3-u58.bin",    0x020000, 0x310f5c76, 2 | BRF_ESS | BRF_PRG }, //  6 CPU #1 code

	{ "u68.bin",      0x100000, 0x9a7f6c34, 6 | BRF_SND },			 //  7 YM2610 (delta-t) ADPCM data
};

STD_ROM_PICK(sngkacea)
STD_ROM_FN(sngkacea)

struct BurnDriver BurnDrvSngkacea = {
	"sngkacea", "samuraia", NULL, NULL, "1993",
	"Sengoku Ace (Japan, set 2)\0", NULL, "Psikyo (Banpresto license)", "Psikyo 68EC020",
	L"\u6226\u56FD\u30A8\u30FC\u30B9 (Japan, set 2)\0Sengoku Ace\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, sngkaceaRomInfo, sngkaceaRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, sngkaceDIPInfo,
	SamuraiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Gunbird (World)

static struct BurnRomInfo gunbirdRomDesc[] = {
	{ "4.u46",        0x040000, 0xb78ec99d, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5.u39",        0x040000, 0x925f095d, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x7d7e8a00, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u24.bin",      0x200000, 0x5e3ffc9d, 3 | BRF_GRA },			 //  3
	{ "u15.bin",      0x200000, 0xa827bfb5, 3 | BRF_GRA },			 //  4
	{ "u25.bin",      0x100000, 0xef652e0c, 3 | BRF_GRA },			 //  5

	{ "u3.bin",       0x040000, 0x0905aeb2, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u33.bin",      0x200000, 0x54494e6b, 4 | BRF_GRA },			 //  7 Tile data

	{ "3.u71",        0x020000, 0x2168e4ba, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u64.bin",      0x080000, 0xe187ed4f, 5 | BRF_SND },			 //  9 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0x9e07104d, 6 | BRF_SND },			 // 10 YM2610 ADPCM data
	
	{ "3020.u19",     0x000001, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },// 11
	{ "3021.u69",     0x000001, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },// 12
};

STD_ROM_PICK(gunbird)
STD_ROM_FN(gunbird)

static INT32 GunbirdInit() // gunbirdj, gunbirdk, batlkroadk, batlkroad 
{
	return CommonInit(PSIKYO_HW_GUNBIRD, NULL, NULL);
}

struct BurnDriver BurnDrvGunbird = {
	"gunbird", NULL, NULL, NULL, "1994",
	"Gunbird (World)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, gunbirdRomInfo, gunbirdRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, gunbirdWorldDIPInfo,
	GunbirdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Gunbird (Korea)

static struct BurnRomInfo gunbirdkRomDesc[] = {
	{ "1k.u46",       0x080000, 0x745cee52, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2k.u39",       0x080000, 0x669632fb, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x7d7e8a00, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u24.bin",      0x200000, 0x5e3ffc9d, 3 | BRF_GRA },			 //  3
	{ "u15.bin",      0x200000, 0xa827bfb5, 3 | BRF_GRA },			 //  4
	{ "u25.bin",      0x100000, 0xef652e0c, 3 | BRF_GRA },			 //  5

	{ "u3.bin",       0x040000, 0x0905aeb2, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u33.bin",      0x200000, 0x54494e6b, 4 | BRF_GRA },			 //  7 Tile data

	{ "k3.u71",       0x020000, 0x11994055, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u64.bin",      0x080000, 0xe187ed4f, 5 | BRF_SND },			 //  9 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0x9e07104d, 6 | BRF_SND },			 // 10 YM2610 ADPCM data
};

STD_ROM_PICK(gunbirdk)
STD_ROM_FN(gunbirdk)

struct BurnDriver BurnDrvGunbirdk = {
	"gunbirdk", "gunbird", NULL, NULL, "1994",
	"Gunbird (Korea)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, gunbirdkRomInfo, gunbirdkRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, gunbirdDIPInfo,
	GunbirdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Gunbird (Japan)

static struct BurnRomInfo gunbirdjRomDesc[] = {
	{ "1.u46",        0x040000, 0x474abd69, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2.u39",        0x040000, 0x3e3e661f, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x7d7e8a00, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u24.bin",      0x200000, 0x5e3ffc9d, 3 | BRF_GRA },			 //  3
	{ "u15.bin",      0x200000, 0xa827bfb5, 3 | BRF_GRA },			 //  4
	{ "u25.bin",      0x100000, 0xef652e0c, 3 | BRF_GRA },			 //  5

	{ "u3.bin",       0x040000, 0x0905aeb2, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u33.bin",      0x200000, 0x54494e6b, 4 | BRF_GRA },			 //  7 Tile data

	{ "3.u71",        0x020000, 0x2168e4ba, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u64.bin",      0x080000, 0xe187ed4f, 5 | BRF_SND },			 //  9 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0x9e07104d, 6 | BRF_SND },			 // 10 YM2610 ADPCM data
};

STD_ROM_PICK(gunbirdj)
STD_ROM_FN(gunbirdj)

struct BurnDriver BurnDrvGunbirdj = {
	"gunbirdj", "gunbird", NULL, NULL, "1994",
	"Gunbird (Japan)\0", NULL, "Psikyo", "Psikyo 68EC020",
	L"Gunbird (Japan)\0\u30AC\u30F3\u30D0\u30FC\u30C9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, gunbirdjRomInfo, gunbirdjRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, gunbirdDIPInfo,
	GunbirdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Battle K-Road

static struct BurnRomInfo btlkroadRomDesc[] = {
	{ "4-u46.bin",    0x040000, 0x8a7a28b4, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5-u39.bin",    0x040000, 0x933561fa, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x282d89c3, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u24.bin",      0x200000, 0xbbe9d3d1, 3 | BRF_GRA },			 //  3
	{ "u15.bin",      0x200000, 0xd4d1b07c, 3 | BRF_GRA },			 //  4

	{ "u3.bin",       0x040000, 0x30d541ed, 7 | BRF_GRA },			 //  5 Sprite LUT

	{ "u33.bin",      0x200000, 0x4c8577f1, 4 | BRF_GRA },			 //  6 Tile data

	{ "3-u71.bin",    0x020000, 0x22411fab, 2 | BRF_ESS | BRF_PRG }, //  7 CPU #1 code

	{ "u64.bin",      0x080000, 0x0f33049f, 5 | BRF_SND },			 //  8 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0x51d73682, 6 | BRF_SND },			 //  9 YM2610 ADPCM data
	
	{ "tibpal16l8.u69",    260, 0x00000000, 0 | BRF_NODUMP },		 // NO DUMP
	{ "tibpal16l8.u19",    260, 0x00000000, 0 | BRF_NODUMP },		 // NO DUMP
};

STD_ROM_PICK(btlkroad)
STD_ROM_FN(btlkroad)

struct BurnDriver BurnDrvBtlKRoad = {
	"btlkroad", NULL, NULL, NULL, "1994",
	"Battle K-Road\0", NULL, "Psikyo", "Psikyo 68EC020",
	L"Battle K-Road\0Battle K-Road \u30D0\u30C8\u30EB\u30AF\u30ED\u30FC\u30C9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VSFIGHT, 0,
	NULL, btlkroadRomInfo, btlkroadRomName, NULL, NULL, NULL, NULL, btlkroadInputInfo, btlkroadDIPInfo,
	GunbirdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	320, 224, 4, 3
};


// Battle K-Road (Korea)
// f205v id 1266

static struct BurnRomInfo btlkroadkRomDesc[] = {
	{ "4,dot.u46",    0x040000, 0xe724d429, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5,dot.u39",    0x040000, 0xc0d65765, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u14.bin",      0x200000, 0x282d89c3, 3 | BRF_GRA },		    //  2 Sprite data
	{ "u24.bin",      0x200000, 0xbbe9d3d1, 3 | BRF_GRA },		    //  3
	{ "u15.bin",      0x200000, 0xd4d1b07c, 3 | BRF_GRA },	        //  4

	{ "u3.bin",       0x040000, 0x30d541ed, 7 | BRF_GRA },		    //  5 Sprite LUT

	{ "u33.bin",      0x200000, 0x4c8577f1, 4 | BRF_GRA },		    //  6 Tile data

	{ "3,k.u71",      0x020000, 0xe0f0c597, 2 | BRF_ESS | BRF_PRG }, //  7 CPU #1 code

	{ "u64.bin",      0x080000, 0x0f33049f, 5 | BRF_SND },		    //  8 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0x51d73682, 6 | BRF_SND },		    //  9 YM2610 ADPCM data
	
	{ "tibpal16l8.u69",   260, 0x00000000, 0 | BRF_NODUMP },		// NO DUMP
	{ "tibpal16l8.u19",   260, 0x00000000, 0 | BRF_NODUMP },		// NO DUMP
};

STD_ROM_PICK(btlkroadk)
STD_ROM_FN(btlkroadk)

struct BurnDriver BurnDrvBtlKRoadk = {
	"btlkroadk", "btlkroad", NULL, NULL, "1994",
	"Battle K-Road (Korea)\0", NULL, "Psikyo", "Psikyo 68EC020",
	L"Battle K-Road\0Battle K-Road \u30D0\u30C8\u30EB\u30AF\u30ED\u30FC\u30C9 (Korea)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VSFIGHT, 0,
	NULL, btlkroadkRomInfo, btlkroadkRomName, NULL, NULL, NULL, NULL, btlkroadInputInfo, btlkroadkDIPInfo,
	GunbirdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	320, 224, 4, 3
};


// Strikers 1945 (World)

static struct BurnRomInfo s1945RomDesc[] = {
	{ "2s.u40",       0x040000, 0x9b10062a, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "3s.u41",       0x040000, 0xf87e871a, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u63.bin",    0x020000, 0x42d40ae1, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u61.bin",      0x200000, 0xa839cf47, 6 | BRF_SND },			 //  9 PCM data

	{ "4-u59.bin",    0x001000, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 10 MCU (undumped)
};

STD_ROM_PICK(s1945)
STD_ROM_FN(s1945)

static INT32 S1945Init() // & s1945k
{
	return CommonInit(PSIKYO_HW_S1945, s1945_table, NULL);
}

struct BurnDriver BurnDrvS1945 = {
	"s1945", NULL, NULL, NULL, "1995",
	"Strikers 1945 (World)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945RomInfo, s1945RomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945WorldDIPInfo,
	S1945Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (Japan / World)

static struct BurnRomInfo s1945aRomDesc[] = {
	{ "4-u40.bin",    0x040000, 0x29ffc217, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5-u41.bin",    0x040000, 0xc3d3fb64, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u63.bin",    0x020000, 0x42d40ae1, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u61.bin",      0x200000, 0xa839cf47, 6 | BRF_SND },			 //  9 PCM data
	
	{ "4-u59.bin",    0x001000, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 10 MCU (undumped)
};

STD_ROM_PICK(s1945a)
STD_ROM_FN(s1945a)

static INT32 S1945aInit()
{
	return CommonInit(PSIKYO_HW_S1945, s1945a_table, NULL);
}

struct BurnDriver BurnDrvS1945a = {
	"s1945a", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (Japan / World)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945aRomInfo, s1945aRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945aDIPInfo,
	S1945aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (Japan)

static struct BurnRomInfo s1945jRomDesc[] = {
	{ "1-u40.bin",    0x040000, 0xc00eb012, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2-u41.bin",    0x040000, 0x3f5a134b, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u63.bin",    0x020000, 0x42d40ae1, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u61.bin",      0x200000, 0xa839cf47, 6 | BRF_SND },			 //  9 PCM data

	{ "4-u59.bin",    0x001000, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 10 MCU (undumped)
};

STD_ROM_PICK(s1945j)
STD_ROM_FN(s1945j)

static INT32 S1945jInit()
{
	return CommonInit(PSIKYO_HW_S1945, s1945j_table, NULL);
}

struct BurnDriver BurnDrvS1945j = {
	"s1945j", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (Japan)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945jRomInfo, s1945jRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945DIPInfo,
	S1945jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (World, unprotected)

static struct BurnRomInfo s1945nRomDesc[] = {
	{ "4.u46",    	  0x040000, 0x28fb8181, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "5.u39",    	  0x040000, 0x8ca05f94, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u71.bin",    0x020000, 0xe3e366bd, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u64.bin",      0x080000, 0xa44a4a9b, 5 | BRF_SND },			 //  9 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0xfe1312c2, 6 | BRF_SND },			 // 10 YM2610 ADPCM data
};

STD_ROM_PICK(s1945n)
STD_ROM_FN(s1945n)

static INT32 S1945nInit() // & s1945nj
{
	return CommonInit(PSIKYO_HW_GUNBIRD, NULL, NULL);
}

struct BurnDriver BurnDrvS1945n = {
	"s1945n", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (World, unprotected)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945nRomInfo, s1945nRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945DIPInfo,
	S1945nInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (Japan, unprotected)

static struct BurnRomInfo s1945njRomDesc[] = {
	{ "1-u46.bin",    0x040000, 0x95028132, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "2-u39.bin",    0x040000, 0x3df79a16, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u71.bin",    0x020000, 0xe3e366bd, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u64.bin",      0x080000, 0xa44a4a9b, 5 | BRF_SND },			 //  9 YM2610 ADPCM (delta-t) data

	{ "u56.bin",      0x100000, 0xfe1312c2, 6 | BRF_SND },			 // 10 YM2610 ADPCM data
};

STD_ROM_PICK(s1945nj)
STD_ROM_FN(s1945nj)

struct BurnDriver BurnDrvS1945nj = {
	"s1945nj", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (Japan, unprotected)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945njRomInfo, s1945njRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945DIPInfo,
	S1945nInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (Korea)

static struct BurnRomInfo s1945kRomDesc[] = {
	{ "10.u40",       0x040000, 0x5a32af36, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "9.u41",        0x040000, 0x29cc6d7d, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0x28a27fee, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0xca152a32, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xc5d60ea9, 3 | BRF_GRA },			 //  4
	{ "u23.bin",      0x200000, 0x48710332, 3 | BRF_GRA },			 //  5

	{ "u1.bin",       0x040000, 0xdee22654, 7 | BRF_GRA },			 //  6 Sprite LUT

	{ "u34.bin",      0x200000, 0xaaf83e23, 4 | BRF_GRA },			 //  7 Tile data

	{ "3-u63.bin",    0x020000, 0x42d40ae1, 2 | BRF_ESS | BRF_PRG }, //  8 CPU #1 code

	{ "u61.bin",      0x200000, 0xa839cf47, 6 | BRF_SND },			 //  9 PCM data

	{ "4-u59.bin",    0x001000, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 10 MCU (undumped)
};

STD_ROM_PICK(s1945k)
STD_ROM_FN(s1945k)

struct BurnDriver BurnDrvS1945k = {
	"s1945k", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (Korea)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945kRomInfo, s1945kRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945DIPInfo,
	S1945Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Strikers 1945 (Hong Kong, bootleg)

static struct BurnRomInfo s1945blRomDesc[] = {
	{ "27c010-1",	  0x020000, 0xd3361536, 1 | BRF_PRG | BRF_ESS }, //  0 CPU #0 code
	{ "27c010-2",	  0x020000, 0x1d1916b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "27c010-3",	  0x020000, 0x391e0387, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "27c010-4",	  0x020000, 0x2aebcf6b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rv27c3200.m4", 0x400000, 0x70c8f72e, 3 | BRF_GRA },           //  4 Sprite data
	{ "rv27c3200.m3", 0x400000, 0x0dec2a8d, 3 | BRF_GRA },           //  5

	{ "rv27c1600.m1", 0x200000, 0xaaf83e23, 4 | BRF_GRA },           //  6 Tile data

	{ "rv27c040.m6",  0x080000, 0xc22e5b65, 5 | BRF_SND },           //  7 Samples

	{ "27c010-b",	  0x020000, 0xe38d5ab7, 7 | BRF_GRA },           //  8 Sprite LUT
	{ "27c010-a",	  0x020000, 0xcb8c65ec, 7 | BRF_GRA },           //  9

	{ "27c512",		  0x010000, 0x0da8db45, 0 | BRF_GRA },           // 10 unknown
};

STD_ROM_PICK(s1945bl)
STD_ROM_FN(s1945bl)

static void S1945BlCallback()
{
	// 8bit->32 bit, standard are 16bit->32bit
	BurnLoadRom(Psikyo68KROM + 1, 0, 4);
	BurnLoadRom(Psikyo68KROM + 0, 1, 4);
	BurnLoadRom(Psikyo68KROM + 3, 2, 4);
	BurnLoadRom(Psikyo68KROM + 2, 3, 4);

	// These are interleaved
	BurnLoadRom(PsikyoSpriteLUT + 0, 8, 2);
	BurnLoadRom(PsikyoSpriteLUT + 1, 9, 2);
}

static INT32 S1945blInit()
{
	return CommonInit(PSIKYO_HW_S1945BL, NULL, S1945BlCallback);
}

struct BurnDriverD BurnDrvS1945bl = {
	"s1945bl", "s1945", NULL, NULL, "1995",
	"Strikers 1945 (Hong Kong, bootleg)\0", NULL, "bootleg", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_VERSHOOT, 0,
	NULL, s1945blRomInfo, s1945blRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, s1945DIPInfo, //S1945blInputInfo, S1945blDIPInfo,
	S1945blInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	224, 320, 3, 4
};


// Tengai (World)

static struct BurnRomInfo tengaiRomDesc[] = {
	{ "5-u40.bin",    0x080000, 0x90088195, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "4-u41.bin",    0x080000, 0x0d53196c, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0xed42ef73, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0x8d21caee, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xefe34eed, 3 | BRF_GRA },			 //  4

	{ "u1.bin",       0x040000, 0x681d7d55, 7 | BRF_GRA },			 //  5 Sprite LUT

	{ "u34.bin",      0x400000, 0x2a2e2eeb, 4 | BRF_GRA },			 //  6 Tile data

	{ "1-u63.bin",    0x020000, 0x2025e387, 2 | BRF_ESS | BRF_PRG }, //  7 CPU #1 code

	{ "u61.bin",      0x200000, 0xa63633c5, 6 | BRF_SND },			 //  8 PCM data
	{ "u62.bin",      0x200000, 0x3ad0c357, 6 | BRF_SND },			 //  9
	
	{ "4.u59",    	  0x001000, 0xe563b054, 0 | BRF_ESS | BRF_PRG | BRF_OPT },			 //	10 Mcu
};

STD_ROM_PICK(tengai)
STD_ROM_FN(tengai)

static INT32 TengaiInit() // & Tengaij
{
	return CommonInit(PSIKYO_HW_TENGAI, s1945_table, NULL);
}

struct BurnDriver BurnDrvTengai = {
	"tengai", NULL, NULL, NULL, "1996",
	"Tengai (World)\0", NULL, "Psikyo", "Psikyo 68EC020",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_HORSHOOT, 0,
	NULL, tengaiRomInfo, tengaiRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, tengaiDIPInfo,
	TengaiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	320, 224, 4, 3
};


// Sengoku Blade: Sengoku Ace Episode II (Japan) / Tengai (World)

static struct BurnRomInfo tengaijRomDesc[] = {
	{ "2-u40.bin",    0x080000, 0xab6fe58a, 1 | BRF_ESS | BRF_PRG }, //  0 CPU #0 code
	{ "3-u41.bin",    0x080000, 0x02e42e39, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "u20.bin",      0x200000, 0xed42ef73, 3 | BRF_GRA },			 //  2 Sprite data
	{ "u22.bin",      0x200000, 0x8d21caee, 3 | BRF_GRA },			 //  3
	{ "u21.bin",      0x200000, 0xefe34eed, 3 | BRF_GRA },			 //  4

	{ "u1.bin",       0x040000, 0x681d7d55, 7 | BRF_GRA },			 //  5 Sprite LUT

	{ "u34.bin",      0x400000, 0x2a2e2eeb, 4 | BRF_GRA },			 //  6 Tile data

	{ "1-u63.bin",    0x020000, 0x2025e387, 2 | BRF_ESS | BRF_PRG }, //  7 CPU #1 code

	{ "u61.bin",      0x200000, 0xa63633c5, 6 | BRF_SND },			 //  8 PCM data
	{ "u62.bin",      0x200000, 0x3ad0c357, 6 | BRF_SND },			 //  9
	
	{ "4.u59",    	  0x001000, 0xe563b054, 0 | BRF_ESS | BRF_PRG | BRF_OPT },			 //	10 Mcu
};

STD_ROM_PICK(tengaij)
STD_ROM_FN(tengaij)

struct BurnDriver BurnDrvTengaij = {
	"tengaij", "tengai", NULL, NULL, "1996",
	"Sengoku Blade: Sengoku Ace Episode II (Japan) / Tengai (World)\0", NULL, "Psikyo", "Psikyo 68EC020",
	L"\u6226\u56FD\u30D6\u30EC\u30FC\u30C9 - sengoku Ace episode II (Japan)\0Tengai (World)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PSIKYO, GBF_HORSHOOT, 0,
	NULL, tengaijRomInfo, tengaijRomName, NULL, NULL, NULL, NULL, gunbirdInputInfo, tengaijDIPInfo,
	TengaiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &PsikyoRecalcPalette, 0x1000,
	320, 224, 4, 3
};
