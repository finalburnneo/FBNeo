// FB Neo - Tehkan Wolrd Cup '90 driver
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "msm5205.h"
#include "dtimer.h"
#include "burn_gun.h"
#include "burnint.h"
#include "timer.h"

// CPU clock definitions
#define MAIN_CPU_CLOCK    (18432000 / 4)
#define SUB_CPU_CLOCK     (18432000 / 4)
#define AUDIO_CPU_CLOCK   (18432000 / 4)
#define AY_CLOCK          (18432000 / 12)
#define MSM5205_CLOCK     384000

// CPU Ids
#define CPU_MAIN 0
#define CPU_GRAPHICS 1
#define CPU_SUB 1
#define CPU_SOUND 2

static UINT8 TWCInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 TWCDip[3]        = {0, 0, 0};
static UINT8 TWCInput[3]      = {0x00, 0x00, 0x00};
static INT32 TballPrev[4];
static INT16 TWCAnalog[4]     = {0, 0, 0, 0};
static INT32 last_track[4];
static UINT8 TWCReset         = 0;
static HoldCoin<2> hold_coin;

static UINT8 *Mem              = NULL;
static UINT8 *MemEnd           = NULL;
static UINT8 *RamStart         = NULL;
static UINT8 *RamEnd           = NULL;
static UINT8 *TWCZ80Rom1      = NULL;
static UINT8 *TWCZ80Rom2      = NULL;
static UINT8 *TWCZ80Rom3      = NULL;
static UINT8 *TWCSndRom       = NULL;
static UINT8 *TWCZ80Ram1      = NULL;
static UINT8 *TWCZ80Ram2      = NULL;
static UINT8 *TWCZ80Ram3      = NULL;
static UINT8 *TWCTextVideoRam = NULL;
static UINT8 *TWCBgVideoRam    = NULL;
static UINT8 *TWCColorRam     = NULL;
static UINT8 *TWCSpriteRam    = NULL;
static UINT8 *TWCPalRam       = NULL;
static UINT8 *TWCPalRam2      = NULL;
static UINT8 *TWCSharedRam    = NULL;
static UINT32 *TWCPalette     = NULL;
static UINT8 *TWCCharTiles    = NULL;
static UINT8 *TWCBgTiles      = NULL;
static UINT8 *TWCSprites      = NULL;
static UINT8 *TWCTempGfx      = NULL;

static UINT8 TWCScrollXHi;
static UINT8 TWCScrollXLo;
static UINT8 TWCScrollY;

static UINT8 TWCSoundLatch         = 0;
static UINT8 TWCSoundLatch2         = 0;
static int   msm_data_offs    = 0;
static INT32 msm_toggle       = 0;
static INT32 TWCMSM5205Next;

static INT32 nCyclesDone[3], nCyclesTotal[3];
static INT32 nCyclesSegment;

static UINT8 TWCFlipScreenX;
static UINT8 TWCFlipScreenY;

static INT32 TWCWatchdog;

