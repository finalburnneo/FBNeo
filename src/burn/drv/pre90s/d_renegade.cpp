// FB Alpha Renegade driver module
// Based on MAME driver by Phil Stroffolino, Carlos A. Lozano, Rob Rosenbrock

// todo: clean up this mess. -dink

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "m6805_intf.h"
#include "m6809_intf.h"
#include "burn_ym3526.h"
#include "msm5205.h"

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]        = {0, 0};
static UINT8 DrvInput[3]      = {0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *Mem                 = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *DrvM6502Rom         = NULL;
static UINT8 *DrvM6809Rom         = NULL;
static UINT8 *DrvM68705Rom        = NULL;
static UINT8 *DrvM68705Ram        = NULL;
static UINT8 *DrvADPCMRom         = NULL;
static UINT8 *DrvM6502Ram         = NULL;
static UINT8 *DrvM6809Ram         = NULL;
static UINT8 *DrvVideoRam1        = NULL;
static UINT8 *DrvVideoRam2        = NULL;
static UINT8 *DrvSpriteRam        = NULL;
static UINT8 *DrvPaletteRam1      = NULL;
static UINT8 *DrvPaletteRam2      = NULL;
static UINT8 *DrvChars            = NULL;
static UINT8 *DrvTiles            = NULL;
static UINT8 *DrvSprites          = NULL;
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette         = NULL;

static UINT8 DrvRomBank;
static UINT8 DrvVBlank;
static UINT8 DrvScrollX[2];
static UINT8 DrvSoundLatch;
static UINT8 DrvADPCMPlaying = 0;
static UINT32 DrvADPCMPos = 0;
static UINT32 DrvADPCMEnd = 0;

// MCU Simulation Variables
#define MCU_TYPE_NONE		0
#define MCU_TYPE_MCU		1

static INT32 DisableMCUEmulation = 0;

// MCU Emulation Variables
static INT32 MCUFromMain;
static INT32 MCUFromMcu;
static INT32 MCUMainSent;
static INT32 MCUMcuSent;
static UINT8 MCUDdrA;
static UINT8 MCUDdrB;
static UINT8 MCUDdrC;
static UINT8 MCUPortAOut;
static UINT8 MCUPortBOut;
static UINT8 MCUPortCOut;
static UINT8 MCUPortAIn;
static UINT8 MCUPortBIn;
static UINT8 MCUPortCIn;

static struct BurnInputInfo DrvInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort1 + 6, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , DrvInputPort2 + 2, "p1 fire 3" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort0 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 fire 3" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 7, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};


STDINPUTINFO(Drv)

static inline void DrvMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = DrvInput[1] = 0xff;
	DrvInput[2] = 0x9c;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] -= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] -= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] -= (DrvInputPort2[i] & 1) << i;
	}
}

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xbf, NULL                     },
	{0x15, 0xff, 0xff, 0x03, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x14, 0x01, 0x03, 0x00, "2 Coins 1 Play"         },
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Play"         },
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Plays"        },
	{0x14, 0x01, 0x03, 0x01, "1 Coin  3 Plays"        },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 1 Play"         },
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Play"         },
	{0x14, 0x01, 0x0c, 0x08, "1 Coin  2 Plays"        },
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  3 Plays"        },
	
	{0   , 0xfe, 0   , 2   , "Lives"                  },
	{0x14, 0x01, 0x10, 0x10, "1"                      },
	{0x14, 0x01, 0x10, 0x00, "2"                      },
	
	{0   , 0xfe, 0   , 2   , "Bonus"                  },
	{0x14, 0x01, 0x20, 0x20, "30k"                    },
	{0x14, 0x01, 0x20, 0x00, "None"                   },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x14, 0x01, 0x40, 0x00, "Upright"                },
	{0x14, 0x01, 0x40, 0x40, "Cocktail"               },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x14, 0x01, 0x80, 0x80, "Off"                    },
	{0x14, 0x01, 0x80, 0x00, "On"                     },
	
	// Dip 2	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x15, 0x01, 0x03, 0x02, "Easy"                   },
	{0x15, 0x01, 0x03, 0x03, "Normal"                 },
	{0x15, 0x01, 0x03, 0x01, "Hard"                   },
	{0x15, 0x01, 0x03, 0x00, "Very Hard"              },
};

STDDIPINFO(Drv)

static struct BurnRomInfo DrvRomDesc[] = {
	{ "nb-5.ic51",     0x08000, 0xba683ddf, BRF_ESS | BRF_PRG }, //  0	M6502 Program Code
	{ "na-5.ic52",     0x08000, 0xde7e7df4, BRF_ESS | BRF_PRG }, //	 1
	
	{ "n0-5.ic13",     0x08000, 0x3587de3b, BRF_ESS | BRF_PRG }, //  2	M6809 Program Code
	
