// based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6502_intf.h"
#include "mcs51.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"

static UINT8 DrvInputPort0[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8]     = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[2]            = {0, 0};
static UINT8 DrvInput[3]          = {0x00, 0x00, 0x00};
static UINT8 DrvReset             = 0;

static UINT8 *Mem                 = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *DrvM6809Rom         = NULL;
static UINT8 *DrvM6502Rom         = NULL;
static UINT8 *DrvMCUROM           = NULL;
static UINT8 *DrvProm             = NULL;
static UINT8 *DrvM6809Ram         = NULL;
static UINT8 *DrvM6502Ram         = NULL;
static UINT8 *DrvVideoRam         = NULL;
static UINT8 *DrvColourRam        = NULL;
static UINT8 *DrvSpriteRam        = NULL;
static UINT8 *DrvChars            = NULL;
static UINT8 *DrvSprites          = NULL;
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette         = NULL;

// i8751 MCU emulation
static INT32 realMCU = 0;
static UINT8 i8751PortData[4] = { 0, 0, 0, 0 };

static UINT8 DrvSoundLatch;

typedef INT32 (*SidePcktLoadRoms)();
static SidePcktLoadRoms LoadRomsFunction;

static struct BurnInputInfo DrvInputList[] =
{
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort0 + 6, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort1 + 6, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort0 + 7, "p2 start"  },

	{"Up"                , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 up"     },
	{"Down"              , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 down"   },
	{"Left"              , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 left"   },
	{"Right"             , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 right"  },
	{"Fire 1"            , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"Fire 2"            , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },

	{"Up (Cocktail)"     , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 up"     },
	{"Down (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 down"   },
	{"Left (Cocktail)"   , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 left"   },
	{"Right (Cocktail)"  , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 right"  },
	{"Fire 1 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"Fire 2 (Cocktail)" , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort2 + 6, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
};

STDINPUTINFO(Drv)

static inline void DrvMakeInputs()
{
	DrvInput[0] = DrvInput[1] = 0xff;
	DrvInput[2] = 0x40;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] -= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] -= (DrvInputPort1[i] & 1) << i;
	}

	if (DrvInputPort2[6]) DrvInput[2] -= 0x40;
}

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x12, 0xff, 0xff, 0xff, NULL                     },
	{0x13, 0xff, 0xff, 0xfb, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x12, 0x01, 0x0c, 0x00, "4 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x04, "3 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"       },

	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x12, 0x01, 0x03, 0x03, "1 Coin  2 Credits"      },
	{0x12, 0x01, 0x03, 0x02, "1 Coin  3 Credits"      },
	{0x12, 0x01, 0x03, 0x01, "1 Coin  4 Credits"      },
	{0x12, 0x01, 0x03, 0x00, "1 Coin  6 Credits"      },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x12, 0x01, 0x40, 0x00, "Off"                    },
	{0x12, 0x01, 0x40, 0x40, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Timer Speed"            },
	{0x13, 0x01, 0x03, 0x00, "Stopped"                },
	{0x13, 0x01, 0x03, 0x03, "Slow"                   },
	{0x13, 0x01, 0x03, 0x02, "Medium"                 },
	{0x13, 0x01, 0x03, 0x01, "Fast"                   },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x13, 0x01, 0x0c, 0x04, "2"                      },
	{0x13, 0x01, 0x0c, 0x08, "3"                      },
	{0x13, 0x01, 0x0c, 0x0c, "6"                      },
	{0x13, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x13, 0x01, 0x30, 0x30, "10k 60k 50k+"           },
	{0x13, 0x01, 0x30, 0x20, "20k 70k 50k+"           },
	{0x13, 0x01, 0x30, 0x10, "30k 80k 50k+"           },
	{0x13, 0x01, 0x30, 0x00, "20k 70k 50k+"           },
};

STDDIPINFO(Drv)

static struct BurnDIPInfo DrvjDIPList[]=
{
	// Default Values
	{0x12, 0xff, 0xff, 0xff, NULL                     },
	{0x13, 0xff, 0xff, 0xfb, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"       },
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"      },
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"      },

	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credit"       },
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credit"       },
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"      },
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"      },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x12, 0x01, 0x40, 0x00, "Off"                    },
	{0x12, 0x01, 0x40, 0x40, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Timer Speed"            },
	{0x13, 0x01, 0x03, 0x00, "Stopped"                },
	{0x13, 0x01, 0x03, 0x03, "Slow"                   },
	{0x13, 0x01, 0x03, 0x02, "Medium"                 },
	{0x13, 0x01, 0x03, 0x01, "Fast"                   },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x13, 0x01, 0x0c, 0x04, "2"                      },
	{0x13, 0x01, 0x0c, 0x08, "3"                      },
	{0x13, 0x01, 0x0c, 0x0c, "6"                      },
	{0x13, 0x01, 0x0c, 0x00, "Infinite"               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x13, 0x01, 0x30, 0x30, "10k 60k 50k+"           },
	{0x13, 0x01, 0x30, 0x20, "20k 70k 50k+"           },
	{0x13, 0x01, 0x30, 0x10, "30k 80k 50k+"           },
	{0x13, 0x01, 0x30, 0x00, "20k 70k 50k+"           },
};

STDDIPINFO(Drvj)

static struct BurnDIPInfo DrvbDIPList[]=
{
	// Default Values
	{0x12, 0xff, 0xff, 0xff, NULL                     },
	{0x13, 0xff, 0xff, 0xfb, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                 },
	{0x12, 0x01, 0x0c, 0x00, "4 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x04, "3 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 1 Credit"       },
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"       },

	{0   , 0xfe, 0   , 4   , "Coin B"                 },
	{0x12, 0x01, 0x03, 0x03, "1 Coin  2 Credits"      },
	{0x12, 0x01, 0x03, 0x02, "1 Coin  3 Credits"      },
	{0x12, 0x01, 0x03, 0x01, "1 Coin  4 Credits"      },
	{0x12, 0x01, 0x03, 0x00, "1 Coin  6 Credits"      },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x12, 0x01, 0x40, 0x00, "Off"                    },
	{0x12, 0x01, 0x40, 0x40, "On"                     },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Timer Speed"            },
	{0x13, 0x01, 0x03, 0x00, "Stopped"                },
	{0x13, 0x01, 0x03, 0x03, "Medium"                 },
	{0x13, 0x01, 0x03, 0x02, "Fast"                   },
	{0x13, 0x01, 0x03, 0x01, "Fastest"                },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x13, 0x01, 0x0c, 0x04, "2"                      },
	{0x13, 0x01, 0x0c, 0x08, "3"                      },
	{0x13, 0x01, 0x0c, 0x0c, "6"                      },
	{0x13, 0x01, 0x0c, 0x00, "Infinite"               },
};

STDDIPINFO(Drvb)

// Side Pocket (World)
/* DE-0245-2 */

static struct BurnRomInfo DrvRomDesc[] = {
	{ "dh00-e.3c",     0x10000, 0x251b316e, BRF_ESS | BRF_PRG }, 	//  0	M6809 Program Code

	{ "dh04.3h",       0x08000, 0xd076e62e, BRF_ESS | BRF_PRG }, 	//  1	M6502 Program Code

	{ "dh-e.6d",       0x01000, 0x00654574, BRF_PRG }, 				//  2	I8751 microcontroller

	{ "dh07-e.13k",    0x08000, 0x9d6f7969, BRF_GRA },	    		//  3	Chars
	{ "dh06-e.13j",    0x08000, 0x580e4e43, BRF_GRA },	     		//  4
	{ "dh05-e.13h",    0x08000, 0x05ab71d2, BRF_GRA },	     		//  5

	{ "dh01.14a",      0x08000, 0xa2cdfbea, BRF_GRA },	     		//  6	Sprites
	{ "dh02.15a",      0x08000, 0xeeb5c3e7, BRF_GRA },	     		//  7
	{ "dh03.17a",      0x08000, 0x8e18d21d, BRF_GRA },	     		//  8

	{ "dh-09.16l",     0x00100, 0xce049b4f, BRF_GRA },	     		//  9	PROMS
	{ "dh-08.15l",     0x00100, 0xcdf2180f, BRF_GRA },	     		// 10
};

STD_ROM_PICK(Drv)
STD_ROM_FN(Drv)


// Side Pocket (Japan, Cocktail)
/* DE-0245-1 */

static struct BurnRomInfo DrvjRomDesc[] = {
	{ "dh00-1.3c",     0x10000, 0xa66bc28d, BRF_ESS | BRF_PRG }, 	//  0	M6809 Program Code

	{ "dh04_6-19.3h",  0x08000, 0x053ff83a, BRF_ESS | BRF_PRG }, 	//  1	M6502 Program Code

	{ "dh.6d",         0x01000, 0xf7e099b6, BRF_PRG }, 				//  2	I8751 MCU

	{ "dh07.13k",      0x08000, 0x7d0ce858, BRF_GRA },	    		//  3	Chars
	{ "dh06.13j",      0x08000, 0xb86ddf72, BRF_GRA },	     		//  4
	{ "dh05.13h",      0x08000, 0xdf6f94f2, BRF_GRA },	     		//  5

	{ "dh01.14a",      0x08000, 0xa2cdfbea, BRF_GRA },	     		//  6	Sprites
	{ "dh02.15a",      0x08000, 0xeeb5c3e7, BRF_GRA },	     		//  7
	{ "dh03.17a",      0x08000, 0x8e18d21d, BRF_GRA },	     		//  8

	{ "dh-09.16l",     0x00100, 0xce049b4f, BRF_GRA },	     		//  9	PROMS
	{ "dh-08.15l",     0x00100, 0xcdf2180f, BRF_GRA },	     		// 10
};

STD_ROM_PICK(Drvj)
STD_ROM_FN(Drvj)


// Side Pocket (bootleg, set 1)

static struct BurnRomInfo DrvbRomDesc[] = {
	{ "sp_09.bin",     0x04000, 0x3c6fe54b, BRF_ESS | BRF_PRG }, 	//  0	M6809 Program Code
	{ "sp_08.bin",     0x08000, 0x347f81cd, BRF_ESS | BRF_PRG }, 	//  1

	{ "dh04.3h",       0x08000, 0xd076e62e, BRF_ESS | BRF_PRG }, 	//  2	M6502 Program Code

	{ "dh07-e.13k",    0x08000, 0x9d6f7969, BRF_GRA },	    		//  3	Chars
	{ "dh06-e.13j",    0x08000, 0x580e4e43, BRF_GRA },	     		//  4
	{ "dh05-e.13h",    0x08000, 0x05ab71d2, BRF_GRA },	     		//  5

	{ "dh01.14a",      0x08000, 0xa2cdfbea, BRF_GRA },	     		//  6	Sprites
	{ "dh02.15a",      0x08000, 0xeeb5c3e7, BRF_GRA },	     		//  7
	{ "dh03.17a",      0x08000, 0x8e18d21d, BRF_GRA },	     		//  8

	{ "dh-09.16l",     0x00100, 0xce049b4f, BRF_GRA },	     		//  9	PROMS
	{ "dh-08.15l",     0x00100, 0xcdf2180f, BRF_GRA },	     		// 10
};

STD_ROM_PICK(Drvb)
STD_ROM_FN(Drvb)


// Side Pocket (bootleg, set 2)

static struct BurnRomInfo Drvb2RomDesc[] = {
	{ "b-9.2a",        0x04000, 0x40fd0d85, BRF_ESS | BRF_PRG }, 	//  0	M6809 Program Code
	{ "b-8.3a",        0x08000, 0x26e0116a, BRF_ESS | BRF_PRG }, 	//  1

	{ "b-4.7a",        0x08000, 0xd076e62e, BRF_ESS | BRF_PRG }, 	//  2	M6502 Program Code

	{ "b-7.9m",        0x08000, 0x9d6f7969, BRF_GRA },	    		//  3	Chars
	{ "b-6.8m",        0x08000, 0x580e4e43, BRF_GRA },	     		//  4
	{ "b-5.7m",        0x08000, 0x05ab71d2, BRF_GRA },	     		//  5

	{ "b-1.1p",        0x08000, 0xa2cdfbea, BRF_GRA },	     		//  6	Sprites
	{ "b-2.1r",        0x08000, 0xeeb5c3e7, BRF_GRA },	     		//  7
	{ "b-3.1t",        0x08000, 0x8e18d21d, BRF_GRA },	     		//  8

	{ "dh-09.bpr",     0x00100, 0xce049b4f, BRF_GRA },	     		//  9	PROMS
	{ "dh-08.bpr",     0x00100, 0xcdf2180f, BRF_GRA },	     		// 10
};

STD_ROM_PICK(Drvb2)
STD_ROM_FN(Drvb2)

// i8751 MCU (sidepckt)
static void DrvMCUReset(); // forward
static void DrvMCUSync(); // ""

static UINT8 mcu_read_port(INT32 port)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return 0xff;

	switch (port & 0x3) {
		case 2: return i8751PortData[2];
	}

	return 0xff;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (!(port >= MCS51_PORT_P0 && port <= MCS51_PORT_P3))
		return;

	port &= 0x3;

	if (port == 3)
	{
		if (~data & 0x1 && i8751PortData[3] & 0x1) {
			M6809SetIRQLine(M6809_FIRQ_LINE, CPU_IRQSTATUS_ACK);
		}

		if (data & 0x1 && ~i8751PortData[3] & 0x1) {
			M6809SetIRQLine(M6809_FIRQ_LINE, CPU_IRQSTATUS_NONE);
		}

		if (~data & 0x2 && i8751PortData[3] & 0x2) {
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
		}
	}

	i8751PortData[port] = data;
}

