// FB Alpha SNK / Alpha 68k based driver module
// Based on MAME driver by Pierpaolo Prazzoli, Bryan McPhail,Stephane Humbert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "dac.h"

static UINT8 DrvInputPort0[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort1[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvDip[2]        = { 0, 0 };
static UINT8 DrvInput[3]      = { 0, 0, 0 };
static UINT8 DrvReset         = 0;

static UINT8 *Mem                 = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *Drv68KRom           = NULL;
static UINT8 *Drv68KRam           = NULL;
static UINT8 *DrvZ80Rom           = NULL;
static UINT8 *DrvZ80Ram           = NULL;
static UINT8 *DrvProms            = NULL;
static UINT8 *DrvColourProm       = NULL;
static UINT8 *DrvVideoRam         = NULL;
static UINT8 *DrvSpriteRam        = NULL;
static UINT8 *DrvSharedRam        = NULL;
static UINT8 *DrvGfxData[8]       = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette         = NULL;

static UINT32 DrvCredits = 0;
static UINT32 DrvTrigState = 0;
static UINT32 DrvDeposits1 = 0;
static UINT32 DrvDeposits2 = 0;
static UINT32 DrvCoinValue = 0;
static UINT32 DrvMicroControllerData = 0;
static INT32  DrvLatch = 0;
static INT32  DrvCoinID = 0;
static INT32 DrvMicroControllerID = 0;
static INT32 DrvFlipScreen = 0;
static UINT8 DrvSoundLatch = 0;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

static INT32 nDrvTotal68KCycles = 0;
static INT32 nDrvTotalZ80Cycles = 0;

typedef void (*DrvRender)();
static DrvRender DrvDrawFunction = NULL;
static void SstingryDraw();
static void KyrosDraw();

static struct BurnInputInfo SstingryInputList[] =
{
	{"Coin 1"            , BIT_DIGITAL  , DrvInputPort2 + 0, "p1 coin"   },
	{"Start 1"           , BIT_DIGITAL  , DrvInputPort0 + 7, "p1 start"  },
	{"Coin 2"            , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 coin"   },
	{"Start 2"           , BIT_DIGITAL  , DrvInputPort1 + 7, "p2 start"  },
	
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort0 + 2, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "p1 fire 2" },
	
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort1 + 0, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort1 + 1, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort1 + 2, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
};

STDINPUTINFO(Sstingry)

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x03) {
		*nJoystickInputs &= ~0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs &= ~0x0c;
	}
}

static inline void DrvMakeInputs()
{
	DrvInput[0] = DrvInput[1] = 0x00;
	DrvInput[2] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] -= (DrvInputPort2[i] & 1) << i;
	}

	DrvClearOpposites(&DrvInput[0]);
	DrvClearOpposites(&DrvInput[1]);
}

static struct BurnDIPInfo SstingryDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0x0f, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x11, 0x01, 0x01, 0x00, "Off"                    },
	{0x11, 0x01, 0x01, 0x01, "On"                     },
	
	{0   , 0xfe, 0   , 8   , "Coinage"                },
	{0x11, 0x01, 0x0e, 0x0e, "A 1C/1C B 1C/1C"        },
	{0x11, 0x01, 0x0e, 0x06, "A 1C/2C B 2C/1C"        },
	{0x11, 0x01, 0x0e, 0x0a, "A 1C/3C B 3C/1C"        },
	{0x11, 0x01, 0x0e, 0x02, "A 1C/4C B 4C/1C"        },
	{0x11, 0x01, 0x0e, 0x0c, "A 1C/5C B 5C/1C"        },
	{0x11, 0x01, 0x0e, 0x04, "A 1C/6C B 6C/1C"        },
	{0x11, 0x01, 0x0e, 0x08, "A 2C/3C B 7C/1C"        },
	{0x11, 0x01, 0x0e, 0x00, "A 3C/2C B 8C/1C"        },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x11, 0x01, 0x30, 0x00, "3"                      },
	{0x11, 0x01, 0x30, 0x10, "4"                      },
	{0x11, 0x01, 0x30, 0x20, "5"                      },
	{0x11, 0x01, 0x30, 0x30, "6"                      },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x11, 0x01, 0x80, 0x00, "Upright"                },
	{0x11, 0x01, 0x80, 0x80, "Cocktail"               },
};

STDDIPINFO(Sstingry)