	{ "nc-5.bin",      0x08000, 0x9adfaa5d, BRF_GRA },	     //  3	Characters
	
	{ "n1-5.ic1",      0x08000, 0x4a9f47f3, BRF_GRA },	     //  4	Tiles
	{ "n6-5.ic28",     0x08000, 0xd62a0aa8, BRF_GRA },	     //  5
	{ "n7-5.ic27",     0x08000, 0x7ca5a532, BRF_GRA },	     //  6
	{ "n2-5.ic14",     0x08000, 0x8d2e7982, BRF_GRA },	     //  7
	{ "n8-5.ic26",     0x08000, 0x0dba31d3, BRF_GRA },	     //  8
	{ "n9-5.ic25",     0x08000, 0x5b621b6a, BRF_GRA },	     //  9
	
	{ "nh-5.bin",      0x08000, 0xdcd7857c, BRF_GRA },	     //  10	Sprites
	{ "nd-5.bin",      0x08000, 0x2de1717c, BRF_GRA },	     //  11
	{ "nj-5.bin",      0x08000, 0x0f96a18e, BRF_GRA },	     //  12
	{ "nn-5.bin",      0x08000, 0x1bf15787, BRF_GRA },	     //  13
	{ "ne-5.bin",      0x08000, 0x924c7388, BRF_GRA },	     //  14
	{ "nk-5.bin",      0x08000, 0x69499a94, BRF_GRA },	     //  15
	{ "ni-5.bin",      0x08000, 0x6f597ed2, BRF_GRA },	     //  16
	{ "nf-5.bin",      0x08000, 0x0efc8d45, BRF_GRA },	     //  17
	{ "nl-5.bin",      0x08000, 0x14778336, BRF_GRA },	     //  18
	{ "no-5.bin",      0x08000, 0x147dd23b, BRF_GRA },	     //  19
	{ "ng-5.bin",      0x08000, 0xa8ee3720, BRF_GRA },	     //  20
	{ "nm-5.bin",      0x08000, 0xc100258e, BRF_GRA },	     //  21
	
	{ "n3-5.ic33",     0x08000, 0x78fd6190, BRF_GRA },	     //  22	ADPCM
	{ "n4-5.ic32",     0x08000, 0x6557564c, BRF_GRA },	     //  23
	{ "n5-5.ic31",     0x08000, 0x7ee43a3c, BRF_GRA },	     //  24
	
	{ "nz-5.ic97",     0x00800, 0x32e47560, BRF_ESS | BRF_PRG },	// 25 MCU
};

STD_ROM_PICK(Drv)
STD_ROM_FN(Drv)

static struct BurnRomInfo DrvjRomDesc[] = {
	{ "nb-01.bin",     0x08000, 0x93fcfdf5, BRF_ESS | BRF_PRG }, //  0	M6502 Program Code
	{ "ta18-11.bin",   0x08000, 0xf240f5cd, BRF_ESS | BRF_PRG }, //	 1
	
	{ "n0-5.bin",      0x08000, 0x3587de3b, BRF_ESS | BRF_PRG }, //  2	M6809 Program Code
	
	{ "ta18-25.bin",   0x08000, 0x9bd2bea3, BRF_GRA },	     //  3	Characters
	
	{ "ta18-01.bin",   0x08000, 0xdaf15024, BRF_GRA },	     //  4	Tiles
	{ "ta18-06.bin",   0x08000, 0x1f59a248, BRF_GRA },	     //  5
	{ "n7-5.bin",      0x08000, 0x7ca5a532, BRF_GRA },	     //  6
	{ "ta18-02.bin",   0x08000, 0x994c0021, BRF_GRA },	     //  7
	{ "ta18-04.bin",   0x08000, 0x55b9e8aa, BRF_GRA },	     //  8
	{ "ta18-03.bin",   0x08000, 0x0475c99a, BRF_GRA },	     //  9
	
	{ "ta18-20.bin",   0x08000, 0xc7d54139, BRF_GRA },	     //  10	Sprites
	{ "ta18-24.bin",   0x08000, 0x84677d45, BRF_GRA },	     //  11
	{ "ta18-18.bin",   0x08000, 0x1c770853, BRF_GRA },	     //  12
	{ "ta18-14.bin",   0x08000, 0xaf656017, BRF_GRA },	     //  13
	{ "ta18-23.bin",   0x08000, 0x3fd19cf7, BRF_GRA },	     //  14
	{ "ta18-17.bin",   0x08000, 0x74c64c6e, BRF_GRA },	     //  15
	{ "ta18-19.bin",   0x08000, 0xc8795fd7, BRF_GRA },	     //  16
	{ "ta18-22.bin",   0x08000, 0xdf3a2ff5, BRF_GRA },	     //  17
	{ "ta18-16.bin",   0x08000, 0x7244bad0, BRF_GRA },	     //  18
	{ "ta18-13.bin",   0x08000, 0xb6b14d46, BRF_GRA },	     //  19
	{ "ta18-21.bin",   0x08000, 0xc95e009b, BRF_GRA },	     //  20
	{ "ta18-15.bin",   0x08000, 0xa5d61d01, BRF_GRA },	     //  21
	