static void DrvMCUInit()
{
	mcs51_init();
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);
}

static void DrvMCUExit()
{
	if (realMCU) {
		mcs51_exit();
		realMCU = 0;
	}
}

static INT32 DrvMCURun(INT32 cycles)
{
	return mcs51Run(cycles);
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);

	SCAN_VAR(i8751PortData);

	return 0;
}

static void DrvMCUWrite(UINT16 data)
{
	if (realMCU == 0) return;

	DrvMCUSync();

	i8751PortData[2] = data;

	mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_HOLD);
}

static UINT8 DrvMCURead()
{
	if (realMCU == 0) return 0xff;

	DrvMCUSync();

	return i8751PortData[1];
}

static void DrvMCUSync()
{
	INT32 todo = ((double)M6809TotalCycles() * (8000000/12) / 2000000) - mcs51TotalCycles();

	if (todo > 0) {
		DrvMCURun(todo);
	}
}

static void DrvMCUReset()
{
	if (realMCU == 0) return;

	memset(&i8751PortData, 0, sizeof(i8751PortData));
	mcs51_reset();
}

static UINT8 SidepcktM6809ReadByte(UINT16 Address)
{
	switch (Address) {
		case 0x3000: {
			return DrvInput[0];
		}

		case 0x3001: {
			return DrvInput[1];
		}

		case 0x3002: {
			return DrvDip[0];
		}

		case 0x3003: {
			return DrvDip[1] | DrvInput[2];
		}

		case 0x300c: {
			// nop
			return 0;
		}

		case 0x3014: {
			return DrvMCURead();
		}

		default: {
			bprintf(PRINT_NORMAL, _T("M6809 Read Byte %04X\n"), Address);
		}
	}

	return 0;
}

