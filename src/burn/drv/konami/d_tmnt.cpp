// FB Neo Teenage Mutant Ninja Turtle driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "upd7759.h"
#include "k007232.h"
#include "k053260.h"
#include "eeprom.h"

static UINT8 DrvInputPort0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort3[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort4[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvInputPort5[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDip[3]        = {0, 0, 0};
static UINT8 DrvInput[6]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static UINT8 DrvReset         = 0;

static UINT8 *Mem                 = NULL;
static UINT8 *MemEnd              = NULL;
static UINT8 *RamStart            = NULL;
static UINT8 *RamEnd              = NULL;
static UINT8 *Drv68KRom           = NULL;
static UINT8 *Drv68KRam           = NULL;
static UINT8 *DrvZ80Rom           = NULL;
static UINT8 *DrvZ80Ram           = NULL;
static UINT8 *DrvSoundRom         = NULL;
static UINT8 *DrvUPD7759CRom      = NULL;
static UINT8 *DrvTileRom          = NULL;
static UINT8 *DrvSpriteRom        = NULL;
static UINT8 *DrvSpriteRam        = NULL;
static UINT8 *DrvPaletteRam       = NULL;
static UINT8 *DrvNvRam            = NULL;
static UINT8 *DrvTiles            = NULL;
static UINT8 *DrvSprites          = NULL;
static UINT8 *DrvTempRom          = NULL;
static UINT32 *DrvPalette          = NULL;
static INT16  *DrvTitleSample      = NULL;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

static UINT8 bIrqEnable;
static INT32 LayerColourBase[3];
static INT32 SpriteColourBase;
static INT32 LayerPri[3];
static UINT8 DrvSoundLatch;
static INT32 PriorityFlag;

static INT32 LastCntrl;
static INT32 Toggle;

static INT32 dim_c, dim_v;
static INT32 NoDim = 0;

static INT32 DrvNvRamBank;
static INT32 CuebrickSndIrqFire;
static INT32 BlswhstlTileRomBank;

static INT32 TitleSoundLatch;
static INT32 PlayTitleSample;
static double TitleSamplePos = 0;
static double TitleSampleGain;
static INT32 TitleSampleOutputDir;

static UINT8 DrvVBlank;

static INT32 InitEEPROMCount;

static INT32 uses_k007232 = 0;

static INT32 is_tmnt2 = 0;

static const eeprom_interface BlswhstlEEPROMInterface =
{
	7,
	8,
	"011000",
	"011100",
	0,
	"0100000000000",
	"0100110000000",
	0,
	0
};

static const eeprom_interface thndrx2_eeprom_interface =
{
	7,
	8,
	"011000",
	"010100",
	0,
	"0100000000000",
	"0100110000000",
	0,
	0
};

static UINT8 VerSwitcher = 0;	// Fake Dip

static struct BurnInputInfo TmntInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	
	{"P3 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p3 coin"   },
	{"P3 Up"             , BIT_DIGITAL  , DrvInputPort3 + 2, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , DrvInputPort3 + 3, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , DrvInputPort3 + 0, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , DrvInputPort3 + 1, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , DrvInputPort3 + 4, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , DrvInputPort3 + 5, "p3 fire 2" },
	
	{"P4 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p4 coin"   },
	{"P4 Up"             , BIT_DIGITAL  , DrvInputPort4 + 2, "p4 up"     },
	{"P4 Down"           , BIT_DIGITAL  , DrvInputPort4 + 3, "p4 down"   },
	{"P4 Left"           , BIT_DIGITAL  , DrvInputPort4 + 0, "p4 left"   },
	{"P4 Right"          , BIT_DIGITAL  , DrvInputPort4 + 1, "p4 right"  },
	{"P4 Fire 1"         , BIT_DIGITAL  , DrvInputPort4 + 4, "p4 fire 1" },
	{"P4 Fire 2"         , BIT_DIGITAL  , DrvInputPort4 + 5, "p4 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "service"   },
	{"Service 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "service2"  },
	{"Service 3"         , BIT_DIGITAL  , DrvInputPort0 + 6, "service3"  },
	{"Service 4"         , BIT_DIGITAL  , DrvInputPort0 + 7, "service4"  },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2       , "dip"       },
};

STDINPUTINFO(Tmnt)

static struct BurnInputInfo Tmnt2pInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "service"   },
	{"Service 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "service2"  },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2       , "dip"       },
};

STDINPUTINFO(Tmnt2p)

static struct BurnInputInfo MiaInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort0 + 3, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , DrvInputPort1 + 6, "p1 fire 3" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort0 + 4, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , DrvInputPort2 + 6, "p2 fire 3" },
	
	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort0 + 6, "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, DrvDip + 0       , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, DrvDip + 1       , "dip"       },
	{"Dip 3"             , BIT_DIPSWITCH, DrvDip + 2       , "dip"       },
};

STDINPUTINFO(Mia)

static struct BurnInputInfo BlswhstlInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	
	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service"           , BIT_DIGITAL  , DrvInputPort0 + 2, "service"   },
	{"Diagnostics"       , BIT_DIGITAL  , DrvInputPort0 + 3, "diag"      },
};

STDINPUTINFO(Blswhstl)

static struct BurnInputInfo Ssriders4pInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	
	{"P3 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p3 coin"   },
	{"P3 Up"             , BIT_DIGITAL  , DrvInputPort3 + 2, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , DrvInputPort3 + 3, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , DrvInputPort3 + 0, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , DrvInputPort3 + 1, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , DrvInputPort3 + 4, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , DrvInputPort3 + 5, "p3 fire 2" },
	
	{"P4 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p4 coin"   },
	{"P4 Up"             , BIT_DIGITAL  , DrvInputPort4 + 2, "p4 up"     },
	{"P4 Down"           , BIT_DIGITAL  , DrvInputPort4 + 3, "p4 down"   },
	{"P4 Left"           , BIT_DIGITAL  , DrvInputPort4 + 0, "p4 left"   },
	{"P4 Right"          , BIT_DIGITAL  , DrvInputPort4 + 1, "p4 right"  },
	{"P4 Fire 1"         , BIT_DIGITAL  , DrvInputPort4 + 4, "p4 fire 1" },
	{"P4 Fire 2"         , BIT_DIGITAL  , DrvInputPort4 + 5, "p4 fire 2" },
	
	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "service"   },
	{"Service 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "service2"  },
	{"Service 3"         , BIT_DIGITAL  , DrvInputPort0 + 6, "service3"  },
	{"Service 4"         , BIT_DIGITAL  , DrvInputPort0 + 7, "service4"  },
	{"Diagnostics"       , BIT_DIGITAL  , DrvInputPort5 + 7, "diag"      },
};

STDINPUTINFO(Ssriders4p)

static struct BurnInputInfo VerSwitchInputList[] = {
	{"Dip Ex"            , BIT_DIPSWITCH , &VerSwitcher    , "dip"       },	// 22
};

STDINPUTINFOEXT(Ssriders4pVerSw, Ssriders4p, VerSwitch)

static struct BurnInputInfo SsridersInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	
	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "service"   },
	{"Service 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "service2"  },
	{"Diagnostics"       , BIT_DIGITAL  , DrvInputPort5 + 7, "diag"      },
};

STDINPUTINFO(Ssriders)

static struct BurnInputInfo Ssriders4psInputList[] =
{
	{"P1 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , DrvInputPort1 + 7, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , DrvInputPort1 + 2, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , DrvInputPort1 + 3, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , DrvInputPort1 + 0, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , DrvInputPort1 + 1, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , DrvInputPort1 + 4, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , DrvInputPort1 + 5, "p1 fire 2" },
	
	{"P2 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , DrvInputPort2 + 7, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , DrvInputPort2 + 2, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , DrvInputPort2 + 3, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , DrvInputPort2 + 0, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , DrvInputPort2 + 1, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , DrvInputPort2 + 4, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , DrvInputPort2 + 5, "p2 fire 2" },
	
	{"P3 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 2, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , DrvInputPort3 + 7, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , DrvInputPort3 + 2, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , DrvInputPort3 + 3, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , DrvInputPort3 + 0, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , DrvInputPort3 + 1, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , DrvInputPort3 + 4, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , DrvInputPort3 + 5, "p3 fire 2" },
	
	{"P4 Coin"           , BIT_DIGITAL  , DrvInputPort0 + 3, "p4 coin"   },
	{"P4 Start"          , BIT_DIGITAL  , DrvInputPort4 + 7, "p4 start"  },
	{"P4 Up"             , BIT_DIGITAL  , DrvInputPort4 + 2, "p4 up"     },
	{"P4 Down"           , BIT_DIGITAL  , DrvInputPort4 + 3, "p4 down"   },
	{"P4 Left"           , BIT_DIGITAL  , DrvInputPort4 + 0, "p4 left"   },
	{"P4 Right"          , BIT_DIGITAL  , DrvInputPort4 + 1, "p4 right"  },
	{"P4 Fire 1"         , BIT_DIGITAL  , DrvInputPort4 + 4, "p4 fire 1" },
	{"P4 Fire 2"         , BIT_DIGITAL  , DrvInputPort4 + 5, "p4 fire 2" },
	
	{"Reset"             , BIT_DIGITAL  , &DrvReset        , "reset"     },
	{"Service 1"         , BIT_DIGITAL  , DrvInputPort0 + 4, "service"   },
	{"Service 2"         , BIT_DIGITAL  , DrvInputPort0 + 5, "service2"  },
	{"Service 3"         , BIT_DIGITAL  , DrvInputPort0 + 6, "service3"  },
	{"Service 4"         , BIT_DIGITAL  , DrvInputPort0 + 7, "service4"  },
	{"Diagnostics"       , BIT_DIGITAL  , DrvInputPort5 + 7, "diag"      },
};

STDINPUTINFO(Ssriders4ps)

static struct BurnInputInfo Thndrx2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvInputPort0 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvInputPort0 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort0 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort0 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort0 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort1 + 1,	"p2 coin"	},

	{"P2 Start",		BIT_DIGITAL,	DrvInputPort2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	DrvInputPort1 + 2,	"service"	},
	{"Diagnostics",		BIT_DIGITAL,	DrvInputPort1 + 3,	"diag"		},
};

STDINPUTINFO(Thndrx2)

static struct BurnInputInfo LgtnfghtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvInputPort1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvInputPort1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvInputPort1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort0 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort0 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvInputPort2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	DrvInputPort0 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,			"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDip + 2,			"dip"		},
};

STDINPUTINFO(Lgtnfght)

static struct BurnInputInfo QgakumonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p1 fire 4"	},
	{"P1 Push",			BIT_DIGITAL,	DrvInputPort1 + 4,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort0 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 fire 4"	},
	{"P2 Push",			BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	DrvInputPort0 + 4,	"service"	},
	{"Diagnostics",		BIT_DIGITAL,	DrvInputPort5 + 7,	"diag"		},
};

STDINPUTINFO(Qgakumon)

static struct BurnInputInfo PunkshotInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvInputPort1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvInputPort1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort0 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 5,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvInputPort0 + 2,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvInputPort3 + 2,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvInputPort3 + 3,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvInputPort3 + 0,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvInputPort3 + 1,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvInputPort3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvInputPort3 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvInputPort0 + 3,	"p4 coin"	},
	{"P4 Up",			BIT_DIGITAL,	DrvInputPort4 + 2,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvInputPort4 + 3,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvInputPort4 + 0,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvInputPort4 + 1,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvInputPort4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvInputPort4 + 5,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvInputPort0 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvInputPort0 + 5,	"service2"	},
	{"Service 3",		BIT_DIGITAL,	DrvInputPort0 + 6,	"service3"	},
	{"Service 4",		BIT_DIGITAL,	DrvInputPort0 + 7,	"service4"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,			"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDip + 2,			"dip"		},
};

STDINPUTINFO(Punkshot)

static struct BurnInputInfo Punkshot2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvInputPort5 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvInputPort1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvInputPort1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvInputPort1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvInputPort1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvInputPort1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvInputPort0 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvInputPort5 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvInputPort2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvInputPort2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvInputPort2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvInputPort2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	DrvInputPort0 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,			"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDip + 2,			"dip"		},
};

STDINPUTINFO(Punkshot2)

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
	// Reset Inputs
	DrvInput[0] = DrvInput[1] = DrvInput[2] = DrvInput[3] = DrvInput[4] = DrvInput[5] = 0x00;

	// Compile Digital Inputs
	for (int i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvInputPort0[i] & 1) << i;
		DrvInput[1] |= (DrvInputPort1[i] & 1) << i;
		DrvInput[2] |= (DrvInputPort2[i] & 1) << i;
		DrvInput[3] |= (DrvInputPort3[i] & 1) << i;
		DrvInput[4] |= (DrvInputPort4[i] & 1) << i;
		DrvInput[5] |= (DrvInputPort5[i] & 1) << i;
	}

	// Clear Opposites
	DrvClearOpposites(&DrvInput[0]);
	DrvClearOpposites(&DrvInput[1]);
	DrvClearOpposites(&DrvInput[2]);
	DrvClearOpposites(&DrvInput[3]);
	DrvClearOpposites(&DrvInput[4]);
}

static inline void SsridersMakeInputs()
{
	DrvMakeInputs();

	DrvInput[5] ^= 0xf6;
	//if (DrvInputPort5[7]) DrvInput[5] -= 0x80;
}

static struct BurnDIPInfo SsridershsDIPList[] =
{
	DIP_OFFSET(0x22)

	// Fake DIPs
	{0x00, 0xff, 0xff, 0x00, NULL                           },  // 1v4

	{0,    0xfe, 0,    3,    "Version change (Must reload)" },
	{0x00, 0x01, 0x03, 0x00, "1v4"                          },
	{0x00, 0x01, 0x03, 0x01, "Firepower"                    },
	{0x00, 0x01, 0x03, 0x02, "Second Round"                 },
};

STDDIPINFO(Ssridershs)