	{ "ta18-09.bin",   0x08000, 0x07ed4705, BRF_GRA },	     //  22	ADPCM
	{ "ta18-08.bin",   0x08000, 0xc9312613, BRF_GRA },	     //  23
	{ "ta18-07.bin",   0x08000, 0x02e3f3ed, BRF_GRA },	     //  24
	
	{ "nz-0.bin",      0x00800, 0x98a39880, BRF_ESS | BRF_PRG },	// 25 MCU
};

STD_ROM_PICK(Drvj)
STD_ROM_FN(Drvj)

static struct BurnRomInfo DrvubRomDesc[] = {
	{ "na-5.ic52",     0x08000, 0xde7e7df4, BRF_ESS | BRF_PRG }, //  0	M6502 Program Code
	{ "40.ic51",       0x08000, 0x3dbaac11, BRF_ESS | BRF_PRG }, //	 1  bootleg
	
	{ "n0-5.ic13",     0x08000, 0x3587de3b, BRF_ESS | BRF_PRG }, //  2	M6809 Program Code
	
	{ "nc-5.bin",      0x08000, 0x9adfaa5d, BRF_GRA },	     //  3	Characters
	
	{ "n1-5.ic1",      0x08000, 0x4a9f47f3, BRF_GRA },	     //  4	Tiles
	{ "n6-5.ic28",     0x08000, 0xd62a0aa8, BRF_GRA },	     //  5
	{ "n7-5.ic27",     0x08000, 0x7ca5a532, BRF_GRA },	     //  6
	{ "n2-5.ic14",     0x08000, 0x8d2e7982, BRF_GRA },	     //  7
	{ "n8-5.ic26",     0x08000, 0x0dba31d3, BRF_GRA },	     //  8
	{ "n9-5.ic25",     0x08000, 0x5b621b6a, BRF_GRA },	     //  9
	
	{ "nh-5.bin",      0x08000, 0xdcd7857c, BRF_GRA },	     //  10	Sprites
	{ "nd-5.bin",      0x08000, 0x2de1717c, BRF_GRA },	     //  11
	{ "nj-5.bin",      0x08000, 0x0f96a18e, BRF_GRA },	     //  12
	{ "nn-5.bin",      0x08000, 0x1bf15787, BRF_GRA },	     //  13
	{ "ne-5.bin",      0x08000, 0x924c7388, BRF_GRA },	     //  14
	{ "nk-5.bin",      0x08000, 0x69499a94, BRF_GRA },	     //  15
	{ "ni-5.bin",      0x08000, 0x6f597ed2, BRF_GRA },	     //  16
	{ "nf-5.bin",      0x08000, 0x0efc8d45, BRF_GRA },	     //  17
	{ "nl-5.bin",      0x08000, 0x14778336, BRF_GRA },	     //  18
	{ "no-5.bin",      0x08000, 0x147dd23b, BRF_GRA },	     //  19
	{ "ng-5.bin",      0x08000, 0xa8ee3720, BRF_GRA },	     //  20
	{ "nm-5.bin",      0x08000, 0xc100258e, BRF_GRA },	     //  21
	
	{ "n3-5.ic33",     0x08000, 0x78fd6190, BRF_GRA },	     //  22	ADPCM
	{ "n4-5.ic32",     0x08000, 0x6557564c, BRF_GRA },	     //  23
	{ "n5-5.ic31",     0x08000, 0x7ee43a3c, BRF_GRA },	     //  24	
};

STD_ROM_PICK(Drvub)
STD_ROM_FN(Drvub)

static struct BurnRomInfo DrvbRomDesc[] = {
	{ "ta18-10.bin",   0x08000, 0xa90cf44a, BRF_ESS | BRF_PRG }, //  0	M6502 Program Code
	{ "ta18-11.bin",   0x08000, 0xf240f5cd, BRF_ESS | BRF_PRG }, //	 1
	
	{ "n0-5.bin",      0x08000, 0x3587de3b, BRF_ESS | BRF_PRG }, //  2	M6809 Program Code
	
	{ "ta18-25.bin",   0x08000, 0x9bd2bea3, BRF_GRA },	     //  3	Characters
	