static void SidepcktM6809WriteByte(UINT16 Address, UINT8 Data)
{
	switch (Address) {
		case 0x3004: {
			DrvSoundLatch = Data;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
			return;
		}

		case 0x300c: {
			// flipscreen
			return;
		}

		case 0x3018: {
			DrvMCUWrite(Data);
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("M6809 Write Byte %04X, %02X\n"), Address, Data);
		}
	}
}

static UINT8 SidepcktSoundReadByte(UINT16 Address)
{
	switch (Address) {
		case 0x3000: {
			return DrvSoundLatch;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Read Byte %04X\n"), Address);
		}
	}

	return 0;
}

static void SidepcktSoundWriteByte(UINT16 Address, UINT8 Data)
{
	switch (Address) {
		case 0x1000: {
			BurnYM2203Write(0, 0, Data);
			return;
		}

		case 0x1001: {
			BurnYM2203Write(0, 1, Data);
			return;
		}

		case 0x2000: {
			BurnYM3526Write(0, Data);
			return;
		}

		case 0x2001: {
			BurnYM3526Write(1, Data);
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("M6502 Write Byte %04X, %02X\n"), Address, Data);
		}
	}
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	M6502SetIRQLine(M6502_IRQ_LINE, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvM6809Rom            = Next; Next += 0x010000;
	DrvM6502Rom            = Next; Next += 0x008000;
	DrvMCUROM              = Next; Next += 0x001000;
	DrvProm                = Next; Next += 0x000200;

	RamStart               = Next;

	DrvM6809Ram            = Next; Next += 0x001c00;
	DrvM6502Ram            = Next; Next += 0x001000;
	DrvVideoRam            = Next; Next += 0x000400;
	DrvColourRam           = Next; Next += 0x000400;
	DrvSpriteRam           = Next; Next += 0x000100;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x0800 * 8 * 8;
	DrvSprites             = Next; Next += 0x0400 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	M6809Open(0);
	M6809Reset();
	BurnYM2203Reset();
	DrvMCUReset();
	M6809Close();

	M6502Open(0);
	M6502Reset();
	BurnYM3526Reset();
	M6502Close();

	DrvSoundLatch = 0;

	HiscoreReset();

	return 0;
}