static struct BurnDIPInfo TmntDIPList[]=
{
	// Default Values
	{0x21, 0xff, 0xff, 0xff, NULL                     },
	{0x22, 0xff, 0xff, 0x5e, NULL                     },
	{0x23, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 16   , "Coinage"               },
	{0x21, 0x01, 0x0f, 0x00, "5 Coins 1 Credit"       },
	{0x21, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"       },
	{0x21, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"       },
	{0x21, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"       },
	{0x21, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"      },
	{0x21, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"      },
	{0x21, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"       },
	{0x21, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"      },
	{0x21, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"      },
	{0x21, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"      },
	{0x21, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"      },
	{0x21, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"      },
	{0x21, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"      },
	{0x21, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"      },
	{0x21, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"      },
	{0x21, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"      },

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x22, 0x01, 0x03, 0x03, "1"                      },
	{0x22, 0x01, 0x03, 0x02, "2"                      },
	{0x22, 0x01, 0x03, 0x01, "3"                      },
	{0x22, 0x01, 0x03, 0x00, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x22, 0x01, 0x60, 0x60, "Easy"                   },
	{0x22, 0x01, 0x60, 0x40, "Normal"                 },
	{0x22, 0x01, 0x60, 0x20, "Difficult"              },
	{0x22, 0x01, 0x60, 0x00, "Very Difficult"         },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x22, 0x01, 0x80, 0x80, "Off"                    },
	{0x22, 0x01, 0x80, 0x00, "On"                     },
	
	// Dip 3
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x23, 0x01, 0x01, 0x01, "Off"                    },
	{0x23, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Test Mode"              },
	{0x23, 0x01, 0x04, 0x04, "Off"                    },
	{0x23, 0x01, 0x04, 0x00, "On"                     },
};

STDDIPINFO(Tmnt)

static struct BurnDIPInfo Tmnt2pDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                     },
	{0x14, 0xff, 0xff, 0x5e, NULL                     },
	{0x15, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 16   , "Coin A"                },
	{0x13, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"       },
	{0x13, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"       },
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"       },
	{0x13, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"      },
	{0x13, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"      },
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"       },
	{0x13, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"      },
	{0x13, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"      },
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"      },
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"      },
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"      },
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"      },
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"      },
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"      },
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"      },
	{0x13, 0x01, 0x0f, 0x00, "Free Play"              },
	
	{0   , 0xfe, 0   , 15   , "Coin B"                },
	{0x13, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"       },
	{0x13, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"       },
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"       },
	{0x13, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"      },
	{0x13, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"      },
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"       },
	{0x13, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"      },
	{0x13, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"      },
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"      },
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"      },
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"      },
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"      },
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"      },
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"      },
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"      },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x14, 0x01, 0x03, 0x03, "1"                      },
	{0x14, 0x01, 0x03, 0x02, "2"                      },
	{0x14, 0x01, 0x03, 0x01, "3"                      },
	{0x14, 0x01, 0x03, 0x00, "5"                      },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x14, 0x01, 0x60, 0x60, "Easy"                   },
	{0x14, 0x01, 0x60, 0x40, "Normal"                 },
	{0x14, 0x01, 0x60, 0x20, "Difficult"              },
	{0x14, 0x01, 0x60, 0x00, "Very Difficult"         },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x14, 0x01, 0x80, 0x80, "Off"                    },
	{0x14, 0x01, 0x80, 0x00, "On"                     },
	
	// Dip 3
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x15, 0x01, 0x01, 0x01, "Off"                    },
	{0x15, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Test Mode"              },
	{0x15, 0x01, 0x04, 0x04, "Off"                    },
	{0x15, 0x01, 0x04, 0x00, "On"                     },
};

STDDIPINFO(Tmnt2p)

static struct BurnDIPInfo MiaDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xff, NULL                     },
	{0x15, 0xff, 0xff, 0x56, NULL                     },
	{0x16, 0xff, 0xff, 0xff, NULL                     },
	
	// Dip 1
	{0   , 0xfe, 0   , 16   , "Coin A"                },
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"       },
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"       },
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"       },
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"      },
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"      },
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"       },
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"      },
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"      },
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"      },
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"      },
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"      },
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"      },
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"      },
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"      },
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"      },
	{0x14, 0x01, 0x0f, 0x00, "Free Play"              },
	
	{0   , 0xfe, 0   , 15   , "Coin B"                },
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"       },
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"       },
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"       },
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"      },
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"      },
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"       },
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"      },
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"      },
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"      },
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"      },
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"      },
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"      },
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"      },
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"      },
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"      },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x15, 0x01, 0x03, 0x03, "2"                      },
	{0x15, 0x01, 0x03, 0x02, "3"                      },
	{0x15, 0x01, 0x03, 0x01, "5"                      },
	{0x15, 0x01, 0x03, 0x00, "7"                      },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"             },
	{0x15, 0x01, 0x18, 0x18, "30k  80k"               },
	{0x15, 0x01, 0x18, 0x10, "50k 100k"               },
	{0x15, 0x01, 0x18, 0x08, "50k"                    },
	{0x15, 0x01, 0x18, 0x00, "100k"                   },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"             },
	{0x15, 0x01, 0x60, 0x60, "Easy"                   },
	{0x15, 0x01, 0x60, 0x40, "Normal"                 },
	{0x15, 0x01, 0x60, 0x20, "Difficult"              },
	{0x15, 0x01, 0x60, 0x00, "Very Difficult"         },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"            },
	{0x15, 0x01, 0x80, 0x80, "Off"                    },
	{0x15, 0x01, 0x80, 0x00, "On"                     },
	
	// Dip 3
	{0   , 0xfe, 0   , 2   , "Flip Screen"            },
	{0x16, 0x01, 0x01, 0x01, "Off"                    },
	{0x16, 0x01, 0x01, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "VRAM Character Check"   },
	{0x16, 0x01, 0x02, 0x02, "Off"                    },
	{0x16, 0x01, 0x02, 0x00, "On"                     },
	
	{0   , 0xfe, 0   , 2   , "Test Mode"              },
	{0x16, 0x01, 0x04, 0x04, "Off"                    },
	{0x16, 0x01, 0x04, 0x00, "On"                     },
};

STDDIPINFO(Mia)

static struct BurnDIPInfo LgtnfghtDIPList[]=
{
	{0x14, 0xff, 0xff, 0x5e, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xfd, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "2"			},
	{0x14, 0x01, 0x03, 0x02, "3"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x18, 0x18, "100000 400000"	},
	{0x14, 0x01, 0x18, 0x10, "150000 500000"	},
	{0x14, 0x01, 0x18, 0x08, "200000"		},
	{0x14, 0x01, 0x18, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x60, 0x60, "Easy"			},
	{0x14, 0x01, 0x60, 0x40, "Medium"		},
	{0x14, 0x01, 0x60, 0x20, "Hard"			},
	{0x14, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x15, 0x01, 0x0f, 0x02, "4 Coins 1 Credit" 	},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 1 Credit" 	},
	{0x15, 0x01, 0x0f, 0x08, "2 Coins 1 Credit" 	},
	{0x15, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit" 	},
	{0x15, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x15, 0x01, 0xf0, 0x20, "4 Coins 1 Credit" 	},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 1 Credit" 	},
	{0x15, 0x01, 0xf0, 0x80, "2 Coins 1 Credit" 	},
	{0x15, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit" 	},
	{0x15, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x15, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x16, 0x01, 0x02, 0x02, "Mono"			},
	{0x16, 0x01, 0x02, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x04, 0x04, "Off"			},
	{0x16, 0x01, 0x04, 0x00, "On"			},
};

STDDIPINFO(Lgtnfght)

static struct BurnDIPInfo PunkshotDIPList[]=
{
	{0x21, 0xff, 0xff, 0xff, NULL			},
	{0x22, 0xff, 0xff, 0x7f, NULL			},
	{0x23, 0xff, 0xff, 0xf0, NULL			},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x21, 0x01, 0x0f, 0x00, "5 Coins 1 Credit" 	},
	{0x21, 0x01, 0x0f, 0x02, "4 Coins 1 Credit" 	},
	{0x21, 0x01, 0x0f, 0x05, "3 Coins 1 Credit" 	},
	{0x21, 0x01, 0x0f, 0x08, "2 Coins 1 Credit" 	},
	{0x21, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x21, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x21, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit" 	},
	{0x21, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x21, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x21, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x21, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x21, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x21, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x21, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x21, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x21, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Continue"		},
	{0x21, 0x01, 0x10, 0x10, "Normal"		},
	{0x21, 0x01, 0x10, 0x00, "1 Coin"		},

	{0   , 0xfe, 0   ,    4, "Energy"		},
	{0x22, 0x01, 0x03, 0x03, "30"			},
	{0x22, 0x01, 0x03, 0x02, "40"			},
	{0x22, 0x01, 0x03, 0x01, "50"			},
	{0x22, 0x01, 0x03, 0x00, "60"			},

	{0   , 0xfe, 0   ,    4, "Period Length"	},
	{0x22, 0x01, 0x0c, 0x0c, "2 Minutes"		},
	{0x22, 0x01, 0x0c, 0x08, "3 Minutes"		},
	{0x22, 0x01, 0x0c, 0x04, "4 Minutes"		},
	{0x22, 0x01, 0x0c, 0x00, "5 Minutes"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x22, 0x01, 0x60, 0x60, "Easy"			},
	{0x22, 0x01, 0x60, 0x40, "Medium"		},
	{0x22, 0x01, 0x60, 0x20, "Hard"			},
	{0x22, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x22, 0x01, 0x80, 0x80, "Off"			},
	{0x22, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x23, 0x01, 0x10, 0x10, "Off"			},
	{0x23, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x23, 0x01, 0x40, 0x40, "Off"			},
	{0x23, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x23, 0x01, 0x80, 0x80, "Off"			},
	{0x23, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Punkshot)

static struct BurnDIPInfo Punkshot2DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x7f, NULL			},
	{0x14, 0xff, 0xff, 0xf0, NULL			},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x12, 0x01, 0x0f, 0x00, "5 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Continue"		},
	{0x12, 0x01, 0x10, 0x10, "Normal"		},
	{0x12, 0x01, 0x10, 0x00, "1 Coin"		},

	{0   , 0xfe, 0   ,    4, "Energy"		},
	{0x13, 0x01, 0x03, 0x03, "40"			},
	{0x13, 0x01, 0x03, 0x02, "50"			},
	{0x13, 0x01, 0x03, 0x01, "60"			},
	{0x13, 0x01, 0x03, 0x00, "70"			},

	{0   , 0xfe, 0   ,    4, "Period Length"	},
	{0x13, 0x01, 0x0c, 0x0c, "3 Minutes"		},
	{0x13, 0x01, 0x0c, 0x08, "4 Minutes"		},
	{0x13, 0x01, 0x0c, 0x04, "5 Minutes"		},
	{0x13, 0x01, 0x0c, 0x00, "6 Minutes"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Medium"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Punkshot2)

static struct BurnDIPInfo PunkshotjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x7f, NULL			},
	{0x14, 0xff, 0xff, 0xf0, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit" 	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credit" 	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credit" 	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credit" 	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit" 	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Period Time"	},
	{0x13, 0x01, 0x0c, 0x0c, "1 Minute" 		},
	{0x13, 0x01, 0x0c, 0x08, "2 Minutes"		},
	{0x13, 0x01, 0x0c, 0x04, "3 Minutes"		},
	{0x13, 0x01, 0x0c, 0x00, "4 Minutes"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Medium"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Punkshotj)

static struct BurnRomInfo TmntRomDesc[] = {
	{ "963-x23.j17",        0x020000, 0xa9549004, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-x24.k17",        0x020000, 0xe5cc9067, BRF_ESS | BRF_PRG }, //  1
	{ "963-x21.j15",        0x010000, 0x5789cf92, BRF_ESS | BRF_PRG }, //  2
	{ "963-x22.k15",        0x010000, 0x0a74e277, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	   	   //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	   	   //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	   	   // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	   	   // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmnt)
STD_ROM_FN(Tmnt)

static struct BurnRomInfo TmntuRomDesc[] = {
	{ "963-r23.j17",        0x020000, 0xa7f61195, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-r24.k17",        0x020000, 0x661e056a, BRF_ESS | BRF_PRG }, //  1
	{ "963-r21.j15",        0x010000, 0xde047bb6, BRF_ESS | BRF_PRG }, //  2
	{ "963-r22.k15",        0x010000, 0xd86a0888, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	   	   // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmntu)
STD_ROM_FN(Tmntu)

static struct BurnRomInfo TmntuaRomDesc[] = {
	{ "963-n23.j17",        0x020000, 0x388c333f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-n24.k17",        0x020000, 0xaf3efd63, BRF_ESS | BRF_PRG }, //  1
	{ "963-j21.j15",        0x010000, 0x7bee9fe8, BRF_ESS | BRF_PRG }, //  2
	{ "963-j22.k15",        0x010000, 0x2efed09f, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmntua)
STD_ROM_FN(Tmntua)

static struct BurnRomInfo TmntubRomDesc[] = {
	{ "963-j23.j17",        0x020000, 0xf77314e2, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-j24.k17",        0x020000, 0x47f662d3, BRF_ESS | BRF_PRG }, //  1
	{ "963-j21.j15",        0x010000, 0x7bee9fe8, BRF_ESS | BRF_PRG }, //  2
	{ "963-j22.k15",        0x010000, 0x2efed09f, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmntub)
STD_ROM_FN(Tmntub)

static struct BurnRomInfo TmntucRomDesc[] = {
	{ "963-h23.j17",        0x020000, 0x718086e1, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-h24.k17",        0x020000, 0x2f7d66e1, BRF_ESS | BRF_PRG }, //  1
	{ "963-h21.j15",        0x010000, 0x1944641e, BRF_ESS | BRF_PRG }, //  2
	{ "963-h22.k15",        0x010000, 0x50ce5512, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmntuc)
STD_ROM_FN(Tmntuc)

static struct BurnRomInfo TmhtRomDesc[] = {
	{ "963-f23.j17",        0x020000, 0x9cb5e461, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-f24.k17",        0x020000, 0x2d902fab, BRF_ESS | BRF_PRG }, //  1
	{ "963-f21.j15",        0x010000, 0x9fa25378, BRF_ESS | BRF_PRG }, //  2
	{ "963-f22.k15",        0x010000, 0x2127ee53, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmht)
STD_ROM_FN(Tmht)

static struct BurnRomInfo TmhtaRomDesc[] = {
	{ "963-s23.j17",        0x020000, 0xb5af7eee, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-s24.k17",        0x020000, 0xbcb8ce8b, BRF_ESS | BRF_PRG }, //  1
	{ "963-s21.j15",        0x010000, 0x0b88bfa6, BRF_ESS | BRF_PRG }, //  2
	{ "963-s22.k15",        0x010000, 0x44ce6d4b, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmhta)
STD_ROM_FN(Tmhta)

static struct BurnRomInfo TmhtbRomDesc[] = {
	{ "unk3.i17",        	0x020000, 0x537eb047, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "unk4.k17",        	0x020000, 0x5afae564, BRF_ESS | BRF_PRG }, //  1
	{ "unk2.j15",        	0x010000, 0xee34de05, BRF_ESS | BRF_PRG }, //  2
	{ "unk5.k15",        	0x010000, 0x5ef58d4e, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmhtb)
STD_ROM_FN(Tmhtb)

static struct BurnRomInfo TmntjRomDesc[] = {
	{ "963_223.j17",        0x020000, 0x0d34a5ff, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963_224.k17",        0x020000, 0x2fd453f2, BRF_ESS | BRF_PRG }, //  1
	{ "963_221.j15",        0x010000, 0xfa8e25fd, BRF_ESS | BRF_PRG }, //  2
	{ "963_222.k15",        0x010000, 0xca437a4f, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmntj)
STD_ROM_FN(Tmntj)

static struct BurnRomInfo TmntaRomDesc[] = {
	{ "tmnt j17.bin",       0x020000, 0x00819687, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "tmnt k17.bin",       0x020000, 0x6930e085, BRF_ESS | BRF_PRG }, //  1
	{ "tmnt j15.bin",       0x010000, 0xfd1e2e01, BRF_ESS | BRF_PRG }, //  2
	{ "tmnt k15.bin",       0x010000, 0xb01eea79, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmnta)
STD_ROM_FN(Tmnta)

static struct BurnRomInfo Tmht2pRomDesc[] = {
	{ "963-u23.j17",        0x020000, 0x58bec748, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-u24.k17",        0x020000, 0xdce87c8d, BRF_ESS | BRF_PRG }, //  1
	{ "963-u21.j15",        0x010000, 0xabce5ead, BRF_ESS | BRF_PRG }, //  2
	{ "963-u22.k15",        0x010000, 0x4ecc8d6b, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmht2p)
STD_ROM_FN(Tmht2p)

static struct BurnRomInfo Tmht2paRomDesc[] = {
	{ "963-_23.j17",        0x020000, 0x8698061a, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-_24.k17",        0x020000, 0x4036c075, BRF_ESS | BRF_PRG }, //  1
	{ "963-_21.j15",        0x010000, 0xddcc979c, BRF_ESS | BRF_PRG }, //  2
	{ "963-_22.k15",        0x010000, 0x71a38d27, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmht2pa)
STD_ROM_FN(Tmht2pa)

static struct BurnRomInfo Tmnt2pjRomDesc[] = {
	{ "963-123.j17",        0x020000, 0x6a3527c9, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "963-124.k17",        0x020000, 0x2c4bfa15, BRF_ESS | BRF_PRG }, //  1
	{ "963-121.j15",        0x010000, 0x4181b733, BRF_ESS | BRF_PRG }, //  2
	{ "963-122.k15",        0x010000, 0xc64eb5ff, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmnt2pj)
STD_ROM_FN(Tmnt2pj)

static struct BurnRomInfo Tmnt2poRomDesc[] = {
	{ "tmnt123.j17",        0x020000, 0x2d905183, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "tmnt124.k17",        0x020000, 0xe0125352, BRF_ESS | BRF_PRG }, //  1
	{ "tmnt21.j15",         0x010000, 0x12deeafb, BRF_ESS | BRF_PRG }, //  2
	{ "tmnt22.k15",         0x010000, 0xaec4f1c3, BRF_ESS | BRF_PRG }, //  3
	
	{ "963e20.g13",         0x008000, 0x1692a6d6, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "963a28.h27",         0x080000, 0xdb4769a8, BRF_GRA },	       //  5	Tiles
	{ "963a29.k27",         0x080000, 0x8069cd2e, BRF_GRA },	       //  6
	
	{ "963a17.h4",          0x080000, 0xb5239a44, BRF_GRA },	       //  7	Sprites
	{ "963a18.h6",          0x080000, 0xdd51adef, BRF_GRA },	       //  8
	{ "963a15.k4",          0x080000, 0x1f324eed, BRF_GRA },	       //  9
	{ "963a16.k6",          0x080000, 0xd4bd9984, BRF_GRA },	       // 10
	
	{ "963a30.g7",          0x000100, 0xabd82680, BRF_GRA },	       // 11	PROMs
	{ "963a31.g19",         0x000100, 0xf8004a1c, BRF_GRA },	       // 12
	
	{ "963a26.c13",         0x020000, 0xe2ac3063, BRF_SND },	       // 13	K007232 Samples
	
	{ "963a27.d18",         0x020000, 0x2dfd674b, BRF_SND },	       // 14	UP7759C Samples
	
	{ "963a25.d5",          0x080000, 0xfca078c7, BRF_SND },	       // 15	Title Music Sample
};

STD_ROM_PICK(Tmnt2po)
STD_ROM_FN(Tmnt2po)

static struct BurnRomInfo MiaRomDesc[] = {
	{ "808t20.h17",         0x020000, 0x6f0acb1d, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "808t21.j17",         0x020000, 0x42a30416, BRF_ESS | BRF_PRG }, //  1
	
	{ "808e03.f4",          0x008000, 0x3d93a7cd, BRF_ESS | BRF_PRG }, //  2	Z80 Program 
	
	{ "808e12.f28",         0x010000, 0xd62f1fde, BRF_GRA },	       //  3	Tiles
	{ "808e13.h28",         0x010000, 0x1fa708f4, BRF_GRA },	       //  4
	{ "808e22.i28",         0x010000, 0x73d758f6, BRF_GRA },	       //  5
	{ "808e23.k28",         0x010000, 0x8ff08b21, BRF_GRA },	       //  6
	
	{ "808d17.j4",          0x080000, 0xd1299082, BRF_GRA },	       //  7	Sprites
	{ "808d15.h4",          0x080000, 0x2b22a6b6, BRF_GRA },	       //  8
	
	{ "808a18.f16",         0x000100, 0xeb95aede, BRF_GRA },	       //  9	PROMs
	
	{ "808d01.d4",          0x020000, 0xfd4d37c0, BRF_SND },	       // 10	K007232 Samples
};

STD_ROM_PICK(Mia)
STD_ROM_FN(Mia)

static struct BurnRomInfo Mia2RomDesc[] = {
	{ "808s20.h17",         0x020000, 0xcaa2897f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "808s21.j17",         0x020000, 0x3d892ffb, BRF_ESS | BRF_PRG }, //  1
	
	{ "808e03.f4",          0x008000, 0x3d93a7cd, BRF_ESS | BRF_PRG }, //  2	Z80 Program 
	
	{ "808e12.f28",         0x010000, 0xd62f1fde, BRF_GRA },	       //  3	Tiles
	{ "808e13.h28",         0x010000, 0x1fa708f4, BRF_GRA },	       //  4
	{ "808e22.i28",         0x010000, 0x73d758f6, BRF_GRA },	       //  5
	{ "808e23.k28",         0x010000, 0x8ff08b21, BRF_GRA },	       //  6
	
	{ "808d17.j4",          0x080000, 0xd1299082, BRF_GRA },	       //  7	Sprites
	{ "808d15.h4",          0x080000, 0x2b22a6b6, BRF_GRA },	       //  8
	
	{ "808a18.f16",         0x000100, 0xeb95aede, BRF_GRA },	       //  9	PROMs
	
	{ "808d01.d4",          0x020000, 0xfd4d37c0, BRF_SND },	       // 10	K007232 Samples
};

STD_ROM_PICK(Mia2)
STD_ROM_FN(Mia2)

static struct BurnRomInfo CuebrickRomDesc[] = {
	{ "903d25.g12",         0x010000, 0x8d575663, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "903d24.f12",         0x010000, 0x2973625d, BRF_ESS | BRF_PRG }, //  1
	
	{ "903c29.k21",         0x010000, 0xfada986d, BRF_GRA },	       //  2	Tiles
	{ "903c27.k17",         0x010000, 0x5bd4b8e1, BRF_GRA },	       //  3
	{ "903c28.k19",         0x010000, 0x80d2bfaf, BRF_GRA },	       //  4
	{ "903c26.k15",         0x010000, 0xf808fa3d, BRF_GRA },	       //  5
	
	{ "903d23.k12",         0x010000, 0xc39fc9fd, BRF_GRA },	       //  6	Sprites
	{ "903d21.k8",          0x010000, 0x3c7bf8cd, BRF_GRA },	       //  7
	{ "903d22.k10",         0x010000, 0x95ad8591, BRF_GRA },	       //  8
	{ "903d20.k6",          0x010000, 0x2872a1bb, BRF_GRA },	       //  9
};

STD_ROM_PICK(Cuebrick)
STD_ROM_FN(Cuebrick)

static struct BurnRomInfo BlswhstlRomDesc[] = {
	{ "060_l02.e09",        0x020000, 0xe8b7b234, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "060_l03.g09",        0x020000, 0x3c26d281, BRF_ESS | BRF_PRG }, //  1
	{ "060_l09.e11",        0x020000, 0x14628736, BRF_ESS | BRF_PRG }, //  2
	{ "060_l10.g11",        0x020000, 0xf738ad4a, BRF_ESS | BRF_PRG }, //  3
	
	{ "060j01.f3",          0x010000, 0xf9d9a673, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "060e07.k16",         0x080000, 0xc400edf3, BRF_GRA },	       //  5	Tiles
	{ "060e08.k12",         0x080000, 0x70dddba1, BRF_GRA },	       //  6
	
	{ "060e06.k7",          0x080000, 0x09381492, BRF_GRA },	       //  7	Sprites
	{ "060e05.k3",          0x080000, 0x32454241, BRF_GRA },	       //  8
	
	{ "060e04.d1",          0x100000, 0xc680395d, BRF_SND },	       //  9	K053260 Samples

	{ "blswhstl.nv",  		0x000080, 0x87434e3f, BRF_OPT },
};

STD_ROM_PICK(Blswhstl)
STD_ROM_FN(Blswhstl)

static struct BurnRomInfo BlswhstlaRomDesc[] = {
	{ "060_m02.e09",        0x020000, 0xbc9dd08f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "060_m03.g09",        0x020000, 0x7b6ee4a4, BRF_ESS | BRF_PRG }, //  1
	{ "060_m09.e11",        0x020000, 0x14628736, BRF_ESS | BRF_PRG }, //  2
	{ "060_m10.g11",        0x020000, 0xf738ad4a, BRF_ESS | BRF_PRG }, //  3
	
	{ "060j01.f3",          0x010000, 0xf9d9a673, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "060e07.k16",         0x080000, 0xc400edf3, BRF_GRA },	       //  5	Tiles
	{ "060e08.k12",         0x080000, 0x70dddba1, BRF_GRA },	       //  6
	
	{ "060e06.k7",          0x080000, 0x09381492, BRF_GRA },	       //  7	Sprites
	{ "060e05.k3",          0x080000, 0x32454241, BRF_GRA },	       //  8
	
	{ "060e04.d1",          0x100000, 0xc680395d, BRF_SND },	       //  9	K053260 Samples

	{ "blswhstl.nv",  		0x000080, 0x87434e3f, BRF_OPT },
};

STD_ROM_PICK(Blswhstla)
STD_ROM_FN(Blswhstla)

static struct BurnRomInfo DetatwinRomDesc[] = {
	{ "060_j02.e09",        0x020000, 0x11b761ac, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "060_j03.g09",        0x020000, 0x8d0b588c, BRF_ESS | BRF_PRG }, //  1
	{ "060_j09.e11",        0x020000, 0xf2a5f15f, BRF_ESS | BRF_PRG }, //  2
	{ "060_j10.g11",        0x020000, 0x36eefdbc, BRF_ESS | BRF_PRG }, //  3
	
	{ "060j01.f3",          0x010000, 0xf9d9a673, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "060e07.k16",         0x080000, 0xc400edf3, BRF_GRA },	   	   //  5	Tiles
	{ "060e08.k12",         0x080000, 0x70dddba1, BRF_GRA },	       //  6
	
	{ "060e06.k7",          0x080000, 0x09381492, BRF_GRA },	       //  7	Sprites
	{ "060e05.k3",          0x080000, 0x32454241, BRF_GRA },	       //  8
	
	{ "060e04.d1",          0x100000, 0xc680395d, BRF_SND },	       //  9	K053260 Samples

	{ "blswhstl.nv",  		0x000080, 0x87434e3f, BRF_OPT },
};

STD_ROM_PICK(Detatwin)
STD_ROM_FN(Detatwin)

static struct BurnRomInfo SsridersRomDesc[] = {
	{ "064eac02.8e",        0x040000, 0x5a5425f4, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064eac03.8g",        0x040000, 0x093c00fb, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_eac.nv",  	0x000080, 0xf6d641a7, BRF_OPT },
};

STD_ROM_PICK(Ssriders)
STD_ROM_FN(Ssriders)

static struct BurnRomInfo SsridersebdRomDesc[] = {
	{ "064ebd02.8e",        0x040000, 0x8deef9ac, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064ebd03.8g",        0x040000, 0x2370c107, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_ebd.nv",  	0x000080, 0xcbc903f6, BRF_OPT },
};

STD_ROM_PICK(Ssridersebd)
STD_ROM_FN(Ssridersebd)

static struct BurnRomInfo SsridersebcRomDesc[] = {
	{ "064ebc02.8e",        0x040000, 0x9bd7d164, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064ebc03.8g",        0x040000, 0x40fd4165, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_ebc.nv",  	0x000080, 0x1918e90f, BRF_OPT },
};

STD_ROM_PICK(Ssridersebc)
STD_ROM_FN(Ssridersebc)

static struct BurnRomInfo SsridersudaRomDesc[] = {
	{ "064uda02.8e",        0x040000, 0x5129a6b7, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064uda03.8g",        0x040000, 0x9f887214, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_uda.nv",  	0x000080, 0x148c6d64, BRF_OPT },
};

STD_ROM_PICK(Ssridersuda)
STD_ROM_FN(Ssridersuda)

static struct BurnRomInfo SsriderseaaRomDesc[] = {
	{ "064eaa02.8e",        0x040000, 0x4844660f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064eaa03.8g",        0x040000, 0x0b9bcc7c, BRF_ESS | BRF_PRG }, //  1
	{ "064eaa04.10e",       0x020000, 0x5d917c1c, BRF_ESS | BRF_PRG }, //  2
	{ "064eaa05.10g",       0x020000, 0xf4647b74, BRF_ESS | BRF_PRG }, //  3
	
	{ "064eaa01.2f",        0x010000, 0xbce45d82, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_eaa.nv",  	0x000080, 0x74a45ef5, BRF_OPT },
};

STD_ROM_PICK(Ssriderseaa)
STD_ROM_FN(Ssriderseaa)

static struct BurnRomInfo SsridersuabRomDesc[] = {
	{ "064uab02.8e",        0x040000, 0xf1a3c548, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064uab03.8g",        0x040000, 0x66a61287, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_uab.nv",  	0x000080, 0xfe08b210, BRF_OPT },
};

STD_ROM_PICK(Ssridersuab)
STD_ROM_FN(Ssridersuab)

static struct BurnRomInfo SsridersuacRomDesc[] = {
	{ "064uac02.8e",        0x040000, 0x870473b6, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064uac03.8g",        0x040000, 0xeadf289a, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_uac.nv",  	0x000080, 0x26c8f1a0, BRF_OPT },
};

STD_ROM_PICK(Ssridersuac)
STD_ROM_FN(Ssridersuac)

static struct BurnRomInfo SsridersubcRomDesc[] = {
	{ "064ubc02.8e",        0x040000, 0xaca7fda5, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064ubc03.8g",        0x040000, 0xbb1fdeff, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_ubc.nv",  	0x000080, 0x31c5504f, BRF_OPT },
};

STD_ROM_PICK(Ssridersubc)
STD_ROM_FN(Ssridersubc)

static struct BurnRomInfo SsridersabdRomDesc[] = {
	{ "064abd02.8e",        0x040000, 0x713406cb, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064abd03.8g",        0x040000, 0x680feb3c, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_abd.nv",  	0x000080, 0xbfdafedf, BRF_OPT },
};

STD_ROM_PICK(Ssridersabd)
STD_ROM_FN(Ssridersabd)

static struct BurnRomInfo SsridersaddRomDesc[] = {
	{ "064add02.8e",        0x040000, 0x06b0138e, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064add03.8g",        0x040000, 0x03eb8b91, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_add.nv",  	0x000080, 0xf06242d5, BRF_OPT },
};

STD_ROM_PICK(Ssridersadd)
STD_ROM_FN(Ssridersadd)

static struct BurnRomInfo SsridersjacRomDesc[] = {
	{ "064jac02.8e",        0x040000, 0x64a35f6f, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064jac03.8g",        0x040000, 0xb5957946, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_jac.nv",  	0x000080, 0xeeb0c55f, BRF_OPT },
};

STD_ROM_PICK(Ssridersjac)
STD_ROM_FN(Ssridersjac)

static struct BurnRomInfo SsridersjadRomDesc[] = {
	{ "064jad02.8e",        0x040000, 0x13709ee6, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064jad03.8g",        0x040000, 0x2fa32229, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_jad.nv",  	0x000080, 0x8962058c, BRF_OPT },
};

STD_ROM_PICK(Ssridersjad)
STD_ROM_FN(Ssridersjad)

static struct BurnRomInfo SsridersjbdRomDesc[] = {
	{ "064jbd02.8e",        0x040000, 0x7acdc1e3, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "064jbd03.8g",        0x040000, 0x6a424918, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04.10e",       0x020000, 0xef2315bd, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05.10g",       0x020000, 0x51d6fbc4, BRF_ESS | BRF_PRG }, //  3
	
	{ "064e01.2f",          0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4	Z80 Program 
	
	{ "064e12.16k",         0x080000, 0xe2bdc619, BRF_GRA },	       //  5	Tiles
	{ "064e11.12k",         0x080000, 0x2d8ca8b0, BRF_GRA },	       //  6
	
	{ "064e09.7l",          0x100000, 0x4160c372, BRF_GRA },	       //  7	Sprites
	{ "064e07.3l",          0x100000, 0x64dd673c, BRF_GRA },	       //  8
	
	{ "064e06.1d",          0x100000, 0x59810df9, BRF_SND },	       //  9	K053260 Samples

	{ "ssriders_jbd.nv",  	0x000080, 0x006200e3, BRF_OPT },
};

STD_ROM_PICK(Ssridersjbd)
STD_ROM_FN(Ssridersjbd)

static struct BurnRomInfo ssridersbRomDesc[] = {
	{ "sunsetb.03",		0x080000, 0x37ffe90b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "sunsetb.04",		0x080000, 0x8ff647b7, BRF_ESS | BRF_PRG }, //  1

	{ "064e12.16k",		0x080000, 0xe2bdc619, BRF_GRA },	       //  2	Tiles
	{ "064e11.12k",		0x080000, 0x2d8ca8b0, BRF_GRA },	       //  3

	{ "sunsetb.05",		0x080000, 0x8a0ff31a, BRF_GRA },	       //  4	Sprites
	{ "sunsetb.06",		0x080000, 0xfdf2c887, BRF_GRA },	       //  5
	{ "sunsetb.07",		0x080000, 0xa545b1ed, BRF_GRA },	       //  6
	{ "sunsetb.08",		0x080000, 0xf867cd38, BRF_GRA },	       //  7

	{ "sunsetb.01",		0x080000, 0x1a8b5ca2, BRF_SND },	       //  8	OKI Samples
	{ "sunsetb.02",		0x080000, 0x5d485523, BRF_SND },	       //  9
};

STD_ROM_PICK(ssridersb)
STD_ROM_FN(ssridersb)

static struct BurnRomInfo thndrx2RomDesc[] = {
	{ "073-ea-l02.11c",	0x020000, 0xeae02b51, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "073-ea-l03.12c",	0x020000, 0x738ed007, BRF_ESS | BRF_PRG }, //  1

	{ "073-c01.4f",		0x010000, 0x44ebe83c, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "073-c06.16k",	0x080000, 0x24e22b42, BRF_GRA },           //  3 	Tiles
	{ "073-c05.12k",	0x080000, 0x952a935f, BRF_GRA },           //  4

	{ "073-c07.7k",		0x080000, 0x14e93f38, BRF_GRA },           //  5 	Sprites
	{ "073-c08.3k",		0x080000, 0x09fab3ab, BRF_GRA },           //  6

	{ "073-b04.2d",		0x080000, 0x05287a0b, BRF_SND },           //  7 	K053260 Samples

	{ "thndrx2.nv",  	0x000080, 0xca613e98, BRF_OPT },
};

STD_ROM_PICK(thndrx2)
STD_ROM_FN(thndrx2)

static struct BurnRomInfo thndrx2aRomDesc[] = {
	{ "073-aa-m02.11c",	0x020000, 0x5b5b4cc0, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "073-aa-m03.12c",	0x020000, 0x320435a8, BRF_ESS | BRF_PRG }, //  1

	{ "073-c01.4f",		0x010000, 0x44ebe83c, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "073-c06.16k",	0x080000, 0x24e22b42, BRF_GRA },           //  3 	Tiles
	{ "073-c05.12k",	0x080000, 0x952a935f, BRF_GRA },           //  4

	{ "073-c07.7k",		0x080000, 0x14e93f38, BRF_GRA },           //  5 	Sprites
	{ "073-c08.3k",		0x080000, 0x09fab3ab, BRF_GRA },           //  6

	{ "073-b04.2d",		0x080000, 0x05287a0b, BRF_SND },           //  7 	K053260 Samples

	{ "thndrx2a.nv",  	0x000080, 0xf7893d00, BRF_OPT },
};

STD_ROM_PICK(thndrx2a)
STD_ROM_FN(thndrx2a)

static struct BurnRomInfo thndrx2jRomDesc[] = {
	{ "073-ja-k02.11c",	0x020000, 0x0c8b2d3f, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "073-ja-k03.12c",	0x020000, 0x3803b427, BRF_ESS | BRF_PRG }, //  1

	{ "073-c01.4f",		0x010000, 0x44ebe83c, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "073-c06.16k",	0x080000, 0x24e22b42, BRF_GRA },           //  3 	Tiles
	{ "073-c05.12k",	0x080000, 0x952a935f, BRF_GRA },           //  4

	{ "073-c07.7k",		0x080000, 0x14e93f38, BRF_GRA },           //  5 	Sprites
	{ "073-c08.3k",		0x080000, 0x09fab3ab, BRF_GRA },           //  6

	{ "073-b04.2d",		0x080000, 0x05287a0b, BRF_SND },           //  7 	K053260 Samples

	{ "thndrx2j.nv",  	0x000080, 0x83b4799b, BRF_OPT },
};

STD_ROM_PICK(thndrx2j)
STD_ROM_FN(thndrx2j)

static struct BurnRomInfo lgtnfghtRomDesc[] = {
	{ "939k02.e11",		0x020000, 0x2dfefa53, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "939k03.e15",		0x020000, 0x14f0c454, BRF_ESS | BRF_PRG }, //  1

	{ "939e01.d7",		0x008000, 0x4a5fc848, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "939a07.k14",		0x080000, 0x7955dfcf, BRF_GRA },           //  3 	Tiles
	{ "939a08.k19",		0x080000, 0xed95b385, BRF_GRA },           //  4

	{ "939a06.k8",		0x080000, 0xe393c206, BRF_GRA },           //  5 	Sprites
	{ "939a05.k2",		0x080000, 0x3662d47a, BRF_GRA },           //  6

	{ "939a04.c5",		0x080000, 0xc24e2b6e, BRF_SND },           //  7 	K053260 Samples
};

STD_ROM_PICK(lgtnfght)
STD_ROM_FN(lgtnfght)

static struct BurnRomInfo lgtnfghtuRomDesc[] = {
	{ "939m02.e11",		0x020000, 0x61a12184, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "939m03.e15",		0x020000, 0x6db6659d, BRF_ESS | BRF_PRG }, //  1

	{ "939e01.d7",		0x008000, 0x4a5fc848, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "939a07.k14",		0x080000, 0x7955dfcf, BRF_GRA },           //  3 	Tiles
	{ "939a08.k19",		0x080000, 0xed95b385, BRF_GRA },           //  4

	{ "939a06.k8",		0x080000, 0xe393c206, BRF_GRA },           //  5 	Sprites
	{ "939a05.k2",		0x080000, 0x3662d47a, BRF_GRA },           //  6

	{ "939a04.c5",		0x080000, 0xc24e2b6e, BRF_SND },           //  7 	K053260 Samples
};

STD_ROM_PICK(lgtnfghtu)
STD_ROM_FN(lgtnfghtu)

static struct BurnRomInfo lgtnfghtaRomDesc[] = {
	{ "939j_02.e11",	0x020000, 0xbbbb4a74, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "939j_03.e15",	0x020000, 0x8d4da7b7, BRF_ESS | BRF_PRG }, //  1

	{ "939e01.d7",		0x008000, 0x4a5fc848, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "939a07.k14",		0x080000, 0x7955dfcf, BRF_GRA },           //  3 	Tiles
	{ "939a08.k19",		0x080000, 0xed95b385, BRF_GRA },           //  4

	{ "939a06.k8",		0x080000, 0xe393c206, BRF_GRA },           //  5 	Sprites
	{ "939a05.k2",		0x080000, 0x3662d47a, BRF_GRA },           //  6

	{ "939a04.c5",		0x080000, 0xc24e2b6e, BRF_SND },           //  7 	K053260 Samples
};

STD_ROM_PICK(lgtnfghta)
STD_ROM_FN(lgtnfghta)

static struct BurnRomInfo trigonRomDesc[] = {
	{ "939j02.e11",		0x020000, 0x38381d1b, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "939j03.e15",		0x020000, 0xb5beddcd, BRF_ESS | BRF_PRG }, //  1

	{ "939e01.d7",		0x008000, 0x4a5fc848, BRF_ESS | BRF_PRG }, //  2 	Z80 Code

	{ "939a07.k14",		0x080000, 0x7955dfcf, BRF_GRA },           //  3 	Tiles
	{ "939a08.k19",		0x080000, 0xed95b385, BRF_GRA },           //  4

	{ "939a06.k8",		0x080000, 0xe393c206, BRF_GRA },           //  5 	Sprites
	{ "939a05.k2",		0x080000, 0x3662d47a, BRF_GRA },           //  6

	{ "939a04.c5",		0x080000, 0xc24e2b6e, BRF_SND },           //  7 	K053260 Samples
};

STD_ROM_PICK(trigon)
STD_ROM_FN(trigon)

static struct BurnRomInfo tmnt2RomDesc[] = {
	{ "063uaa02.8e",	0x020000, 0x58d5c93d, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063uaa03.8g",	0x020000, 0x0541fec9, BRF_ESS | BRF_PRG }, //  1
	{ "063uaa04.10e",	0x020000, 0x1d441a7d, BRF_ESS | BRF_PRG }, //  2
	{ "063uaa05.10g",	0x020000, 0x9c428273, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_uaa.nv",  	0x000080, 0x284357bd, BRF_OPT },
};

STD_ROM_PICK(tmnt2)
STD_ROM_FN(tmnt2)

static struct BurnRomInfo tmnt22puRomDesc[] = {
	{ "063uda02.8e",	0x020000, 0xaadffe3a, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063uda03.8g",	0x020000, 0x125687a8, BRF_ESS | BRF_PRG }, //  1
	{ "063uda04.10e",	0x020000, 0xfb5c7ded, BRF_ESS | BRF_PRG }, //  2
	{ "063uda05.10g",	0x020000, 0x3c40fe66, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_uda.nv",  	0x000080, 0x44928d33, BRF_OPT },
};

STD_ROM_PICK(tmnt22pu)
STD_ROM_FN(tmnt22pu)

static struct BurnRomInfo tmnt24puRomDesc[] = {
	{ "063uea02.8e",	0x020000, 0x5eebc67f, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063uea03.8g",	0x020000, 0x1b956869, BRF_ESS | BRF_PRG }, //  1
	{ "063uea04.10e",	0x020000, 0xe13d93a6, BRF_ESS | BRF_PRG }, //  2
	{ "063uea05.10g",	0x020000, 0xa3a1f5ea, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_uea.nv",  	0x000080, 0x4f086faa, BRF_OPT },
};

STD_ROM_PICK(tmnt24pu)
STD_ROM_FN(tmnt24pu)

static struct BurnRomInfo tmht22peRomDesc[] = {
	{ "063eba02.8e",	0x020000, 0x99409094, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063eba03.8g",	0x020000, 0x4d65650f, BRF_ESS | BRF_PRG }, //  1
	{ "063eba04.10e",	0x020000, 0xf6e3b9c7, BRF_ESS | BRF_PRG }, //  2
	{ "063eba05.10g",	0x020000, 0x1bad6696, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_eba.nv",  	0x000080, 0xc0a3ed50, BRF_OPT },
};

STD_ROM_PICK(tmht22pe)
STD_ROM_FN(tmht22pe)

static struct BurnRomInfo tmht24peRomDesc[] = {
	{ "063eaa02.8e",	0x020000, 0x665a68de, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063eaa03.8g",	0x020000, 0x7b7fb3af, BRF_ESS | BRF_PRG }, //  1
	{ "063eaa04.10e",	0x020000, 0x69f38e1d, BRF_ESS | BRF_PRG }, //  2
	{ "063eaa05.10g",	0x020000, 0x818032af, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_eaa.nv",  	0x000080, 0x124af18f, BRF_OPT },
};

STD_ROM_PICK(tmht24pe)
STD_ROM_FN(tmht24pe)

static struct BurnRomInfo tmnt2aRomDesc[] = {
	{ "063ada02.8e",	0x020000, 0x4f11b587, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "063ada03.8g",	0x020000, 0x82a1b9ac, BRF_ESS | BRF_PRG }, //  1
	{ "063ada04.10e",	0x020000, 0x05ad187a, BRF_ESS | BRF_PRG }, //  2
	{ "063ada05.10g",	0x020000, 0xd4826547, BRF_ESS | BRF_PRG }, //  3

	{ "063b01.2f",		0x010000, 0x364f548a, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "063b12.16k",		0x080000, 0xd3283d19, BRF_GRA },           //  5	Tiles
	{ "063b11.12k",		0x080000, 0x6ebc0c15, BRF_GRA },           //  6

	{ "063b09.7l",		0x100000, 0x2d7a9d2a, BRF_GRA },           //  7	Sprites
	{ "063b10.7k",		0x080000, 0xf2dd296e, BRF_GRA },           //  8
	{ "063b07.3l",		0x100000, 0xd9bee7bf, BRF_GRA },           //  9
	{ "063b08.3k",		0x080000, 0x3b1ae36f, BRF_GRA },           // 10

	{ "063b06.1d",		0x200000, 0x1e510aa5, BRF_SND },           // 11	K053260 Samples

	{ "tmnt2_ada.nv",  	0x000080, 0x063068a0, BRF_OPT },
};

STD_ROM_PICK(tmnt2a)
STD_ROM_FN(tmnt2a)

static struct BurnRomInfo qgakumonRomDesc[] = {
	{ "248jaa02.8e",	0x040000, 0xfab79410, BRF_ESS | BRF_PRG }, //  0 	68000 Program Code
	{ "248jaa03.8g",	0x040000, 0x8d888ef3, BRF_ESS | BRF_PRG }, //  1
	{ "248jaa04.10e",	0x040000, 0x56cb16cb, BRF_ESS | BRF_PRG }, //  2
	{ "248jaa05.10g",	0x040000, 0x27614fcd, BRF_ESS | BRF_PRG }, //  3

	{ "248a01.2f",		0x010000, 0xa8a41cc6, BRF_ESS | BRF_PRG }, //  4 	Z80 Code

	{ "248a12.16k",		0x080000, 0x62870987, BRF_GRA },           //  5 	Tiles
	{ "248a11.12k",		0x080000, 0xfad2dbfd, BRF_GRA },           //  6

	{ "248a09.7l",		0x100000, 0xa176e205, BRF_GRA },           //  7	Sprites
	{ "248a07.3l",		0x100000, 0x9595589f, BRF_GRA },           //  8

	{ "248a06.1d",		0x200000, 0x0fba1def, BRF_SND },           //  9	K053260 Samples

	{ "qgakumon.nv",  	0x000080, 0x847bd238, BRF_OPT },
};

STD_ROM_PICK(qgakumon)
STD_ROM_FN(qgakumon)

static struct BurnRomInfo punkshotRomDesc[] = {
	{ "907-j02.i7",		0x020000, 0xdbb3a23b, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "907-j03.i10",	0x020000, 0x2151d1ab, BRF_ESS | BRF_PRG }, //  1

	{ "907f01.e8",		0x008000, 0xf040c484, BRF_ESS | BRF_PRG }, //  2	Z80 Program

	{ "907d06.e23",		0x040000, 0xf5cc38f4, BRF_GRA },	   	   //  3	Tiles
	{ "907d05.e22",		0x040000, 0xe25774c1, BRF_GRA },	       //  4

	{ "907d07.k2",		0x100000, 0xb0fe4543, BRF_GRA },	       //  5	Sprites
	{ "907d08.k7",		0x100000, 0xd5ac8d9d, BRF_GRA },	       //  6

	{ "907d04.d3",		0x080000, 0x090feb5e, BRF_SND },	       //  7	K053260 Samples
};

STD_ROM_PICK(punkshot)
STD_ROM_FN(punkshot)

static struct BurnRomInfo punkshot2RomDesc[] = {
	{ "907m02.i7",		0x020000, 0x59e14575, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "907m03.i10",		0x020000, 0xadb14b1e, BRF_ESS | BRF_PRG }, //  1

	{ "907f01.e8",		0x008000, 0xf040c484, BRF_ESS | BRF_PRG }, //  2	Z80 Program

	{ "907d06.e23",		0x040000, 0xf5cc38f4, BRF_GRA },	       //  3	Tiles
	{ "907d05.e22",		0x040000, 0xe25774c1, BRF_GRA },	       //  4

	{ "907d07.k2",		0x100000, 0xb0fe4543, BRF_GRA },	       //  5	Sprites
	{ "907d08.k7",		0x100000, 0xd5ac8d9d, BRF_GRA },	       //  6

	{ "907d04.d3",		0x080000, 0x090feb5e, BRF_SND },	       //  7	K053260 Samples
};

STD_ROM_PICK(punkshot2)
STD_ROM_FN(punkshot2)

static struct BurnRomInfo punkshot2eRomDesc[] = {
	{ "907_2.i7",		0x020000, 0xaa55516c, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "907_3.i10",		0x020000, 0x0d3aa3d5, BRF_ESS | BRF_PRG }, //  1

	{ "907f01.e8",		0x008000, 0xf040c484, BRF_ESS | BRF_PRG }, //  2	Z80 Program

	{ "907d06.e23",		0x040000, 0xf5cc38f4, BRF_GRA },	       //  3	Tiles
	{ "907d05.e22",		0x040000, 0xe25774c1, BRF_GRA },	       //  4

	{ "907d07.k2",		0x100000, 0xb0fe4543, BRF_GRA },	       //  5	Sprites
	{ "907d08.k7",		0x100000, 0xd5ac8d9d, BRF_GRA },	       //  6

	{ "907d04.d3",		0x080000, 0x090feb5e, BRF_SND },	       //  7	K053260 Samples
};

STD_ROM_PICK(punkshot2e)
STD_ROM_FN(punkshot2e)

static struct BurnRomInfo punkshotjRomDesc[] = {
	{ "907z02.i7",		0x020000, 0x7a3a5c89, BRF_ESS | BRF_PRG }, //  0	68000 Program Code
	{ "907z03.i10",		0x020000, 0x22a3d9d6, BRF_ESS | BRF_PRG }, //  1

	{ "907f01.e8",		0x008000, 0xf040c484, BRF_ESS | BRF_PRG }, //  2	Z80 Program

	{ "907d06.e23",		0x040000, 0xf5cc38f4, BRF_GRA },	       //  3	Tiles
	{ "907d05.e22",		0x040000, 0xe25774c1, BRF_GRA },	       //  4

	{ "907d07.k2",		0x100000, 0xb0fe4543, BRF_GRA },	       //  5	Sprites
	{ "907d08.k7",		0x100000, 0xd5ac8d9d, BRF_GRA },	       //  6

	{ "907d04.d3",		0x080000, 0x090feb5e, BRF_SND },	       //  7	K053260 Samples
};

STD_ROM_PICK(punkshotj)
STD_ROM_FN(punkshotj)

static INT32 TmntMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x060000;
	DrvZ80Rom              = Next; Next += 0x008000;
	DrvSoundRom            = Next; Next += 0x020000;
	DrvUPD7759CRom         = Next; Next += 0x020000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x200000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;

	RamEnd                 = Next;

	konami_palette32	   = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);
	DrvTitleSample         = (INT16*)Next; Next += 0x40000 * sizeof(INT16);
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x004000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 MiaMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x040000;
	DrvZ80Rom              = Next; Next += 0x008000;
	DrvSoundRom            = Next; Next += 0x020000;
	DrvTileRom             = Next; Next += 0x040000;
	DrvSpriteRom           = Next; Next += 0x100000;
		
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x008000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;

	RamEnd                 = Next;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x002000 * 8 * 8;
	DrvSprites             = Next; Next += 0x002000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 CuebrickMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x020000;
	DrvTileRom             = Next; Next += 0x040000;
	DrvSpriteRom           = Next; Next += 0x040000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x008000;
	DrvPaletteRam          = Next; Next += 0x001000;
	
	RamEnd                 = Next;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x002000 * 8 * 8;
	DrvSprites             = Next; Next += 0x001000 * 16 * 16;
	DrvNvRam               = Next; Next += 0x400 * 0x20;

	MemEnd                 = Next;

	return 0;
}

static INT32 BlswhstlMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x080000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x100000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x100000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;
	DrvSpriteRam           = Next; Next += 0x004000;

	RamEnd                 = Next;
	
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x002000 * 16 * 16;
	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 SsridersMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x0c0000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x100000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x200000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004080;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;
	DrvSpriteRam           = Next; Next += 0x004000;

	RamEnd                 = Next;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x004000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 Tmnt2MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x100000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x200000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x400000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x005080;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;
	DrvSpriteRam           = Next; Next += 0x004000;

	RamEnd                 = Next;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x008000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 Thndrx2MemIndex()
{
	UINT8 *Next; Next = Mem;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;

	RamEnd                 = Next;

	Drv68KRom              = Next; Next += 0x080000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x080000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x100000;
	
	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x002000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 LgtnfghtMemIndex()
{
	UINT8 *Next; Next = Mem;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;
	DrvSpriteRam           = Next; Next += 0x004000;

	RamEnd                 = Next;

	Drv68KRom              = Next; Next += 0x080000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x080000;
	DrvTileRom             = Next; Next += 0x100000;
	DrvSpriteRom           = Next; Next += 0x100000;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x008000 * 8 * 8;
	DrvSprites             = Next; Next += 0x002000 * 16 * 16;

	MemEnd                 = Next;

	return 0;
}

static INT32 PunkshotMemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x040000;
	DrvZ80Rom              = Next; Next += 0x010000;
	DrvSoundRom            = Next; Next += 0x080000;
	DrvTileRom             = Next; Next += 0x080000;
	DrvSpriteRom           = Next; Next += 0x200000;
	
	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x004000;
	DrvZ80Ram              = Next; Next += 0x000800;
	DrvPaletteRam          = Next; Next += 0x001000;

	RamEnd                 = Next;

	konami_palette32       = (UINT32*)Next;
	DrvPalette             = (UINT32*)Next; Next += 0x00810 * sizeof(UINT32);
	DrvTiles               = Next; Next += 0x004000 * 8 * 8;
	DrvSprites             = Next; Next += 0x004000 * 16 * 16;

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
	
	if (uses_k007232) K007232Reset(0);
	BurnYM2151Reset();
	
	KonamiICReset();
	
	bIrqEnable = 0;
	DrvSoundLatch = 0;
	TitleSoundLatch = 0;
	PlayTitleSample = 0;
	TitleSamplePos = 0;
	PriorityFlag = 0;

	HiscoreReset();

	return 0;
}

static INT32 TmntDoReset()
{
	INT32 nRet = DrvDoReset();
	
	if (uses_k007232) K007232Reset(0);

	ZetOpen(0);
	UPD7759Reset();
	UPD7759StartWrite(0, 0); // First sample is cut w/o this hack. ("Fire!" after coin-up/select character)
	UPD7759ResetWrite(0, 1);
	ZetClose();

	return nRet;
}

static INT32 CuebrickDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	BurnYM2151Reset();	
	
	KonamiICReset();
	
	bIrqEnable = 0;
	DrvNvRamBank = 0;
	PriorityFlag = 0;
	CuebrickSndIrqFire = 0;

	HiscoreReset();

	return 0;
}

static INT32 BlswhstlDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnYM2151Reset();
	K053260Reset(0);
	
	KonamiICReset();
	
	EEPROMReset();
	
	if (EEPROMAvailable()) {
		InitEEPROMCount = 0;
	} else {
		InitEEPROMCount = 10;
	}
	
	BlswhstlTileRomBank = 0;
	
	K052109_irq_enabled = 0;

	HiscoreReset();

	return 0;
}

static INT32 SsridersDoReset()
{
	static const UINT8 thndrx2_eeprom_data[128] = { // Enable stereo sound (Dink)
		0x00, 0x00, 0x40, 0xC0, 0x80, 0x20, 0x00, 0x0E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	static const UINT8 thndrx2j_eeprom_data[128] = { // Enable stereo sound (Dink)
		0x00, 0x00, 0x40, 0xC0, 0x80, 0x20, 0x00, 0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	static const UINT8 thndrx2a_eeprom_data[128] = { // Enable stereo sound (Dink)
		0x00, 0x00, 0x40, 0xC0, 0x80, 0x20, 0x00, 0x2E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	SekOpen(0);
	SekReset();
	SekClose();
	
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnYM2151Reset();
	K053260Reset(0);
	
	KonamiICReset();
	
	EEPROMReset();

	if (EEPROMAvailable()) {
		InitEEPROMCount = 0;
	} else {
		if (strncmp(BurnDrvGetTextA(DRV_NAME), "thndrx2j", 8) == 0) {
			EEPROMFill(thndrx2j_eeprom_data, 0, 128);
		} else
		if (strncmp(BurnDrvGetTextA(DRV_NAME), "thndrx2a", 8) == 0) {
			EEPROMFill(thndrx2a_eeprom_data, 0, 128);
		} else
			if (strncmp(BurnDrvGetTextA(DRV_NAME), "thndrx2", 7) == 0) {
				EEPROMFill(thndrx2_eeprom_data, 0, 128);
			} else {
				InitEEPROMCount = 10;
			}
	}

	K052109_irq_enabled = 0;
	DrvVBlank = 0;

	dim_c = 0;
	dim_v = 0;

	HiscoreReset();

	return 0;
}

static INT32 PunkshotDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();
	
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	BurnYM2151Reset();
	K053260Reset(0);
	
	KonamiICReset();
	
	K052109_irq_enabled = 0;
	DrvVBlank = 0;

	HiscoreReset();

	return 0;
}

static UINT8 __fastcall Tmnt68KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x100000)
	K051937ByteRead(0x140000)
	K051960ByteRead(0x140400)
	
	switch (a) {
		case 0x0a0001: {
			return 0xff - DrvInput[0];
		}
		
		case 0x0a0003: {
			return 0xff - DrvInput[1];
		}
		
		case 0x0a0005: {
			return 0xff - DrvInput[2];
		}
		
		case 0x0a0007: {
			return 0xff - DrvInput[3];
		}
		
		case 0x0a0011: {
			return DrvDip[0];
		}
		
		case 0x0a0013: {
			return DrvDip[1];
		}
		
		case 0x0a0015: {
			return 0xff - DrvInput[4];
		}
		
		case 0x0a0019: {
			return DrvDip[2];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Tmnt68KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x100000)
	K015937ByteWrite(0x140000)
	K051960ByteWrite(0x140400)

	switch (a) {
		case 0x0a0001: {
			if (LastCntrl == 0x08 && (d & 0x08) == 0) {
				ZetOpen(0);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
				ZetClose();
			}
			LastCntrl = d & 0x08;
			
			bIrqEnable = (d & 0x20) ? 1 : 0;
			
			K052109RMRDLine = d & 0x80;
			return;
		}
		
		case 0x0a0009: {
			DrvSoundLatch = d;
			return;
		}
		
		case 0x0a0011: {
			// watchdog write
			return;
		}
		
		case 0x0c0001: {
			PriorityFlag = (d & 0x0c) >> 2;
			return;
		}
		
		case 0x10e801: {
			// nop???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Tmnt68KReadWord(UINT32 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Tmnt68KWriteWord(UINT32 a, UINT16 d)
{
	K051960WordWrite(0x140400)
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static UINT8 __fastcall Mia68KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x100000)
	K051937ByteRead(0x140000)
	K051960ByteRead(0x140400)
	
	switch (a) {
		case 0x0a0001: {
			return 0xff - DrvInput[0];
		}
		
		case 0x0a0003: {
			return 0xff - DrvInput[1];
		}
		
		case 0x0a0005: {
			return 0xff - DrvInput[2];
		}
		
		case 0x0a0011: {
			return DrvDip[0];
		}
		
		case 0x0a0013: {
			return DrvDip[1];
		}
		
		case 0x0a0019: {
			return DrvDip[2];
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Mia68KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x100000)
	K015937ByteWrite(0x140000)
	K051960ByteWrite(0x140400)
	
	switch (a) {
		case 0x0a0001: {
			if (LastCntrl == 0x08 && (d & 0x08) == 0) {
				ZetOpen(0);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
				ZetClose();
			}
			LastCntrl = d & 0x08;
			
			bIrqEnable = (d & 0x20) ? 1 : 0;
			
			K052109RMRDLine = d & 0x80;
			return;
		}
		
		case 0x0a0009: {
			DrvSoundLatch = d;
			return;
		}
		
		case 0x0a0011: {
			// watchdog write
			return;
		}
		
		case 0x10e801: {
			// nop???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Mia68KReadWord(UINT32 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Mia68KWriteWord(UINT32 a, UINT16 d)
{
	K051960WordWrite(0x140400)
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static UINT8 __fastcall Cuebrick68KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x100000)
	K051937ByteRead(0x140000)
	K051960ByteRead(0x140400)
	
	if (a >= 0x0b0000 && a <= 0x0b03ff) {
		return DrvNvRam[(a - 0x0b0000) + (DrvNvRamBank * 0x400)];
	}
	
	switch (a) {
		case 0x0a0001: {
			return 0xff - DrvInput[0];
		}
		
		case 0x0a0003: {
			return 0xff - DrvInput[1];
		}
		
		case 0x0a0005: {
			return 0xff - DrvInput[2];
		}
		
		case 0x0a0011: {
			return DrvDip[1];
		}
		
		case 0x0a0013: {
			return DrvDip[0];
		}
		
		case 0x0a0019: {
			return DrvDip[2];
		}
		
		case 0x0c0000:
			return 0xff;
			
		case 0x0c0002: {
			return BurnYM2151Read();
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Cuebrick68KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x100000)
	K015937ByteWrite(0x140000)
	K051960ByteWrite(0x140400)
	
	if (a >= 0x0b0000 && a <= 0x0b03ff) {
		DrvNvRam[(a - 0x0b0000) + (DrvNvRamBank * 0x400)] = d;
		return;
	}
	
	switch (a) {
		case 0x0a0001: {
			bIrqEnable = (d & 0x20) ? 1 : 0;
			
			K052109RMRDLine = d & 0x80;
			return;
		}
		
		case 0x0a0011: {
			// watchdog write
			return;
		}
		
		case 0x0b0400: {
			DrvNvRamBank = d;
			return;
		}
		
		case 0x0c0000: {
			BurnYM2151SelectRegister(d);
			return;
		}
		
		case 0x0c0002: {
			BurnYM2151WriteRegister(d);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Cuebrick68KReadWord(UINT32 a)
{
	if (a >= 0x0b0000 && a <= 0x0b03ff) {
		SEK_DEF_READ_WORD(0, a);
		return 0;
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Cuebrick68KWriteWord(UINT32 a, UINT16 d)
{
	K051960WordWrite(0x140400)
	
	if (a >= 0x0b0000 && a <= 0x0b03ff) {
		SEK_DEF_WRITE_WORD(0, a, d);
		return;
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static UINT8 __fastcall Blswhstl68KReadByte(UINT32 a)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		INT32 Offset = (a - 0x180000) >> 1;
		
		if (a & 1) {
			return K052109Read(Offset + 0x2000);
		} else {
			return K052109Read(Offset + 0x0000);
		}
	}
	
	if (a >= 0x300000 && a <= 0x303fff) {
		INT32 Offset = (a - 0x300000);
		
		if (Offset & (0x31 << 1)) {
			return DrvSpriteRam[Offset ^ 1];
		} else {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);
			return K053245ReadWord(0, Offset/2) >> ((~a & 1) << 3);
		}
	}
	
	if (a >= 0x500000 && a <= 0x50003f) {
		INT32 Offset = (a - 0x500000) >> 1;
		if (a & 1) {
			return K054000Read(Offset);
		} else {
			return 0;
		}
	}
	
	if (a >= 0x680000 && a <= 0x68001f) {
		INT32 Offset = (a - 0x680000) >> 1;
		Offset &= ~1;
		
		if (a & 1) {
			return K053244Read(0, Offset + 1);
		} else {
			return K053244Read(0, Offset + 0);
		}
	}
	
	switch (a) {
		case 0x700001: {
			return 0xff - DrvInput[1];
		}
		
		case 0x700003: {
			return 0xff - DrvInput[2];
		}
		
		case 0x700005: {
			UINT8 Res = 0xff - DrvInput[0];
			
			if (InitEEPROMCount) {
				InitEEPROMCount--;
				Res &= 0xf7;
			}
			
			Toggle ^= 0x40;
			
			return Res ^ Toggle;
		}
		
		case 0x700007: {
			return 0xfe | (EEPROMRead() & 0x01);
		}
		
		case 0x780601:
		case 0x780603: {
			INT32 Offset = (a - 0x780601) >> 1;
			return K053260Read(0, Offset + 2);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Blswhstl68KWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		INT32 Offset = (a - 0x180000) >> 1;
		
		if (a & 1) {
			K052109Write(Offset + 0x2000, d);
		} else {
			K052109Write(Offset + 0x0000, d);
		}
		return;
	}

	if (a >= 0x300000 && a <= 0x303fff) {
		INT32 Offset = (a - 0x300000);
		DrvSpriteRam[Offset ^ 1] = d;

		if (!(Offset & (0x31 << 1))) {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);

			INT32 data = K053245ReadWord(0, Offset/2);

			if (a & 1) {
				data &= 0xff00;
				data |= d;
			} else {
				data &= 0x00ff;
				data |= d << 8;
			}

			K053245WriteWord(0, Offset / 2, data);
		}
		return;
	}

	if (a >= 0x500000 && a <= 0x50003f) {
		INT32 Offset = (a - 0x500000) >> 1;
		if (a & 1) K054000Write(Offset, d);
		return;
	}
	
	if (a >= 0x680000 && a <= 0x68001f) {
		INT32 Offset = (a - 0x680000) >> 1;
		Offset &= ~1;
		
		if (a & 1) {
			K053244Write(0, Offset + 1, d);
		} else {
			K053244Write(0, Offset + 0, d);
		}
		return;
	}
	
	if (a >= 0x780700 && a <= 0x78071f) {
		K053251Write((a - 0x780700) >> 1, d);
		return;
	}
	
	switch (a) {
		case 0x700201: {
			EEPROMWrite(d & 0x04, d & 0x02, d & 0x01);
			return;
		}
		
		case 0x700301: {
			K052109RMRDLine = d & 0x08;
			BlswhstlTileRomBank = (d & 0x80) >> 7;
			return;
		}
		
		case 0x700401: {
			// watchdog write
			return;
		}
		
		case 0x780601: {
			K053260Write(0, 0, d);
			return;
		}
		
		case 0x780605: {
			ZetOpen(0);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Blswhstl68KReadWord(UINT32 a)
{
	if (a >= 0x300000 && a <= 0x303fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0x300000) >> 1;
		
		if (Offset & 0x31) {
			return BURN_ENDIAN_SWAP_INT16(SpriteRam[Offset]);
		} else {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			return K053245ReadWord(0, Offset);
		}
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Blswhstl68KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		INT32 Offset = (a - 0x180000) >> 1;

		K052109Write(Offset + 0x2000, d >> 8);
		K052109Write(Offset + 0x0000, d & 0xff);
		return;
	}
	
	if (a >= 0x300000 && a <= 0x303fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0x300000) >> 1;
		SpriteRam[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		
		if (!(Offset & 0x0031)) {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			K053245WriteWord(0, Offset, d);
		}
		return;
	}
	
	if (a >= 0x500000 && a <= 0x50003f) {
		INT32 Offset = (a - 0x500000) >> 1;
		K054000Write(Offset, d & 0xff);
		return;
	}
	
	if (a >= 0x680000 && a <= 0x68001f) {
		INT32 Offset = (a - 0x680000) >> 1;
		Offset &= ~1;
		
		K053244Write(0, Offset + 1, d & 0xff);
		K053244Write(0, Offset + 0, d >> 8);
		return;
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

#ifdef _XBOX1
#define pow powf
#endif

static void tmnt2_protection_write()
{
	UINT32 src_addr, dst_addr, mod_addr, attr1, code, attr2, cbase, cmod, color;
	INT32 xoffs, yoffs, xmod, ymod, zmod, xzoom, yzoom, i;
	UINT16 *mcu;
	UINT16 src[4], mod[24];
	UINT8 keepaspect, xlock, ylock, zlock;

	mcu = (UINT16*)(Drv68KRam + 0x4800);
	if ((BURN_ENDIAN_SWAP_INT16(mcu[8]) & 0xff00) != 0x8200) return;

	src_addr = (BURN_ENDIAN_SWAP_INT16(mcu[0]) | (BURN_ENDIAN_SWAP_INT16(mcu[1])&0xff)<<16) >> 1;
	dst_addr = (BURN_ENDIAN_SWAP_INT16(mcu[2]) | (BURN_ENDIAN_SWAP_INT16(mcu[3])&0xff)<<16) >> 1;
	mod_addr = (BURN_ENDIAN_SWAP_INT16(mcu[4]) | (BURN_ENDIAN_SWAP_INT16(mcu[5])&0xff)<<16) >> 1;
	zlock    = (BURN_ENDIAN_SWAP_INT16(mcu[8]) & 0xff) == 0x0001;

	for (i=0; i< 4; i++) src[i] = SekReadWord((src_addr + i) * 2);
	for (i=0; i<24; i++) mod[i] = SekReadWord((mod_addr + i) * 2);

	code = src[0];			// code

	i= src[1];
	attr1 = i>>2 & 0x3f00;	// flip y, flip x and sprite size
	attr2 = i & 0x380;		// mirror y, mirror x, shadow
	cbase = i & 0x01f;		// base color
	cmod  = mod[0x2a/2]>>8;
	color = (cbase != 0x0f && cmod <= 0x1f && !zlock) ? cmod : cbase;

	xoffs = (INT16)src[2];	// local x
	yoffs = (INT16)src[3];	// local y

	i = mod[0];
	attr2 |= i & 0x0060;	// priority
	keepaspect = (i & 0x0014) == 0x0014;
	if (i & 0x8000) { attr1 |= 0x8000; }	// active
	if (keepaspect)	{ attr1 |= 0x4000; }	// keep aspect
	if (i & 0x4000) { attr1 ^= 0x1000; xoffs = -xoffs; }	// flip x

	xmod = (INT16)mod[6];	// global x
	ymod = (INT16)mod[7];	// global y
	zmod = (INT16)mod[8];	// global z
	xzoom = mod[0x1c/2];
	yzoom = (keepaspect) ? xzoom : mod[0x1e/2];
	ylock = xlock = (i & 0x0020 && (!xzoom || xzoom == 0x100));

	if (!xlock)
	{
		i = xzoom - 0x4f00;
		if (i > 0)
		{
			i >>= 8;
			xoffs += (INT32)(pow(i, /*1.891292*/1.841292) * xoffs / 599.250121);
		}
		else if (i < 0)
		{
			i = (i>>3) + (i>>4) + (i>>5) + (i>>6) + xzoom;
			xoffs = (i > 0) ? (xoffs * i / 0x4f00) : 0;
		}
	}
	if (!ylock)
	{
		i = yzoom - 0x4f00;
		if (i > 0)
		{
			i >>= 8;
			yoffs += (INT32)(pow(i, /*1.891292*/1.841292) * yoffs / 599.250121);
		}
		else if (i < 0)
		{
			i = (i>>3) + (i>>4) + (i>>5) + (i>>6) + yzoom;
			yoffs = (i > 0) ? (yoffs * i / 0x4f00) : 0;
		}

	}
	if (!zlock) yoffs += zmod;
	xoffs += xmod;
	yoffs += ymod;

	SekWriteWord((dst_addr +  0) * 2, attr1);
	SekWriteWord((dst_addr +  2) * 2, code);
	SekWriteWord((dst_addr +  4) * 2, (UINT32)yoffs);
	SekWriteWord((dst_addr +  6) * 2, (UINT32)xoffs);
	SekWriteWord((dst_addr + 12) * 2, attr2 | color);
}

static void __fastcall Tmnt268KProtWord(UINT32 a, UINT16 d)
{
	*((UINT16*)(Drv68KRam + 0x4800 + (a & 0x1e))) = BURN_ENDIAN_SWAP_INT16(d);
}

static void __fastcall Tmnt268KProtByte(UINT32 a, UINT8 d)
{
	a &= 0x1f;
	Drv68KRam[0x4801 ^ a] = d;

	if (a == 0x18) tmnt2_protection_write();
}

static UINT16 ssriders_protection_r()
{
    INT32 data = SekReadWord(0x105a0a);
    INT32 cmd  = SekReadWord(0x1058fc);

	switch (cmd)
	{
		case 0x100b:
			return 0x0064;

		case 0x6003:
			return data & 0x000f;

		case 0x6004:
			return data & 0x001f;

		case 0x6000:
			return data & 0x0001;

		case 0x0000:
			return data & 0x00ff;

		case 0x6007:
			return data & 0x00ff;

		case 0x8abc:
			/* collision table */
			data = -SekReadWord(0x105818);
			data = ((data / 8 - 4) & 0x1f) * 0x40;
            data += ((SekReadWord(0x105cb0) + SekReadWord(0x1040c8) - 6) / 8 + 12) & 0x3f;
			return data;

		default:
			return 0xffff;
    }
}

static void ssriders_protection_w(INT32 offset)
{
	offset >>= 1;

	if (offset == 1)
	{
		INT32 logical_pri,hardware_pri;

		/* create sprite priority attributes */
		hardware_pri = 1;
		for (logical_pri = 1;logical_pri < 0x100;logical_pri <<= 1)
		{
			INT32 i;

			for (i = 0;i < 128;i++)
			{
				if ((SekReadWord(0x180006 + 128*i) >> 8) == (UINT16)logical_pri)
				{
					K053245Write(0,((8*i) * 2)+1,hardware_pri);
					hardware_pri++;
				}
			}
		}
	}
}

static UINT8 __fastcall Ssriders68KReadByte(UINT32 a)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		INT32 Offset = (a - 0x180000);
		
		if (Offset & (0x31 << 1)) {
			return DrvSpriteRam[Offset ^ 1];
		} else {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);
			return K053245ReadWord(0, Offset / 2) >> ((~a & 1) << 3);
		}
	}

	if ((a & 0xffff80) == 0x1c0500) {
		return Drv68KRam[0x4001 ^ (a & 0x7f)];
	}

	if (a >= 0x5a0000 && a <= 0x5a001f) {
		INT32 Offset = (a - 0x5a0000) >> 1;
		Offset &= ~1;
		
		if (a & 1) {
			return K053244Read(0, Offset + 1);
		} else {
			return K053244Read(0, Offset + 0);
		}
	}
	
	if (a >= 0x600000 && a <= 0x603fff) {
		INT32 Offset = (a - 0x600000) >> 1;
		
		if (a & 1) {
			return K052109Read(Offset + 0x2000);
		} else {
			return K052109Read(Offset + 0x0000);
		}
	}
	
	switch (a) {
		case 0x1c0800:
		case 0x1c0801:
			return ssriders_protection_r() >> ((~a & 1) << 3);

		case 0x1c0001: {
			return 0xff - DrvInput[1];
		}
		
		case 0x1c0003: {
			return 0xff - DrvInput[2];
		}
		
		case 0x1c0005: {
			return 0xff - DrvInput[3];
		}
		
		case 0x1c0007: {
			return 0xff - DrvInput[4];
		}
		
		case 0x1c0101: {
			return 0xff - DrvInput[0];
		}
		
		case 0x1c0103: {
			UINT8 Res = DrvInput[5];

			if (DrvVBlank) Res |= 0x08;
			Res |= EEPROMRead() & 0x01;
			
			if (InitEEPROMCount) {
				InitEEPROMCount--;
				Res &= 0x7f;
			}
			
			Toggle ^= 0x04;
			
			return Res ^ Toggle;
		}
		
		case 0x1c0401: {
			// watchdog read
			return 0;
		}
		
		case 0x5c0601:
		case 0x5c0603: {
			INT32 Offset = (a - 0x5c0601) >> 1;
			return K053260Read(0, Offset + 2);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall Ssriders68KWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		INT32 Offset = (a - 0x180000);
		DrvSpriteRam[Offset ^ 1] = d;

		if (!(Offset & (0x31 << 1))) {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);

			INT32 data = K053245ReadWord(0, Offset/2);

			if (a & 1) {
				data &= 0xff00;
				data |= d;
			} else {
				data &= 0x00ff;
				data |= d << 8;
			}

			K053245WriteWord(0, Offset/2, data);

		}
		return;
	}

	if ((a & ~3) == 0x1c0800) {
		bprintf (0, _T("protection write byte %x %x\n"), a, d);
		return;
	}

	// extra ram for tmnt2
	if ((a & 0xffff80) == 0x1c0500) {
		Drv68KRam[0x4001 ^ (a & 0x7f)] = d;
		//*((UINT16*)(Drv68KRam + 0x4000 + (a & 0x7e))) = d;
		return;
	}

	if (a >= 0x5a0000 && a <= 0x5a001f) {
		INT32 Offset = (a - 0x5a0000) >> 1;
		Offset &= ~1;
		
		K053244Write(0, Offset + (a&1), d);
		return;
	}
	
	if (a >= 0x5c0700 && a <= 0x5c071f) {
		K053251Write((a - 0x5c0700) >> 1, d);
		return;
	}
	
	if (a >= 0x600000 && a <= 0x603fff) {
		INT32 Offset = (a - 0x600000) >> 1;
		
		if (a & 1) {
			K052109Write(Offset + 0x2000, d);
		} else {
			K052109Write(Offset + 0x0000, d);
		}
		return;
	}
	
	switch (a) {
		case 0x1c0201: {
			EEPROMWrite(d & 0x04, d & 0x02, d & 0x01);
			K053244BankSelect(0, ((d & 0x20) >> 5) << 2);
			dim_c = d & 0x18;
			return;
		}
		
		case 0x1c0301: {
			K052109RMRDLine = d & 0x08;
			dim_v = (d & 0x70) >> 4;
			return;
		}
		
		case 0x5c0601: {
			K053260Write(0, 0, d);
			return;
		}
		
		case 0x5c0605: {
			ZetOpen(0);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
			return;
		}	
		
		case 0x1c0401: {
			// watchdog write
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall Ssriders68KReadWord(UINT32 a)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0x180000) >> 1;
		
		if (Offset & 0x31) {
			return BURN_ENDIAN_SWAP_INT16(SpriteRam[Offset]);
		} else {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			return K053245ReadWord(0, Offset);
		}
	}

	if ((a & 0xffff80) == 0x1c0500) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(Drv68KRam + 0x4000 + (a & 0x7e))));
	}

	if (a == 0x1c0800) return ssriders_protection_r();

	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("68K Read word => %06X\n"), a);
		}
	}
		
	return 0;
}

static void __fastcall Ssriders68KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x180000 && a <= 0x183fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0x180000) >> 1;
		SpriteRam[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		
		if (!(Offset & 0x0031)) {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			K053245WriteWord(0, Offset, d);
		}
		return;
	}
	
	if (a >= 0x5a0000 && a <= 0x5a001f) {
		INT32 Offset = (a - 0x5a0000) >> 1;
		Offset &= ~1;
		
		K053244Write(0, Offset + 1, d & 0xff);
		K053244Write(0, Offset + 0, d >> 8);
		return;
	}

	if ((a & 0xffff80) == 0x1c0500) {
		*((UINT16*)(Drv68KRam + 0x4000 + (a & 0x7e))) = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}

	if (a == 0x1c0802) {
		ssriders_protection_w(2);
		return;
	}

	switch (a) {
		case 0x1c0800:
		case 0x1c0802: {
			ssriders_protection_w(a - 0x1c0800);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K Write word => %06X, %04X\n"), a, d);
		}
	}
}

static UINT8 __fastcall Thndrx268KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x600000)
	K051937ByteRead(0x700000)

	if ((a & 0xffffc00) == 0x700400) {
		return K051960Read(a & 0x3ff);
	}

	if (a >= 0x500000 && a <= 0x50003f) {
		INT32 Offset = (a - 0x500000) >> 1;
		if (a & 1) {
			return K054000Read(Offset);
		} else {
			return 0;
		}
	}

	switch (a)
	{
		case 0x400001:
		case 0x400003:
			return K053260Read(0, ((a & 2) >> 1) + 2);

		case 0x500200:
		{
			INT32 ret = DrvInput[1]^0xff;
			if (InitEEPROMCount) {
				ret &= 0xf7;
				InitEEPROMCount--;
			}
			return ret;
		}

		case 0x500201:
			return DrvInput[0]^0xff;

		case 0x500202:
		{
			Toggle ^= 0x08;
			INT32 ret = 0xfe;
			ret |= (EEPROMRead() & 1);
			ret ^= Toggle;

			return ret;
		}

		case 0x500203:
			return DrvInput[2]^0xff;
	}
		

	return 0;
}

static void __fastcall Thndrx268KWriteWord(UINT32 a, UINT16 d)
{
	if ((a & 0xffffc00) == 0x700400) {
		K051960Write((a & 0x3fe) | 0, d >> 8);
		K051960Write((a & 0x3fe) | 1, d & 0xff);
		return;
	}
}

static void __fastcall Thndrx268KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x600000)
	K015937ByteWrite(0x700000)

	if ((a & 0xffffc00) == 0x700400) {
		K051960Write(a & 0x3ff, d);
		return;
	}
	
	if (a >= 0x300000 && a <= 0x30001f) {
		if (a & 1) K053251Write((a - 0x300000) >> 1, d);
		return;
	}

	if (a >= 0x500000 && a <= 0x50003f) {
		INT32 Offset = (a - 0x500000) >> 1;
		if (a & 1) K054000Write(Offset, d);
		return;
	}

	switch (a)
	{
		case 0x400001:
			K053260Write(0, 0, d);
		return;

		case 0x500101:
		{
			EEPROMWriteBit(d & 0x01);
			EEPROMSetCSLine((d & 0x02) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((d & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);

		//	EEPROMWrite(d & 0x04, d & 0x02, d & 0x01);

			if (LastCntrl == 0 && (d & 0x20) != 0) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
			LastCntrl = d & 0x20;

			K052109RMRDLine = d & 0x40;
		}
		return;
	}
}

static UINT16 __fastcall Lgtnfght68KReadWord(UINT32 a)
{
	if (a >= 0xb0000 && a <= 0xb3fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0xb0000) >> 1;
		
		if (Offset & 0x31) {
			return BURN_ENDIAN_SWAP_INT16(SpriteRam[Offset]);
		} else {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			return K053245ReadWord(0, Offset);
		}
	}

	return 0;
}

static UINT8 __fastcall Lgtnfght68KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x100000)

	if (a >= 0xb0000 && a <= 0xb3fff) {
		INT32 Offset = (a - 0xb0000);
		
		if (Offset & (0x31 << 1)) {
			return DrvSpriteRam[Offset ^ 1];
		} else {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);
			return K053245ReadWord(0, Offset / 2) >> ((~a & 1) << 3);
		}
	}
		
	if (a >= 0xc0000 && a <= 0xc001f) {
		INT32 Offset = (a - 0xc0000) >> 1;
		Offset &= ~1;
		
		if (a & 1) {
			return K053244Read(0, Offset + 1);
		} else {
			return K053244Read(0, Offset + 0);
		}
	}

	switch (a)
	{
		case 0x0a0001:
			return ~DrvInput[0] & 0xfb;

		case 0x0a0003:
			return ~DrvInput[1];

		case 0x0a0005:
			return ~DrvInput[2];

		case 0x0a0007:
			return DrvDip[0];

		case 0x0a0009:
			return DrvDip[1];

		case 0x0a0011:
			return DrvDip[2];

		case 0x0a0021:
		case 0x0a0023:
			return K053260Read(0, ((a & 2) >> 1) + 2); // k053260
	}

	return 0;
}

static void __fastcall Lgtnfght68KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0xb0000 && a <= 0xb3fff) {
		UINT16 *SpriteRam = (UINT16*)DrvSpriteRam;
		INT32 Offset = (a - 0xb0000) >> 1;
		SpriteRam[Offset] = BURN_ENDIAN_SWAP_INT16(d);
		
		if (!(Offset & 0x0031)) {
			Offset = ((Offset & 0x000e) >> 1) | ((Offset & 0x1fc0) >> 3);
			K053245WriteWord(0, Offset, d);
		}
		return;
	}
}

static void __fastcall Lgtnfght68KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x100000)

	if (a >= 0x0e0000 && a <= 0x0e001f) {
		K053251Write((a - 0x0e0000) >> 1, d);
		return;
	}

	if (a >= 0xb0000 && a <= 0xb3fff) {
		INT32 Offset = (a - 0xb0000);
		DrvSpriteRam[Offset ^ 1] = d;

		if (!(Offset & (0x31 << 1))) {
			Offset = ((Offset & (0x000e << 1)) >> 1) | ((Offset & (0x1fc0 << 1)) >> 3);

			INT32 data = K053245ReadWord(0, Offset/2);

			if (a & 1) {
				data &= 0xff00;
				data |= d;
			} else {
				data &= 0x00ff;
				data |= d << 8;
			}

			K053245WriteWord(0, Offset/2, data);

		}
		return;
	}

	if (a >= 0xc0000 && a <= 0xc001f) {
		INT32 Offset = (a - 0xc0000) >> 1;
		Offset &= ~1;
		
		K053244Write(0, Offset + (a&1), d);
		return;
	}

	switch (a)
	{
		case 0xa0019:
		{
			if (LastCntrl == 0 && (d & 0x4) != 0) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
			LastCntrl = d & 0x4;

			K052109RMRDLine = d & 0x8;
		}
		return;

		case 0xa0021:
			K053260Write(0, 0, d);
		return;

		case 0xa0029: // watchdog
		return;
	}
}

static UINT16 __fastcall Punkshot68KReadWord(UINT32 a)
{
	if ((a & 0xfffc00) == 0x110400) {
		return K051960Read((a & 0x3fe) | 1) | (K051960Read((a & 0x3fe) | 0) << 8);
	}

	return 0;
}

static UINT8 __fastcall Punkshot68KReadByte(UINT32 a)
{
	K052109WordNoA12Read(0x100000)
	K051937ByteRead(0x110000)
	K051960ByteRead(0x110400)

	switch (a)
	{
		case 0x0a0000:
			return DrvDip[1];

		case 0x0a0001:
			return DrvDip[0];

		case 0x0a0003:
			return ~DrvInput[0];

		case 0x0a0002:
			return DrvDip[2] | (~DrvInput[5] & 0x0f);

		case 0x0a0005:
			return ~DrvInput[3];

		case 0x0a0004:
			return ~DrvInput[4];

		case 0x0a0007:
			return ~DrvInput[1];

		case 0x0a0006:
			return ~DrvInput[2];

		case 0x0a0041:
		case 0x0a0043:
			return K053260Read(0, ((a & 2) >> 1) | 2);

		case 0xfffffc:
		case 0xfffffd:
		case 0xfffffe:
		case 0xffffff:
			return BurnRandom();
	}

	return 0;
}

static void __fastcall Punkshot68KWriteWord(UINT32 a, UINT16 d)
{
	if ((a & 0xfffc00) == 0x110400) {
		K051960Write((a & 0x3fe) | 0, d >> 8);
		K051960Write((a & 0x3fe) | 1, d & 0xff);

		return;
	}

	if ((a & 0xff8000) == 0x100000) {
		SekWriteByte(a+1, d & 0xff);
		return;
	}
}

static void __fastcall Punkshot68KWriteByte(UINT32 a, UINT8 d)
{
	K052109WordNoA12Write(0x100000)
	K015937ByteWrite(0x110000)
	K051960ByteWrite(0x110400)

	if ((a & 0xffffe1) == 0xa0061) {
		K053251Write((a & 0x1f) >> 1, d);
		return;
	}

	switch (a)
	{
		case 0x0a0020:
		case 0x0a0021:
		{
			K052109RMRDLine = d & 0x08;

			if (LastCntrl == 0x04 && (d & 0x04) == 0x00) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}

			LastCntrl = d & 0x04;
		}
		return;

		case 0x0a0041:
			K053260Write(0, 0, d);
		return;

		case 0x0a0081:
			// watchdog
		return;
	}
}

static UINT8 __fastcall TmntZ80Read(UINT16 a)
{
	if (a >= 0xb000 && a <= 0xb00d) {
		return K007232ReadReg(0, a - 0xb000);
	}
	
	switch (a) {
		case 0x9000: {
			return TitleSoundLatch;
		}
		
		case 0xa000: {
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return DrvSoundLatch;
		}
		
		case 0xc001: {
			return BurnYM2151Read();
		}
		
		case 0xf000: {
			return UPD7759BusyRead(0);
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall TmntZ80Write(UINT16 a, UINT8 d)
{
	if (a >= 0xb000 && a <= 0xb00d) {
		K007232WriteReg(0, (a - 0xb000), d);
		return;
	}
	
	switch (a) {
		case 0x9000: {
			TitleSoundLatch = d;
			if (d & 0x04) {
				PlayTitleSample = 1;
			} else {
				PlayTitleSample = 0;
				TitleSamplePos = 0;
			}
			
			UPD7759ResetWrite(0, d & 2);
			return;
		}
		
		case 0xc000: {
			BurnYM2151SelectRegister(d);
			return;
		}
		
		case 0xc001: {
			BurnYM2151WriteRegister(d);
			return;
		}
		
		case 0xd000: {
			UPD7759PortWrite(0, d);
			return;
		}
		
		case 0xe000: {
			UPD7759StartWrite(0, d);
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall MiaZ80Read(UINT16 a)
{
	if (a >= 0xb000 && a <= 0xb00d) {
		return K007232ReadReg(0, a - 0xb000);
	}
	
	switch (a) {
		case 0xa000: {
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return DrvSoundLatch;
		}
		
		case 0xc001: {
			return BurnYM2151Read();
		}

		case 0xf000: return 0; // NOP
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall MiaZ80Write(UINT16 a, UINT8 d)
{
	if (a >= 0xb000 && a <= 0xb00d) {
		K007232WriteReg(0, (a - 0xb000), d);
		return;
	}
	
	switch (a) {
		case 0xc000: {
			BurnYM2151SelectRegister(d);
			return;
		}
		
		case 0xc001: {
			BurnYM2151WriteRegister(d);
			return;
		}

		case 0xf000: return; // NOP
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall BlswhstlZ80Read(UINT16 a)
{
	if (a >= 0xfa00 && a <= 0xfa2f) {
		if (a == 0xfa00) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return K053260Read(0, a - 0xfa00);
	}
	
	switch (a) {
		case 0xf801: {
			return BurnYM2151Read();
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall BlswhstlZ80Write(UINT16 a, UINT8 d)
{
	if (a >= 0xfa00 && a <= 0xfa2f) {
		K053260Write(0, (a - 0xfa00), d);
		return;
	}
	
	switch (a) {
		case 0xf800: {
			BurnYM2151SelectRegister(d);
			return;
		}
		
		case 0xf801: {
			BurnYM2151WriteRegister(d);
			return;
		}
		
		case 0xfc00: { // tmnt2 needs this
			ZetRun(100);
			ZetNmi();
			return;
		}

		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static void __fastcall Thndrx2Z80Write(UINT16 a, UINT8 d)
{
	if (a >= 0xfc00 && a <= 0xfc2f) {
		K053260Write(0, (a - 0xfc00), d);
		return;
	}

	switch (a)
	{
		case 0xf800:
		case 0xf810:
			BurnYM2151SelectRegister(d);
		return;

		case 0xf801:
		case 0xf811:
			BurnYM2151WriteRegister(d);
		return;

		case 0xfa00:
			ZetRun(100);
			ZetNmi();
		return;
	}
}

static UINT8 __fastcall Thndrx2Z80Read(UINT16 a)
{
	if (a >= 0xfc00 && a <= 0xfc2f) {
		if (a == 0xfc00) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return K053260Read(0, a - 0xfc00);
	}

	switch (a)
	{
		case 0xf801:
		case 0xf811:
			return BurnYM2151Read();
	}

	return 0;
}

static void __fastcall LgtnfghtZ80Write(UINT16 a, UINT8 d)
{
	if (a >= 0xc000 && a <= 0xc02f) {
		K053260Write(0, (a - 0xc000), d);
		return;
	}

	switch (a)
	{
		case 0xa000:
			BurnYM2151SelectRegister(d);
		return;

		case 0xa001:
			BurnYM2151WriteRegister(d);
		return;
	}
}

static UINT8 __fastcall LgtnfghtZ80Read(UINT16 a)
{
	if (a >= 0xc000 && a <= 0xc02f) {
		if (a == 0xc000) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return K053260Read(0, a - 0xc000);
	}

	switch (a)
	{
		case 0xa001:
			return BurnYM2151Read();
	}

	return 0;
}

static void shuffle(UINT16 *buf, INT32 len)
{
	INT32 i;
	UINT16 t;

	if (len == 2) return;

	if (len % 4) return;

	len /= 2;

	for (i = 0;i < len/2;i++) {
		t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	shuffle(buf,len);
	shuffle(buf + len,len);
}

static void byte_shuffle(UINT8 *buf, INT32 len)
{
	INT32 i;
	UINT8 t;

	if (len == 2) return;

	if (len % 4) return;

	len /= 2;

	for (i = 0;i < len/2;i++) {
		t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	byte_shuffle(buf,len);
	byte_shuffle(buf + len,len);
}

static void TmntUnscrambleGfx(UINT8 *pSrc, INT32 nLength)
{
	INT32 bits[32];
	for (INT32 i = 0; i < nLength; i += 4) {
		for (INT32 j = 0; j < 4; j++) {
			for (INT32 k = 0; k < 8; k++) {
				bits[8*j + k] = (pSrc[i + j] >> k) & 1;
			}
		}

		for (INT32 j = 0; j < 4; j++) {
			pSrc[i + j] = 0;
			for (INT32 k = 0; k < 8; k++) {
				pSrc[i + j] |= bits[j + 4*k] << k;
			}
		}
	}
}

static void TmntUnscrambleSprites()
{
	BurnLoadRom(DrvTempRom + 0x200000, 11, 1);
	UINT8 *SpriteConvTable = DrvTempRom + 0x200000;
	
	memcpy(DrvTempRom, DrvSpriteRom, 0x200000);
	
	for (INT32 A = 0; A < 0x80000; A++) {
		INT32 B, i, entry;
		INT32 bits[10];

		static const UINT8 bit_pick_table[10][8] = {
			{ 3,   3,   3,   3,   3,   3,   3,   3 },
			{ 0,   0,   5,   5,   5,   5,   5,   5 },
			{ 1,   1,   0,   0,   0,   7,   7,   7 },
			{ 2,   2,   1,   1,   1,   0,   0,   9 },
			{ 4,   4,   2,   2,   2,   1,   1,   0 },
			{ 5,   6,   4,   4,   4,   2,   2,   1 },
			{ 6,   5,   6,   6,   6,   4,   4,   2 },
			{ 7,   7,   7,   7,   8,   6,   6,   4 },
			{ 8,   8,   8,   8,   7,   8,   8,   6 },
			{ 9,   9,   9,   9,   9,   9,   9,   8 }
		};

		entry = SpriteConvTable[(A & 0x7f800) >> 11] & 7;

		for (i = 0;i < 10;i++)
			bits[i] = (A >> i) & 0x01;

		B = A & 0x7fc00;

		for (i = 0; i < 10;i++) B |= bits[bit_pick_table[i][entry]] << i;

		DrvSpriteRom[4*A+0] = DrvTempRom[4*B+0];
		DrvSpriteRom[4*A+1] = DrvTempRom[4*B+1];
		DrvSpriteRom[4*A+2] = DrvTempRom[4*B+2];
		DrvSpriteRom[4*A+3] = DrvTempRom[4*B+3];
	}
}

static void MiaUnscrambleSprites()
{
	memcpy(DrvTempRom, DrvSpriteRom, 0x100000);
	
	for (INT32 A = 0; A < 0x40000; A++) {
		INT32 B, i;
		INT32 bits[8];
		for (i = 0; i < 8; i++) bits[i] = (A >> i) & 0x01;

		B = A & 0x3ff00;

		if ((A & 0x3c000) == 0x3c000) {
			B |= bits[3] << 0;
			B |= bits[5] << 1;
			B |= bits[0] << 2;
			B |= bits[1] << 3;
			B |= bits[2] << 4;
			B |= bits[4] << 5;
			B |= bits[6] << 6;
			B |= bits[7] << 7;
		} else {
			B |= bits[3] << 0;
			B |= bits[5] << 1;
			B |= bits[7] << 2;
			B |= bits[0] << 3;
			B |= bits[1] << 4;
			B |= bits[2] << 5;
			B |= bits[4] << 6;
			B |= bits[6] << 7;
		}

		DrvSpriteRom[4*A+0] = DrvTempRom[4*B+0];
		DrvSpriteRom[4*A+1] = DrvTempRom[4*B+1];
		DrvSpriteRom[4*A+2] = DrvTempRom[4*B+2];
		DrvSpriteRom[4*A+3] = DrvTempRom[4*B+3];
	}
}

static void TmntDecodeTitleSample()
{
	for (INT32 i = 0; i < 0x40000; i++) {
		INT32 val = DrvTempRom[2 * i + 0] + (DrvTempRom[(2 * i) + 1] << 8);
		INT32 expo = val >> 13;

	  	val = (val >> 3) & (0x3ff);
		val -= 0x200;

		val <<= (expo-3);

		DrvTitleSample[i] = val;
	}
}

static void TmntTitleSampleSetRoute(double nVolume, INT32 nRouteDir)
{
	TitleSampleGain = nVolume;
	TitleSampleOutputDir = nRouteDir;
}

static INT32 TilePlaneOffsets[4]     = { 24, 16, 8, 0 };
static INT32 TileXOffsets[8]         = { 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 TileYOffsets[8]         = { 0, 32, 64, 96, 128, 160, 192, 224 };
static INT32 SpritePlaneOffsets[4]   = { 24, 16, 8, 0 };
static INT32 SpritePlaneOffsets2[4]  = { 0, 8, 16, 24 };
static INT32 SpriteXOffsets[16]      = { 0, 1, 2, 3, 4, 5, 6, 7, 256, 257, 258, 259, 260, 261, 262, 263 };
static INT32 SpriteYOffsets[16]      = { 0, 32, 64, 96, 128, 160, 192, 224, 512, 544, 576, 608, 640, 672, 704, 736 };

static void K052109TmntCallback(INT32 Layer, INT32 Bank, INT32 *Code, INT32 *Colour, INT32*, INT32 *)
{
	*Code |= ((*Colour & 0x03) << 8) | ((*Colour & 0x10) << 6) | ((*Colour & 0x0c) << 9) | (Bank << 13);
	*Colour = LayerColourBase[Layer] + ((*Colour & 0xe0) >> 5);
}

static void K052109MiaCallback(INT32 Layer, INT32 Bank, INT32 *Code, INT32 *Colour, INT32 *xFlip, INT32 *)
{
	*xFlip = *Colour & 0x04;
	
	if (Layer == 0) {
		*Code |= ((*Colour & 0x01) << 8);
		*Colour = LayerColourBase[Layer] + ((*Colour & 0x80) >> 5) + ((*Colour & 0x10) >> 1);
	} else {
		*Code |= ((*Colour & 0x01) << 8) | ((*Colour & 0x18) << 6) | (Bank << 11);
		*Colour = LayerColourBase[Layer] + ((*Colour & 0xe0) >> 5);
	}
}

static void K052109CuebrickCallback(INT32 Layer, INT32, INT32 *Code, INT32 *Colour, INT32 *, INT32 *)
{
	if (K052109RMRDLine == 0 && Layer == 0) {
		*Code |= ((*Colour & 0x01) << 8);
		*Colour = LayerColourBase[Layer] + ((*Colour & 0x0e) >> 1);

	} else {
		*Code |= ((*Colour & 0x0f) << 8);
		*Colour = LayerColourBase[Layer] + ((*Colour & 0xe0) >> 5);
	}
}

static void K052109BlswhstlCallback(INT32 Layer, INT32 Bank, INT32 *Code, INT32 *Colour, INT32 *, INT32 *)
{
	*Code |= ((*Colour & 0x01) << 8) | ((*Colour & 0x10) << 5) | ((*Colour & 0x0c) << 8) | (Bank << 12) | (BlswhstlTileRomBank << 14);
	*Colour = LayerColourBase[Layer] + ((*Colour & 0xe0) >> 5);
}

static void K051960TmntCallback(INT32 *Code, INT32 *Colour, INT32*, INT32*)
{
	*Code |= (*Colour & 0x10) << 9;
	*Colour = SpriteColourBase + (*Colour & 0x0f);
}

static void K051960MiaCallback(INT32* /*Code*/, INT32 *Colour, INT32*, INT32*)
{
	*Colour = SpriteColourBase + (*Colour & 0x0f);
}

static void K051960CuebrickCallback(INT32* Code, INT32 *Colour, INT32*, INT32*)
{
	*Colour = SpriteColourBase + (*Colour & 0x0f);
	*Code &= 0xfff;
}

static void K051960Thndrx2Callback(INT32* code, INT32 *color, INT32* priority, INT32*)
{
	INT32 pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= LayerPri[2])					*priority = 0x00;
	else if (pri > LayerPri[2] && pri <= LayerPri[1])	*priority = 0xf0;
	else if (pri > LayerPri[1] && pri <= LayerPri[0])	*priority = 0xfc;
	else 							*priority = 0xfe;

	*code &= 0x1fff;
	*color = SpriteColourBase + (*color & 0x0f);
}

static void K051960PunkshotCallback(INT32 *code, INT32 *color, INT32 *priority, INT32 *)
{
	INT32 pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= LayerPri[2])					*priority = 0x00;
	else if (pri > LayerPri[2] && pri <= LayerPri[1])	*priority = 0xf0;
	else if (pri > LayerPri[1] && pri <= LayerPri[0])	*priority = 0xfc;
	else 							*priority = 0xfe;

	*code |= (*color & 0x10) << 9;
	*code &= 0x3fff;
	*color = SpriteColourBase + (*color & 0x0f);
}

static void K053245BlswhstlCallback(INT32 *Code, INT32 *Colour, INT32 *Priority)
{
	INT32 Pri = 0x20 | ((*Colour & 0x60) >> 2);
	if (Pri <= LayerPri[2])                           	*Priority = 0x00;
	else if (Pri > LayerPri[2] && Pri <= LayerPri[1]) 	*Priority = 0xf0;
	else if (Pri > LayerPri[1] && Pri <= LayerPri[0]) 	*Priority = 0xfc;
	else 	                                          	*Priority = 0xfe;

	*Code &= 0x1fff;
	*Colour = SpriteColourBase + (*Colour & 0x1f);
}

static void K053245SsridersCallback(INT32 *, INT32 *Colour, INT32 *Priority)
{
	INT32 Pri = 0x20 | ((*Colour & 0x60) >> 2);
	if (Pri <= LayerPri[2])                           	*Priority = 0x00;
	else if (Pri > LayerPri[2] && Pri <= LayerPri[1]) 	*Priority = 0xf0;
	else if (Pri > LayerPri[1] && Pri <= LayerPri[0]) 	*Priority = 0xfc;
	else 	                                          	*Priority = 0xfe;

	*Colour = SpriteColourBase + (*Colour & 0x1f);
}

static void K053245LgtnfghtCallback(INT32 *code, INT32 *color, INT32 *priority)
{
	INT32 pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= LayerPri[2])					*priority = 0x00;
	else if (pri > LayerPri[2] && pri <= LayerPri[1])	*priority = 0xf0;
	else if (pri > LayerPri[1] && pri <= LayerPri[0])	*priority = 0xfc;
	else 							*priority = 0xfe;

	*color = SpriteColourBase + (*color & 0x1f);
	*code &= 0x3fff;
}

static void DrvK007232VolCallback(INT32 v)
{
	K007232SetVolume(0, 0, (v >> 4) * 0x11, 0);
	K007232SetVolume(0, 1, 0, (v & 0x0f) * 0x11);
}

static void CuebrickYM2151IrqHandler(INT32 Irq)
{
	CuebrickSndIrqFire = Irq;
}

#define BurnAllocMemIndexEx(Name) do {                			\
	Mem = NULL;                                 				\
	Name##MemIndex();                                 			\
	INT32 nLen = MemEnd - (UINT8 *)0;           				\
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;	\
	memset(Mem, 0, nLen);                       				\
	Name##MemIndex();                                 			\
} while (0)

static INT32 TmntInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Tmnt);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K051960Init(DrvSpriteRom, DrvSprites, 0x1fffff);
	K051960SetCallback(K051960TmntCallback);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040000, 3, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 6, 1); if (nRet != 0) return 1;
	shuffle((UINT16*)DrvTileRom, 0x080000);
	TmntUnscrambleGfx(DrvTileRom, 0x100000);
	GfxDecode(0x100000 / 32, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	DrvTempRom = (UINT8 *)BurnMalloc(0x200100);
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x080000,  8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x100000,  9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x180000, 10, 1); if (nRet != 0) return 1;
	shuffle((UINT16*)DrvSpriteRom, 0x100000);
	TmntUnscrambleGfx(DrvSpriteRom, 0x200000);
	TmntUnscrambleSprites();
	
	// Decode the sprites
	GfxDecode(0x200000 / 128, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, DrvSpriteRom, DrvSprites);	
	
	// Load K007232 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 13, 1); if (nRet != 0) return 1;	
	
	// Load UPD7759C Sample Roms
	nRet = BurnLoadRom(DrvUPD7759CRom + 0x00000, 14, 1); if (nRet != 0) return 1;
	
	// Load title sample
	memset(DrvTempRom, 0, 0x080000);
	nRet = BurnLoadRom(DrvTempRom + 0x00000, 15, 1); if (nRet != 0) return 1;
	TmntDecodeTitleSample();
	
	BurnFree(DrvTempRom);
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x080000, 0x080fff, MAP_RAM);
	SekSetReadWordHandler(0, Tmnt68KReadWord);
	SekSetWriteWordHandler(0, Tmnt68KWriteWord);
	SekSetReadByteHandler(0, Tmnt68KReadByte);
	SekSetWriteByteHandler(0, Tmnt68KWriteByte);
	SekClose();
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(TmntZ80Read);
	ZetSetWriteHandler(TmntZ80Write);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom                );
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	
	K007232Init(0, 3579545, DrvSoundRom, 0x20000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback);
	K007232PCMSetAllRoutes(0, 0.33 / 2, BURN_SND_ROUTE_BOTH);
	uses_k007232 = 1;

	UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvUPD7759CRom);
	UPD7759SetRoute(0, 0.60 / 2, BURN_SND_ROUTE_BOTH);
	UPD7759SetSyncCallback(0, ZetTotalCycles, 3579545);

	TmntTitleSampleSetRoute(1.00 / 2, BURN_SND_ROUTE_BOTH);
	
	LayerColourBase[0] = 0;
	LayerColourBase[1] = 32;
	LayerColourBase[2] = 40;
	SpriteColourBase = 16;

	// Reset the driver
	TmntDoReset();

	return 0;
}

static INT32 MiaInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Mia);
	
	K052109Init(DrvTileRom, DrvTiles, 0x03ffff);
	K052109SetCallback(K052109MiaCallback);
	K051960Init(DrvSpriteRom, DrvSprites, 0x0fffff);
	K051960SetCallback(K051960MiaCallback);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 2, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x000001, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x020000, 5, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x020001, 6, 2); if (nRet != 0) return 1;
	shuffle((UINT16*)DrvTileRom, 0x020000);
	TmntUnscrambleGfx(DrvTileRom, 0x040000);
	GfxDecode(0x2000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	DrvTempRom = (UINT8 *)BurnMalloc(0x100000);
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x080000,  8, 1); if (nRet != 0) return 1;
	shuffle((UINT16*)DrvSpriteRom, 0x080000);
	TmntUnscrambleGfx(DrvSpriteRom, 0x100000);
	MiaUnscrambleSprites();
	
	// Decode the sprites
	GfxDecode(0x2000, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, DrvSpriteRom, DrvSprites);	
	
	// Load K007232 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 10, 1); if (nRet != 0) return 1;
	
	BurnFree(DrvTempRom);
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRam + 0x0000  , 0x040000, 0x043fff, MAP_RAM);
	SekMapMemory(Drv68KRam + 0x4000  , 0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x080000, 0x080fff, MAP_RAM);
	SekSetReadWordHandler(0, Mia68KReadWord);
	SekSetWriteWordHandler(0, Mia68KWriteWord);
	SekSetReadByteHandler(0, Mia68KReadByte);
	SekSetWriteByteHandler(0, Mia68KWriteByte);
	SekClose();
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(MiaZ80Read);
	ZetSetWriteHandler(MiaZ80Write);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom                );
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);
	
	K007232Init(0, 3579545, DrvSoundRom, 0x20000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback);
	K007232PCMSetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	uses_k007232 = 1;
	
	LayerColourBase[0] = 0;
	LayerColourBase[1] = 32;
	LayerColourBase[2] = 40;
	SpriteColourBase = 16;

	// Reset the driver
	DrvDoReset();

	return 0;
}

static INT32 CuebrickInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Cuebrick);
	
	K052109Init(DrvTileRom, DrvTiles, 0x03ffff);
	K052109SetCallback(K052109CuebrickCallback);

	K051960Init(DrvSpriteRom, DrvSprites, 0x03ffff);
	K051960SetCallback(K051960CuebrickCallback);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x000001, 3, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x020000, 4, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x020001, 5, 2); if (nRet != 0) return 1;
	byte_shuffle(DrvTileRom, 0x040000);
	GfxDecode(0x2000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000, 6, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x000001, 7, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x020000, 8, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x020001, 9, 2); if (nRet != 0) return 1;
	byte_shuffle(DrvSpriteRom, 0x040000);
	GfxDecode(0x0800, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x400, DrvSpriteRom, DrvSprites);	
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(Drv68KRam + 0x0000  , 0x040000, 0x043fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x080000, 0x080fff, MAP_RAM);
	SekSetReadWordHandler(0, Cuebrick68KReadWord);
	SekSetWriteWordHandler(0, Cuebrick68KWriteWord);
	SekSetReadByteHandler(0, Cuebrick68KReadByte);
	SekSetWriteByteHandler(0, Cuebrick68KWriteByte);
	SekClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&CuebrickYM2151IrqHandler);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	LayerColourBase[0] = 0;
	LayerColourBase[1] = 32;
	LayerColourBase[2] = 40;
	SpriteColourBase = 16;

	// Reset the driver
	CuebrickDoReset();

	return 0;
}

static INT32 BlswhstlInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Blswhstl);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109BlswhstlCallback);
	K052109AdjustScroll(8, 0);

	K053245Init(0, DrvSpriteRom, DrvSprites, 0xfffff, K053245BlswhstlCallback);
	K053245SetSpriteOffset(0, -112, 16);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040000, 3, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 6, 1); if (nRet != 0) return 1;
	BurnByteswap(DrvTileRom, 0x100000);
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x080000,  8, 1); if (nRet != 0) return 1;
	BurnByteswap(DrvSpriteRom, 0x100000);
	konami_rom_deinterleave_2(DrvSpriteRom, 0x100000);
	K053245GfxDecode(DrvSpriteRom, DrvSprites, 0x100000);
	
	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 9, 1); if (nRet != 0) return 1;
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x204000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x400000, 0x400fff, MAP_RAM);
	SekSetReadWordHandler(0, Blswhstl68KReadWord);
	SekSetWriteWordHandler(0, Blswhstl68KWriteWord);
	SekSetReadByteHandler(0, Blswhstl68KReadByte);
	SekSetWriteByteHandler(0, Blswhstl68KWriteByte);
	SekClose();
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(BlswhstlZ80Read);
	ZetSetWriteHandler(BlswhstlZ80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.70, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.70, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x100000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.50, BURN_SND_ROUTE_RIGHT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.50, BURN_SND_ROUTE_LEFT);

	EEPROMInit(&BlswhstlEEPROMInterface);

	NoDim = 1;
	
	// Reset the driver
	BlswhstlDoReset();

	return 0;
}

static INT32 SsridersInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Ssriders);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(8, 0);

	K053245Init(0, DrvSpriteRom, DrvSprites, 0x1fffff, K053245LgtnfghtCallback);
	K053245SetSpriteOffset(0, -112, 16);
	K05324xSetZRejection(0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x080001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x080000, 3, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x100000,  8, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x200000);
	K053245GfxDecode(DrvSpriteRom, DrvSprites, 0x200000);
	
	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 9, 1); if (nRet != 0) return 1;
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x0bffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x140000, 0x140fff, MAP_RAM);
	SekSetReadWordHandler(0, Ssriders68KReadWord);
	SekSetWriteWordHandler(0, Ssriders68KWriteWord);
	SekSetReadByteHandler(0, Ssriders68KReadByte);
	SekSetWriteByteHandler(0, Ssriders68KWriteByte);
	SekClose();
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(BlswhstlZ80Read);
	ZetSetWriteHandler(BlswhstlZ80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x100000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.70, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.70, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&BlswhstlEEPROMInterface);
	
	// Reset the driver
	SsridersDoReset();

	return 0;
}

static INT32 Thndrx2Init()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Thndrx2);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(8, 0);

	K051960Init(DrvSpriteRom, DrvSprites, 0x0fffff);
	K051960SetCallback(K051960Thndrx2Callback);
	K051960SetSpriteOffset(8, 0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 2, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 4, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x080000,  6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x100000);
	GfxDecode(0x2000, 4, 16, 16, SpritePlaneOffsets2, SpriteXOffsets, SpriteYOffsets, 0x400, DrvSpriteRom, DrvSprites);
	
	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 7, 1); if (nRet != 0) return 1;

	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x200000, 0x200fff, MAP_RAM);
	SekSetWriteWordHandler(0, Thndrx268KWriteWord);
	SekSetReadByteHandler(0, Thndrx268KReadByte);
	SekSetWriteByteHandler(0, Thndrx268KWriteByte);
	SekClose();

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(Thndrx2Z80Read);
	ZetSetWriteHandler(Thndrx2Z80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();

	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x80000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&thndrx2_eeprom_interface);
	
	// Reset the driver
	SsridersDoReset();

	return 0;
}

static INT32 LgtnfghtInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Lgtnfght);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(8, 0);

	K053245Init(0, DrvSpriteRom, DrvSprites, 0x0fffff, K053245LgtnfghtCallback);
	K053245SetSpriteOffset(0, -112, 16);
	K05324xSetZRejection(0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 2, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 4, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,  5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x080000,  6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x100000);
	K053245GfxDecode(DrvSpriteRom, DrvSprites, 0x100000);

	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 7, 1); if (nRet != 0) return 1;

	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPaletteRam       , 0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(Drv68KRam           , 0x090000, 0x093fff, MAP_RAM);
	SekSetReadWordHandler(0, Lgtnfght68KReadWord);
	SekSetWriteWordHandler(0, Lgtnfght68KWriteWord);
	SekSetReadByteHandler(0, Lgtnfght68KReadByte);
	SekSetWriteByteHandler(0, Lgtnfght68KWriteByte);
	SekClose();

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(LgtnfghtZ80Read);
	ZetSetWriteHandler(LgtnfghtZ80Write);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Rom                );
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80Ram                );
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80Ram                );
	ZetClose();

	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x80000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.70, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.70, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&thndrx2_eeprom_interface);

	NoDim = 1;

	// Reset the driver
	SsridersDoReset();

	return 0;
}

static INT32 Tmnt2Init()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Tmnt2);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(8, 0);

	K053245Init(0, DrvSpriteRom, DrvSprites, 0x3fffff, K053245SsridersCallback);
	K053245SetSpriteOffset(0, -112, 16);
	K05324xSetZRejection(0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x040000, 3, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,   7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x100000,   8, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x200000,   9, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x300000,  10, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x400000);
	K053245GfxDecode(DrvSpriteRom, DrvSprites, 0x400000);
	
	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 11, 1); if (nRet != 0) return 1;
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x140000, 0x140fff, MAP_RAM);
	SekSetReadWordHandler(0, Ssriders68KReadWord);
	SekSetWriteWordHandler(0, Ssriders68KWriteWord);
	SekSetReadByteHandler(0, Ssriders68KReadByte);
	SekSetWriteByteHandler(0, Ssriders68KWriteByte);

	SekMapHandler(1,		  0x1c0800, 0x1c081f, MAP_WRITE);
	SekSetWriteWordHandler(1, 	Tmnt268KProtWord);
	SekSetWriteByteHandler(1, 	Tmnt268KProtByte);
	SekClose();

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(BlswhstlZ80Read);
	ZetSetWriteHandler(BlswhstlZ80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x200000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&BlswhstlEEPROMInterface);
	
	// Reset the driver
	SsridersDoReset();

	is_tmnt2 = 1;

	return 0;
}

static INT32 QgakumonInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Tmnt2);
	
	K052109Init(DrvTileRom, DrvTiles, 0x0fffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(0, 0);

	K053245Init(0, DrvSpriteRom, DrvSprites, 0x3fffff, K053245SsridersCallback);
	K053245SetSpriteOffset(0, -112, 16);
	K05324xSetZRejection(0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x080001, 2, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x080000, 3, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 4, 1); if (nRet != 0) return 1;
	
	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x080000, 6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x100000);
	GfxDecode(0x8000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000,   7, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x200000,   8, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x400000);
	K053245GfxDecode(DrvSpriteRom, DrvSprites, 0x400000);
	
	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000,  9, 1); if (nRet != 0) return 1;
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRam           , 0x104000, 0x107fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x140000, 0x140fff, MAP_RAM);
	SekSetReadWordHandler(0, Ssriders68KReadWord);
	SekSetWriteWordHandler(0, Ssriders68KWriteWord);
	SekSetReadByteHandler(0, Ssriders68KReadByte);
	SekSetWriteByteHandler(0, Ssriders68KWriteByte);

	SekMapHandler(1,		  0x1c0800, 0x1c081f, MAP_WRITE);
	SekSetWriteWordHandler(1, 	Tmnt268KProtWord);
	SekSetWriteByteHandler(1, 	Tmnt268KProtByte);
	SekClose();

	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(BlswhstlZ80Read);
	ZetSetWriteHandler(BlswhstlZ80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();
	
	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x200000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&BlswhstlEEPROMInterface);
	
	// Reset the driver
	SsridersDoReset();

	return 0;
}

static INT32 PunkshotInit()
{
	GenericTilesInit();

	INT32 nRet = 0;
	
	// Allocate and Blank all required memory
	BurnAllocMemIndexEx(Punkshot);
	
	K052109Init(DrvTileRom, DrvTiles, 0x07ffff);
	K052109SetCallback(K052109TmntCallback);
	K052109AdjustScroll(8, 0); 

	K051960Init(DrvSpriteRom, DrvSprites, 0x1fffff);
	K051960SetCallback(K051960PunkshotCallback);
	K051960SetSpriteOffset(8, 0);

	// Load 68000 Program Roms
	nRet = BurnLoadRom(Drv68KRom + 0x000001, 0, 2); if (nRet != 0) return 1;
	nRet = BurnLoadRom(Drv68KRom + 0x000000, 1, 2); if (nRet != 0) return 1;
	
	// Load Z80 Program Roms
	nRet = BurnLoadRom(DrvZ80Rom, 2, 1); if (nRet != 0) return 1;

	// Load and decode the tiles
	nRet = BurnLoadRom(DrvTileRom + 0x000000, 3, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvTileRom + 0x040000, 4, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvTileRom, 0x80000);
	GfxDecode(0x4000, 4, 8, 8, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, DrvTileRom, DrvTiles);
	
	// Load the sprites
	nRet = BurnLoadRom(DrvSpriteRom + 0x000000, 5, 1); if (nRet != 0) return 1;
	nRet = BurnLoadRom(DrvSpriteRom + 0x100000, 6, 1); if (nRet != 0) return 1;
	konami_rom_deinterleave_2(DrvSpriteRom, 0x200000);
	GfxDecode(0x4000, 4, 16, 16, SpritePlaneOffsets2, SpriteXOffsets, SpriteYOffsets, 0x400, DrvSpriteRom, DrvSprites);	

	// Load K053260 Sample Roms
	nRet = BurnLoadRom(DrvSoundRom + 0x00000, 7, 1); if (nRet != 0) return 1;

	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRom           , 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRam  	 , 0x080000, 0x083fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam       , 0x090000, 0x090fff, MAP_RAM);
	SekSetReadWordHandler(0, 	Punkshot68KReadWord);
	SekSetWriteWordHandler(0, 	Punkshot68KWriteWord);
	SekSetReadByteHandler(0, 	Punkshot68KReadByte);
	SekSetWriteByteHandler(0, 	Punkshot68KWriteByte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(Thndrx2Z80Read);
	ZetSetWriteHandler(Thndrx2Z80Write);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80Rom                );
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80Rom                );
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80Ram                );
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80Ram                );
	ZetClose();

	// Setup the YM2151 emulation
	BurnYM2151Init(3579545);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);
	
	K053260Init(0, 3579545, DrvSoundRom, 0x80000);
	K053260PCMSetAllRoutes(0, 0.70, BURN_SND_ROUTE_BOTH);

	// Reset the driver
	PunkshotDoReset();

	return 0;
}

#undef BurnAllocMemIndexEx

static INT32 CommonExit()
{
	SekExit();

	BurnYM2151Exit();
	
	GenericTilesExit();

	KonamiICExit();

	BurnFree(Mem);

	bIrqEnable = 0;
	DrvSoundLatch = 0;
	TitleSoundLatch = 0;
	PlayTitleSample = 0;
	TitleSamplePos = 0;
	SpriteColourBase = 0;
	memset(LayerColourBase, 0, sizeof(LayerColourBase));
	memset(LayerPri, 0, sizeof(LayerPri));
	PriorityFlag = 0;
	
	DrvNvRamBank = 0;
	CuebrickSndIrqFire = 0;
	BlswhstlTileRomBank = 0;
	DrvVBlank = 0;
	InitEEPROMCount = 0;
	uses_k007232 = 0;
	is_tmnt2 = 0;

	return 0;
}

static INT32 CommonZ80Exit()
{
	ZetExit();
		
	return CommonExit();
}

static INT32 TmntExit()
{
	UPD7759Exit();
	K007232Exit();
	
	return CommonZ80Exit();
}

static INT32 MiaExit()
{
	K007232Exit();
	
	return CommonZ80Exit();
}

static INT32 BlswhstlExit()
{
	K053260Exit();
	EEPROMExit();
	NoDim = 0;

	return CommonZ80Exit();
}

static INT32 PunkshotExit()
{
	K053260Exit();
	
	return CommonZ80Exit();
}

static inline UINT8 pal5bit(UINT8 bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

static void DrvCalcPalette()
{
	UINT16 *PaletteRam = (UINT16*)DrvPaletteRam;
	
	for (INT32 i = 0; i < 0x800; i += 2) {
		INT32 Offset = i & ~1;
		UINT32 Data = (BURN_ENDIAN_SWAP_INT16(PaletteRam[Offset]) << 8) | BURN_ENDIAN_SWAP_INT16(PaletteRam[Offset + 1]);

		DrvPalette[Offset >> 1] = (pal5bit(Data >> 0) << 16) | (pal5bit(Data >> 5) << 8) | pal5bit(Data >> 10);
	}
}

static void BlswhstlCalcPalette()
{
	UINT16 *PaletteRam = (UINT16*)DrvPaletteRam;
	
	for (INT32 i = 0; i < 0x800; i++) {
		UINT32 Data = BURN_ENDIAN_SWAP_INT16(PaletteRam[i]);

		DrvPalette[i] = (pal5bit(Data >> 0) << 16) | (pal5bit(Data >> 5) << 8) | pal5bit(Data >> 10);
	}
}

static void sortlayers(INT32 *layer,INT32 *pri)
{
#define SWAP(a,b) \
	if (pri[a] < pri[b]) \
	{ \
		INT32 t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0,1)
	SWAP(0,2)
	SWAP(1,2)
}

static inline void BlswhstlCalcPaletteWithContrast(INT32 i, INT32 brt)
{
	INT32 r,g,b;
	UINT16 *PaletteRam = (UINT16*)DrvPaletteRam;
	
	UINT32 Data = BURN_ENDIAN_SWAP_INT16(PaletteRam[i]);

	r = pal5bit(Data >> 0);
	g = pal5bit(Data >> 5);
	b = pal5bit(Data >> 10);

	r = (r * brt) / 100;
	g = (g * brt) / 100;
	b = (b * brt) / 100;

	DrvPalette[i] = (r<<16) | (g<<8) | b;
}

static void PaletteDim(INT32 layer2, INT32 layer0)
{
	INT32 i, dim, en, cb, ce, brt;

	dim = dim_v | ((~dim_c & 0x10) >> 1);
	en  = (K053251GetPriority(5) && K053251GetPriority(5) != 0x3e);

	brt = 100;
	if (en) brt -= 40*dim/8;

	if (brt < 100 && !NoDim) {
		cb = LayerColourBase[layer2] << 4;
		ce = cb + 128;

		//bprintf(0, _T("--pri(5) = %02x    pri(6) = %02x\n"), K053251GetPriority(5), K053251GetPriority(6));
		//bprintf(0, _T("dimming layer %x:  %x - %x\n"), layer2, cb, ce);
		if (is_tmnt2 && K053251GetPriority(5) == 0x30) {
			// dim just the first layer (Mars through the windows)
			cb = LayerColourBase[layer0] << 4;
			ce = cb + 128;
			for (i = 0; i < 0x800; i++)
				BlswhstlCalcPaletteWithContrast(i, 100);

			for (i = cb; i < ce; i++) // selected layer
				BlswhstlCalcPaletteWithContrast(i, brt);
		} else {
			for (i =  0; i < cb; i++)
				BlswhstlCalcPaletteWithContrast(i, brt);

			for (i = cb; i < ce; i++) // text
				BlswhstlCalcPaletteWithContrast(i, 100);

			for (i = ce; i < 0x800; i++)
				BlswhstlCalcPaletteWithContrast(i, brt);
		}

		if (~dim_c & 0x10) {
			konami_set_highlight_over_sprites_mode(1);
			konami_set_highlight_mode(1);
		} else {
			konami_set_highlight_over_sprites_mode(0);
			konami_set_highlight_mode(0);
		}

	} else {
		BlswhstlCalcPalette();
	}
}

static INT32 TmntDraw()
{
	DrvCalcPalette();
	K052109UpdateScroll();
	
	K052109RenderLayer(2, K052109_OPAQUE, 0);
	if ((PriorityFlag & 1) == 1) K051960SpritesRender(0, 0);
	K052109RenderLayer(1, 0, 0);
	if ((PriorityFlag & 1) == 0) K051960SpritesRender(0, 0);
	K052109RenderLayer(0, 0, 0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 BlswhstlDraw()
{
	INT32 Layer[3];

	K052109UpdateScroll();
	
	INT32 BGColourBase = K053251GetPaletteIndex(0);
	SpriteColourBase   = K053251GetPaletteIndex(1);
	LayerColourBase[0] = K053251GetPaletteIndex(2);
	LayerColourBase[1] = K053251GetPaletteIndex(4);
	LayerColourBase[2] = K053251GetPaletteIndex(3);

	LayerPri[0] = K053251GetPriority(2);
	LayerPri[1] = K053251GetPriority(4);
	LayerPri[2] = K053251GetPriority(3);
	Layer[0] = 0;
	Layer[1] = 1;
	Layer[2] = 2;

	KonamiClearBitmaps(DrvPalette[16 * BGColourBase]);

	sortlayers(Layer, LayerPri);
	PaletteDim(Layer[2], Layer[0]);
	// save for later (dink)
	//bprintf(0, _T("layer order: %x   %x   %x\n"), Layer[0], Layer[1], Layer[2]);
	//bprintf(0, _T("pri   order: %x   %x   %x\n"), LayerPri[0], LayerPri[1], LayerPri[2]);
	if (nBurnLayer & 1) K052109RenderLayer(Layer[0], 0, 1);
	if (nBurnLayer & 2) K052109RenderLayer(Layer[1], 0, 2);
	if (nBurnLayer & 4) K052109RenderLayer(Layer[2], 0, 4);

	if (nSpriteEnable & 1) K053245SpritesRender(0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 Thndrx2Draw()
{
	INT32 Layer[3];
	
	BlswhstlCalcPalette();
	K052109UpdateScroll();
	
	INT32 BGColourBase = K053251GetPaletteIndex(0);
	SpriteColourBase   = K053251GetPaletteIndex(1);
	LayerColourBase[0] = K053251GetPaletteIndex(2);
	LayerColourBase[1] = K053251GetPaletteIndex(4);
	LayerColourBase[2] = K053251GetPaletteIndex(3);
	
	LayerPri[0] = K053251GetPriority(2);
	LayerPri[1] = K053251GetPriority(4);
	LayerPri[2] = K053251GetPriority(3);
	Layer[0] = 0;
	Layer[1] = 1;
	Layer[2] = 2;
	
	KonamiClearBitmaps(DrvPalette[16 * BGColourBase]);

	sortlayers(Layer, LayerPri);

	if (nBurnLayer & 1) K052109RenderLayer(Layer[0], 0, 1);
	if (nBurnLayer & 2) K052109RenderLayer(Layer[1], 0, 2);
	if (nBurnLayer & 4) K052109RenderLayer(Layer[2], 0, 4);

	if (nSpriteEnable & 1) K051960SpritesRender(-1, -1);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 PunkshotDraw()
{
	INT32 Layer[3];
	
	BlswhstlCalcPalette();
	K052109UpdateScroll();
	
//	INT32 BGColourBase   = K053251GetPaletteIndex(0);
	SpriteColourBase   = K053251GetPaletteIndex(1);
	LayerColourBase[0] = K053251GetPaletteIndex(2);
	LayerColourBase[1] = K053251GetPaletteIndex(4);
	LayerColourBase[2] = K053251GetPaletteIndex(3);
	
	LayerPri[0] = K053251GetPriority(2);
	LayerPri[1] = K053251GetPriority(4);
	LayerPri[2] = K053251GetPriority(3);
	Layer[0] = 0;
	Layer[1] = 1;
	Layer[2] = 2;

	sortlayers(Layer, LayerPri);

	if (nBurnLayer & 1) K052109RenderLayer(Layer[0], K052109_OPAQUE, 1);
	if (nBurnLayer & 2) K052109RenderLayer(Layer[1], 0, 2);
	if (nBurnLayer & 4) K052109RenderLayer(Layer[2], 0, 4);

	if (nSpriteEnable & 1) K051960SpritesRender(-1, -1);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static void RenderTitleSample(INT16 *pSoundBuf, INT32 nLength)
{
	double Addr = TitleSamplePos;
	double Step = (double)20000 / nBurnSoundRate;
	
	for (INT32 i = 0; i < nLength; i += 2) {
		if (Addr > 0x3ffff) break;
		INT16 Sample = DrvTitleSample[(INT32)Addr];
		
		INT16 nLeftSample = 0, nRightSample = 0;
		
		if ((TitleSampleOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Sample * TitleSampleGain);
		}
		if ((TitleSampleOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(Sample * TitleSampleGain);
		}
		
		pSoundBuf[i + 0] += nLeftSample;
		pSoundBuf[i + 1] += nRightSample;
		
		Addr += Step;
	}
	
	TitleSamplePos = Addr;
}

static INT32 TmntFrame()
{
	INT32 nInterleave = nBurnSoundLen;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) TmntDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 8000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == (nInterleave - 1) && bIrqEnable) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();
		
		// Run Z80
		nCurrentCPU = 1;
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;
		ZetClose();
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
			if (PlayTitleSample) RenderTitleSample(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
			if (PlayTitleSample) RenderTitleSample(pSoundBuf, nSegmentLength);
		}
		UPD7759Render(pBurnSoundOut, nBurnSoundLen);

		BurnSoundLimiter(pBurnSoundOut, nBurnSoundLen, 0.55, 1.80);
	}
	
	if (pBurnDraw) TmntDraw();

	return 0;
}

static INT32 MiaFrame()
{
	INT32 nInterleave = nBurnSoundLen;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 8000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == (nInterleave - 1) && bIrqEnable) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();
		
		// Run Z80
		nCurrentCPU = 1;
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;
		ZetClose();
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
		}
	}
	
	if (pBurnDraw) TmntDraw();

	return 0;
}

static INT32 CuebrickFrame()
{
	INT32 nInterleave = 10;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) CuebrickDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 8000000 / 60;
	nCyclesDone[0] = 0;

	SekNewFrame();
	SekOpen(0);
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;		
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == (nInterleave - 1) && bIrqEnable) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (CuebrickSndIrqFire) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
				
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
	}
	
	SekClose();
	
	if (pBurnDraw) TmntDraw();

	return 0;
}

static INT32 BlswhstlFrame()
{
	INT32 nInterleave = 32;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) BlswhstlDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 16000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == (nInterleave - 1) && K052109_irq_enabled) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		SekClose();
		
		// Run Z80
		nCurrentCPU = 1;
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;
		ZetClose();
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}
	
	if (pBurnDraw) BlswhstlDraw();
	
	K053245ClearBuffer(0);

	return 0;
}

static INT32 SsridersFrame()
{
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) SsridersDoReset();

	SsridersMakeInputs();

	nCyclesTotal[0] = 16000000 / 60;
	nCyclesTotal[1] = 8000000 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == 19) DrvVBlank = 0;
		if (i == 240) DrvVBlank = 1;
		SekClose();

		// Run Z80
		nCurrentCPU = 1;
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	SekOpen(0);
	if (K052109_irq_enabled) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	SekClose();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) BlswhstlDraw();

	return 0;
}

static INT32 Thndrx2Frame()
{
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) SsridersDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 12000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		
		// Run Z80
		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (K052109_irq_enabled) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) Thndrx2Draw();

	return 0;
}

static INT32 LgtnfghtFrame()
{
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) SsridersDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 12000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		
		// Run Z80
		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (K052109_irq_enabled) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) BlswhstlDraw();

	return 0;
}

static INT32 Tmnt2Frame()
{
	if (DrvReset) SsridersDoReset();

	SsridersMakeInputs();

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	nCyclesTotal[0] = 16000000 / 60;
	nCyclesTotal[1] = 8000000 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;
	INT32 drawn = 0;

	SekNewFrame();
	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 scanline = (i + 240) % 256;

		SekOpen(0);
		if (scanline == 22) {
			DrvVBlank = 0;
			if (pBurnDraw) BlswhstlDraw();
			drawn = 1;
		}
		if (scanline == 240) {
			DrvVBlank = 1;
			if (K052109_irq_enabled) {
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			}
		}
		CPU_RUN(0, Sek);
		SekClose();

		ZetOpen(0);
		CPU_RUN(1, Zet);
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw && !drawn) BlswhstlDraw();

