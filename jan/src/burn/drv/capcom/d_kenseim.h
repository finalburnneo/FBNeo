static UINT8 KenseimInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 KenseimInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 KenseimInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 KenseimDip[2]        = {0, 0};
static UINT8 KenseimInput[3]      = {0x00, 0x00, 0x00};
static UINT8 KenseimReset         = 0;
static UINT8 *KenseimRom          = NULL;
static UINT8 *KenseimRam          = NULL;

static struct BurnInputInfo KenseimInputList[] =
{
	{"Coin"             , BIT_DIGITAL  , KenseimInputPort0 + 0, "p1 coin"   },
	{"Ryu Start"        , BIT_DIGITAL  , KenseimInputPort0 + 1, "p1 start"  },
	{"Chun-Li Start"    , BIT_DIGITAL  , KenseimInputPort0 + 2, "p2 start"  },

	{"Mole A1"          , BIT_DIGITAL  , KenseimInputPort1 + 0, "p1 fire 1" },
	{"Mole A2"          , BIT_DIGITAL  , KenseimInputPort1 + 1, "p1 fire 2" },
	{"Mole A3"          , BIT_DIGITAL  , KenseimInputPort1 + 2, "p1 fire 3" },
	{"Mole A4"          , BIT_DIGITAL  , KenseimInputPort1 + 3, "p1 fire 4" },
	{"Mole A5"          , BIT_DIGITAL  , KenseimInputPort1 + 4, "p1 fire 5" },
	{"Mole A6"          , BIT_DIGITAL  , KenseimInputPort1 + 5, "p1 fire 6" },
	
	{"Mole B1"          , BIT_DIGITAL  , KenseimInputPort2 + 0, "p2 fire 1" },
	{"Mole B2"          , BIT_DIGITAL  , KenseimInputPort2 + 1, "p2 fire 2" },
	{"Mole B3"          , BIT_DIGITAL  , KenseimInputPort2 + 2, "p2 fire 3" },
	{"Mole B4"          , BIT_DIGITAL  , KenseimInputPort2 + 3, "p2 fire 4" },
	{"Mole B5"          , BIT_DIGITAL  , KenseimInputPort2 + 4, "p2 fire 5" },
	{"Mole B6"          , BIT_DIGITAL  , KenseimInputPort2 + 5, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &KenseimReset        , "reset"     },
	{"Service"          , BIT_DIGITAL  , KenseimInputPort0 + 3, "service"   },
	{"Dip A"            , BIT_DIPSWITCH, &Cpi01A              , "dip"       },
	{"Dip B"            , BIT_DIPSWITCH, &Cpi01C              , "dip"       },
	{"Dip C"            , BIT_DIPSWITCH, &Cpi01E              , "dip"       },
	{"Dip 1"            , BIT_DIPSWITCH, KenseimDip + 0       , "dip"       },
	{"Dip 2"            , BIT_DIPSWITCH, KenseimDip + 1       , "dip"       },
};

STDINPUTINFO(Kenseim)

static struct BurnDIPInfo KenseimDIPList[]=
{
	// Default Values
	{0x11, 0xff, 0xff, 0x00, NULL                     },
	{0x12, 0xff, 0xff, 0x00, NULL                     },
	{0x13, 0xff, 0xff, 0x20, NULL                     },
	{0x14, 0xff, 0xff, 0xff, NULL                     },
	{0x15, 0xff, 0xff, 0xff, NULL                     },