static INT32 CharPlaneOffsets[3]   = { 0, 0x40000, 0x80000 };
static INT32 CharXOffsets[8]       = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 CharYOffsets[8]       = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 SpritePlaneOffsets[3] = { 0, 0x40000, 0x80000 };
static INT32 SpriteXOffsets[16]    = { 128, 129, 130, 131, 132, 133, 134, 135, 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 SpriteYOffsets[16]    = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

static INT32 SidepcktLoadRoms()
{
	INT32 nRet;

	nRet = BurnLoadRom(DrvM6809Rom + 0x00000,  0, 1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvM6502Rom + 0x00000,  1, 1); if (nRet != 0) return 1;

	realMCU = 1;
	nRet = BurnLoadRom(DrvMCUROM   + 0x00000,  2, 1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvTempRom  + 0x00000,  3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x08000,  4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x10000,  5, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 3, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);

	memset(DrvTempRom, 0, 0x18000);
	nRet = BurnLoadRom(DrvTempRom  + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x08000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x10000,  8, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 3, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x100, DrvTempRom, DrvSprites);

	nRet = BurnLoadRom(DrvProm     + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProm     + 0x00100, 10, 1); if (nRet != 0) return 1;

	return 0;
}

static INT32 SidepcktbLoadRoms()
{
	INT32 nRet;

	nRet = BurnLoadRom(DrvM6809Rom + 0x04000,  0, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvM6809Rom + 0x08000,  1, 1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvM6502Rom + 0x00000,  2, 1); if (nRet != 0) return 1;

	nRet = BurnLoadRom(DrvTempRom  + 0x00000,  3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x08000,  4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x10000,  5, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 3, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvChars);

	memset(DrvTempRom, 0, 0x18000);
	nRet = BurnLoadRom(DrvTempRom  + 0x00000,  6, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x08000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom  + 0x10000,  8, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 3, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x100, DrvTempRom, DrvSprites);

	nRet = BurnLoadRom(DrvProm     + 0x00000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProm     + 0x00100, 10, 1); if (nRet != 0) return 1;

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

	BurnSetRefreshRate(58.0);

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x18000);

	realMCU = 0; // set in romloadfunction!

	if (LoadRomsFunction()) return 1;

	BurnFree(DrvTempRom);

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809Ram + 0x0000, 0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvVideoRam         , 0x1000, 0x13ff, MAP_RAM);
	M6809MapMemory(DrvM6809Ram + 0x1000, 0x1400, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvColourRam        , 0x1800, 0x1bff, MAP_RAM);
	M6809MapMemory(DrvM6809Ram + 0x1400, 0x1c00, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvSpriteRam        , 0x2000, 0x20ff, MAP_RAM);
	M6809MapMemory(DrvM6809Ram + 0x1800, 0x2100, 0x24ff, MAP_RAM);
	M6809MapMemory(DrvM6809Rom + 0x4000, 0x4000, 0xffff, MAP_ROM);
	M6809SetReadHandler(SidepcktM6809ReadByte);
	M6809SetWriteHandler(SidepcktM6809WriteByte);
	M6809Close();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502Ram            , 0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvM6502Rom            , 0x8000, 0xffff, MAP_ROM);
	M6502SetReadHandler(SidepcktSoundReadByte);
	M6502SetWriteHandler(SidepcktSoundWriteByte);
	M6502Close();

	if (realMCU) {
		DrvMCUInit();
	}

	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnTimerAttachM6809(2000000);
	BurnYM2203SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	BurnYM3526Init(3000000, &DrvFMIRQHandler, 1);
	BurnTimerAttachYM3526(&M6502Config, 1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 SidepcktInit()
{
	LoadRomsFunction = SidepcktLoadRoms;

	return DrvInit();
}

static INT32 SidepcktjInit()
{
	LoadRomsFunction = SidepcktLoadRoms;

	return DrvInit();
}

static INT32 SidepcktbInit()
{
	LoadRomsFunction = SidepcktbLoadRoms;

	return DrvInit();
}

static INT32 DrvExit()
{
	M6809Exit();
	M6502Exit();

	DrvMCUExit();

	BurnYM2203Exit();
	BurnYM3526Exit();

	GenericTilesExit();

	BurnFree(Mem);

	DrvSoundLatch = 0;

	LoadRomsFunction = NULL;

	return 0;
}

static void DrvCalcPalette()
{
	for (INT32 i = 0; i < 0x100; i++) {
		INT32 Bit0, Bit1, Bit2, Bit3, r, g, b;

		Bit0 = (DrvProm[i] >> 4) & 0x01;
		Bit1 = (DrvProm[i] >> 5) & 0x01;
		Bit2 = (DrvProm[i] >> 6) & 0x01;
		Bit3 = (DrvProm[i] >> 7) & 0x01;
		r = 0x0e * Bit0 + 0x1f * Bit1 + 0x43 * Bit2 + 0x8f * Bit3;

		Bit0 = (DrvProm[i] >> 0) & 0x01;
		Bit1 = (DrvProm[i] >> 1) & 0x01;
		Bit2 = (DrvProm[i] >> 2) & 0x01;
		Bit3 = (DrvProm[i] >> 3) & 0x01;
		g = 0x0e * Bit0 + 0x1f * Bit1 + 0x43 * Bit2 + 0x8f * Bit3;

		Bit0 = (DrvProm[i + 0x100] >> 0) & 0x01;
		Bit1 = (DrvProm[i + 0x100] >> 1) & 0x01;
		Bit2 = (DrvProm[i + 0x100] >> 2) & 0x01;
		Bit3 = (DrvProm[i + 0x100] >> 3) & 0x01;
		b = 0x0e * Bit0 + 0x1f * Bit1 + 0x43 * Bit2 + 0x8f * Bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void DrvRenderBgLayer(INT32 RenderCategory)
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex = 0, Category;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 32; mx++) {
			Attr = DrvColourRam[TileIndex];
			Code = (DrvVideoRam[TileIndex] + ((Attr & 0x07) << 8)) & 0x7ff;
			Colour = ((Attr & 0x10) >> 3) | ((Attr & 0x20) >> 5);
			Category = (Attr & 0x80) >> 7;

			if (Category == RenderCategory) {
				x = 8 * mx;
				y = 8 * my;

				y -= 16;

				x = 248 - x;

				if (RenderCategory) {
					if (x > 8 && x < 248 && y > 8 && y < 216) {
						Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 3, 0, 128, DrvChars);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 3, 0, 128, DrvChars);
					}
				} else {
					if (x > 8 && x < 248 && y > 8 && y < 216) {
						Render8x8Tile(pTransDraw, Code, x, y, Colour, 3, 128, DrvChars);
					} else {
						Render8x8Tile_Clip(pTransDraw, Code, x, y, Colour, 3, 128, DrvChars);
					}
				}
			}

			TileIndex++;
		}
	}
}