static struct BurnDIPInfo KyrosDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0x8e, NULL                     },

	// Dip 1
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x11, 0x01, 0x01, 0x01, "Off"                    },
	{0x11, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 8   , "Coinage"                },
	{0x11, 0x01, 0x0e, 0x0e, "A 1C/1C B 1C/1C"        },
	{0x11, 0x01, 0x0e, 0x06, "A 1C/2C B 2C/1C"        },
	{0x11, 0x01, 0x0e, 0x0a, "A 1C/3C B 3C/1C"        },
	{0x11, 0x01, 0x0e, 0x02, "A 1C/4C B 4C/1C"        },
	{0x11, 0x01, 0x0e, 0x0c, "A 1C/5C B 5C/1C"        },
	{0x11, 0x01, 0x0e, 0x04, "A 1C/6C B 6C/1C"        },
	{0x11, 0x01, 0x0e, 0x08, "A 2C/3C B 7C/1C"        },
	{0x11, 0x01, 0x0e, 0x00, "A 3C/2C B 8C/1C"        },
	
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x11, 0x01, 0x30, 0x00, "3"                      },
	{0x11, 0x01, 0x30, 0x10, "4"                      },
	{0x11, 0x01, 0x30, 0x20, "5"                      },
	{0x11, 0x01, 0x30, 0x30, "6"                      },
	
	{0   , 0xfe, 0   , 2   , "Difficulty"             },
	{0x11, 0x01, 0x40, 0x00, "Easy"                   },
	{0x11, 0x01, 0x40, 0x40, "Hard"                   },
	
	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x11, 0x01, 0x80, 0x80, "Upright"                },
	{0x11, 0x01, 0x80, 0x00, "Cocktail"               },
};

STDDIPINFO(Kyros)

static struct BurnRomInfo SstingryRomDesc[] = {
	{ "ss_05.rom",     0x004000, 0xbfb28d53, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "ss_07.rom",     0x004000, 0xeb1b65c5, BRF_ESS | BRF_PRG }, //  1
	{ "ss_04.rom",     0x004000, 0x2e477a79, BRF_ESS | BRF_PRG }, //  2
	{ "ss_06.rom",     0x004000, 0x597620cb, BRF_ESS | BRF_PRG }, //  3
	
	{ "ss_01.rom",     0x004000, 0xfef09a92, BRF_ESS | BRF_PRG }, //  4	Z80 Program
	{ "ss_02.rom",     0x004000, 0xab4e8c01, BRF_ESS | BRF_PRG }, //  5
	
	{ "d8748.bin",     0x000400, 0x7fcbfc30, BRF_OPT | BRF_PRG }, //  6 MCU (not used)
	
	{ "ss_12.rom",     0x004000, 0x74caa9e9, BRF_GRA },	      //  7	Sprites
	{ "ss_08.rom",     0x004000, 0x32368925, BRF_GRA },	      //  8
	{ "ss_13.rom",     0x004000, 0x13da6203, BRF_GRA },	      //  9
	{ "ss_10.rom",     0x004000, 0x2903234a, BRF_GRA },	      //  10
	{ "ss_11.rom",     0x004000, 0xd134302e, BRF_GRA },	      //  11
	{ "ss_09.rom",     0x004000, 0x6f9d938a, BRF_GRA },	      //  12
	
	{ "ic92",          0x000100, 0xe7ce1179, BRF_GRA },	      //  13	PROMs
	{ "ic93",          0x000100, 0x9af8a375, BRF_GRA },	      //  14
	{ "ic91",          0x000100, 0xc3965079, BRF_GRA },	      //  15
	{ "ssprom2.bin",   0x000100, 0xc2205b71, BRF_GRA },	      //  16
	{ "ssprom1.bin",   0x000100, 0x1003186c, BRF_GRA },	      //  17
};

STD_ROM_PICK(Sstingry)
STD_ROM_FN(Sstingry)

static struct BurnRomInfo KyrosRomDesc[] = {
	{ "2.10c",         0x008000, 0x4bd030b1, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "1.13c",         0x008000, 0x75cfbc5e, BRF_ESS | BRF_PRG }, //  1
	{ "4.10b",         0x008000, 0xbe2626c2, BRF_ESS | BRF_PRG }, //  2
	{ "3.13b",         0x008000, 0xfb25e71a, BRF_ESS | BRF_PRG }, //  3
	
	{ "2s.1f",         0x004000, 0x800ceb27, BRF_ESS | BRF_PRG }, //  4	Z80 Program
	{ "1s.1d",         0x008000, 0x87d3e719, BRF_ESS | BRF_PRG }, //  5
	
	{ "kyros_68705u3.bin", 0x001000, 0xc20880b7, BRF_OPT | BRF_PRG }, //  6 MCU (not used)
	{ "kyros_mcu.bin", 0x000800, 0x3a902a19, BRF_OPT | BRF_PRG }, //  7
	
	{ "8.9pr",         0x008000, 0xc5290944, BRF_GRA },	      //  8	Graphics Data
	{ "11.11m",        0x008000, 0xfbd44f1e, BRF_GRA },	      //  9
	{ "12.11n",        0x008000, 0x10fed501, BRF_GRA },	      //  10
	{ "9.9s",          0x008000, 0xdd40ca33, BRF_GRA },	      //  11
	{ "13.11p",        0x008000, 0xe6a02030, BRF_GRA },	      //  12
	{ "14.11r",        0x008000, 0x722ad23a, BRF_GRA },	      //  13
	{ "15.3t",         0x008000, 0x045fdda4, BRF_GRA },	      //  14
	{ "17.7t",         0x008000, 0x7618ec00, BRF_GRA },	      //  15
	{ "18.9t",         0x008000, 0x0ee74171, BRF_GRA },	      //  16
	{ "16.5t",         0x008000, 0x2cf14824, BRF_GRA },	      //  17
	{ "19.11t",        0x008000, 0x4f336306, BRF_GRA },	      //  18
	{ "20.13t",        0x008000, 0xa165d06b, BRF_GRA },	      //  19
	