	{ "ta18-01.bin",   0x08000, 0xdaf15024, BRF_GRA },	     //  4	Tiles
	{ "ta18-06.bin",   0x08000, 0x1f59a248, BRF_GRA },	     //  5
	{ "n7-5.bin",      0x08000, 0x7ca5a532, BRF_GRA },	     //  6
	{ "ta18-02.bin",   0x08000, 0x994c0021, BRF_GRA },	     //  7
	{ "ta18-04.bin",   0x08000, 0x55b9e8aa, BRF_GRA },	     //  8
	{ "ta18-03.bin",   0x08000, 0x0475c99a, BRF_GRA },	     //  9
	
	{ "ta18-20.bin",   0x08000, 0xc7d54139, BRF_GRA },	     //  10	Sprites
	{ "ta18-24.bin",   0x08000, 0x84677d45, BRF_GRA },	     //  11
	{ "ta18-18.bin",   0x08000, 0x1c770853, BRF_GRA },	     //  12
	{ "ta18-14.bin",   0x08000, 0xaf656017, BRF_GRA },	     //  13
	{ "ta18-23.bin",   0x08000, 0x3fd19cf7, BRF_GRA },	     //  14
	{ "ta18-17.bin",   0x08000, 0x74c64c6e, BRF_GRA },	     //  15
	{ "ta18-19.bin",   0x08000, 0xc8795fd7, BRF_GRA },	     //  16
	{ "ta18-22.bin",   0x08000, 0xdf3a2ff5, BRF_GRA },	     //  17
	{ "ta18-16.bin",   0x08000, 0x7244bad0, BRF_GRA },	     //  18
	{ "ta18-13.bin",   0x08000, 0xb6b14d46, BRF_GRA },	     //  19
	{ "ta18-21.bin",   0x08000, 0xc95e009b, BRF_GRA },	     //  20
	{ "ta18-15.bin",   0x08000, 0xa5d61d01, BRF_GRA },	     //  21
	
	{ "ta18-09.bin",   0x08000, 0x07ed4705, BRF_GRA },	     //  22	ADPCM
	{ "ta18-08.bin",   0x08000, 0xc9312613, BRF_GRA },	     //  23
	{ "ta18-07.bin",   0x08000, 0x02e3f3ed, BRF_GRA },	     //  24
};

STD_ROM_PICK(Drvb)
STD_ROM_FN(Drvb)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvM6502Rom            = Next; Next += 0x10000;
	DrvM6809Rom            = Next; Next += 0x08000;
	DrvM68705Rom           = Next; Next += 0x00800;
	DrvADPCMRom            = Next; Next += 0x18000;

	RamStart               = Next;

	DrvM6502Ram            = Next; Next += 0x01800;
	DrvM6809Ram            = Next; Next += 0x01000;
	DrvM68705Ram           = Next; Next += 0x00070;
	DrvSpriteRam           = Next; Next += 0x00800;
	DrvVideoRam1           = Next; Next += 0x00800;
	DrvVideoRam2           = Next; Next += 0x00800;
	DrvPaletteRam1         = Next; Next += 0x00100;
	DrvPaletteRam2         = Next; Next += 0x00100;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x0400 * 8 * 8;
	DrvTiles               = Next; Next += 0x0800 * 16 * 16;
	DrvSprites             = Next; Next += 0x1000 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static UINT8 mcu_reset_r()
{
	m6805Open(0);
	m68705Reset();
	m6805Close();

	return 0;
}

static void mcu_w(UINT8 data)
{
	MCUFromMain = data;
	MCUMainSent = 1;
	m6805Open(0);
	m68705SetIrqLine(0, 1);
	m6805Close();
}

static UINT8 mcu_r()
{
	MCUMcuSent = 0;
	return MCUFromMcu;
}

static UINT8 mcu_status_r()
{
	UINT8 Res = 0;

	if (DisableMCUEmulation) {
		Res = 1;
	} else {
		if (!MCUMainSent)
			Res |= 0x01;
		if (!MCUMcuSent)
			Res |= 0x02;
	}
	
	return Res;
}

static INT32 DrvDoReset()
{
	M6502Open(0);
	M6502Reset();
	M6502Close();
	
	M6809Open(0);
	BurnYM3526Reset();
	MSM5205Reset();
	M6809Reset();
	M6809Close();
	
	if (!DisableMCUEmulation) {
		m6805Open(0);
		m68705Reset();
		m6805Close();
		
		MCUFromMain = 0;
		MCUFromMcu = 0;
		MCUMainSent = 0;
		MCUMcuSent = 0;
		MCUDdrA = 0;
		MCUDdrB = 0;
		MCUDdrC = 0;
		MCUPortAOut = 0;
		MCUPortBOut = 0;
		MCUPortCOut = 0;
		MCUPortAIn = 0;
		MCUPortBIn = 0;
		MCUPortCIn = 0;
	}

	DrvRomBank = 0;
	DrvVBlank = 0;
	memset(DrvScrollX, 0, 2);
	DrvSoundLatch = 0;
	DrvADPCMPlaying = 0;
	DrvADPCMPos = 0;
	DrvADPCMEnd = 0;

	HiscoreReset();

	return 0;
}