static void DrawSprites()
{
	for (INT32 Offs = 0; Offs < 0x100; Offs += 4) {
		INT32 sx, sy, Code, Colour, xFlip, yFlip;

		Code = (DrvSpriteRam[Offs + 3] + ((DrvSpriteRam[Offs + 1] & 0x03) << 8)) & 0x3ff;
		Colour = (DrvSpriteRam[Offs + 1] & 0xf0) >> 4;

		sx = DrvSpriteRam[Offs + 2] - 2;
		sy = DrvSpriteRam[Offs];

		xFlip = DrvSpriteRam[Offs + 1] & 0x08;
		yFlip = DrvSpriteRam[Offs + 1] & 0x04;

		sy -= 16;

		if (xFlip) {
			if (yFlip) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 0, DrvSprites);
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, Code, sx - 256, sy, Colour, 3, 0, 0, DrvSprites);
			} else {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 0, DrvSprites);
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, Code, sx - 256, sy, Colour, 3, 0, 0, DrvSprites);
			}
		} else {
			if (yFlip) {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 0, DrvSprites);
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, Code, sx - 256, sy, Colour, 3, 0, 0, DrvSprites);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, Code, sx, sy, Colour, 3, 0, 0, DrvSprites);
				Render16x16Tile_Mask_Clip(pTransDraw, Code, sx - 256, sy, Colour, 3, 0, 0, DrvSprites);
			}
		}
	}
}

