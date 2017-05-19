#include "cps.h"

INT32 Cps2Volume = 39;
INT32 Cps2DisableDigitalVolume = 0;
UINT8 Cps2VolUp;
UINT8 Cps2VolDwn;

UINT16 Cps2VolumeStates[40] = {
	0xf010, 0xf008, 0xf004, 0xf002, 0xf001, 0xe810, 0xe808, 0xe804, 0xe802, 0xe801,
	0xe410, 0xe408, 0xe404, 0xe402, 0xe401, 0xe210, 0xe208, 0xe204, 0xe202, 0xe201,
	0xe110, 0xe108, 0xe104, 0xe102, 0xe101, 0xe090, 0xe088, 0xe084, 0xe082, 0xe081,
	0xe050, 0xe048, 0xe044, 0xe042, 0xe041, 0xe030, 0xe028, 0xe024, 0xe022, 0xe021
};

// Input Definitions

static struct BurnInputInfo Cps2FightingInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Weak Punch"    , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Medium Punch"  , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Strong Punch"  , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Weak Kick"     , BIT_DIGITAL  , CpsInp011+0, "p1 fire 4" },
	{"P1 Medium Kick"   , BIT_DIGITAL  , CpsInp011+1, "p1 fire 5" },
	{"P1 Strong Kick"   , BIT_DIGITAL  , CpsInp011+2, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Weak Punch"    , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Medium Punch"  , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Strong Punch"  , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Weak Kick"     , BIT_DIGITAL  , CpsInp011+4, "p2 fire 4" },
	{"P2 Medium Kick"   , BIT_DIGITAL  , CpsInp011+5, "p2 fire 5" },
	{"P2 Strong Kick"   , BIT_DIGITAL  , CpsInp020+6, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 7" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 8" },
};

STDINPUTINFO(Cps2Fighting)

static struct BurnInputInfo NineXXInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Bomb"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Bomb"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(NineXX)

static struct BurnInputInfo Nine44InputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Bomb"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Bomb"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Nine44)

static struct BurnInputInfo ArmwarInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsInp020+6, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsInp020+2, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsInp011+3, "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsInp011+2, "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsInp011+1, "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsInp011+0, "p3 right"  },
	{"P3 Attack"        , BIT_DIGITAL  , CpsInp011+4, "p3 fire 1" },
	{"P3 Jump"          , BIT_DIGITAL  , CpsInp011+5, "p3 fire 2" },
	{"P3 Shot"          , BIT_DIGITAL  , CpsInp011+6, "p3 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Armwar)

static struct BurnInputInfo AvspInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsInp020+6, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsInp020+2, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsInp011+3, "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsInp011+2, "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsInp011+1, "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsInp011+0, "p3 right"  },
	{"P3 Shot"          , BIT_DIGITAL  , CpsInp011+4, "p3 fire 1" },
	{"P3 Attack"        , BIT_DIGITAL  , CpsInp011+5, "p3 fire 2" },
	{"P3 Jump"          , BIT_DIGITAL  , CpsInp011+6, "p3 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Avsp)

static struct BurnInputInfo BatcirInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsInp020+6, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsInp020+2, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsInp011+3, "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsInp011+2, "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsInp011+1, "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsInp011+0, "p3 right"  },
	{"P3 Attack"        , BIT_DIGITAL  , CpsInp011+4, "p3 fire 1" },
	{"P3 Jump"          , BIT_DIGITAL  , CpsInp011+5, "p3 fire 2" },

	{"P4 Coin"          , BIT_DIGITAL  , CpsInp020+7, "p4 coin"   },
	{"P4 Start"         , BIT_DIGITAL  , CpsInp020+3, "p4 start"  },
	{"P4 Up"            , BIT_DIGITAL  , CpsInp010+3, "p4 up"     },
	{"P4 Down"          , BIT_DIGITAL  , CpsInp010+2, "p4 down"   },
	{"P4 Left"          , BIT_DIGITAL  , CpsInp010+1, "p4 left"   },
	{"P4 Right"         , BIT_DIGITAL  , CpsInp010+0, "p4 right"  },
	{"P4 Attack"        , BIT_DIGITAL  , CpsInp010+4, "p4 fire 1" },
	{"P4 Jump"          , BIT_DIGITAL  , CpsInp010+5, "p4 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Batcir)

static struct BurnInputInfo ChokoInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot1"         , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Shot2"         , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Shot3"         , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Choko)

static struct BurnInputInfo CsclubInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Button 1"      , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Button 2"      , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Button 3"      , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Button 1"      , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Button 2"      , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Button 3"      , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Csclub)

static struct BurnInputInfo CybotsInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Low Attack"    , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 High Attack"   , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Boost"         , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Weapon"        , BIT_DIGITAL  , CpsInp001+7, "p1 fire 4" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Low Attack"    , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 High Attack"   , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Boost"         , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Weapon"        , BIT_DIGITAL  , CpsInp000+7, "p2 fire 4" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 5" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 6" },
};

STDINPUTINFO(Cybots)

static struct BurnInputInfo DdsomInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Select"        , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Use"           , BIT_DIGITAL  , CpsInp001+7, "p1 fire 4" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Select"        , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Use"           , BIT_DIGITAL  , CpsInp000+7, "p2 fire 4" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsInp020+6, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsInp020+2, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsInp011+3, "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsInp011+2, "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsInp011+1, "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsInp011+0, "p3 right"  },
	{"P3 Attack"        , BIT_DIGITAL  , CpsInp011+4, "p3 fire 1" },
	{"P3 Jump"          , BIT_DIGITAL  , CpsInp011+5, "p3 fire 2" },
	{"P3 Select"        , BIT_DIGITAL  , CpsInp011+6, "p3 fire 3" },
	{"P3 Use"           , BIT_DIGITAL  , CpsInp011+7, "p3 fire 4" },

	{"P4 Coin"          , BIT_DIGITAL  , CpsInp020+7, "p4 coin"   },
	{"P4 Start"         , BIT_DIGITAL  , CpsInp020+3, "p4 start"  },
	{"P4 Up"            , BIT_DIGITAL  , CpsInp010+3, "p4 up"     },
	{"P4 Down"          , BIT_DIGITAL  , CpsInp010+2, "p4 down"   },
	{"P4 Left"          , BIT_DIGITAL  , CpsInp010+1, "p4 left"   },
	{"P4 Right"         , BIT_DIGITAL  , CpsInp010+0, "p4 right"  },
	{"P4 Attack"        , BIT_DIGITAL  , CpsInp010+4, "p4 fire 1" },
	{"P4 Jump"          , BIT_DIGITAL  , CpsInp010+5, "p4 fire 2" },
	{"P4 Select"        , BIT_DIGITAL  , CpsInp010+6, "p4 fire 3" },
	{"P4 Use"           , BIT_DIGITAL  , CpsInp010+7, "p4 fire 4" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 5" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 6" },
};

STDINPUTINFO(Ddsom)

static struct BurnInputInfo DdtodInputList[] =
{
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Use"           , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Select"        , BIT_DIGITAL  , CpsInp001+7, "p1 fire 4" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Use"           , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Select"        , BIT_DIGITAL  , CpsInp000+7, "p2 fire 4" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsInp020+6, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsInp020+2, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsInp011+3, "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsInp011+2, "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsInp011+1, "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsInp011+0, "p3 right"  },
	{"P3 Attack"        , BIT_DIGITAL  , CpsInp011+4, "p3 fire 1" },
	{"P3 Jump"          , BIT_DIGITAL  , CpsInp011+5, "p3 fire 2" },
	{"P3 Use"           , BIT_DIGITAL  , CpsInp011+6, "p3 fire 3" },
	{"P3 Select"        , BIT_DIGITAL  , CpsInp011+7, "p3 fire 4" },

	{"P4 Coin"          , BIT_DIGITAL  , CpsInp020+7, "p4 coin"   },
	{"P4 Start"         , BIT_DIGITAL  , CpsInp020+3, "p4 start"  },
	{"P4 Up"            , BIT_DIGITAL  , CpsInp010+3, "p4 up"     },
	{"P4 Down"          , BIT_DIGITAL  , CpsInp010+2, "p4 down"   },
	{"P4 Left"          , BIT_DIGITAL  , CpsInp010+1, "p4 left"   },
	{"P4 Right"         , BIT_DIGITAL  , CpsInp010+0, "p4 right"  },
	{"P4 Attack"        , BIT_DIGITAL  , CpsInp010+4, "p4 fire 1" },
	{"P4 Jump"          , BIT_DIGITAL  , CpsInp010+5, "p4 fire 2" },
	{"P4 Use"           , BIT_DIGITAL  , CpsInp010+6, "p4 fire 3" },
	{"P4 Select"        , BIT_DIGITAL  , CpsInp010+7, "p4 fire 4" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset,   "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 5" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 6" },
};

STDINPUTINFO(Ddtod)

static struct BurnInputInfo DimahooInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Bomb"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Shot (auto)"   , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Bomb"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Shot (auto)"   , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Dimahoo)

static struct BurnInputInfo EcofghtrInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Turn 1"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Turn 2"        , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Turn 1"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Turn 2"        , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Ecofghtr)

static struct BurnInputInfo GigawingInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Bomb"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Bomb"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Gigawing)

static struct BurnInputInfo JyangokuInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot1"         , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Shot2"         , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Jyangoku)

static struct BurnInputInfo Megaman2InputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Attack"        , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Jump"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Select"        , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Attack"        , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Jump"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Select"        , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Megaman2)

static struct BurnInputInfo Mmancp2uInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Punch"         , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Kick"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Special"       , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Punch"         , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Kick"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Special"       , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Mmancp2u)

static struct BurnInputInfo MmatrixInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 2" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 3" },
};

STDINPUTINFO(Mmatrix)

static struct BurnInputInfo MpangInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot1"         , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Shot2"         , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot1"         , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Shot2"         , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Mpang)

static struct BurnInputInfo ProgearInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Shot"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Bomb"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Auto"          , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Shot"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Bomb"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Auto"          , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Progear)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo Pzloop2InputList[] = {
	{"P1 Coin"          , BIT_DIGITAL   , CpsInp020+4   , "p1 coin"     },
	{"P1 Start"         , BIT_DIGITAL   , CpsInp020+0   , "p1 start"    },
	{"P1 Up"            , BIT_DIGITAL   , CpsInp001+3   , "p1 up"       },
	{"P1 Down"          , BIT_DIGITAL   , CpsInp001+2   , "p1 down"     },
	{"P1 Left"          , BIT_DIGITAL   , CpsInp001+1   , "p1 left"     },
	{"P1 Right"         , BIT_DIGITAL   , CpsInp001+0   , "p1 right"    },
	{"P1 Shot"          , BIT_DIGITAL   , CpsInp001+4   , "p1 fire 1"   },
	A("P1 Paddle"       , BIT_ANALOG_REL, &CpsInpPaddle1, "mouse x-axis"),

	{"P2 Coin"          , BIT_DIGITAL   , CpsInp020+5   , "p2 coin"     },
	{"P2 Start"         , BIT_DIGITAL   , CpsInp020+1   , "p2 start"    },
	{"P2 Up"            , BIT_DIGITAL   , CpsInp000+3   , "p2 up"       },
	{"P2 Down"          , BIT_DIGITAL   , CpsInp000+2   , "p2 down"     },
	{"P2 Left"          , BIT_DIGITAL   , CpsInp000+1   , "p2 left"     },
	{"P2 Right"         , BIT_DIGITAL   , CpsInp000+0   , "p2 right"    },
	{"P2 Shot"          , BIT_DIGITAL   , CpsInp000+4   , "p2 fire 1"   },
	A("P2 Paddle"       , BIT_ANALOG_REL, &CpsInpPaddle2, "p2 z-axis"   ),

	{"Reset"            , BIT_DIGITAL   , &CpsReset     , "reset"       },
	{"Diagnostic"       , BIT_DIGITAL   , CpsInp021+1   , "diag"        },
	{"Service"          , BIT_DIGITAL   , CpsInp021+2   , "service"     },
	{"Volume Up"        , BIT_DIGITAL   , &Cps2VolUp    , "p1 fire 2"   },
	{"Volume Down"      , BIT_DIGITAL   , &Cps2VolDwn   , "p1 fire 3"   },
};

#undef A

STDINPUTINFO(Pzloop2)

static struct BurnInputInfo QndreamInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Answer 1"      , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Answer 2"      , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Answer 3"      , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Answer 4"      , BIT_DIGITAL  , CpsInp001+7, "p1 fire 4" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Answer 1"      , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Answer 2"      , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Answer 3"      , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Answer 4"      , BIT_DIGITAL  , CpsInp000+7, "p2 fire 4" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset,   "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 5" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 6" },
};

STDINPUTINFO(Qndream)

static struct BurnInputInfo RingdestInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Hold"          , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Weak punch"    , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Strong punch"  , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Button 4"      , BIT_DIGITAL  , CpsInp011+0, "p1 fire 4" },
	{"P1 Weak kick"     , BIT_DIGITAL  , CpsInp011+1, "p1 fire 5" },
	{"P1 Strong kick"   , BIT_DIGITAL  , CpsInp011+2, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Hold"          , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Weak punch"    , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Strong punch"  , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Button 4"      , BIT_DIGITAL  , CpsInp011+4, "p2 fire 4" },
	{"P2 Weak kick"     , BIT_DIGITAL  , CpsInp011+5, "p2 fire 5" },
	{"P2 Strong kick"   , BIT_DIGITAL  , CpsInp020+6, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 7" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 8" },
};

STDINPUTINFO(Ringdest)

static struct BurnInputInfo SgemfInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Punch"         , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Kick"          , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Special"       , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Punch"         , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Kick"          , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Special"       , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 4" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 5" },
};

STDINPUTINFO(Sgemf)

static struct BurnInputInfo Spf2tInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Rotate Left"   , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Rotate Right"  , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Rotate Left"   , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Rotate Right"  , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "p1 fire 3" },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "p1 fire 4" },
};

STDINPUTINFO(Spf2t)

// Rom Definitions