	return 0;
}

static INT32 PunkshotFrame()
{
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;

	if (DrvReset) PunkshotDoReset();

	DrvMakeInputs();

	nCyclesTotal[0] = 12000000 / 60;
	nCyclesTotal[1] = 3579545 / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekNewFrame();
	ZetNewFrame();
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run 68000
		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		
		// Run Z80
		nCurrentCPU = 1;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[nCurrentCPU] += nCyclesSegment;

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (K052109_irq_enabled) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) PunkshotDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029693;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	KonamiICScan(nAction);
	
	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		
		BurnYM2151Scan(nAction, pnMin);

		SCAN_VAR(bIrqEnable);
		SCAN_VAR(DrvSoundLatch);
		SCAN_VAR(TitleSoundLatch);
		SCAN_VAR(PlayTitleSample);
		SCAN_VAR(TitleSamplePos);
		SCAN_VAR(PriorityFlag);
		SCAN_VAR(DrvVBlank);
		SCAN_VAR(dim_c);
		SCAN_VAR(dim_v);

		SCAN_VAR(LastCntrl);
		SCAN_VAR(Toggle);

		BurnRandomScan(nAction);
	}

	return 0;
}

static INT32 TmntScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		K007232Scan(nAction, pnMin);
		UPD7759Scan(nAction, pnMin);
	}
	
	return DrvScan(nAction, pnMin);
}