	// CPS Dip C
	{0   , 0xfe, 0   , 2   , "Freeze"                 },
	{0x13, 0x01, 0x10, 0x08, "Off"                    },
	{0x13, 0x01, 0x10, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x13, 0x01, 0x10, 0x00, "Off"                    },
	{0x13, 0x01, 0x10, 0x10, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x13, 0x01, 0x20, 0x00, "Off"                    },
	{0x13, 0x01, 0x20, 0x20, "On"                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coinage"                },
	{0x14, 0x01, 0x03, 0x00, "4 Coins 1 Credit"       },
	{0x14, 0x01, 0x03, 0x01, "3 Coins 1 Credit"       },
	{0x14, 0x01, 0x03, 0x03, "2 Coins 1 Credit"       },
	{0x14, 0x01, 0x03, 0x02, "1 Coin  1 Credit"       },
	
	{0   , 0xfe, 0   , 4   , "Head Appear Once Ratio" },
	{0x14, 0x01, 0x0c, 0x00, "0"                      },
	{0x14, 0x01, 0x0c, 0x04, "1"                      },
	{0x14, 0x01, 0x0c, 0x08, "2"                      },
	{0x14, 0x01, 0x0c, 0x0c, "3"                      },
	
	{0   , 0xfe, 0   , 4   , "Strength of Computer"  },
	{0x14, 0x01, 0x30, 0x00, "0"                      },
	{0x14, 0x01, 0x30, 0x10, "1"                      },
	{0x14, 0x01, 0x30, 0x20, "2"                      },
	{0x14, 0x01, 0x30, 0x30, "3"                      },
	
	{0   , 0xfe, 0   , 2   , "Game Time"              },
	{0x14, 0x01, 0x40, 0x00, "Long (59 seconds)"      },
	{0x14, 0x01, 0x40, 0x40, "Short (49 seconds)"     },
	
	{0   , 0xfe, 0   , 2   , "Winner of 2 Player faces Vega" },
	{0x14, 0x01, 0x80, 0x00, "No"                     },
	{0x14, 0x01, 0x80, 0x80, "Yes"                    },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Unknown 1 (1-bit)"      },
	{0x15, 0x01, 0x01, 0x00, "0"                      },
	{0x15, 0x01, 0x01, 0x01, "1"                      },
	
	{0   , 0xfe, 0   , 4   , "Unknown 2 (2-bit)"      },
	{0x15, 0x01, 0x06, 0x00, "0"                      },
	{0x15, 0x01, 0x06, 0x02, "1"                      },
	{0x15, 0x01, 0x06, 0x04, "2"                      },
	{0x15, 0x01, 0x06, 0x06, "3"                      },
	
	{0   , 0xfe, 0   , 4   , "Unknown 3 (2-bit)"      },
	{0x15, 0x01, 0x18, 0x00, "0"                      },
	{0x15, 0x01, 0x18, 0x08, "1"                      },
	{0x15, 0x01, 0x18, 0x10, "2"                      },
	{0x15, 0x01, 0x18, 0x18, "3"                      },
	
	{0   , 0xfe, 0   , 2   , "Test Mode"              },
	{0x15, 0x01, 0x80, 0x80, "Off"                    },
	{0x15, 0x01, 0x80, 0x00, "On"                     },
};

STDDIPINFO(Kenseim)

static struct BurnRomInfo KenseimRomDesc[] = {
	{ "knm_23.8f",     0x080000, 0xf8368900, BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP },
	{ "knm_21.6f",     0x080000, 0xa8025e91, BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP },
	// repeat to allocate enough memory and load in right place
	{ "knm_21.6f",     0x080000, 0xa8025e91, BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP },

	{ "knm_01.3a",     0x080000, 0x923f0c0c, BRF_GRA | CPS1_TILES },
	{ "knm_02.4a",     0x080000, 0xfa694f67, BRF_GRA | CPS1_TILES },
	{ "knm_03.5a",     0x080000, 0xaf7af02c, BRF_GRA | CPS1_TILES },
	{ "knm_04.6a",     0x080000, 0x607a9af4, BRF_GRA | CPS1_TILES },
	{ "knm_05.7a",     0x080000, 0xd877eee9, BRF_GRA | CPS1_TILES },
	{ "knm_06.8a",     0x080000, 0x8821a281, BRF_GRA | CPS1_TILES },
	{ "knm_07.9a",     0x080000, 0x00306d09, BRF_GRA | CPS1_TILES },
	{ "knm_08.10a",    0x080000, 0x4a329d16, BRF_GRA | CPS1_TILES },
	{ "knm_10.3c",     0x080000, 0xca93a942, BRF_GRA | CPS1_TILES },
	{ "knm_11.4c",     0x080000, 0xa91f3091, BRF_GRA | CPS1_TILES },
	{ "knm_12.5c",     0x080000, 0x5da8303a, BRF_GRA | CPS1_TILES },
	{ "knm_13.6c",     0x080000, 0x889bb671, BRF_GRA | CPS1_TILES },