static dtimer sndtimer;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TWCInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TWCInputPort2 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	TWCInputPort2 + 1,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TWCInputPort2 + 2,	"p1 start"	},
	{"P2 Start",		BIT_DIGITAL,	TWCInputPort2 + 3,	"p2 start"	},

	{"P1 Button",		BIT_DIGITAL,	TWCInputPort0 + 5,	"p1 shot"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &TWCAnalog[0],     "p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &TWCAnalog[1],     "p1 y-axis"),

	{"P2 Button",		BIT_DIGITAL,	TWCInputPort1 + 5,	"p2 shot"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &TWCAnalog[2],     "p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &TWCAnalog[3],     "p2 y-axis"),

	{"Reset",			BIT_DIGITAL,	&TWCReset,		    "reset"		},
	{"Dip A",			BIT_DIPSWITCH,	TWCDip + 0,	    "dip"		},
	{"Dip B",			BIT_DIPSWITCH,	TWCDip + 1,	    "dip"		},
	{"Dip C",			BIT_DIPSWITCH,	TWCDip + 1,	    "dip"		},
};
#undef A
STDINPUTINFO(TWC)

inline static void TWCMakeInputs()
{
	TWCInput[0] = TWCInput[1] = 0x00;
	TWCInput[2] = 0x03;

	for (INT32 i = 0; i < 8; i++) {
		TWCInput[0] ^= (TWCInputPort0[i] & 1) << i;
		TWCInput[1] ^= (TWCInputPort1[i] & 1) << i;
		TWCInput[2] ^= (TWCInputPort2[i] & 1) << i;
	}

	hold_coin.checklow(0, TWCInput[2], 1<<0, 1);
	hold_coin.checklow(1, TWCInput[2], 1<<1, 1);

	// device, portA_reverse?, portB_reverse?
	BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
	BurnTrackballFrame(0, TWCAnalog[0], TWCAnalog[1], 0x02, 0x0f);  // 0x02, 0x0f taken from konami/d_bladestl.cpp
	BurnTrackballUpdate(0);

	BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
	BurnTrackballFrame(1, TWCAnalog[2], TWCAnalog[3], 0x02, 0x0f);
	BurnTrackballUpdate(1);

}


static struct BurnDIPInfo TWCDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0xff, NULL                    },
	{0x12, 0xff, 0xff, 0xff, NULL                    },
	{0x13, 0xff, 0xff, 0xff, NULL                    },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Difficulty"            },
	{0x11, 0x01, 0x03, 0x02, "Easy"                  },
	{0x11, 0x01, 0x03, 0x03, "Normal"                },
	{0x11, 0x01, 0x03, 0x01, "Hard"                  },
	{0x11, 0x01, 0x03, 0x00, "Very Hard"             },

	{0   , 0xfe, 0   , 2   , "Timer Speed"           },
	{0x11, 0x01, 0x04, 0x04, "60/60"                 },
	{0x11, 0x01, 0x04, 0x00, "55/60"                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"           },
	{0x11, 0x01, 0x08, 0x00, "Off"                   },
	{0x11, 0x01, 0x08, 0x08, "On"                    },

	// Dip 2
	{0   , 0xfe, 0   , 8  , "Coin A"                      },
	{0x12, 0x01, 0x07, 0x01, " 2 Coins 1 Credit"          },
	{0x12, 0x01, 0x07, 0x07, " 1 Coin  1 Credit"          },
	{0x12, 0x01, 0x07, 0x00, " 2 Coins 3 Credits"         },
	{0x12, 0x01, 0x07, 0x06, " 1 Coin  2 Credits"         },
	{0x12, 0x01, 0x07, 0x05, " 1 Coin  3 Credits"         },
	{0x12, 0x01, 0x07, 0x04, " 1 Coin  4 Credits"         },
	{0x12, 0x01, 0x07, 0x03, " 1 Coin  5 Credits"         },
	{0x12, 0x01, 0x07, 0x02, " 1 Coin  6 Credits"         },

	{0   , 0xfe, 0   , 8  , "Coin B"                      },
	{0x12, 0x01, 0x38, 0x08, " 2 Coins 1 Credit"          },
	{0x12, 0x01, 0x38, 0x38, " 1 Coin  1 Credit"          },
	{0x12, 0x01, 0x38, 0x00, " 2 Coins 3 Credits"         },
	{0x12, 0x01, 0x38, 0x30, " 1 Coin  2 Credits"         },
	{0x12, 0x01, 0x38, 0x28, " 1 Coin  3 Credits"         },
	{0x12, 0x01, 0x38, 0x20, " 1 Coin  4 Credits"         },
	{0x12, 0x01, 0x38, 0x18, " 1 Coin  5 Credits"         },
	{0x12, 0x01, 0x38, 0x10, " 1 Coin  6 Credits"         },

	{0   , 0xfe, 0   , 4  , "Start Credits (P1&P2)/Extra" },
	{0x12, 0x01, 0xc0, 0x80, " 1&1/200%"                  },
	{0x12, 0x01, 0xc0, 0xc0, " 1&2/100%"                  },
	{0x12, 0x01, 0xc0, 0x40, " 2&2/100%"                  },
	{0x12, 0x01, 0xc0, 0x00, " 2&3/67%"                   },

	// Dip 3
	{0   , 0xfe, 0   , 4  , "1P Game Time"                },
	{0x13, 0x01, 0x03, 0x00, " 2:30"                      },
	{0x13, 0x01, 0x03, 0x01, " 2:00"                      },
	{0x13, 0x01, 0x03, 0x03, " 1:30"                      },
	{0x13, 0x01, 0x03, 0x02, " 1:00"                      },

	{0   , 0xfe, 0   , 32  , "2P Game Time"               },
	{0x13, 0x01, 0x7c, 0x00, " 5:00/3:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x60, " 5:00/2:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x20, " 5:00/2:35 Extra"           },
	{0x13, 0x01, 0x7c, 0x40, " 5:00/2:30 Extra"           },
	{0x13, 0x01, 0x7c, 0x04, " 4:00/2:30 Extra"           },
	{0x13, 0x01, 0x7c, 0x64, " 4:00/2:15 Extra"           },
	{0x13, 0x01, 0x7c, 0x24, " 4:00/2:05 Extra"           },
	{0x13, 0x01, 0x7c, 0x44, " 4:00/2:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x1c, " 3:30/2:15 Extra"           },
	{0x13, 0x01, 0x7c, 0x7c, " 3:30/2:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x3c, " 3:30/1:50 Extra"           },
	{0x13, 0x01, 0x7c, 0x5c, " 3:30/1:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x08, " 3:00/2:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x68, " 3:00/1:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x28, " 3:00/1:35 Extra"           },
	{0x13, 0x01, 0x7c, 0x48, " 3:00/1:30 Extra"           },
	{0x13, 0x01, 0x7c, 0x0c, " 2:30/1:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x6c, " 2:30/1:30 Extra"           },
	{0x13, 0x01, 0x7c, 0x2c, " 2:30/1:20 Extra"           },
	{0x13, 0x01, 0x7c, 0x4c, " 2:30/1:15 Extra"           },
	{0x13, 0x01, 0x7c, 0x10, " 2:00/1:30 Extra"           },
	{0x13, 0x01, 0x7c, 0x70, " 2:00/1:15 Extra"           },
	{0x13, 0x01, 0x7c, 0x30, " 2:00/1:05 Extra"           },
	{0x13, 0x01, 0x7c, 0x50, " 2:00/1:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x14, " 1:30/1:15 Extra"           },
	{0x13, 0x01, 0x7c, 0x74, " 1:30/1:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x34, " 1:30/0:50 Extra"           },
	{0x13, 0x01, 0x7c, 0x54, " 1:30/0:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x18, " 1:00/1:00 Extra"           },
	{0x13, 0x01, 0x7c, 0x78, " 1:00/0:45 Extra"           },
	{0x13, 0x01, 0x7c, 0x38, " 1:00/0:35 Extra"           },
	{0x13, 0x01, 0x7c, 0x58, " 1:00/0:30 Extra"           },

	{0   , 0xfe, 0   , 2   , "Game Type"                  },
	{0x13, 0x01, 0x80, 0x80, " Timer In"                  },
	{0x13, 0x01, 0x80, 0x00, " Credit In"                 },
};

STDDIPINFO(TWC)

static void __fastcall TWCSoundWritePort(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write(0, port & 3, data);
			return;
	}
}

static UINT8 __fastcall TWCSoundReadPort(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x02:
			return AY8910Read(0);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	TWCZ80Rom1            = Next; Next += 0x0c000;
	TWCZ80Rom2            = Next; Next += 0x08000;
	TWCZ80Rom3            = Next; Next += 0x04000;
	TWCSndRom             = Next; Next += 0x04000;

	RamStart               = Next;

	TWCZ80Ram1            = Next; Next += 0x00800;
	TWCZ80Ram2            = Next; Next += 0x04800;
	TWCZ80Ram3            = Next; Next += 0x00800;

	TWCSharedRam          = Next; Next += 0x00800;

	TWCTextVideoRam           = Next; Next += 0x00400;
	TWCColorRam           = Next; Next += 0x00400;
	TWCPalRam             = Next; Next += 0x00600;
	TWCPalRam2            = Next; Next += 0x00200;
	TWCBgVideoRam          = Next; Next += 0x00800;
	TWCSpriteRam          = Next; Next += 0x00400;

	RamEnd                     = Next;

	// Char Layout: 512 chars, 8x8 pixels, 4 bits/pixel
	TWCCharTiles          = Next; Next += (512 * 8 * 8 * 4);
	// Tile Layout: 1024 tiles, 16x8 pixels, 4 bits/pixel
	TWCBgTiles            = Next; Next += (1024 * 16 * 8 * 4);
	// Sprite Layout: 512 sprites, 16x16 pixels, 4 bits/pixel
	TWCSprites            = Next; Next += (512 * 16 * 16 * 4);

	// Palette format: xBGR_444 (xxxxBBBBGGGGRRRR), 768
	TWCPalette            = (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

// Taken from d_TWC.cpp
inline static UINT32 xBGR_444_CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	// xBGR_444, xxxxBBBBGGGGRRRR
	b = (nColour >> 8) & 0x0f;
	g = (nColour >> 4) & 0x0f;
	r = (nColour >> 0) & 0x0f;

	b = (b << 4) | b;
	g = (g << 4) | g;
	r = (r << 4) | r;

	return BurnHighCol(r, g, b, 0);
}

static INT32 TWCCalcPalette()
{
	INT32 i;

	for (i = 0; i < 0x600; i++) {
		TWCPalette[i / 2] = xBGR_444_CalcCol(
									TWCPalRam[i | 1] |
		 							(TWCPalRam[i & ~1] << 8)
								);
	}

	return 0;
}


static UINT8 trackball_read(UINT16 offset)
{
	return (BurnTrackballRead(offset>>1, offset&1) - TballPrev[offset]) & 0xff;
}


static UINT8 __fastcall TWCMainRead(UINT16 address)
{
	switch (address) {
		case 0xda00:
			return 0; // teedoff_unk_r

		case 0xf800:
		case 0xf801:
			return trackball_read(address & 1);

		case 0xf802:
		case 0xf806:
			return 0xff - TWCInput[2];  // System

		case 0xf810:
		case 0xf811:
			return trackball_read(2 + (address & 1));

		case 0xf803:
			return 0x20 - TWCInput[0];  // Player 1

		case 0xf813:
			return 0x20 - TWCInput[1];  // Player 2

		case 0xf820:
			return TWCSoundLatch2;

		case 0xf840:
			return TWCDip[1];	// DSW2

		case 0xf860:
			TWCWatchdog = 0;
			return;

		case 0xf850:
			return TWCDip[2];	// DSW3

		case 0xf870:
			return TWCDip[0];	//DSW1
	}

	return 0;
}


static void trackball_reset(UINT16 offset, UINT8 data)
{
	TballPrev[offset] = (BurnTrackballRead(offset>>1, offset&1) + data) & 0xff;
}

static void sound_sync()
{
	// Both CPUs are same model with same frequency
	INT32 cyc = ZetTotalCycles(CPU_MAIN) - ZetTotalCycles(CPU_SOUND);
	if (cyc > 0) {
		BurnTimerUpdate(ZetTotalCycles() + cyc);
	}
}


static void __fastcall TWCMainWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		// 0xc000 .. 0xcfff: Shared RAM
		// 0xd000 .. 0xd3ff: TextVideoRAM
		// 0xd400 .. 0xd7ff: ColorRAM
		// 0xd800 .. 0xddff: PaletteRAM
		// 0xde00 .. 0xdfff: Shared RAM (PaletteRAM2)
		// 0xe000 .. 0xe7ff: BGVideoRAM
		// 0xe800 .. 0xebff: SpriteRAM
		case 0xec00:
			TWCScrollXHi = data;
			return;

		case 0xec01:
			TWCScrollXLo = data;
			return;

		case 0xec02:
			TWCScrollY = data;
			return;

		case 0xf800:
		case 0xf801:
			trackball_reset(address & 1, data);
			return;

		case 0xf802:
			return;     // placeholder for future adding of gridiron_led0

		case 0xf810:
		case 0xf811:
			trackball_reset(2 + (address & 1), data);
			return;

		case 0xf812:
			return;     // placeholder for future adding of gridiron_led1

		case 0xf820:
			sound_sync();
			TWCSoundLatch = data;
			ZetNmi(CPU_SOUND);
			return;

		case 0xf840:
			ZetSetRESETLine(CPU_GRAPHICS, data ? Z80_CLEAR_LINE : Z80_ASSERT_LINE);
			return;

		case 0xf850:
			ZetSetRESETLine(CPU_SOUND, data ? Z80_CLEAR_LINE : Z80_ASSERT_LINE);
			MSM5205ResetWrite(0, data);
			return;

		case 0xf860:
			TWCFlipScreenX = data & 0x40;
			// updateFlip() ??
			return;

		case 0xf870:
			TWCFlipScreenY = data & 0x40;
			// updateFlip() ??
			return;
	}
}

static UINT8 __fastcall TWCSubRead(UINT16 address)
{
	switch (address) {
		case 0xf860:
			TWCWatchdog = 0;
			return;
	}

	return 0;
}

static void __fastcall TWCSubWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xec00:
			TWCScrollXHi = data;
			return;

		case 0xec01:
			TWCScrollXLo = data;
			return;

		case 0xec02:
			TWCScrollY = data;
			return;
	}
}

static UINT8 __fastcall TWCSoundRead(UINT16 address)
{
	switch (address) {
		case 0xc000:
			return TWCSoundLatch;
	}

	return 0;
}

static void __fastcall TWCSoundWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x8001:
			MSM5205ResetWrite(0, data);
			return;

		case 0xc000:
			TWCSoundLatch2 = data;
			return;
	}
}

static INT32 TWCFrame()
{
	TWCWatchdog++;
    if (TWCReset || TWCWatchdog >= 180) TWCDoReset(); // Handle reset

    TWCMakeInputs(); // Update inputs

    ZetNewFrame(); // Reset CPU cycle counters

	// Number of interrupt slices per frame
    INT32 nInterleave = MSM5205CalcInterleave(0, AUDIO_CPU_CLOCK);

    // Total cycles each CPU should run per frame
    INT32 nCyclesTotal[3] = {
        (INT32)((INT64)18432000 / 4 / 59.17), // Main CPU
        (INT32)((INT64)18432000 / 4 / 59.17), // Sub CPU
        (INT32)((INT64)18432000 / 4 / 59.17)  // Audio CPU
    };

    INT32 nCyclesDone[3] = { 0, 0, 0 }; // Cycles executed so far


    // Run CPUs in sync
    for (INT32 i = 0; i < nInterleave; i++) {
        // Main CPU
        ZetOpen(CPU_MAIN);
        CPU_RUN(CPU_MAIN, Zet); // Run the main CPU
        ZetClose();

        // Sub CPU
        ZetOpen(CPU_GRAPHICS);
        CPU_RUN(CPU_GRAPHICS, Zet); // Run the sub CPU
        ZetClose();

        // Audio CPU
        ZetOpen(CPU_SOUND);
        CPU_RUN_TIMER(CPU_SOUND); // Run the audio CPU (with timer synchronization)
        ZetClose();
    }

	// Trigger interrupt at end of frame
	ZetSetIRQLine(CPU_MAIN, 0, CPU_IRQSTATUS_HOLD);
	ZetSetIRQLine(CPU_GRAPHICS, 0, CPU_IRQSTATUS_HOLD);

    // Update sound
	if (pBurnSoundOut) {
		// Update AY-8910 sound chip
		AY8910Render(pBurnSoundOut, nBurnSoundLen);

		// Update MSM5205 sound chip
		MSM5205Update();
	}

	// Render frame
    if (pBurnDraw) BurnDrvRedraw();

    return 0;
}


static INT32 CharPlaneOffsets[4]   = { 0, 1, 2, 3 };
static INT32 CharXOffsets[8]       = { 4, 0, 12, 8, 20, 16, 28, 24 };
static INT32 CharYOffsets[8]       = { 0, 32, 64, 96, 128, 160, 192, 224 };

static INT32 SpritePlaneOffsets[4] = { 0, 1, 2, 3 };
static INT32 SpriteXOffsets[16]    = {     4,     0,     12,     8,     20,     16,     28,     24,
                                       256+4, 256+0, 256+12, 256+8, 256+20, 256+16, 256+28, 256+24 };
static INT32 SpriteYOffsets[16]    = {     0,     32,     64,     96,     128,     160,     192,     224,
                                       512+0, 512+32, 512+64, 512+96, 512+128, 512+160, 512+192, 512+224 };

static INT32 TilePlaneOffsets[4]   = { 0, 1, 2, 3 };
static INT32 TileXOffsets[16]      = {     4,     0,     12,     8,     20,     16,     28,     24,
                                       256+4, 256+0, 256+12, 256+8, 256+20, 256+16, 256+28, 256+24 };
static INT32 TileYOffsets[8]       = { 0, 32, 64, 96, 128, 160, 192, 224 };

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate * 12 / 18432000;
}