static UINT8 RenegadeReadByte(UINT16 Address)
{
	switch (Address) {
		case 0x3800: {
			return DrvInput[0];
		}
		
		case 0x3801: {
			return DrvInput[1];
		}
		
		case 0x3802: {
			UINT8 MCUStatus = mcu_status_r();
			if (MCUStatus) MCUStatus = (MCUStatus - 1) * 0x10;
			return DrvInput[2] + DrvDip[1] + (DrvVBlank ? 0x40 : 0) + MCUStatus;
		}
		
		case 0x3803: {
			return DrvDip[0];
		}
		
		case 0x3804: {
			if (!DisableMCUEmulation) {
				return mcu_r();
			}
			return 0;
		}
		
		case 0x3805: {
			if (!DisableMCUEmulation) {
				return mcu_reset_r();
			}
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Read Byte %04X\n"), Address);
		}
	}

	return 0;
}

static void bankswitch(UINT8 Data)
{
	DrvRomBank = Data & 1;
	M6502MapMemory(DrvM6502Rom + 0x8000 + (DrvRomBank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void RenegadeWriteByte(UINT16 Address, UINT8 Data)
{
	switch (Address) {
		case 0x3800: {
			DrvScrollX[0] = Data;
			return;
		}
		
		case 0x3801: {
			DrvScrollX[1] = Data;
			return;
		}
		
		case 0x3802: {
			DrvSoundLatch = Data;
			M6809Open(0);
			M6809SetIRQLine(M6809_IRQ_LINE, CPU_IRQSTATUS_AUTO);
			M6809Close();
			return;
		}
		
		case 0x3803: {
			// flipscreen
			return;
		}
		
		case 0x3804: {
			if (!DisableMCUEmulation) mcu_w(Data);
			return;
		}
		
		case 0x3805: {
			bankswitch(Data);
			return;
		}
		
		case 0x3806: {
			// nop
			return;
		}
		
		case 0x3807: {
			// coin counter
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Write Byte %04X, %02X\n"), Address, Data);
		}
	}
}

static UINT8 RenegadeM6809ReadByte(UINT16 Address)
{
	switch (Address) {
		case 0x1000: {
			return DrvSoundLatch;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6809 Read Byte %04X\n"), Address);
		}
	}

	return 0;
}

static void RenegadeM6809WriteByte(UINT16 Address, UINT8 Data)
{
	switch (Address) {
		case 0x1800: {
			MSM5205ResetWrite(0, 0);
			DrvADPCMPlaying = 1;
			return;
		}
		
		case 0x2000: {
			switch (Data & 0x1c) {
				case 0x18: DrvADPCMPos = 0 * 0x8000 * 2; break;
				case 0x14: DrvADPCMPos = 1 * 0x8000 * 2; break;
				case 0x0c: DrvADPCMPos = 2 * 0x8000 * 2; break;
				default: DrvADPCMPos = DrvADPCMEnd = 0; return;
			}
			DrvADPCMPos |= (Data & 0x03) * 0x2000 * 2;
			DrvADPCMEnd = DrvADPCMPos + 0x2000 * 2;
			return;
		}
		
		case 0x2800: {
			BurnYM3526Write(0, Data);
			return;
		}
		
		case 0x2801: {
			BurnYM3526Write(1, Data);
			return;
		}
		
		case 0x3000: {
			MSM5205ResetWrite(0, 1);
			DrvADPCMPlaying = 0;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("M6809 Write Byte %04X, %02X\n"), Address, Data);
		}
	}
}

static UINT8 MCUReadByte(UINT16 address)
{
	switch (address & 0x7ff) {
		case 0x000: {
			return (MCUPortAOut & MCUDdrA) | (MCUPortAIn & ~MCUDdrA);
		}
		
		case 0x001: {
			return (MCUPortBOut & MCUDdrB) | (MCUPortBIn & ~MCUDdrB);
		}
		
		case 0x002: {
			MCUPortCIn = 0;
			if (MCUMainSent) MCUPortCIn |= 0x01;
			if (!MCUMcuSent) MCUPortCIn |= 0x02;

			return (MCUPortCOut & MCUDdrC) | (MCUPortCIn & ~MCUDdrC);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("MCU Read %x\n"), address);
		}		
	}

	return 0;
}

static void MCUWriteByte(UINT16 address, UINT8 data)
{
	switch (address & 0x7ff) {
		case 0x000: {
			MCUPortAOut = data;
			break;
		}
		
		case 0x001: {
			if ((MCUDdrB & 0x02) && (~data & 0x02) && (MCUPortBOut & 0x02)) {
				MCUPortAIn = MCUFromMain;

				if (MCUMainSent) {
					m68705SetIrqLine(0, 0);
					MCUMainSent = 0;
				}
			}
			
			if ((MCUDdrB & 0x04) && (data & 0x04) && (~MCUPortBOut & 0x04)) {
				MCUFromMcu = MCUPortAOut;
				MCUMcuSent = 1;
			}

			MCUPortBOut = data;
			break;
		}
		
		case 0x002: {
			MCUPortCOut = data;
			break;
		}
		
		case 0x004: {
			MCUDdrA = data;
			break;
		}
		
		case 0x005: {
			MCUDdrB = data;
			break;
		}
		
		case 0x006: {
			MCUDdrC = data;
			break;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("MCU Write %x, %x\n"), address, data);
		}
	}
}