	{ "mb7114l.5r",    0x000100, 0x3628bf36, BRF_GRA },	      //  20	PROMs
	{ "mb7114l.4r",    0x000100, 0x850704e4, BRF_GRA },	      //  21
	{ "mb7114l.6r",    0x000100, 0xa54f60d7, BRF_GRA },	      //  22
	{ "mb7114l.5p",    0x000100, 0x1cc53765, BRF_GRA },	      //  23
	{ "mb7114l.6p",    0x000100, 0xb0d6971f, BRF_GRA },	      //  24
	
	{ "0.1t",          0x002000, 0x5d0acb4c, BRF_GRA },	      //  25	Colour PROM
};

STD_ROM_PICK(Kyros)
STD_ROM_FN(Kyros)

static struct BurnRomInfo KyrosjRomDesc[] = {
	{ "2j.10c",        0x008000, 0xb324c11b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "1j.13c",        0x008000, 0x8496241b, BRF_ESS | BRF_PRG }, //  1
	{ "4.10a",         0x008000, 0x0187f59d, BRF_ESS | BRF_PRG }, //  2
	{ "3.13a",         0x008000, 0xab97941d, BRF_ESS | BRF_PRG }, //  3
	
	{ "2s.1f",         0x004000, 0x800ceb27, BRF_ESS | BRF_PRG }, //  4	Z80 Program
	{ "1s.1d",         0x008000, 0x87d3e719, BRF_ESS | BRF_PRG }, //  5
	
	{ "kyros_68705u3.bin", 0x001000, 0xc20880b7, BRF_OPT | BRF_PRG }, //  6 MCU (not used)
	{ "kyros_mcu.bin", 0x000800, 0x3a902a19, BRF_OPT | BRF_PRG }, //  7
	
	{ "8.9r",          0x008000, 0xd8203284, BRF_GRA },	      //  8	Graphics Data
	{ "11.12m",        0x008000, 0xa2f9738c, BRF_GRA },	      //  9
	{ "12.11n",        0x008000, 0x10fed501, BRF_GRA },	      //  10
	{ "9j.9s",         0x008000, 0x3e725349, BRF_GRA },	      //  11
	{ "13.11p",        0x008000, 0xe6a02030, BRF_GRA },	      //  12
	{ "14.12r",        0x008000, 0x39d07db9, BRF_GRA },	      //  13
	{ "15.3t",         0x008000, 0x045fdda4, BRF_GRA },	      //  14
	{ "17.7t",         0x008000, 0x7618ec00, BRF_GRA },	      //  15
	{ "18.9t",         0x008000, 0x0ee74171, BRF_GRA },	      //  16
	{ "16j.5t",        0x008000, 0xe1566679, BRF_GRA },	      //  17
	{ "19.11t",        0x008000, 0x4f336306, BRF_GRA },	      //  18
	{ "20j.13t",       0x008000, 0x0624b4c0, BRF_GRA },	      //  19
	
	{ "mb7114l.5r",    0x000100, 0x3628bf36, BRF_GRA },	      //  20	PROMs
	{ "mb7114l.4r",    0x000100, 0x850704e4, BRF_GRA },	      //  21
	{ "mb7114l.6r",    0x000100, 0xa54f60d7, BRF_GRA },	      //  22
	{ "mb7114l.5p",    0x000100, 0x1cc53765, BRF_GRA },	      //  23
	{ "mb7114l.6p",    0x000100, 0xb0d6971f, BRF_GRA },	      //  24
	
	{ "0j.1t",         0x002000, 0xa34ecb29, BRF_OPT },	      //  25
};

STD_ROM_PICK(Kyrosj)
STD_ROM_FN(Kyrosj)