static INT32 MiaScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		K007232Scan(nAction, pnMin);
	}
	
	return DrvScan(nAction, pnMin);
}

static INT32 CuebrickScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data = DrvNvRam;
		ba.nLen = 0x400 * 0x20;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(CuebrickSndIrqFire);
		SCAN_VAR(DrvNvRamBank);
	}

	return DrvScan(nAction, pnMin);
}

static INT32 SsridersScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		K053260Scan(nAction, pnMin);

		SCAN_VAR(InitEEPROMCount);

		EEPROMScan(nAction, pnMin);
	}

	return DrvScan(nAction, pnMin);
}

static INT32 Thndrx2aScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		K053260Scan(nAction, pnMin);
	}
	
	return DrvScan(nAction, pnMin);
}

struct BurnDriver BurnDrvTmnt = {
	"tmnt", NULL, NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (World 4 Players, version X)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntRomInfo, TmntRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmntu = {
	"tmntu", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (US 4 Players, version R)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntuRomInfo, TmntuRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmntua = {
	"tmntua", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (US 4 Players, version N)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntuaRomInfo, TmntuaRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmntub = {
	"tmntub", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (US 4 Players, version J)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntubRomInfo, TmntubRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmntuc = {
	"tmntuc", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (US 4 Players, version H)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntucRomInfo, TmntucRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmht = {
	"tmht", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Hero Turtles (UK 4 Players, version F)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmhtRomInfo, TmhtRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmhta = {
	"tmhta", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Hero Turtles (UK 4 Players, version S)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmhtaRomInfo, TmhtaRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmhtb = {
	"tmhtb", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Hero Turtles (UK 4 Players, version ?)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmhtbRomInfo, TmhtbRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmntj = {
	"tmntj", "tmnt", NULL, NULL, "1990",
	"Teenage Mutant Ninja Turtles (Japan 4 Players, version 2)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntjRomInfo, TmntjRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmnta = {
	"tmnta", "tmnt", NULL, NULL, "1990",
	"Teenage Mutant Ninja Turtles (Asia 4 Players, version ?)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, TmntaRomInfo, TmntaRomName, NULL, NULL, NULL, NULL, TmntInputInfo, TmntDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmht2p = {
	"tmht2p", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Hero Turtles (UK 2 Players, version U)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, Tmht2pRomInfo, Tmht2pRomName, NULL, NULL, NULL, NULL, Tmnt2pInputInfo, Tmnt2pDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmht2pa = {
	"tmht2pa", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Hero Turtles (UK 2 Players, version ?)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, Tmht2paRomInfo, Tmht2paRomName, NULL, NULL, NULL, NULL, Tmnt2pInputInfo, Tmnt2pDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmht2pj = {
	"tmnt2pj", "tmnt", NULL, NULL, "1990",
	"Teenage Mutant Ninja Turtles (Japan 2 Players, version 1)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, Tmnt2pjRomInfo, Tmnt2pjRomName, NULL, NULL, NULL, NULL, Tmnt2pInputInfo, Tmnt2pDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvTmht2po = {
	"tmnt2po", "tmnt", NULL, NULL, "1989",
	"Teenage Mutant Ninja Turtles (Oceania 2 Players, version ?)\0", NULL, "Konami", "GX963",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, Tmnt2poRomInfo, Tmnt2poRomName, NULL, NULL, NULL, NULL, Tmnt2pInputInfo, Tmnt2pDIPInfo,
	TmntInit, TmntExit, TmntFrame, TmntDraw, TmntScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvMia = {
	"mia", NULL, NULL, NULL, "1989",
	"M.I.A. - Missing in Action (version T)\0", NULL, "Konami", "GX808",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, MiaRomInfo, MiaRomName, NULL, NULL, NULL, NULL, MiaInputInfo, MiaDIPInfo,
	MiaInit, MiaExit, MiaFrame, TmntDraw, MiaScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvMia2 = {
	"mia2", "mia", NULL, NULL, "1989",
	"M.I.A. - Missing in Action (version S)\0", NULL, "Konami", "GX808",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, Mia2RomInfo, Mia2RomName, NULL, NULL, NULL, NULL, MiaInputInfo, MiaDIPInfo,
	MiaInit, MiaExit, MiaFrame, TmntDraw, MiaScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvCuebrick = {
	"cuebrick", NULL, NULL, NULL, "1989",
	"Cue Brick (World, version D)\0", NULL, "Konami", "GX903",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_PUZZLE, 0,
	NULL, CuebrickRomInfo, CuebrickRomName, NULL, NULL, NULL, NULL, MiaInputInfo, MiaDIPInfo,
	CuebrickInit, CommonExit, CuebrickFrame, TmntDraw, CuebrickScan,
	NULL, 0x400, 304, 224, 4, 3
};

struct BurnDriver BurnDrvBlswhstl = {
	"blswhstl", NULL, NULL, NULL, "1991",
	"Bells & Whistles (World, version L)\0", NULL, "Konami", "GX060",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, BlswhstlRomInfo, BlswhstlRomName, NULL, NULL, NULL, NULL, BlswhstlInputInfo, NULL,
	BlswhstlInit, BlswhstlExit, BlswhstlFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 224, 280, 3, 4
};

struct BurnDriver BurnDrvBlswhstla = {
	"blswhstla", "blswhstl", NULL, NULL, "1991",
	"Bells & Whistles (Asia, version M)\0", NULL, "Konami", "GX060",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, BlswhstlaRomInfo, BlswhstlaRomName, NULL, NULL, NULL, NULL, BlswhstlInputInfo, NULL,
	BlswhstlInit, BlswhstlExit, BlswhstlFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 224, 280, 3, 4
};

struct BurnDriver BurnDrvDetatwin = {
	"detatwin", "blswhstl", NULL, NULL, "1991",
	"Detana!! Twin Bee (Japan ver. J)\0", NULL, "Konami", "GX060",
	L"\u51FA\u305F\u306A!! Twin Bee \u30C4\u30A4\u30F3\u30D3\u30FC (Japan, version J)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, DetatwinRomInfo, DetatwinRomName, NULL, NULL, NULL, NULL, BlswhstlInputInfo, NULL,
	BlswhstlInit, BlswhstlExit, BlswhstlFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 224, 280, 3, 4
};

struct BurnDriver BurnDrvSsriders = {
	"ssriders", NULL, NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. EAC)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersRomInfo, SsridersRomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersebd = {
	"ssridersebd", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (2 Players ver. EBD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersebdRomInfo, SsridersebdRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersebc = {
	"ssridersebc", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (2 Players ver. EBC)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersebcRomInfo, SsridersebcRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersuda = {
	"ssridersuda", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. UDA)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersudaRomInfo, SsridersudaRomName, NULL, NULL, NULL, NULL, Ssriders4psInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsriderseaa = {
	"ssriderseaa", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. EAA)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsriderseaaRomInfo, SsriderseaaRomName, NULL, NULL, NULL, NULL, Ssriders4psInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersuab = {
	"ssridersuab", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. UAB)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersuabRomInfo, SsridersuabRomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersuac = {
	"ssridersuac", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. UAC)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersuacRomInfo, SsridersuacRomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersubc = {
	"ssridersubc", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (2 Players ver. UBC)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersubcRomInfo, SsridersubcRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersabd = {
	"ssridersabd", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (2 Players ver. ABD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersabdRomInfo, SsridersabdRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersadd = {
	"ssridersadd", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. ADD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersaddRomInfo, SsridersaddRomName, NULL, NULL, NULL, NULL, Ssriders4psInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersjac = {
	"ssridersjac", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. JAC)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersjacRomInfo, SsridersjacRomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersjad = {
	"ssridersjad", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (4 Players ver. JAD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersjadRomInfo, SsridersjadRomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvSsridersjbd = {
	"ssridersjbd", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (2 Players ver. JBD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridersjbdRomInfo, SsridersjbdRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	SsridersInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

static INT32 SsridersbInit()
{
	return 1;
}

struct BurnDriverD BurnDrvSsridersb = {
	"ssridersb", "ssriders", NULL, NULL, "1991",
	"Sunset Riders (bootleg 4 Players ver. ADD)\0", NULL, "Konami", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, ssridersbRomInfo, ssridersbRomName, NULL, NULL, NULL, NULL, Ssriders4psInputInfo, NULL,
	SsridersbInit, CommonExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};

struct BurnDriver BurnDrvThndrx2 = {
	"thndrx2", NULL, NULL, NULL, "1991",
	"Thunder Cross II (World)\0", NULL, "Konami", "GX073",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, thndrx2RomInfo, thndrx2RomName, NULL, NULL, NULL, NULL, Thndrx2InputInfo, NULL,
	Thndrx2Init, BlswhstlExit, Thndrx2Frame, Thndrx2Draw, Thndrx2aScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvThndrx2a = {
	"thndrx2a", "thndrx2", NULL, NULL, "1991",
	"Thunder Cross II (Asia)\0", NULL, "Konami", "GX073",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, thndrx2aRomInfo, thndrx2aRomName, NULL, NULL, NULL, NULL, Thndrx2InputInfo, NULL,
	Thndrx2Init, BlswhstlExit, Thndrx2Frame, Thndrx2Draw, Thndrx2aScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvThndrx2j = {
	"thndrx2j", "thndrx2", NULL, NULL, "1991",
	"Thunder Cross II (Japan)\0", NULL, "Konami", "GX073",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, thndrx2jRomInfo, thndrx2jRomName, NULL, NULL, NULL, NULL, Thndrx2InputInfo, NULL,
	Thndrx2Init, BlswhstlExit, Thndrx2Frame, Thndrx2Draw, Thndrx2aScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvLgtnfght = {
	"lgtnfght", NULL, NULL, NULL, "1990",
	"Lightning Fighters (World)\0", NULL, "Konami", "GX939",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, lgtnfghtRomInfo, lgtnfghtRomName, NULL, NULL, NULL, NULL, LgtnfghtInputInfo, LgtnfghtDIPInfo,
	LgtnfghtInit, BlswhstlExit, LgtnfghtFrame, BlswhstlDraw, Thndrx2aScan, 
	NULL, 0x800, 224, 288, 3, 4
};

struct BurnDriver BurnDrvLgtnfghtu = {
	"lgtnfghtu", "lgtnfght", NULL, NULL, "1990",
	"Lightning Fighters (US)\0", NULL, "Konami", "GX939",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, lgtnfghtuRomInfo, lgtnfghtuRomName, NULL, NULL, NULL, NULL, LgtnfghtInputInfo, LgtnfghtDIPInfo,
	LgtnfghtInit, BlswhstlExit, LgtnfghtFrame, BlswhstlDraw, Thndrx2aScan, 
	NULL, 0x800, 224, 288, 3, 4
};

struct BurnDriver BurnDrvLgtnfghta = {
	"lgtnfghta", "lgtnfght", NULL, NULL, "1990",
	"Lightning Fighters (Asia)\0", NULL, "Konami", "GX939",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, lgtnfghtaRomInfo, lgtnfghtaRomName, NULL, NULL, NULL, NULL, LgtnfghtInputInfo, LgtnfghtDIPInfo,
	LgtnfghtInit, BlswhstlExit, LgtnfghtFrame, BlswhstlDraw, Thndrx2aScan, 
	NULL, 0x800, 224, 288, 3, 4
};

struct BurnDriver BurnDrvTrigon = {
	"trigon", "lgtnfght", NULL, NULL, "1990",
	"Trigon (Japan)\0", NULL, "Konami", "GX939",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, trigonRomInfo, trigonRomName, NULL, NULL, NULL, NULL, LgtnfghtInputInfo, LgtnfghtDIPInfo,
	LgtnfghtInit, BlswhstlExit, LgtnfghtFrame, BlswhstlDraw, Thndrx2aScan, 
	NULL, 0x800, 224, 288, 3, 4
};

struct BurnDriver BurnDrvTmnt2 = {
	"tmnt2", NULL, NULL, NULL, "1991",
	"Teenage Mutant Ninja Turtles - Turtles in Time (4 Players ver. UAA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmnt2RomInfo, tmnt2RomName, NULL, NULL, NULL, NULL, Ssriders4pInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvTmnt22pu = {
	"tmnt22pu", "tmnt2", NULL, NULL, "1991",
	"Teenage Mutant Ninja Turtles - Turtles in Time (2 Players ver. UDA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmnt22puRomInfo, tmnt22puRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvTmnt24pu = {
	"tmnt24pu", "tmnt2", NULL, NULL, "1991",
	"Teenage Mutant Ninja Turtles - Turtles in Time (4 Players ver. UEA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmnt24puRomInfo, tmnt24puRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvTmht22pe = {
	"tmht22pe", "tmnt2", NULL, NULL, "1991",
	"Teenage Mutant Hero Turtles - Turtles in Time (2 Players ver. EBA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmht22peRomInfo, tmht22peRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvTmht24pe = {
	"tmht24pe", "tmnt2", NULL, NULL, "1991",
	"Teenage Mutant Hero Turtles - Turtles in Time (4 Players ver. EAA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmht24peRomInfo, tmht24peRomName, NULL, NULL, NULL, NULL, SsridersInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvTmnt2a = {
	"tmnt2a", "tmnt2", NULL, NULL, "1991",
	"Teenage Mutant Ninja Turtles - Turtles in Time (4 Players ver. ADA)\0", NULL, "Konami", "GX063",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, tmnt2aRomInfo, tmnt2aRomName, NULL, NULL, NULL, NULL, Ssriders4psInputInfo, NULL,
	Tmnt2Init, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan,
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvQgakumon = {
	"qgakumon", NULL, NULL, NULL, "1993",
	"Quiz Gakumon no Susume (Japan ver. JA2 Type L)\0", NULL, "Konami", "GX248",
	L"\u30AF\u30A4\u30BA \u5B66\u554F\u30CE\u30B9\u30B9\u30E1 Gakumon no Susume (Japan ver. JA2 Type L)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KONAMI_68K_Z80, GBF_QUIZ, 0,
	NULL, qgakumonRomInfo, qgakumonRomName, NULL, NULL, NULL, NULL, QgakumonInputInfo, NULL,
	QgakumonInit, BlswhstlExit, Tmnt2Frame, BlswhstlDraw, SsridersScan, 
	NULL, 0x800, 304, 224, 4, 3
};

struct BurnDriver BurnDrvPunkshot = {
	"punkshot", NULL, NULL, NULL, "1990",
	"Punk Shot (US 4 Players)\0", NULL, "Konami", "GX907",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_SPORTSMISC, 0,
	NULL, punkshotRomInfo, punkshotRomName, NULL, NULL, NULL, NULL, PunkshotInputInfo, PunkshotDIPInfo,
	PunkshotInit, PunkshotExit, PunkshotFrame, PunkshotDraw, Thndrx2aScan, 
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvPunkshot2 = {
	"punkshot2", "punkshot", NULL, NULL, "1990",
	"Punk Shot (US 2 Players)\0", NULL, "Konami", "GX907",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SPORTSMISC, 0,
	NULL, punkshot2RomInfo, punkshot2RomName, NULL, NULL, NULL, NULL, Punkshot2InputInfo, Punkshot2DIPInfo,
	PunkshotInit, PunkshotExit, PunkshotFrame, PunkshotDraw, Thndrx2aScan, 
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvPunkshot2e = {
	"punkshot2e", "punkshot", NULL, NULL, "1990",
	"Punk Shot (World 2 Players)\0", NULL, "Konami", "GX907",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SPORTSMISC, 0,
	NULL, punkshot2eRomInfo, punkshot2eRomName, NULL, NULL, NULL, NULL, Punkshot2InputInfo, Punkshot2DIPInfo,
	PunkshotInit, PunkshotExit, PunkshotFrame, PunkshotDraw, Thndrx2aScan, 
	NULL, 0x800, 288, 224, 4, 3
};

struct BurnDriver BurnDrvPunkshotj = {
	"punkshotj", "punkshot", NULL, NULL, "1990",
	"Punk Shot (Japan 2 Players)\0", NULL, "Konami", "GX907",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SPORTSMISC, 0,
	NULL, punkshotjRomInfo, punkshotjRomName, NULL, NULL, NULL, NULL, Punkshot2InputInfo, PunkshotjDIPInfo,
	PunkshotInit, PunkshotExit, PunkshotFrame, PunkshotDraw, Thndrx2aScan, 
	NULL, 0x800, 288, 224, 4, 3
};

// -----------------------------------------------------------------------------
// Sunset Riders Hack Series
// -----------------------------------------------------------------------------

static struct BurnRomInfo SsridershsRomDesc[] = {
	/* 1v4 - 20190729 */
	{ "064eac02_1v4.8e",	0x040000, 0xf6bce510, BRF_ESS | BRF_PRG }, //  0 68000 Program Code
	{ "064eac03_1v4.8g",	0x040000, 0x9e998ebe, BRF_ESS | BRF_PRG }, //  1
	{ "064eab04_1v4.10e",	0x020000, 0x06755c89, BRF_ESS | BRF_PRG }, //  2
	{ "064eab05_1v4.10g",	0x020000, 0x0aad4619, BRF_ESS | BRF_PRG }, //  3

	{ "064e01.2f",			0x010000, 0x44b9bc52, BRF_ESS | BRF_PRG }, //  4 Z80 Program 

	{ "064e12.16k",			0x080000, 0xe2bdc619, BRF_GRA },           //  5 Tiles
	{ "064e11.12k",			0x080000, 0x2d8ca8b0, BRF_GRA },           //  6

	{ "064e09.7l",			0x100000, 0x4160c372, BRF_GRA },           //  7 Sprites
	{ "064e07.3l",			0x100000, 0x64dd673c, BRF_GRA },           //  8

	{ "064e06.1d",			0x100000, 0x59810df9, BRF_SND },           //  9 K053260 Samples

	{ "ssriders_eac.nv",	0x000080, 0xf6d641a7, BRF_OPT },           // 10

	/* Firepower - 20220627 */
	{ "064eac02_fp.8e",		0x040000, 0xec967819, BRF_ESS | BRF_PRG }, // 11 68000 Program Code
	{ "064eac03_fp.8g",		0x040000, 0x11437187, BRF_ESS | BRF_PRG }, // 12
	{ "064eab04_fp.10e",	0x020000, 0xf21e4481, BRF_ESS | BRF_PRG }, // 13
	{ "064eab05_fp.10g",	0x020000, 0x81de0fd8, BRF_ESS | BRF_PRG }, // 14

	/* Second Round - 20190420 */
	{ "064eac02_sr.8e",		0x040000, 0x734f4b4c, BRF_ESS | BRF_PRG }, // 15 68000 Program Code
	{ "064eac03_sr.8g",		0x040000, 0xd50e01fd, BRF_ESS | BRF_PRG }, // 16
	{ "064eab04_sr.10e",	0x020000, 0x56bb2e75, BRF_ESS | BRF_PRG }, // 17
//	{ "064eab05_sr.10g",	0x020000, 0x0aad4619, BRF_ESS | BRF_PRG }, // 18
};

STD_ROM_PICK(Ssridershs)
STD_ROM_FN(Ssridershs)

static INT32 SsridershsInit()
{
	nBurnDrvSubActive = (VerSwitcher & 0x03);

	switch (nBurnDrvSubActive) {
		case 0x00:
			pszCustomNameA = "Sunset Riders - 1v4 (Hack)\0";
			break;

		case 0x01:
			pszCustomNameA = "Sunset Riders - Firepower (Hack)\0";
			break;

		case 0x02:
			pszCustomNameA = "Sunset Riders - Second Round (Hack)\0";
	}

	INT32 nRet = SsridersInit();

	if ((0 == nRet) && nBurnDrvSubActive) {
		INT32 nIndex = (0x01 == nBurnDrvSubActive) ? 11 : 15;
		memset(Drv68KRom, 0, 0x0c0000);
		// Load and byte-swap 68000 Program roms
		if (0 != BurnLoadRom(Drv68KRom + 0x000001, nIndex + 0, 2)) return 1;
		if (0 != BurnLoadRom(Drv68KRom + 0x000000, nIndex + 1, 2)) return 1;
		if (0 != BurnLoadRom(Drv68KRom + 0x080001, nIndex + 2, 2)) return 1;
		if (0x02 == nBurnDrvSubActive) nIndex = 0;	// 064eab05_sr.10g == 064eab05_1v4.10g
		if (0 != BurnLoadRom(Drv68KRom + 0x080000, nIndex + 3, 2)) return 1;
	}

	return nRet;
}

struct BurnDriver BurnDrvSsridershs = {
	"ssridershs", "ssriders", NULL, NULL, "2019-2020",
	"Sunset Riders (Hack Series)\0", "Other versions are selected in the dipswitch", "hack", "GX064",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_KONAMI_68K_Z80, GBF_RUNGUN, 0,
	NULL, SsridershsRomInfo, SsridershsRomName, NULL, NULL, NULL, NULL, Ssriders4pVerSwInputInfo, SsridershsDIPInfo,
	SsridershsInit, BlswhstlExit, SsridersFrame, BlswhstlDraw, SsridersScan,
	NULL, 0x810, 288, 224, 4, 3
};