void msm_irq_cb(int param)
{
	ZetSetIRQLine(CPU_SOUND, CPU_IRQSTATUS_HOLD);
}

static UINT8 portA_r(UINT32)
{
	return msm_data_offs & 0xff;
}

static UINT8 portB_r(UINT32)
{
	return (msm_data_offs >> 8) & 0xff;
}

static void portA_w(UINT32, UINT32 data)
{
	msm_data_offs = (msm_data_offs & 0xff00) | data;
}

static void portB_w(UINT32, UINT32 data)
{
	msm_data_offs = (msm_data_offs & 0x00ff) | (data << 8);
}


static void adpcm_int()
{	
	UINT8 msm_data = TWCSndRom[msm_data_offs & 0x7fff];

	
	if (msm_toggle == 0)
		MSM5205DataWrite(0, (msm_data >> 4) & 0x0f);
	else
	{
		MSM5205DataWrite(0, msm_data & 0x0f);
		msm_data_offs++;
	}

	msm_toggle ^= 1;
}

static INT32 readbitX(const UINT8 *src, INT32 bitnum, INT32 &ssize)
{
	if ( (bitnum / 8) > ssize) ssize = (bitnum / 8);

	return src[bitnum / 8] & (0x80 >> (bitnum % 8));
}

/*
In the GfxDecodeX function, the modulo argument is used to calculate the offset for accessing
the correct portion of the source data (pSrc) for each sprite or tile (c). Specifically,
it determines how much to skip in the source data when moving from one sprite/tile to the next.
*/
static void GfxDecodeX(INT32 num, INT32 numPlanes, INT32 xSize, INT32 ySize, INT32 planeoffsets[], INT32 xoffsets[], INT32 yoffsets[], INT32 modulo, UINT8 *pSrc, UINT8 *pDest)
{
	INT32 c;

	INT32 src_len = 0;
	INT32 dst_len = 0;

	for (c = 0; c < num; c++) {
		INT32 plane, x, y;

		UINT8 *dp = pDest + (c * xSize * ySize);
		memset(dp, 0, xSize * ySize);

		if ( (c * xSize + ySize) > dst_len ) dst_len = (c * xSize + ySize);

		for (plane = 0; plane < numPlanes; plane++) {
			INT32 planebit = 1 << (numPlanes - 1 - plane);
			INT32 planeoffs = (c * modulo) + planeoffsets[plane];

			for (y = 0; y < ySize; y++) {
				INT32 yoffs = planeoffs + yoffsets[y];
				dp = pDest + (c * xSize * ySize) + (y * xSize);
				if ( (c * xSize * ySize) + (y * xSize) > dst_len ) dst_len = (c * xSize * ySize) + (y * xSize);

				for (x = 0; x < xSize; x++) {
					if (readbitX(pSrc, yoffs + xoffsets[x], src_len)) dp[x] |= planebit;
				}
			}
		}
	}

	bprintf(0, _T("gfxdecode  src / dst size:  %x   %x\n"), src_len, dst_len);

}