static INT32 SstingryMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x20000;
	DrvZ80Rom              = Next; Next += 0x08000;
	DrvProms               = Next; Next += 0x00500;

	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x04000;
	DrvZ80Ram              = Next; Next += 0x00800;
	DrvVideoRam            = Next; Next += 0x01000;
	DrvSharedRam           = Next; Next += 0x01000;
	DrvSpriteRam           = Next; Next += 0x02000;

	RamEnd                 = Next;

	DrvGfxData[0]          = Next; Next += 0x00400 * 8 * 8;
	DrvGfxData[1]          = Next; Next += 0x00400 * 8 * 8;
	DrvGfxData[2]          = Next; Next += 0x00400 * 8 * 8;
	DrvGfxData[3]          = Next; Next += 0x00400 * 8 * 8;
	DrvPalette             = (UINT32*)Next; Next += 0x00101 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 KyrosMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x20000;
	DrvZ80Rom              = Next; Next += 0x0c000;
	DrvProms               = Next; Next += 0x00500;
	DrvColourProm          = Next; Next += 0x02000;

	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x04000;
	DrvZ80Ram              = Next; Next += 0x00800;
	DrvVideoRam            = Next; Next += 0x01000;
	DrvSharedRam           = Next; Next += 0x01000;
	DrvSpriteRam           = Next; Next += 0x02000;

	RamEnd                 = Next;

	DrvGfxData[0]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[1]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[2]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[3]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[4]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[5]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[6]          = Next; Next += 0x00800 * 8 * 8;
	DrvGfxData[7]          = Next; Next += 0x00800 * 8 * 8;
	DrvPalette             = (UINT32*)Next; Next += 0x00101 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnYM2203Reset();
	DACReset();
	
	DrvCredits = 0;
	DrvTrigState = 0;
	DrvDeposits1 = 0;
	DrvDeposits2 = 0;
	DrvCoinValue = 0;
	DrvMicroControllerData = 0;
	DrvLatch = 0;
	DrvFlipScreen = 0;
	DrvSoundLatch = 0;
	
	return 0;
}

static UINT16 kyros_alpha_trigger_r(UINT32 Offset)
{
	static const UINT8 coinage1[8][2] = {{1,1}, {1,5}, {1,3}, {2,3}, {1,2}, {1,6}, {1,4}, {3,2}};
	static const UINT8 coinage2[8][2] = {{1,1}, {5,1}, {3,1}, {7,1}, {2,1}, {6,1}, {4,1}, {8,1}};
	UINT16 * RAM = (UINT16*)DrvSharedRam;
	INT32 Source = RAM[Offset];
	
//	bprintf(PRINT_NORMAL, _T("kyros_alpha_trigger_r %x\n"), Offset);

	switch (Offset)	{
		case 0x22: {
			RAM[0x22] = (Source & 0xff00) | (DrvCredits & 0x00ff);
			return 0;
		}
	
		case 0x29: {
			DrvTrigState++;
			if ((DrvInput[2] & 0x03) == 0x03) DrvLatch = 0;
			
			if ((DrvInput[2] & 0x01) == 0x00 && !DrvLatch)	{
				RAM[0x29] = (Source & 0xff00) | (DrvCoinID & 0xff);
				RAM[0x22] = (Source & 0xff00) | 0x00;
				DrvLatch = 1;

				DrvCoinValue = (~DrvDip[0] >> 1) & 0x07;
				DrvDeposits1++;
				if (DrvDeposits1 == coinage1[DrvCoinValue][0]) {
					DrvCredits = coinage1[DrvCoinValue][1];
					DrvDeposits1 = 0;
				} else {
					DrvCredits = 0;
				}
			} else if ((DrvInput[2] & 0x02) == 0x00 && !DrvLatch) {
				RAM[0x29] = (Source & 0xff00) | (DrvCoinID >> 8);
				RAM[0x22] = (Source & 0xff00) | 0x0;
				DrvLatch = 1;

				DrvCoinValue = (~DrvDip[0] >> 1) & 0x07;
				DrvDeposits2++;
				if (DrvDeposits2 == coinage2[DrvCoinValue][0]) {
					DrvCredits = coinage2[DrvCoinValue][1];
					DrvDeposits2 = 0;
				} else {
					DrvCredits = 0;
				}
			} else {
				if (DrvMicroControllerID == 0x00ff) {
					if (DrvTrigState >= 12 /*|| m_game_id == ALPHA68K_JONGBOU*/) {
						DrvTrigState = 0;
						DrvMicroControllerData = 0x21;
					} else {
						DrvMicroControllerData = 0x00;
					}
				} else {
					DrvMicroControllerData = 0x00;
				}

				RAM[0x29] = (Source & 0xff00) | DrvMicroControllerData;
			}
			return 0;
		}
		
		case 0xff: {
			RAM[0xff] = (Source & 0xff00) | DrvMicroControllerID;
			break;
		}
	}

	return 0;
}

static void kyros_alpha_trigger_w(UINT32 Offset, UINT16 Data)
{
	if (Offset == 0x5b) DrvFlipScreen = Data & 0x01;
}