static INT32 CharPlaneOffsets[3]   = { 2, 4, 6 };
static INT32 CharXOffsets[8]       = { 1, 0, 65, 64, 129, 128, 193, 192 };
static INT32 CharYOffsets[8]       = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Tile1PlaneOffsets[3]  = { 0x00004, 0x40000, 0x40004 };
static INT32 Tile2PlaneOffsets[3]  = { 0x00000, 0x60000, 0x60004 };
static INT32 Tile3PlaneOffsets[3]  = { 0x20004, 0x80000, 0x80004 };
static INT32 Tile4PlaneOffsets[3]  = { 0x20000, 0xa0000, 0xa0004 };
static INT32 TileXOffsets[16]      = { 3, 2, 1, 0, 131, 130, 129, 128, 259, 258, 257, 256, 387, 386, 385, 384 };
static INT32 TileYOffsets[16]      = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 1500000;
}

static void DrvMSM5205Int()
{
	if (!DrvADPCMPlaying) {
		MSM5205ResetWrite(0, 1);
		return;
	}

	if (DrvADPCMPos >= DrvADPCMEnd) {
		MSM5205ResetWrite(0, 1);
		DrvADPCMPlaying = false;
		M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
	} else {
		UINT8 const data = DrvADPCMRom[DrvADPCMPos / 2];
		MSM5205DataWrite(0, (DrvADPCMPos & 1) ? data & 0xf : data >> 4);
		DrvADPCMPos++;
	}
}