static INT32 TWCInit()
{
	INT32 nRet = 0, nLen;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;

	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

	memset(Mem, 0, nLen);
	MemIndex();

	TWCTempGfx = (UINT8*)BurnMalloc(0x10000);
	if (TWCTempGfx == NULL) return 1;

	nRet = BurnLoadRom(TWCZ80Rom1 + 0x00000,  0,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x04000,  1,  1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCZ80Rom1 + 0x04000,  2,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom2 + 0x00000,  3,  1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(TWCZ80Rom3 + 0x00000,  4,  1); if (nRet != 0) return 1;

	memset(TWCTempGfx, 0, 0x4000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  5, 1); if (nRet != 0) return 1;
	GfxDecodeX(512, 4, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, TWCTempGfx, TWCCharTiles);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  7, 1); if (nRet != 0) return 1;
	GfxDecodeX(512, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, TWCTempGfx, TWCSprites);

	memset(TWCTempGfx, 0, 0x10000);
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(TWCTempGfx + 0x08000,  9, 1); if (nRet != 0) return 1;
	GfxDecodeX(1024, 4, 16, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, TWCTempGfx, TWCBgTiles);

	BurnFree(TWCTempGfx);

	nRet = BurnLoadRom(TWCSndRom, 10, 1); if (nRet !=0) return 1;

	// Main CPU
	ZetInit(CPU_MAIN);
	ZetOpen(CPU_MAIN);
	ZetSetReadHandler(TWCMainRead);
	ZetSetWriteHandler(TWCMainWrite);
	ZetMapArea(0x0000, 0xbfff, 0, TWCZ80Rom1         );
	ZetMapArea(0x0000, 0xbfff, 2, TWCZ80Rom1         );
	ZetMapArea(0xc000, 0xc7ff, 0, TWCZ80Ram1         );
	ZetMapArea(0xc000, 0xc7ff, 1, TWCZ80Ram1         );
	ZetMapArea(0xc000, 0xc7ff, 2, TWCZ80Ram1         );
	ZetMapArea(0xc800, 0xcfff, 0, TWCSharedRam       );
	ZetMapArea(0xc800, 0xcfff, 1, TWCSharedRam       );
	ZetMapArea(0xc800, 0xcfff, 2, TWCSharedRam       );
	ZetMapArea(0xd000, 0xd3ff, 0, TWCTextVideoRam        );
	ZetMapArea(0xd000, 0xd3ff, 1, TWCTextVideoRam        );
	ZetMapArea(0xd000, 0xd3ff, 2, TWCTextVideoRam        );
	ZetMapArea(0xd400, 0xd7ff, 0, TWCColorRam        );
	ZetMapArea(0xd400, 0xd7ff, 1, TWCColorRam        );
	ZetMapArea(0xd400, 0xd7ff, 2, TWCColorRam        );
	ZetMapArea(0xd800, 0xddff, 0, TWCPalRam          );
	ZetMapArea(0xd800, 0xddff, 1, TWCPalRam          );
	ZetMapArea(0xd800, 0xddff, 2, TWCPalRam          );
	ZetMapArea(0xde00, 0xdfff, 0, TWCPalRam2         );
	ZetMapArea(0xde00, 0xdfff, 1, TWCPalRam2         );
	ZetMapArea(0xde00, 0xdfff, 2, TWCPalRam2         );
	ZetMapArea(0xe000, 0xe7ff, 0, TWCBgVideoRam       );
	ZetMapArea(0xe000, 0xe7ff, 1, TWCBgVideoRam       );
	ZetMapArea(0xe000, 0xe7ff, 2, TWCBgVideoRam       );
	ZetMapArea(0xe800, 0xebff, 0, TWCSpriteRam       );
	ZetMapArea(0xe800, 0xebff, 1, TWCSpriteRam       );
	ZetMapArea(0xe800, 0xebff, 2, TWCSpriteRam       );
	ZetClose();

	// Graphics "sub" CPU
	ZetInit(CPU_GRAPHICS);
	ZetOpen(CPU_GRAPHICS);
	ZetSetReadHandler(TWCSubRead);
	ZetSetWriteHandler(TWCSubWrite);
	ZetMapArea(0x0000, 0x7fff, 0, TWCZ80Rom2         );
	ZetMapArea(0x0000, 0x7fff, 1, TWCZ80Rom2         );
	ZetMapArea(0x8000, 0xc7ff, 0, TWCZ80Ram2         );
	ZetMapArea(0x8000, 0xc7ff, 1, TWCZ80Ram2         );
	ZetMapArea(0x8000, 0xc7ff, 2, TWCZ80Ram2         );
	ZetMapArea(0xc800, 0xcfff, 0, TWCSharedRam       );
	ZetMapArea(0xc800, 0xcfff, 1, TWCSharedRam       );
	ZetMapArea(0xc800, 0xcfff, 2, TWCSharedRam       );
	ZetMapArea(0xd000, 0xd3ff, 0, TWCTextVideoRam        );
	ZetMapArea(0xd000, 0xd3ff, 1, TWCTextVideoRam        );
	ZetMapArea(0xd000, 0xd3ff, 2, TWCTextVideoRam        );
	ZetMapArea(0xd400, 0xd7ff, 0, TWCColorRam        );
	ZetMapArea(0xd400, 0xd7ff, 1, TWCColorRam        );
	ZetMapArea(0xd400, 0xd7ff, 2, TWCColorRam        );
	ZetMapArea(0xd800, 0xddff, 0, TWCPalRam          );
	ZetMapArea(0xd800, 0xddff, 1, TWCPalRam          );
	ZetMapArea(0xd800, 0xddff, 2, TWCPalRam          );
	ZetMapArea(0xde00, 0xdfff, 0, TWCPalRam2         );
	ZetMapArea(0xde00, 0xdfff, 1, TWCPalRam2         );
	ZetMapArea(0xde00, 0xdfff, 2, TWCPalRam2         );
	ZetMapArea(0xe000, 0xe7ff, 0, TWCBgVideoRam       );
	ZetMapArea(0xe000, 0xe7ff, 1, TWCBgVideoRam       );
	ZetMapArea(0xe000, 0xe7ff, 2, TWCBgVideoRam       );
	ZetMapArea(0xe800, 0xebff, 0, TWCSpriteRam       );
	ZetMapArea(0xe800, 0xebff, 1, TWCSpriteRam       );
	ZetMapArea(0xe800, 0xebff, 2, TWCSpriteRam       );
	ZetClose();


	// Sound CPU
	ZetInit(CPU_SOUND);
	ZetOpen(CPU_SOUND);
	ZetSetReadHandler(TWCSoundRead);
	ZetSetWriteHandler(TWCSoundWrite);
	ZetSetOutHandler(TWCSoundWritePort);
	ZetSetInHandler(TWCSoundReadPort);
	ZetMapArea(0x0000, 0x3fff, 0, TWCZ80Rom3);
	ZetMapArea(0x0000, 0x3fff, 1, TWCZ80Rom3);
	ZetMapArea(0x4000, 0x47ff, 0, TWCZ80Ram3);
	ZetMapArea(0x4000, 0x47ff, 1, TWCZ80Ram3);
	ZetMapArea(0x4000, 0x47ff, 2, TWCZ80Ram3);
	ZetClose();

	GenericTilesInit();

	BurnSetRefreshRate(59.17);

    // Watchdog timer (not directly supported in FBNeo, but can be emulated if needed)
    // WATCHDOG_TIMER(config, "watchdog");

	AY8910Init(0, AUDIO_CPU_CLOCK, 0);
	AY8910Init(1, AUDIO_CPU_CLOCK, 1);
	AY8910SetPorts(0, NULL, NULL, &portA_w, &portB_w);
	AY8910SetPorts(1, &portA_r, &portB_r, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH); // Plane Noise/Bass/Shot
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH); // Whistle/Snare
	AY8910SetBuffered(ZetTotalCycles, AUDIO_CPU_CLOCK);

	timerInit();
	timerAdd(sndtimer, 0, msm_irq_cb);
	sndtimer.start(3579545 / 8000, -1, 1, 1);

	MSM5205Init(0, DrvSynchroniseStream, 384000, adpcm_int, MSM5205_S48_4B, 0);
	MSM5205SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);

	// Initialize analog controls for player 1 and player 2
	BurnGunInit(2, true);

	TWCDoReset();

	return 0;
}