static struct BurnRomInfo NinexxRomDesc[] = {
	{ "19xu.03",       0x080000, 0x05955268, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xu.04",       0x080000, 0x3111ab7f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xu.05",       0x080000, 0x38df4a63, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xu.06",       0x080000, 0x5c7e60d3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xx.key",      0x000014, 0x77e67ba1, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexx)
STD_ROM_FN(Ninexx)

static struct BurnRomInfo NinexxaRomDesc[] = {
	{ "09xa.03b",      0x080000, 0x2e994897, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  /* Yes it's actually 09xa, that's not a typo */
	{ "09xa.04b",      0x080000, 0x6364d001, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  /* Yes it's actually 09xa, that's not a typo */
	{ "09xa.05b",      0x080000, 0x00c1949b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  /* Yes it's actually 09xa, that's not a typo */
	{ "09xa.06b",      0x080000, 0x363c1f6e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  /* Yes it's actually 09xa, that's not a typo */
	{ "19xa.07",       0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  /* This one was different, it actually was 19xa */

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxa.key",     0x000014, 0x2cd32eb9, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxa)
STD_ROM_FN(Ninexxa)

static struct BurnRomInfo Ninexxar1RomDesc[] = {
	{ "19xa.03",       0x080000, 0x0c20fd50, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xa.04",       0x080000, 0x1fc37508, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xa.05",       0x080000, 0x6c9cc4ed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xa.06",       0x080000, 0xca5b9f76, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxa.key",     0x000014, 0x2cd32eb9, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxar1)
STD_ROM_FN(Ninexxar1)

static struct BurnRomInfo NinexxbRomDesc[] = {
	{ "19xb.03a",      0x080000, 0x341bdf4a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xb.04a",      0x080000, 0xdff8069e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xb.05a",      0x080000, 0xa47a92a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xb.06a",      0x080000, 0xc52df10d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxb.key",     0x000014, 0x4200e334, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxb)
STD_ROM_FN(Ninexxb)

static struct BurnRomInfo NinexxhRomDesc[] = {
	{ "19xh.03a",      0x080000, 0x357be2ac, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xh.04a",      0x080000, 0xbb13ea3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xh.05a",      0x080000, 0xcbd76601, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xh.06a",      0x080000, 0xb362de8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxh.key",     0x000014, 0x215cf208, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxh)
STD_ROM_FN(Ninexxh)

static struct BurnRomInfo NinexxjRomDesc[] = {
	{ "19xj-03b.6a",   0x080000, 0xbcad93dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj-04b.7a",   0x080000, 0x931882a1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj-05b.8a",   0x080000, 0xe7eeddc4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj-06b.9a",   0x080000, 0xf27cd6b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj-07.6d",    0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // 19x.07

	{ "19x-69.4j",     0x080000, 0x427aeb18, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.13m
	{ "19x-59.4d",     0x080000, 0x63bdbf54, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.15m
	{ "19x-79.4m",     0x080000, 0x2dfe18b5, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.17m
	{ "19x-89.4p",     0x080000, 0xcbef9579, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.19m
	{ "19x-73.8j",     0x080000, 0x8e81f595, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.14m
	{ "19x-74.9j",     0x080000, 0x6d7ad22e, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.14m
	{ "19x-75.10j",    0x080000, 0xcb1a1b6a, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.14m
	{ "19x-76.11j",    0x080000, 0x26fc2b08, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.14m
	{ "19x-63.8d",     0x080000, 0x6f8b045e, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.16m
	{ "19x-64.9d",     0x080000, 0xccd5725a, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.16m
	{ "19x-65.10d",    0x080000, 0x6cf6db35, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.16m
	{ "19x-66.11d",    0x080000, 0x16115dd3, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.16m
	{ "19x-83.8m",     0x080000, 0xc11f88c1, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.18m
	{ "19x-84.9m",     0x080000, 0x68cc9cd8, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.18m
	{ "19x-85.10m",    0x080000, 0xf213666b, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.18m
	{ "19x-86.11m",    0x080000, 0x574e0473, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.18m
	{ "19x-93.8p",     0x080000, 0x9fad3c55, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.20m
	{ "19x-94.9p",     0x080000, 0xe10e252c, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.20m
	{ "19x-95.10p",    0x080000, 0x2b86fa67, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.20m
	{ "19x-96.11p",    0x080000, 0xae6eb692, CPS2_GFX_19XXJ | BRF_GRA }, // 19x.20m

	{ "19x-01.1a",     0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG }, // 19x.01

	{ "19x-51.6a",     0x080000, 0xe9cd7780, CPS2_QSND | BRF_SND }, // 19x.11m
	{ "19x-52.7a",     0x080000, 0xb27b91a8, CPS2_QSND | BRF_SND }, // 19x.11m
	{ "19x-53.8a",     0x080000, 0x2e563ee2, CPS2_QSND | BRF_SND }, // 19x.11m
	{ "19x-54.9a",     0x080000, 0xf47c1f24, CPS2_QSND | BRF_SND }, // 19x.11m
	{ "19x-55.10a",    0x080000, 0x0b1af6e0, CPS2_QSND | BRF_SND }, // 19x.12m
	{ "19x-56.11a",    0x080000, 0xdfa8819f, CPS2_QSND | BRF_SND }, // 19x.12m
	{ "19x-57.12a",    0x080000, 0x229ba777, CPS2_QSND | BRF_SND }, // 19x.12m
	{ "19x-58.13a",    0x080000, 0xc7dceba4, CPS2_QSND | BRF_SND }, // 19x.12m
	
	{ "19xxj.key",     0x000014, 0x9aafa71a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxj)
STD_ROM_FN(Ninexxj)

static struct BurnRomInfo Ninexxjr1RomDesc[] = {
	{ "19xj.03a",      0x080000, 0xed08bdd1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.04a",      0x080000, 0xfb8e3f29, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.05a",      0x080000, 0xaa508ac4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.06a",      0x080000, 0xff2d785b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxj.key",     0x000014, 0x9aafa71a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxjr1)
STD_ROM_FN(Ninexxjr1)

static struct BurnRomInfo Ninexxjr2RomDesc[] = {
	{ "19xj.03",       0x080000, 0x26a381ed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.04",       0x080000, 0x30100cca, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.05",       0x080000, 0xde67e938, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xj.06",       0x080000, 0x39f9a409, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "19xxj.key",     0x000014, 0x9aafa71a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxjr2)
STD_ROM_FN(Ninexxjr2)

static struct BurnRomInfo Nine44RomDesc[] = {
	{ "nffu.03",       0x080000, 0x9693cf8f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nff.04",        0x080000, 0xdba1c66e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nffu.05",       0x080000, 0xea813eb7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "nff.13m",       0x400000, 0xc9fca741, CPS2_GFX | BRF_GRA },
	{ "nff.15m",       0x400000, 0xf809d898, CPS2_GFX | BRF_GRA },
	{ "nff.17m",       0x400000, 0x15ba4507, CPS2_GFX | BRF_GRA },
	{ "nff.19m",       0x400000, 0x3dd41b8c, CPS2_GFX | BRF_GRA },
	{ "nff.14m",       0x100000, 0x3fe3a54b, CPS2_GFX | BRF_GRA },
	{ "nff.16m",       0x100000, 0x565cd231, CPS2_GFX | BRF_GRA },
	{ "nff.18m",       0x100000, 0x63ca5988, CPS2_GFX | BRF_GRA },
	{ "nff.20m",       0x100000, 0x21eb8f3b, CPS2_GFX | BRF_GRA },

	{ "nff.01",        0x020000, 0xd2e44318, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "nff.11m",       0x400000, 0x243e4e05, CPS2_QSND | BRF_SND },
	{ "nff.12m",       0x400000, 0x4fcf1600, CPS2_QSND | BRF_SND },
	
	{ "1944.key",      0x000014, 0x61734f5b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nine44)
STD_ROM_FN(Nine44)

static struct BurnRomInfo Nine44jRomDesc[] = {
	{ "nffj.03",       0x080000, 0x247521ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nff.04",        0x080000, 0xdba1c66e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nffj.05",       0x080000, 0x7f20c2ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "nff.13m",       0x400000, 0xc9fca741, CPS2_GFX | BRF_GRA },
	{ "nff.15m",       0x400000, 0xf809d898, CPS2_GFX | BRF_GRA },
	{ "nff.17m",       0x400000, 0x15ba4507, CPS2_GFX | BRF_GRA },
	{ "nff.19m",       0x400000, 0x3dd41b8c, CPS2_GFX | BRF_GRA },
	{ "nff.14m",       0x100000, 0x3fe3a54b, CPS2_GFX | BRF_GRA },
	{ "nff.16m",       0x100000, 0x565cd231, CPS2_GFX | BRF_GRA },
	{ "nff.18m",       0x100000, 0x63ca5988, CPS2_GFX | BRF_GRA },
	{ "nff.20m",       0x100000, 0x21eb8f3b, CPS2_GFX | BRF_GRA },

	{ "nff.01",        0x020000, 0xd2e44318, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "nff.11m",       0x400000, 0x243e4e05, CPS2_QSND | BRF_SND },
	{ "nff.12m",       0x400000, 0x4fcf1600, CPS2_QSND | BRF_SND },
	
	{ "1944j.key",     0x000014, 0x210202aa, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nine44j)
STD_ROM_FN(Nine44j)

static struct BurnRomInfo ArmwarRomDesc[] = {
	{ "pwge.03c",      0x080000, 0x31f74931, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwge.04c",      0x080000, 0x16f34f5f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwge.05b",      0x080000, 0x4403ed08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09a",       0x080000, 0x4c26baee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwar.key",    0x000014, 0xfe979382, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwar)
STD_ROM_FN(Armwar)

static struct BurnRomInfo Armwarr1RomDesc[] = {
	{ "pwge.03b",      0x080000, 0xe822e3e9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwge.04b",      0x080000, 0x4f89de39, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwge.05a",      0x080000, 0x83df24e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09",        0x080000, 0xddc85ca6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwar.key",    0x000014, 0xfe979382, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwarr1)
STD_ROM_FN(Armwarr1)

static struct BurnRomInfo ArmwaruRomDesc[] = {
	{ "pwgu.03b",      0x080000, 0x8b95497a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgu.04b",      0x080000, 0x29eb5661, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgu.05b",      0x080000, 0xa54e9e44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09a",       0x080000, 0x4c26baee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwaru.key",   0x000014, 0xfb9aada5, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwaru)
STD_ROM_FN(Armwaru)

static struct BurnRomInfo Armwaru1RomDesc[] = {
	{ "pwgu.03a",      0x080000, 0x73d397b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgu.04a",      0x080000, 0x1f1de215, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgu.05a",      0x080000, 0x835fbe73, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09",        0x080000, 0xddc85ca6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwaru.key",   0x000014, 0xfb9aada5, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwaru1)
STD_ROM_FN(Armwaru1)

static struct BurnRomInfo PgearRomDesc[] = {
	{ "pwgj.03a",      0x080000, 0xc79c0c02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgj.04a",      0x080000, 0x167c6ed8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgj.05a",      0x080000, 0xa63fb400, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09a",       0x080000, 0x4c26baee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "pgear.key",     0x000014, 0xc576d6fd, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pgear)
STD_ROM_FN(Pgear)

static struct BurnRomInfo Pgearr1RomDesc[] = {
	{ "pwgj.03",       0x080000, 0xf264e74b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgj.04",       0x080000, 0x23a84983, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwgj.05",       0x080000, 0xbef58c62, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09",        0x080000, 0xddc85ca6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "pgear.key",     0x000014, 0xc576d6fd, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pgearr1)
STD_ROM_FN(Pgearr1)

static struct BurnRomInfo ArmwaraRomDesc[] = {
	{ "pwga.03b",      0x080000, 0x347743e1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwga.04b",      0x080000, 0x42dbfb2e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwga.05b",      0x080000, 0x835fbe73, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09a",       0x080000, 0x4c26baee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwara.key",   0x000014, 0x525439c0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwara)
STD_ROM_FN(Armwara)

static struct BurnRomInfo Armwarar1RomDesc[] = {
	{ "pwga.03a",      0x080000, 0x8d474ab1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwga.04a",      0x080000, 0x81b5aec7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwga.05a",      0x080000, 0x2618e819, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09",        0x080000, 0xddc85ca6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "armwara.key",   0x000014, 0x525439c0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwarar1)
STD_ROM_FN(Armwarar1)

static struct BurnRomInfo AvspRomDesc[] = {
	{ "avpe.03d",      0x080000, 0x774334a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avpe.04d",      0x080000, 0x7fa83769, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.06",        0x080000, 0x190b817f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "avsp.key",      0x000014, 0xe69fa35b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avsp)
STD_ROM_FN(Avsp)

static struct BurnRomInfo AvspaRomDesc[] = {
	{ "avpa.03d",      0x080000, 0x6c1c1858, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avpa.04d",      0x080000, 0x94f50b0c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.06",        0x080000, 0x190b817f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "avspa.key",     0x000014, 0x728efc00, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avspa)
STD_ROM_FN(Avspa)

static struct BurnRomInfo AvsphRomDesc[] = {
	{ "avph.03d",      0x080000, 0x3e440447, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avph.04d",      0x080000, 0xaf6fc82f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.06",        0x080000, 0x190b817f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "avsph.key",     0x000014, 0xcae7b680, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avsph)
STD_ROM_FN(Avsph)

static struct BurnRomInfo AvspjRomDesc[] = {
	{ "avpj.03d",      0x080000, 0x49799119, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avpj.04d",      0x080000, 0x8cd2bba8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.06",        0x080000, 0x190b817f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "avspj.key",     0x000014, 0x3d5ccc08, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avspj)
STD_ROM_FN(Avspj)

static struct BurnRomInfo AvspuRomDesc[] = {
	{ "avpu.03d",      0x080000, 0x42757950, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avpu.04d",      0x080000, 0x5abcdee6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.06",        0x080000, 0x190b817f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "avspu.key",     0x000014, 0x4e68e346, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avspu)
STD_ROM_FN(Avspu)

static struct BurnRomInfo BatcirRomDesc[] = {
	{ "btce.03",       0x080000, 0xbc60484b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btce.04",       0x080000, 0x457d55f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btce.05",       0x080000, 0xe86560d7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btce.06",       0x080000, 0xf778e61b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.07",        0x080000, 0x7322d5db, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.08",        0x080000, 0x6aac85ab, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.09",        0x080000, 0x1203db08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "btc.13m",       0x400000, 0xdc705bad, CPS2_GFX | BRF_GRA },
	{ "btc.15m",       0x400000, 0xe5779a3c, CPS2_GFX | BRF_GRA },
	{ "btc.17m",       0x400000, 0xb33f4112, CPS2_GFX | BRF_GRA },
	{ "btc.19m",       0x400000, 0xa6fcdb7e, CPS2_GFX | BRF_GRA },

	{ "btc.01",        0x020000, 0x1e194310, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "btc.02",        0x020000, 0x01aeb8e6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "btc.11m",       0x200000, 0xc27f2229, CPS2_QSND | BRF_SND },
	{ "btc.12m",       0x200000, 0x418a2e33, CPS2_QSND | BRF_SND },
	
	{ "batcir.key",    0x000014, 0xe316ae67, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Batcir)
STD_ROM_FN(Batcir)

static struct BurnRomInfo BatciraRomDesc[] = {
	{ "btca.03",       0x080000, 0x1ad20d87, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btca.04",       0x080000, 0x2b3f4dbe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btca.05",       0x080000, 0x8238a3d9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btca.06",       0x080000, 0x446c7c02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.07",        0x080000, 0x7322d5db, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.08",        0x080000, 0x6aac85ab, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.09",        0x080000, 0x1203db08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "btc.13m",       0x400000, 0xdc705bad, CPS2_GFX | BRF_GRA },
	{ "btc.15m",       0x400000, 0xe5779a3c, CPS2_GFX | BRF_GRA },
	{ "btc.17m",       0x400000, 0xb33f4112, CPS2_GFX | BRF_GRA },
	{ "btc.19m",       0x400000, 0xa6fcdb7e, CPS2_GFX | BRF_GRA },

	{ "btc.01",        0x020000, 0x1e194310, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "btc.02",        0x020000, 0x01aeb8e6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "btc.11m",       0x200000, 0xc27f2229, CPS2_QSND | BRF_SND },
	{ "btc.12m",       0x200000, 0x418a2e33, CPS2_QSND | BRF_SND },
	
	{ "batcira.key",   0x000014, 0x384500f3, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Batcira)
STD_ROM_FN(Batcira)

static struct BurnRomInfo BatcirjRomDesc[] = {
	{ "btcj.03",       0x080000, 0x6b7e168d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btcj.04",       0x080000, 0x46ba3467, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btcj.05",       0x080000, 0x0e23a859, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btcj.06",       0x080000, 0xa853b59c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.07",        0x080000, 0x7322d5db, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.08",        0x080000, 0x6aac85ab, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.09",        0x080000, 0x1203db08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "btc.13m",       0x400000, 0xdc705bad, CPS2_GFX | BRF_GRA },
	{ "btc.15m",       0x400000, 0xe5779a3c, CPS2_GFX | BRF_GRA },
	{ "btc.17m",       0x400000, 0xb33f4112, CPS2_GFX | BRF_GRA },
	{ "btc.19m",       0x400000, 0xa6fcdb7e, CPS2_GFX | BRF_GRA },

	{ "btc.01",        0x020000, 0x1e194310, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "btc.02",        0x020000, 0x01aeb8e6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "btc.11m",       0x200000, 0xc27f2229, CPS2_QSND | BRF_SND },
	{ "btc.12m",       0x200000, 0x418a2e33, CPS2_QSND | BRF_SND },
	
	{ "batcirj.key",   0x000014, 0x9f9fb965, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Batcirj)
STD_ROM_FN(Batcirj)

static struct BurnRomInfo ChokoRomDesc[] = {
	{ "tkoj.03",       0x080000, 0x11f5452f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "tkoj.04",       0x080000, 0x68655378, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "tkoj1_d.simm1", 0x200000, 0x6933377d, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj1_c.simm1", 0x200000, 0x7f668950, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj1_b.simm1", 0x200000, 0xcfb68ca9, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj1_a.simm1", 0x200000, 0x437e21c5, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj3_d.simm3", 0x200000, 0xa9e32b57, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj3_c.simm3", 0x200000, 0xb7ab9338, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj3_b.simm3", 0x200000, 0x4d3f919a, CPS2_GFX_SIMM | BRF_GRA },
	{ "tkoj3_a.simm3", 0x200000, 0xcfef17ab, CPS2_GFX_SIMM | BRF_GRA },
	
	{ "tko.01",        0x020000, 0x6eda50c2, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "tkoj5_a.simm5", 0x200000, 0xab45d509, CPS2_QSND_SIMM_BYTESWAP | BRF_SND },	
	{ "tkoj5_b.simm5", 0x200000, 0xfa905c3d, CPS2_QSND_SIMM_BYTESWAP | BRF_SND },
	
	{ "choko.key",     0x000014, 0x08505e8b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Choko)
STD_ROM_FN(Choko)

static struct BurnRomInfo CsclubRomDesc[] = {
	{ "csce.03a",      0x080000, 0x824082be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.04a",      0x080000, 0x74e6a4fe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.05a",      0x080000, 0x8ae0df19, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.06a",      0x080000, 0x51f2f0d3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.07a",      0x080000, 0x003968fd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.73",        0x080000, 0x335f07c3, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.74",        0x080000, 0xab215357, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.75",        0x080000, 0xa2367381, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.76",        0x080000, 0x728aac1f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.63",        0x080000, 0x3711b8ca, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.64",        0x080000, 0x828a06d8, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.65",        0x080000, 0x86ee4569, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.66",        0x080000, 0xc24f577f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.83",        0x080000, 0x0750d12a, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.84",        0x080000, 0x90a92f39, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.85",        0x080000, 0xd08ab012, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.86",        0x080000, 0x41652583, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.93",        0x080000, 0xa756c7f7, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.94",        0x080000, 0xfb7ccc73, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.95",        0x080000, 0x4d014297, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.96",        0x080000, 0x6754b1ef, CPS2_GFX_SPLIT4 | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.51",        0x080000, 0x5a52afd5, CPS2_QSND | BRF_SND },
	{ "csc.52",        0x080000, 0x1408a811, CPS2_QSND | BRF_SND },
	{ "csc.53",        0x080000, 0x4fb9f57c, CPS2_QSND | BRF_SND },
	{ "csc.54",        0x080000, 0x9a8f40ec, CPS2_QSND | BRF_SND },
	{ "csc.55",        0x080000, 0x91529a91, CPS2_QSND | BRF_SND },
	{ "csc.56",        0x080000, 0x9a345334, CPS2_QSND | BRF_SND },
	{ "csc.57",        0x080000, 0xaedc27f2, CPS2_QSND | BRF_SND },
	{ "csc.58",        0x080000, 0x2300b7b3, CPS2_QSND | BRF_SND },
	
	{ "csclub.key",    0x000014, 0x903907d7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclub)
STD_ROM_FN(Csclub)

static struct BurnRomInfo Csclub1RomDesc[] = {
	{ "csce.03",       0x080000, 0xf2c852ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.04",       0x080000, 0x1184530f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.05",       0x080000, 0x804e2b6b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce.06",       0x080000, 0x09277cb9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc.07",        0x080000, 0x01b05caa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.14m",       0x200000, 0xe8904afa, CPS2_GFX | BRF_GRA },
	{ "csc.16m",       0x200000, 0xc98c8079, CPS2_GFX | BRF_GRA },
	{ "csc.18m",       0x200000, 0xc030df5a, CPS2_GFX | BRF_GRA },
	{ "csc.20m",       0x200000, 0xb4e55863, CPS2_GFX | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.11m",       0x200000, 0xa027b827, CPS2_QSND | BRF_SND },
	{ "csc.12m",       0x200000, 0xcb7f6e55, CPS2_QSND | BRF_SND },
	
	{ "csclub.key",    0x000014, 0x903907d7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclub1)
STD_ROM_FN(Csclub1)

static struct BurnRomInfo CsclubaRomDesc[] = {
	{ "csca.03",       0x080000, 0xb6acd708, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csca.04",       0x080000, 0xd44ae35f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csca.05",       0x080000, 0x8da76aec, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csca.06",       0x080000, 0xa1b7b1ee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc.07",        0x080000, 0x01b05caa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.14m",       0x200000, 0xe8904afa, CPS2_GFX | BRF_GRA },
	{ "csc.16m",       0x200000, 0xc98c8079, CPS2_GFX | BRF_GRA },
	{ "csc.18m",       0x200000, 0xc030df5a, CPS2_GFX | BRF_GRA },
	{ "csc.20m",       0x200000, 0xb4e55863, CPS2_GFX | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.11m",       0x200000, 0xa027b827, CPS2_QSND | BRF_SND },
	{ "csc.12m",       0x200000, 0xcb7f6e55, CPS2_QSND | BRF_SND },
	
	{ "cscluba.key",   0x000014, 0x591908dc, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cscluba)
STD_ROM_FN(Cscluba)

static struct BurnRomInfo CsclubhRomDesc[] = {
	{ "csch.03",       0x080000, 0x0dd7e46d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csch.04",       0x080000, 0x486e8143, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csch.05",       0x080000, 0x9e509dfb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csch.06",       0x080000, 0x817ba313, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc.07",        0x080000, 0x01b05caa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.14m",       0x200000, 0xe8904afa, CPS2_GFX | BRF_GRA },
	{ "csc.16m",       0x200000, 0xc98c8079, CPS2_GFX | BRF_GRA },
	{ "csc.18m",       0x200000, 0xc030df5a, CPS2_GFX | BRF_GRA },
	{ "csc.20m",       0x200000, 0xb4e55863, CPS2_GFX | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.11m",       0x200000, 0xa027b827, CPS2_QSND | BRF_SND },
	{ "csc.12m",       0x200000, 0xcb7f6e55, CPS2_QSND | BRF_SND },
	
	{ "csclubh.key",   0x000014, 0xb0adc39e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclubh)
STD_ROM_FN(Csclubh)

static struct BurnRomInfo CsclubjRomDesc[] = {
	{ "cscj.03",       0x080000, 0xec4ddaa2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.04",       0x080000, 0x60c632bb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.05",       0x080000, 0xad042003, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.06",       0x080000, 0x169e4d40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc.07",        0x080000, 0x01b05caa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.14m",       0x200000, 0xe8904afa, CPS2_GFX | BRF_GRA },
	{ "csc.16m",       0x200000, 0xc98c8079, CPS2_GFX | BRF_GRA },
	{ "csc.18m",       0x200000, 0xc030df5a, CPS2_GFX | BRF_GRA },
	{ "csc.20m",       0x200000, 0xb4e55863, CPS2_GFX | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.11m",       0x200000, 0xa027b827, CPS2_QSND | BRF_SND },
	{ "csc.12m",       0x200000, 0xcb7f6e55, CPS2_QSND | BRF_SND },
	
	{ "csclubj.key",   0x000014, 0x519a04db, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclubj)
STD_ROM_FN(Csclubj)

static struct BurnRomInfo CsclubjyRomDesc[] = {
// this is fairly redundant, same code as csclubj, same gfx as csclub (yellow case - all eprom), but it's a valid shipped combination
	{ "cscj.03",       0x080000, 0xec4ddaa2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.04",       0x080000, 0x60c632bb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.05",       0x080000, 0xad042003, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cscj.06",       0x080000, 0x169e4d40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc.07",        0x080000, 0x01b05caa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.73",        0x080000, 0x335f07c3, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.74",        0x080000, 0xab215357, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.75",        0x080000, 0xa2367381, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.76",        0x080000, 0x728aac1f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.63",        0x080000, 0x3711b8ca, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.64",        0x080000, 0x828a06d8, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.65",        0x080000, 0x86ee4569, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.66",        0x080000, 0xc24f577f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.83",        0x080000, 0x0750d12a, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.84",        0x080000, 0x90a92f39, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.85",        0x080000, 0xd08ab012, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.86",        0x080000, 0x41652583, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.93",        0x080000, 0xa756c7f7, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.94",        0x080000, 0xfb7ccc73, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.95",        0x080000, 0x4d014297, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.96",        0x080000, 0x6754b1ef, CPS2_GFX_SPLIT4 | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.51",        0x080000, 0x5a52afd5, CPS2_QSND | BRF_SND },
	{ "csc.52",        0x080000, 0x1408a811, CPS2_QSND | BRF_SND },
	{ "csc.53",        0x080000, 0x4fb9f57c, CPS2_QSND | BRF_SND },
	{ "csc.54",        0x080000, 0x9a8f40ec, CPS2_QSND | BRF_SND },
	{ "csc.55",        0x080000, 0x91529a91, CPS2_QSND | BRF_SND },
	{ "csc.56",        0x080000, 0x9a345334, CPS2_QSND | BRF_SND },
	{ "csc.57",        0x080000, 0xaedc27f2, CPS2_QSND | BRF_SND },
	{ "csc.58",        0x080000, 0x2300b7b3, CPS2_QSND | BRF_SND },
	
	{ "csclubj.key",   0x000014, 0x519a04db, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclubjy)
STD_ROM_FN(Csclubjy)

static struct BurnRomInfo CybotsRomDesc[] = {
	{ "cybe.03",       0x080000, 0x234381cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cybe.04",       0x080000, 0x80691061, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.05",        0x080000, 0xec40408e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.06",        0x080000, 0x1ad0bed2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.07",        0x080000, 0x6245a39a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.08",        0x080000, 0x4b48e223, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.09",        0x080000, 0xe15238f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.10",        0x080000, 0x75f4003b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "cyb.13m",       0x400000, 0xf0dce192, CPS2_GFX | BRF_GRA },
	{ "cyb.15m",       0x400000, 0x187aa39c, CPS2_GFX | BRF_GRA },
	{ "cyb.17m",       0x400000, 0x8a0e4b12, CPS2_GFX | BRF_GRA },
	{ "cyb.19m",       0x400000, 0x34b62612, CPS2_GFX | BRF_GRA },
	{ "cyb.14m",       0x400000, 0xc1537957, CPS2_GFX | BRF_GRA },
	{ "cyb.16m",       0x400000, 0x15349e86, CPS2_GFX | BRF_GRA },
	{ "cyb.18m",       0x400000, 0xd83e977d, CPS2_GFX | BRF_GRA },
	{ "cyb.20m",       0x400000, 0x77cdad5c, CPS2_GFX | BRF_GRA },

	{ "cyb.01",        0x020000, 0x9c0fb079, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "cyb.02",        0x020000, 0x51cb0c4e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "cyb.11m",       0x200000, 0x362ccab2, CPS2_QSND | BRF_SND },
	{ "cyb.12m",       0x200000, 0x7066e9cc, CPS2_QSND | BRF_SND },
	
	{ "cybots.key",    0x000014, 0x9bbcbef3, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cybots)
STD_ROM_FN(Cybots)

static struct BurnRomInfo CybotsjRomDesc[] = {
	{ "cybj.03",       0x080000, 0x6096eada, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cybj.04",       0x080000, 0x7b0ffaa9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.05",        0x080000, 0xec40408e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.06",        0x080000, 0x1ad0bed2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.07",        0x080000, 0x6245a39a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.08",        0x080000, 0x4b48e223, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.09",        0x080000, 0xe15238f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.10",        0x080000, 0x75f4003b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "cyb.13m",       0x400000, 0xf0dce192, CPS2_GFX | BRF_GRA },
	{ "cyb.15m",       0x400000, 0x187aa39c, CPS2_GFX | BRF_GRA },
	{ "cyb.17m",       0x400000, 0x8a0e4b12, CPS2_GFX | BRF_GRA },
	{ "cyb.19m",       0x400000, 0x34b62612, CPS2_GFX | BRF_GRA },
	{ "cyb.14m",       0x400000, 0xc1537957, CPS2_GFX | BRF_GRA },
	{ "cyb.16m",       0x400000, 0x15349e86, CPS2_GFX | BRF_GRA },
	{ "cyb.18m",       0x400000, 0xd83e977d, CPS2_GFX | BRF_GRA },
	{ "cyb.20m",       0x400000, 0x77cdad5c, CPS2_GFX | BRF_GRA },

	{ "cyb.01",        0x020000, 0x9c0fb079, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "cyb.02",        0x020000, 0x51cb0c4e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "cyb.11m",       0x200000, 0x362ccab2, CPS2_QSND | BRF_SND },
	{ "cyb.12m",       0x200000, 0x7066e9cc, CPS2_QSND | BRF_SND },
	
	{ "cybotsj.key",   0x000014, 0xd4d560b7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cybotsj)
STD_ROM_FN(Cybotsj)

static struct BurnRomInfo CybotsuRomDesc[] = {
	{ "cybu.03",       0x080000, 0xdb4da8f4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cybu.04",       0x080000, 0x1eec68ac, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.05",        0x080000, 0xec40408e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.06",        0x080000, 0x1ad0bed2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.07",        0x080000, 0x6245a39a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.08",        0x080000, 0x4b48e223, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.09",        0x080000, 0xe15238f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.10",        0x080000, 0x75f4003b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "cyb.13m",       0x400000, 0xf0dce192, CPS2_GFX | BRF_GRA },
	{ "cyb.15m",       0x400000, 0x187aa39c, CPS2_GFX | BRF_GRA },
	{ "cyb.17m",       0x400000, 0x8a0e4b12, CPS2_GFX | BRF_GRA },
	{ "cyb.19m",       0x400000, 0x34b62612, CPS2_GFX | BRF_GRA },
	{ "cyb.14m",       0x400000, 0xc1537957, CPS2_GFX | BRF_GRA },
	{ "cyb.16m",       0x400000, 0x15349e86, CPS2_GFX | BRF_GRA },
	{ "cyb.18m",       0x400000, 0xd83e977d, CPS2_GFX | BRF_GRA },
	{ "cyb.20m",       0x400000, 0x77cdad5c, CPS2_GFX | BRF_GRA },

	{ "cyb.01",        0x020000, 0x9c0fb079, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "cyb.02",        0x020000, 0x51cb0c4e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "cyb.11m",       0x200000, 0x362ccab2, CPS2_QSND | BRF_SND },
	{ "cyb.12m",       0x200000, 0x7066e9cc, CPS2_QSND | BRF_SND },
	
	{ "cybotsu.key",   0x000014, 0x7a09403c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cybotsu)
STD_ROM_FN(Cybotsu)

static struct BurnRomInfo DdsomRomDesc[] = {
	{ "dd2e.03e",      0x080000, 0x449361AF, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.04e",      0x080000, 0x5B7052B6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.05e",      0x080000, 0x788D5F60, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.06e",      0x080000, 0xE0807E1E, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.07",       0x080000, 0xbb777a02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsom.key",     0x000014, 0x541e425d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsom)
STD_ROM_FN(Ddsom)

static struct BurnRomInfo Ddsomr1RomDesc[] = {
	{ "dd2e.03d",      0x080000, 0x6c084ab5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.04d",      0x080000, 0x9b94a947, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.05d",      0x080000, 0x5d6a63c6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.06d",      0x080000, 0x31bde8ee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.07",       0x080000, 0xbb777a02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsom.key",     0x000014, 0x541e425d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomr1)
STD_ROM_FN(Ddsomr1)

static struct BurnRomInfo Ddsomr2RomDesc[] = {
	{ "dd2e.03b",      0x080000, 0xcd2deb66, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.04b",      0x080000, 0xbfee43cc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.05b",      0x080000, 0x049ab19d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.06b",      0x080000, 0x3994fb8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.07",       0x080000, 0xbb777a02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsom.key",     0x000014, 0x541e425d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomr2)
STD_ROM_FN(Ddsomr2)

static struct BurnRomInfo Ddsomr3RomDesc[] = {
	{ "dd2e.03a",      0x080000, 0x6de67678, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.04a",      0x080000, 0x0e45739a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.05a",      0x080000, 0x3dce8025, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.06a",      0x080000, 0x51bafbef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.07",       0x080000, 0xbb777a02, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },	
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsom.key",     0x000014, 0x541e425d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomr3)
STD_ROM_FN(Ddsomr3)

static struct BurnRomInfo DdsomaRomDesc[] = {
	{ "dd2a.03g",      0x080000, 0x0b4fec22, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2a.04g",      0x080000, 0x055b7019, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05g",       0x080000, 0x5eb1991c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06g",       0x080000, 0xc26b5e55, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsoma.key",    0x000014, 0x8c3cc560, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsoma)
STD_ROM_FN(Ddsoma)

static struct BurnRomInfo Ddsomar1RomDesc[] = {
	{ "dd2a.03c",      0x080000, 0x17162039, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2a.04c",      0x080000, 0x950bec38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2a.05c",      0x080000, 0xfa298eba, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2a.06c",      0x080000, 0x28f75b35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // dd2a.07 on sticker
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // dd2a.08 on sticker
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // dd2a.09 on sticker
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // dd2a.10 on sticker

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",       0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG }, // dd2a.01 on sticker
	{ "dd2.02",       0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG }, // dd2a.02 on sticker

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsoma.key",    0x000014, 0x8c3cc560, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomar1)
STD_ROM_FN(Ddsomar1)

static struct BurnRomInfo DdsombRomDesc[] = {
	{ "dd2b.03a",      0x080000, 0xe8ce7fbb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2b.04a",      0x080000, 0x6b679664, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2b.05a",      0x080000, 0x9b2534eb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2b.06a",      0x080000, 0x3b21ba59, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2b.07",       0x080000, 0xfce2558d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomb.key",    0x000014, 0x00b4cc49, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomb)
STD_ROM_FN(Ddsomb)

static struct BurnRomInfo DdsomhRomDesc[] = {
	{ "dd2h.03a",      0x080000, 0xe472c9f3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2h.04a",      0x080000, 0x315a7706, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2h.05a",      0x080000, 0x9b2534eb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2h.06a",      0x080000, 0x3b21ba59, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2h.07a",      0x080000, 0xfce2558d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.08",       0x080000, 0x30970890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.09",       0x080000, 0x99e2194d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2e.10",       0x080000, 0xe198805e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomh.key",    0x000014, 0xcaf6b540, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomh)
STD_ROM_FN(Ddsomh)

static struct BurnRomInfo DdsomjRomDesc[] = {
	{ "dd2j.03g",      0x080000, 0xe6c8c985, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2j.04g",      0x080000, 0x8386c0bd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05g",       0x080000, 0x5eb1991c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06g",       0x080000, 0xc26b5e55, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomj.key",    0x000014, 0xd8dadb22, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomj)
STD_ROM_FN(Ddsomj)

static struct BurnRomInfo Ddsomjr1RomDesc[] = {
	{ "dd2j.03b",      0x080000, 0x965d74e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2j.04b",      0x080000, 0x958eb8f3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05b",       0x080000, 0xd38571ca, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06b",       0x080000, 0x6d5a3bbb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomj.key",    0x000014, 0xd8dadb22, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomjr1)
STD_ROM_FN(Ddsomjr1)

static struct BurnRomInfo Ddsomjr2RomDesc[] = {
	{ "dd2j.03b",      0x080000, 0xb2fd4a24, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2j.04b",      0x080000, 0x3a68c310, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05b",       0x080000, 0xaa56f42f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06b",       0x080000, 0x2f8cd040, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomj.key",    0x000014, 0xd8dadb22, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomjr2)
STD_ROM_FN(Ddsomjr2)

static struct BurnRomInfo DdsomuRomDesc[] = {
	{ "dd2u.03g",      0x080000, 0xfb089b39, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2u.04g",      0x080000, 0xcd432b73, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05g",       0x080000, 0x5eb1991c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06g",       0x080000, 0xc26b5e55, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomu.key",    0x000014, 0x09ae0f7c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomu)
STD_ROM_FN(Ddsomu)

static struct BurnRomInfo Ddsomur1RomDesc[] = {
	{ "dd2u.03d",      0x080000, 0x0f700d84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2u.04d",      0x080000, 0xb99eb254, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05d",       0x080000, 0xb23061f3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06d",       0x080000, 0x8bf1d8ce, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.10",        0x080000, 0xad954c26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "ddsomu.key",    0x000014, 0x09ae0f7c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomur1)
STD_ROM_FN(Ddsomur1)

static struct BurnRomInfo DdtodRomDesc[] = {
	{ "dade.03c",      0x080000, 0x8e73533d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dade.04c",      0x080000, 0x00c2e82e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dade.05c",      0x080000, 0xea996008, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06a",       0x080000, 0x6225495a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07a",       0x080000, 0xb3480ec3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtod.key",     0x000014, 0x41dfca41, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtod)
STD_ROM_FN(Ddtod)

static struct BurnRomInfo Ddtodr1RomDesc[] = {
	{ "dade.03a",      0x080000, 0x665a035e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dade.04a",      0x080000, 0x02613207, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dade.05a",      0x080000, 0x36845996, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtod.key",     0x000014, 0x41dfca41, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodr1)
STD_ROM_FN(Ddtodr1)

static struct BurnRomInfo DdtodaRomDesc[] = {
	{ "dada.03c",      0x080000, 0xbf243e15, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dada.04c",      0x080000, 0x76551eec, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dada.05c",      0x080000, 0x0a0ad827, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06a",       0x080000, 0x6225495a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07a",       0x080000, 0xb3480ec3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtoda.key",    0x000014, 0xe5e8d1b8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtoda)
STD_ROM_FN(Ddtoda)

static struct BurnRomInfo Ddtodar1RomDesc[] = {
	{ "dada.03a",      0x080000, 0xfc6f2dd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dada.04a",      0x080000, 0xd4be4009, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dada.05a",      0x080000, 0x6712d1cf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtoda.key",    0x000014, 0xe5e8d1b8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodar1)
STD_ROM_FN(Ddtodar1)

static struct BurnRomInfo DdtodhRomDesc[] = {
	{ "dadh.03c",      0x080000, 0x5750a861, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.04c",      0x080000, 0xcfbf1b56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.05c",      0x080000, 0xa6e562ba, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06a",       0x080000, 0x6225495a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07a",       0x080000, 0xb3480ec3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodh.key",    0x000014, 0x65f33a1c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodh)
STD_ROM_FN(Ddtodh)

static struct BurnRomInfo Ddtodhr1RomDesc[] = {
	{ "dadh.03b",      0x080000, 0xae0cb98e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.04b",      0x080000, 0xb5774363, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.05b",      0x080000, 0x6ce2a485, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodh.key",    0x000014, 0x65f33a1c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodhr1)
STD_ROM_FN(Ddtodhr1)

static struct BurnRomInfo Ddtodhr2RomDesc[] = {
	{ "dadh.03a",      0x080000, 0x43d04aa3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.04a",      0x080000, 0x8b8d296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadh.05a",      0x080000, 0xdaae6b14, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodh.key",    0x000014, 0x65f33a1c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodhr2)
STD_ROM_FN(Ddtodhr2)

static struct BurnRomInfo DdtodjRomDesc[] = {
	{ "dadj.03c",      0x080000, 0x0b1b5798, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.04c",      0x080000, 0xc6a2fbc8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.05c",      0x080000, 0x189b15fe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06a",       0x080000, 0x6225495a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07a",       0x080000, 0xb3480ec3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodj.key",    0x000014, 0x5414dfca, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodj)
STD_ROM_FN(Ddtodj)

static struct BurnRomInfo Ddtodjr1RomDesc[] = {
	{ "dadj.03b",      0x080000, 0x87606b85, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.04b",      0x080000, 0x24d49575, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.05b",      0x080000, 0x56ce51f7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodj.key",    0x000014, 0x5414dfca, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodjr1)
STD_ROM_FN(Ddtodjr1)

static struct BurnRomInfo Ddtodjr2RomDesc[] = {
	{ "dadj.03a",      0x080000, 0x711638dc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.04a",      0x080000, 0x4869639c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadj.05a",      0x080000, 0x484c0efa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodj.key",    0x000014, 0x5414dfca, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodjr2)
STD_ROM_FN(Ddtodjr2)

static struct BurnRomInfo DdtoduRomDesc[] = {
	{ "dadu.03b",      0x080000, 0xa519905f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadu.04b",      0x080000, 0x52562d38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadu.05b",      0x080000, 0xee1cfbfe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodu.key",    0x000014, 0x7c03ec9e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodu)
STD_ROM_FN(Ddtodu)

static struct BurnRomInfo Ddtodur1RomDesc[] = {
	{ "dadu.03a",      0x080000, 0x4413f177, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadu.04a",      0x080000, 0x168de230, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadu.05a",      0x080000, 0x03d39e91, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06",        0x080000, 0x13aa3e56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.07",        0x080000, 0x431cb6dd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "ddtodu.key",    0x000014, 0x7c03ec9e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodur1)
STD_ROM_FN(Ddtodur1)

static struct BurnRomInfo DimahooRomDesc[] = {
	{ "gmde.03",       0x080000, 0x968fcecd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.04",        0x080000, 0x37485567, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.05",        0x080000, 0xda269ffb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.06",        0x080000, 0x55b483c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "gmd.13m",       0x400000, 0x80dd19f0, CPS2_GFX | BRF_GRA },
	{ "gmd.15m",       0x400000, 0xdfd93a78, CPS2_GFX | BRF_GRA },
	{ "gmd.17m",       0x400000, 0x16356520, CPS2_GFX | BRF_GRA },
	{ "gmd.19m",       0x400000, 0xdfc33031, CPS2_GFX | BRF_GRA },

	{ "gmd.01",        0x020000, 0x3f9bc985, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "gmd.02",        0x020000, 0x3fd39dde, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "gmd.11m",       0x400000, 0x06a65542, CPS2_QSND | BRF_SND },
	{ "gmd.12m",       0x400000, 0x50bc7a31, CPS2_QSND | BRF_SND },
	
	{ "dimahoo.key",   0x000014, 0x7d6d2db9, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dimahoo)
STD_ROM_FN(Dimahoo)

static struct BurnRomInfo GmdjRomDesc[] = {
	{ "gmdj.03",       0x080000, 0xcd6979e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.04",        0x080000, 0x37485567, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.05",        0x080000, 0xda269ffb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.06",        0x080000, 0x55b483c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "gmd.13m",       0x400000, 0x80dd19f0, CPS2_GFX | BRF_GRA },
	{ "gmd.15m",       0x400000, 0xdfd93a78, CPS2_GFX | BRF_GRA },
	{ "gmd.17m",       0x400000, 0x16356520, CPS2_GFX | BRF_GRA },
	{ "gmd.19m",       0x400000, 0xdfc33031, CPS2_GFX | BRF_GRA },

	{ "gmd.01",        0x020000, 0x3f9bc985, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "gmd.02",        0x020000, 0x3fd39dde, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "gmd.11m",       0x400000, 0x06a65542, CPS2_QSND | BRF_SND },
	{ "gmd.12m",       0x400000, 0x50bc7a31, CPS2_QSND | BRF_SND },
	
	{ "gmahou.key",    0x000014, 0x76a5e659, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gmdj)
STD_ROM_FN(Gmdj)

static struct BurnRomInfo DimahoouRomDesc[] = {
	{ "gmdu.03",       0x080000, 0x43bcb15f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.04",        0x080000, 0x37485567, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.05",        0x080000, 0xda269ffb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.06",        0x080000, 0x55b483c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "gmd.13m",       0x400000, 0x80dd19f0, CPS2_GFX | BRF_GRA },
	{ "gmd.15m",       0x400000, 0xdfd93a78, CPS2_GFX | BRF_GRA },
	{ "gmd.17m",       0x400000, 0x16356520, CPS2_GFX | BRF_GRA },
	{ "gmd.19m",       0x400000, 0xdfc33031, CPS2_GFX | BRF_GRA },

	{ "gmd.01",        0x020000, 0x3f9bc985, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "gmd.02",        0x020000, 0x3fd39dde, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "gmd.11m",       0x400000, 0x06a65542, CPS2_QSND | BRF_SND },
	{ "gmd.12m",       0x400000, 0x50bc7a31, CPS2_QSND | BRF_SND },
	
	{ "dimahoou.key",  0x000014, 0x8254d7ab, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dimahoou)
STD_ROM_FN(Dimahoou)

static struct BurnRomInfo DstlkRomDesc[] = {
	{ "vame.03a",      0x080000, 0x004c9cff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.04a",      0x080000, 0xae413ff2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.05a",      0x080000, 0x60678756, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.06a",      0x080000, 0x912870b3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.07a",      0x080000, 0xdabae3e8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.08a",      0x080000, 0x2c6e3077, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.09a",      0x080000, 0xf16db74b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vame.10a",      0x080000, 0x701e2147, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "dstlk.key",     0x000014, 0xcfa46dec, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlk)
STD_ROM_FN(Dstlk)

static struct BurnRomInfo DstlkaRomDesc[] = {
	{ "vama.03a",      0x080000, 0x294e0bec, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.04a",      0x080000, 0xbc18e128, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.05a",      0x080000, 0xe709fa59, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.06a",      0x080000, 0x55e4d387, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.07a",      0x080000, 0x24e8f981, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.08a",      0x080000, 0x743f3a8e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.09a",      0x080000, 0x67fa5573, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vama.10a",      0x080000, 0x5e03d747, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "dstlka.key",    0x000014, 0xd31d61bc, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlka)
STD_ROM_FN(Dstlka)

static struct BurnRomInfo DstlkhRomDesc[] = {
	{ "vamh.03c",      0x080000, 0x4d7b9e8f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.04c",      0x080000, 0x2217e9a0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.05c",      0x080000, 0x3a05b13c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.06c",      0x080000, 0x11d70a1c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.07c",      0x080000, 0xdb5a8767, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.08c",      0x080000, 0x2a4fd79b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.09c",      0x080000, 0x15187632, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamh.10c",      0x080000, 0x192d2d81, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "dstlkh.key",    0x000014, 0xd748cb77, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlkh)
STD_ROM_FN(Dstlkh)

static struct BurnRomInfo DstlkuRomDesc[] = {
	{ "vamu.03b",      0x080000, 0x68a6343f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.04b",      0x080000, 0x58161453, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.05b",      0x080000, 0xdfc038b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.06b",      0x080000, 0xc3842c89, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.07b",      0x080000, 0x25b60b6e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.08b",      0x080000, 0x2113c596, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.09b",      0x080000, 0x2d1e9ae5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.10b",      0x080000, 0x81145622, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "dstlku.key",    0x000014, 0xc76091ba, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlku)
STD_ROM_FN(Dstlku)

static struct BurnRomInfo Dstlkur1RomDesc[] = {
	{ "vamu.03a",      0x080000, 0x628899f9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.04a",      0x080000, 0x696d9b25, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.05a",      0x080000, 0x673ed50a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.06a",      0x080000, 0xf2377be7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.07a",      0x080000, 0xd8f498c4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.08a",      0x080000, 0xe6a8a1a0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.09a",      0x080000, 0x8dd55b24, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.10a",      0x080000, 0xc1a3d9be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "dstlku.key",    0x000014, 0xc76091ba, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlkur1)
STD_ROM_FN(Dstlkur1)

static struct BurnRomInfo VampjRomDesc[] = {
	{ "vamj.03a",      0x080000, 0xf55d3722, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.04b",      0x080000, 0x4d9c43c4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.05a",      0x080000, 0x6c497e92, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.06a",      0x080000, 0xf1bbecb6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.07a",      0x080000, 0x1067ad84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.08a",      0x080000, 0x4b89f41f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.09a",      0x080000, 0xfc0a4aac, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.10a",      0x080000, 0x9270c26b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "vampj.key",     0x000014, 0x8418cc6f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vampj)
STD_ROM_FN(Vampj)

static struct BurnRomInfo VampjaRomDesc[] = {
	{ "vamj.03a",      0x080000, 0xf55d3722, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.04a",      0x080000, 0xfdcbdae3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.05a",      0x080000, 0x6c497e92, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.06a",      0x080000, 0xf1bbecb6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.07a",      0x080000, 0x1067ad84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.08a",      0x080000, 0x4b89f41f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.09a",      0x080000, 0xfc0a4aac, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.10a",      0x080000, 0x9270c26b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "vampj.key",     0x000014, 0x8418cc6f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vampja)
STD_ROM_FN(Vampja)

static struct BurnRomInfo Vampjr1RomDesc[] = {
	{ "vamj.03",       0x080000, 0x8895bf77, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.04",       0x080000, 0x5027db3d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.05",       0x080000, 0x97c66fdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.06",       0x080000, 0x9b4c3426, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.07",       0x080000, 0x303bc4fd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.08",       0x080000, 0x3dea3646, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.09",       0x080000, 0xc119a827, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamj.10",       0x080000, 0x46593b79, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "vampj.key",     0x000014, 0x8418cc6f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vampjr1)
STD_ROM_FN(Vampjr1)

static struct BurnRomInfo EcofghtrRomDesc[] = {
	{ "uece.03",       0x080000, 0xec2c1137, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uece.04",       0x080000, 0xb35f99db, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uece.05",       0x080000, 0xd9d42d31, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uece.06",       0x080000, 0x9d9771cf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "ecofghtr.key",  0x000014, 0x2250fd9e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtr)
STD_ROM_FN(Ecofghtr)

static struct BurnRomInfo EcofghtraRomDesc[] = {
	{ "ueca.03",       0x080000, 0xbd4589b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueca.04",       0x080000, 0x1d134b7d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueca.05",       0x080000, 0x9c581fc7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueca.06",       0x080000, 0xc92a7c50, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "ecofghtra.key",  0x000014, 0x4f99a9f5, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtra)
STD_ROM_FN(Ecofghtra)

static struct BurnRomInfo EcofghtrhRomDesc[] = {
	{ "uech.03",       0x080000, 0x14c9365e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uech.04",       0x080000, 0x579495dc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uech.05",       0x080000, 0x96807a8e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uech.06",       0x080000, 0x682b9dbc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "ecofghtrh.key",  0x000014, 0x9a9027c8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtrh)
STD_ROM_FN(Ecofghtrh)

static struct BurnRomInfo EcofghtruRomDesc[] = {
	{ "uecu.03a",      0x080000, 0x22d88a4d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.04a",      0x080000, 0x6436cfcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.05a",      0x080000, 0x336f121b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.06a",      0x080000, 0x6f99d984, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "ecofghtru.key",  0x000014, 0x611aa137, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtru)
STD_ROM_FN(Ecofghtru)

static struct BurnRomInfo Ecofghtru1RomDesc[] = {
	{ "uecu.03",       0x080000, 0x6792480c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.04",       0x080000, 0x95ce69d5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.05",       0x080000, 0x3a1e78ad, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecu.06",       0x080000, 0xa3e2f3cc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "ecofghtru.key",  0x000014, 0x611aa137, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtru1)
STD_ROM_FN(Ecofghtru1)

static struct BurnRomInfo UecologyRomDesc[] = {
	{ "uecj.03",       0x080000, 0x94c40a4c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecj.04",       0x080000, 0x8d6e3a09, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecj.05",       0x080000, 0x8604ecd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "uecj.06",       0x080000, 0xb7e1d31f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "uecology.key",  0x000014, 0x0bab792d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Uecology)
STD_ROM_FN(Uecology)

static struct BurnRomInfo GigawingRomDesc[] = {
	{ "ggwu.03",       0x080000, 0xac725eb2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwu.04",       0x080000, 0x392f4118, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggw.05",        0x080000, 0x3239d642, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "gigawing.key",  0x000014, 0x5076c26b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawing)
STD_ROM_FN(Gigawing)

static struct BurnRomInfo GigawingaRomDesc[] = {
	{ "ggwa.03a",      0x080000, 0x116f8837, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwa.04a",      0x080000, 0xe6e3f0c4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwa.05a",      0x080000, 0x465e8ac9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "gigawinga.key", 0x000014, 0x7401627e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawinga)
STD_ROM_FN(Gigawinga)

static struct BurnRomInfo GigawingbRomDesc[] = {
	{ "ggwb.03",       0x080000, 0xa1f8a448, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwb.04",       0x080000, 0x6a423e76, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggw.05",        0x080000, 0x3239d642, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "gigawingb.key", 0x000014, 0x5e7805fa, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawingb)
STD_ROM_FN(Gigawingb)

static struct BurnRomInfo GigawinghRomDesc[] = {
	{ "ggwh.03a",      0x080000, 0xb9ee36eb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwh.04a",      0x080000, 0x72e548fe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggw.05",        0x080000, 0x3239d642, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "gigawingh.key", 0x000014, 0x43198223, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawingh)
STD_ROM_FN(Gigawingh)

static struct BurnRomInfo GigawingjRomDesc[] = {
	{ "ggwj.03a",      0x080000, 0xfdd23b91, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwj.04a",      0x080000, 0x8c6e093c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwj.05a",      0x080000, 0x43811454, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "gigawingj.key", 0x000014, 0x8121a25e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawingj)
STD_ROM_FN(Gigawingj)

static struct BurnRomInfo Hsf2RomDesc[] = {
	{ "hs2u.03",       0x080000, 0xb308151e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2u.04",       0x080000, 0x327aa49c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.05",        0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.06",        0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.07",        0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.08",        0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.09",        0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.10",        0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13m",       0x800000, 0xa6ecab17, CPS2_GFX | BRF_GRA },
	{ "hs2.15m",       0x800000, 0x10a0ae4d, CPS2_GFX | BRF_GRA },
	{ "hs2.17m",       0x800000, 0xadfa7726, CPS2_GFX | BRF_GRA },
	{ "hs2.19m",       0x800000, 0xbb3ae322, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2.11m",       0x800000, 0x0e15c359, CPS2_QSND | BRF_SND },
	
	{ "hsf2.key",      0x000014, 0xfc9b18c9, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2)
STD_ROM_FN(Hsf2)

static struct BurnRomInfo Hsf2aRomDesc[] = {
	{ "hs2a.03",       0x080000, 0xd50a17e0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2a.04",       0x080000, 0xa27f42de, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.05",        0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.06",        0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.07",        0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.08",        0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.09",        0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.10",        0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13m",       0x800000, 0xa6ecab17, CPS2_GFX | BRF_GRA },
	{ "hs2.15m",       0x800000, 0x10a0ae4d, CPS2_GFX | BRF_GRA },
	{ "hs2.17m",       0x800000, 0xadfa7726, CPS2_GFX | BRF_GRA },
	{ "hs2.19m",       0x800000, 0xbb3ae322, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2.11m",       0x800000, 0x0e15c359, CPS2_QSND | BRF_SND },
	
	{ "hsf2a.key",     0x000014, 0x2cd9eb99, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2a)
STD_ROM_FN(Hsf2a)

static struct BurnRomInfo Hsf2jRomDesc[] = {
	{ "hs2j.03c",      0x080000, 0x6efe661f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.04b",      0x080000, 0x93f2500a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.05",       0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.06",       0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.07",       0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.08",       0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.09",       0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.10",       0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13m" ,      0x800000, 0xa6ecab17, CPS2_GFX | BRF_GRA },
	{ "hs2.15m",       0x800000, 0x10a0ae4d, CPS2_GFX | BRF_GRA },
	{ "hs2.17m",       0x800000, 0xadfa7726, CPS2_GFX | BRF_GRA },
	{ "hs2.19m",       0x800000, 0xbb3ae322, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2.11m",       0x800000, 0x0e15c359, CPS2_QSND | BRF_SND },
	
	{ "hsf2j.key",     0x000014, 0x19455a93, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2j)
STD_ROM_FN(Hsf2j)

static struct BurnRomInfo Hsf2j1RomDesc[] = {
	{ "hs2j.03",       0x080000, 0x00738f73, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2j.04",       0x080000, 0x40072c4a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.05",        0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.06",        0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.07",        0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.08",        0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.09",        0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.10",        0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13m" ,      0x800000, 0xa6ecab17, CPS2_GFX | BRF_GRA },
	{ "hs2.15m",       0x800000, 0x10a0ae4d, CPS2_GFX | BRF_GRA },
	{ "hs2.17m",       0x800000, 0xadfa7726, CPS2_GFX | BRF_GRA },
	{ "hs2.19m",       0x800000, 0xbb3ae322, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2.11m",       0x800000, 0x0e15c359, CPS2_QSND | BRF_SND },
	
	{ "hsf2j.key",     0x000014, 0x19455a93, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2j1)
STD_ROM_FN(Hsf2j1)

static struct BurnRomInfo JyangokuRomDesc[] = {
	{ "majj.03",       0x080000, 0x4614a3b2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "maj1_d.simm1",  0x200000, 0xba0fe27b, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj1_c.simm1",  0x200000, 0x2cd141bf, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj1_b.simm1",  0x200000, 0xe29e4c26, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj1_a.simm1",  0x200000, 0x7f68b88a, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj3_d.simm3",  0x200000, 0x3aaeb90b, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj3_c.simm3",  0x200000, 0x97894cea, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj3_b.simm3",  0x200000, 0xec737d9d, CPS2_GFX_SIMM | BRF_GRA },
	{ "maj3_a.simm3",  0x200000, 0xc23b6f22, CPS2_GFX_SIMM | BRF_GRA },
	
	{ "maj.01",        0x020000, 0x1fe8c213, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "maj5_a.simm5",  0x200000, 0x5ad9ee53, CPS2_QSND_SIMM_BYTESWAP | BRF_SND },	
	{ "maj5_b.simm5",  0x200000, 0xefb3dbfb, CPS2_QSND_SIMM_BYTESWAP | BRF_SND },
	
	{ "jyangoku.key",  0x000014, 0x95b0a560, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Jyangoku)
STD_ROM_FN(Jyangoku)

static struct BurnRomInfo Megaman2RomDesc[] = {
	{ "rm2u.03",       0x080000, 0x8ffc2cd1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2u.04",       0x080000, 0xbb30083a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2.05",        0x080000, 0x02ee9efc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rm2.14m",       0x200000, 0x9b1f00b4, CPS2_GFX | BRF_GRA },
	{ "rm2.16m",       0x200000, 0xc2bb0c24, CPS2_GFX | BRF_GRA },
	{ "rm2.18m",       0x200000, 0x12257251, CPS2_GFX | BRF_GRA },
	{ "rm2.20m",       0x200000, 0xf9b6e786, CPS2_GFX | BRF_GRA },

	{ "rm2.01a",       0x020000, 0xd18e7859, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "rm2.02",        0x020000, 0xc463ece0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rm2.11m",       0x200000, 0x2106174d, CPS2_QSND | BRF_SND },
	{ "rm2.12m",       0x200000, 0x546c1636, CPS2_QSND | BRF_SND },
	
	{ "megaman2.key",  0x000014, 0x6828ed6d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Megaman2)
STD_ROM_FN(Megaman2)

static struct BurnRomInfo Megaman2aRomDesc[] = {
	{ "rm2a.03",       0x080000, 0x2b330ca7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2a.04",       0x080000, 0x8b47942b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2.05",        0x080000, 0x02ee9efc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rm2.14m",       0x200000, 0x9b1f00b4, CPS2_GFX | BRF_GRA },
	{ "rm2.16m",       0x200000, 0xc2bb0c24, CPS2_GFX | BRF_GRA },
	{ "rm2.18m",       0x200000, 0x12257251, CPS2_GFX | BRF_GRA },
	{ "rm2.20m",       0x200000, 0xf9b6e786, CPS2_GFX | BRF_GRA },

	{ "rm2.01a",       0x020000, 0xd18e7859, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "rm2.02",        0x020000, 0xc463ece0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rm2.11m",       0x200000, 0x2106174d, CPS2_QSND | BRF_SND },
	{ "rm2.12m",       0x200000, 0x546c1636, CPS2_QSND | BRF_SND },
	
	{ "megaman2a.key", 0x000014, 0xd6e8dcd7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Megaman2a)
STD_ROM_FN(Megaman2a)

static struct BurnRomInfo Megaman2hRomDesc[] = {
	{ "rm2h.03",       0x080000, 0xbb180378, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2h.04",       0x080000, 0x205ffcd6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2.05",        0x080000, 0x02ee9efc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rm2.14m",       0x200000, 0x9b1f00b4, CPS2_GFX | BRF_GRA },
	{ "rm2.16m",       0x200000, 0xc2bb0c24, CPS2_GFX | BRF_GRA },
	{ "rm2.18m",       0x200000, 0x12257251, CPS2_GFX | BRF_GRA },
	{ "rm2.20m",       0x200000, 0xf9b6e786, CPS2_GFX | BRF_GRA },

	{ "rm2.01a",       0x020000, 0xd18e7859, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "rm2.02",        0x020000, 0xc463ece0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rm2.11m",       0x200000, 0x2106174d, CPS2_QSND | BRF_SND },
	{ "rm2.12m",       0x200000, 0x546c1636, CPS2_QSND | BRF_SND },
	
	{ "megaman2h.key", 0x000014, 0x99cb8d19, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Megaman2h)
STD_ROM_FN(Megaman2h)

static struct BurnRomInfo Rockman2jRomDesc[] = {
	{ "rm2j.03",       0x080000, 0xdbaa1437, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2j.04",       0x080000, 0xcf5ba612, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2.05",        0x080000, 0x02ee9efc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rm2.14m",       0x200000, 0x9b1f00b4, CPS2_GFX | BRF_GRA },
	{ "rm2.16m",       0x200000, 0xc2bb0c24, CPS2_GFX | BRF_GRA },
	{ "rm2.18m",       0x200000, 0x12257251, CPS2_GFX | BRF_GRA },
	{ "rm2.20m",       0x200000, 0xf9b6e786, CPS2_GFX | BRF_GRA },

	{ "rm2.01a",       0x020000, 0xd18e7859, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "rm2.02",        0x020000, 0xc463ece0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rm2.11m",       0x200000, 0x2106174d, CPS2_QSND | BRF_SND },
	{ "rm2.12m",       0x200000, 0x546c1636, CPS2_QSND | BRF_SND },
	
	{ "rockman2j.key", 0x000014, 0xc590187a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Rockman2j)
STD_ROM_FN(Rockman2j)

static struct BurnRomInfo Mmancp2uRomDesc[] = {
	{ "rcmu.03b",      0x080000, 0xc39f037f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcmu.04b",      0x080000, 0xcd6f5e99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcm.05b",       0x080000, 0x4376ea95, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rcm.73",        0x080000, 0x774c6e04, CPS2_GFX | BRF_GRA },
	{ "rcm.63",        0x080000, 0xacad7c62, CPS2_GFX | BRF_GRA },
	{ "rcm.83",        0x080000, 0x6af30499, CPS2_GFX | BRF_GRA },
	{ "rcm.93",        0x080000, 0x7a5a5166, CPS2_GFX | BRF_GRA },
	{ "rcm.74",        0x080000, 0x004ec725, CPS2_GFX | BRF_GRA },
	{ "rcm.64",        0x080000, 0x65c0464e, CPS2_GFX | BRF_GRA },
	{ "rcm.84",        0x080000, 0xfb3097cc, CPS2_GFX | BRF_GRA },
	{ "rcm.94",        0x080000, 0x2e16557a, CPS2_GFX | BRF_GRA },
	{ "rcm.75",        0x080000, 0x70a73f99, CPS2_GFX | BRF_GRA },
	{ "rcm.65",        0x080000, 0xecedad3d, CPS2_GFX | BRF_GRA },
	{ "rcm.85",        0x080000, 0x3d6186d8, CPS2_GFX | BRF_GRA },
	{ "rcm.95",        0x080000, 0x8c7700f1, CPS2_GFX | BRF_GRA },
	{ "rcm.76",        0x080000, 0x89a889ad, CPS2_GFX | BRF_GRA },
	{ "rcm.66",        0x080000, 0x1300eb7b, CPS2_GFX | BRF_GRA },
	{ "rcm.86",        0x080000, 0x6d974ebd, CPS2_GFX | BRF_GRA },
	{ "rcm.96",        0x080000, 0x7da4cd24, CPS2_GFX | BRF_GRA },

	{ "rcm.01",        0x020000, 0xd60cf8a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rcm.51",        0x080000, 0xb6d07080, CPS2_QSND | BRF_SND },
	{ "rcm.52",        0x080000, 0xdfddc493, CPS2_QSND | BRF_SND },
	{ "rcm.53",        0x080000, 0x6062ae3a, CPS2_QSND | BRF_SND },
	{ "rcm.54",        0x080000, 0x08c6f3bf, CPS2_QSND | BRF_SND },
	{ "rcm.55",        0x080000, 0xf97dfccc, CPS2_QSND | BRF_SND },
	{ "rcm.56",        0x080000, 0xade475bc, CPS2_QSND | BRF_SND },
	{ "rcm.57",        0x080000, 0x075effb3, CPS2_QSND | BRF_SND },
	{ "rcm.58",        0x080000, 0xf6c1f87b, CPS2_QSND | BRF_SND },
	
	{ "mmancp2u.key",  0x000014, 0x17ca6659, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mmancp2u)
STD_ROM_FN(Mmancp2u)

static struct BurnRomInfo Mmancp2ur1RomDesc[] = {
	{ "rcmu.03a",      0x080000, 0xc6b75320, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcmu.04a",      0x080000, 0x47880111, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcmu.05a",      0x080000, 0x4376ea95, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rcm.73",        0x080000, 0x774c6e04, CPS2_GFX | BRF_GRA },
	{ "rcm.63",        0x080000, 0xacad7c62, CPS2_GFX | BRF_GRA },
	{ "rcm.83",        0x080000, 0x6af30499, CPS2_GFX | BRF_GRA },
	{ "rcm.93",        0x080000, 0x7a5a5166, CPS2_GFX | BRF_GRA },
	{ "rcm.74",        0x080000, 0x004ec725, CPS2_GFX | BRF_GRA },
	{ "rcm.64",        0x080000, 0x65c0464e, CPS2_GFX | BRF_GRA },
	{ "rcm.84",        0x080000, 0xfb3097cc, CPS2_GFX | BRF_GRA },
	{ "rcm.94",        0x080000, 0x2e16557a, CPS2_GFX | BRF_GRA },
	{ "rcm.75",        0x080000, 0x70a73f99, CPS2_GFX | BRF_GRA },
	{ "rcm.65",        0x080000, 0xecedad3d, CPS2_GFX | BRF_GRA },
	{ "rcm.85",        0x080000, 0x3d6186d8, CPS2_GFX | BRF_GRA },
	{ "rcm.95",        0x080000, 0x8c7700f1, CPS2_GFX | BRF_GRA },
	{ "rcm.76",        0x080000, 0x89a889ad, CPS2_GFX | BRF_GRA },
	{ "rcm.66",        0x080000, 0x1300eb7b, CPS2_GFX | BRF_GRA },
	{ "rcm.86",        0x080000, 0x6d974ebd, CPS2_GFX | BRF_GRA },
	{ "rcm.96",        0x080000, 0x7da4cd24, CPS2_GFX | BRF_GRA },

	{ "rcm.01",        0x020000, 0xd60cf8a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rcm.51",        0x080000, 0xb6d07080, CPS2_QSND | BRF_SND },
	{ "rcm.52",        0x080000, 0xdfddc493, CPS2_QSND | BRF_SND },
	{ "rcm.53",        0x080000, 0x6062ae3a, CPS2_QSND | BRF_SND },
	{ "rcm.54",        0x080000, 0x08c6f3bf, CPS2_QSND | BRF_SND },
	{ "rcm.55",        0x080000, 0xf97dfccc, CPS2_QSND | BRF_SND },
	{ "rcm.56",        0x080000, 0xade475bc, CPS2_QSND | BRF_SND },
	{ "rcm.57",        0x080000, 0x075effb3, CPS2_QSND | BRF_SND },
	{ "rcm.58",        0x080000, 0xf6c1f87b, CPS2_QSND | BRF_SND },
	
	{ "mmancp2u.key",  0x000014, 0x17ca6659, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mmancp2ur1)
STD_ROM_FN(Mmancp2ur1)

static struct BurnRomInfo Rmancp2jRomDesc[] = {
	{ "rcmj.03a",      0x080000, 0x30559f60, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcmj.04a",      0x080000, 0x5efc9366, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rcm.05a",       0x080000, 0x517ccde2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rcm.73",        0x080000, 0x774c6e04, CPS2_GFX | BRF_GRA },
	{ "rcm.63",        0x080000, 0xacad7c62, CPS2_GFX | BRF_GRA },
	{ "rcm.83",        0x080000, 0x6af30499, CPS2_GFX | BRF_GRA },
	{ "rcm.93",        0x080000, 0x7a5a5166, CPS2_GFX | BRF_GRA },
	{ "rcm.74",        0x080000, 0x004ec725, CPS2_GFX | BRF_GRA },
	{ "rcm.64",        0x080000, 0x65c0464e, CPS2_GFX | BRF_GRA },
	{ "rcm.84",        0x080000, 0xfb3097cc, CPS2_GFX | BRF_GRA },
	{ "rcm.94",        0x080000, 0x2e16557a, CPS2_GFX | BRF_GRA },
	{ "rcm.75",        0x080000, 0x70a73f99, CPS2_GFX | BRF_GRA },
	{ "rcm.65",        0x080000, 0xecedad3d, CPS2_GFX | BRF_GRA },
	{ "rcm.85",        0x080000, 0x3d6186d8, CPS2_GFX | BRF_GRA },
	{ "rcm.95",        0x080000, 0x8c7700f1, CPS2_GFX | BRF_GRA },
	{ "rcm.76",        0x080000, 0x89a889ad, CPS2_GFX | BRF_GRA },
	{ "rcm.66",        0x080000, 0x1300eb7b, CPS2_GFX | BRF_GRA },
	{ "rcm.86",        0x080000, 0x6d974ebd, CPS2_GFX | BRF_GRA },
	{ "rcm.96",        0x080000, 0x7da4cd24, CPS2_GFX | BRF_GRA },

	{ "rcm.01",        0x020000, 0xd60cf8a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rcm.51",        0x080000, 0xb6d07080, CPS2_QSND | BRF_SND },
	{ "rcm.52",        0x080000, 0xdfddc493, CPS2_QSND | BRF_SND },
	{ "rcm.53",        0x080000, 0x6062ae3a, CPS2_QSND | BRF_SND },
	{ "rcm.54",        0x080000, 0x08c6f3bf, CPS2_QSND | BRF_SND },
	{ "rcm.55",        0x080000, 0xf97dfccc, CPS2_QSND | BRF_SND },
	{ "rcm.56",        0x080000, 0xade475bc, CPS2_QSND | BRF_SND },
	{ "rcm.57",        0x080000, 0x075effb3, CPS2_QSND | BRF_SND },
	{ "rcm.58",        0x080000, 0xf6c1f87b, CPS2_QSND | BRF_SND },
	
	{ "rmancp2j.key",  0x000014, 0x17309a70, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Rmancp2j)
STD_ROM_FN(Rmancp2j)

static struct BurnRomInfo MmatrixRomDesc[] = {
	{ "mmxu.03",       0x080000, 0xAB65b599, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxu.04",       0x080000, 0x0135FC6C, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxu.05",       0x080000, 0xF1FD2B84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mmx.13m",       0x400000, 0x04748718, CPS2_GFX | BRF_GRA },
	{ "mmx.15m",       0x400000, 0x38074F44, CPS2_GFX | BRF_GRA },
	{ "mmx.17m",       0x400000, 0xE4635E35, CPS2_GFX | BRF_GRA },
	{ "mmx.19m",       0x400000, 0x4400A3F2, CPS2_GFX | BRF_GRA },
	{ "mmx.14m",       0x400000, 0xD52BF491, CPS2_GFX | BRF_GRA },
	{ "mmx.16m",       0x400000, 0x23F70780, CPS2_GFX | BRF_GRA },
	{ "mmx.18m",       0x400000, 0x2562C9D5, CPS2_GFX | BRF_GRA },
	{ "mmx.20m",       0x400000, 0x583A9687, CPS2_GFX | BRF_GRA },

	{ "mmx.01",        0x020000, 0xC57E8171, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mmx.11m",       0x400000, 0x4180B39F, CPS2_QSND | BRF_SND },
	{ "mmx.12m",       0x400000, 0x95E22A59, CPS2_QSND | BRF_SND },
	
	{ "mmatrix.key",   0x000014, 0x8ed66bc4, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mmatrix)
STD_ROM_FN(Mmatrix)

static struct BurnRomInfo MmatrixjRomDesc[] = {
	{ "mmxj.03",       0x080000, 0x1D5DE213, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxj.04",       0x080000, 0xD943A339, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxj.05",       0x080000, 0x0C8B4ABB, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mmx.13m",       0x400000, 0x04748718, CPS2_GFX | BRF_GRA },
	{ "mmx.15m",       0x400000, 0x38074F44, CPS2_GFX | BRF_GRA },
	{ "mmx.17m",       0x400000, 0xE4635E35, CPS2_GFX | BRF_GRA },
	{ "mmx.19m",       0x400000, 0x4400A3F2, CPS2_GFX | BRF_GRA },
	{ "mmx.14m",       0x400000, 0xD52BF491, CPS2_GFX | BRF_GRA },
	{ "mmx.16m",       0x400000, 0x23F70780, CPS2_GFX | BRF_GRA },
	{ "mmx.18m",       0x400000, 0x2562C9D5, CPS2_GFX | BRF_GRA },
	{ "mmx.20m",       0x400000, 0x583A9687, CPS2_GFX | BRF_GRA },

	{ "mmx.01",        0x020000, 0xC57E8171, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mmx.11m",       0x400000, 0x4180B39F, CPS2_QSND | BRF_SND },
	{ "mmx.12m",       0x400000, 0x95E22A59, CPS2_QSND | BRF_SND },
	
	{ "mmatrixj.key",  0x000014, 0x3b50d889, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mmatrixj)
STD_ROM_FN(Mmatrixj)

static struct BurnRomInfo MpangRomDesc[] = {
	{ "mpne.03c",      0x080000, 0xfe16fc9f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mpne.04c",      0x080000, 0x2cc5ec22, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mpn-simm.01c",  0x200000, 0x388DB66B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01d",  0x200000, 0xAFF1B494, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01a",  0x200000, 0xA9C4857B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01b",  0x200000, 0xF759DF22, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03c",  0x200000, 0xDEC6B720, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03d",  0x200000, 0xF8774C18, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03a",  0x200000, 0xC2AEA4EC, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03b",  0x200000, 0x84D6DC33, CPS2_GFX_SIMM | BRF_GRA },

	{ "mpn.01",        0x020000, 0x90C7ADB6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mpn-simm.05a",  0x200000, 0x318A2E21, CPS2_QSND_SIMM | BRF_SND },
	{ "mpn-simm.05b",  0x200000, 0x5462F4E8, CPS2_QSND_SIMM | BRF_SND },
	
	{ "mpang.key",     0x000014, 0x95354b0f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mpang)
STD_ROM_FN(Mpang)

static struct BurnRomInfo Mpangr1RomDesc[] = {
	{ "mpne.03b",      0x080000, 0x6ef0f9b2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mpne.04b",      0x080000, 0x30a468bb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mpn-simm.01c",  0x200000, 0x388DB66B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01d",  0x200000, 0xAFF1B494, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01a",  0x200000, 0xA9C4857B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01b",  0x200000, 0xF759DF22, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03c",  0x200000, 0xDEC6B720, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03d",  0x200000, 0xF8774C18, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03a",  0x200000, 0xC2AEA4EC, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03b",  0x200000, 0x84D6DC33, CPS2_GFX_SIMM | BRF_GRA },

	{ "mpn.01",        0x020000, 0x90C7ADB6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mpn-simm.05a",  0x200000, 0x318A2E21, CPS2_QSND_SIMM | BRF_SND },
	{ "mpn-simm.05b",  0x200000, 0x5462F4E8, CPS2_QSND_SIMM | BRF_SND },
	
	{ "mpang.key",     0x000014, 0x95354b0f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mpangr1)
STD_ROM_FN(Mpangr1)

static struct BurnRomInfo MpanguRomDesc[] = {
	{ "mpnu.03",       0x080000, 0x6e7ed03c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mpnu.04",       0x080000, 0xde079131, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mpn.13m",       0x200000, 0xc5f123dc, CPS2_GFX | BRF_GRA },
	{ "mpn.15m",       0x200000, 0x8e033265, CPS2_GFX | BRF_GRA },
	{ "mpn.17m",       0x200000, 0xcfcd73d2, CPS2_GFX | BRF_GRA },
	{ "mpn.19m",       0x200000, 0x2db1ffbc, CPS2_GFX | BRF_GRA },
	
	{ "mpn.01",        0x020000, 0x90C7ADB6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mpn.q1",        0x100000, 0xd21c1f5a, CPS2_QSND | BRF_SND },	
	{ "mpn.q2",        0x100000, 0xd22090b1, CPS2_QSND | BRF_SND },	
	{ "mpn.q3",        0x100000, 0x60aa5ef2, CPS2_QSND | BRF_SND },	
	{ "mpn.q4",        0x100000, 0x3a67d203, CPS2_QSND | BRF_SND },
	
	{ "mpang.key",     0x000014, 0x95354b0f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mpangu)
STD_ROM_FN(Mpangu)

static struct BurnRomInfo MpangjRomDesc[] = {
	{ "mpnj.03a",      0x080000, 0xBF597b1C, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mpnj.04a",      0x080000, 0xF4A3AB0F, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mpn-simm.01c",  0x200000, 0x388DB66B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01d",  0x200000, 0xAFF1B494, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01a",  0x200000, 0xA9C4857B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01b",  0x200000, 0xF759DF22, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03c",  0x200000, 0xDEC6B720, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03d",  0x200000, 0xF8774C18, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03a",  0x200000, 0xC2AEA4EC, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03b",  0x200000, 0x84D6DC33, CPS2_GFX_SIMM | BRF_GRA },

	{ "mpn.01",        0x020000, 0x90C7ADB6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mpn-simm.05a",  0x200000, 0x318A2E21, CPS2_QSND_SIMM | BRF_SND },
	{ "mpn-simm.05b",  0x200000, 0x5462F4E8, CPS2_QSND_SIMM | BRF_SND },
	
	{ "mpang.key",     0x000014, 0x95354b0f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mpangj)
STD_ROM_FN(Mpangj)

static struct BurnRomInfo MshRomDesc[] = {
	{ "mshe.03e",      0x080000, 0xbd951414, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshe.04e",      0x080000, 0x19dd42f2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05",        0x080000, 0x6a091b9e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "msh.key",       0x000014, 0xb494368e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Msh)
STD_ROM_FN(Msh)

static struct BurnRomInfo MshaRomDesc[] = {
	{ "msha.03e",      0x080000, 0xec84ec44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msha.04e",      0x080000, 0x098b8503, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05",        0x080000, 0x6a091b9e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "msha.key",      0x000014, 0x00f3f2ca, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Msha)
STD_ROM_FN(Msha)

static struct BurnRomInfo MshbRomDesc[] = {
	{ "mshb.03c",      0x080000, 0x19697f74, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshb.04c",      0x080000, 0x95317a6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05a",       0x080000, 0xf37539e6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },


	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "mshb.key",      0x000014, 0x92196837, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshb)
STD_ROM_FN(Mshb)

static struct BurnRomInfo MshhRomDesc[] = {
	{ "mshh.03c",      0x080000, 0x8d84b0fa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshh.04c",      0x080000, 0xd638f601, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05a",       0x080000, 0xf37539e6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "mshh.key",      0x000014, 0x5dddf5e7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshh)
STD_ROM_FN(Mshh)

static struct BurnRomInfo MshjRomDesc[] = {
	{ "mshj.03g",      0x080000, 0x261f4091, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshj.04g",      0x080000, 0x61d791c6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05a",       0x080000, 0xf37539e6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "mshj.key",      0x000014, 0x888761ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshj)
STD_ROM_FN(Mshj)

static struct BurnRomInfo Mshjr1RomDesc[] = {
	{ "mshj.03f",      0x080000, 0xff172fd2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshj.04f",      0x080000, 0xebbb205a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05",        0x080000, 0x6a091b9e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "mshj.key",      0x000014, 0x888761ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshjr1)
STD_ROM_FN(Mshjr1)

static struct BurnRomInfo MshuRomDesc[] = {
	{ "mshu.03",       0x080000, 0xd2805bdd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshu.04",       0x080000, 0x743f96ff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.05",        0x080000, 0x6a091b9e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "mshu.key",      0x000014, 0x745c1bee, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshu)
STD_ROM_FN(Mshu)

static struct BurnRomInfo MshvsfRomDesc[] = {
	{ "mvse.03f",      0x080000, 0xb72dc199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvse.04f",      0x080000, 0x6ef799f9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsf.key",    0x000014, 0x64660867, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsf)
STD_ROM_FN(Mshvsf)

static struct BurnRomInfo MshvsfhRomDesc[] = {
	{ "mvsh.03f",      0x080000, 0x4f60f41e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsh.04f",      0x080000, 0xdc08ec12, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfh.key",   0x000014, 0xb93d576f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfh)
STD_ROM_FN(Mshvsfh)

static struct BurnRomInfo MshvsfaRomDesc[] = {
	{ "mvsa.03f",      0x080000, 0x5b863716, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsa.04f",      0x080000, 0x4886e65f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfa.key",   0x000014, 0x6810a3af, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfa)
STD_ROM_FN(Mshvsfa)

static struct BurnRomInfo Mshvsfa1RomDesc[] = {
	{ "mvsa.03",       0x080000, 0x92ef1933, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsa.04",       0x080000, 0x4b24373c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05",        0x080000, 0xac180c1c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfa.key",   0x000014, 0x6810a3af, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfa1)
STD_ROM_FN(Mshvsfa1)

static struct BurnRomInfo MshvsfbRomDesc[] = {
	{ "mvsb.03g",      0x080000, 0x143895ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsb.04g",      0x080000, 0xdd8a886c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05d",       0x080000, 0x921fc542, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfb.key",   0x000014, 0x3f5bb6e4, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfb)
STD_ROM_FN(Mshvsfb)

static struct BurnRomInfo Mshvsfb1RomDesc[] = {
	{ "mvsb.03f",      0x080000, 0x9c4bb950, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsb.04f",      0x080000, 0xd3320d13, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfb.key",   0x000014, 0x3f5bb6e4, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfb1)
STD_ROM_FN(Mshvsfb1)

static struct BurnRomInfo MshvsfjRomDesc[] = {
	{ "mvsj.03i",      0x080000, 0xd8cbb691, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsj.04i",      0x080000, 0x32741ace, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05h",       0x080000, 0x77870dc3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfj.key",   0x000014, 0x565eeebb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfj)
STD_ROM_FN(Mshvsfj)

static struct BurnRomInfo Mshvsfj1RomDesc[] = {
	{ "mvsj.03h",      0x080000, 0xfbe2115f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsj.04h",      0x080000, 0xb528a367, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05g",       0x080000, 0x9515a245, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfj.key",   0x000014, 0x565eeebb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfj1)
STD_ROM_FN(Mshvsfj1)

static struct BurnRomInfo Mshvsfj2RomDesc[] = {
	{ "mvsj.03g",      0x080000, 0xfdfa7e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsj.04g",      0x080000, 0xc921825f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfj.key",   0x000014, 0x565eeebb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfj2)
STD_ROM_FN(Mshvsfj2)

static struct BurnRomInfo MshvsfuRomDesc[] = {
	{ "mvsu.03g",      0x080000, 0x0664ab15, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsu.04g",      0x080000, 0x97e060ee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05d",       0x080000, 0x921fc542, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfu.key",   0x000014, 0x4c04797b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfu)
STD_ROM_FN(Mshvsfu)

static struct BurnRomInfo Mshvsfu1RomDesc[] = {
	{ "mvsu.03d",      0x080000, 0xae60a66a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsu.04d",      0x080000, 0x91f67d8a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.05a",       0x080000, 0x1a5de0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "mshvsfu.key",   0x000014, 0x4c04797b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfu1)
STD_ROM_FN(Mshvsfu1)

static struct BurnRomInfo MvscRomDesc[] = {
	{ "mvce.03a",      0x080000, 0x824e4a90, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvce.04a",      0x080000, 0x436c5a4e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvsc.key",      0x000014, 0x7e101e09, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvsc)
STD_ROM_FN(Mvsc)

static struct BurnRomInfo Mvscr1RomDesc[] = {
	{ "mvce.03",       0x080000, 0xe0633fc0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvce.04",       0x080000, 0xa450a251, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05",        0x080000, 0x7db71ce9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06",        0x080000, 0x4b0b6d3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvsc.key",      0x000014, 0x7e101e09, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscr1)
STD_ROM_FN(Mvscr1)

static struct BurnRomInfo MvscaRomDesc[] = {
	{ "mvca.03a",      0x080000, 0x2ff4ae25, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvca.04a",      0x080000, 0xf28427ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvsca.key",     0x000014, 0x31edaee8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvsca)
STD_ROM_FN(Mvsca)

static struct BurnRomInfo Mvscar1RomDesc[] = {
	{ "mvca.03",       0x080000, 0xfe5fa7b9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvca.04",       0x080000, 0x082b701c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05",        0x080000, 0x7db71ce9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06",        0x080000, 0x4b0b6d3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvsca.key",     0x000014, 0x31edaee8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscar1)
STD_ROM_FN(Mvscar1)

static struct BurnRomInfo MvscbRomDesc[] = {
	{ "mvcb.03a",      0x080000, 0x7155953b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcb.04a",      0x080000, 0xfb117d0e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvscb.key",     0x000014, 0xd74a7a3d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscb)
STD_ROM_FN(Mvscb)

static struct BurnRomInfo MvschRomDesc[] = {
	{ "mvch.03",       0x080000, 0x6a0ec9f7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvch.04",       0x080000, 0x00f03fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvsch.key",     0x000014, 0xdd647c0d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvsch)
STD_ROM_FN(Mvsch)

static struct BurnRomInfo MvscjRomDesc[] = {
	{ "mvcj.03a",      0x080000, 0x3df18879, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcj.04a",      0x080000, 0x07d212e8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvscj.key",     0x000014, 0x9dedbcaf, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscj)
STD_ROM_FN(Mvscj)

static struct BurnRomInfo Mvscjr1RomDesc[] = {
	{ "mvcj.03",       0x080000, 0x2164213f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcj.04",       0x080000, 0xc905c86f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05",        0x080000, 0x7db71ce9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06",        0x080000, 0x4b0b6d3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvscj.key",     0x000014, 0x9dedbcaf, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscjr1)
STD_ROM_FN(Mvscjr1)

static struct BurnRomInfo MvscuRomDesc[] = {
	{ "mvcu.03d",      0x080000, 0xc6007557, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.04d",      0x080000, 0x724b2b20, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.07",        0x080000, 0xc3baa32b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvscu.key",     0x000014, 0xa83db333, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscu)
STD_ROM_FN(Mvscu)

static struct BurnRomInfo Mvscur1RomDesc[] = {
	{ "mvcu.03",       0x080000, 0x13f2be57, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.04",       0x080000, 0x5e9b380d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.05",       0x080000, 0x12f321be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.06",       0x080000, 0x2f1524bc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.07",       0x080000, 0x5fdecadb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcu.10",       0x080000, 0x4f36cd63, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "mvscu.key",     0x000014, 0xa83db333, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscur1)
STD_ROM_FN(Mvscur1)

static struct BurnRomInfo MvscjsingRomDesc[] = {
	{ "mvc_ja.simm1",  0x200000, 0x6a2ef7c2, CPS2_PRG_68K_SIMM | BRF_ESS | BRF_PRG },
	{ "mvc_ja.simm3",  0x200000, 0x699d09ad, CPS2_PRG_68K_SIMM | BRF_ESS | BRF_PRG },

	{ "mvc64-13m.13",  0x800000, 0x8428ce69, CPS2_GFX | BRF_GRA },
	{ "mvc64-15m.15",  0x800000, 0x2e0028f4, CPS2_GFX | BRF_GRA },
	{ "mvc64-17m.17",  0x800000, 0x308ca826, CPS2_GFX | BRF_GRA },
	{ "mvc64-19m.19",  0x800000, 0x10699fe1, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc64-11m.11",  0x800000, 0x5d8819e0, CPS2_QSND | BRF_SND },
	
	{ "mvscj.key",     0x000014, 0x9dedbcaf, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscjsing)
STD_ROM_FN(Mvscjsing)

static struct BurnRomInfo NwarrRomDesc[] = {
	{ "vphe.03f",      0x080000, 0xa922c44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.04c",      0x080000, 0x7312d890, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.05d",      0x080000, 0xcde8b506, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.06c",      0x080000, 0xbe99e7d0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.07b",      0x080000, 0x69e0e60c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.08b",      0x080000, 0xd95a3849, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.09b",      0x080000, 0x9882561c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphe.10b",      0x080000, 0x976fa62f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "nwarr.key",     0x000014, 0x618a13ca, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarr)
STD_ROM_FN(Nwarr)

static struct BurnRomInfo NwarraRomDesc[] = {
	{ "vpha.03b",      0x080000, 0x0a70cdd6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.04b",      0x080000, 0x70ce62e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.05b",      0x080000, 0x5692a03f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.06b",      0x080000, 0xb810fe66, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.07b",      0x080000, 0x1be264f3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.08b",      0x080000, 0x86f1ed52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.09b",      0x080000, 0x7e96bd0a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vpha.10b",      0x080000, 0x58bce2fd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "nwarra.key",    0x000014, 0x9bafff67, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarra)
STD_ROM_FN(Nwarra)

static struct BurnRomInfo NwarrbRomDesc[] = {
	{ "vphb.03d",      0x080000, 0x3a426d3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.04a",      0x080000, 0x51c4bb2f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.05c",      0x080000, 0xac44d997, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.06a",      0x080000, 0x5072a5fe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.07",       0x080000, 0x9b355192, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.08",       0x080000, 0x42220f84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.09",       0x080000, 0x029e015d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphb.10",       0x080000, 0x37b3ce37, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "nwarrb.key",    0x000014, 0x4ffc0a54, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarrb)
STD_ROM_FN(Nwarrb)

static struct BurnRomInfo NwarrhRomDesc[] = {
	{ "vphh.03d",      0x080000, 0x6029c7be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.04a",      0x080000, 0xd26625ee, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.05c",      0x080000, 0x73ee0b8a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.06a",      0x080000, 0xa5b3a50a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.07",       0x080000, 0x5fc2bdc1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.08",       0x080000, 0xe65588d9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.09",       0x080000, 0xa2ce6d63, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphh.10",       0x080000, 0xe2f4f4b9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "nwarrh.key",    0x000014, 0x5fb16b23, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarrh)
STD_ROM_FN(Nwarrh)

static struct BurnRomInfo NwarruRomDesc[] = {
	{ "vphu.03f",      0x080000, 0x85d6a359, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.04c",      0x080000, 0xcb7fce77, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.05e",      0x080000, 0xe08f2bba, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.06c",      0x080000, 0x08c04cdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.07b",      0x080000, 0xb5a5ab19, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.08b",      0x080000, 0x51bb20fb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.09b",      0x080000, 0x41a64205, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.10b",      0x080000, 0x2b1d43ae, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "nwarru.key",    0x000014, 0x1c593f9b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarru)
STD_ROM_FN(Nwarru)

static struct BurnRomInfo VhuntjRomDesc[] = {
	{ "vphj.03f",      0x080000, 0x3de2e333, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.04c",      0x080000, 0xc95cf304, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.05d",      0x080000, 0x50de5ddd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.06c",      0x080000, 0xac3bd3d5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.07b",      0x080000, 0x0761309f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.08b",      0x080000, 0x5a5c2bf5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.09b",      0x080000, 0x823d6d99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.10b",      0x080000, 0x32c7d8f0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "vhuntj.key",    0x000014, 0x72854f68, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhuntj)
STD_ROM_FN(Vhuntj)

static struct BurnRomInfo Vhuntjr1sRomDesc[] = {
	{ "vphjstop.03b",  0x080000, 0x9c4e6191, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.04c",      0x080000, 0xc95cf304, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.05d",      0x080000, 0x50de5ddd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.06c",      0x080000, 0xac3bd3d5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.07b",      0x080000, 0x0761309f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.08b",      0x080000, 0x5a5c2bf5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.09b",      0x080000, 0x823d6d99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.10b",      0x080000, 0x32c7d8f0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "vhuntj.key",    0x000014, 0x72854f68, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhuntjr1s)
STD_ROM_FN(Vhuntjr1s)

static struct BurnRomInfo Vhuntjr1RomDesc[] = {
	{ "vphj.03c",      0x080000, 0x606b682a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.04b",      0x080000, 0xa3b40393, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.05b",      0x080000, 0xfccd5558, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.06b",      0x080000, 0x07e10a73, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.07b",      0x080000, 0x0761309f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.08b",      0x080000, 0x5a5c2bf5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.09b",      0x080000, 0x823d6d99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.10b",      0x080000, 0x32c7d8f0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "vhuntj.key",    0x000014, 0x72854f68, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhuntjr1)
STD_ROM_FN(Vhuntjr1)

static struct BurnRomInfo Vhuntjr2RomDesc[] = {
	{ "vphj.03b",      0x080000, 0x679c3fa9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.04a",      0x080000, 0xeb6e71e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.05a",      0x080000, 0xeaf634ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.06a",      0x080000, 0xb70cc6be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.07a",      0x080000, 0x46ab907d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.08a",      0x080000, 0x1c00355e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.09a",      0x080000, 0x026e6f82, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphj.10a",      0x080000, 0xaadfb3ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "vhuntj.key",    0x000014, 0x72854f68, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhuntjr2)
STD_ROM_FN(Vhuntjr2)

static struct BurnRomInfo ProgearRomDesc[] = {
	{ "pgau.03",       0x080000, 0x343a783e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pgau.04",       0x080000, 0x16208d79, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pga-simm.01c",  0x200000, 0x452f98b0, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01d",  0x200000, 0x9e672092, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01a",  0x200000, 0xae9ddafe, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01b",  0x200000, 0x94d72D94, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03c",  0x200000, 0x48a1886d, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03d",  0x200000, 0x172d7e37, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03a",  0x200000, 0x9ee33d98, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03b",  0x200000, 0x848dee32, CPS2_GFX_SIMM | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga-simm.05a",  0x200000, 0xc0aac80c, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.05b",  0x200000, 0x37a65d86, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06a",  0x200000, 0xd3f1e934, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06b",  0x200000, 0x8b39489a, CPS2_QSND_SIMM | BRF_SND },
	
	{ "progear.key",   0x000014, 0x46736b17, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progear)
STD_ROM_FN(Progear)

static struct BurnRomInfo ProgearaRomDesc[] = {
	{ "pgaa.03",       0x080000, 0x25e6e2ce, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pgaa.04",       0x080000, 0x8104307e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pga-simm.01c",  0x200000, 0x452f98b0, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01d",  0x200000, 0x9e672092, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01a",  0x200000, 0xae9ddafe, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01b",  0x200000, 0x94d72D94, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03c",  0x200000, 0x48a1886d, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03d",  0x200000, 0x172d7e37, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03a",  0x200000, 0x9ee33d98, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03b",  0x200000, 0x848dee32, CPS2_GFX_SIMM | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga-simm.05a",  0x200000, 0xc0aac80c, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.05b",  0x200000, 0x37a65d86, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06a",  0x200000, 0xd3f1e934, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06b",  0x200000, 0x8b39489a, CPS2_QSND_SIMM | BRF_SND },
	
	{ "progeara.key",  0x000014, 0x30a0fab6, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progeara)
STD_ROM_FN(Progeara)

static struct BurnRomInfo ProgearjRomDesc[] = {
	{ "pgaj.03",       0x080000, 0x06dbba54, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pgaj.04",       0x080000, 0xa1f1f1bc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pga-simm.01c",  0x200000, 0x452f98b0, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01d",  0x200000, 0x9e672092, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01a",  0x200000, 0xae9ddafe, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01b",  0x200000, 0x94d72D94, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03c",  0x200000, 0x48a1886d, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03d",  0x200000, 0x172d7e37, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03a",  0x200000, 0x9ee33d98, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03b",  0x200000, 0x848dee32, CPS2_GFX_SIMM | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga-simm.05a",  0x200000, 0xc0aac80c, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.05b",  0x200000, 0x37a65d86, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06a",  0x200000, 0xd3f1e934, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06b",  0x200000, 0x8b39489a, CPS2_QSND_SIMM | BRF_SND },
	
	{ "progearj.key",  0x000014, 0xd8d515e5, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progearj)
STD_ROM_FN(Progearj)

static struct BurnRomInfo Pzloop2RomDesc[] = {
	{ "pl2e.03",       0x080000, 0x3b1285b2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2e.04",       0x080000, 0x40a2d647, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2e.05",       0x080000, 0x0f11d818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2e.06",       0x080000, 0x86fbbdf4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pl2-simm.01c",  0x200000, 0x137b13a7, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01d",  0x200000, 0xa2db1507, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01a",  0x200000, 0x7e80ff8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01b",  0x200000, 0xcd93e6ed, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03c",  0x200000, 0x0f52bbca, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03d",  0x200000, 0xa62712c3, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03a",  0x200000, 0xb60c9f8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03b",  0x200000, 0x83fef284, CPS2_GFX_SIMM | BRF_GRA },

	{ "pl2.01",        0x020000, 0x35697569, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pl2-simm.05a",  0x200000, 0x85d8fbe8, CPS2_QSND_SIMM | BRF_SND },
	{ "pl2-simm.05b",  0x200000, 0x1ed62584, CPS2_QSND_SIMM | BRF_SND },
	
	{ "pzloop2.key",   0x000014, 0xae13be78, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pzloop2)
STD_ROM_FN(Pzloop2)

static struct BurnRomInfo Pzloop2jRomDesc[] = {
	{ "pl2j.03c",      0x080000, 0x3b76b806, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.04c",      0x080000, 0x8878a42a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.05c",      0x080000, 0x51081ea4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.06c",      0x080000, 0x51c68494, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pl2-simm.01c",  0x200000, 0x137b13a7, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01d",  0x200000, 0xa2db1507, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01a",  0x200000, 0x7e80ff8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01b",  0x200000, 0xcd93e6ed, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03c",  0x200000, 0x0f52bbca, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03d",  0x200000, 0xa62712c3, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03a",  0x200000, 0xb60c9f8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03b",  0x200000, 0x83fef284, CPS2_GFX_SIMM | BRF_GRA },

	{ "pl2.01",        0x020000, 0x35697569, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pl2-simm.05a",  0x200000, 0x85d8fbe8, CPS2_QSND_SIMM | BRF_SND },
	{ "pl2-simm.05b",  0x200000, 0x1ed62584, CPS2_QSND_SIMM | BRF_SND },
	
	{ "pzloop2.key",   0x000014, 0xae13be78, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pzloop2j)
STD_ROM_FN(Pzloop2j)

static struct BurnRomInfo Pzloop2jr1RomDesc[] = {
	{ "pl2j.03a",      0x080000, 0x0a751bd0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.04a",      0x080000, 0xc3f72afe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.05a",      0x080000, 0x6ea9dbfc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pl2j.06a",      0x080000, 0x0f14848d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pl2-simm.01c",  0x200000, 0x137b13a7, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01d",  0x200000, 0xa2db1507, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01a",  0x200000, 0x7e80ff8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.01b",  0x200000, 0xcd93e6ed, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03c",  0x200000, 0x0f52bbca, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03d",  0x200000, 0xa62712c3, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03a",  0x200000, 0xb60c9f8e, CPS2_GFX_SIMM | BRF_GRA },
	{ "pl2-simm.03b",  0x200000, 0x83fef284, CPS2_GFX_SIMM | BRF_GRA },

	{ "pl2.01",        0x020000, 0x35697569, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pl2-simm.05a",  0x200000, 0x85d8fbe8, CPS2_QSND_SIMM | BRF_SND },
	{ "pl2-simm.05b",  0x200000, 0x1ed62584, CPS2_QSND_SIMM | BRF_SND },
	
	{ "pzloop2.key",   0x000014, 0xae13be78, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pzloop2jr1)
STD_ROM_FN(Pzloop2jr1)

static struct BurnRomInfo QndreamRomDesc[] = {
	{ "tqzj.03a",      0x080000, 0x7acf3e30, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "tqzj.04",       0x080000, 0xf1044a87, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "tqzj.05",       0x080000, 0x4105ba0e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "tqzj.06",       0x080000, 0xc371e8a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "tqz.14m",       0x200000, 0x98af88a2, CPS2_GFX | BRF_GRA },
	{ "tqz.16m",       0x200000, 0xdf82d491, CPS2_GFX | BRF_GRA },
	{ "tqz.18m",       0x200000, 0x42f132ff, CPS2_GFX | BRF_GRA },
	{ "tqz.20m",       0x200000, 0xb2e128a3, CPS2_GFX | BRF_GRA },

	{ "tqz.01",        0x020000, 0xe9ce9d0a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "tqz.11m",       0x200000, 0x78e7884f, CPS2_QSND | BRF_SND },
	{ "tqz.12m",       0x200000, 0x2e049b13, CPS2_QSND | BRF_SND },
	
	{ "qndream.key",   0x000014, 0x97eee4ff, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Qndream)
STD_ROM_FN(Qndream)

static struct BurnRomInfo RingdestRomDesc[] = {
	{ "smbe.03b",      0x080000, 0xb8016278, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbe.04b",      0x080000, 0x18c4c447, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbe.05b",      0x080000, 0x18ebda7f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbe.06b",      0x080000, 0x89c80007, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.07",        0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.08",        0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "ringdest.key",  0x000014, 0x17f9269c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ringdest)
STD_ROM_FN(Ringdest)

static struct BurnRomInfo RingdestaRomDesc[] = {
	{ "smba.03a",      0x080000, 0xd3744dfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smba.04a",      0x080000, 0xf32d5b4f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smba.05a",      0x080000, 0x1016454f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smba.06a",      0x080000, 0x94b420cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.07",        0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.08",        0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "ringdesta.key", 0x000014, 0x905c9065, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ringdesta)
STD_ROM_FN(Ringdesta)

static struct BurnRomInfo RingdesthRomDesc[] = {
	{ "smbh.03b",      0x080000, 0x2e316584, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbh.04b",      0x080000, 0x9950a23a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbh.05b",      0x080000, 0x41e0b3fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbh.06b",      0x080000, 0x89c80007, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbh.07b",      0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbh.08b",      0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "ringdesth.key", 0x000014, 0xffb8d049, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ringdesth)
STD_ROM_FN(Ringdesth)

static struct BurnRomInfo SmbombRomDesc[] = {
	{ "smbj.03a",      0x080000, 0x1c5613de, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.04a",      0x080000, 0x29071ed7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.05a",      0x080000, 0xeb20bce4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.06a",      0x080000, 0x94b420cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.07",        0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.08",        0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "smbomb.key",    0x000014, 0xf690069b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Smbomb)
STD_ROM_FN(Smbomb)

static struct BurnRomInfo Smbombr1RomDesc[] = {
	{ "smbj.03",       0x080000, 0x52eafb10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.04",       0x080000, 0xaa6e8078, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.05",       0x080000, 0xb69e7d5f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbj.06",       0x080000, 0x8d857b56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.07",        0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.08",        0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "smbomb.key",    0x000014, 0xf690069b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Smbombr1)
STD_ROM_FN(Smbombr1)

static struct BurnRomInfo SfaRomDesc[] = {
	{ "sfze.03d",      0x080000, 0xebf2054d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04b",       0x080000, 0x8b73b0e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfa.key",       0x000014, 0x7c095631, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa)
STD_ROM_FN(Sfa)

static struct BurnRomInfo Sfar1RomDesc[] = {
	{ "sfze.03c",      0x080000, 0xa1b69dd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfze.04b",      0x080000, 0xbb90acd5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfa.key",       0x000014, 0x7c095631, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfar1)
STD_ROM_FN(Sfar1)

static struct BurnRomInfo Sfar2RomDesc[] = {
	{ "sfze.03b",      0x080000, 0x2bf5708e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04a",       0x080000, 0x5f99e9a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfa.key",       0x000014, 0x7c095631, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfar2)
STD_ROM_FN(Sfar2)

static struct BurnRomInfo Sfar3RomDesc[] = {
	{ "sfze.03a",      0x080000, 0xfdbcd434, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04",        0x080000, 0x0c436d30, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05",        0x080000, 0x1f363612, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfa.key",       0x000014, 0x7c095631, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfar3)
STD_ROM_FN(Sfar3)

static struct BurnRomInfo SfauRomDesc[] = {
	{ "sfzu.03a",      0x080000, 0x49fc7db9, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // If there's a US 950605 then this should be sfzu.03b
	{ "sfz.04a",       0x080000, 0x5f99e9a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfau.key",      0x000014, 0x1dd0998d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfau)
STD_ROM_FN(Sfau)

static struct BurnRomInfo SfzaRomDesc[] = {
	{ "sfza.03b",      0x080000, 0xca91bed9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04a",       0x080000, 0x5f99e9a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfza.key",      0x000014, 0x2aa6ac63, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfza)
STD_ROM_FN(Sfza)

static struct BurnRomInfo Sfzar1RomDesc[] = {
	{ "sfza.03a",      0x080000, 0xf38d8c8d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04",        0x080000, 0x0c436d30, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05",        0x080000, 0x1f363612, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfza.key",      0x000014, 0x2aa6ac63, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzar1)
STD_ROM_FN(Sfzar1)

static struct BurnRomInfo SfzbRomDesc[] = {
	{ "sfzb.03g",      0x080000, 0x348862d4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfzb.04e",      0x080000, 0x8d9b2480, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzb.key",      0x000014, 0xb0570359, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzb)
STD_ROM_FN(Sfzb)

static struct BurnRomInfo Sfzbr1RomDesc[] = {
	{ "sfzb.03e",      0x080000, 0xecba89a3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04b",       0x080000, 0x8b73b0e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzb.key",      0x000014, 0xb0570359, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzbr1)
STD_ROM_FN(Sfzbr1)

static struct BurnRomInfo SfzhRomDesc[] = {
	{ "sfzh.03d",      0x080000, 0x6e08cbe0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04c",       0x080000, 0xbb90acd5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05c",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzh.key",      0x000014, 0x4763446f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzh)
STD_ROM_FN(Sfzh)

static struct BurnRomInfo Sfzhr1RomDesc[] = {
	{ "sfzh.03c",      0x080000, 0xbce635aa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04a",       0x080000, 0x5f99e9a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzh.key",      0x000014, 0x4763446f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzhr1)
STD_ROM_FN(Sfzhr1)

static struct BurnRomInfo SfzjRomDesc[] = {
	{ "sfzj.03c",      0x080000, 0xf5444120, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04b",       0x080000, 0x8b73b0e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzj.key",      0x000014, 0x355d85b8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzj)
STD_ROM_FN(Sfzj)

static struct BurnRomInfo Sfzjr1RomDesc[] = {
	{ "sfzj.03b",      0x080000, 0x844220c2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04a",       0x080000, 0x5f99e9a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzj.key",      0x000014, 0x355d85b8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzjr1)
STD_ROM_FN(Sfzjr1)

static struct BurnRomInfo Sfzjr2RomDesc[] = {
	{ "sfzj.03a",      0x080000, 0x3cfce93c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04",        0x080000, 0x0c436d30, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05",        0x080000, 0x1f363612, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "sfzj.key",      0x000014, 0x355d85b8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfzjr2)
STD_ROM_FN(Sfzjr2)

static struct BurnRomInfo Sfa2RomDesc[] = {
	{ "sz2e.03",       0x080000, 0x1061e6bb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2e.04",       0x080000, 0x22d17b26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05",        0x080000, 0x4b442a7c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.07",        0x080000, 0x8e184246, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfa2.key",      0x000014, 0x1578dcb0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa2)
STD_ROM_FN(Sfa2)

static struct BurnRomInfo Sfa2uRomDesc[] = {
	{ "sz2u.03a",      0x080000, 0xd03e504f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.04a",      0x080000, 0xfae0e9c3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.05a",      0x080000, 0xd02dd758, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.06",       0x080000, 0xc5c8eb63, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.07",       0x080000, 0x5de01cc5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.08",       0x080000, 0xbea11d56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfa2u.key",     0x000014, 0x4a8d91ef, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa2u)
STD_ROM_FN(Sfa2u)

static struct BurnRomInfo Sfa2ur1RomDesc[] = {
	{ "sz2u.03",       0x080000, 0x84a09006, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.04",       0x080000, 0xac46e5ed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.05",       0x080000, 0x6c0c79d3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.06",       0x080000, 0xc5c8eb63, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.07",       0x080000, 0x5de01cc5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2u.08",       0x080000, 0xbea11d56, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfa2u.key",     0x000014, 0x4a8d91ef, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa2ur1)
STD_ROM_FN(Sfa2ur1)

static struct BurnRomInfo Sfz2aRomDesc[] = {
	{ "sz2a.03a",      0x080000, 0x30d2099f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2a.04a",      0x080000, 0x1cc94db1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05a",       0x080000, 0x98e8e992, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2a.07a",      0x080000, 0x0aed2494, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2a.key",     0x000014, 0x777b7358, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2a)
STD_ROM_FN(Sfz2a)

static struct BurnRomInfo Sfz2bRomDesc[] = {
	{ "sz2b.03b",      0x080000, 0x1ac12812, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.04b",      0x080000, 0xe4ffaf68, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.05a",      0x080000, 0xdd224156, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.06a",      0x080000, 0xa45a75a6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.07a",      0x080000, 0x7d19d5ec, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.08",       0x080000, 0x92b66e01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2b.key",     0x000014, 0x35b1df07, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2b)
STD_ROM_FN(Sfz2b)

static struct BurnRomInfo Sfz2br1RomDesc[] = {
	{ "sz2b.03",       0x080000, 0xe6ce530b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.04",       0x080000, 0x1605a0cb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05",        0x080000, 0x4b442a7c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.07",       0x080000, 0x947e8ac6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2b.08",       0x080000, 0x92b66e01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2b.key",     0x000014, 0x35b1df07, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2br1)
STD_ROM_FN(Sfz2br1)

static struct BurnRomInfo Sfz2hRomDesc[] = {
	{ "sz2h.03" ,      0x080000, 0xbfeddf5b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2h.04",       0x080000, 0xea5009fb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05",        0x080000, 0x4b442a7c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2h.07",       0x080000, 0x947e8ac6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2h.08",       0x080000, 0x92b66e01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2h.key",     0x000014, 0x2719ea16, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2h)
STD_ROM_FN(Sfz2h)

static struct BurnRomInfo Sfz2jRomDesc[] = {
	{ "sz2j.03b",      0x080000, 0x3e1e2e85, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.04b",      0x080000, 0xf53d6c45, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.05b",      0x080000, 0xdd224156, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.06b",      0x080000, 0xa45a75a6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.07b",      0x080000, 0x6352f038, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.08b",      0x080000, 0x92b66e01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2j.key",     0x000014, 0x455bd098, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2j)
STD_ROM_FN(Sfz2j)

static struct BurnRomInfo Sfz2jr1RomDesc[] = {
	{ "sz2j.03a",      0x080000, 0x97461e28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.04a",      0x080000, 0xae4851a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05a",       0x080000, 0x98e8e992, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.07a",      0x080000, 0xd910b2a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2j.key",     0x000014, 0x455bd098, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2jr1)
STD_ROM_FN(Sfz2jr1)

static struct BurnRomInfo Sfz2nRomDesc[] = {
	{ "sz2n.03" ,      0x080000, 0x58924741, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2n.04",       0x080000, 0x592a17c5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05",        0x080000, 0x4b442a7c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.07",        0x080000, 0x8e184246, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2n.key",     0x000014, 0xd1cc49d5, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2n)
STD_ROM_FN(Sfz2n)

static struct BurnRomInfo Sfz2alRomDesc[] = {
	{ "szaa.03",       0x080000, 0x88e7023e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.04",       0x080000, 0xae8ec36e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.05",       0x080000, 0xf053a55e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.06",       0x080000, 0xcfc0e7a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.07",       0x080000, 0x5feb8b20, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.08",       0x080000, 0x6eb6d412, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sza.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sza.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sza.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sza.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sza.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sza.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sza.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sza.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sza.01",        0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sza.02",        0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sza.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sza.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2al.key",    0x000014, 0x2904963e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2al)
STD_ROM_FN(Sfz2al)

static struct BurnRomInfo Sfz2albRomDesc[] = {
	{ "szab.03",       0x080000, 0xcb436eca, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szab.04",       0x080000, 0x14534bea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szab.05",       0x080000, 0x7fb10658, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.06",        0x080000, 0x0abda2fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.07",        0x080000, 0xe9430762, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.08",        0x080000, 0xb65711a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sza.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sza.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sza.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sza.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sza.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sza.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sza.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sza.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sza.01",        0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sza.02",        0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sza.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sza.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2alb.key",   0x000014, 0xc8b3ac73, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2alb)
STD_ROM_FN(Sfz2alb)

static struct BurnRomInfo Sfz2alhRomDesc[] = {
	{ "szah.03",       0x080000, 0x06f93d1d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szah.04",       0x080000, 0xe62ee914, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szah.05",       0x080000, 0x2b7f4b20, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.06",        0x080000, 0x0abda2fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.07",        0x080000, 0xe9430762, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sza.08",        0x080000, 0xb65711a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sza.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sza.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sza.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sza.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sza.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sza.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sza.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sza.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sza.01",        0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sza.02",        0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sza.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sza.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2alh.key",   0x000014, 0xf320f655, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2alh)
STD_ROM_FN(Sfz2alh)

static struct BurnRomInfo Sfz2aljRomDesc[] = {
	{ "szaj.03a",      0x080000, 0xa3802fe3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaj.04a",      0x080000, 0xe7ca87c7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaj.05a",      0x080000, 0xc88ebf88, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaj.06a",      0x080000, 0x35ed5b7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaj.07a",      0x080000, 0x975dcb3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaj.08a",      0x080000, 0xdc73f2d7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sza.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sza.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sza.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sza.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sza.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sza.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sza.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sza.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sza.01",        0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sza.02",        0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sza.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sza.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "sfz2alj.key",   0x000014, 0x4c42320f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2alj)
STD_ROM_FN(Sfz2alj)

static struct BurnRomInfo Sfa3RomDesc[] = {
	{ "sz3e.03c",      0x080000, 0x9762b206, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3e.04c",      0x080000, 0x5ad3f721, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3.key",      0x000014, 0x54fa39c6, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3)
STD_ROM_FN(Sfa3)

static struct BurnRomInfo Sfa3bRomDesc[] = {
	{ "sz3b.03",       0x080000, 0x046c9b4d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3b.04",       0x080000, 0xda211919, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3b.key",     0x000014, 0x2d0a1351, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3b)
STD_ROM_FN(Sfa3b)

static struct BurnRomInfo Sfa3hRomDesc[] = {
	{ "sz3h.03c",      0x080000, 0xb3b563a3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3h.04c",      0x080000, 0x47891fec, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3h.key",     0x000014, 0x1b34998c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3h)
STD_ROM_FN(Sfa3h)

static struct BurnRomInfo Sfa3hr1RomDesc[] = {
	{ "sz3h.03",       0x080000, 0x4b16cb3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3h.04",       0x080000, 0x88ad2e6a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3h.key",     0x000014, 0x1b34998c, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3hr1)
STD_ROM_FN(Sfa3hr1)

static struct BurnRomInfo Sfa3uRomDesc[] = {
	{ "sz3u.03c",      0x080000, 0xe007da2e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3u.04c",      0x080000, 0x5f78f0e7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3u.key",     0x000014, 0x4a8f98c1, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3u)
STD_ROM_FN(Sfa3u)

static struct BurnRomInfo Sfa3ur1RomDesc[] = {
	{ "sz3u.03",       0x080000, 0xb5984a19, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3u.04",       0x080000, 0x7e8158ba, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3u.key",     0x000014, 0x4a8f98c1, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3ur1)
STD_ROM_FN(Sfa3ur1)

static struct BurnRomInfo Sfa3usRomDesc[] = {
	{ "sz3-usam_03.6a",     0x080000, 0x14319e29, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_04.7a",     0x080000, 0x65fbc272, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_05.8a",     0x080000, 0xe93c47d1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_06.9a",     0x080000, 0x1bf09de3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_07.6d",     0x080000, 0xf6296d96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_08.7d",     0x080000, 0x1f4008ff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_09.8d",     0x080000, 0x822fc451, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3-usam_10.9d",     0x080000, 0x92713468, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       		0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       		0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       		0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       		0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       		0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       		0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       		0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       		0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3-usam_01.1a",     0x020000, 0xc180947d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3-usam_02.2a",     0x020000, 0x9ebc280f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       		0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       		0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfa3u.key",          0x000014, 0x4a8f98c1, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3us)
STD_ROM_FN(Sfa3us)

static struct BurnRomInfo Sfz3aRomDesc[] = {
	{ "sz3a.03d",      0x080000, 0xd7e140d6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3a.04d",      0x080000, 0xe06869a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfz3a.key",     0x000014, 0x09045d61, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3a)
STD_ROM_FN(Sfz3a)

static struct BurnRomInfo Sfz3ar1RomDesc[] = {
	{ "sz3a.03a",      0x080000, 0x29c681fd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3a.04",       0x080000, 0x9ddd1484, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfz3a.key",     0x000014, 0x09045d61, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3ar1)
STD_ROM_FN(Sfz3ar1)

static struct BurnRomInfo Sfz3jRomDesc[] = {
	{ "sz3j.03c",      0x080000, 0xcadf4a51, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3j.04c",      0x080000, 0xfcb31228, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfz3j.key",     0x000014, 0xd30cca8d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3j)
STD_ROM_FN(Sfz3j)

static struct BurnRomInfo Sfz3jr1RomDesc[] = {
	{ "sz3j.03a",      0x080000, 0x6ee0beae, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3j.04a",      0x080000, 0xa6e2978d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05a",       0x080000, 0x05964b7d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06a",       0x080000, 0x78ce2179, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07a",       0x080000, 0x398bf52f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08a",       0x080000, 0x866d0588, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09a",       0x080000, 0x2180892c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfz3j.key",     0x000014, 0xd30cca8d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3jr1)
STD_ROM_FN(Sfz3jr1)

static struct BurnRomInfo Sfz3jr2RomDesc[] = {
	{ "sz3j.03",       0x080000, 0xf7cb4b13, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3j.04",       0x080000, 0x0846c29d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "sfz3j.key",     0x000014, 0xd30cca8d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3jr2)
STD_ROM_FN(Sfz3jr2)

static struct BurnRomInfo SgemfRomDesc[] = {
	{ "pcfu.03",       0x080000, 0xac2e8566, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.04",        0x080000, 0xf4314c96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.05",        0x080000, 0x215655f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.06",        0x080000, 0xea6f13ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.07",        0x080000, 0x5ac6d5ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pcf.13m",       0x400000, 0x22d72ab9, CPS2_GFX | BRF_GRA },
	{ "pcf.15m",       0x400000, 0x16a4813c, CPS2_GFX | BRF_GRA },
	{ "pcf.17m",       0x400000, 0x1097e035, CPS2_GFX | BRF_GRA },
	{ "pcf.19m",       0x400000, 0xd362d874, CPS2_GFX | BRF_GRA },
	{ "pcf.14m",       0x100000, 0x0383897c, CPS2_GFX | BRF_GRA },
	{ "pcf.16m",       0x100000, 0x76f91084, CPS2_GFX | BRF_GRA },
	{ "pcf.18m",       0x100000, 0x756c3754, CPS2_GFX | BRF_GRA },
	{ "pcf.20m",       0x100000, 0x9ec9277d, CPS2_GFX | BRF_GRA },

	{ "pcf.01",        0x020000, 0x254e5f33, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pcf.02",        0x020000, 0x6902f4f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pcf.11m",       0x400000, 0xa5dea005, CPS2_QSND | BRF_SND },
	{ "pcf.12m",       0x400000, 0x4ce235fe, CPS2_QSND | BRF_SND },
	
	{ "sgemf.key",     0x000014, 0x3d604021, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sgemf)
STD_ROM_FN(Sgemf)

static struct BurnRomInfo PfghtjRomDesc[] = {
	{ "pcfj.03",       0x080000, 0x681da43e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.04",        0x080000, 0xf4314c96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.05",        0x080000, 0x215655f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.06",        0x080000, 0xea6f13ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.07",        0x080000, 0x5ac6d5ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pcf.13m",       0x400000, 0x22d72ab9, CPS2_GFX | BRF_GRA },
	{ "pcf.15m",       0x400000, 0x16a4813c, CPS2_GFX | BRF_GRA },
	{ "pcf.17m",       0x400000, 0x1097e035, CPS2_GFX | BRF_GRA },
	{ "pcf.19m",       0x400000, 0xd362d874, CPS2_GFX | BRF_GRA },
	{ "pcf.14m",       0x100000, 0x0383897c, CPS2_GFX | BRF_GRA },
	{ "pcf.16m",       0x100000, 0x76f91084, CPS2_GFX | BRF_GRA },
	{ "pcf.18m",       0x100000, 0x756c3754, CPS2_GFX | BRF_GRA },
	{ "pcf.20m",       0x100000, 0x9ec9277d, CPS2_GFX | BRF_GRA },

	{ "pcf.01",        0x020000, 0x254e5f33, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pcf.02",        0x020000, 0x6902f4f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pcf.11m",       0x400000, 0xa5dea005, CPS2_QSND | BRF_SND },
	{ "pcf.12m",       0x400000, 0x4ce235fe, CPS2_QSND | BRF_SND },
	
	{ "pfghtj.key",    0x000014, 0x62297638, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Pfghtj)
STD_ROM_FN(Pfghtj)

static struct BurnRomInfo SgemfaRomDesc[] = {
	{ "pcfa.03",       0x080000, 0xe17c089a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.04",        0x080000, 0xf4314c96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.05",        0x080000, 0x215655f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.06",        0x080000, 0xea6f13ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.07",        0x080000, 0x5ac6d5ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pcf.13m",       0x400000, 0x22d72ab9, CPS2_GFX | BRF_GRA },
	{ "pcf.15m",       0x400000, 0x16a4813c, CPS2_GFX | BRF_GRA },
	{ "pcf.17m",       0x400000, 0x1097e035, CPS2_GFX | BRF_GRA },
	{ "pcf.19m",       0x400000, 0xd362d874, CPS2_GFX | BRF_GRA },
	{ "pcf.14m",       0x100000, 0x0383897c, CPS2_GFX | BRF_GRA },
	{ "pcf.16m",       0x100000, 0x76f91084, CPS2_GFX | BRF_GRA },
	{ "pcf.18m",       0x100000, 0x756c3754, CPS2_GFX | BRF_GRA },
	{ "pcf.20m",       0x100000, 0x9ec9277d, CPS2_GFX | BRF_GRA },

	{ "pcf.01",        0x020000, 0x254e5f33, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pcf.02",        0x020000, 0x6902f4f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pcf.11m",       0x400000, 0xa5dea005, CPS2_QSND | BRF_SND },
	{ "pcf.12m",       0x400000, 0x4ce235fe, CPS2_QSND | BRF_SND },
	
	{ "sgemfa.key",    0x000014, 0xdd513738, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sgemfa)
STD_ROM_FN(Sgemfa)

static struct BurnRomInfo SgemfhRomDesc[] = {
	{ "pcfh.03",       0x080000, 0xe9103347, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.04",        0x080000, 0xf4314c96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.05",        0x080000, 0x215655f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.06",        0x080000, 0xea6f13ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.07",        0x080000, 0x5ac6d5ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pcf.13m",       0x400000, 0x22d72ab9, CPS2_GFX | BRF_GRA },
	{ "pcf.15m",       0x400000, 0x16a4813c, CPS2_GFX | BRF_GRA },
	{ "pcf.17m",       0x400000, 0x1097e035, CPS2_GFX | BRF_GRA },
	{ "pcf.19m",       0x400000, 0xd362d874, CPS2_GFX | BRF_GRA },
	{ "pcf.14m",       0x100000, 0x0383897c, CPS2_GFX | BRF_GRA },
	{ "pcf.16m",       0x100000, 0x76f91084, CPS2_GFX | BRF_GRA },
	{ "pcf.18m",       0x100000, 0x756c3754, CPS2_GFX | BRF_GRA },
	{ "pcf.20m",       0x100000, 0x9ec9277d, CPS2_GFX | BRF_GRA },

	{ "pcf.01",        0x020000, 0x254e5f33, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pcf.02",        0x020000, 0x6902f4f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pcf.11m",       0x400000, 0xa5dea005, CPS2_QSND | BRF_SND },
	{ "pcf.12m",       0x400000, 0x4ce235fe, CPS2_QSND | BRF_SND },
	
	{ "sgemfh.key",    0x000014, 0xf97f4b7d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sgemfh)
STD_ROM_FN(Sgemfh)

static struct BurnRomInfo Spf2tRomDesc[] = {
	{ "pzfe.03",       0x080000, 0x2af51954, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // marked pzfe.04 but same as pzf.04

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "spf2t.key",     0x000014, 0x4c4dc7e3, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2t)
STD_ROM_FN(Spf2t)

static struct BurnRomInfo Spf2tuRomDesc[] = {
	{ "pzfu.03a",      0x080000, 0x346e62ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "spf2tu.key",    0x000014, 0x5d7b15e8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2tu)
STD_ROM_FN(Spf2tu)

static struct BurnRomInfo Spf2taRomDesc[] = {
	{ "pzfa.03",       0x080000, 0x3cecfa78, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "spf2ta.key",    0x000014, 0x61e93a18, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2ta)
STD_ROM_FN(Spf2ta)

static struct BurnRomInfo Spf2thRomDesc[] = {
	{ "pzfh.03",       0x080000, 0x20510f2d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "spf2th.key",    0x000014, 0x292db449, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2th)
STD_ROM_FN(Spf2th)

static struct BurnRomInfo Spf2xjRomDesc[] = {
	{ "pzfj.03a",      0x080000, 0x2070554a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "spf2xj.key",    0x000014, 0xdc39fd34, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2xj)
STD_ROM_FN(Spf2xj)

static struct BurnRomInfo Ssf2RomDesc[] = {
	{ "ssfe-03b",      0x080000, 0xaf654792, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.04",       0x080000, 0xb082aa67, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.05",       0x080000, 0x02b9c137, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe-06b",      0x080000, 0x1c8e44a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.07",       0x080000, 0x2409001d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf-01a",       0x020000, 0x71fcdfc9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2.key",      0x000014, 0xe469ccbb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2)
STD_ROM_FN(Ssf2)

static struct BurnRomInfo Ssf2r1RomDesc[] = {
	{ "ssfe.03",       0x080000, 0xa597745d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.04",       0x080000, 0xb082aa67, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.05",       0x080000, 0x02b9c137, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.06",       0x080000, 0x70d470c5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.07",       0x080000, 0x2409001d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2.key",      0x000014, 0xe469ccbb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2r1)
STD_ROM_FN(Ssf2r1)

static struct BurnRomInfo Ssf2aRomDesc[] = {
	{ "ssfa.03b",      0x080000, 0x83a059bf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.04a",      0x080000, 0x5d873642, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.05",       0x080000, 0xf8fb4de2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.06b",      0x080000, 0x3185d19d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.07",       0x080000, 0x36e29217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2a.key",     0x000014, 0x5fb6013f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2a)
STD_ROM_FN(Ssf2a)

static struct BurnRomInfo Ssf2ar1RomDesc[] = {
	{ "ssfa.03a",      0x080000, 0xd2a3c520, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.04a",      0x080000, 0x5d873642, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.05",       0x080000, 0xf8fb4de2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.06",       0x080000, 0xaa8acee7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.07",       0x080000, 0x36e29217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2a.key",     0x000014, 0x5fb6013f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2ar1)
STD_ROM_FN(Ssf2ar1)

static struct BurnRomInfo Ssf2hRomDesc[] = {
	{ "ssfh.03",       0x080000, 0xb086b355, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.04",       0x080000, 0x1e629b29, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.05",       0x080000, 0xb5997e10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.06",       0x080000, 0x793b8fad, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.07",       0x080000, 0xcbb92ac3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2h.key",     0x000014, 0x8331bc8e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2h)
STD_ROM_FN(Ssf2h)

static struct BurnRomInfo Ssf2jRomDesc[] = {
	{ "ssfj.03b",      0x080000, 0x5c2e356d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.04a",      0x080000, 0x013bd55c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.05",       0x080000, 0x0918d19a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.06b",      0x080000, 0x014e0c6d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.07",       0x080000, 0xeb6a9b1b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2j.key",     0x000014, 0xbca45cc2, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2j)
STD_ROM_FN(Ssf2j)

static struct BurnRomInfo Ssf2jr1RomDesc[] = {
	{ "ssfj.03a",      0x080000, 0x0bbf1304, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.04a",      0x080000, 0x013bd55c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.05",       0x080000, 0x0918d19a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.06",       0x080000, 0xd808a6cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.07",       0x080000, 0xeb6a9b1b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2j.key",     0x000014, 0xbca45cc2, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2jr1)
STD_ROM_FN(Ssf2jr1)

static struct BurnRomInfo Ssf2jr2RomDesc[] = {
	{ "ssfj.03",       0x080000, 0x7eb0efed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.04",       0x080000, 0xd7322164, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.05",       0x080000, 0x0918d19a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.06",       0x080000, 0xd808a6cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.07",       0x080000, 0xeb6a9b1b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2j.key",     0x000014, 0xbca45cc2, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2jr2)
STD_ROM_FN(Ssf2jr2)

static struct BurnRomInfo Ssf2uRomDesc[] = {
	{ "ssfu.03a",      0x080000, 0x72f29c33, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfu.04a",      0x080000, 0x935cea44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfu.05",       0x080000, 0xa0acb28a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfu.06",       0x080000, 0x47413dcf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfu.07",       0x080000, 0xe6066077, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2u.key",     0x000014, 0x2f4f8e9d, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2u)
STD_ROM_FN(Ssf2u)

static struct BurnRomInfo Ssf2tbRomDesc[] = {
	{ "ssfe.03tc",     0x080000, 0x496a8409, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.04tc",     0x080000, 0x4b45c18b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.05t",      0x080000, 0x6a9c6444, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.06tb",     0x080000, 0xe4944fc3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.07t",      0x080000, 0x2c9f4782, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tb.key",    0x000014, 0x1ecc92b2, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tb)
STD_ROM_FN(Ssf2tb)

static struct BurnRomInfo Ssf2tbaRomDesc[] = {
	{ "ssfa.03tb",     0x080000, 0x8de631d2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.04ta",     0x080000, 0xabef3042, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.05t",      0x080000, 0xedfa018f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.06tb",     0x080000, 0x2b9d1dbc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfa.07t",      0x080000, 0xf4a25159, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf-01a",       0x020000, 0x71fcdfc9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tba.key",   0x000014, 0x8d2740ed, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tba)
STD_ROM_FN(Ssf2tba)

static struct BurnRomInfo Ssf2tbr1RomDesc[] = {
	{ "ssfe.03t",      0x080000, 0x1e018e34, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.04t",      0x080000, 0xac92efaf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.05t",      0x080000, 0x6a9c6444, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.06t",      0x080000, 0xf442562b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfe.07t",      0x080000, 0x2c9f4782, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tb.key",    0x000014, 0x1ecc92b2, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tbr1)
STD_ROM_FN(Ssf2tbr1)

static struct BurnRomInfo Ssf2tbjRomDesc[] = {
	{ "ssftj.03b",     0x080000, 0xe78a3433, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssftj.04t",     0x080000, 0xb4dc1906, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssftj.05",      0x080000, 0xa7e35fbc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.06tb",     0x080000, 0x0737c30d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.07t",      0x080000, 0x1f239515, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tbj.key",   0x000014, 0xbcc2e017, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tbj)
STD_ROM_FN(Ssf2tbj)

static struct BurnRomInfo Ssf2tbj1RomDesc[] = {
	{ "ssfj.03t",      0x080000, 0x980d4450, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.04t",      0x080000, 0xb4dc1906, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.05t",      0x080000, 0xa7e35fbc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.06t",      0x080000, 0xcb592b30, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfj.07t",      0x080000, 0x1f239515, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tbj.key",   0x000014, 0xbcc2e017, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tbj1)
STD_ROM_FN(Ssf2tbj1)

static struct BurnRomInfo Ssf2tbhRomDesc[] = {
	{ "ssfh.03tb",     0x080000, 0x6db7d28b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.04t",      0x080000, 0x0fe7d895, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.05t",      0x080000, 0x41be4f2d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.06tb",     0x080000, 0xd2522eb1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfh.07t",      0x080000, 0xb1c3a3c6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "ssf2tbh.key",   0x000014, 0xfddecf4f, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tbh)
STD_ROM_FN(Ssf2tbh)

static struct BurnRomInfo Ssf2tRomDesc[] = {
	{ "sfxe.03c",      0x080000, 0x2fa1f396, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxe.04a",      0x080000, 0xd0bc29c6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxe.05",       0x080000, 0x65222964, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxe.06a",      0x080000, 0x8fe9f531, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxe.07",       0x080000, 0x8a7d0cb6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxe.08",       0x080000, 0x74c24062, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2t.key",     0x000014, 0x524d608e, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2t)
STD_ROM_FN(Ssf2t)

static struct BurnRomInfo Ssf2taRomDesc[] = {
	{ "sfxa.03c",      0x080000, 0x04b9ff34, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxa.04a",      0x080000, 0x16ea5f7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxa.05",       0x080000, 0x53d61f0c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxa.06a",      0x080000, 0x066d09b5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxa.07",       0x080000, 0xa428257b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxa.08",       0x080000, 0x39be596c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2ta.key",    0x000014, 0xc11fa8e9, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2ta)
STD_ROM_FN(Ssf2ta)

static struct BurnRomInfo Ssf2thRomDesc[] = {
	{ "sfxh.03c",      0x080000, 0xfbe80dfe, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.04a",      0x080000, 0xef9dd4b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.05",       0x080000, 0x09e56ecc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.06a",      0x080000, 0xe6f210be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.07",       0x080000, 0x900ba1a4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.08",       0x080000, 0xc15f0424, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxh.09",       0x080000, 0x5b92b3f9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2th.key",    0x000014, 0xf6ce6a35, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2th)
STD_ROM_FN(Ssf2th)

static struct BurnRomInfo Ssf2tuRomDesc[] = {
	{ "sfxu.03e",      0x080000, 0xd6ff689e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.04a",      0x080000, 0x532b5ffd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.05",       0x080000, 0xffa3c6de, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.06b",      0x080000, 0x83f9382b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.07a",      0x080000, 0x6ab673e8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.08",       0x080000, 0xb3c71810, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2tu.key",    0x000014, 0xf7d62def, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tu)
STD_ROM_FN(Ssf2tu)

static struct BurnRomInfo Ssf2tur1RomDesc[] = {
	{ "sfxu.03c",      0x080000, 0x86e4a335, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.04a",      0x080000, 0x532b5ffd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.05",       0x080000, 0xffa3c6de, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.06a",      0x080000, 0xe4c04c99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.07",       0x080000, 0xd8199e41, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxu.08",       0x080000, 0xb3c71810, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2tu.key",    0x000014, 0xf7d62def, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tur1)
STD_ROM_FN(Ssf2tur1)

static struct BurnRomInfo Ssf2xjRomDesc[] = {
	{ "sfxj.03d",      0x080000, 0x50b52b37, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.04a",      0x080000, 0xaf7767b4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.05",       0x080000, 0xf4ff18f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.06b",      0x080000, 0x413477c2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.07a",      0x080000, 0xa18b3d83, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.08",       0x080000, 0x2de76f10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2xj.key",    0x000014, 0x160d1424, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2xj)
STD_ROM_FN(Ssf2xj)

static struct BurnRomInfo Ssf2xjr1RomDesc[] = {
	{ "sfxj.03c",      0x080000, 0xa7417b79, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.04a",      0x080000, 0xaf7767b4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.05",       0x080000, 0xf4ff18f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.06a",      0x080000, 0x260d0370, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.07",       0x080000, 0x1324d02a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxj.08",       0x080000, 0x2de76f10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfx.09",        0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "ssf2xj.key",    0x000014, 0x160d1424, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2xjr1)
STD_ROM_FN(Ssf2xjr1)

static struct BurnRomInfo Ssf2xjr1rRomDesc[] = {
	{ "sfxo.03c",      0x080000, 0x2ba33dc6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.04a",      0x080000, 0xba663dd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.05",       0x080000, 0x1321625c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.06a",      0x080000, 0x0cc490ed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.07",       0x080000, 0x64b9015e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.08",       0x080000, 0xb60f4b58, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxo.09",       0x080000, 0x642fae3f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.69",        0x080000, 0xe9123f9f, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.70",        0x080000, 0x2f8201f3, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.71",        0x080000, 0x0fa334b4, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.72",        0x080000, 0xb76740d3, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.73",        0x080000, 0x14f058ec, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.74",        0x080000, 0x800c3ae9, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.75",        0x080000, 0x06cf540b, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.76",        0x080000, 0x71084e42, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.59",        0x080000, 0x6eb3ee4d, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.60",        0x080000, 0x2bcf1eda, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.61",        0x080000, 0x3330cc11, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.62",        0x080000, 0x96e2ead3, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.63",        0x080000, 0xe356a275, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.64",        0x080000, 0xfec5698b, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.65",        0x080000, 0x69da0751, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.66",        0x080000, 0xcc53ec15, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.79",        0x080000, 0xcf0d44a8, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.80",        0x080000, 0x56a153a4, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.81",        0x080000, 0x5484e5f6, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.82",        0x080000, 0xfce6b7f5, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.83",        0x080000, 0x042d7970, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.84",        0x080000, 0x88c472e6, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.85",        0x080000, 0xa7d66348, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.86",        0x080000, 0xcf9119c8, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.89",        0x080000, 0x6d374ad9, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.90",        0x080000, 0x34cf8bcf, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.91",        0x080000, 0xd796ea3f, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.92",        0x080000, 0xc85fb7e3, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.93",        0x080000, 0x6c50c2b5, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "ssf.94",        0x080000, 0x59549f63, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.95",        0x080000, 0x86c97869, CPS2_GFX_SPLIT8 | BRF_GRA },
	{ "sfx.96",        0x080000, 0x1c0e1989, CPS2_GFX_SPLIT8 | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.51a",       0x080000, 0x9eda6954, CPS2_QSND | BRF_SND },
	{ "ssf.52a",       0x080000, 0x355f6589, CPS2_QSND | BRF_SND },
	{ "ssf.53a",       0x080000, 0xd5d08a05, CPS2_QSND | BRF_SND },
	{ "ssf.54a",       0x080000, 0x930725eb, CPS2_QSND | BRF_SND },
	{ "ssf.55a",       0x080000, 0x827abf3c, CPS2_QSND | BRF_SND },
	{ "ssf.56a",       0x080000, 0x3919c0e5, CPS2_QSND | BRF_SND },
	{ "ssf.57a",       0x080000, 0x1ba9bfa6, CPS2_QSND | BRF_SND },
	{ "ssf.58a",       0x080000, 0x0c89a272, CPS2_QSND | BRF_SND },
	
	{ "ssf2xjr1r.key", 0x000014, 0x82c86e63, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2xjr1r)
STD_ROM_FN(Ssf2xjr1r)

static struct BurnRomInfo Vhunt2RomDesc[] = {
	{ "vh2j.03a",      0x080000, 0x9ae8f186, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.04a",      0x080000, 0xe2fabf53, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.05",       0x080000, 0xde34f624, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.06",       0x080000, 0x6a3b9897, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.07",       0x080000, 0xb021c029, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.08",       0x080000, 0xac873dff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.09",       0x080000, 0xeaefce9c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.10",       0x080000, 0x11730952, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vh2.13m",       0x400000, 0x3b02ddaa, CPS2_GFX | BRF_GRA },
	{ "vh2.15m",       0x400000, 0x4e40de66, CPS2_GFX | BRF_GRA },
	{ "vh2.17m",       0x400000, 0xb31d00c9, CPS2_GFX | BRF_GRA },
	{ "vh2.19m",       0x400000, 0x149be3ab, CPS2_GFX | BRF_GRA },
	{ "vh2.14m",       0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vh2.16m",       0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vh2.18m",       0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vh2.20m",       0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },

	{ "vh2.01",        0x020000, 0x67b9f779, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vh2.02",        0x020000, 0xaaf15fcb, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vh2.11m",       0x400000, 0x38922efd, CPS2_QSND | BRF_SND },
	{ "vh2.12m",       0x400000, 0x6e2430af, CPS2_QSND | BRF_SND },
	
	{ "vhunt2.key",    0x000014, 0x61306b20, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhunt2)
STD_ROM_FN(Vhunt2)

static struct BurnRomInfo Vhunt2r1RomDesc[] = {
	{ "vh2j.03",       0x080000, 0x1a5feb13, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.04",       0x080000, 0x434611a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.05",       0x080000, 0xde34f624, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.06",       0x080000, 0x6a3b9897, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.07",       0x080000, 0xb021c029, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.08",       0x080000, 0xac873dff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.09",       0x080000, 0xeaefce9c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.10",       0x080000, 0x11730952, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vh2.13m",       0x400000, 0x3b02ddaa, CPS2_GFX | BRF_GRA },
	{ "vh2.15m",       0x400000, 0x4e40de66, CPS2_GFX | BRF_GRA },
	{ "vh2.17m",       0x400000, 0xb31d00c9, CPS2_GFX | BRF_GRA },
	{ "vh2.19m",       0x400000, 0x149be3ab, CPS2_GFX | BRF_GRA },
	{ "vh2.14m",       0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vh2.16m",       0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vh2.18m",       0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vh2.20m",       0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },

	{ "vh2.01",        0x020000, 0x67b9f779, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vh2.02",        0x020000, 0xaaf15fcb, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vh2.11m",       0x400000, 0x38922efd, CPS2_QSND | BRF_SND },
	{ "vh2.12m",       0x400000, 0x6e2430af, CPS2_QSND | BRF_SND },
	
	{ "vhunt2.key",    0x000014, 0x61306b20, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhunt2r1)
STD_ROM_FN(Vhunt2r1)

static struct BurnRomInfo VsavRomDesc[] = {
	{ "vm3e.03d",      0x080000, 0xf5962a8c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3e.04d",      0x080000, 0x21b40ea2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.05a",       0x080000, 0x4118e00f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.06a",       0x080000, 0x2f4fd3a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.07b",       0x080000, 0xcbda91b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.08a",       0x080000, 0x6ca47259, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.09b",       0x080000, 0xf4a339e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.10b",       0x080000, 0xfffbb5b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "vsav.key",      0x000014, 0xa6e3b164, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsav)
STD_ROM_FN(Vsav)

static struct BurnRomInfo VsavaRomDesc[] = {
	{ "vm3a.03d",      0x080000, 0x44c1198f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3a.04d",      0x080000, 0x2218b781, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.05a",       0x080000, 0x4118e00f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.06a",       0x080000, 0x2f4fd3a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.07b",       0x080000, 0xcbda91b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.08a",       0x080000, 0x6ca47259, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.09b",       0x080000, 0xf4a339e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.10b",       0x080000, 0xfffbb5b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "vsava.key",     0x000014, 0x8a3520f4, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsava)
STD_ROM_FN(Vsava)

static struct BurnRomInfo VsavhRomDesc[] = {
	{ "vm3h.03a",      0x080000, 0x7cc62df8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3h.04d",      0x080000, 0xd716f3b5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.05a",       0x080000, 0x4118e00f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.06a",       0x080000, 0x2f4fd3a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.07b",       0x080000, 0xcbda91b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.08a",       0x080000, 0x6ca47259, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.09b",       0x080000, 0xf4a339e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.10b",       0x080000, 0xfffbb5b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "vsavh.key",     0x000014, 0xa7dd6409, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavh)
STD_ROM_FN(Vsavh)

static struct BurnRomInfo VsavjRomDesc[] = {
	{ "vm3j.03d",      0x080000, 0x2a2e74a4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.04d",      0x080000, 0x1c2427bc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.05a",      0x080000, 0x95ce88d5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.06b",      0x080000, 0x2c4297e0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.07b",      0x080000, 0xa38aaae7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.08a",      0x080000, 0x5773e5c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.09b",      0x080000, 0xd064f8b9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.10b",      0x080000, 0x434518e9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "vsavj.key",     0x000014, 0x36d28ab8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavj)
STD_ROM_FN(Vsavj)

static struct BurnRomInfo VsavuRomDesc[] = {
	{ "vm3u.03d",      0x080000, 0x1f295274, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3u.04d",      0x080000, 0xc46adf81, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.05a",       0x080000, 0x4118e00f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.06a",       0x080000, 0x2f4fd3a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.07b",       0x080000, 0xcbda91b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.08a",       0x080000, 0x6ca47259, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.09b",       0x080000, 0xf4a339e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.10b",       0x080000, 0xfffbb5b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "vsavu.key",     0x000014, 0xff21b9d7, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavu)
STD_ROM_FN(Vsavu)

static struct BurnRomInfo Vsav2RomDesc[] = {
	{ "vs2j.03",       0x080000, 0x89fd86b4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.04",       0x080000, 0x107c091b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.05",       0x080000, 0x61979638, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.06",       0x080000, 0xf37c5bc2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.07",       0x080000, 0x8f885809, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.08",       0x080000, 0x2018c120, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.09",       0x080000, 0xfac3c217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.10",       0x080000, 0xeb490213, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vs2.13m",       0x400000, 0x5c852f52, CPS2_GFX | BRF_GRA },
	{ "vs2.15m",       0x400000, 0xa20f58af, CPS2_GFX | BRF_GRA },
	{ "vs2.17m",       0x400000, 0x39db59ad, CPS2_GFX | BRF_GRA },
	{ "vs2.19m",       0x400000, 0x00c763a7, CPS2_GFX | BRF_GRA },
	{ "vs2.14m",       0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vs2.16m",       0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vs2.18m",       0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vs2.20m",       0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },

	{ "vs2.01",        0x020000, 0x35190139, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vs2.02",        0x020000, 0xc32dba09, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vs2.11m",       0x400000, 0xd67e47b7, CPS2_QSND | BRF_SND },
	{ "vs2.12m",       0x400000, 0x6d020a14, CPS2_QSND | BRF_SND },
	
	{ "vsav2.key",     0x000014, 0x289028ce, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsav2)
STD_ROM_FN(Vsav2)

static struct BurnRomInfo XmcotaRomDesc[] = {
	{ "xmne.03f",      0x080000, 0x5a726d13, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmne.04f",      0x080000, 0x06a83f3a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmne.05b",      0x080000, 0x87b0ed0f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmne.10b",      0x080000, 0xcb36b0a4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcota.key",    0x000014, 0x6665bbfb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcota)
STD_ROM_FN(Xmcota)

static struct BurnRomInfo Xmcotar1RomDesc[] = {
	{ "xmne.03e",      0x080000, 0xa9a09b09, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmne.04e",      0x080000, 0x52fa2106, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcota.key",    0x000014, 0x6665bbfb, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotar1)
STD_ROM_FN(Xmcotar1)

static struct BurnRomInfo XmcotaaRomDesc[] = {
	{ "xmna.03e",      0x080000, 0xf1ade6e7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmna.04e",      0x080000, 0xb5a8843d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaa.key",   0x000014, 0x3fdd2d42, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaa)
STD_ROM_FN(Xmcotaa)

static struct BurnRomInfo Xmcotaar1RomDesc[] = {
	{ "xmna.03a",      0x080000, 0x7df8b27e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmna.04a",      0x080000, 0xb44e30a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05",        0x080000, 0xc3ed62a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06",        0x080000, 0xf03c52e1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07",        0x080000, 0x325626b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08",        0x080000, 0x7194ea10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09",        0x080000, 0xae946df3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10",        0x080000, 0x32a6be1d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaa.key",   0x000014, 0x3fdd2d42, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaar1)
STD_ROM_FN(Xmcotaar1)

static struct BurnRomInfo XmcotahRomDesc[] = {
	{ "xmnh.03f",      0x080000, 0xe4b85a90, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnh.04f",      0x080000, 0x7dfe1406, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnh.05b",      0x080000, 0x87b0ed0f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnh.10b",      0x080000, 0xcb36b0a4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotah.key",   0x000014, 0xc9a45a5a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotah)
STD_ROM_FN(Xmcotah)

static struct BurnRomInfo Xmcotahr1RomDesc[] = {
	{ "xmnh.03d",      0x080000, 0x63b0a84f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnh.04d",      0x080000, 0xb1b9b727, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotah.key",   0x000014, 0xc9a45a5a, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotahr1)
STD_ROM_FN(Xmcotahr1)

static struct BurnRomInfo XmcotajRomDesc[] = {
	{ "xmnj.03e",      0x080000, 0x0df29f5f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnj.04e",      0x080000, 0x4a65833b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaj.key",   0x000014, 0xd278b4ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaj)
STD_ROM_FN(Xmcotaj)

static struct BurnRomInfo Xmcotaj1RomDesc[] = {
	{ "xmnj.03d",      0x080000, 0x79086d62, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnj.04d",      0x080000, 0x38eed613, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05",        0x080000, 0xc3ed62a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06",        0x080000, 0xf03c52e1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07",        0x080000, 0x325626b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08",        0x080000, 0x7194ea10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09",        0x080000, 0xae946df3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10",        0x080000, 0x32a6be1d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaj.key",   0x000014, 0xd278b4ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaj1)
STD_ROM_FN(Xmcotaj1)

static struct BurnRomInfo Xmcotaj2RomDesc[] = {
	{ "xmnj.03b",      0x080000, 0xc8175fb3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnj.04b",      0x080000, 0x54b3fba3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05",        0x080000, 0xc3ed62a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06",        0x080000, 0xf03c52e1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07",        0x080000, 0x325626b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08",        0x080000, 0x7194ea10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09",        0x080000, 0xae946df3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10",        0x080000, 0x32a6be1d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaj.key",   0x000014, 0xd278b4ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaj2)
STD_ROM_FN(Xmcotaj2)

static struct BurnRomInfo Xmcotaj3RomDesc[] = {
	{ "xmnj.03a",      0x080000, 0x00761611, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnj.04a",      0x080000, 0x614d3f60, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05",        0x080000, 0xc3ed62a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06",        0x080000, 0xf03c52e1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07",        0x080000, 0x325626b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08",        0x080000, 0x7194ea10, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09",        0x080000, 0xae946df3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10",        0x080000, 0x32a6be1d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaj.key",   0x000014, 0xd278b4ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotaj3)
STD_ROM_FN(Xmcotaj3)

static struct BurnRomInfo XmcotajrRomDesc[] = {
	{ "xmno.03a",      0x080000, 0x7ab19acf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.04a",      0x080000, 0x7615dd21, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.05a",      0x080000, 0x0303d672, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.06a",      0x080000, 0x332839a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.07",       0x080000, 0x6255e8d5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.08",       0x080000, 0xb8ebe77c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.09",       0x080000, 0x5440d950, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmno.10a",      0x080000, 0xb8296966, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01",        0x020000, 0x7178336e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02",        0x020000, 0x0ec58501, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotaj.key",   0x000014, 0xd278b4ac, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotajr)
STD_ROM_FN(Xmcotajr)

static struct BurnRomInfo XmcotauRomDesc[] = {
	{ "xmnu.03e",      0x080000, 0x0bafeb0e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmnu.04e",      0x080000, 0xc29bdae3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "xmcotau.key",   0x000014, 0x623d3357, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotau)
STD_ROM_FN(Xmcotau)

static struct BurnRomInfo XmvsfRomDesc[] = {
	{ "xvse.03f",      0x080000, 0xdb06413f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04f",      0x080000, 0xef015aef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsf.key",     0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsf)
STD_ROM_FN(Xmvsf)

static struct BurnRomInfo Xmvsfr1RomDesc[] = {
	{ "xvse.03d",      0x080000, 0x5ae5bd3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04d",      0x080000, 0x5eb9c02e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsf.key",     0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfr1)
STD_ROM_FN(Xmvsfr1)

static struct BurnRomInfo XmvsfaRomDesc[] = {
	{ "xvsa.03k",      0x080000, 0xd0cca7a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsa.04k",      0x080000, 0x8c8e76fd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfa.key",    0x000014, 0x44941468, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfa)
STD_ROM_FN(Xmvsfa)

static struct BurnRomInfo Xmvsfar1RomDesc[] = {
	{ "xvsa.03",       0x080000, 0x520054df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  // Missing revision letter
	{ "xvsa.04",       0x080000, 0x13086e55, CPS2_PRG_68K | BRF_ESS | BRF_PRG },  // Missing revision letter
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfa.key",    0x000014, 0x44941468, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfar1)
STD_ROM_FN(Xmvsfar1)

static struct BurnRomInfo Xmvsfar2RomDesc[] = {
	{ "xvsa.03e",      0x080000, 0x9bdde21c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsa.04e",      0x080000, 0x33300edf, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfa.key",    0x000014, 0x44941468, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfar2)
STD_ROM_FN(Xmvsfar2)

static struct BurnRomInfo Xmvsfar3RomDesc[] = {
	{ "xvsa.03d",      0x080000, 0x2b164fd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsa.04d",      0x080000, 0x2d32f039, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvsa.02",       0x020000, 0x19272e4c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfa.key",    0x000014, 0x44941468, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfar3)
STD_ROM_FN(Xmvsfar3)

static struct BurnRomInfo XmvsfbRomDesc[] = {
	{ "xvsb.03h",      0x080000, 0x05baccca, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsb.04h",      0x080000, 0xe350c755, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfb.key",    0x000014, 0xf0384798, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfb)
STD_ROM_FN(Xmvsfb)

static struct BurnRomInfo XmvsfhRomDesc[] = {
	{ "xvsh.03a",      0x080000, 0xd4fffb04, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsh.04a",      0x080000, 0x1b4ea638, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfh.key",    0x000014, 0xf632a36b, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfh)
STD_ROM_FN(Xmvsfh)

static struct BurnRomInfo XmvsfjRomDesc[] = {
	{ "xvsj.03k",      0x080000, 0x2a167526, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsj.04k",      0x080000, 0xd993436b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfj.key",    0x000014, 0x87576cda, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfj)
STD_ROM_FN(Xmvsfj)

static struct BurnRomInfo Xmvsfjr1RomDesc[] = {
	{ "xvsj.03i",      0x080000, 0xef24da96, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsj.04i",      0x080000, 0x70a59b35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfj.key",    0x000014, 0x87576cda, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfjr1)
STD_ROM_FN(Xmvsfjr1)

static struct BurnRomInfo Xmvsfjr2RomDesc[] = {
	{ "xvsj.03d",      0x080000, 0xbeb81de9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsj.04d",      0x080000, 0x23d11271, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfj.key",    0x000014, 0x87576cda, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfjr2)
STD_ROM_FN(Xmvsfjr2)

static struct BurnRomInfo Xmvsfjr3RomDesc[] = {
	{ "xvsj.03c",      0x080000, 0x180656a1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsj.04c",      0x080000, 0x5832811c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05",        0x080000, 0x030e0e1e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06",        0x080000, 0x5d04a8ff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfj.key",    0x000014, 0x87576cda, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfjr3)
STD_ROM_FN(Xmvsfjr3)

static struct BurnRomInfo XmvsfuRomDesc[] = {
	{ "xvsu.03k",      0x080000, 0x8739ef61, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsu.04k",      0x080000, 0xe11d35c1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfu.key",    0x000014, 0xeca13458, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfu)
STD_ROM_FN(Xmvsfu)

static struct BurnRomInfo Xmvsfur1RomDesc[] = {
	// US version "I" of Xmen vs Street Fighters has been dumped.
	// It is identical to US version "H" but with different labels.
//	{ "xvsu.03i",      0x080000, 0x5481155a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvsu.04i",      0x080000, 0x1e236388, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
//	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "xvsu.03h",      0x080000, 0x5481155a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsu.04h",      0x080000, 0x1e236388, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfu.key",    0x000014, 0xeca13458, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfur1)
STD_ROM_FN(Xmvsfur1)

static struct BurnRomInfo Xmvsfur2RomDesc[] = {
	{ "xvsu.03d",      0x080000, 0xbd8b152f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsu.04d",      0x080000, 0x7c7d1da3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "xmvsfu.key",    0x000014, 0xeca13458, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfur2)
STD_ROM_FN(Xmvsfur2)

// Driver functions

static INT32 MmatrixInit()
{
	Mmatrix = 1;
	
	return Cps2Init();
}

static INT32 MvscjsingInit()
{
	// The case has a volume knob, and the digital switches are missing and the slider is missing from the test screen
	Cps2DisableDigitalVolume = 1;
	
	return Cps2Init();
}

static INT32 Pzloop2Init()
{
	Pzloop2 = 1;
	
	return Cps2Init();
}

static INT32 Sfa2Init()
{
	Sfa2ObjHack = 1;
	
	return Cps2Init();
}

static INT32 Ssf2Init()
{
	INT32 nRet = Cps2Init();
	
	nCpsGfxScroll[3] = 0;
	
	return nRet;
}

static INT32 Ssf2tbInit()
{
	Ssf2tb = 1;
	
	return Ssf2Init();
}

static INT32 Ssf2tInit()
{
	INT32 nRet;
	
	Ssf2t = 1;
	
	nRet = Cps2Init();
	
	nCpsGfxScroll[3] = 0;
	
	return nRet;
}

static INT32 XmcotaInit()
{
	Xmcota = 1;
	
	return Cps2Init();
}

static INT32 DrvExit()
{
	Pzloop2 = 0;
	Sfa2ObjHack = 0;
	Ssf2t = 0;
	Ssf2tb = 0;
	Xmcota = 0;
	Mmatrix = 0;

	Cps2Volume = 39;
	Cps2DisableDigitalVolume = 0;
	
	CpsLayer1XOffs = 0;
	CpsLayer2XOffs = 0;
	CpsLayer3XOffs = 0;
	CpsLayer1YOffs = 0;
	CpsLayer2YOffs = 0;
	CpsLayer3YOffs = 0;
	
	return CpsExit();
}

// Driver Definitions

struct BurnDriver BurnDrvCps19xx = {
	"19xx", NULL, NULL, NULL, "1995",
	"19XX - the war against destiny (951207 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxRomInfo, NinexxRomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxa = {
	"19xxa", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (960104 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxaRomInfo, NinexxaRomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxar1 = {
	"19xxar1", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951207 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Ninexxar1RomInfo, Ninexxar1RomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxb = {
	"19xxb", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951218 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxbRomInfo, NinexxbRomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxh = {
	"19xxh", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951218 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxhRomInfo, NinexxhRomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxj = {
	"19xxj", "19xx", NULL, NULL, "1996",
	"19XX - the war against destiny (960104 Japan, yellow case)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxjRomInfo, NinexxjRomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxjr1 = {
	"19xxjr1", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951225 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Ninexxjr1RomInfo, Ninexxjr1RomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps19xxjr2 = {
	"19xxjr2", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951207 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Ninexxjr2RomInfo, Ninexxjr2RomName, NULL, NULL, NineXXInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps1944 = {
	"1944", NULL, NULL, NULL, "2000",
	"1944 - the loop master (000620 USA)\0", NULL, "Capcom / 8ing / Raizing", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Nine44RomInfo, Nine44RomName, NULL, NULL, Nine44InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCps1944j = {
	"1944j", "1944", NULL, NULL, "2000",
	"1944 - the loop master (000620 Japan)\0", NULL, "Capcom / 8ing / Raizing", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Nine44jRomInfo, Nine44jRomName, NULL, NULL, Nine44InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwar = {
	"armwar", NULL, NULL, NULL, "1994",
	"Armored Warriors (941024 Europe)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, ArmwarRomInfo, ArmwarRomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwarr1 = {
	"armwarr1", "armwar", NULL, NULL, "1994",
	"Armored Warriors (941011 Europe)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Armwarr1RomInfo, Armwarr1RomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwaru = {
	"armwaru", "armwar", NULL, NULL, "1994",
	"Armored Warriors (941024 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, ArmwaruRomInfo, ArmwaruRomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwaru1 = {
	"armwaru1", "armwar", NULL, NULL, "1994",
	"Armored Warriors (940920 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Armwaru1RomInfo, Armwaru1RomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPgear = {
	"pgear", "armwar", NULL, NULL, "1994",
	"Powered Gear - strategic variant armor equipment (941024 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, PgearRomInfo, PgearRomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPgearr1 = {
	"pgearr1", "armwar", NULL, NULL, "1994",
	"Powered Gear - strategic variant armor equipment (940916 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Pgearr1RomInfo, Pgearr1RomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwara = {
	"armwara", "armwar", NULL, NULL, "1994",
	"Armored Warriors (941024 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, ArmwaraRomInfo, ArmwaraRomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwarar1 = {
	"armwarar1", "armwar", NULL, NULL, "1994",
	"Armored Warriors (940920 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Armwarar1RomInfo, Armwarar1RomName, NULL, NULL, ArmwarInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvsp = {
	"avsp", NULL, NULL, NULL, "1994",
	"Alien vs Predator (940520 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvspRomInfo, AvspRomName, NULL, NULL, AvspInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvspa = {
	"avspa", "avsp", NULL, NULL, "1994",
	"Alien vs Predator (940520 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvspaRomInfo, AvspaRomName, NULL, NULL, AvspInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvsph = {
	"avsph", "avsp", NULL, NULL, "1994",
	"Alien vs Predator (940520 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvsphRomInfo, AvsphRomName, NULL, NULL, AvspInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvspj = {
	"avspj", "avsp", NULL, NULL, "1994",
	"Alien vs Predator (940520 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvspjRomInfo, AvspjRomName, NULL, NULL, AvspInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvspu = {
	"avspu", "avsp", NULL, NULL, "1994",
	"Alien vs Predator (940520 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvspuRomInfo, AvspuRomName, NULL, NULL, AvspInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsBatcir = {
	"batcir", NULL, NULL, NULL, "1997",
	"Battle Circuit (970319 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, BatcirRomInfo, BatcirRomName, NULL, NULL, BatcirInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsBatcira = {
	"batcira", "batcir", NULL, NULL, "1997",
	"Battle Circuit (970319 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, BatciraRomInfo, BatciraRomName, NULL, NULL, BatcirInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsBatcirj = {
	"batcirj", "batcir", NULL, NULL, "1997",
	"Battle Circuit (970319 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, BatcirjRomInfo, BatcirjRomName, NULL, NULL, BatcirInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsChoko = {
	"choko", NULL, NULL, NULL, "2001",
	"Choko (010820 Japan)\0", NULL, "Mitchell", "CPS2",
	L"\u9577\u6C5F (Choko 010820 Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, ChokoRomInfo, ChokoRomName, NULL, NULL, ChokoInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclub = {
	"csclub", NULL, NULL, NULL, "1997",
	"Capcom Sports Club (971017 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, CsclubRomInfo, CsclubRomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclub1 = {
	"csclub1", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, Csclub1RomInfo, Csclub1RomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCscluba = {
	"cscluba", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, CsclubaRomInfo, CsclubaRomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclubh = {
	"csclubh", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, CsclubhRomInfo, CsclubhRomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclubj = {
	"csclubj", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, CsclubjRomInfo, CsclubjRomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclubjy = {
	"csclubjy", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Japan, yellow case)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, CsclubjyRomInfo, CsclubjyRomName, NULL, NULL, CsclubInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCybots = {
	"cybots", NULL, NULL, NULL, "1995",
	"Cyberbots - fullmetal madness (950424 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, CybotsRomInfo, CybotsRomName, NULL, NULL, CybotsInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCybotsj = {
	"cybotsj", "cybots", NULL, NULL, "1995",
	"Cyberbots - fullmetal madness (950420 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, CybotsjRomInfo, CybotsjRomName, NULL, NULL, CybotsInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCybotsu = {
	"cybotsu", "cybots", NULL, NULL, "1995",
	"Cyberbots - fullmetal madness (950424 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, CybotsuRomInfo, CybotsuRomName, NULL, NULL, CybotsInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsom = {
	"ddsom", NULL, NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960619 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomRomInfo, DdsomRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomr1 = {
	"ddsomr1", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960223 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomr1RomInfo, Ddsomr1RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomr2 = {
	"ddsomr2", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960209 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomr2RomInfo, Ddsomr2RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomr3 = {
	"ddsomr3", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960208 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomr3RomInfo, Ddsomr3RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsoma = {
	"ddsoma", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960619 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomaRomInfo, DdsomaRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomar1 = {
	"ddsomar1", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960208 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomar1RomInfo, Ddsomar1RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomb = {
	"ddsomb", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960223 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsombRomInfo, DdsombRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomh = {
	"ddsomh", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960223 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomhRomInfo, DdsomhRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomj = {
	"ddsomj", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960619 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomjRomInfo, DdsomjRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomjr1 = {
	"ddsomjr1", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960206 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomjr1RomInfo, Ddsomjr1RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomjr2 = {
	"ddsomjr2", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960223 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomjr2RomInfo, Ddsomjr2RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomu = {
	"ddsomu", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960619 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomuRomInfo, DdsomuRomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomur1 = {
	"ddsomur1", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960209 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddsomur1RomInfo, Ddsomur1RomName, NULL, NULL, DdsomInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtod = {
	"ddtod", NULL, NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940412 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtodRomInfo, DdtodRomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodr1 = {
	"ddtodr1", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940113 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodr1RomInfo, Ddtodr1RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtoda = {
	"ddtoda", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940412 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtodaRomInfo, DdtodaRomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodar1 = {
	"ddtodar1", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940113 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodar1RomInfo, Ddtodar1RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodh = {
	"ddtodh", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940412 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtodhRomInfo, DdtodhRomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodhr1 = {
	"ddtodhr1", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940125 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodhr1RomInfo, Ddtodhr1RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodhr2 = {
	"ddtodhr2", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940113 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodhr2RomInfo, Ddtodhr2RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodj = {
	"ddtodj", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940412 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtodjRomInfo, DdtodjRomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodjr1 = {
	"ddtodjr1", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940125 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodjr1RomInfo, Ddtodjr1RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodjr2 = {
	"ddtodjr2", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940113 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodjr2RomInfo, Ddtodjr2RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodu = {
	"ddtodu", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940125 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtoduRomInfo, DdtoduRomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodur1 = {
	"ddtodur1", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940113 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Ddtodur1RomInfo, Ddtodur1RomName, NULL, NULL, DdtodInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDimahoo = {
	"dimahoo", NULL, NULL, NULL, "2000",
	"Dimahoo (000121 Euro)\0", NULL, "8ing / Raizing / Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, DimahooRomInfo, DimahooRomName, NULL, NULL, DimahooInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCpsGreatMahouDaiJ = {
	"gmahou", "dimahoo", NULL, NULL, "2000",
	"Great Mahou Daisakusen (000121 Japan)\0", NULL, "8ing / Raizing / Capcom", "CPS2",
	L"\u30B0\u30EC\u30FC\u30C8\u9B54\u6CD5\u5927\u4F5C\u6226 (Great Mahou Daisakusen 000121 Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GmdjRomInfo, GmdjRomName, NULL, NULL, DimahooInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCpsDimahoou = {
	"dimahoou", "dimahoo", NULL, NULL, "2000",
	"Dimahoo (000121 USA)\0", NULL, "8ing / Raizing / Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, DimahoouRomInfo, DimahoouRomName, NULL, NULL, DimahooInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCpsDstlk = {
	"dstlk", NULL, NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940705 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, DstlkRomInfo, DstlkRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDstlka = {
	"dstlka", "dstlk", NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940705 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, DstlkaRomInfo, DstlkaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDstlkh = {
	"dstlkh", "dstlk", NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940818 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, DstlkhRomInfo, DstlkhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDstlku = {
	"dstlku", "dstlk", NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940818 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, DstlkuRomInfo, DstlkuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDstlkur1 = {
	"dstlkur1", "dstlk", NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940705 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Dstlkur1RomInfo, Dstlkur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVampj = {
	"vampj", "dstlk", NULL, NULL, "1994",
	"Vampire - the night warriors (940705 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VampjRomInfo, VampjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVampja = {
	"vampja", "dstlk", NULL, NULL, "1994",
	"Vampire - the night warriors (940705 Japan, alt)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VampjaRomInfo, VampjaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVampjr1 = {
	"vampjr1", "dstlk", NULL, NULL, "1994",
	"Vampire - the night warriors (940630 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vampjr1RomInfo, Vampjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtr = {
	"ecofghtr", NULL, NULL, NULL, "1993",
	"Eco Fighters (931203 etc)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, EcofghtrRomInfo, EcofghtrRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtra = {
	"ecofghtra", "ecofghtr", NULL, NULL, "1993",
	"Eco Fighters (931203 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, EcofghtraRomInfo, EcofghtraRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtrh = {
	"ecofghtrh", "ecofghtr", NULL, NULL, "1993",
	"Eco Fighters (931203 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, EcofghtrhRomInfo, EcofghtrhRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtru = {
	"ecofghtru", "ecofghtr", NULL, NULL, "1993",
	"Eco Fighters (940215 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, EcofghtruRomInfo, EcofghtruRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtru1 = {
	"ecofghtru1", "ecofghtr", NULL, NULL, "1993",
	"Eco Fighters (931203 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, Ecofghtru1RomInfo, Ecofghtru1RomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsUecology = {
	"uecology", "ecofghtr", NULL, NULL, "1993",
	"Ultimate Ecology (931203 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, UecologyRomInfo, UecologyRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawing = {
	"gigawing", NULL, NULL, NULL, "1999",
	"Giga Wing (990222 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingRomInfo, GigawingRomName, NULL, NULL, GigawingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawinga = {
	"gigawinga", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990222 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingaRomInfo, GigawingaRomName, NULL, NULL, GigawingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawingb = {
	"gigawingb", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990222 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingbRomInfo, GigawingbRomName, NULL, NULL, GigawingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawingh = {
	"gigawingh", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990222 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawinghRomInfo, GigawinghRomName, NULL, NULL, GigawingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawingj = {
	"gigawingj", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990223 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingjRomInfo, GigawingjRomName, NULL, NULL, GigawingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2 = {
	"hsf2", NULL, NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (040202 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2RomInfo, Hsf2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2a = {
	"hsf2a", "hsf2", NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (040202 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2aRomInfo, Hsf2aRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2j = {
	"hsf2j", "hsf2", NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (040202 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2jRomInfo, Hsf2jRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2j1 = {
	"hsf2j1", "hsf2", NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (031222 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2j1RomInfo, Hsf2j1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsJyangoku = {
	"jyangoku", NULL, NULL, NULL, "1999",
	"Jyangokushi  -Haoh no Saihai- (990527 Japan)\0", NULL, "Mitchell", "CPS2",
	L"\u96C0\u570B\u5FD7 -\u8987\u738B\u306E\u91C7\u724C- (Jyangokushi 990527 Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_MAHJONG, 0,
	NULL, JyangokuRomInfo, JyangokuRomName, NULL, NULL, JyangokuInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMegaman2 = {
	"megaman2", NULL, NULL, NULL, "1996",
	"Mega Man 2 - the power fighters (960708 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Megaman2RomInfo, Megaman2RomName, NULL, NULL, Megaman2InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000 ,384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMegaman2a = {
	"megaman2a", "megaman2", NULL, NULL, "1996",
	"Mega Man 2 - the power fighters (960708 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Megaman2aRomInfo, Megaman2aRomName, NULL, NULL, Megaman2InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000 ,384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMegaman2h = {
	"megaman2h", "megaman2", NULL, NULL, "1996",
	"Mega Man 2 - the power fighters (960712 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Megaman2hRomInfo, Megaman2hRomName, NULL, NULL, Megaman2InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000 ,384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRockman2j = {
	"rockman2j", "megaman2", NULL, NULL, "1996",
	"Rockman 2 - the power fighters (960708 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Rockman2jRomInfo, Rockman2jRomName, NULL, NULL, Megaman2InputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMmancp2u = {
	"mmancp2u", "megaman", NULL, NULL, "1995",
	"Mega Man - The Power Battle (951006 USA, SAMPLE Version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Mmancp2uRomInfo, Mmancp2uRomName, NULL, NULL, Mmancp2uInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMmancp2ur1 = {
	"mmancp2ur1", "megaman", NULL, NULL, "1995",
	"Mega Man - The Power Battle (950926 USA, SAMPLE Version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Mmancp2ur1RomInfo, Mmancp2ur1RomName, NULL, NULL, Mmancp2uInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRmancp2j = {
	"rmancp2j", "megaman", NULL, NULL, "1999",
	"Rockman: The Power Battle (950922 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Rmancp2jRomInfo, Rmancp2jRomName, NULL, NULL, Mmancp2uInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMarsMatrix = {
	"mmatrix", NULL, NULL, NULL, "2000",
	"Mars Matrix (000412 USA)\0", NULL, "Capcom / Takumi", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, MmatrixRomInfo, MmatrixRomName, NULL, NULL, MmatrixInputInfo, NULL,
	MmatrixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMarsMatrixJ = {
	"mmatrixj", "mmatrix", NULL, NULL, "2000",
	"Mars Matrix (000412 Japan)\0", NULL, "Capcom / Takumi", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, MmatrixjRomInfo, MmatrixjRomName, NULL, NULL, MmatrixInputInfo, NULL,
	MmatrixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMPang = {
	"mpang", NULL, NULL, NULL, "2000",
	"Mighty! Pang (001010 Euro)\0", NULL, "Mitchell", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, MpangRomInfo, MpangRomName, NULL, NULL, MpangInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMPangr1 = {
	"mpangr1", "mpang", NULL, NULL, "2000",
	"Mighty! Pang (000925 Euro)\0", NULL, "Mitchell", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, Mpangr1RomInfo, Mpangr1RomName, NULL, NULL, MpangInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMPangu = {
	"mpangu", "mpang", NULL, NULL, "2000",
	"Mighty! Pang (001010 USA)\0", NULL, "Mitchell", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, 0,
	NULL, MpanguRomInfo, MpanguRomName, NULL, NULL, MpangInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMPangj = {
	"mpangj", "mpang", NULL, NULL, "2000",
	"Mighty! Pang (001011 Japan)\0", NULL, "Mitchell", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, MpangjRomInfo, MpangjRomName, NULL, NULL, MpangInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMsh = {
	"msh", NULL, NULL, NULL, "1995",
	"Marvel Super Heroes (951024 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshRomInfo, MshRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMsha = {
	"msha", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951024 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshaRomInfo, MshaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshb = {
	"mshb", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951117 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshbRomInfo, MshbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshh = {
	"mshh", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951117 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshhRomInfo, MshhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshj = {
	"mshj", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951117 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshjRomInfo, MshjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshjr1 = {
	"mshjr1", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951024 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Mshjr1RomInfo, Mshjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshu = {
	"mshu", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951024 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshuRomInfo, MshuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsf = {
	"mshvsf", NULL, NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfRomInfo, MshvsfRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfa = {
	"mshvsfa", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfaRomInfo, MshvsfaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfa1 = {
	"mshvsfa1", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970620 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfa1RomInfo, Mshvsfa1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfb = {
	"mshvsfb", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970827 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfbRomInfo, MshvsfbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfb1 = {
	"mshvsfb1", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfb1RomInfo, Mshvsfb1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfh = {
	"mshvsfh", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfhRomInfo, MshvsfhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfj = {
	"mshvsfj", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970707 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfjRomInfo, MshvsfjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfj1 = {
	"mshvsfj1", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970702 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfj1RomInfo, Mshvsfj1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfj2 = {
	"mshvsfj2", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfj2RomInfo, Mshvsfj2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfu = {
	"mshvsfu", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970827 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MshvsfuRomInfo, MshvsfuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfu1 = {
	"mshvsfu1", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfu1RomInfo, Mshvsfu1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvsc = {
	"mvsc", NULL, NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscRomInfo, MvscRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscr1 = {
	"mvscr1", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980112 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mvscr1RomInfo, Mvscr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvsca = {
	"mvsca", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscaRomInfo, MvscaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscar1 = {
	"mvscar1", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980112 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mvscar1RomInfo, Mvscar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscb = {
	"mvscb", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscbRomInfo, MvscbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvsch = {
	"mvsch", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvschRomInfo, MvschRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscj = {
	"mvscj", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscjRomInfo, MvscjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscjr1 = {
	"mvscjr1", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980112 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mvscjr1RomInfo, Mvscjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscu = {
	"mvscu", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscuRomInfo, MvscuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscur1 = {
	"mvscur1", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (971222 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mvscur1RomInfo, Mvscur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscjsing = {
	"mvscjsing", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 Japan, single PCB)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscjsingRomInfo, MvscjsingRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	MvscjsingInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarr = {
	"nwarr", NULL, NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950316 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarrRomInfo, NwarrRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarra = {
	"nwarra", "nwarr", NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950302 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarraRomInfo, NwarraRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarrb = {
	"nwarrb", "nwarr", NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950403 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarrbRomInfo, NwarrbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarrh = {
	"nwarrh", "nwarr", NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950403 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarrhRomInfo, NwarrhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarru = {
	"nwarru", "nwarr", NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950406 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarruRomInfo, NwarruRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhuntj = {
	"vhuntj", "nwarr", NULL, NULL, "1995",
	"Vampire Hunter - darkstalkers' revenge (950316 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VhuntjRomInfo, VhuntjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhuntjr1s = {
	"vhuntjr1s", "nwarr", NULL, NULL, "1995",
	"Vampire Hunter - darkstalkers' revenge (950307 Japan stop version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhuntjr1sRomInfo, Vhuntjr1sRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhuntjr1 = {
	"vhuntjr1", "nwarr", NULL, NULL, "1995",
	"Vampire Hunter - darkstalkers' revenge (950307 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhuntjr1RomInfo, Vhuntjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhuntjr2 = {
	"vhuntjr2", "nwarr", NULL, NULL, "1995",
	"Vampire Hunter - darkstalkers' revenge (950302 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhuntjr2RomInfo, Vhuntjr2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsProgear = {
	"progear", NULL, NULL, NULL, "2001",
	"Progear (010117 USA)\0", NULL, "Capcom / Cave", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearRomInfo, ProgearRomName, NULL, NULL, ProgearInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsProgeara = {
	"progeara", "progear", NULL, NULL, "2001",
	"Progear (010117 Asia)\0", NULL, "Capcom / Cave", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearaRomInfo, ProgearaRomName, NULL, NULL, ProgearInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsProgearj = {
	"progearj", "progear", NULL, NULL, "2001",
	"Progear No Arashi (010117 Japan)\0", NULL, "Capcom / Cave", "CPS2",
	L"\u30D7\u30ED\u30AE\u30A2\u306E\u5D50 (Progear No Arashi 010117 Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearjRomInfo, ProgearjRomName, NULL, NULL, ProgearInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPzloop2 = {
	"pzloop2", NULL, NULL, NULL, "2001",
	"Puzz Loop 2 (010302 Euro)\0", NULL, "Mitchell, distritued by Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, Pzloop2RomInfo, Pzloop2RomName, NULL, NULL, Pzloop2InputInfo, NULL,
	Pzloop2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPzloop2j = {
	"pzloop2j", "pzloop2", NULL, NULL, "2001",
	"Puzz Loop 2 (010226 Japan)\0", NULL, "Mitchell, distritued by Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, Pzloop2jRomInfo, Pzloop2jRomName, NULL, NULL, Pzloop2InputInfo, NULL,
	Pzloop2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPzloop2jr1 = {
	"pzloop2jr1", "pzloop2", NULL, NULL, "2001",
	"Puzz Loop 2 (010205 Japan)\0", NULL, "Mitchell, distritued by Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, Pzloop2jr1RomInfo, Pzloop2jr1RomName, NULL, NULL, Pzloop2InputInfo, NULL,
	Pzloop2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsQndream = {
	"qndream", NULL, NULL, NULL, "1996",
	"Quiz Nanairo Dreams - nijiirochou no kiseki (nanairo dreams 960826 Japan)\0", NULL, "Capcom", "CPS2",
	L"Quiz \u306A\u306A\u3044\u308D Dreams - \u8679\u8272\u753A\u306E\u5947\u8DE1 (Nanairo Dreams 960826 Japan)\0Quiz Nanairo Dreams - nijiirochou no kiseki\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_QUIZ, 0,
	NULL, QndreamRomInfo, QndreamRomName, NULL, NULL, QndreamInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRingdest = {
	"ringdest", NULL, NULL, NULL, "1994",
	"Ring of Destruction - slammasters II (940902 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, RingdestRomInfo, RingdestRomName, NULL, NULL, RingdestInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSmbomb = {
	"smbomb", "ringdest", NULL, NULL, "1994",
	"Super Muscle Bomber - the international blowout (940831 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, SmbombRomInfo, SmbombRomName, NULL, NULL, RingdestInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSmbombr1 = {
	"smbombr1", "ringdest", NULL, NULL, "1994",
	"Super Muscle Bomber - the international blowout (940808 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Smbombr1RomInfo, Smbombr1RomName, NULL, NULL, RingdestInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRingdesta = {
	"ringdesta", "ringdest", NULL, NULL, "1994",
	"Ring of Destruction - slammasters II (940831 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, RingdestaRomInfo, RingdestaRomName, NULL, NULL, RingdestInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRingdesth = {
	"ringdesth", "ringdest", NULL, NULL, "1994",
	"Ring of Destruction - slammasters II (940902 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, RingdesthRomInfo, RingdesthRomName, NULL, NULL, RingdestInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa = {
	"sfa", NULL, NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950727 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfaRomInfo, SfaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfar1 = {
	"sfar1", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950718 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfar1RomInfo, Sfar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfar2 = {
	"sfar2", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950627 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfar2RomInfo, Sfar2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfar3 = {
	"sfar3", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950605 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfar3RomInfo, Sfar3RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfau = {
	"sfau", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950627 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfauRomInfo, SfauRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfza = {
	"sfza", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950627 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfzaRomInfo, SfzaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzar1 = {
	"sfzar1", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950605 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfzar1RomInfo, Sfzar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzb = {
	"sfzb", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (951109 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfzbRomInfo, SfzbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzbr1 = {
	"sfzbr1", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950727 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfzbr1RomInfo, Sfzbr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzh = {
	"sfzh", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950718 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfzhRomInfo, SfzhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzhr1 = {
	"sfzhr1", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950627 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfzhr1RomInfo, Sfzhr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzj = {
	"sfzj", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950727 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfzjRomInfo, SfzjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzjr1 = {
	"sfzjr1", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950627 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfzjr1RomInfo, Sfzjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfzjr2 = {
	"sfzjr2", "sfa", NULL, NULL, "1995",
	"Street Fighter Zero (950605 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfzjr2RomInfo, Sfzjr2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa2 = {
	"sfa2", NULL, NULL, NULL, "1996",
	"Street Fighter Alpha 2 (960229 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa2RomInfo, Sfa2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa2u = {
	"sfa2u", "sfa2", NULL, NULL, "1996",
	"Street Fighter Alpha 2 (960430 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa2uRomInfo, Sfa2uRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa2ur1 = {
	"sfa2ur1", "sfa2", NULL, NULL, "1996",
	"Street Fighter Alpha 2 (960306 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa2ur1RomInfo, Sfa2ur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2a = {
	"sfz2a", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960227 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2aRomInfo, Sfz2aRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2b = {
	"sfz2b", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960531 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2bRomInfo, Sfz2bRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2br1 = {
	"sfz2br1", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960304 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2br1RomInfo, Sfz2br1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2h = {
	"sfz2h", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960304 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2hRomInfo, Sfz2hRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2j = {
	"sfz2j", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960430 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2jRomInfo, Sfz2jRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2jr1 = {
	"sfz2jr1", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960227 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2jr1RomInfo, Sfz2jr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2n = {
	"sfz2n", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960229 Oceania)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2nRomInfo, Sfz2nRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2al = {
	"sfz2al", NULL, NULL, NULL, "1996",
	"Street Fighter Zero 2 Alpha (960826 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2alRomInfo, Sfz2alRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2alb = {
	"sfz2alb", "sfz2al", NULL, NULL, "1996",
	"Street Fighter Zero 2 Alpha (960813 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2albRomInfo, Sfz2albRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2alh = {
	"sfz2alh", "sfz2al", NULL, NULL, "1996",
	"Street Fighter Zero 2 Alpha (960813 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2alhRomInfo, Sfz2alhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2alj = {
	"sfz2alj", "sfz2al", NULL, NULL, "1996",
	"Street Fighter Zero 2 Alpha (960805 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2aljRomInfo, Sfz2aljRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Sfa2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3 = {
	"sfa3", NULL, NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980904 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3RomInfo, Sfa3RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3b = {
	"sfa3b", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980629 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3bRomInfo, Sfa3bRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3h = {
	"sfa3h", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980904 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3hRomInfo, Sfa3hRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3hr1 = {
	"sfa3hr1", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980629 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3hr1RomInfo, Sfa3hr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3u = {
	"sfa3u", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980904 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3uRomInfo, Sfa3uRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3ur1 = {
	"sfa3ur1", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980629 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3ur1RomInfo, Sfa3ur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3us = {
	"sfa3us", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980616 USA, SAMPLE Version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3usRomInfo, Sfa3usRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3a = {
	"sfz3a", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980904 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3aRomInfo, Sfz3aRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3ar1 = {
	"sfz3ar1", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980701 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3ar1RomInfo, Sfz3ar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3j = {
	"sfz3j", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980904 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3jRomInfo, Sfz3jRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3jr1 = {
	"sfz3jr1", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980727 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3jr1RomInfo, Sfz3jr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3jr2 = {
	"sfz3jr2", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980629 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3jr2RomInfo, Sfz3jr2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSgemf = {
	"sgemf", NULL, NULL, NULL, "1997",
	"Super Gem Fighter Mini Mix (970904 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SgemfRomInfo, SgemfRomName, NULL, NULL, SgemfInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsPfghtj = {
	"pfghtj", "sgemf", NULL, NULL, "1997",
	"Pocket Fighter (970904 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, PfghtjRomInfo, PfghtjRomName, NULL, NULL, SgemfInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSgemfa = {
	"sgemfa", "sgemf", NULL, NULL, "1997",
	"Super Gem Fighter Mini Mix (970904 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SgemfaRomInfo, SgemfaRomName, NULL, NULL, SgemfInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSgemfh = {
	"sgemfh", "sgemf", NULL, NULL, "1997",
	"Super Gem Fighter Mini Mix (970904 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SgemfhRomInfo, SgemfhRomName, NULL, NULL, SgemfInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2t = {
	"spf2t", NULL, NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Super Puzzle Fighter 2 Turbo 960529 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2tRomInfo, Spf2tRomName, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2tu = {
	"spf2tu", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Super Puzzle Fighter 2 Turbo 960620 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2tuRomInfo, Spf2tuRomName, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2xj = {
	"spf2xj", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II X (Super Puzzle Fighter 2 X 960531 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2xjRomInfo, Spf2xjRomName, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2ta = {
	"spf2ta", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Super Puzzle Fighter 2 Turbo 960529 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2taRomInfo, Spf2taRomName, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2th = {
	"spf2th", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Super Puzzle Fighter 2 Turbo 960531 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2thRomInfo, Spf2thRomName, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2 = {
	"ssf2", NULL, NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 931005 etc)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2RomInfo, Ssf2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2r1 = {
	"ssf2r1", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 etc)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2r1RomInfo, Ssf2r1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2a = {
	"ssf2a", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 931005 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE,2,HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2aRomInfo, Ssf2aRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2ar1 = {
	"ssf2ar1", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930914 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2ar1RomInfo, Ssf2ar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2h = {
	"ssf2h", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE,2,HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2hRomInfo, Ssf2hRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2j = {
	"ssf2j", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 931005 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2jRomInfo, Ssf2jRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2jr1 = {
	"ssf2jr1", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2jr1RomInfo, Ssf2jr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2jr2 = {
	"ssf2jr2", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930910 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2jr2RomInfo, Ssf2jr2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2u = {
	"ssf2u", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2uRomInfo, Ssf2uRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tb = {
	"ssf2tb", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (931119 etc)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbRomInfo, Ssf2tbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tba = {
	"ssf2tba", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (931005 Asia)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbaRomInfo, Ssf2tbaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tbr1 = {
	"ssf2tbr1", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (930911 etc)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbr1RomInfo, Ssf2tbr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tbj = {
	"ssf2tbj", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (931005 Japan)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbjRomInfo, Ssf2tbjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tbj1 = {
	"ssf2tbj1", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (930911 Japan)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbj1RomInfo, Ssf2tbj1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tbh = {
	"ssf2tbh", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (931005 Hispanic)\0", "Linkup feature not implemented", "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbhRomInfo, Ssf2tbhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2t = {
	"ssf2t", NULL, NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 etc)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tRomInfo, Ssf2tRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2th = {
	"ssf2th", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2thRomInfo, Ssf2thRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2ta = {
	"ssf2ta", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2taRomInfo, Ssf2taRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tu = {
	"ssf2tu", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940323 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tuRomInfo, Ssf2tuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tur1 = {
	"ssf2tur1", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tur1RomInfo, Ssf2tur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2xj = {
	"ssf2xj", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II X - grand master challenge (super street fighter 2 X 940311 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2xjRomInfo, Ssf2xjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2xjr1 = {
	"ssf2xjr1", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II X - grand master challenge (super street fighter 2 X 940223 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2xjr1RomInfo, Ssf2xjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2xjr1r = {
	"ssf2xjr1r", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II X - grand master challenge (super street fighter 2 X 940223 Japan rent version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2xjr1rRomInfo, Ssf2xjr1rRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhunt2 = {
	"vhunt2", NULL, NULL, NULL, "1997",
	"Vampire Hunter 2 - darkstalkers revenge (970929 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhunt2RomInfo, Vhunt2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhunt2r1 = {
	"vhunt2r1", "vhunt2", NULL, NULL, "1997",
	"Vampire Hunter 2 - darkstalkers revenge (970913 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhunt2r1RomInfo, Vhunt2r1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsav = {
	"vsav", NULL, NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavRomInfo, VsavRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsava = {
	"vsava", "vsav", NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavaRomInfo, VsavaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsavh = {
	"vsavh", "vsav", NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavhRomInfo, VsavhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsavj = {
	"vsavj", "vsav", NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavjRomInfo, VsavjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsavu = {
	"vsavu", "vsav", NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavuRomInfo, VsavuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsav2 = {
	"vsav2", NULL, NULL, NULL, "1997",
	"Vampire Savior 2 - the lord of vampire (970913 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsav2RomInfo, Vsav2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcota = {
	"xmcota", NULL, NULL, NULL, "1995",
	"X-Men - children of the atom (950331 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotaRomInfo, XmcotaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotar1 = {
	"xmcotar1", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950105 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotar1RomInfo, Xmcotar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaa = {
	"xmcotaa", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950105 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotaaRomInfo, XmcotaaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaar1 = {
	"xmcotaar1", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (941217 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotaar1RomInfo, Xmcotaar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotah = {
	"xmcotah", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950331 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2,HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotahRomInfo, XmcotahRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotahr1 = {
	"xmcotahr1", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950105 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2,HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotahr1RomInfo, Xmcotahr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaj = {
	"xmcotaj", "xmcota", NULL, NULL, "1994",
	"X-Men - children of the atom (950105 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotajRomInfo, XmcotajRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaj1 = {
	"xmcotaj1", "xmcota", NULL, NULL, "1994",
	"X-Men - children of the atom (941222 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotaj1RomInfo, Xmcotaj1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaj2 = {
	"xmcotaj2", "xmcota", NULL, NULL, "1994",
	"X-Men - children of the atom (941219 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotaj2RomInfo, Xmcotaj2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotaj3 = {
	"xmcotaj3", "xmcota", NULL, NULL, "1994",
	"X-Men - children of the atom (941217 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotaj3RomInfo, Xmcotaj3RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotajr = {
	"xmcotajr", "xmcota", NULL, NULL, "1994",
	"X-Men - children of the atom (941208 Japan, rent version)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotajrRomInfo, XmcotajrRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotau = {
	"xmcotau", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950105 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, XmcotauRomInfo, XmcotauRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	XmcotaInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsf = {
	"xmvsf", NULL, NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfRomInfo, XmvsfRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfr1 = {
	"xmvsfr1", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960910 Euro)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfr1RomInfo, Xmvsfr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfa = {
	"xmvsfa", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961023 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfaRomInfo, XmvsfaRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfar1 = {
	"xmvsfar1", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfar1RomInfo, Xmvsfar1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfar2 = {
	"xmvsfar2", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960919 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfar2RomInfo, Xmvsfar2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfar3 = {
	"xmvsfar3", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960910 Asia)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfar3RomInfo, Xmvsfar3RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfb = {
	"xmvsfb", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961023 Brazil)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfbRomInfo, XmvsfbRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfh = {
	"xmvsfh", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 Hispanic)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfhRomInfo, XmvsfhRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfj = {
	"xmvsfj", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961023 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfjRomInfo, XmvsfjRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfjr1 = {
	"xmvsfjr1", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfjr1RomInfo, Xmvsfjr1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfjr2 = {
	"xmvsfjr2", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960910 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfjr2RomInfo, Xmvsfjr2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfjr3 = {
	"xmvsfjr3", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960909 Japan)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfjr3RomInfo, Xmvsfjr3RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfu = {
	"xmvsfu", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961023 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, XmvsfuRomInfo, XmvsfuRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfur1 = {
	"xmvsfur1", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfur1RomInfo, Xmvsfur1RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfur2 = {
	"xmvsfur2", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (960910 USA)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfur2RomInfo, Xmvsfur2RomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Phoenix sets

static struct BurnRomInfo NinexxdRomDesc[] = {
	{ "19xud.03",      0x080000, 0xf81b60e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xud.04",      0x080000, 0xcc44638c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xud.05",      0x080000, 0x33a168de, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19xud.06",      0x080000, 0xe0111282, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "19x.07",        0x080000, 0x61c0296c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "19x.13m",       0x080000, 0x427aeb18, CPS2_GFX | BRF_GRA },
	{ "19x.15m",       0x080000, 0x63bdbf54, CPS2_GFX | BRF_GRA },
	{ "19x.17m",       0x080000, 0x2dfe18b5, CPS2_GFX | BRF_GRA },
	{ "19x.19m",       0x080000, 0xcbef9579, CPS2_GFX | BRF_GRA },
	{ "19x.14m",       0x200000, 0xe916967c, CPS2_GFX | BRF_GRA },
	{ "19x.16m",       0x200000, 0x6e75f3db, CPS2_GFX | BRF_GRA },
	{ "19x.18m",       0x200000, 0x2213e798, CPS2_GFX | BRF_GRA },
	{ "19x.20m",       0x200000, 0xab9d5b96, CPS2_GFX | BRF_GRA },

	{ "19x.01",        0x020000, 0xef55195e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "19x.11m",       0x200000, 0xd38beef3, CPS2_QSND | BRF_SND },
	{ "19x.12m",       0x200000, 0xd47c96e2, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ninexxd)
STD_ROM_FN(Ninexxd)

static struct BurnRomInfo Nine44dRomDesc[] = {
	{ "nffud.03",      0x080000, 0x28E8AAE4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nff.04",        0x080000, 0xdba1C66e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nffu.05",       0x080000, 0xea813eb7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "nff.13m",       0x400000, 0xC9fca741, CPS2_GFX | BRF_GRA },
	{ "nff.15m",       0x400000, 0xf809d898, CPS2_GFX | BRF_GRA },
	{ "nff.17m",       0x400000, 0x15ba4507, CPS2_GFX | BRF_GRA },
	{ "nff.19m",       0x400000, 0x3dd41b8c, CPS2_GFX | BRF_GRA },
	{ "nff.14m",       0x100000, 0x3fe3a54b, CPS2_GFX | BRF_GRA },
	{ "nff.16m",       0x100000, 0x565cd231, CPS2_GFX | BRF_GRA },
	{ "nff.18m",       0x100000, 0x63ca5988, CPS2_GFX | BRF_GRA },
	{ "nff.20m",       0x100000, 0x21eb8f3B, CPS2_GFX | BRF_GRA },

	{ "nff.01",        0x020000, 0xd2e44318, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "nff.11m",       0x400000, 0x243e4e05, CPS2_QSND | BRF_SND },
	{ "nff.12m",       0x400000, 0x4fcf1600, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nine44d)
STD_ROM_FN(Nine44d)

static struct BurnRomInfo Nine44adRomDesc[] = {
	{ "nffuad.03",     0x080000, 0x78188e42, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nff.04",        0x080000, 0xdba1C66e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "nffu.05",       0x080000, 0xea813eb7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "nff.13m",       0x400000, 0xC9fca741, CPS2_GFX | BRF_GRA },
	{ "nff.15m",       0x400000, 0xf809d898, CPS2_GFX | BRF_GRA },
	{ "nff.17m",       0x400000, 0x15ba4507, CPS2_GFX | BRF_GRA },
	{ "nff.19m",       0x400000, 0x3dd41b8c, CPS2_GFX | BRF_GRA },
	{ "nff.14m",       0x100000, 0x3fe3a54b, CPS2_GFX | BRF_GRA },
	{ "nff.16m",       0x100000, 0x565cd231, CPS2_GFX | BRF_GRA },
	{ "nff.18m",       0x100000, 0x63ca5988, CPS2_GFX | BRF_GRA },
	{ "nff.20m",       0x100000, 0x21eb8f3B, CPS2_GFX | BRF_GRA },

	{ "nff.01",        0x020000, 0xd2e44318, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "nff.11m",       0x400000, 0x243e4e05, CPS2_QSND | BRF_SND },
	{ "nff.12m",       0x400000, 0x4fcf1600, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nine44ad)
STD_ROM_FN(Nine44ad)

static struct BurnRomInfo Armwar1dRomDesc[] = {
	{ "pwged.03b",     0x080000, 0x496bd483, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwged.04b",     0x080000, 0x9bd6a38f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwged.05a",     0x080000, 0x4c11d30f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.06",        0x080000, 0x87a60ce8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.07",        0x080000, 0xf7b148df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.08",        0x080000, 0xcc62823e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.09",        0x080000, 0xddc85ca6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pwg.10",        0x080000, 0x07c4fb28, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pwg.13m",       0x400000, 0xae8fe08e, CPS2_GFX | BRF_GRA },
	{ "pwg.15m",       0x400000, 0xdb560f58, CPS2_GFX | BRF_GRA },
	{ "pwg.17m",       0x400000, 0xbc475b94, CPS2_GFX | BRF_GRA },
	{ "pwg.19m",       0x400000, 0x07439ff7, CPS2_GFX | BRF_GRA },
	{ "pwg.14m",       0x100000, 0xc3f9ba63, CPS2_GFX | BRF_GRA },
	{ "pwg.16m",       0x100000, 0x815b0e7b, CPS2_GFX | BRF_GRA },
	{ "pwg.18m",       0x100000, 0x0109c71b, CPS2_GFX | BRF_GRA },
	{ "pwg.20m",       0x100000, 0xeb75ffbe, CPS2_GFX | BRF_GRA },

	{ "pwg.01",        0x020000, 0x18a5c0e4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pwg.02",        0x020000, 0xc9dfffa6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pwg.11m",       0x200000, 0xa78f7433, CPS2_QSND | BRF_SND },
	{ "pwg.12m",       0x200000, 0x77438ed0, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Armwar1d)
STD_ROM_FN(Armwar1d)

static struct BurnRomInfo AvspdRomDesc[] = {
	{ "avped.03d",     0x080000, 0x66aa8aad, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avped.04d",     0x080000, 0x579306c2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avp.05d",       0x080000, 0xfbfb5d7a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "avpd.06",       0x080000, 0x63094539, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "avp.13m",       0x200000, 0x8f8b5ae4, CPS2_GFX | BRF_GRA },
	{ "avp.15m",       0x200000, 0xb00280df, CPS2_GFX | BRF_GRA },
	{ "avp.17m",       0x200000, 0x94403195, CPS2_GFX | BRF_GRA },
	{ "avp.19m",       0x200000, 0xe1981245, CPS2_GFX | BRF_GRA },
	{ "avp.14m",       0x200000, 0xebba093e, CPS2_GFX | BRF_GRA },
	{ "avp.16m",       0x200000, 0xfb228297, CPS2_GFX | BRF_GRA },
	{ "avp.18m",       0x200000, 0x34fb7232, CPS2_GFX | BRF_GRA },
	{ "avp.20m",       0x200000, 0xf90baa21, CPS2_GFX | BRF_GRA },

	{ "avp.01",        0x020000, 0x2d3b4220, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	
	{ "avp.11m",       0x200000, 0x83499817, CPS2_QSND | BRF_SND },
	{ "avp.12m",       0x200000, 0xf4110d49, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Avspd)
STD_ROM_FN(Avspd)

static struct BurnRomInfo BatcirdRomDesc[] = {
	{ "btced.03",      0x080000, 0x0737db6d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btced.04",      0x080000, 0xef1a8823, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btced.05",      0x080000, 0x20bdbb14, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btced.06",      0x080000, 0xb4d8f5bc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.07",        0x080000, 0x7322d5db, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.08",        0x080000, 0x6aac85ab, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "btc.09",        0x080000, 0x1203db08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "btc.13m",       0x400000, 0xdc705bad, CPS2_GFX | BRF_GRA },
	{ "btc.15m",       0x400000, 0xe5779a3c, CPS2_GFX | BRF_GRA },
	{ "btc.17m",       0x400000, 0xb33f4112, CPS2_GFX | BRF_GRA },
	{ "btc.19m",       0x400000, 0xa6fcdb7e, CPS2_GFX | BRF_GRA },

	{ "btc.01",        0x020000, 0x1e194310, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "btc.02",        0x020000, 0x01aeb8e6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "btc.11m",       0x200000, 0xc27f2229, CPS2_QSND | BRF_SND },
	{ "btc.12m",       0x200000, 0x418a2e33, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Batcird)
STD_ROM_FN(Batcird)

static struct BurnRomInfo Csclub1dRomDesc[] = {
	{ "csce_d.03",     0x080000, 0x5aedc6e6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce_d.04",     0x080000, 0xa3d9aa25, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce_d.05",     0x080000, 0x0915c9d1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csce_d.06",     0x080000, 0x09c77d99, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "csc_d.07",      0x080000, 0x77478e25, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "csc.73",        0x080000, 0x335f07c3, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.74",        0x080000, 0xab215357, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.75",        0x080000, 0xa2367381, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.76",        0x080000, 0x728aac1f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.63",        0x080000, 0x3711b8ca, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.64",        0x080000, 0x828a06d8, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.65",        0x080000, 0x86ee4569, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.66",        0x080000, 0xc24f577f, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.83",        0x080000, 0x0750d12a, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.84",        0x080000, 0x90a92f39, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.85",        0x080000, 0xd08ab012, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.86",        0x080000, 0x41652583, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.93",        0x080000, 0xa756c7f7, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.94",        0x080000, 0xfb7ccc73, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.95",        0x080000, 0x4d014297, CPS2_GFX_SPLIT4 | BRF_GRA },
	{ "csc.96",        0x080000, 0x6754b1ef, CPS2_GFX_SPLIT4 | BRF_GRA },

	{ "csc.01",        0x020000, 0xee162111, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "csc.51",        0x080000, 0x5a52afd5, CPS2_QSND | BRF_SND },
	{ "csc.52",        0x080000, 0x1408a811, CPS2_QSND | BRF_SND },
	{ "csc.53",        0x080000, 0x4fb9f57c, CPS2_QSND | BRF_SND },
	{ "csc.54",        0x080000, 0x9a8f40ec, CPS2_QSND | BRF_SND },
	{ "csc.55",        0x080000, 0x91529a91, CPS2_QSND | BRF_SND },
	{ "csc.56",        0x080000, 0x9a345334, CPS2_QSND | BRF_SND },
	{ "csc.57",        0x080000, 0xaedc27f2, CPS2_QSND | BRF_SND },
	{ "csc.58",        0x080000, 0x2300b7b3, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Csclub1d)
STD_ROM_FN(Csclub1d)

static struct BurnRomInfo CybotsudRomDesc[] = {
	{ "cybu_d.03",     0x080000, 0xee7560fb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cybu_d.04",     0x080000, 0x7e7425a0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.05",        0x080000, 0xec40408e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.06",        0x080000, 0x1ad0bed2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.07",        0x080000, 0x6245a39a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.08",        0x080000, 0x4b48e223, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.09",        0x080000, 0xe15238f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.10",        0x080000, 0x75f4003b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "cyb.13m",       0x400000, 0xf0dce192, CPS2_GFX | BRF_GRA },
	{ "cyb.15m",       0x400000, 0x187aa39c, CPS2_GFX | BRF_GRA },
	{ "cyb.17m",       0x400000, 0x8a0e4b12, CPS2_GFX | BRF_GRA },
	{ "cyb.19m",       0x400000, 0x34b62612, CPS2_GFX | BRF_GRA },
	{ "cyb.14m",       0x400000, 0xc1537957, CPS2_GFX | BRF_GRA },
	{ "cyb.16m",       0x400000, 0x15349e86, CPS2_GFX | BRF_GRA },
	{ "cyb.18m",       0x400000, 0xd83e977d, CPS2_GFX | BRF_GRA },
	{ "cyb.20m",       0x400000, 0x77cdad5c, CPS2_GFX | BRF_GRA },

	{ "cyb.01",        0x020000, 0x9c0fb079, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "cyb.02",        0x020000, 0x51cb0c4e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "cyb.11m",       0x200000, 0x362ccab2, CPS2_QSND | BRF_SND },
	{ "cyb.12m",       0x200000, 0x7066e9cc, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cybotsud)
STD_ROM_FN(Cybotsud)

static struct BurnRomInfo CybotsjdRomDesc[] = {
	{ "cybj_d.03",     0x080000, 0x9eb34071, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cybj_d.04",     0x080000, 0xcf223cd7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.05",        0x080000, 0xec40408e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.06",        0x080000, 0x1ad0bed2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.07",        0x080000, 0x6245a39a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.08",        0x080000, 0x4b48e223, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.09",        0x080000, 0xe15238f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "cyb.10",        0x080000, 0x75f4003b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "cyb.13m",       0x400000, 0xf0dce192, CPS2_GFX | BRF_GRA },
	{ "cyb.15m",       0x400000, 0x187aa39c, CPS2_GFX | BRF_GRA },
	{ "cyb.17m",       0x400000, 0x8a0e4b12, CPS2_GFX | BRF_GRA },
	{ "cyb.19m",       0x400000, 0x34b62612, CPS2_GFX | BRF_GRA },
	{ "cyb.14m",       0x400000, 0xc1537957, CPS2_GFX | BRF_GRA },
	{ "cyb.16m",       0x400000, 0x15349e86, CPS2_GFX | BRF_GRA },
	{ "cyb.18m",       0x400000, 0xd83e977d, CPS2_GFX | BRF_GRA },
	{ "cyb.20m",       0x400000, 0x77cdad5c, CPS2_GFX | BRF_GRA },

	{ "cyb.01",        0x020000, 0x9c0fb079, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "cyb.02",        0x020000, 0x51cb0c4e, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "cyb.11m",       0x200000, 0x362ccab2, CPS2_QSND | BRF_SND },
	{ "cyb.12m",       0x200000, 0x7066e9cc, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Cybotsjd)
STD_ROM_FN(Cybotsjd)

static struct BurnRomInfo DdsomudRomDesc[] = {
	{ "dd2ud.03g",     0x080000, 0x816f695a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2ud.04g",     0x080000, 0x7cc81c6b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.05g",       0x080000, 0x5eb1991c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.06g",       0x080000, 0xc26b5e55, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.07",        0x080000, 0x909a0b8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.08",        0x080000, 0xe53c4d01, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2.09",        0x080000, 0x5f86279f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dd2d.10",       0x080000, 0x0c172f8f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dd2.13m",       0x400000, 0xa46b4e6e, CPS2_GFX | BRF_GRA },
	{ "dd2.15m",       0x400000, 0xd5fc50fc, CPS2_GFX | BRF_GRA },
	{ "dd2.17m",       0x400000, 0x837c0867, CPS2_GFX | BRF_GRA },
	{ "dd2.19m",       0x400000, 0xbb0ec21c, CPS2_GFX | BRF_GRA },
	{ "dd2.14m",       0x200000, 0x6d824ce2, CPS2_GFX | BRF_GRA },
	{ "dd2.16m",       0x200000, 0x79682ae5, CPS2_GFX | BRF_GRA },
	{ "dd2.18m",       0x200000, 0xacddd149, CPS2_GFX | BRF_GRA },
	{ "dd2.20m",       0x200000, 0x117fb0c0, CPS2_GFX | BRF_GRA },

	{ "dd2.01",        0x020000, 0x99d657e5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "dd2.02",        0x020000, 0x117a3824, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dd2.11m",       0x200000, 0x98d0c325, CPS2_QSND | BRF_SND },
	{ "dd2.12m",       0x200000, 0x5ea2e7fa, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddsomud)
STD_ROM_FN(Ddsomud)

static struct BurnRomInfo DdtoddRomDesc[] = {
	{ "daded.03c",     0x080000, 0x843330f4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "daded.04c",     0x080000, 0x306f14fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "daded.05c",     0x080000, 0x8c6b8328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dad.06a",       0x080000, 0x6225495a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "dadd.07a",      0x080000, 0x0f0df6cc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "dad.13m",       0x200000, 0xda3cb7d6, CPS2_GFX | BRF_GRA },
	{ "dad.15m",       0x200000, 0x92b63172, CPS2_GFX | BRF_GRA },
	{ "dad.17m",       0x200000, 0xb98757f5, CPS2_GFX | BRF_GRA },
	{ "dad.19m",       0x200000, 0x8121ce46, CPS2_GFX | BRF_GRA },
	{ "dad.14m",       0x100000, 0x837e6f3f, CPS2_GFX | BRF_GRA },
	{ "dad.16m",       0x100000, 0xf0916bdb, CPS2_GFX | BRF_GRA },
	{ "dad.18m",       0x100000, 0xcef393ef, CPS2_GFX | BRF_GRA },
	{ "dad.20m",       0x100000, 0x8953fe9e, CPS2_GFX | BRF_GRA },

	{ "dad.01",        0x020000, 0x3f5e2424, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "dad.11m",       0x200000, 0x0c499b67, CPS2_QSND | BRF_SND },
	{ "dad.12m",       0x200000, 0x2f0b5a4e, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ddtodd)
STD_ROM_FN(Ddtodd)

static struct BurnRomInfo DimahoudRomDesc[] = {
	{ "gmdud.03",      0x080000, 0x12888435, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.04",        0x080000, 0x37485567, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmd.05",        0x080000, 0xda269ffb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "gmdud.06",      0x080000, 0xd825efda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "gmd.13m",       0x400000, 0x80dd19f0, CPS2_GFX | BRF_GRA },
	{ "gmd.15m",       0x400000, 0xdfd93A78, CPS2_GFX | BRF_GRA },
	{ "gmd.17m",       0x400000, 0x16356520, CPS2_GFX | BRF_GRA },
	{ "gmd.19m",       0x400000, 0xdfc33031, CPS2_GFX | BRF_GRA },

	{ "gmd.01",        0x020000, 0x3f9bc985, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "gmd.02",        0x020000, 0x3fd39dde, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "gmd.11m",       0x400000, 0x06a65542, CPS2_QSND | BRF_SND },
	{ "gmd.12m",       0x400000, 0x50bc7a31, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dimahoud)
STD_ROM_FN(Dimahoud)

static struct BurnRomInfo Dstlku1dRomDesc[] = {
	{ "vamud.03a",     0x080000, 0x47b7a680, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamud.04a",     0x080000, 0x3b7a4939, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.05a",      0x080000, 0x673ed50a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.06a",      0x080000, 0xf2377be7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.07a",      0x080000, 0xd8f498c4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.08a",      0x080000, 0xe6a8a1a0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamud.09a",     0x080000, 0x8b333a19, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vamu.10a",      0x080000, 0xc1a3d9be, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vam.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vam.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vam.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vam.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vam.14m",       0x100000, 0xbd87243c, CPS2_GFX | BRF_GRA },
	{ "vam.16m",       0x100000, 0xafec855f, CPS2_GFX | BRF_GRA },
	{ "vam.18m",       0x100000, 0x3a033625, CPS2_GFX | BRF_GRA },
	{ "vam.20m",       0x100000, 0x2bff6a89, CPS2_GFX | BRF_GRA },

	{ "vam.01",        0x020000, 0x64b685d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vam.02",        0x020000, 0xcf7c97c7, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vam.11m",       0x200000, 0x4a39deb2, CPS2_QSND | BRF_SND },
	{ "vam.12m",       0x200000, 0x1a3e5c03, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Dstlku1d)
STD_ROM_FN(Dstlku1d)

static struct BurnRomInfo EcofghtrdRomDesc[] = {
	{ "ueced.03",      0x080000, 0xac725d2b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueced.04",      0x080000, 0xf800138d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueced.05",      0x080000, 0xeb6a12f2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ueced.06",      0x080000, 0x8380ec9a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "uec.13m",       0x200000, 0xdcaf1436, CPS2_GFX | BRF_GRA },
	{ "uec.15m",       0x200000, 0x2807df41, CPS2_GFX | BRF_GRA },
	{ "uec.17m",       0x200000, 0x8a708d02, CPS2_GFX | BRF_GRA },
	{ "uec.19m",       0x200000, 0xde7be0ef, CPS2_GFX | BRF_GRA },
	{ "uec.14m",       0x100000, 0x1a003558, CPS2_GFX | BRF_GRA },
	{ "uec.16m",       0x100000, 0x4ff8a6f9, CPS2_GFX | BRF_GRA },
	{ "uec.18m",       0x100000, 0xb167ae12, CPS2_GFX | BRF_GRA },
	{ "uec.20m",       0x100000, 0x1064bdc2, CPS2_GFX | BRF_GRA },

	{ "uec.01",        0x020000, 0xc235bd15, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "uec.11m",       0x200000, 0x81b25d39, CPS2_QSND | BRF_SND },
	{ "uec.12m",       0x200000, 0x27729e52, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ecofghtrd)
STD_ROM_FN(Ecofghtrd)

static struct BurnRomInfo GigawingdRomDesc[] = {
	{ "ggwu_d.03",     0x080000, 0xdde92dfa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwu_d.04",     0x080000, 0xe0509ae2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggw_d.05",      0x080000, 0x722d0042, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawingd)
STD_ROM_FN(Gigawingd)

static struct BurnRomInfo GigawingjdRomDesc[] = {
	{ "ggwjd.03a",     0x080000, 0xcb1c756e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwjd.04a",     0x080000, 0xfa158e04, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ggwjd.05a",     0x080000, 0x1c5bc4e7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ggw.13m",       0x400000, 0x105530a4, CPS2_GFX | BRF_GRA },
	{ "ggw.15m",       0x400000, 0x9e774ab9, CPS2_GFX | BRF_GRA },
	{ "ggw.17m",       0x400000, 0x466e0ba4, CPS2_GFX | BRF_GRA },
	{ "ggw.19m",       0x400000, 0x840c8dea, CPS2_GFX | BRF_GRA },

	{ "ggw.01",        0x020000, 0x4c6351d5, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ggw.11m",       0x400000, 0xe172acf5, CPS2_QSND | BRF_SND },
	{ "ggw.12m",       0x400000, 0x4bee4e8f, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Gigawingjd)
STD_ROM_FN(Gigawingjd)

static struct BurnRomInfo Hsf2dRomDesc[] = {
	{ "hs2ad.03",      0x080000, 0x0153d371, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2ad.04",      0x080000, 0x0276b78a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.05",        0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.06",        0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.07",        0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.08",        0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.09",        0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.10",        0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13m",       0x800000, 0xa6ecab17, CPS2_GFX | BRF_GRA },
	{ "hs2.15m",       0x800000, 0x10a0ae4d, CPS2_GFX | BRF_GRA },
	{ "hs2.17m",       0x800000, 0xadfa7726, CPS2_GFX | BRF_GRA },
	{ "hs2.19m",       0x800000, 0xbb3ae322, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2.11m",       0x800000, 0x0e15c359, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2d)
STD_ROM_FN(Hsf2d)

static struct BurnRomInfo Hsf2daRomDesc[] = {
	{ "hs2ad.03",      0x080000, 0x0153d371, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2ad.04",      0x080000, 0x0276b78a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.05",        0x080000, 0xdde34a35, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.06",        0x080000, 0xf4e56dda, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.07",        0x080000, 0xee4420fc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.08",        0x080000, 0xc9441533, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.09",        0x080000, 0x3fc638a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "hs2.10",        0x080000, 0x20d0f9e4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "hs2.13",        0x400000, 0x6ff7f5f1, CPS2_GFX | BRF_GRA },
	{ "hs2.15",        0x400000, 0x26da2f5d, CPS2_GFX | BRF_GRA },
	{ "hs2.17",        0x400000, 0xe769ee8e, CPS2_GFX | BRF_GRA },
	{ "hs2.19",        0x400000, 0x7f47fd07, CPS2_GFX | BRF_GRA },
	{ "hs2.14",        0x400000, 0xc549e333, CPS2_GFX | BRF_GRA },
	{ "hs2.16",        0x400000, 0x0a8541f9, CPS2_GFX | BRF_GRA },
	{ "hs2.18",        0x400000, 0xf4c92b27, CPS2_GFX | BRF_GRA },
	{ "hs2.20",        0x400000, 0xba355a75, CPS2_GFX | BRF_GRA },

	{ "hs2.01",        0x020000, 0xc1a13786, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "hs2.02",        0x020000, 0x2d8794aa, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "hs2(__hsf2da).11m", 0x400000, 0x5cb00496, CPS2_QSND | BRF_SND },
	{ "hs2.12m",       0x400000, 0x8f298007, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Hsf2da)
STD_ROM_FN(Hsf2da)

static struct BurnRomInfo Megamn2dRomDesc[] = {
	{ "rm2ud.03",      0x080000, 0xd3635f25, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2ud.04",      0x080000, 0x768a1705, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "rm2.05",        0x080000, 0x02ee9efc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "rm2.14m",       0x200000, 0x9b1f00b4, CPS2_GFX | BRF_GRA },
	{ "rm2.16m",       0x200000, 0xc2bb0c24, CPS2_GFX | BRF_GRA },
	{ "rm2.18m",       0x200000, 0x12257251, CPS2_GFX | BRF_GRA },
	{ "rm2.20m",       0x200000, 0xf9b6e786, CPS2_GFX | BRF_GRA },

	{ "rm2.01a",       0x020000, 0xd18e7859, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "rm2.02",        0x020000, 0xc463ece0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "rm2.11m",       0x200000, 0x2106174d, CPS2_QSND | BRF_SND },
	{ "rm2.12m",       0x200000, 0x546c1636, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Megamn2d)
STD_ROM_FN(Megamn2d)

static struct BurnRomInfo MmatrixdRomDesc[] = {
	{ "mmxud.03",      0x080000, 0x36711e60, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxud.04",      0x080000, 0x4687226f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mmxud.05",      0x080000, 0x52124398, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mmx.13m",       0x400000, 0x04748718, CPS2_GFX | BRF_GRA },
	{ "mmx.15m",       0x400000, 0x38074f44, CPS2_GFX | BRF_GRA },
	{ "mmx.17m",       0x400000, 0xe4635e35, CPS2_GFX | BRF_GRA },
	{ "mmx.19m",       0x400000, 0x4400a3f2, CPS2_GFX | BRF_GRA },
	{ "mmx.14m",       0x400000, 0xd52bf491, CPS2_GFX | BRF_GRA },
	{ "mmx.16m",       0x400000, 0x23f70780, CPS2_GFX | BRF_GRA },
	{ "mmx.18m",       0x400000, 0x2562c9d5, CPS2_GFX | BRF_GRA },
	{ "mmx.20m",       0x400000, 0x583a9687, CPS2_GFX | BRF_GRA },

	{ "mmx.01",        0x020000, 0xc57e8171, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mmx.11m",       0x400000, 0x4180b39f, CPS2_QSND | BRF_SND },
	{ "mmx.12m",       0x400000, 0x95e22a59, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mmatrixd)
STD_ROM_FN(Mmatrixd)

static struct BurnRomInfo MpangjdRomDesc[] = {
	{ "mpnj-pnx.03",   0x080000, 0xdac63128, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mpnj-pnx.04",   0x080000, 0xd0b2592b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mpn-simm.01c",  0x200000, 0x388DB66B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01d",  0x200000, 0xAFF1B494, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01a",  0x200000, 0xA9C4857B, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.01b",  0x200000, 0xF759DF22, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03c",  0x200000, 0xDEC6B720, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03d",  0x200000, 0xF8774C18, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03a",  0x200000, 0xC2AEA4EC, CPS2_GFX_SIMM | BRF_GRA },
	{ "mpn-simm.03b",  0x200000, 0x84D6DC33, CPS2_GFX_SIMM | BRF_GRA },

	{ "mpn.01",        0x020000, 0x90C7ADB6, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mpn-simm.05a",  0x200000, 0x318A2E21, CPS2_QSND_SIMM | BRF_SND },
	{ "mpn-simm.05b",  0x200000, 0x5462F4E8, CPS2_QSND_SIMM | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mpangjd)
STD_ROM_FN(Mpangjd)

static struct BurnRomInfo MshudRomDesc[] = {
	{ "mshud.03",      0x080000, 0xc1d8c4c6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshud.04",      0x080000, 0xe73dda16, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mshud.05",      0x080000, 0x3b493e84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.06b",       0x080000, 0x803e3fa4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.07a",       0x080000, 0xc45f8e27, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.08a",       0x080000, 0x9ca6f12c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.09a",       0x080000, 0x82ec27af, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "msh.10b",       0x080000, 0x8d931196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "msh.13m",       0x400000, 0x09d14566, CPS2_GFX | BRF_GRA },
	{ "msh.15m",       0x400000, 0xee962057, CPS2_GFX | BRF_GRA },
	{ "msh.17m",       0x400000, 0x604ece14, CPS2_GFX | BRF_GRA },
	{ "msh.19m",       0x400000, 0x94a731e8, CPS2_GFX | BRF_GRA },
	{ "msh.14m",       0x400000, 0x4197973e, CPS2_GFX | BRF_GRA },
	{ "msh.16m",       0x400000, 0x438da4a0, CPS2_GFX | BRF_GRA },
	{ "msh.18m",       0x400000, 0x4db92d94, CPS2_GFX | BRF_GRA },
	{ "msh.20m",       0x400000, 0xa2b0c6c0, CPS2_GFX | BRF_GRA },

	{ "msh.01",        0x020000, 0xc976e6f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "msh.02",        0x020000, 0xce67d0d9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "msh.11m",       0x200000, 0x37ac6d30, CPS2_QSND | BRF_SND },
	{ "msh.12m",       0x200000, 0xde092570, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshud)
STD_ROM_FN(Mshud)

static struct BurnRomInfo Mshvsfu1dRomDesc[] = {
	{ "mvsu_d.03d",    0x080000, 0x1c88bee3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvsu_d.04d",    0x080000, 0x1e8b2535, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs_d.05a",     0x080000, 0x373856fb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.06a",       0x080000, 0x959f3030, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.07b",       0x080000, 0x7f915bdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.08a",       0x080000, 0xc2813884, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.09b",       0x080000, 0x3ba08818, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvs.10b",       0x080000, 0xcf0dba98, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvs.13m",       0x400000, 0x29b05fd9, CPS2_GFX | BRF_GRA },
	{ "mvs.15m",       0x400000, 0xfaddccf1, CPS2_GFX | BRF_GRA },
	{ "mvs.17m",       0x400000, 0x97aaf4c7, CPS2_GFX | BRF_GRA },
	{ "mvs.19m",       0x400000, 0xcb70e915, CPS2_GFX | BRF_GRA },
	{ "mvs.14m",       0x400000, 0xb3b1972d, CPS2_GFX | BRF_GRA },
	{ "mvs.16m",       0x400000, 0x08aadb5d, CPS2_GFX | BRF_GRA },
	{ "mvs.18m",       0x400000, 0xc1228b35, CPS2_GFX | BRF_GRA },
	{ "mvs.20m",       0x400000, 0x366cc6c2, CPS2_GFX | BRF_GRA },

	{ "mvs.01",        0x020000, 0x68252324, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvs.02",        0x020000, 0xb34e773d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvs.11m",       0x400000, 0x86219770, CPS2_QSND | BRF_SND },
	{ "mvs.12m",       0x400000, 0xf2fd7f68, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mshvsfu1d)
STD_ROM_FN(Mshvsfu1d)

static struct BurnRomInfo MvscudRomDesc[] = {
	{ "mvcud.03d",     0x080000, 0x75cde3e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcud.04d",     0x080000, 0xb32ea484, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.05a",       0x080000, 0x2d8c8e86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.06a",       0x080000, 0x8528e1f5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvcd.07",       0x080000, 0x205293e9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.08",        0x080000, 0xbc002fcd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.09",        0x080000, 0xc67b26df, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "mvc.10",        0x080000, 0x0fdd1e26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "mvc.13m",       0x400000, 0xfa5f74bc, CPS2_GFX | BRF_GRA },
	{ "mvc.15m",       0x400000, 0x71938a8f, CPS2_GFX | BRF_GRA },
	{ "mvc.17m",       0x400000, 0x92741d07, CPS2_GFX | BRF_GRA },
	{ "mvc.19m",       0x400000, 0xbcb72fc6, CPS2_GFX | BRF_GRA },
	{ "mvc.14m",       0x400000, 0x7f1df4e4, CPS2_GFX | BRF_GRA },
	{ "mvc.16m",       0x400000, 0x90bd3203, CPS2_GFX | BRF_GRA },
	{ "mvc.18m",       0x400000, 0x67aaf727, CPS2_GFX | BRF_GRA },
	{ "mvc.20m",       0x400000, 0x8b0bade8, CPS2_GFX | BRF_GRA },

	{ "mvc.01",        0x020000, 0x41629e95, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "mvc.02",        0x020000, 0x963abf6b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "mvc.11m",       0x400000, 0x850fe663, CPS2_QSND | BRF_SND },
	{ "mvc.12m",       0x400000, 0x7ccb1896, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Mvscud)
STD_ROM_FN(Mvscud)

static struct BurnRomInfo NwarrudRomDesc[] = {
	{ "vphud.03f",     0x080000, 0x20d4d5a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphud.04c",     0x080000, 0x61be9b42, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphud.05e",     0x080000, 0x1ba906d8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.06c",      0x080000, 0x08c04cdb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.07b",      0x080000, 0xb5a5ab19, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.08b",      0x080000, 0x51bb20fb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphu.09b",      0x080000, 0x41a64205, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vphud.10b",     0x080000, 0x9619adad, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vph.13m",       0x400000, 0xc51baf99, CPS2_GFX | BRF_GRA },
	{ "vph.15m",       0x400000, 0x3ce83c77, CPS2_GFX | BRF_GRA },
	{ "vph.17m",       0x400000, 0x4f2408e0, CPS2_GFX | BRF_GRA },
	{ "vph.19m",       0x400000, 0x9ff60250, CPS2_GFX | BRF_GRA },
	{ "vph.14m",       0x400000, 0x7a0e1add, CPS2_GFX | BRF_GRA },
	{ "vph.16m",       0x400000, 0x2f41ca75, CPS2_GFX | BRF_GRA },
	{ "vph.18m",       0x400000, 0x64498eed, CPS2_GFX | BRF_GRA },
	{ "vph.20m",       0x400000, 0x17f2433f, CPS2_GFX | BRF_GRA },

	{ "vph.01",        0x020000, 0x5045dcac, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vph.02",        0x020000, 0x86b60e59, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vph.11m",       0x200000, 0xe1837d33, CPS2_QSND | BRF_SND },
	{ "vph.12m",       0x200000, 0xfbd3cd90, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Nwarrud)
STD_ROM_FN(Nwarrud)

static struct BurnRomInfo ProgearudRomDesc[] = {
	{ "pgau_d.03",     0x080000, 0xba22b9c5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pgau_d.04",     0x080000, 0xdf3927ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pga-simm.01c",  0x200000, 0x452f98b0, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01d",  0x200000, 0x9e672092, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01a",  0x200000, 0xae9ddafe, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01b",  0x200000, 0x94d72d94, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03c",  0x200000, 0x48a1886d, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03d",  0x200000, 0x172d7e37, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03a",  0x200000, 0x9ee33d98, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03b",  0x200000, 0x848dee32, CPS2_GFX_SIMM | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga-simm.05a",  0x200000, 0xc0aac80c, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.05b",  0x200000, 0x37a65d86, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06a",  0x200000, 0xd3f1e934, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06b",  0x200000, 0x8b39489a, CPS2_QSND_SIMM | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progearud)
STD_ROM_FN(Progearud)

static struct BurnRomInfo ProgearjdRomDesc[] = {
	{ "pgaj_d.03",     0x080000, 0x0271f3a3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pgaj_d.04",     0x080000, 0xbe4b7799, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pga-simm.01c",  0x200000, 0x452f98b0, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01d",  0x200000, 0x9e672092, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01a",  0x200000, 0xae9ddafe, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.01b",  0x200000, 0x94d72d94, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03c",  0x200000, 0x48a1886d, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03d",  0x200000, 0x172d7e37, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03a",  0x200000, 0x9ee33d98, CPS2_GFX_SIMM | BRF_GRA },
	{ "pga-simm.03b",  0x200000, 0x848dee32, CPS2_GFX_SIMM | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga-simm.05a",  0x200000, 0xc0aac80c, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.05b",  0x200000, 0x37a65d86, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06a",  0x200000, 0xd3f1e934, CPS2_QSND_SIMM | BRF_SND },
	{ "pga-simm.06b",  0x200000, 0x8b39489a, CPS2_QSND_SIMM | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progearjd)
STD_ROM_FN(Progearjd)

static struct BurnRomInfo RingdstdRomDesc[] = {
	{ "smbed.03b",     0x080000, 0xf6fba4cd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbed.04b",     0x080000, 0x193bc493, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbed.05b",     0x080000, 0x168cccbb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smbed.06b",     0x080000, 0x04673262, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.07",        0x080000, 0xb9a11577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "smb.08",        0x080000, 0xf931b76b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "smb.13m",       0x200000, 0xd9b2d1de, CPS2_GFX | BRF_GRA },
	{ "smb.15m",       0x200000, 0x9a766d92, CPS2_GFX | BRF_GRA },
	{ "smb.17m",       0x200000, 0x51800f0f, CPS2_GFX | BRF_GRA },
	{ "smb.19m",       0x200000, 0x35757e96, CPS2_GFX | BRF_GRA },
	{ "smb.14m",       0x200000, 0xe5bfd0e7, CPS2_GFX | BRF_GRA },
	{ "smb.16m",       0x200000, 0xc56c0866, CPS2_GFX | BRF_GRA },
	{ "smb.18m",       0x200000, 0x4ded3910, CPS2_GFX | BRF_GRA },
	{ "smb.20m",       0x200000, 0x26ea1ec5, CPS2_GFX | BRF_GRA },
	{ "smb.21m",       0x080000, 0x0a08c5fc, CPS2_GFX | BRF_GRA },
	{ "smb.23m",       0x080000, 0x0911b6c4, CPS2_GFX | BRF_GRA },
	{ "smb.25m",       0x080000, 0x82d6c4ec, CPS2_GFX | BRF_GRA },
	{ "smb.27m",       0x080000, 0x9b48678b, CPS2_GFX | BRF_GRA },

	{ "smb.01",        0x020000, 0x0abc229a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "smb.02",        0x020000, 0xd051679a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "smb.11m",       0x200000, 0xc56935f9, CPS2_QSND | BRF_SND },
	{ "smb.12m",       0x200000, 0x955b0782, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ringdstd)
STD_ROM_FN(Ringdstd)

static struct BurnRomInfo SfadRomDesc[] = {
	{ "sfzed.03d",     0x080000, 0xa1a54827, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04b",       0x080000, 0x8b73b0e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfad)
STD_ROM_FN(Sfad)

static struct BurnRomInfo SfaudRomDesc[] = {
	// is this a region hack of sfad, no original dump of 950727 USA exists currently
	{ "sfzud.03a",     0x080000, 0x9f2ff577, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.04b",       0x080000, 0x8b73b0e5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.05a",       0x080000, 0x0810544d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfz.06",        0x080000, 0x806e8f38, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	
	{ "sfz.14m",       0x200000, 0x90fefdb3, CPS2_GFX | BRF_GRA },
	{ "sfz.16m",       0x200000, 0x5354c948, CPS2_GFX | BRF_GRA },
	{ "sfz.18m",       0x200000, 0x41a1e790, CPS2_GFX | BRF_GRA },
	{ "sfz.20m",       0x200000, 0xa549df98, CPS2_GFX | BRF_GRA },

	{ "sfz.01",        0x020000, 0xffffec7d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfz.02",        0x020000, 0x45f46a08, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfz.11m",       0x200000, 0xc4b093cd, CPS2_QSND | BRF_SND },
	{ "sfz.12m",       0x200000, 0x8bdbc4b4, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfaud)
STD_ROM_FN(Sfaud)

static struct BurnRomInfo Sfz2adRomDesc[] = {
	{ "sz2ad.03a",     0x080000, 0x017f8fab, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2ad.04a",     0x080000, 0xf50e5ea2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05a",       0x080000, 0x98e8e992, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2a.07a",      0x080000, 0x0aed2494, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2ad)
STD_ROM_FN(Sfz2ad)

static struct BurnRomInfo Sfz2jdRomDesc[] = {
	{ "sz2j_d.03a",    0x080000, 0xb7325df8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j_d.04a",    0x080000, 0xa1022a3e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.05a",       0x080000, 0x98e8e992, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.06",        0x080000, 0x5b1d49c0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2j.07a",      0x080000, 0xd910b2a2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz2.08",        0x080000, 0x0fe8585d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2jd)
STD_ROM_FN(Sfz2jd)

static struct BurnRomInfo Sfz2aldRomDesc[] = {
	{ "szaad.03",      0x080000, 0x89f9483b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaad.04",      0x080000, 0xaef27ae5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.05",       0x080000, 0xf053a55e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.06",       0x080000, 0xcfc0e7a8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.07",       0x080000, 0x5feb8b20, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "szaa.08",       0x080000, 0x6eb6d412, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz2.13m",       0x400000, 0x4d1f1f22, CPS2_GFX | BRF_GRA },
	{ "sz2.15m",       0x400000, 0x19cea680, CPS2_GFX | BRF_GRA },
	{ "sz2.17m",       0x400000, 0xe01b4588, CPS2_GFX | BRF_GRA },
	{ "sz2.19m",       0x400000, 0x0feeda64, CPS2_GFX | BRF_GRA },
	{ "sz2.14m",       0x100000, 0x0560c6aa, CPS2_GFX | BRF_GRA },
	{ "sz2.16m",       0x100000, 0xae940f87, CPS2_GFX | BRF_GRA },
	{ "sz2.18m",       0x100000, 0x4bc3c8bc, CPS2_GFX | BRF_GRA },
	{ "sz2.20m",       0x100000, 0x39e674c0, CPS2_GFX | BRF_GRA },

	{ "sz2.01a",       0x020000, 0x1bc323cf, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz2.02a",       0x020000, 0xba6a5013, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz2.11m",       0x200000, 0xaa47a601, CPS2_QSND | BRF_SND },
	{ "sz2.12m",       0x200000, 0x2237bc53, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz2ald)
STD_ROM_FN(Sfz2ald)

static struct BurnRomInfo Sfa3udRomDesc[] = {
	{ "sz3ud.03c",     0x080000, 0x6db8add7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3ud.04c",     0x080000, 0xd9c65a26, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05c",       0x080000, 0x57fd0a40, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06c",       0x080000, 0xf6305f8b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07c",       0x080000, 0x6eab0f6f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08c",       0x080000, 0x910c4a3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09c",       0x080000, 0xb29e5199, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10b",       0x080000, 0xdeb2ff52, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfa3ud)
STD_ROM_FN(Sfa3ud)

static struct BurnRomInfo Sfz3jr2dRomDesc[] = {
	{ "sz3j_d.03",     0x080000, 0xb0436151, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3j_d.04",     0x080000, 0x642d8170, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.05",        0x080000, 0x9b21518a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.06",        0x080000, 0xe7a6c3a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.07",        0x080000, 0xec4c0cfd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.08",        0x080000, 0x5c7e7240, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.09",        0x080000, 0xc5589553, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sz3.10",        0x080000, 0xa9717252, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3.13m",       0x400000, 0x0f7a60d9, CPS2_GFX | BRF_GRA },
	{ "sz3.15m",       0x400000, 0x8e933741, CPS2_GFX | BRF_GRA },
	{ "sz3.17m",       0x400000, 0xd6e98147, CPS2_GFX | BRF_GRA },
	{ "sz3.19m",       0x400000, 0xf31a728a, CPS2_GFX | BRF_GRA },
	{ "sz3.14m",       0x400000, 0x5ff98297, CPS2_GFX | BRF_GRA },
	{ "sz3.16m",       0x400000, 0x52b5bdee, CPS2_GFX | BRF_GRA },
	{ "sz3.18m",       0x400000, 0x40631ed5, CPS2_GFX | BRF_GRA },
	{ "sz3.20m",       0x400000, 0x763409b4, CPS2_GFX | BRF_GRA },

	{ "sz3.01",        0x020000, 0xde810084, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sz3.02",        0x020000, 0x72445dc4, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3.11m",       0x400000, 0x1c89eed1, CPS2_QSND | BRF_SND },
	{ "sz3.12m",       0x400000, 0xf392b13a, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3jr2d)
STD_ROM_FN(Sfz3jr2d)

static struct BurnRomInfo SgemfdRomDesc[] = {
	{ "pcfud.03",      0x080000, 0x8b83674a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcfd.04",       0x080000, 0xb58f1d03, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.05",        0x080000, 0x215655f6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.06",        0x080000, 0xea6f13ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pcf.07",        0x080000, 0x5ac6d5ea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pcf.13m",       0x400000, 0x22d72ab9, CPS2_GFX | BRF_GRA },
	{ "pcf.15m",       0x400000, 0x16a4813c, CPS2_GFX | BRF_GRA },
	{ "pcf.17m",       0x400000, 0x1097e035, CPS2_GFX | BRF_GRA },
	{ "pcf.19m",       0x400000, 0xd362d874, CPS2_GFX | BRF_GRA },
	{ "pcf.14m",       0x100000, 0x0383897c, CPS2_GFX | BRF_GRA },
	{ "pcf.16m",       0x100000, 0x76f91084, CPS2_GFX | BRF_GRA },
	{ "pcf.18m",       0x100000, 0x756c3754, CPS2_GFX | BRF_GRA },
	{ "pcf.20m",       0x100000, 0x9ec9277d, CPS2_GFX | BRF_GRA },

	{ "pcf.01",        0x020000, 0x254e5f33, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pcf.02",        0x020000, 0x6902f4f9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pcf.11m",       0x400000, 0xa5dea005, CPS2_QSND | BRF_SND },
	{ "pcf.12m",       0x400000, 0x4ce235fe, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sgemfd)
STD_ROM_FN(Sgemfd)

static struct BurnRomInfo Spf2tdRomDesc[] = {
	{ "pzfu_d.03a",    0x080000, 0x7836b903, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2td)
STD_ROM_FN(Spf2td)

static struct BurnRomInfo Spf2xjdRomDesc[] = {
	{ "pzfjd.03a",     0x080000, 0x5e85ed08, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0xb80649e2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Spf2xjd)
STD_ROM_FN(Spf2xjd)

static struct BurnRomInfo Ssf2dRomDesc[] = {
	// region hack of ssf2ud?
	{ "ssfed.03",      0x080000, 0xfad5daf8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.04",      0x080000, 0x0d31af65, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.05",      0x080000, 0x75c651ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.06",      0x080000, 0x85c3ec00, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.07",      0x080000, 0x9320e350, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2d)
STD_ROM_FN(Ssf2d)

static struct BurnRomInfo Ssf2udRomDesc[] = {
	{ "ssfud.03a",     0x080000, 0xfad5daf8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfud.04a",     0x080000, 0x0d31af65, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfud.05",      0x080000, 0x75c651ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfud.06",      0x080000, 0x85c3ec00, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfud.07",      0x080000, 0x247e2504, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2ud)
STD_ROM_FN(Ssf2ud)

static struct BurnRomInfo Ssf2tbdRomDesc[] = {
	{ "ssfed.3tc",     0x080000, 0x5d86caf8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.4tc",     0x080000, 0xf6e1f98d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.5t",      0x080000, 0x75c651ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.6tb",     0x080000, 0x9adac7d7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "ssfed.7t",      0x080000, 0x84f54db3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "ssf.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "ssf.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "ssf.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "ssf.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "ssf.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "ssf.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "ssf.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "ssf.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },

	{ "ssf.01",        0x020000, 0xeb247e8c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "ssf.q01",       0x080000, 0xa6f9da5c, CPS2_QSND | BRF_SND },
	{ "ssf.q02",       0x080000, 0x8c66ae26, CPS2_QSND | BRF_SND },
	{ "ssf.q03",       0x080000, 0x695cc2ca, CPS2_QSND | BRF_SND },
	{ "ssf.q04",       0x080000, 0x9d9ebe32, CPS2_QSND | BRF_SND },
	{ "ssf.q05",       0x080000, 0x4770e7b7, CPS2_QSND | BRF_SND },
	{ "ssf.q06",       0x080000, 0x4e79c951, CPS2_QSND | BRF_SND },
	{ "ssf.q07",       0x080000, 0xcdd14313, CPS2_QSND | BRF_SND },
	{ "ssf.q08",       0x080000, 0x6f5a088c, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tbd)
STD_ROM_FN(Ssf2tbd)

static struct BurnRomInfo Ssf2tdRomDesc[] = {
	{ "sfxed.03c",     0x080000, 0xed99d850, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.04a",     0x080000, 0x38d9b364, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.05",      0x080000, 0xc63358d0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.06a",     0x080000, 0xccb29808, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.07",      0x080000, 0x61f94982, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.08",      0x080000, 0xd399c36c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxed.09",      0x080000, 0x317b5dbc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2td)
STD_ROM_FN(Ssf2td)

static struct BurnRomInfo Ssf2tadRomDesc[] = {
	{ "sfxad.03c",     0x080000, 0xe3c92ece, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.04a",     0x080000, 0x9bf3bb2e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.05",      0x080000, 0xc63358d0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.06a",     0x080000, 0xccb29808, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.07",      0x080000, 0x61f94982, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.08",      0x080000, 0xd399c36c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxad.09",      0x080000, 0x436784ae, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2tad)
STD_ROM_FN(Ssf2tad)

static struct BurnRomInfo Ssf2xjr1dRomDesc[] = {
	{ "sfxjd.03c",     0x080000, 0x316de996, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxjd.04a",     0x080000, 0x9bf3bb2e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxjd.05",      0x080000, 0xc63358d0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxjd.06a",     0x080000, 0xccb29808, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxjd.07",      0x080000, 0x61f94982, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxjd.08",      0x080000, 0xd399c36c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "sfxd.09",       0x080000, 0x0b3a6196, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sfx.13m",       0x200000, 0xcf94d275, CPS2_GFX | BRF_GRA },
	{ "sfx.15m",       0x200000, 0x5eb703af, CPS2_GFX | BRF_GRA },
	{ "sfx.17m",       0x200000, 0xffa60e0f, CPS2_GFX | BRF_GRA },
	{ "sfx.19m",       0x200000, 0x34e825c5, CPS2_GFX | BRF_GRA },
	{ "sfx.14m",       0x100000, 0xb7cc32e7, CPS2_GFX | BRF_GRA },
	{ "sfx.16m",       0x100000, 0x8376ad18, CPS2_GFX | BRF_GRA },
	{ "sfx.18m",       0x100000, 0xf5b1b336, CPS2_GFX | BRF_GRA },
	{ "sfx.20m",       0x100000, 0x459d5c6b, CPS2_GFX | BRF_GRA },
	{ "sfx.21m",       0x100000, 0xe32854af, CPS2_GFX | BRF_GRA },
	{ "sfx.23m",       0x100000, 0x760f2927, CPS2_GFX | BRF_GRA },
	{ "sfx.25m",       0x100000, 0x1ee90208, CPS2_GFX | BRF_GRA },
	{ "sfx.27m",       0x100000, 0xf814400f, CPS2_GFX | BRF_GRA },

	{ "sfx.01",        0x020000, 0xb47b8835, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "sfx.02",        0x020000, 0x0022633f, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sfx.11m",       0x200000, 0x9bdbd476, CPS2_QSND | BRF_SND },
	{ "sfx.12m",       0x200000, 0xa05e3aab, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Ssf2xjr1d)
STD_ROM_FN(Ssf2xjr1d)

static struct BurnRomInfo VsavdRomDesc[] = {
	{ "vm3ed.03d",     0x080000, 0x97d805e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3ed.04d",     0x080000, 0x5e07fdce, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.05a",       0x080000, 0x4118e00f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.06a",       0x080000, 0x2f4fd3a9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.07b",       0x080000, 0xcbda91b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.08a",       0x080000, 0x6ca47259, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.09b",       0x080000, 0xf4a339e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3.10b",       0x080000, 0xfffbb5b8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vm3.13m",       0x400000, 0xfd8a11eb, CPS2_GFX | BRF_GRA },
	{ "vm3.15m",       0x400000, 0xdd1e7d4e, CPS2_GFX | BRF_GRA },
	{ "vm3.17m",       0x400000, 0x6b89445e, CPS2_GFX | BRF_GRA },
	{ "vm3.19m",       0x400000, 0x3830fdc7, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",       0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",       0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",       0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",       0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },

	{ "vm3.01",        0x020000, 0xf778769b, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",        0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",       0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",       0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavd)
STD_ROM_FN(Vsavd)

static struct BurnRomInfo Vhunt2dRomDesc[] = {
//	{ "vh2j_d.06",     0x080000, 0xf320ea30, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // apparently a bad dump of vh2j.06, originally loaded instead of vh2j.07 which wasn't encrypted, and clearly not correct

	{ "vh2j_d.03a",    0x080000, 0x696e0157, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j_d.04a",    0x080000, 0xced9bba3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.05",       0x080000, 0xde34f624, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.06",       0x080000, 0x6a3b9897, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.07",       0x080000, 0xb021c029, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.08",       0x080000, 0xac873dff, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.09",       0x080000, 0xeaefce9c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vh2j.10",       0x080000, 0x11730952, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vh2.13m",       0x400000, 0x3b02ddaa, CPS2_GFX | BRF_GRA },
	{ "vh2.15m",       0x400000, 0x4e40de66, CPS2_GFX | BRF_GRA },
	{ "vh2.17m",       0x400000, 0xb31d00c9, CPS2_GFX | BRF_GRA },
	{ "vh2.19m",       0x400000, 0x149be3ab, CPS2_GFX | BRF_GRA },
	{ "vh2.14m",       0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vh2.16m",       0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vh2.18m",       0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vh2.20m",       0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },

	{ "vh2.01",        0x020000, 0x67b9f779, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vh2.02",        0x020000, 0xaaf15fcb, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vh2.11m",       0x400000, 0x38922efd, CPS2_QSND | BRF_SND },
	{ "vh2.12m",       0x400000, 0x6e2430af, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhunt2d)
STD_ROM_FN(Vhunt2d)

static struct BurnRomInfo Vsav2dRomDesc[] = {
//	{ "vs2j_d.03",     0x080000, 0x5ee19aee, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // bad dump?
//	{ "vs2j_d.04",     0x080000, 0x80116c47, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // bad dump?
//	{ "vs2j_d.05",     0x080000, 0xdc74a062, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // apparently a bad dump of vs2j.04, originally loaded instead of vs2j.05 which wasn't encrypted, and clearly not correct
//	{ "vs2j_d.08",     0x080000, 0x97554918, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // apparently a bad dump of vs2j.08, originally loaded instead of vs2j.08 which wasn't encrypted, and clearly not correct

	{ "vs2j_d.03",     0x080000, 0x50865f7b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j_d.04",     0x080000, 0xc3bff0e3, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.05",       0x080000, 0x61979638, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.06",       0x080000, 0xf37c5bc2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.07",       0x080000, 0x8f885809, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.08",       0x080000, 0x2018c120, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.09",       0x080000, 0xfac3c217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.10",       0x080000, 0xeb490213, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "vs2.13m",       0x400000, 0x5c852f52, CPS2_GFX | BRF_GRA },
	{ "vs2.15m",       0x400000, 0xa20f58af, CPS2_GFX | BRF_GRA },
	{ "vs2.17m",       0x400000, 0x39db59ad, CPS2_GFX | BRF_GRA },
	{ "vs2.19m",       0x400000, 0x00c763a7, CPS2_GFX | BRF_GRA },
	{ "vs2.14m",       0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vs2.16m",       0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vs2.18m",       0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vs2.20m",       0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },

	{ "vs2.01",        0x020000, 0x35190139, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vs2.02",        0x020000, 0xc32dba09, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vs2.11m",       0x400000, 0xd67e47b7, CPS2_QSND | BRF_SND },
	{ "vs2.12m",       0x400000, 0x6d020a14, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsav2d)
STD_ROM_FN(Vsav2d)

static struct BurnRomInfo Xmcotar1dRomDesc[] = {
	{ "xmned.03e",     0x080000, 0xbef56003, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmned.04e",     0x080000, 0xb1a21fa6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.05a",       0x080000, 0xac0d7759, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.06a",       0x080000, 0x1b86a328, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.07a",       0x080000, 0x2c142a44, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.08a",       0x080000, 0xf712d44f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.09a",       0x080000, 0x9241cae8, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xmn.10a",       0x080000, 0x53c0eab9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xmn.13m",       0x400000, 0xbf4df073, CPS2_GFX | BRF_GRA },
	{ "xmn.15m",       0x400000, 0x4d7e4cef, CPS2_GFX | BRF_GRA },
	{ "xmn.17m",       0x400000, 0x513eea17, CPS2_GFX | BRF_GRA },
	{ "xmn.19m",       0x400000, 0xd23897fc, CPS2_GFX | BRF_GRA },
	{ "xmn.14m",       0x400000, 0x778237b7, CPS2_GFX | BRF_GRA },
	{ "xmn.16m",       0x400000, 0x67b36948, CPS2_GFX | BRF_GRA },
	{ "xmn.18m",       0x400000, 0x015a7c4c, CPS2_GFX | BRF_GRA },
	{ "xmn.20m",       0x400000, 0x9dde2758, CPS2_GFX | BRF_GRA },

	{ "xmn.01a",       0x020000, 0x40f479ea, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xmn.02a",       0x020000, 0x39d9b5ad, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xmn.11m",       0x200000, 0xc848a6bc, CPS2_QSND | BRF_SND },
	{ "xmn.12m",       0x200000, 0x729c188f, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmcotar1d)
STD_ROM_FN(Xmcotar1d)

static struct BurnRomInfo Xmvsfu1dRomDesc[] = {
//	{ "xvsd.05a",      0x080000, 0xde347b11, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // bad dump?, originally loaded in place of xvs.05a
//	{ "xvsd.07",       0x080000, 0xf761ded7, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // bad dump?, originally loaded in place of xvs.07a
	
	{ "xvsud.03h",     0x080000, 0x4e2e76b7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvsud.04h",     0x080000, 0x290c61a7, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",       0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",       0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",        0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.08",        0x080000, 0x81929675, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.09",        0x080000, 0x9641f36b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs.13m",       0x400000, 0xf6684efd, CPS2_GFX | BRF_GRA },
	{ "xvs.15m",       0x400000, 0x29109221, CPS2_GFX | BRF_GRA },
	{ "xvs.17m",       0x400000, 0x92db3474, CPS2_GFX | BRF_GRA },
	{ "xvs.19m",       0x400000, 0x3733473c, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",       0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",       0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",       0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",       0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",        0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",        0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",       0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",       0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsfu1d)
STD_ROM_FN(Xmvsfu1d)

void __fastcall PhoenixOutputWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0xfffff0 && a <= 0xfffffb) {
		CpsFrg[a & 0x0f] = d;
		// should this value also feed through to RAM (CpsRamFF) or should I return here?
	}
	
	CpsRamFF[(a - 0xff0000) ^ 1] = d;
}

void __fastcall PhoenixOutputWriteWord(UINT32 a, UINT16 d)
{
	SEK_DEF_WRITE_WORD(3, a, d);
}

void __fastcall PhoenixSpriteWriteByte(UINT32 a, UINT8 d)
{
	// Not seen anything write here but just in case...
	INT32 Offset = a - 0x700000;
	
	CpsRam708[(Offset ^ 1) + 0x0000] = d;
	CpsRam708[(Offset ^ 1) + 0x8000] = d;
	
	bprintf(PRINT_NORMAL, _T("Phoenix Sprite Write Byte %x, %x\n"), a, d);
}

void __fastcall PhoenixSpriteWriteWord(UINT32 a, UINT16 d)
{
	UINT16 *Ram = (UINT16*)CpsRam708;
	INT32 Offset = (a - 0x700000) >> 1;
	
	Ram[Offset + 0x0000] = d;
	Ram[Offset + 0x4000] = d;
}

static INT32 PhoenixInit()
{
	INT32 nRet = Cps2Init();
	
	nCpsNumScanlines = 262;	// phoenix sets seem to be sensitive to timing??
	
	SekOpen(0);
	SekMapHandler(3, 0xff0000, 0xffffff, MAP_WRITE);
	SekSetWriteByteHandler(3, PhoenixOutputWriteByte);
	SekSetWriteWordHandler(3, PhoenixOutputWriteWord);
	SekMapHandler(4, 0x700000, 0x701fff, MAP_WRITE);
	SekSetWriteByteHandler(4, PhoenixSpriteWriteByte);
	SekSetWriteWordHandler(4, PhoenixSpriteWriteWord);
	SekClose();
	
	return nRet;
}

static INT32 MmatrixPhoenixInit()
{
	Mmatrix = 1;
	
	return PhoenixInit();
}

static INT32 Ssf2PhoenixInit()
{
	INT32 nRet = PhoenixInit();
	
	nCpsGfxScroll[3] = 0;
	
	return nRet;
}

static int Ssf2tbPhoenixInit()
{
	Ssf2tb = 1;
	
	return Ssf2PhoenixInit();
}

static INT32 Ssf2tPhoenixInit()
{
	INT32 nRet;
	
	Ssf2t = 1;
	
	nRet = PhoenixInit();
	
	nCpsGfxScroll[3] = 0;
	
	return nRet;
}

struct BurnDriver BurnDrvCps19xxd = {
	"19xxd", "19xx", NULL, NULL, "1995",
	"19XX - the war against destiny (951207 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, NinexxdRomInfo, NinexxdRomName, NULL, NULL, NineXXInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCps1944d = {
	"1944d", "1944", NULL, NULL, "2000",
	"1944 - the loop master (000620 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Nine44dRomInfo, Nine44dRomName, NULL, NULL, Nine44InputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCps1944ad = {
	"1944ad", "1944", NULL, NULL, "2000",
	"1944 - the loop master (000620 USA Phoenix Edition, alt)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, FBF_19XX,
	NULL, Nine44adRomInfo, Nine44adRomName, NULL, NULL, Nine44InputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsArmwar1d = {
	"armwar1d", "armwar", NULL, NULL, "1994",
	"Armored Warriors (941011 Europe Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, Armwar1dRomInfo, Armwar1dRomName, NULL, NULL, ArmwarInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsAvspd = {
	"avspd", "avsp", NULL, NULL, "1994",
	"Alien vs Predator (940520 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG ,3,HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, AvspdRomInfo, AvspdRomName, NULL, NULL, AvspInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsBatcird = {
	"batcird", "batcir", NULL, NULL, "1997",
	"Battle Circuit (970319 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, BatcirdRomInfo, BatcirdRomName, NULL, NULL, BatcirInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCsclub1d = {
	"csclub1d", "csclub", NULL, NULL, "1997",
	"Capcom Sports Club (970722 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_SPORTSMISC, 0,
	NULL, Csclub1dRomInfo, Csclub1dRomName, NULL, NULL, CsclubInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCybotsud = {
	"cybotsud", "cybots", NULL, NULL, "1995",
	"Cyberbots - fullmetal madness (950424 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, CybotsudRomInfo, CybotsudRomName, NULL, NULL, CybotsInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsCybotsjd = {
	"cybotsjd", "cybots", NULL, NULL, "1995",
	"Cyberbots - fullmetal madness (Japan 950424) (decrypted bootleg)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, CybotsjdRomInfo, CybotsjdRomName, NULL, NULL, CybotsInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdsomud = {
	"ddsomud", "ddsom", NULL, NULL, "1996",
	"Dungeons & Dragons - shadow over mystara (960619 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdsomudRomInfo, DdsomudRomName, NULL, NULL, DdsomInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDdtodd = {
	"ddtodd", "ddtod", NULL, NULL, "1994",
	"Dungeons & Dragons - tower of doom (940412 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_CAPCOM_CPS2, GBF_SCRFIGHT, 0,
	NULL, DdtoddRomInfo, DdtoddRomName, NULL, NULL, DdtodInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsDimahoud = {
	"dimahoud", "dimahoo", NULL, NULL, "2000",
	"Dimahoo (000121 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, DimahoudRomInfo, DimahoudRomName, NULL, NULL, DimahooInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 224, 384, 3, 4
};

struct BurnDriver BurnDrvCpsDstlku1d = {
	"dstlku1d", "dstlk", NULL, NULL, "1994",
	"Darkstalkers - the night warriors (940705 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Dstlku1dRomInfo, Dstlku1dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsEcofghtrd = {
	"ecofghtrd", "ecofghtr", NULL, NULL, "1993",
	"Eco Fighters (931203 World Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_HORSHOOT, 0,
	NULL, EcofghtrdRomInfo, EcofghtrdRomName, NULL, NULL, EcofghtrInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawingd = {
	"gigawingd", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990222 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingdRomInfo, GigawingdRomName, NULL, NULL, GigawingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsGigawingjd = {
	"gigawingjd", "gigawing", NULL, NULL, "1999",
	"Giga Wing (990223 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, GigawingjdRomInfo, GigawingjdRomName, NULL, NULL, GigawingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2d = {
	"hsf2d", "hsf2", NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (040202 Asia Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2dRomInfo, Hsf2dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsHsf2da = {
	"hsf2da", "hsf2", NULL, NULL, "2004",
	"Hyper Street Fighter II: The Anniversary Edition (040202 Asia Phoenix Edition, alt)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Hsf2daRomInfo, Hsf2daRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMegamn2d = {
	"megamn2d", "megaman2", NULL, NULL, "1996",
	"Mega Man 2 - the power fighters (960708 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Megamn2dRomInfo, Megamn2dRomName, NULL, NULL, Megaman2InputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000 ,384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMarsMatrixd = {
	"mmatrixd", "mmatrix", NULL, NULL, "2000",
	"Mars Matrix (000412 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VERSHOOT, 0,
	NULL, MmatrixdRomInfo, MmatrixdRomName, NULL, NULL, MmatrixInputInfo, NULL,
	MmatrixPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMPangjd = {
	"mpangjd", "mpang", NULL, NULL, "2000",
	"Mighty! Pang (001011 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_PUZZLE, 0,
	NULL, MpangjdRomInfo, MpangjdRomName, NULL, NULL, MpangInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshud = {
	"mshud", "msh", NULL, NULL, "1995",
	"Marvel Super Heroes (951024 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, MshudRomInfo, MshudRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMshvsfu1d = {
	"mshvsfu1d", "mshvsf", NULL, NULL, "1997",
	"Marvel Super Heroes vs Street Fighter (970625 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Mshvsfu1dRomInfo, Mshvsfu1dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsMvscud = {
	"mvscud", "mvsc", NULL, NULL, "1998",
	"Marvel vs Capcom - clash of super heroes (980123 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, MvscudRomInfo, MvscudRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsNwarrud = {
	"nwarrud", "nwarr", NULL, NULL, "1995",
	"Night Warriors - darkstalkers' revenge (950406 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, NwarrudRomInfo, NwarrudRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsProgearud = {
	"progearud", "progear", NULL, NULL, "2001",
	"Progear(010117 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearudRomInfo, ProgearudRomName, NULL, NULL, ProgearInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsProgearjd = {
	"progearjd", "progear", NULL, NULL, "2001",
	"Progear No Arashi (010117 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	L"\u30D7\u30ED\u30AE\u30A2\u306E\u5D50 (Progear No Arashi 010117 Japan Phoenix Edition)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearjdRomInfo, ProgearjdRomName, NULL, NULL, ProgearInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsRingdstd = {
	"ringdstd", "ringdest", NULL, NULL, "1994",
	"Ring of Destruction - slammasters II (940902 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, RingdstdRomInfo, RingdstdRomName, NULL, NULL, RingdestInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfad = {
	"sfad", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950727 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfadRomInfo, SfadRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfaud = {
	"sfaud", "sfa", NULL, NULL, "1995",
	"Street Fighter Alpha - warriors' dreams (950727 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SfaudRomInfo, SfaudRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2ad = {
	"sfz2ad", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960227 Asia Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2adRomInfo, Sfz2adRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2jd = {
	"sfz2jd", "sfa2", NULL, NULL, "1996",
	"Street Fighter Zero 2 (960227 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2jdRomInfo, Sfz2jdRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz2ald = {
	"sfz2ald", "sfz2al", NULL, NULL, "1996",
	"Street Fighter Zero 2 Alpha (960826 Asia Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG,2,HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz2aldRomInfo, Sfz2aldRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfa3ud = {
	"sfa3ud", "sfa3", NULL, NULL, "1998",
	"Street Fighter Alpha 3 (980904 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfa3udRomInfo, Sfa3udRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSfz3jr2d = {
	"sfz3jr2d", "sfa3", NULL, NULL, "1998",
	"Street Fighter Zero 3 (980629 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3jr2dRomInfo, Sfz3jr2dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSgemfd = {
	"sgemfd", "sgemf", NULL, NULL, "1997",
	"Super Gem Fighter Mini Mix (970904 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, SgemfdRomInfo, SgemfdRomName, NULL, NULL, SgemfInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2td = {
	"spf2td", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Super Puzzle Fighter 2 Turbo 960620 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2tdRomInfo, Spf2tdRomName, NULL, NULL, Spf2tInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSpf2xjd = {
	"spf2xjd", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II X (Super Puzzle Fighter 2 X 960531 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2xjdRomInfo, Spf2xjdRomName, NULL, NULL, Spf2tInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2d = {
	"ssf2d", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 etc Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2dRomInfo, Ssf2dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2ud = {
	"ssf2ud", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the new challengers (super street fighter 2 930911 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2udRomInfo, Ssf2udRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tbd = {
	"ssf2tbd", "ssf2", NULL, NULL, "1993",
	"Super Street Fighter II - the tournament battle (931119 etc Phoenix Edition)\0", "Linkup feature not implemented", "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tbdRomInfo, Ssf2tbdRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tbPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2td = {
	"ssf2td", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 etc Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tdRomInfo, Ssf2tdRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2tad = {
	"ssf2tad", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II Turbo (super street fighter 2 X 940223 Asia Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2tadRomInfo, Ssf2tadRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsSsf2xjr1d = {
	"ssf2xjr1d", "ssf2t", NULL, NULL, "1994",
	"Super Street Fighter II X - grand master challenge (super street fighter 2 X 940223 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Ssf2xjr1dRomInfo, Ssf2xjr1dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	Ssf2tPhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsavd = {
	"vsavd", "vsav", NULL, NULL, "1997",
	"Vampire Savior - the lord of vampire (970519 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, VsavdRomInfo, VsavdRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVhunt2d = {
	"vhunt2d", "vhunt2", NULL, NULL, "1997",
	"Vampire Hunter 2 - darkstalkers revenge (970913 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhunt2dRomInfo, Vhunt2dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsVsav2d = {
	"vsav2d", "vsav2", NULL, NULL, "1997",
	"Vampire Savior 2 - the lord of vampire (970913 Japan Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsav2dRomInfo, Vsav2dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmcotar1d = {
	"xmcotar1d", "xmcota", NULL, NULL, "1995",
	"X-Men - children of the atom (950105 Euro Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Xmcotar1dRomInfo, Xmcotar1dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsfu1d = {
	"xmvsfu1d", "xmvsf", NULL, NULL, "1996",
	"X-Men vs Street Fighter (961004 USA Phoenix Edition)\0", NULL, "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsfu1dRomInfo, Xmvsfu1dRomName, NULL, NULL, Cps2FightingInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// other bootlegs

// Progear No Arashi (010117 Japan, decrypted set)
static struct BurnRomInfo ProgearjblRomDesc[] = {
	{ "pgaj_bl.03",    0x080000, 0x4fef676c, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // this fails the rom test - bootleggers probably didn't update checksum
	{ "pgaj_bl.04",    0x080000, 0xa069bd3b, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, // this fails the rom test - bootleggers probably didn't update checksum

	{ "pga.13m",       0x400000, 0x5194c198,  CPS2_GFX | BRF_GRA },
	{ "pga.15m",       0x400000, 0xb794e83f,  CPS2_GFX | BRF_GRA },
	{ "pga.17m",       0x400000, 0x87f22918,  CPS2_GFX | BRF_GRA },
	{ "pga.19m",       0x400000, 0x65ffb45b,  CPS2_GFX | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pga.11m",       0x400000, 0xabdd224e, CPS2_QSND | BRF_SND },
	{ "pga.12m",       0x400000, 0xdac53406, CPS2_QSND | BRF_SND },
	
	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Progearjbl)
STD_ROM_FN(Progearjbl)

struct BurnDriver BurnDrvCpsProgearjbl = {
	"progearjbl", "progear", NULL, NULL, "2001",
	"Progear No Arashi (010117 Japan, decrypted set)\0", NULL, "bootleg", "CPS2",
	L"\u30D7\u30ED\u30AE\u30A2\u306E\u5D50 (Progear No Arashi 010117 Japan, decrypted set)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, ProgearjblRomInfo, ProgearjblRomName, NULL, NULL, ProgearInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Halfway To Hell: Progear Red Label (2016-1-17 Red label ver)
static struct BurnRomInfo HalfwayRomDesc[] = {
	{ "halfway.03",    0x080000, 0x55ce8d4a, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, 
	{ "tohell.04",     0x080000, 0x71060b9e, CPS2_PRG_68K | BRF_ESS | BRF_PRG }, 

	{ "redlabel.13m",  0x400000, 0x5194c198,  CPS2_GFX | BRF_GRA },
	{ "redlabel.15m",  0x400000, 0xb794e83f,  CPS2_GFX | BRF_GRA },
	{ "redlabel.17m",  0x400000, 0x87f22918,  CPS2_GFX | BRF_GRA },
	{ "redlabel.19m",  0x400000, 0x65ffb45b,  CPS2_GFX | BRF_GRA },

	{ "pga.01",        0x020000, 0xbdbfa992, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "redlabel.11m",  0x400000, 0x33ebf625, CPS2_QSND | BRF_SND },
	{ "redlabel.12m",  0x400000, 0x47f25cf4, CPS2_QSND | BRF_SND },

	{ "phoenix.key",   0x000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },	
};

STD_ROM_PICK(Halfway)
STD_ROM_FN(Halfway)

struct BurnDriver BurnDrvCpsHalfway = {
	"halfway", "progear", NULL, NULL, "2016",
	"Halfway To Hell: Progear Red Label (2016-1-17 Red label ver)\0", NULL, "The Halfway House", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM, GBF_HORSHOOT, 0,
	NULL, HalfwayRomInfo, HalfwayRomName, NULL, NULL, ProgearInputInfo, NULL,
	PhoenixInit, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Gigaman 2: The Power Fighters (bootleg of Megaman 2)
static struct BurnRomInfo Gigaman2RomDesc[] = {
	{ "sys_rom1.bin",  0x400000, 0x2eaa5e10, BRF_ESS | BRF_PRG },

	{ "cg_rom1.bin",   0x800000, 0xed55a641, BRF_GRA },
	{ "cg_rom2.bin",   0x800000, 0x63918c05, BRF_GRA },

	{ "pcm_rom1.bin",  0x800000, 0x41a854ab, BRF_SND },
	
	{ "89c4051.bin",   0x010000, 0x00000000, BRF_PRG | BRF_NODUMP }, // sound MCU
};

STD_ROM_PICK(Gigaman2)
STD_ROM_FN(Gigaman2)

static UINT8 *Gigaman2DummyQsndRam = NULL;

static INT32 Gigaman2Init()
{
	Cps = 2;
	Cps2DisableQSnd = 1;
	
	CpsLayer1XOffs = -0x09;
	CpsLayer2XOffs = -0x09;
	CpsLayer3XOffs = -0x09;
	
	nCpsGfxLen  = 0x800000;
	nCpsRomLen  = 0x180000;
	nCpsCodeLen = 0x180000;
	nCpsZRomLen = 0;
	nCpsQSamLen = 0;
	nCpsAdLen   = 0x800000;
	
	Gigaman2DummyQsndRam = (UINT8*)BurnMalloc(0x20000);
	
	CpsInit();
	
	INT32 nRet = 0;
	
	// Load program rom (seperate data and code)
	UINT8 *pTemp = (UINT8*)BurnMalloc(0x400000);
	if (!pTemp) return 1;
	nRet = BurnLoadRom(pTemp, 0, 1); if (nRet) return 1;
	memcpy(CpsRom , pTemp + 0x000000, 0x180000);
	memcpy(CpsCode, pTemp + 0x200000, 0x180000);
	BurnFree(pTemp);
	
	// Load graphic roms, descramble and decode
	pTemp = (UINT8*)BurnMalloc(0xc00000);
	if (!pTemp) return 1;
	// we are only interested in the first 0x400000 of each rom
	nRet = BurnLoadRom(pTemp + 0x000000, 1, 1); if (nRet) return 1;
	nRet = BurnLoadRom(pTemp + 0x400000, 2, 1); if (nRet) return 1;
	
	// copy to CpsGfx as a temp buffer and descramble
	memcpy(CpsGfx, pTemp, nCpsGfxLen);
	memset(pTemp, 0, 0xc00000);
	UINT16 *pTemp16 = (UINT16*)pTemp;
	UINT16 *CpsGfx16 = (UINT16*)CpsGfx;
	for (INT32 i = 0; i < 0x800000 >> 1; i++) {
		pTemp16[i] = CpsGfx16[((i & ~7) >> 2) | ((i & 4) << 18) | ((i & 2) >> 1) | ((i & 1) << 21)];
	}
	
	// copy back to CpsGfx as a temp buffer and put into a format easier to decode
	memcpy(CpsGfx, pTemp, nCpsGfxLen);
	memset(pTemp, 0, 0xc00000);
	for (INT32 i = 0; i < 0x100000; i++) {
		pTemp16[i + 0x000000] = CpsGfx16[(i * 4) + 0];
		pTemp16[i + 0x100000] = CpsGfx16[(i * 4) + 1];
		pTemp16[i + 0x200000] = CpsGfx16[(i * 4) + 2];
		pTemp16[i + 0x300000] = CpsGfx16[(i * 4) + 3];
	}
	
	// clear CpsGfx and finally decode
	memset(CpsGfx, 0, nCpsGfxLen);	
	Cps2LoadTilesGigaman2(CpsGfx, pTemp);
	BurnFree(pTemp);
	
	// Load the MSM6295 Data
	nRet = BurnLoadRom(CpsAd, 3, 1); if (nRet) return 1;
	
	nRet = CpsRunInit();
	
	SekOpen(0);
	SekMapMemory(Gigaman2DummyQsndRam, 0x618000, 0x619fff, MAP_RAM);
	SekClose();
	
//	nCpsNumScanlines = 262;	// phoenix sets seem to be sensitive to timing??
	
	return nRet;
}

static INT32 Gigaman2Exit()
{
	BurnFree(Gigaman2DummyQsndRam);
	
	return DrvExit();
}

static INT32 Gigaman2Scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data   = Gigaman2DummyQsndRam;
		ba.nLen   = 0x020000;
		ba.szName = "Gigaman2DummyQsndRam";
		BurnAcb(&ba);
	}

	return CpsAreaScan(nAction, pnMin);
}

struct BurnDriver BurnDrvCpsGigaman2 = {
	"gigaman2", "megaman2", NULL, NULL, "1996",
	"Gigaman 2: The Power Fighters (bootleg)\0", "No Sound (MCU not dumped)", "bootleg", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, 0,
	NULL, Gigaman2RomInfo, Gigaman2RomName, NULL, NULL, Megaman2InputInfo, NULL,
	Gigaman2Init, Gigaman2Exit, Cps2Frame, CpsRedraw, Gigaman2Scan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};