UINT8 __fastcall Kyros68KReadByte(UINT32 a)
{
	if (a >= 0x080000 && a <= 0x0801ff) {
		return kyros_alpha_trigger_r((a - 0x080000) >> 1);
	}
	
	switch (a) {
		case 0x060000: {
			return DrvVideoRam[0 ^ 1];
		}
		
		case 0x0c0000: {
			return DrvInput[1];
		}
		
		case 0x0c0001: {
			return DrvInput[0];
		}
		
		case 0x0e0000: {
			return DrvDip[0];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

UINT16 __fastcall Kyros68KReadWord(UINT32 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

void __fastcall Kyros68KWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x080000 && a <= 0x0801ff) {
		return kyros_alpha_trigger_w(a - 0x080000, d);
	}
	
	switch (a) {
		case 0x060001: {
			DrvVideoRam[1 ^ 1] = d;
			return;
		}
		
		case 0x0e0000: {
			DrvSoundLatch = d;
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

void __fastcall Kyros68KWriteWord(UINT32 a, UINT16 d)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

UINT8 __fastcall SstingryZ80Read(UINT16 a)
{
	switch (a) {
		case 0xc100: {
			return DrvSoundLatch;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

void __fastcall SstingryZ80Write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xc102: {
			DrvSoundLatch = 0;
			return;
		}
		
		case 0xc104: {
			DACSignedWrite(0, d);
			return;
		}
		
		case 0xc106:
		case 0xc108:
		case 0xc10a:
		case 0xc10c:
		case 0xc10e: {
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall KyrosZ80Read(UINT16 a)
{
	switch (a) {
		case 0xe000: {
			return DrvSoundLatch;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

void __fastcall KyrosZ80Write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xe002: {
			DrvSoundLatch = 0;
			return;
		}
		
		case 0xe004: {
			DACSignedWrite(0, d);
			return;
		}
		
		case 0xe006:
		case 0xe008:
		case 0xe00a:
		case 0xe00c:
		case 0xe00e: {
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall KyrosZ80PortRead(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Read => %02X\n"), a);
		}
	}

	return 0;
}

void __fastcall KyrosZ80PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		case 0x10: {
			BurnYM2203Write(0, 0, d);
			return;
		}
		
		case 0x11: {
			BurnYM2203Write(0, 1, d);
			return;
		}
		
		case 0x80: {
			BurnYM2203Write(1, 1, d);
			return;
		}
		
		case 0x81: {
			BurnYM2203Write(1, 0, d);
			return;
		}
		
		case 0x90: {
			BurnYM2203Write(2, 1, d);
			return;
		}
		
		case 0x91: {
			BurnYM2203Write(2, 0, d);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

static INT32 Sstingry1PlaneOffsets[3]   = { 4, 0x40000, 0x40004 };
static INT32 Sstingry1XOffsets[8]       = { 67, 66, 65, 64, 3, 2, 1, 0 };
static INT32 Sstingry1Offsets[8]        = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Sstingry2PlaneOffsets[3]   = { 0, 0x140000, 0x140004 };
static INT32 Sstingry2XOffsets[8]       = { 67, 66, 65, 64, 3, 2, 1, 0 };
static INT32 Sstingry2Offsets[8]        = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Sstingry3PlaneOffsets[3]   = { 0, 0x80000, 0x80004 };
static INT32 Sstingry3XOffsets[8]       = { 67, 66, 65, 64, 3, 2, 1, 0 };
static INT32 Sstingry3Offsets[8]        = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Kyros1PlaneOffsets[3]      = { 4, 0x40000, 0x40004 };
static INT32 Kyros1XOffsets[8]          = { 67, 66, 65, 64, 3, 2, 1, 0 };
static INT32 Kyros1Offsets[8]           = { 0, 8, 16, 24, 32, 40, 48, 56 };
static INT32 Kyros2PlaneOffsets[3]      = { 0, 0x80000, 0x80004 };
static INT32 Kyros2XOffsets[8]          = { 67, 66, 65, 64, 3, 2, 1, 0 };
static INT32 Kyros2Offsets[8]           = { 0, 8, 16, 24, 32, 40, 48, 56 };

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / nDrvTotalZ80Cycles);
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / nDrvTotalZ80Cycles;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / ((double)nDrvTotalZ80Cycles / (nBurnFPS / 100.000))));
}

static INT32 SstingryInit()
{
	INT32 nRet = 0, nLen;
	
	Mem = NULL;
	SstingryMemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	SstingryMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x60000);

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x08001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x08000, 3, 2); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom + 0x04000, 5, 1); if (nRet != 0) return 1;
	
	memset(DrvTempRom, 0, 0x60000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 12, 1); if (nRet != 0) return 1;
	GfxDecode(0x400, 3, 8, 8, Sstingry1PlaneOffsets, Sstingry1XOffsets, Sstingry1Offsets, 0x80, DrvTempRom + 0x00000, DrvGfxData[0]);
	GfxDecode(0x400, 3, 8, 8, Sstingry2PlaneOffsets, Sstingry2XOffsets, Sstingry2Offsets, 0x80, DrvTempRom + 0x00000, DrvGfxData[1]);
	GfxDecode(0x400, 3, 8, 8, Sstingry1PlaneOffsets, Sstingry1XOffsets, Sstingry1Offsets, 0x80, DrvTempRom + 0x10000, DrvGfxData[2]);
	GfxDecode(0x400, 3, 8, 8, Sstingry3PlaneOffsets, Sstingry3XOffsets, Sstingry3Offsets, 0x80, DrvTempRom + 0x10000, DrvGfxData[3]);
	BurnFree(DrvTempRom);
	
	nRet = BurnLoadRom(DrvProms + 0x00000,  13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00100,  14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00200,  15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00300,  16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00400,  17, 1); if (nRet != 0) return 1;
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvSharedRam        , 0x020000, 0x020fff, MAP_RAM);
	SekMapMemory(DrvSpriteRam        , 0x040000, 0x041fff, MAP_RAM);
	SekSetReadByteHandler(0, Kyros68KReadByte);
	SekSetWriteByteHandler(0, Kyros68KWriteByte);
	SekSetReadWordHandler(0, Kyros68KReadWord);
	SekSetWriteWordHandler(0, Kyros68KWriteWord);
	SekClose();
	
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom                );
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80Ram                );
	ZetSetReadHandler(SstingryZ80Read);
	ZetSetWriteHandler(SstingryZ80Write);
	ZetSetInHandler(KyrosZ80PortRead);
	ZetSetOutHandler(KyrosZ80PortWrite);
	ZetClose();
	
	nDrvTotal68KCycles = 6000000;
	nDrvTotalZ80Cycles = 3579545;
	
	BurnYM2203Init(3, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(3579545);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);
	
	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();
	DrvDrawFunction = SstingryDraw;
	
	DrvMicroControllerID = 0x00ff;
	DrvCoinID = 0x22 | (0x22 << 8);

	DrvDoReset();

	return 0;
}

static INT32 KyrosInit()
{
	INT32 nRet = 0, nLen;
	
	Mem = NULL;
	KyrosMemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	KyrosMemIndex();

	DrvTempRom = (UINT8 *)BurnMalloc(0x60000);

	nRet = BurnLoadRom(Drv68KRom + 0x00001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x00000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x10001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x10000, 3, 2); if (nRet != 0) return 1;
	memset(DrvTempRom, 0, 0x60000);
	memcpy(DrvTempRom, Drv68KRom, 0x20000);
	memcpy(Drv68KRom + 0x00000, DrvTempRom + 0x00000, 0x8000);
	memcpy(Drv68KRom + 0x10000, DrvTempRom + 0x08000, 0x8000);
	memcpy(Drv68KRom + 0x08000, DrvTempRom + 0x10000, 0x8000);
	memcpy(Drv68KRom + 0x18000, DrvTempRom + 0x18000, 0x8000);
	
	nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 4, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvZ80Rom + 0x04000, 5, 1); if (nRet != 0) return 1;
	
	memset(DrvTempRom, 0, 0x60000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x08000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x10000, 10, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x18000, 11, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x20000, 12, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x28000, 13, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x30000, 14, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x38000, 15, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x40000, 16, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x48000, 17, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x50000, 18, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTempRom + 0x58000, 19, 1); if (nRet != 0) return 1;
	GfxDecode(0x800, 3, 8, 8, Kyros1PlaneOffsets, Kyros1XOffsets, Kyros1Offsets, 0x80, DrvTempRom + 0x00000, DrvGfxData[0]);
	GfxDecode(0x800, 3, 8, 8, Kyros2PlaneOffsets, Kyros2XOffsets, Kyros2Offsets, 0x80, DrvTempRom + 0x00000, DrvGfxData[1]);
	GfxDecode(0x800, 3, 8, 8, Kyros1PlaneOffsets, Kyros1XOffsets, Kyros1Offsets, 0x80, DrvTempRom + 0x18000, DrvGfxData[2]);
	GfxDecode(0x800, 3, 8, 8, Kyros2PlaneOffsets, Kyros2XOffsets, Kyros2Offsets, 0x80, DrvTempRom + 0x18000, DrvGfxData[3]);
	GfxDecode(0x800, 3, 8, 8, Kyros1PlaneOffsets, Kyros1XOffsets, Kyros1Offsets, 0x80, DrvTempRom + 0x30000, DrvGfxData[4]);
	GfxDecode(0x800, 3, 8, 8, Kyros2PlaneOffsets, Kyros2XOffsets, Kyros2Offsets, 0x80, DrvTempRom + 0x30000, DrvGfxData[5]);
	GfxDecode(0x800, 3, 8, 8, Kyros1PlaneOffsets, Kyros1XOffsets, Kyros1Offsets, 0x80, DrvTempRom + 0x48000, DrvGfxData[6]);
	GfxDecode(0x800, 3, 8, 8, Kyros2PlaneOffsets, Kyros2XOffsets, Kyros2Offsets, 0x80, DrvTempRom + 0x48000, DrvGfxData[7]);
	BurnFree(DrvTempRom);
	
	nRet = BurnLoadRom(DrvProms + 0x00000,  20, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00100,  21, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00200,  22, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00300,  23, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvProms + 0x00400,  24, 1); if (nRet != 0) return 1;
	
	nRet = BurnLoadRom(DrvColourProm + 0x00000,  25, 1); if (nRet != 0) return 1;
	
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvSharedRam        , 0x020000, 0x020fff, MAP_RAM);
	SekMapMemory(DrvSpriteRam        , 0x040000, 0x041fff, MAP_RAM);
	SekSetReadByteHandler(0, Kyros68KReadByte);
	SekSetWriteByteHandler(0, Kyros68KWriteByte);
	SekSetReadWordHandler(0, Kyros68KReadWord);
	SekSetWriteWordHandler(0, Kyros68KWriteWord);
	SekClose();
	
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80Rom                );
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80Ram                );
	ZetSetReadHandler(KyrosZ80Read);
	ZetSetWriteHandler(KyrosZ80Write);
	ZetSetInHandler(KyrosZ80PortRead);
	ZetSetOutHandler(KyrosZ80PortWrite);
	ZetClose();
	
	nDrvTotal68KCycles = 6000000;
	nDrvTotalZ80Cycles = 4000000;
	
	BurnYM2203Init(3, 2000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_YM2203_ROUTE, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_1, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_2, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(2, BURN_SND_YM2203_AY8910_ROUTE_3, 0.90, BURN_SND_ROUTE_BOTH);
	
	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();
	DrvDrawFunction = KyrosDraw;
	
	DrvMicroControllerID = 0x0012;
	DrvCoinID = 0x22 | (0x22 << 8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	ZetExit();
	BurnYM2203Exit();
	DACExit();
	
	GenericTilesExit();
	
	BurnFree(Mem);
	
	DrvCredits = 0;
	DrvTrigState = 0;
	DrvDeposits1 = 0;
	DrvDeposits2 = 0;
	DrvCoinValue = 0;
	DrvMicroControllerData = 0;
	DrvLatch = 0;
	DrvCoinID = 0;
	DrvMicroControllerID = 0;
	DrvFlipScreen = 0;
	DrvSoundLatch = 0;
	
	DrvDrawFunction = NULL;

	return 0;
}

static inline UINT8 pal4bit(UINT8 bits)
{
	bits &= 0x0f;
	return (bits << 4) | bits;
}

static void KyrosCalcPalette()
{
	INT32 i;
	UINT32 Palette[256];
	
	for (i = 0; i < 256; i++) {
		INT32 r = pal4bit(DrvProms[i + 0x000]);
		INT32 g = pal4bit(DrvProms[i + 0x100]);
		INT32 b = pal4bit(DrvProms[i + 0x200]);
		
		Palette[i] = BurnHighCol(r, g, b, 0);
	}
	
	for (i = 0; i < 256; i++) {
		UINT8 CTabEntry = ((DrvProms[i + 0x300] & 0x0f) << 4) | (DrvProms[i + 0x400] & 0x0f);
		DrvPalette[i] = Palette[CTabEntry];
	}
	
	UINT16 *RAM = (UINT16*)DrvVideoRam;
	DrvPalette[0x100] = Palette[RAM[0] & 0xff];
}

static void SstingryDrawSprites(INT32 c, INT32 d)
{
	UINT16 *RAM = (UINT16*)DrvSpriteRam;
	INT32 Data, Offs, mx, my, Colour, Tile, i, Bank, yFlip, xFlip;
	
	for (Offs = 0; Offs < 0x400; Offs += 0x20) {
		mx = RAM[Offs + c];
		my = -(mx >> 8) & 0xff;
		mx &= 0xff;
		if (mx > 0xf8) mx -= 0x100;
		
		if (DrvFlipScreen) my = 249 - my;
		
		for (i = 0; i < 0x20; i++) {
			Data = RAM[Offs + d + i];
			if (Data != 0x40) {
				yFlip = Data & 0x1000;
				xFlip = 0;
				
				if (DrvFlipScreen) {
					if (yFlip) {
						yFlip = 0;
					} else {
						yFlip = 1;
					}
					xFlip = 1;
				}

				Colour = (Data >> 7 & 0x18) | (Data >> 13 & 7);
				Tile = Data & 0x3ff;
				Bank = Data >> 10 & 3;
				
				if (mx > 0 && mx < 248 && (my - 16) > 0 && (my - 16) < 216) {
					if (xFlip) {
						if (yFlip) {
							Render8x8Tile_Mask_FlipXY(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						} else {
							Render8x8Tile_Mask_FlipX(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						}
					} else {
						if (yFlip) {
							Render8x8Tile_Mask_FlipY(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						} else {
							Render8x8Tile_Mask(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						}
					}	
				} else {
					if (xFlip) {
						if (yFlip) {
							Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						} else {
							Render8x8Tile_Mask_FlipX_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						}
					} else {
						if (yFlip) {
							Render8x8Tile_Mask_FlipY_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
						}
					}
				}
			}

			if (DrvFlipScreen) {
				my = (my - 8) & 0xff;
			} else {
				my = (my + 8) & 0xff;
			}
		}
	}
}

static void KyrosVideoBanking(INT32 *Bank, INT32 Data)
{
	*Bank = (Data >> 13 & 4) | (Data >> 10 & 3);
}

static void KyrosDrawSprites(INT32 c, INT32 d)
{
	UINT16 *RAM = (UINT16*)DrvSpriteRam;
	INT32 Data, Offs, mx, my, Colour, Tile, i, Bank, yFlip, xFlip;
	
	for (Offs = 0; Offs < 0x400; Offs += 0x20) {
		mx = RAM[Offs + c];
		my = -(mx >> 8) & 0xff;
		mx &= 0xff;

		if (DrvFlipScreen) my = 249 - my;

		for (i = 0; i < 0x20; i++) {
			Data = RAM[Offs + d + i];
			if (Data != 0x20)	{
				Colour = DrvColourProm[(Data >> 1 & 0x1000) | (Data & 0xffc) | (Data >> 14 & 3)];
				if (Colour != 0xff)	{
					yFlip = Data & 0x1000;
					xFlip = 0;

					if(DrvFlipScreen) {
						if (yFlip) {
							yFlip = 0;
						} else {
							yFlip = 1;
						}
						xFlip = 1;
					}

					Tile = (Data >> 3 & 0x400) | (Data & 0x3ff);
//					if (m_game_id == ALPHA68K_KYROS)
						KyrosVideoBanking(&Bank, Data);
//					else
//						jongbou_video_banking(&bank, data);

					if (mx > 0 && mx < 248 && (my - 16) > 0 && (my - 16) < 216) {
						if (xFlip) {
							if (yFlip) {
								Render8x8Tile_Mask_FlipXY(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							} else {
								Render8x8Tile_Mask_FlipX(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							}
						} else {
							if (yFlip) {
								Render8x8Tile_Mask_FlipY(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							} else {
								Render8x8Tile_Mask(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							}
						}	
					} else {
						if (xFlip) {
							if (yFlip) {
								Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							} else {
								Render8x8Tile_Mask_FlipX_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							}
						} else {
							if (yFlip) {
								Render8x8Tile_Mask_FlipY_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							} else {
								Render8x8Tile_Mask_Clip(pTransDraw, Tile, mx, my - 16, Colour, 3, 0, 0, DrvGfxData[Bank]);
							}
						}
					}
				}
			}

			if (DrvFlipScreen) {
				my = (my - 8) & 0xff;
			} else {
				my = (my + 8) & 0xff;
			}
		}
	}
}

static void SstingryDraw()
{
	BurnTransferClear();
	KyrosCalcPalette();
	for (INT32 i = 0; i < nScreenHeight * nScreenWidth; i++) {
		pTransDraw[i] = 0x100;
	}
	SstingryDrawSprites(2, 0x0800);
	SstingryDrawSprites(3, 0x0c00);
	SstingryDrawSprites(1, 0x0400);
	BurnTransferCopy(DrvPalette);
}

static void KyrosDraw()
{
	BurnTransferClear();
	KyrosCalcPalette();
	for (INT32 i = 0; i < nScreenHeight * nScreenWidth; i++) {
		pTransDraw[i] = 0x100;
	}
	KyrosDrawSprites(2, 0x0800);
	KyrosDrawSprites(3, 0x0c00);
	KyrosDrawSprites(1, 0x0400);
	BurnTransferCopy(DrvPalette);
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 132;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = nDrvTotal68KCycles / 60;
	nCyclesTotal[1] = nDrvTotalZ80Cycles / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	SekOpen(0);
	ZetOpen(0);
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesDone[0] += SekRun(nCyclesSegment);
		if (i == 125) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (i == 66) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		if (i == 44 || i == 88) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		if (i & 1) ZetNmi();
	}
	
	BurnTimerEndFrame(nCyclesTotal[1]);
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}
	
	SekClose();
	ZetClose();
	
	if (pBurnDraw) DrvDrawFunction();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029735;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

	}
	
	return 0;
}

struct BurnDriver BurnDrvSstingry = {
	"sstingry", NULL, NULL, NULL, "1986",
	"Super Stingray (Japan)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, SstingryRomInfo, SstingryRomName, NULL, NULL, SstingryInputInfo, SstingryDIPInfo,
	SstingryInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x101, 224, 256, 3, 4
};

struct BurnDriver BurnDrvKyros = {
	"kyros", NULL, NULL, NULL, "1987",
	"Kyros\0", NULL, "Alpha Denshi Co. (World Games Inc. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, KyrosRomInfo, KyrosRomName, NULL, NULL, SstingryInputInfo, KyrosDIPInfo,
	KyrosInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x101, 224, 256, 3, 4
};

struct BurnDriver BurnDrvKyrosj = {
	"kyrosj", "kyros", NULL, NULL, "1986",
	"Kyros No Yakata (Japan)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, KyrosjRomInfo, KyrosjRomName, NULL, NULL, SstingryInputInfo, KyrosDIPInfo,
	KyrosInit, DrvExit, DrvFrame, NULL, DrvScan,
	NULL, 0x101, 224, 256, 3, 4
};