static INT32 TWCExit()
{
	ZetExit();
	GenericTilesExit();

	BurnFree(Mem);
	
	TWCScrollXLo = 0;
	TWCScrollXHi = 0;
	TWCScrollY = 0;
	TWCSoundLatch  = 0;
	TWCSoundLatch2 = 0;

	return 0;
}

static INT32 TWCScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029721;
	}



}

static struct BurnRomInfo TWCRomDesc[] = {
	{ "twc-1.bin",      0x04000, 0x34d6d5ff, BRF_ESS | BRF_PRG }, //  0	Z80 #1 Program Code
	{ "twc-2.bin",      0x04000, 0x7017a221, BRF_ESS | BRF_PRG }, //  1	Z80 #1 Program Code
	{ "twc-3.bin",      0x04000, 0x8b662902, BRF_ESS | BRF_PRG }, //  2	Z80 #1 Program Code

	{ "twc-4.bin",      0x08000, 0x70a9f883, BRF_ESS | BRF_PRG }, //  3	Z80 #2 Program Code

	{ "twc-6.bin",      0x04000, 0xe3112be2, BRF_ESS | BRF_PRG }, //  4	Z80 #3 Program Code

	{ "twc-12.bin",		0x04000, 0xa9e274f8, BRF_GRA },           //  5 Fg Tiles