	{ "knm_09.12a",    0x020000, 0x15394dd7, BRF_PRG | CPS1_Z80_PROGRAM },
	
	{ "knm_18.11c",    0x020000, 0x9e3e4773, BRF_SND | CPS1_OKIM6295_SAMPLES },
	{ "knm_19.12c",    0x020000, 0xd6c4047f, BRF_SND | CPS1_OKIM6295_SAMPLES },
	
	A_BOARD_PLDS
	
	{ "knm10b.1a",     0x000117, 0xe40131d4, BRF_OPT },	// b-board PLDs
	{ "iob1.12d",      0x000117, 0x3abc0700, BRF_OPT },
	{ "bprg1.11d",     0x000117, 0x31793da7, BRF_OPT },
	{ "ioc1.ic7",      0x000117, 0x0d182081, BRF_OPT },	// c-board PLDs
	{ "c632.ic1",      0x000117, 0x0fbd9270, BRF_OPT },
	
	{ "kensei_mogura_ver1.0.u2", 0x008000, 0x725cfcfc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(Kenseim)
STD_ROM_FN(Kenseim)

static inline void KenseimMakeInputs()
{
	// Reset Inputs
	KenseimInput[0] = 0x00;
	KenseimInput[1] = 0x20;
	KenseimInput[2] = 0x20;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		KenseimInput[0] |= (KenseimInputPort0[i] & 1) << i;
	}
	
	for (INT32 i = 0; i < 6; i++) {
		KenseimInput[1] -= (KenseimInputPort1[i] & 1) << i;
		KenseimInput[2] -= (KenseimInputPort2[i] & 1) << i;
	}
}

static INT32 KenseimDoReset()
{
	CpsReset = 1;
	
	ZetOpen(1);
	ZetReset();
	ZetClose();
	
	return 0;
}

static UINT8 __fastcall KenseimPortRead(UINT16 a)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Port Read => %02X\n"), a);
		}
	}

	return 0;
}

static void __fastcall KenseimPortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Port Write => %02X, %02X\n"), a, d);
		}
	}
}

static INT32 KenseimInit()
{
	KenseimRom = BurnMalloc(0x8000 * sizeof(UINT8));
	KenseimRam = BurnMalloc(0x8000 * sizeof(UINT8));
	
	ZetInit(1);
	ZetOpen(1);
	ZetSetInHandler(KenseimPortRead);
	ZetSetOutHandler(KenseimPortWrite);
	ZetMapArea(0x0000, 0x7fff, 0, KenseimRom);
	ZetMapArea(0x0000, 0x7fff, 2, KenseimRom);
	ZetMapArea(0x8000, 0xffff, 0, KenseimRam);
	ZetMapArea(0x8000, 0xffff, 1, KenseimRam);
	ZetMapArea(0x8000, 0xffff, 2, KenseimRam);
	ZetClose();
	
	return DrvInit();
}

static INT32 KenseimExit()
{
	ZetExit();
	
	BurnFree(KenseimRom);
	BurnFree(KenseimRam);
	
	return DrvExit();
}

static INT32 KenseimFrame()
{
	INT32 nCyclesTotal = (8000000 / 2) / 60;
		
	if (KenseimReset) {
		KenseimDoReset();
	}
	
	KenseimMakeInputs();
	
	ZetOpen(1);
	ZetRun(nCyclesTotal);
	ZetClose();
	
	return Cps1Frame();
}

static INT32 KenseimRedraw()
{
	return CpsRedraw();
}

static INT32 KenseimScan(INT32 nAction, INT32 *pnMin)
{
	return CpsAreaScan(nAction, pnMin);
}

struct BurnDriverD BurnDrvCpsKenseim = {
	"kenseim", NULL, NULL, NULL, "1994",
	"Ken Sei Mogura: Street Fighter II (Japan 940418, Ver 1.00)\0", NULL, "Capcom / Togo / Sigma", "CPS1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS1, GBF_MINIGAMES, 0,
	NULL, KenseimRomInfo, KenseimRomName, NULL, NULL, KenseimInputInfo, KenseimDIPInfo,
	KenseimInit, KenseimExit, KenseimFrame, KenseimRedraw, KenseimScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};