static INT32 DrvInit(INT32 nMcuType)
{
	INT32 nRet = 0, nLen;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x60000);

	nRet = BurnLoadRom(DrvM6502Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvM6502Rom + 0x08000, 1, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvM6809Rom + 0x00000, 2, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvTempRom, 3, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 3, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, DrvTempRom, DrvChars);
	
	memset(DrvTempRom, 0, 0x60000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 9, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvTiles + (0x000 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvTiles + (0x100 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvTiles + (0x200 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvTiles + (0x300 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvTiles + (0x400 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvTiles + (0x500 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvTiles + (0x600 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvTiles + (0x700 * 16 * 16));
	
	memset(DrvTempRom, 0, 0x60000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x38000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 19, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 21, 1); if (nRet != 0) return 1;
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvSprites + (0x000 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvSprites + (0x100 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvSprites + (0x200 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x00000, DrvSprites + (0x300 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvSprites + (0x400 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvSprites + (0x500 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvSprites + (0x600 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x18000, DrvSprites + (0x700 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x30000, DrvSprites + (0x800 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x30000, DrvSprites + (0x900 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x30000, DrvSprites + (0xa00 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x30000, DrvSprites + (0xb00 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile1PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x48000, DrvSprites + (0xc00 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile2PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x48000, DrvSprites + (0xd00 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile3PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x48000, DrvSprites + (0xe00 * 16 * 16));
	GfxDecode(0x100, 3, 16, 16, Tile4PlaneOffsets, TileXOffsets, TileYOffsets, 0x200, DrvTempRom + 0x48000, DrvSprites + (0xf00 * 16 * 16));
	
	nRet = BurnLoadRom(DrvADPCMRom + 0x00000, 22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvADPCMRom + 0x08000, 23, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvADPCMRom + 0x10000, 24, 1); if (nRet != 0) return 1;
		
	BurnFree(DrvTempRom);
	
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502Ram            , 0x0000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvVideoRam2           , 0x1800, 0x1fff, MAP_RAM);
	M6502MapMemory(DrvSpriteRam           , 0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVideoRam1           , 0x2800, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvPaletteRam1         , 0x3000, 0x30ff, MAP_RAM);
	M6502MapMemory(DrvPaletteRam2         , 0x3100, 0x31ff, MAP_RAM);
	M6502MapMemory(DrvM6502Rom + 0x8000   , 0x4000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvM6502Rom            , 0x8000, 0xffff, MAP_ROM);
	M6502SetReadHandler(RenegadeReadByte);
	M6502SetWriteHandler(RenegadeWriteByte);
	M6502Close();
	
	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809Ram          , 0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvM6809Rom          , 0x8000, 0xffff, MAP_ROM);
	M6809SetReadHandler(RenegadeM6809ReadByte);
	M6809SetWriteHandler(RenegadeM6809WriteByte);
	M6809Close();
	
	MSM5205Init(0, DrvSynchroniseStream, 12000000 / 32, DrvMSM5205Int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	
	if (nMcuType == MCU_TYPE_MCU) {
		nRet = BurnLoadRom(DrvM68705Rom, 25, 1); if (nRet != 0) return 1;
		
		m6805Init(1, 0x800);
		m6805Open(0);
		m6805MapMemory(DrvM68705Ram         , 0x0010, 0x007f, MAP_RAM);
		m6805MapMemory(DrvM68705Rom + 0x0080, 0x0080, 0x07ff, MAP_ROM);
		m6805SetWriteHandler(MCUWriteByte);
		m6805SetReadHandler(MCUReadByte);
		m6805Close();
	}
	
	if (nMcuType == MCU_TYPE_NONE) {
		DisableMCUEmulation = 1;
	}
	
	BurnYM3526Init(3000000, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&M6809Config, 1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();
	
	DrvDoReset();

	return 0;
}

static INT32 RenegadeInit()
{
	return DrvInit(MCU_TYPE_MCU);
}

static INT32 KuniokunbInit()
{
	return DrvInit(MCU_TYPE_NONE);
}

static INT32 DrvExit()
{
	M6502Exit();
	M6809Exit();
	if (!DisableMCUEmulation) m6805Exit();
	
	BurnYM3526Exit();
	MSM5205Exit();
	
	GenericTilesExit();
	
	BurnFree(Mem);
	
	DisableMCUEmulation = 0;
	
	MCUFromMain = 0;
	MCUFromMcu = 0;
	MCUMainSent = 0;
	MCUMcuSent = 0;
	MCUDdrA = 0;
	MCUDdrB = 0;
	MCUDdrC = 0;
	MCUPortAOut = 0;
	MCUPortBOut = 0;
	MCUPortCOut = 0;
	MCUPortAIn = 0;
	MCUPortBIn = 0;
	MCUPortCIn = 0;
	
	DrvRomBank = 0;
	DrvVBlank = 0;
	memset(DrvScrollX, 0, sizeof(DrvScrollX));
	DrvSoundLatch = 0;
	DrvADPCMPlaying = 0;
	DrvADPCMPos = 0;
	DrvADPCMEnd = 0;

	return 0;
}

static inline UINT8 pal4bit(UINT8 bits)
{
	bits &= 0x0f;
	return (bits << 4) | bits;
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = pal4bit(nColour >> 0);
	g = pal4bit(nColour >> 4);
	b = pal4bit(nColour >> 8);

	return BurnHighCol(r, g, b, 0);
}

static void DrvCalcPalette()
{
	for (INT32 i = 0; i < 0x100; i++) {
		INT32 Val = DrvPaletteRam1[i] + (DrvPaletteRam2[i] << 8);
		
		DrvPalette[i] = CalcCol(Val);
	}
}

static void DrvRenderBgLayer()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex = 0, xScroll;
	
	xScroll = DrvScrollX[0] + (DrvScrollX[1] << 8);
	xScroll &= 0x3ff;
	xScroll -= 256;

	for (my = 0; my < 16; my++) {
		for (mx = 0; mx < 64; mx++) {
			Attr = DrvVideoRam1[TileIndex + 0x400];
			Code = DrvVideoRam1[TileIndex + 0x000];
			Colour = Attr >> 5;
			
			x = 16 * mx;
			y = 16 * my;
			
			x -= xScroll;
			if (x < -16) x += 1024;
			
			x -= 8;

			if (x > 0 && x < 224 && y > 0 && y < 224) {
				Render16x16Tile(pTransDraw, Code, x, y, Colour, 3, 192, DrvTiles + ((Attr & 0x07) * 0x100 * 16 * 16));
			} else {
				Render16x16Tile_Clip(pTransDraw, Code, x, y, Colour, 3, 192, DrvTiles + ((Attr & 0x07) * 0x100 * 16 * 16));
			}

			TileIndex++;
		}
	}
}

static void DrvRenderSprites()
{
	UINT8 *Source = DrvSpriteRam;
	UINT8 *Finish = Source + 96 * 4;

	while (Source < Finish) {
		INT32 sy = 224 - Source[0];

		if (sy >= 16) {
			INT32 Attr = Source[1];
			INT32 sx = Source[3];
			INT32 Code = Source[2];
			INT32 SpriteBank = Attr & 0xf;
			INT32 Colour = (Attr >> 4) & 0x3;
			INT32 xFlip = Attr & 0x40;

			if (sx > 248) sx -= 256;
			
			sx -= 8;

			if (Attr & 0x80) {
				Code &= ~1;
				if (sx > 16 && sx < 224 && (sy + 16) > 0 && (sy + 16) < 224) {
					if (xFlip) {
						Render16x16Tile_Mask_FlipX(pTransDraw, Code + 1, sx, sy + 16, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
					} else {
						Render16x16Tile_Mask(pTransDraw, Code + 1, sx, sy + 16, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
					}
				} else {
					if (xFlip) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code + 1, sx, sy + 16, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, Code + 1, sx, sy + 16, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
					}
				}
			} else {
				sy += 16;
			}
			
			if (sx > 16 && sx < 224 && sy > 0 && sy < 224) {
				if (xFlip) {
					Render16x16Tile_Mask_FlipX(pTransDraw, Code, sx, sy, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
				} else {
					Render16x16Tile_Mask(pTransDraw, Code, sx, sy, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
				}
			} else {
				if (xFlip) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 128, DrvSprites + (SpriteBank * 0x100 * 16 * 16));
				}
			}
		}
		Source += 4;
	}
}

static void DrvRenderCharLayer()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex = 0;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 32; mx++) {
			Attr = DrvVideoRam2[TileIndex + 0x400];
			Code = ((Attr & 3) << 8) + DrvVideoRam2[TileIndex + 0x000];
			Colour = Attr >> 6;
			
			x = 8 * mx;
			y = 8 * my;
			
			x -= 8;

			if (x > 0 && x < 232 && y > 0 && y < 232) {
				Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 3, 0, 0, DrvChars);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 3, 0, 0, DrvChars);
			}

			TileIndex++;
		}
	}
}

static INT32 DrvDraw()
{
	BurnTransferClear();
	DrvCalcPalette();
	DrvRenderBgLayer();
	DrvRenderSprites();
	DrvRenderCharLayer();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	INT32 nInterleave = MSM5205CalcInterleave(0, 12000000 / 8);

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	INT32 nCyclesTotal[3] = { (12000000 / 8) / 60, (12000000 / 8) / 60, (12000000 / 4) / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	DrvVBlank = 0;

	M6502NewFrame();
	M6809NewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		M6502Open(0);
		CPU_RUN(0, M6502);
		if (i == ((nInterleave / 10) * 7)) DrvVBlank = 1;
		if (i ==  (nInterleave / 2)) M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
		if (i == ((nInterleave / 10) * 9)) M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_HOLD);
		M6502Close();

		if (!DisableMCUEmulation) {
			m6805Open(0);
			CPU_RUN(2, m6805);
			m6805Close();
		}

		M6809Open(0);
		CPU_RUN_TIMER(1);
		MSM5205Update();
		M6809Close();
	}

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029696;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6502Scan(nAction);
		m6805Scan(nAction);
		M6809Scan(nAction);

		BurnYM3526Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(DrvRomBank);
		SCAN_VAR(DrvScrollX);
		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(DrvADPCMPlaying);
		SCAN_VAR(DrvADPCMPos);
		SCAN_VAR(DrvADPCMEnd);
		SCAN_VAR(MCUFromMain);
		SCAN_VAR(MCUFromMcu);
		SCAN_VAR(MCUMainSent);
		SCAN_VAR(MCUMcuSent);
		SCAN_VAR(MCUDdrA);
		SCAN_VAR(MCUDdrB);
		SCAN_VAR(MCUDdrC);
		SCAN_VAR(MCUPortAOut);
		SCAN_VAR(MCUPortBOut);
		SCAN_VAR(MCUPortCOut);
		SCAN_VAR(MCUPortAIn);
		SCAN_VAR(MCUPortBIn);
		SCAN_VAR(MCUPortCIn);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(DrvRomBank);
		M6502Close();
	}

	return 0;
}

struct BurnDriver BurnDrvRenegade = {
	"renegade", NULL, NULL, NULL, "1986",
	"Renegade (US)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, DrvRomInfo, DrvRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	RenegadeInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 240, 240, 4, 3
};

struct BurnDriver BurnDrvKuniokun = {
	"kuniokun", "renegade", NULL, NULL, "1986",
	"Nekketsu Kouha Kunio-kun (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, DrvjRomInfo, DrvjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	RenegadeInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 240, 240, 4, 3
};

struct BurnDriver BurnDrvRenegadeb = {
	"renegadeb", "renegade", NULL, NULL, "1986",
	"Renegade (US bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, DrvubRomInfo, DrvubRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	KuniokunbInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 240, 240, 4, 3
};

struct BurnDriver BurnDrvKuniokunb = {
	"kuniokunb", "renegade", NULL, NULL, "1986",
	"Nekketsu Kouha Kunio-kun (Japan bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, DrvbRomInfo, DrvbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	KuniokunbInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 240, 240, 4, 3
};