	{ "twc-8.bin",		0x08000, 0x055a5264, BRF_GRA },           //  6 Sprites
	{ "twc-7.bin",		0x08000, 0x59faebe7, BRF_GRA },           //  7 Sprites

	{ "twc-11.bin",		0x08000, 0x669389fc, BRF_GRA },           //  8 Bg Tiles
	{ "twc-9.bin",		0x08000, 0x347ef108, BRF_GRA },           //  9 Bg Tiles

	{ "twc-5.bin",		0x04000, 0x444b5544, BRF_SND },           //  10 ADPCM Samples
};

STD_ROM_PICK(TWC)
STD_ROM_FN(TWC)

static INT32 TWCDoReset()
{
	TWCScrollXHi = 0;
	TWCScrollXLo = 0;
	TWCScrollY = 0;

	TWCSoundLatch = 0;
	TWCSoundLatch2 = 0;

	TWCWatchdog = 0;

	ZetReset(0);
	ZetReset(1);

	ZetOpen(2);
	ZetReset();
	ZetClose();

	hold_coin.reset();

	HiscoreReset();

	return 0;
}


struct BurnDriver BurnDrvTWC = {
	"TWC",				// The filename of the zip file (without extension)
	NULL,					// The filename of the parent (without extension, NULL if not applicable)
	NULL,					// The filename of the board ROMs (without extension, NULL if not applicable)
	NULL,					// The filename of the samples zip file (without extension, NULL if not applicable)
	"1985",
	"Tehkan World Cup (set 1)\0",	// Full Name A
	NULL,							// Comment A
	"Tehkan",						// Manufacturer A
	"Miscellaneous",				// System A
	NULL,					// Full Name W
	NULL,					// Comment W
	NULL,					// Manufacturer W
	NULL,					// System W
	// Flags
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED,
	2,						// Players: Max number of players a game supports (so we can remove single player games from netplay)
	HARDWARE_MISC_PRE90S, 	// Hardware: Which type of hardware the game runs on
	GBF_SPORTSFOOTBALL,		// Genre
	0,						// Family
	NULL,					// Function to get possible zip names
	TWCRomInfo,		// Function to get the length and crc of each rom
	TWCRomName,		// Function to get the possible names for each rom
	NULL,					// Function to get hdd info
	NULL,					// Function to get the possible names for each hdd
	NULL,					// Function to get the sample flags
	NULL,					// Function to get the possible names for each sample
	TWCInputInfo,		// Function to get the input info for the game
	TWCDIPInfo,		// Function to get the input info for the game
	TWCInit,				// Init
	DrvExit,				// Exit
	TWCFrame,				// Frame
	DrvDraw,				// Redraw
	TWCScan,				// Area Scan
	&DrvRecalc,				// Recalc Palettes: Set to 1 if the palette needs to be fully re-calculated
	0x120,					// Number of Palette Entries
	224,					// Screen width
	256,					// Screen height
	3,						// Screen x aspect
	4 						// Screen y aspect
};