static INT32 DrvDraw()
{
	BurnTransferClear();
	DrvCalcPalette();
	if (nBurnLayer & 0x01) DrvRenderBgLayer(0);
	if (nSpriteEnable & 0x02) DrawSprites();
	if (nBurnLayer & 0x04) DrvRenderBgLayer(1);
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	INT32 nCyclesTotal[3] = { 2000000 / 58, 1500000 / 58, 8000000 / 12 / 58 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nInterleave = 100;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	M6809NewFrame();
	M6502NewFrame();
	mcs51NewFrame();

	M6809Open(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		BurnTimerUpdate(nNext);
		if (i == (nInterleave - 1)) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		BurnTimerUpdateYM3526(nNext);

		if (realMCU) {
			CPU_RUN(2, DrvMCU);
		}
	}

	BurnTimerEndFrame(nCyclesTotal[0]);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	M6502Close();

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		M6502Scan(nAction);

		if (realMCU) {
			DrvMCUScan(nAction);
		}

		BurnYM2203Scan(nAction, pnMin);
		BurnYM3526Scan(nAction, pnMin);

		SCAN_VAR(DrvSoundLatch);
	}

	return 0;
}

struct BurnDriver BurnDrvSidepckt = {
	"sidepckt", NULL, NULL, NULL, "1986",
	"Side Pocket (World)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, DrvRomInfo, DrvRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SidepcktInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSidepcktj = {
	"sidepcktj", "sidepckt", NULL, NULL, "1986",
	"Side Pocket (Japan, Cocktail)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, DrvjRomInfo, DrvjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvjDIPInfo,
	SidepcktjInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSidepcktb = {
	"sidepcktb", "sidepckt", NULL, NULL, "1986",
	"Side Pocket (bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, DrvbRomInfo, DrvbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvbDIPInfo,
	SidepcktbInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};

struct BurnDriver BurnDrvSidepcktb2 = {
	"sidepcktb2", "sidepckt", NULL, NULL, "1986",
	"Side Pocket (bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, Drvb2RomInfo, Drvb2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SidepcktbInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	NULL, 0x100, 256, 224, 4, 3
};
