#include "sys16.h"

/*====================================================
Input defs
====================================================*/

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static INT16 Analog[6]; // wwally trackball

static struct BurnInputInfo System18InputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , System16InputPort1 + 2, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , System16InputPort2 + 2, "p2 fire 3" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Service 2"         , BIT_DIGITAL  , System16InputPort0 + 6 , "service2" },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(System18)

static struct BurnInputInfo AstormInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 7, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , System16InputPort1 + 2, "p1 fire 3" },
	{"P1 Service"        , BIT_DIGITAL  , System16InputPort1 + 3, "p1 fire 4" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , System16InputPort2 + 2, "p2 fire 3" },
	{"P2 Service"        , BIT_DIGITAL  , System16InputPort2 + 3, "p2 fire 4" },

	{"P3 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , System16InputPort0 + 6, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , System16InputPort3 + 5, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , System16InputPort3 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , System16InputPort3 + 7, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , System16InputPort3 + 6, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , System16InputPort3 + 0, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , System16InputPort3 + 1, "p3 fire 2" },
	{"P3 Fire 3"         , BIT_DIGITAL  , System16InputPort3 + 2, "p3 fire 3" },
	{"P3 Service"        , BIT_DIGITAL  , System16InputPort3 + 3, "p3 fire 4" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Astorm)

static struct BurnInputInfo DdcrewInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },

	{"P3 Coin"           , BIT_DIGITAL  , System16InputPort0 + 7, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , System16InputPort3 + 3, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , System16InputPort3 + 5, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , System16InputPort3 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , System16InputPort3 + 7, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , System16InputPort3 + 6, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , System16InputPort3 + 0, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , System16InputPort3 + 1, "p3 fire 2" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Ddcrew)

static struct BurnInputInfo Ddcrew2InputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Ddcrew2)

static struct BurnInputInfo DdcrewuInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },

	{"P3 Coin"           , BIT_DIGITAL  , System16InputPort5 + 2, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , System16InputPort5 + 0, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , System16InputPort3 + 5, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , System16InputPort3 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , System16InputPort3 + 7, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , System16InputPort3 + 6, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , System16InputPort3 + 0, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , System16InputPort3 + 1, "p3 fire 2" },

	{"P4 Coin"           , BIT_DIGITAL  , System16InputPort5 + 3, "p4 coin"   },
	{"P4 Start"          , BIT_DIGITAL  , System16InputPort5 + 1, "p4 start"  },
	{"P4 Up"             , BIT_DIGITAL  , System16InputPort4 + 5, "p4 up"     },
	{"P4 Down"           , BIT_DIGITAL  , System16InputPort4 + 4, "p4 down"   },
	{"P4 Left"           , BIT_DIGITAL  , System16InputPort4 + 7, "p4 left"   },
	{"P4 Right"          , BIT_DIGITAL  , System16InputPort4 + 6, "p4 right"  },
	{"P4 Fire 1"         , BIT_DIGITAL  , System16InputPort4 + 0, "p4 fire 1" },
	{"P4 Fire 2"         , BIT_DIGITAL  , System16InputPort4 + 1, "p4 fire 2" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Ddcrewu)

static struct BurnInputInfo DesertbrInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },
	{"P1 Fire 3"         , BIT_DIGITAL  , System16InputPort1 + 2, "p1 fire 3" },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },
	{"P2 Fire 3"         , BIT_DIGITAL  , System16InputPort2 + 2, "p2 fire 3" },

	{"P3 Coin"           , BIT_DIGITAL  , System16InputPort3 + 3, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , System16InputPort0 + 7, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , System16InputPort3 + 5, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , System16InputPort3 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , System16InputPort3 + 7, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , System16InputPort3 + 6, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , System16InputPort3 + 0, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , System16InputPort3 + 1, "p3 fire 2" },
	{"P3 Fire 3"         , BIT_DIGITAL  , System16InputPort3 + 2, "p3 fire 3" },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Desertbr)

static struct BurnInputInfo LghostInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL   , System16InputPort0 + 0, "p1 coin"    },
	A("P1 X-Axis"        , BIT_ANALOG_REL, &System16AnalogPort0,   "mouse x-axis"   ),
	A("P1 Y-Axis"        , BIT_ANALOG_REL, &System16AnalogPort1,   "mouse y-axis"   ),
	{"P1 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 0, "mouse button 1" },
	{"P1 Fire 2"         , BIT_DIGITAL   , System16InputPort1 + 1, "mouse button 2" },

	{"P2 Coin"           , BIT_DIGITAL   , System16InputPort0 + 1, "p2 coin"    },
	A("P2 X-Axis"        , BIT_ANALOG_REL, &System16AnalogPort2,   "p2 x-axis"  ),
	A("P2 Y-Axis"        , BIT_ANALOG_REL, &System16AnalogPort3,   "p2 y-axis"  ),
	{"P2 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 2, "p2 fire 1"  },
	{"P2 Fire 2"         , BIT_DIGITAL   , System16InputPort1 + 3, "p2 fire 2"  },

	{"P3 Coin"           , BIT_DIGITAL   , System16InputPort0 + 7, "p3 coin"    },
	A("P3 X-Axis"        , BIT_ANALOG_REL, &System16AnalogPort4,   "p3 x-axis"  ),
	A("P3 Y-Axis"        , BIT_ANALOG_REL, &System16AnalogPort5,   "p3 y-axis"  ),
	{"P3 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 7, "p3 fire 1"  },
	{"P3 Fire 2"         , BIT_DIGITAL   , System16InputPort1 + 6, "p3 fire 2"  },

	{"Service"           , BIT_DIGITAL   , System16InputPort0 + 4 , "service"   },
	{"Service 2"         , BIT_DIGITAL   , System16InputPort0 + 5 , "service2"  },
	{"Service 3"         , BIT_DIGITAL   , System16InputPort0 + 6 , "service3"  },
	{"Diagnostics"       , BIT_DIGITAL   , System16InputPort0 + 2 , "diag"      },
	{"Reset"             , BIT_DIGITAL   , &System16Reset         , "reset"     },
	{"Dip 1"             , BIT_DIPSWITCH , System16Dip + 0        , "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH , System16Dip + 1        , "dip"       },
};

STDINPUTINFO(Lghost)

static struct BurnInputInfo MwalkInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , System16InputPort0 + 7, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL  , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL  , System16InputPort1 + 5, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL  , System16InputPort1 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL  , System16InputPort1 + 7, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL  , System16InputPort1 + 6, "p1 right"  },
	{"P1 Fire 1"         , BIT_DIGITAL  , System16InputPort1 + 0, "p1 fire 1" },
	{"P1 Fire 2"         , BIT_DIGITAL  , System16InputPort1 + 1, "p1 fire 2" },
	{"P1 Service"        , BIT_DIGITAL  , System16InputPort1 + 3, "service"   },

	{"P2 Coin"           , BIT_DIGITAL  , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL  , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL  , System16InputPort2 + 5, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL  , System16InputPort2 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL  , System16InputPort2 + 7, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL  , System16InputPort2 + 6, "p2 right"  },
	{"P2 Fire 1"         , BIT_DIGITAL  , System16InputPort2 + 0, "p2 fire 1" },
	{"P2 Fire 2"         , BIT_DIGITAL  , System16InputPort2 + 1, "p2 fire 2" },
	{"P2 Service"        , BIT_DIGITAL  , System16InputPort2 + 3, "service2"  },

	{"P3 Coin"           , BIT_DIGITAL  , System16InputPort0 + 0, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL  , System16InputPort3 + 3, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL  , System16InputPort3 + 5, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL  , System16InputPort3 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL  , System16InputPort3 + 7, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL  , System16InputPort3 + 6, "p3 right"  },
	{"P3 Fire 1"         , BIT_DIGITAL  , System16InputPort3 + 0, "p3 fire 1" },
	{"P3 Fire 2"         , BIT_DIGITAL  , System16InputPort3 + 1, "p3 fire 2" },
	{"P3 Service"        , BIT_DIGITAL  , System16InputPort0 + 6, "service3"  },

	{"Service"           , BIT_DIGITAL  , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL  , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL  , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH, System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH, System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Mwalk)

static struct BurnInputInfo WwallyInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL   , System16InputPort0 + 0, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL   , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL   , System16InputPort4 + 3, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL   , System16InputPort4 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL   , System16InputPort4 + 1, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL   , System16InputPort4 + 2, "p1 right"  },
	A("P1 Trackball X",	BIT_ANALOG_REL, &Analog[0],    	"p1 x-axis" ),
	A("P1 Trackball Y",	BIT_ANALOG_REL, &Analog[1],    	"p1 y-axis" ),
	{"P1 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 0, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL   , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL   , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL   , System16InputPort5 + 3, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL   , System16InputPort5 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL   , System16InputPort5 + 1, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL   , System16InputPort5 + 2, "p2 right"  },
	A("P2 Trackball X",	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Trackball Y",	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),
	{"P2 Fire 1"         , BIT_DIGITAL   , System16InputPort2 + 0, "p2 fire 1" },

	{"Service"           , BIT_DIGITAL   , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL   , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL   , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH , System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH , System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Wwally)

static struct BurnInputInfo Wwally3pInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL   , System16InputPort0 + 6, "p1 coin"   },
	{"P1 Start"          , BIT_DIGITAL   , System16InputPort0 + 4, "p1 start"  },
	{"P1 Up"             , BIT_DIGITAL   , System16InputPort4 + 3, "p1 up"     },
	{"P1 Down"           , BIT_DIGITAL   , System16InputPort4 + 4, "p1 down"   },
	{"P1 Left"           , BIT_DIGITAL   , System16InputPort4 + 1, "p1 left"   },
	{"P1 Right"          , BIT_DIGITAL   , System16InputPort4 + 2, "p1 right"  },
	A("P1 Trackball X",	BIT_ANALOG_REL, &Analog[0],    	"p1 x-axis" ),
	A("P1 Trackball Y",	BIT_ANALOG_REL, &Analog[1],    	"p1 y-axis" ),
	{"P1 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 0, "p1 fire 1" },

	{"P2 Coin"           , BIT_DIGITAL   , System16InputPort0 + 1, "p2 coin"   },
	{"P2 Start"          , BIT_DIGITAL   , System16InputPort0 + 5, "p2 start"  },
	{"P2 Up"             , BIT_DIGITAL   , System16InputPort5 + 3, "p2 up"     },
	{"P2 Down"           , BIT_DIGITAL   , System16InputPort5 + 4, "p2 down"   },
	{"P2 Left"           , BIT_DIGITAL   , System16InputPort5 + 1, "p2 left"   },
	{"P2 Right"          , BIT_DIGITAL   , System16InputPort5 + 2, "p2 right"  },
	A("P2 Trackball X",	BIT_ANALOG_REL, &Analog[2],    	"p2 x-axis" ),
	A("P2 Trackball Y",	BIT_ANALOG_REL, &Analog[3],    	"p2 y-axis" ),
	{"P2 Fire 1"         , BIT_DIGITAL   , System16InputPort2 + 0, "p2 fire 1" },

	{"P3 Coin"           , BIT_DIGITAL   , System16InputPort0 + 0, "p3 coin"   },
	{"P3 Start"          , BIT_DIGITAL   , System16InputPort0 + 7, "p3 start"  },
	{"P3 Up"             , BIT_DIGITAL   , System16InputPort6 + 3, "p3 up"     },
	{"P3 Down"           , BIT_DIGITAL   , System16InputPort6 + 4, "p3 down"   },
	{"P3 Left"           , BIT_DIGITAL   , System16InputPort6 + 1, "p3 left"   },
	{"P3 Right"          , BIT_DIGITAL   , System16InputPort6 + 2, "p3 right"  },
	A("P3 Trackball X",	BIT_ANALOG_REL, &Analog[4],    	"p3 x-axis" ),
	A("P3 Trackball Y",	BIT_ANALOG_REL, &Analog[5],    	"p3 y-axis" ),
	{"P3 Fire 1"         , BIT_DIGITAL   , System16InputPort1 + 2, "p3 fire 1" },

	{"Service"           , BIT_DIGITAL   , System16InputPort0 + 3 , "service"  },
	{"Diagnostics"       , BIT_DIGITAL   , System16InputPort0 + 2 , "diag"     },
	{"Reset"             , BIT_DIGITAL   , &System16Reset         , "reset"    },
	{"Dip 1"             , BIT_DIPSWITCH , System16Dip + 0        , "dip"      },
	{"Dip 2"             , BIT_DIPSWITCH , System16Dip + 1        , "dip"      },
};

STDINPUTINFO(Wwally3p)

#undef A

/*====================================================
Dip defs
====================================================*/

#define SYSTEM18_COINAGE(dipval)								\
	{0   , 0xfe, 0   , 16  , "Coin A"                               },			\
	{dipval, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0x0f, 0x05, "2 Coins 1 Credit 5/3 6/4"           },			\
	{dipval, 0x01, 0x0f, 0x04, "2 Coins 1 Credit 4/3"               },			\
	{dipval, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"                   },			\
	{dipval, 0x01, 0x0f, 0x01, "1 Coin  1 Credit 2/3"               },			\
	{dipval, 0x01, 0x0f, 0x02, "1 Coin  1 Credit 4/5"               },			\
	{dipval, 0x01, 0x0f, 0x03, "1 Coin  1 Credit 5/6"               },			\
	{dipval, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"                  },			\
	{dipval, 0x01, 0x0f, 0x00, "Free Play (if coin B too) or 1C/1C" },			\
												\
	{0   , 0xfe, 0   , 16  , "Coin B"                               },			\
	{dipval, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"                   },			\
	{dipval, 0x01, 0xf0, 0x50, "2 Coins 1 Credit 5/3 6/4"           },			\
	{dipval, 0x01, 0xf0, 0x40, "2 Coins 1 Credit 4/3"               },			\
	{dipval, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"                   },			\
	{dipval, 0x01, 0xf0, 0x10, "1 Coin  1 Credit 2/3"               },			\
	{dipval, 0x01, 0xf0, 0x20, "1 Coin  1 Credit 4/5"               },			\
	{dipval, 0x01, 0xf0, 0x30, "1 Coin  1 Credit 5/6"               },			\
	{dipval, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"                  },			\
	{dipval, 0x01, 0xf0, 0x00, "Free Play (if coin A too) or 1C/1C" },

static struct BurnDIPInfo AstormDIPList[]=
{
	// Default Values
	{0x21, 0xff, 0xff, 0xff, NULL                                 },
	{0x22, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x21)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x22, 0x01, 0x01, 0x01, "Off"                                },
	{0x22, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x22, 0x01, 0x02, 0x02, "Off"                                },
	{0x22, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 8   , "Difficulty"                         },
	{0x22, 0x01, 0x1c, 0x04, "Easiest"                            },
	{0x22, 0x01, 0x1c, 0x08, "Easier"                             },
	{0x22, 0x01, 0x1c, 0x0c, "Easy"                               },
	{0x22, 0x01, 0x1c, 0x1c, "Normal"                             },
	{0x22, 0x01, 0x1c, 0x10, "Hard"                               },
	{0x22, 0x01, 0x1c, 0x14, "Harder"                             },
	{0x22, 0x01, 0x1c, 0x18, "Hardest"                            },
	{0x22, 0x01, 0x1c, 0x00, "Special"                            },

	{0   , 0xfe, 0   , 2   , "Coin Chutes"                        },
	{0x22, 0x01, 0x20, 0x00, "1"                                  },
	{0x22, 0x01, 0x20, 0x20, "3"                                  },
};

STDDIPINFO(Astorm)

static struct BurnDIPInfo Astorm2pDIPList[]=
{
	// Default Values
	{0x16, 0xff, 0xff, 0xff, NULL                                 },
	{0x17, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x16)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x17, 0x01, 0x01, 0x01, "Off"                                },
	{0x17, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x17, 0x01, 0x02, 0x02, "Off"                                },
	{0x17, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 8   , "Difficulty"                         },
	{0x17, 0x01, 0x1c, 0x04, "Easiest"                            },
	{0x17, 0x01, 0x1c, 0x08, "Easier"                             },
	{0x17, 0x01, 0x1c, 0x0c, "Easy"                               },
	{0x17, 0x01, 0x1c, 0x1c, "Normal"                             },
	{0x17, 0x01, 0x1c, 0x10, "Hard"                               },
	{0x17, 0x01, 0x1c, 0x14, "Harder"                             },
	{0x17, 0x01, 0x1c, 0x18, "Hardest"                            },
	{0x17, 0x01, 0x1c, 0x00, "Special"                            },

	{0   , 0xfe, 0   , 2   , "Coin Chutes"                        },
	{0x17, 0x01, 0x20, 0x00, "1"                                  },
	{0x17, 0x01, 0x20, 0x20, "2"                                  },
};

STDDIPINFO(Astorm2p)

static struct BurnDIPInfo BloxeedDIPList[]=
{
	// Default Values
	{0x16, 0xff, 0xff, 0xff, NULL                                 },
	{0x17, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x16)

	// Dip 2
	{0   , 0xfe, 0   ,    2, "Allow VS Mode"					  },
	{0x17, 0x01, 0x01, 0x01, "Yes"								  },
	{0x17, 0x01, 0x01, 0x00, "No"								  },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"						  },
	{0x17, 0x01, 0x02, 0x02, "Off"								  },
	{0x17, 0x01, 0x02, 0x00, "On"								  },

	{0   , 0xfe, 0   ,    4, "Lines Per Level"					  },
	{0x17, 0x01, 0x0c, 0x04, "2"								  },
	{0x17, 0x01, 0x0c, 0x00, "2"								  },
	{0x17, 0x01, 0x0c, 0x0c, "3"								  },
	{0x17, 0x01, 0x0c, 0x08, "4"								  },

	{0   , 0xfe, 0   ,    4, "Difficulty"						  },
	{0x17, 0x01, 0x30, 0x20, "Easy"								  },
	{0x17, 0x01, 0x30, 0x30, "Normal"							  },
	{0x17, 0x01, 0x30, 0x10, "Hard"								  },
	{0x17, 0x01, 0x30, 0x00, "Hardest"							  },

	{0   , 0xfe, 0   ,    2, "Unknown"							  },
	{0x17, 0x01, 0x40, 0x40, "Off"								  },
	{0x17, 0x01, 0x40, 0x00, "On"								  },

	{0   , 0xfe, 0   ,    2, "High Speed Mode"					  },
	{0x17, 0x01, 0x80, 0x00, "Off"								  },
	{0x17, 0x01, 0x80, 0x80, "On"								  },
};

STDDIPINFO(Bloxeed)

static struct BurnDIPInfo CltchitrDIPList[]=
{
	// Default Values
	{0x16, 0xff, 0xff, 0xff, NULL                                 },
	{0x17, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x16)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x17, 0x01, 0x01, 0x01, "Off"                                },
	{0x17, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x17, 0x01, 0x02, 0x02, "Off"                                },
	{0x17, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 4   , "Game Time P1"                       },
	{0x17, 0x01, 0x0c, 0x04, "2 Credits 18 Outcounts 14 Min"      },
	{0x17, 0x01, 0x0c, 0x00, "1 Credit   6 Outcounts 7 Min"       },
	{0x17, 0x01, 0x0c, 0x08, "1 Credit  12 Outcounts 12 Min"      },
	{0x17, 0x01, 0x0c, 0x0c, "1C 6OC 8M / 2C 18OC 14M"            },

	{0   , 0xfe, 0   , 4   , "Game Time P2"                       },
	{0x17, 0x01, 0x30, 0x10, "4 Credits 18 Outcounts 16 Min"      },
	{0x17, 0x01, 0x30, 0x00, "2 Credits  6 Outcounts 8 Min"       },
	{0x17, 0x01, 0x30, 0x20, "2 Credits 12 Outcounts 14 Min"      },
	{0x17, 0x01, 0x30, 0x30, "2C 6OC 8M / 4C 18OC 16M"            },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x17, 0x01, 0xc0, 0x40, "Easiest"                            },
	{0x17, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x17, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x17, 0x01, 0xc0, 0x00, "Hard"                               },
};

STDDIPINFO(Cltchitr)

static struct BurnDIPInfo DdcrewDIPList[]=
{
	// Default Values
	{0x1b, 0xff, 0xff, 0xff, NULL                                 },
	{0x1c, 0xff, 0xff, 0xf7, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x1b)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Credits Needed"                     },
	{0x1c, 0x01, 0x01, 0x01, "1 to start, 1 to continue"          },
	{0x1c, 0x01, 0x01, 0x00, "2 to start, 1 to continue"          },

	{0   , 0xfe, 0   , 2   , "Switch to Start"                    },
	{0x1c, 0x01, 0x02, 0x02, "Start"                              },
	{0x1c, 0x01, 0x02, 0x00, "Attack"                             },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x1c, 0x01, 0x04, 0x04, "Common"                             },
	{0x1c, 0x01, 0x04, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x1c, 0x01, 0x08, 0x08, "Off"                                },
	{0x1c, 0x01, 0x08, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 4   , "Player Start/Continue"              },
	{0x1c, 0x01, 0x30, 0x30, "3/3"                                },
	{0x1c, 0x01, 0x30, 0x20, "2/3"                                },
	{0x1c, 0x01, 0x30, 0x10, "2/2"                                },
	{0x1c, 0x01, 0x30, 0x00, "3/4"                                },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x1c, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x1c, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x1c, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x1c, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Ddcrew)

static struct BurnDIPInfo Ddcrew2DIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                                 },
	{0x14, 0xff, 0xff, 0xf7, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x13)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Credits Needed"                     },
	{0x14, 0x01, 0x01, 0x01, "1 to start, 1 to continue"          },
	{0x14, 0x01, 0x01, 0x00, "2 to start, 1 to continue"          },

	{0   , 0xfe, 0   , 2   , "Switch to Start"                    },
	{0x14, 0x01, 0x02, 0x02, "Start"                              },
	{0x14, 0x01, 0x02, 0x00, "Attack"                             },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x14, 0x01, 0x04, 0x04, "Common"                             },
	{0x14, 0x01, 0x04, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x14, 0x01, 0x08, 0x08, "Off"                                },
	{0x14, 0x01, 0x08, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 4   , "Player Start/Continue"              },
	{0x14, 0x01, 0x30, 0x30, "3/3"                                },
	{0x14, 0x01, 0x30, 0x20, "2/3"                                },
	{0x14, 0x01, 0x30, 0x10, "2/2"                                },
	{0x14, 0x01, 0x30, 0x00, "3/4"                                },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x14, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x14, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x14, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x14, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Ddcrew2)

static struct BurnDIPInfo DdcrewuDIPList[]=
{
	// Default Values
	{0x23, 0xff, 0xff, 0xff, NULL                                 },
	{0x24, 0xff, 0xff, 0xf7, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x23)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "Credits Needed"                     },
	{0x24, 0x01, 0x01, 0x01, "1 to start, 1 to continue"          },
	{0x24, 0x01, 0x01, 0x00, "2 to start, 1 to continue"          },

	{0   , 0xfe, 0   , 2   , "Switch to Start"                    },
	{0x24, 0x01, 0x02, 0x02, "Start"                              },
	{0x24, 0x01, 0x02, 0x00, "Attack"                             },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x24, 0x01, 0x04, 0x04, "Common"                             },
	{0x24, 0x01, 0x04, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x24, 0x01, 0x08, 0x08, "Off"                                },
	{0x24, 0x01, 0x08, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 4   , "Player Start/Continue"              },
	{0x24, 0x01, 0x30, 0x30, "3/3"                                },
	{0x24, 0x01, 0x30, 0x20, "2/3"                                },
	{0x24, 0x01, 0x30, 0x10, "2/2"                                },
	{0x24, 0x01, 0x30, 0x00, "3/4"                                },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x24, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x24, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x24, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x24, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Ddcrewu)

static struct BurnDIPInfo DesertbrDIPList[]=
{
	// Default Values
	{0x1e, 0xff, 0xff, 0xff, NULL                                 },
	{0x1f, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x1e)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x17, 0x01, 0x01, 0x01, "Off"                                },
	{0x17, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x1f, 0x01, 0x02, 0x02, "Off"                                },
	{0x1f, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Play Mode"                          },
	{0x1f, 0x01, 0x08, 0x00, "2 Players"                          },
	{0x1f, 0x01, 0x08, 0x08, "3 Players"                          },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x15, 0x01, 0x10, 0x10, "Common"                             },
	{0x15, 0x01, 0x10, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 2   , "Start Button"                       },
	{0x15, 0x01, 0x20, 0x20, "Use"                                },
	{0x15, 0x01, 0x20, 0x00, "Unused"                             },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x1f, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x1f, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x1f, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x1f, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Desertbr)

static struct BurnDIPInfo HamawayDIPList[]=
{
	// Default Values
	{0x16, 0xff, 0xff, 0xff, NULL                                 },
	{0x17, 0xff, 0xff, 0xbf, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x16)

	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x17, 0x01, 0x03, 0x02, "Easy"                               },
	{0x17, 0x01, 0x03, 0x03, "Normal"                             },
	{0x17, 0x01, 0x03, 0x01, "Hard"                               },
	{0x17, 0x01, 0x03, 0x00, "Hardest"                            },

	{0   , 0xfe, 0   , 4   , "Number of Players"                  },
	{0x17, 0x01, 0x0c, 0x08, "1"                                  },
	{0x17, 0x01, 0x0c, 0x04, "2"                                  },
	{0x17, 0x01, 0x0c, 0x0c, "3"                                  },
	{0x17, 0x01, 0x0c, 0x00, "5"                                  },

	{0   , 0xfe, 0   , 2   , "Advertise Sound"                    },
	{0x17, 0x01, 0x40, 0x40, "Off"                                },
	{0x17, 0x01, 0x40, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Credit to Start"                    },
	{0x17, 0x01, 0x80, 0x80, "1"                                  },
	{0x17, 0x01, 0x80, 0x00, "2"                                  },
};

STDDIPINFO(Hamaway)

static struct BurnDIPInfo LghostDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xff, NULL                                 },
	{0x15, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x14)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x15, 0x01, 0x01, 0x01, "Off"                                },
	{0x15, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x15, 0x01, 0x02, 0x02, "Off"                                },
	{0x15, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 8   , "Difficulty"                         },
	{0x15, 0x01, 0x1c, 0x18, "Easy"                               },
	{0x15, 0x01, 0x1c, 0x14, "Easier"                             },
	{0x15, 0x01, 0x1c, 0x0c, "Easiest"                            },
	{0x15, 0x01, 0x1c, 0x1c, "Normal"                             },
	{0x15, 0x01, 0x1c, 0x10, "Hard"                               },
	{0x15, 0x01, 0x1c, 0x04, "Harder"                             },
	{0x15, 0x01, 0x1c, 0x08, "Hardest"                            },
	{0x15, 0x01, 0x1c, 0x00, "Extra Hardest"                      },

	{0   , 0xfe, 0   , 2   , "Continue Mode"                      },
	{0x15, 0x01, 0x20, 0x00, "Off"                                },
	{0x15, 0x01, 0x20, 0x20, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x15, 0x01, 0x40, 0x00, "Common"                             },
	{0x15, 0x01, 0x40, 0x40, "Individual"                         },
};

STDDIPINFO(Lghost)

static struct BurnDIPInfo MwalkDIPList[]=
{
	DIP_OFFSET(0x1e)
	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                                 },
	{0x01, 0xff, 0xff, 0xcd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x00)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x01, 0x01, 0x01, 0x01, "Off"                                },
	{0x01, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x01, 0x01, 0x02, 0x02, "Off"                                },
	{0x01, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Lives"                              },
	{0x01, 0x01, 0x04, 0x04, "2"                                  },
	{0x01, 0x01, 0x04, 0x00, "3"                                  },

	{0   , 0xfe, 0   , 2   , "Player Vitality"                    },
	{0x01, 0x01, 0x08, 0x08, "Low"                                },
	{0x01, 0x01, 0x08, 0x00, "High"                               },

	{0   , 0xfe, 0   , 2   , "Play Mode"                          },
	{0x01, 0x01, 0x10, 0x10, "2 Players"                          },
	{0x01, 0x01, 0x10, 0x00, "3 Players"                          },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x01, 0x01, 0x20, 0x20, "Common"                             },
	{0x01, 0x01, 0x20, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x01, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x01, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x01, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x01, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Mwalk)

static void MwalkFixInputs()
{
	// Depending on dips, coin changes
	if (System16Dip[1] & 0x20) { // Common "Coin Chute"
		if (System16InputPort0[7]) System16InputPort0[0] = 1;
	}
}

static void MwalkudFixInputs()
{
	// Depending on dips, coin changes
	if (~System16Dip[1] & 0x20) { // Common "Coin Chute"
		if (System16InputPort0[7]) System16InputPort0[0] = 1;
	}
}

static struct BurnDIPInfo MwalkudDIPList[]=
{
	DIP_OFFSET(0x1e)
	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                                 },
	{0x01, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x00)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x01, 0x01, 0x01, 0x01, "Off"                                },
	{0x01, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x01, 0x01, 0x02, 0x02, "Off"                                },
	{0x01, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Lives"                              },
	{0x01, 0x01, 0x04, 0x04, "2"                                  },
	{0x01, 0x01, 0x04, 0x00, "3"                                  },

	{0   , 0xfe, 0   , 2   , "Player Vitality"                    },
	{0x01, 0x01, 0x08, 0x08, "Low"                                },
	{0x01, 0x01, 0x08, 0x00, "High"                               },

	{0   , 0xfe, 0   , 2   , "Play Mode"                          },
	{0x01, 0x01, 0x10, 0x00, "2 Players"                          },
	{0x01, 0x01, 0x10, 0x10, "3 Players"                          },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x01, 0x01, 0x20, 0x00, "Common"                             },
	{0x01, 0x01, 0x20, 0x20, "Individual"                         },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x01, 0x01, 0xc0, 0x80, "Easy"                               },
	{0x01, 0x01, 0xc0, 0xc0, "Normal"                             },
	{0x01, 0x01, 0xc0, 0x40, "Hard"                               },
	{0x01, 0x01, 0xc0, 0x00, "Hardest"                            },
};

STDDIPINFO(Mwalkud)

static struct BurnDIPInfo ShdancerDIPList[]=
{
	// Default Values
	{0x16, 0xff, 0xff, 0xff, NULL                                 },
	{0x17, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x16)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x17, 0x01, 0x01, 0x01, "Off"                                },
	{0x17, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x17, 0x01, 0x02, 0x02, "Off"                                },
	{0x17, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 4   , "Lives"                              },
	{0x17, 0x01, 0x0c, 0x00, "2"                                  },
	{0x17, 0x01, 0x0c, 0x0c, "3"                                  },
	{0x17, 0x01, 0x0c, 0x08, "4"                                  },
	{0x17, 0x01, 0x0C, 0x04, "5"                                  },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x17, 0x01, 0x30, 0x20, "Easy"                               },
	{0x17, 0x01, 0x30, 0x30, "Normal"                             },
	{0x17, 0x01, 0x30, 0x10, "Hard"                               },
	{0x17, 0x01, 0x30, 0x00, "Hardest"                            },

	{0   , 0xfe, 0   , 4   , "Time Adjust"                        },
	{0x17, 0x01, 0xc0, 0x00, "2:20"                               },
	{0x17, 0x01, 0xc0, 0x40, "2:40"                               },
	{0x17, 0x01, 0xc0, 0xc0, "3:00"                               },
	{0x17, 0x01, 0xc0, 0x80, "3:30"                               },
};

STDDIPINFO(Shdancer)

static struct BurnDIPInfo WwallyDIPList[]=
{
	DIP_OFFSET(0x15)
	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                                 },
	{0x01, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x00)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x01, 0x01, 0x01, 0x01, "Off"                                },
	{0x01, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x01, 0x01, 0x02, 0x02, "Off"                                },
	{0x01, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x01, 0x01, 0x04, 0x04, "Common"                             },
	{0x01, 0x01, 0x04, 0x00, "Individual"                         },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x01, 0x01, 0x60, 0x40, "Easy"                               },
	{0x01, 0x01, 0x60, 0x60, "Normal"                             },
	{0x01, 0x01, 0x60, 0x20, "Hard"                               },
	{0x01, 0x01, 0x60, 0x00, "Hardest"                            },
};

STDDIPINFO(Wwally)

static struct BurnDIPInfo Wwally3pDIPList[]=
{
	DIP_OFFSET(0x1e)
	// Default Values
	{0x00, 0xff, 0xff, 0xff, NULL                                 },
	{0x01, 0xff, 0xff, 0xfd, NULL                                 },

	// Dip 1
	SYSTEM18_COINAGE(0x00)

	// Dip 2
	{0   , 0xfe, 0   , 2   , "2 Credits to Start"                 },
	{0x01, 0x01, 0x01, 0x01, "Off"                                },
	{0x01, 0x01, 0x01, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"                        },
	{0x01, 0x01, 0x02, 0x02, "Off"                                },
	{0x01, 0x01, 0x02, 0x00, "On"                                 },

	{0   , 0xfe, 0   , 2   , "Coin Chute"                         },
	{0x01, 0x01, 0x04, 0x00, "Common"                             },
	{0x01, 0x01, 0x04, 0x04, "Individual"                         },

	{0   , 0xfe, 0   , 4   , "Difficulty"                         },
	{0x01, 0x01, 0x60, 0x40, "Easy"                               },
	{0x01, 0x01, 0x60, 0x60, "Normal"                             },
	{0x01, 0x01, 0x60, 0x20, "Hard"                               },
	{0x01, 0x01, 0x60, 0x00, "Hardest"                            },
};

STDDIPINFO(Wwally3p)

#undef SYSTEM18_COINAGE

/*====================================================
Rom defs
====================================================*/

static struct BurnRomInfo AstormRomDesc[] = {
	{ "epr-13182.a6",   0x40000, 0xe31f2a1c, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13181.a5",   0x40000, 0x78cd3b26, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",  0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",  0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",  0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",  0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",  0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",  0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",  0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",  0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",  0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",  0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",  0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083a.bin", 0x20000, 0xe7528e06, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",  0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",  0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",  0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0154.key",   0x02000, 0xb86b6b8f, SYS16_ROM_KEY | BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(Astorm)
STD_ROM_FN(Astorm)

static struct BurnRomInfo Astorm3dRomDesc[] = {
	{ "bootleg_epr-13165.a6",   0x40000, 0x2a0dbff5, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13164.a5",   0x40000, 0xf981a183, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",          0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",          0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",          0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",          0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",          0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",          0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",          0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",          0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",          0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",          0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",          0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083.bin",          0x20000, 0x5df3af20, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",          0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",          0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",          0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Astorm3d)
STD_ROM_FN(Astorm3d)

static struct BurnRomInfo Astorm3RomDesc[] = {
	{ "epr-13165.a6",   0x40000, 0x6efcd381, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13164.a5",   0x40000, 0x97d693c6, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",  0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",  0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",  0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",  0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",  0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",  0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",  0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",  0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",  0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",  0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",  0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083.bin",  0x20000, 0x5df3af20, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",  0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",  0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",  0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0148.key",   0x02000, 0x72e4b64a, SYS16_ROM_KEY | BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(Astorm3)
STD_ROM_FN(Astorm3)

static struct BurnRomInfo AstormjRomDesc[] = {
	{ "epr-13085.a6",   0x40000, 0x15f74e2d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13084.a5",   0x40000, 0x9687b38f, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",  0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",  0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",  0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",  0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",  0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",  0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",  0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",  0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",  0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",  0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",  0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083b.bin", 0x20000, 0x169b4b5f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",  0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",  0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",  0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0146.key",   0x02000, 0xe94991c5, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Astormj)
STD_ROM_FN(Astormj)

static struct BurnRomInfo AstormjdRomDesc[] = {
	{ "bootleg_epr-13085.a6",   0x40000, 0xc0d7f3c2, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13084.a5",   0x40000, 0x006635f0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",          0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",          0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",          0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",          0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",          0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",          0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",          0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",          0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",          0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",          0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",          0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083b.bin",         0x20000, 0x169b4b5f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",          0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",          0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",          0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Astormjd)
STD_ROM_FN(Astormjd)

static struct BurnRomInfo AstormuRomDesc[] = {
	{ "epr-13095.a6",   0x40000, 0x55d40742, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13094.a5",   0x40000, 0x92b305f9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",  0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",  0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",  0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",  0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",  0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",  0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",  0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",  0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",  0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",  0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",  0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083.bin",  0x20000, 0x5df3af20, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",  0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",  0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",  0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0147.key",   0x02000, 0x3fd54ba7, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Astormu)
STD_ROM_FN(Astormu)

static struct BurnRomInfo AstormudRomDesc[] = {
	{ "bootleg_epr-13095.a6",   0x40000, 0xa29b1eea, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13094.a5",   0x40000, 0x81f4c6b6, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13073.bin",          0x40000, 0xdf5d0a61, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13074.bin",          0x40000, 0x787afab8, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13075.bin",          0x40000, 0x4e01b477, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13082.bin",          0x40000, 0xa782b704, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13089.bin",          0x40000, 0x2a4227f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13081.bin",          0x40000, 0xeb510228, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13088.bin",          0x40000, 0x3b6b4c55, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13080.bin",          0x40000, 0xe668eefb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13087.bin",          0x40000, 0x2293427d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13079.bin",          0x40000, 0xde9221ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13086.bin",          0x40000, 0x8c9a71c4, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13083.bin",          0x20000, 0x5df3af20, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13076.bin",          0x40000, 0x94e6c76e, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13077.bin",          0x40000, 0xe2ec0d8d, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13078.bin",          0x40000, 0x15684dc5, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Astormud)
STD_ROM_FN(Astormud)

// adds (c) 1989 Elorg
static struct BurnRomInfo BloxeedRomDesc[] = {
	{ "epr-12911.a6",   0x20000, 0xa481581a, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-12910.a5",   0x20000, 0xdd1bc3bf, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "opr-12884.b1",   0x10000, 0xe024aa33, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12885.b2",   0x10000, 0x8041b814, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12886.b3",   0x10000, 0xde32285e, SYS16_ROM_TILES | BRF_GRA },

	{ "opr-12887.b11",  0x10000, 0xf0c0f49d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "opr-12891.a11",  0x10000, 0x90d31a8c, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12888.a4",   0x20000, 0x6f2fc63c, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "317-0139.key",   0x02000, 0x9aae84cb, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Bloxeed)
STD_ROM_FN(Bloxeed)

// Only shows (c) 1989 SEGA
static struct BurnRomInfo BloxeedaRomDesc[] = {
	{ "epr-12890.a6",   0x20000, 0xb0b85294, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-12889.a5",   0x20000, 0x96c67e36, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "opr-12884.b1",   0x10000, 0xe024aa33, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12885.b2",   0x10000, 0x8041b814, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12886.b3",   0x10000, 0xde32285e, SYS16_ROM_TILES | BRF_GRA },

	{ "opr-12887.b11",  0x10000, 0xf0c0f49d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "opr-12891.a11",  0x10000, 0x90d31a8c, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12888.a4",   0x20000, 0x6f2fc63c, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "317-0139.key",   0x02000, 0x9aae84cb, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Bloxeeda)
STD_ROM_FN(Bloxeeda)

static struct BurnRomInfo BloxeeddRomDesc[] = {
	{ "bootleg_epr-12911.a6",   0x20000, 0x2d634453, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-12910.a5",   0x20000, 0xe6d2e9cc, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "opr-12884.b1",           0x10000, 0xe024aa33, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12885.b2",           0x10000, 0x8041b814, SYS16_ROM_TILES | BRF_GRA },
	{ "opr-12886.b3",           0x10000, 0xde32285e, SYS16_ROM_TILES | BRF_GRA },

	{ "opr-12887.b11",          0x10000, 0xf0c0f49d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "opr-12891.a11",          0x10000, 0x90d31a8c, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12888.a4",           0x20000, 0x6f2fc63c, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(Bloxeedd)
STD_ROM_FN(Bloxeedd)

static struct BurnRomInfo CltchitrRomDesc[] = {
	{ "epr-13794.a4",   0x40000, 0xc8d80233, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13795.a6",   0x40000, 0xb0b60b67, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13784.a5",   0x40000, 0x80c8180d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13786.a7",   0x40000, 0x3095dac0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13773.c1",   0x80000, 0x3fc600e5, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13774.c2",   0x80000, 0x2411a824, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13775.c3",   0x80000, 0xcf527bf6, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13779.c10",  0x80000, 0xc707f416, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13787.a10",  0x80000, 0xf05c68c6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13780.c11",  0x80000, 0xa4c341e0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13788.a11",  0x80000, 0x0106fea6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13781.c12",  0x80000, 0xf33b13af, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13789.a12",  0x80000, 0x09ba8835, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13793.c7",   0x80000, 0xa3d31944, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13792.c6",   0x80000, 0x808f9695, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13791.c5",   0x80000, 0x35c16d80, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0176.key",   0x02000, 0x9b072430, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Cltchitr)
STD_ROM_FN(Cltchitr)

static struct BurnRomInfo CltchitrdRomDesc[] = {
	{ "bootleg_epr-13794.a4",   0x40000, 0xb15de2c5, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13795.a6",   0x40000, 0x8fc7e524, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13784.a5",           0x40000, 0x80c8180d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13786.a7",           0x40000, 0x3095dac0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13773.c1",           0x80000, 0x3fc600e5, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13774.c2",           0x80000, 0x2411a824, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13775.c3",           0x80000, 0xcf527bf6, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13779.c10",          0x80000, 0xc707f416, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13787.a10",          0x80000, 0xf05c68c6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13780.c11",          0x80000, 0xa4c341e0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13788.a11",          0x80000, 0x0106fea6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13781.c12",          0x80000, 0xf33b13af, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13789.a12",          0x80000, 0x09ba8835, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13793.c7",           0x80000, 0xa3d31944, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13792.c6",           0x80000, 0x808f9695, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13791.c5",           0x80000, 0x35c16d80, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Cltchitrd)
STD_ROM_FN(Cltchitrd)

static struct BurnRomInfo CltchitrjRomDesc[] = {
	{ "epr-13783.a4",   0x40000, 0xe2a1d5af, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13796.a6",   0x40000, 0x06001c67, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13785.a5",   0x40000, 0x09714762, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13797.a7",   0x40000, 0x361ade9f, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13773.c1",   0x80000, 0x3fc600e5, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13774.c2",   0x80000, 0x2411a824, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13775.c3",   0x80000, 0xcf527bf6, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13779.c10",  0x80000, 0xc707f416, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13787.a10",  0x80000, 0xf05c68c6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13780.c11",  0x80000, 0xa4c341e0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13788.a11",  0x80000, 0x0106fea6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13781.c12",  0x80000, 0xf33b13af, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13789.a12",  0x80000, 0x09ba8835, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13782.c13",  0x40000, 0x73790852, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13790.a13",  0x40000, 0x23849101, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13778.c7",   0x20000, 0x35e86146, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13777.c6",   0x80000, 0xd1524782, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13776.c5",   0x80000, 0x282ac9fe, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0175.key",   0x02000, 0x70d9d283, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Cltchitrj)
STD_ROM_FN(Cltchitrj)

static struct BurnRomInfo CltchitrjdRomDesc[] = {
	{ "bootleg_epr-13783.a4",   0x40000, 0xdafd48af, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13796.a6",   0x40000, 0x219aae33, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13785.a5",           0x40000, 0x09714762, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13797.a7",           0x40000, 0x361ade9f, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13773.c1",           0x80000, 0x3fc600e5, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13774.c2",           0x80000, 0x2411a824, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13775.c3",           0x80000, 0xcf527bf6, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13779.c10",          0x80000, 0xc707f416, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13787.a10",          0x80000, 0xf05c68c6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13780.c11",          0x80000, 0xa4c341e0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13788.a11",          0x80000, 0x0106fea6, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13781.c12",          0x80000, 0xf33b13af, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13789.a12",          0x80000, 0x09ba8835, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13782.c13",          0x40000, 0x73790852, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13790.a13",          0x40000, 0x23849101, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13778.c7",           0x20000, 0x35e86146, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "epr-13777.c6",           0x80000, 0xd1524782, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-13776.c5",           0x80000, 0x282ac9fe, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Cltchitrjd)
STD_ROM_FN(Cltchitrjd)

static struct BurnRomInfo DdcrewRomDesc[] = {
	{ "epr-14160.a4",   0x40000, 0xb9f897b7, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14161.a6",   0x40000, 0xbb03c1f0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0190.key",   0x02000, 0x2d502b11, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrew)
STD_ROM_FN(Ddcrew)

static struct BurnRomInfo DdcrewdRomDesc[] = {
	{ "bootleg_epr-14160.a4",   0x40000, 0x22a6c8e3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14161.a6",   0x40000, 0xd9eaba00, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrewd)
STD_ROM_FN(Ddcrewd)

static struct BurnRomInfo Ddcrew1RomDesc[] = {
	{ "epr-14154.a4",   0x40000, 0x9a0dadf0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14155.a6",   0x40000, 0xe74362f4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0187.key",   0x02000, 0x1dfb60be, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrew1)
STD_ROM_FN(Ddcrew1)

static struct BurnRomInfo Ddcrew1dRomDesc[] = {
	{ "bootleg_epr-14154.a4",   0x40000, 0x95749c77, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14155.a6",   0x40000, 0xbef3c932, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrew1d)
STD_ROM_FN(Ddcrew1d)

static struct BurnRomInfo Ddcrew2RomDesc[] = {
	{ "epr-14148.a4",   0x40000, 0xdf4cb0cf, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14149.a6",   0x40000, 0x380ff818, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0184.key",   0x02000, 0xcee06254, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrew2)
STD_ROM_FN(Ddcrew2)

static struct BurnRomInfo Ddcrew2dRomDesc[] = {
	{ "bootleg_epr-14148.a4",   0x40000, 0xf3ddb3b9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14149.a6",   0x40000, 0x98a83ee1, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrew2d)
STD_ROM_FN(Ddcrew2d)

static struct BurnRomInfo DdcrewjRomDesc[] = {
	{ "epr-14150.a4",   0x40000, 0xfc866b19, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14151.a6",   0x40000, 0x46d23fe4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0185.key",   0x02000, 0xa650b506, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrewj)
STD_ROM_FN(Ddcrewj)

static struct BurnRomInfo DdcrewjdRomDesc[] = {
	{ "bootleg_epr-14150.a4",   0x40000, 0x1f267de5, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14151.a6",   0x40000, 0x35074062, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrewjd)
STD_ROM_FN(Ddcrewjd)

static struct BurnRomInfo Ddcrewj2RomDesc[] = {
	{ "epr-14138.a4",   0x40000, 0xdf280b1b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14140.a6",   0x40000, 0x48f223ee, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0182.key",   0x02000, 0x2e8a3601, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrewj2)
STD_ROM_FN(Ddcrewj2)

static struct BurnRomInfo Ddcrewj2dRomDesc[] = {
	{ "bootleg_epr-14138.a4",   0x40000, 0x1a586bb2, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14140.a6",   0x40000, 0x43e030ae, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrewj2d)
STD_ROM_FN(Ddcrewj2d)

static struct BurnRomInfo DdcrewuRomDesc[] = {
	{ "epr-14152.a4",   0x40000, 0x69c7b571, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14153.a6",   0x40000, 0xe01fae0c, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",   0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",   0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",   0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",   0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",   0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",  0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",  0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",  0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",  0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",  0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",  0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",  0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",  0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",   0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",   0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",   0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",   0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0186.key",   0x02000, 0x7acaf1fd, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Ddcrewu)
STD_ROM_FN(Ddcrewu)

static struct BurnRomInfo DdcrewudRomDesc[] = {
	{ "bootleg_epr-14152.a4",   0x40000, 0x41b782d0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14153.a6",   0x40000, 0x48070486, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14139.a5",           0x40000, 0x06c31531, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mpr-14141.a7",           0x40000, 0x080a494b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-14127.c1",           0x40000, 0x2228cd88, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14128.c2",           0x40000, 0xedba8e10, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-14129.c3",           0x40000, 0xe8ecc305, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14134.c10",          0x80000, 0x4fda6a4b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14142.a10",          0x80000, 0x3cbf1f2a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14135.c11",          0x80000, 0xe9c74876, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14143.a11",          0x80000, 0x59022c31, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14136.c12",          0x80000, 0x720d9858, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14144.a12",          0x80000, 0x7775fdd4, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14137.c13",          0x80000, 0x846c4265, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-14145.a13",          0x80000, 0x0e76c797, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14133.c7",           0x20000, 0xcff96665, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14132.c6",           0x80000, 0x1fae0220, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14131.c5",           0x80000, 0xbe5a7d0b, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "epr-14130.c4",           0x80000, 0x948f34a1, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Ddcrewud)
STD_ROM_FN(Ddcrewud)

static struct BurnRomInfo DesertbrRomDesc[] = {
	{ "epr-14802.a4",   0x080000, 0x9ab93cbc, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14902.a6",   0x080000, 0x6724e7b1, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14793.a5",   0x080000, 0xdc9d7af3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14795.a7",   0x080000, 0x7e5bf7d9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14781.c1",   0x100000, 0xc4f7d7aa, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14782.c2",   0x100000, 0xccc98d05, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14783.c3",   0x100000, 0xef202bec, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14788.c10",  0x100000, 0xb5b05536, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14796.a10",  0x100000, 0xc033220a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14789.c11",  0x100000, 0x0f9bcb97, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14797.a11",  0x100000, 0x4c301cc9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14790.c12",  0x100000, 0x6a07ac27, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14798.a12",  0x100000, 0x50634625, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14791.c13",  0x100000, 0xa4ae352b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14799.a13",  0x100000, 0xaeb7b025, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14787.c7",   0x040000, 0xcc6feec7, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14786.c6",   0x080000, 0xcc8349f2, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14785.c5",   0x080000, 0x7babba13, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14784.c4",   0x080000, 0x073878e4, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0196.key",   0x002000, 0xcb942262, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Desertbr)
STD_ROM_FN(Desertbr)

static struct BurnRomInfo DesertbrdRomDesc[] = {
	{ "bootleg_epr-14802.a4",   0x80000, 0xaf74af1c, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14902.a6",   0x80000, 0xc883404b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14793.a5",           0x80000, 0xdc9d7af3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14795.a7",           0x80000, 0x7e5bf7d9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14781.c1",           0x100000, 0xc4f7d7aa, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14782.c2",           0x100000, 0xccc98d05, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14783.c3",           0x100000, 0xef202bec, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14788.c10",          0x100000, 0xb5b05536, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14796.a10",          0x100000, 0xc033220a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14789.c11",          0x100000, 0x0f9bcb97, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14797.a11",          0x100000, 0x4c301cc9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14790.c12",          0x100000, 0x6a07ac27, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14798.a12",          0x100000, 0x50634625, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14791.c13",          0x100000, 0xa4ae352b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14799.a13",          0x100000, 0xaeb7b025, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14787.c7",           0x40000, 0xcc6feec7, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14786.c6",           0x80000, 0xcc8349f2, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14785.c5",           0x80000, 0x7babba13, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14784.c4",           0x80000, 0x073878e4, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Desertbrd)
STD_ROM_FN(Desertbrd)

static struct BurnRomInfo DesertbrjRomDesc[] = {
	{ "epr-14792.a4",   0x080000, 0x453d9d02, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14794.a6",   0x080000, 0x4426758f, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14793.a5",   0x080000, 0xdc9d7af3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14795.a7",   0x080000, 0x7e5bf7d9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14781.c1",   0x100000, 0xc4f7d7aa, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14782.c2",   0x100000, 0xccc98d05, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14783.c3",   0x100000, 0xef202bec, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14788.c10",  0x100000, 0xb5b05536, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14796.a10",  0x100000, 0xc033220a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14789.c11",  0x100000, 0x0f9bcb97, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14797.a11",  0x100000, 0x4c301cc9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14790.c12",  0x100000, 0x6a07ac27, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14798.a12",  0x100000, 0x50634625, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14791.c13",  0x100000, 0xa4ae352b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14799.a13",  0x100000, 0xaeb7b025, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14787.c7",   0x040000, 0xcc6feec7, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14786.c6",   0x080000, 0xcc8349f2, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14785.c5",   0x080000, 0x7babba13, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14784.c4",   0x080000, 0x073878e4, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0194.key",   0x002000, 0x40cbc4cb, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Desertbrj)
STD_ROM_FN(Desertbrj)

static struct BurnRomInfo DesertbrjdRomDesc[] = {
	{ "bootleg_epr-14792.a4",   0x80000, 0xfd8ed26d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14794.a6",   0x80000, 0x399f167d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14793.a5",           0x80000, 0xdc9d7af3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14795.a7",           0x80000, 0x7e5bf7d9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14781.c1",           0x100000, 0xc4f7d7aa, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14782.c2",           0x100000, 0xccc98d05, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14783.c3",           0x100000, 0xef202bec, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14788.c10",          0x100000, 0xb5b05536, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14796.a10",          0x100000, 0xc033220a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14789.c11",          0x100000, 0x0f9bcb97, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14797.a11",          0x100000, 0x4c301cc9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14790.c12",          0x100000, 0x6a07ac27, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14798.a12",          0x100000, 0x50634625, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14791.c13",          0x100000, 0xa4ae352b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14799.a13",          0x100000, 0xaeb7b025, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14787.c7",           0x40000, 0xcc6feec7, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14786.c6",           0x80000, 0xcc8349f2, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14785.c5",           0x80000, 0x7babba13, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14784.c4",           0x80000, 0x073878e4, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Desertbrjd)
STD_ROM_FN(Desertbrjd)

static struct BurnRomInfo HamawayRomDesc[] = {
	{ "4.bin",          0x040000, 0xcc0981e1, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "6.bin",          0x040000, 0xe8599ee6, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "5.bin",          0x040000, 0xfdb247fd, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "7.bin",          0x040000, 0x63711470, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "c10.bin",        0x040000, 0xc55cb5cf, SYS16_ROM_TILES | BRF_GRA },
	{ "1.bin",          0x040000, 0x33be003f, SYS16_ROM_TILES | BRF_GRA },
	{ "c11.bin",        0x040000, 0x37787915, SYS16_ROM_TILES | BRF_GRA },
	{ "2.bin",          0x040000, 0x60ca5c9f, SYS16_ROM_TILES | BRF_GRA },
	{ "c12.bin",        0x040000, 0xf12f1cf3, SYS16_ROM_TILES | BRF_GRA },
	{ "3.bin",          0x040000, 0x520aa7ae, SYS16_ROM_TILES | BRF_GRA },

	{ "c17.bin",        0x040000, 0xaa28d7aa, SYS16_ROM_SPRITES | BRF_GRA },
	{ "10.bin",         0x040000, 0xc4c95161, SYS16_ROM_SPRITES | BRF_GRA },
	{ "c18.bin",        0x040000, 0x0f8fe8bb, SYS16_ROM_SPRITES | BRF_GRA },
	{ "11.bin",         0x040000, 0x2b5eacbc, SYS16_ROM_SPRITES | BRF_GRA },
	{ "c19.bin",        0x040000, 0x3c616caa, SYS16_ROM_SPRITES | BRF_GRA },
	{ "12.bin",         0x040000, 0xc7bbd579, SYS16_ROM_SPRITES | BRF_GRA },

	{ "c16.bin",        0x040000, 0x913cc18c, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "c15.bin",        0x040000, 0xb53694fc, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Hamaway)
STD_ROM_FN(Hamaway)

static struct BurnRomInfo LghostRomDesc[] = {
	{ "epr-13429.a4",   0x40000, 0x09bd65c0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13430.a6",   0x40000, 0x51009fe0, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13411.a5",   0x40000, 0x5160167b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13413.a7",   0x40000, 0x656b3bd8, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13414.c1",   0x40000, 0xdada2419, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13415.c2",   0x40000, 0xbbb62c48, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13416.c3",   0x40000, 0x1d11dbae, SYS16_ROM_TILES | BRF_GRA },

	{ "epr-13604.c10",  0x80000, 0x4009c8e5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13603.a10",  0x80000, 0x5350a94e, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13424.c11",  0x80000, 0xfb98d920, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13421.a11",  0x80000, 0x2fc75890, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13425.c12",  0x80000, 0xf8252589, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13422.a12",  0x80000, 0x48a0754d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13426.c13",  0x80000, 0x5cfb1e25, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13423.a13",  0x80000, 0x335bbc9d, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13417.c7",   0x20000, 0xcd7beb49, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13420.c6",   0x40000, 0x3de0dee4, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13419.c5",   0x40000, 0xe7021b0a, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13418.c4",   0x40000, 0x0732594d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0166.key",   0x02000, 0x8379961f, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Lghost)
STD_ROM_FN(Lghost)

static struct BurnRomInfo LghostdRomDesc[] = {
	{ "bootleg_epr-13429.a4",   0x40000, 0x3d641671, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13430.a6",   0x40000, 0xf0ba14be, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13411.a5",           0x40000, 0x5160167b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13413.a7",           0x40000, 0x656b3bd8, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13414.c1",           0x40000, 0xdada2419, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13415.c2",           0x40000, 0xbbb62c48, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13416.c3",           0x40000, 0x1d11dbae, SYS16_ROM_TILES | BRF_GRA },

	{ "epr-13604.c10",          0x80000, 0x4009c8e5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13603.a10",          0x80000, 0x5350a94e, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13424.c11",          0x80000, 0xfb98d920, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13421.a11",          0x80000, 0x2fc75890, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13425.c12",          0x80000, 0xf8252589, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13422.a12",          0x80000, 0x48a0754d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13426.c13",          0x80000, 0x5cfb1e25, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13423.a13",          0x80000, 0x335bbc9d, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13417.c7",           0x20000, 0xcd7beb49, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13420.c6",           0x40000, 0x3de0dee4, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13419.c5",           0x40000, 0xe7021b0a, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13418.c4",           0x40000, 0x0732594d, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Lghostd)
STD_ROM_FN(Lghostd)

static struct BurnRomInfo LghostjRomDesc[] = {
	{ "epr-13410.a4",   0x40000, 0x9c1d672b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13412.a6",   0x40000, 0x3a3155fa, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13411.a5",   0x40000, 0x5160167b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13413.a7",   0x40000, 0x656b3bd8, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13414.c1",   0x40000, 0xdada2419, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13415.c2",   0x40000, 0xbbb62c48, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13416.c3",   0x40000, 0x1d11dbae, SYS16_ROM_TILES | BRF_GRA },

	{ "epr-13604.c10",  0x80000, 0x4009c8e5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13603.a10",  0x80000, 0x5350a94e, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13424.c11",  0x80000, 0xfb98d920, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13421.a11",  0x80000, 0x2fc75890, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13425.c12",  0x80000, 0xf8252589, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13422.a12",  0x80000, 0x48a0754d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13426.c13",  0x80000, 0x5cfb1e25, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13423.a13",  0x80000, 0x335bbc9d, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13417.c7",   0x20000, 0xcd7beb49, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13420.c6",   0x40000, 0x3de0dee4, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13419.c5",   0x40000, 0xe7021b0a, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13418.c4",   0x40000, 0x0732594d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0164.key",   0x02000, 0x6bf5cf2d, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Lghostj)
STD_ROM_FN(Lghostj)

static struct BurnRomInfo LghostuRomDesc[] = {
	{ "epr-13427.a4",   0x40000, 0x5bf8fb6b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13428.a6",   0x40000, 0x276775f5, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13411.a5",   0x40000, 0x5160167b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13413.a7",   0x40000, 0x656b3bd8, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13414.c1",   0x40000, 0xdada2419, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13415.c2",   0x40000, 0xbbb62c48, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13416.c3",   0x40000, 0x1d11dbae, SYS16_ROM_TILES | BRF_GRA },

	{ "epr-13604.c10",  0x80000, 0x4009c8e5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13603.a10",  0x80000, 0x5350a94e, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13424.c11",  0x80000, 0xfb98d920, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13421.a11",  0x80000, 0x2fc75890, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13425.c12",  0x80000, 0xf8252589, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13422.a12",  0x80000, 0x48a0754d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13426.c13",  0x80000, 0x5cfb1e25, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13423.a13",  0x80000, 0x335bbc9d, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13417.c7",   0x20000, 0xcd7beb49, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13420.c6",   0x40000, 0x3de0dee4, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13419.c5",   0x40000, 0xe7021b0a, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13418.c4",   0x40000, 0x0732594d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0165.key",   0x02000, 0xa04267ab, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Lghostu)
STD_ROM_FN(Lghostu)

static struct BurnRomInfo LghostudRomDesc[] = {
	{ "bootleg_epr-13427.a4",   0x40000, 0x8e9e092e, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13428.a6",   0x40000, 0x346d4ba3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13411.a5",           0x40000, 0x5160167b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13413.a7",           0x40000, 0x656b3bd8, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "epr-13414.c1",           0x40000, 0xdada2419, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13415.c2",           0x40000, 0xbbb62c48, SYS16_ROM_TILES | BRF_GRA },
	{ "epr-13416.c3",           0x40000, 0x1d11dbae, SYS16_ROM_TILES | BRF_GRA },

	{ "epr-13604.c10",          0x80000, 0x4009c8e5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13603.a10",          0x80000, 0x5350a94e, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13424.c11",          0x80000, 0xfb98d920, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13421.a11",          0x80000, 0x2fc75890, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13425.c12",          0x80000, 0xf8252589, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13422.a12",          0x80000, 0x48a0754d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13426.c13",          0x80000, 0x5cfb1e25, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13423.a13",          0x80000, 0x335bbc9d, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13417.c7",           0x20000, 0xcd7beb49, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13420.c6",           0x40000, 0x3de0dee4, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13419.c5",           0x40000, 0xe7021b0a, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13418.c4",           0x40000, 0x0732594d, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Lghostud)
STD_ROM_FN(Lghostud)

static struct BurnRomInfo MwalkRomDesc[] = {
	{ "epr-13235.a6",   0x40000, 0x6983e129, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13234.a5",   0x40000, 0xc9fd20f2, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0159.key",   0x02000, 0x507838f0, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalk)
STD_ROM_FN(Mwalk)

static struct BurnRomInfo MwalkdRomDesc[] = {
	{ "bootleg_epr-13235.a6",   0x40000, 0x9973f6c3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13234.a5",   0x40000, 0xef5098e9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalkd)
STD_ROM_FN(Mwalkd)

static struct BurnRomInfo MwalkjRomDesc[] = {
	{ "epr-13227.a6",   0x40000, 0x6c0534b3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13226.a5",   0x40000, 0x99765854, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0157.key",   0x02000, 0x324d6931, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalkj)
STD_ROM_FN(Mwalkj)

static struct BurnRomInfo MwalkjdRomDesc[] = {
	{ "bootleg_epr-13227.a6",   0x40000, 0xd8e00521, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13226.a5",   0x40000, 0xcb0ba822, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalkjd)
STD_ROM_FN(Mwalkjd)

static struct BurnRomInfo MwalkuRomDesc[] = {
	{ "epr-13233.a6",   0x40000, 0xf3dac671, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-13232.a5",   0x40000, 0x541d8bdf, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0158.key",   0x02000, 0xa8a50e8c, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalku)
STD_ROM_FN(Mwalku)

static struct BurnRomInfo MwalkudRomDesc[] = {
	{ "bootleg_epr-13233.a6",   0x40000, 0xb34fe725, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-13232.a5",   0x40000, 0x0d100fd4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13216.b1",   0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13217.b2",   0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-13218.b3",   0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-13224.b11",  0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13231.a11",  0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13223.b10",  0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13230.a10",  0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13222.b9",   0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-13229.a9",   0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13221.b8",   0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-13228.a8",   0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-13225.a4",   0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-13219.b4",   0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13220.b5",   0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-13249.b6",   0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "315-5437.ic4",   0x01000, 0x4bf63bc1, SYS16_ROM_I8751 | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Mwalkud)
STD_ROM_FN(Mwalkud)

static struct BurnRomInfo Mwalkbl2RomDesc[] = {
	{ "mwalkbl.01",     0x10000, 0xf49cdb16, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.05",     0x10000, 0xc483f29f, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.02",     0x10000, 0x0bde1896, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.06",     0x10000, 0x5b9fc688, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.03",     0x10000, 0x0c5fe15c, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.07",     0x10000, 0x9e600704, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.04",     0x10000, 0x64692f79, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "mwalkbl.08",     0x10000, 0x546ca530, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr13216.b1",    0x40000, 0x862d2c03, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr13217.b2",    0x40000, 0x7d1ac3ec, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr13218.b3",    0x40000, 0x56d3393c, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr13224.b11",   0x40000, 0xc59f107b, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr13231.a11",   0x40000, 0xa5e96346, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr13223.b10",   0x40000, 0x364f60ff, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr13230.a10",   0x40000, 0x9550091f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr13222.b9",    0x40000, 0x523df3ed, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr13229.a9",    0x40000, 0xf40dc45d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr13221.b8",    0x40000, 0x9ae7546a, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr13228.a8",    0x40000, 0xde3786be, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr13225.a4",    0x20000, 0x56c2e82b, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr13219.b4",    0x40000, 0x19e2061f, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr13220.b5",    0x40000, 0x58d4d9ce, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr13249.b6",    0x40000, 0x623edc5d, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Mwalkbl2)
STD_ROM_FN(Mwalkbl2)

static struct BurnRomInfo ShdancerRomDesc[] = {
	{ "epr-12774b.a6",  0x40000, 0x3d5b3fa9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-12773b.a5",  0x40000, 0x2596004e, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12712.b1",   0x40000, 0x9bdabe3d, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12713.b2",   0x40000, 0x852d2b1c, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12714.b3",   0x40000, 0x448226ce, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-12719.b11",  0x40000, 0xd6888534, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12726.a11",  0x40000, 0xff344945, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12718.b10",  0x40000, 0xba2efc0c, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12725.a10",  0x40000, 0x268a0c17, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12717.b9",   0x40000, 0xc81cc4f8, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12724.a9",   0x40000, 0x0f4903dc, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12716.b8",   0x40000, 0xa870e629, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12723.a8",   0x40000, 0xc606cf90, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12987.a4",   0x20000, 0xd1c020cc, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12715.b4",   0x40000, 0x07051a52, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Shdancer)
STD_ROM_FN(Shdancer)

static struct BurnRomInfo Shdancer1RomDesc[] = {
	{ "epr-12772b.a6",  0x40000, 0x6868a4d4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-12771b.a5",  0x40000, 0x04e30c84, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12712.b1",   0x40000, 0x9bdabe3d, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12713.b2",   0x40000, 0x852d2b1c, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12714.b3",   0x40000, 0x448226ce, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-12719.b11",  0x40000, 0xd6888534, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12726.a11",  0x40000, 0xff344945, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12718.b10",  0x40000, 0xba2efc0c, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12725.a10",  0x40000, 0x268a0c17, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12717.b9",   0x40000, 0xc81cc4f8, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12724.a9",   0x40000, 0x0f4903dc, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12716.b8",   0x40000, 0xa870e629, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12723.a8",   0x40000, 0xc606cf90, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12720.a4",   0x20000, 0x7a0d8de1, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12715.b4",   0x40000, 0x07051a52, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Shdancer1)
STD_ROM_FN(Shdancer1)

static struct BurnRomInfo ShdancerjRomDesc[] = {
	{ "epr-12722b.a6",  0x40000, 0xc00552a2, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-12721b.a5",  0x40000, 0x653d351a, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12712.b1",   0x40000, 0x9bdabe3d, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12713.b2",   0x40000, 0x852d2b1c, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-12714.b3",   0x40000, 0x448226ce, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-12719.b11",  0x40000, 0xd6888534, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12726.a11",  0x40000, 0xff344945, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12718.b10",  0x40000, 0xba2efc0c, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12725.a10",  0x40000, 0x268a0c17, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12717.b9",   0x40000, 0xc81cc4f8, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-12724.a9",   0x40000, 0x0f4903dc, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12716.b8",   0x40000, 0xa870e629, SYS16_ROM_SPRITES | BRF_GRA },
	{ "epr-12723.a8",   0x40000, 0xc606cf90, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-12720.a4",   0x20000, 0x7a0d8de1, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-12715.b4",   0x40000, 0x07051a52, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Shdancerj)
STD_ROM_FN(Shdancerj)

static struct BurnRomInfo ShdancblRomDesc[] = {
	{ "ic39",           0x10000, 0xadc1781c, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic53",           0x10000, 0x1c1ac463, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic38",           0x10000, 0xcd6e155b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic52",           0x10000, 0xbb3c49a4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic37",           0x10000, 0x1bd8d5c3, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic51",           0x10000, 0xce2e71b4, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic36",           0x10000, 0xbb861290, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "ic50",           0x10000, 0x7f7b82b1, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "ic4",            0x20000, 0xf0a016fe, SYS16_ROM_TILES | BRF_GRA },
	{ "ic18",           0x20000, 0xf6bee053, SYS16_ROM_TILES | BRF_GRA },
	{ "ic3",            0x20000, 0xe07e6b5d, SYS16_ROM_TILES | BRF_GRA },
	{ "ic17",           0x20000, 0xf59deba1, SYS16_ROM_TILES | BRF_GRA },
	{ "ic2",            0x20000, 0x60095070, SYS16_ROM_TILES | BRF_GRA },
	{ "ic16",           0x20000, 0x0f0d5dd3, SYS16_ROM_TILES | BRF_GRA },

	{ "ic73",           0x10000, 0x59e77c96, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic58",           0x10000, 0x9cd5c8c7, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic74",           0x10000, 0x90ea5407, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic59",           0x10000, 0xff40e872, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic75",           0x10000, 0x27d2fa61, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic60",           0x10000, 0x826d7245, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic76",           0x10000, 0xf36db688, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic61",           0x10000, 0xdcf8068b, SYS16_ROM_SPRITES | BRF_GRA },

	{ "ic77",           0x10000, 0xf93470b7, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic62",           0x10000, 0x50ca8065, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic78",           0x10000, 0xe533be5d, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic63",           0x10000, 0xd1866aa9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic95",           0x10000, 0x828b8294, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic90",           0x10000, 0x3602b758, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic94",           0x10000, 0xe2fa2b41, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic89",           0x10000, 0x1ba4be93, SYS16_ROM_SPRITES | BRF_GRA },

	{ "ic79",           0x10000, 0xf22548ee, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic64",           0x10000, 0x7a8b7bcc, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic80",           0x10000, 0x6209f7f9, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic65",           0x10000, 0x90ffca14, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic81",           0x10000, 0x34692f23, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic66",           0x10000, 0x5d655517, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic82",           0x10000, 0x7ae40237, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic67",           0x10000, 0x0e5d0855, SYS16_ROM_SPRITES | BRF_GRA },

	{ "ic83",           0x10000, 0xa9040a32, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic68",           0x10000, 0x8d684e53, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic84",           0x10000, 0xd6810031, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic69",           0x10000, 0xc47d32e2, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic92",           0x10000, 0xb57d5cb5, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic88",           0x10000, 0x9de140e1, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic91",           0x10000, 0x49def6c8, SYS16_ROM_SPRITES | BRF_GRA },
	{ "ic87",           0x10000, 0x8172a991, SYS16_ROM_SPRITES | BRF_GRA },

	{ "ic45",           0x10000, 0x576b3a81, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "ic46",           0x10000, 0xc84e8c84, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Shdancbl)
STD_ROM_FN(Shdancbl)

static struct BurnRomInfo WwallyjRomDesc[] = {
	{ "epr-14730b.a4",  0x040000, 0xe72bc17a, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14731b.a6",  0x040000, 0x6e3235b9, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14719.c1",   0x040000, 0x8b58c743, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14720.c2",   0x040000, 0xf96d19f4, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14721.c3",   0x040000, 0xc4ced91d, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14726.c10",  0x100000, 0x7213d1d3, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14732.a10",  0x100000, 0x04ced549, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14727.c11",  0x100000, 0x3b74e0f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14733.a11",  0x100000, 0x6da0444f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14728.c12",  0x080000, 0x5b921587, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14734.a12",  0x080000, 0x6f3f5ed9, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14725.c7",   0x020000, 0x2b29684f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14724.c6",   0x080000, 0x47cbea86, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14723.c5",   0x080000, 0xbc5adc27, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14722.c4",   0x080000, 0x1bd081f8, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0197b.key",  0x002000, 0xf5b7c5b4, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Wwallyj)
STD_ROM_FN(Wwallyj)

static struct BurnRomInfo WwallyjdRomDesc[] = {
	{ "bootleg_epr-14730b.a4",   0x40000, 0x1d46f791, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14731b.a6",   0x40000, 0xa0f6325e, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14719.c1",            0x40000, 0x8b58c743, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14720.c2",            0x40000, 0xf96d19f4, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14721.c3",            0x40000, 0xc4ced91d, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14726.c10",           0x100000, 0x7213d1d3, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14732.a10",           0x100000, 0x04ced549, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14727.c11",           0x100000, 0x3b74e0f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14733.a11",           0x100000, 0x6da0444f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14728.c12",           0x80000, 0x5b921587, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14734.a12",           0x80000, 0x6f3f5ed9, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14725.c7",            0x20000, 0x2b29684f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14724.c6",            0x80000, 0x47cbea86, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14723.c5",            0x80000, 0xbc5adc27, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14722.c4",            0x80000, 0x1bd081f8, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Wwallyjd)
STD_ROM_FN(Wwallyjd)

static struct BurnRomInfo WwallyjaRomDesc[] = {
	{ "epr-14730a.a4",  0x040000, 0xdaa7880e, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14731a.a6",  0x040000, 0x5e36353b, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14719.c1",   0x040000, 0x8b58c743, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14720.c2",   0x040000, 0xf96d19f4, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14721.c3",   0x040000, 0xc4ced91d, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14726.c10",  0x100000, 0x7213d1d3, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14732.a10",  0x100000, 0x04ced549, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14727.c11",  0x100000, 0x3b74e0f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14733.a11",  0x100000, 0x6da0444f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14728.c12",  0x080000, 0x5b921587, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14734.a12",  0x080000, 0x6f3f5ed9, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14725.c7",   0x020000, 0x2b29684f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14724.c6",   0x080000, 0x47cbea86, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14723.c5",   0x080000, 0xbc5adc27, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14722.c4",   0x080000, 0x1bd081f8, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0197a.key",  0x002000, 0x2fb6a9a1, SYS16_ROM_KEY | BRF_ESS | BRF_PRG},
};


STD_ROM_PICK(Wwallyja)
STD_ROM_FN(Wwallyja)

static struct BurnRomInfo WwallyjadRomDesc[] = {
	{ "bootleg_epr-14730a.a4",   0x40000, 0x0d9fb6df, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "bootleg_epr-14731a.a6",   0x40000, 0xb88c140d, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14719.c1",            0x40000, 0x8b58c743, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14720.c2",            0x40000, 0xf96d19f4, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14721.c3",            0x40000, 0xc4ced91d, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14726.c10",           0x100000, 0x7213d1d3, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14732.a10",           0x100000, 0x04ced549, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14727.c11",           0x100000, 0x3b74e0f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14733.a11",           0x100000, 0x6da0444f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14728.c12",           0x80000, 0x5b921587, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14734.a12",           0x80000, 0x6f3f5ed9, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14725.c7",            0x20000, 0x2b29684f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14724.c6",            0x80000, 0x47cbea86, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14723.c5",            0x80000, 0xbc5adc27, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14722.c4",            0x80000, 0x1bd081f8, SYS16_ROM_RF5C68DATA | BRF_SND },
};


STD_ROM_PICK(Wwallyjad)
STD_ROM_FN(Wwallyjad)

static struct BurnRomInfo Wwallyja3pRomDesc[] = {
	{ "epr-14730a.a4",  0x040000, 0xd897bcc6, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },
	{ "epr-14731a.a6",  0x040000, 0x64328385, SYS16_ROM_PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14719.c1",   0x040000, 0x8b58c743, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14720.c2",   0x040000, 0xf96d19f4, SYS16_ROM_TILES | BRF_GRA },
	{ "mpr-14721.c3",   0x040000, 0xc4ced91d, SYS16_ROM_TILES | BRF_GRA },

	{ "mpr-14726.c10",  0x100000, 0x7213d1d3, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14732.a10",  0x100000, 0x04ced549, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14727.c11",  0x100000, 0x3b74e0f0, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14733.a11",  0x100000, 0x6da0444f, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14728.c12",  0x080000, 0x5b921587, SYS16_ROM_SPRITES | BRF_GRA },
	{ "mpr-14734.a12",  0x080000, 0x6f3f5ed9, SYS16_ROM_SPRITES | BRF_GRA },

	{ "epr-14725.c7",   0x020000, 0x2b29684f, SYS16_ROM_Z80PROG | BRF_ESS | BRF_PRG },

	{ "mpr-14724.c6",   0x080000, 0x47cbea86, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14723.c5",   0x080000, 0xbc5adc27, SYS16_ROM_RF5C68DATA | BRF_SND },
	{ "mpr-14722.c4",   0x080000, 0x1bd081f8, SYS16_ROM_RF5C68DATA | BRF_SND },

	{ "317-0198a.key",  0x002000, 0x32d313a4, SYS16_ROM_KEY | BRF_ESS | BRF_PRG },
};


STD_ROM_PICK(Wwallyja3p)
STD_ROM_FN(Wwallyja3p)

/*====================================================
Memory Handlers
====================================================*/

static UINT8 misc_io_data[0x10];

UINT8 system18_io_chip_r(UINT32 offset)
{
	switch (offset) {
		case 0x03:
		case 0x07: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return 0xff;
		}

		case 0x00: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return 0xff - System16Input[1];
		}

		case 0x01: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return 0xff - System16Input[2];
		}

		case 0x02: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return 0xff - System16Input[3];
		}

		case 0x04: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return 0xff - System16Input[0];
		}

		case 0x05: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return System16Dip[0];
		}

		case 0x06: {
			if (misc_io_data[0x1e/2] & (1 << offset)) return misc_io_data[offset];
			return System16Dip[1];
		}

		case 0x08: {
			return 'S';
		}

		case 0x09: {
			return 'E';
		}

		case 0x0a: {
			return 'G';
		}

		case 0x0b: {
			return 'A';
		}

		case 0x0c:
		case 0x0e: {
			return misc_io_data[0x0e];
		}

		case 0x0d:
		case 0x0f: {
			return misc_io_data[0x0f];
		}
	}

	return 0xff;
}

void system18_io_chip_w(UINT32 offset, UINT16 d)
{
	UINT8 old;

	offset &= 0x1f/2;

	old = misc_io_data[offset];
	misc_io_data[offset] = d;

	switch (offset) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x04:
		case 0x05:
		case 0x06: {
			return;
		}

		case 0x07: {
			for (INT32 i = 0; i < 4; i++) {
				if (System16TileBanks[0 + i] != ((d & 0xf) * 4 + i)) {
					System16TileBanks[0 + i] = (d & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
				if (System16TileBanks[4 + i] != (((d >> 4) & 0xf) * 4 + i)) {
					System16TileBanks[4 + i] = ((d >> 4) & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
			}
			return;
		}

		case 0x0e: {
			System16VideoEnable = d & 0x02;
			if ((old ^ d) & 4) System18VdpEnable = d & 0x04;
			return;
		}

	}
}

static UINT8 DdcrewReadIO(UINT32 offset)
{
	switch (offset) {
		case 0x1810: {
			return 0xff - System16Input[3];
		}

		case 0x1811: {
			return 0xff - System16Input[4];
		}

		case 0x1812: {
			return 0xff - System16Input[5];
		}
	}

	return sega_315_5195_io_read(offset);
}

static UINT8 LghostValue;

static UINT8 LghostReadIO(UINT32 offset)
{
	switch (offset) {
		case 0x1808:
		case 0x1809:
		case 0x180a:
		case 0x180b: {
			UINT8 result = LghostValue | 0x7f;
			LghostValue <<= 1;
			return result;
		}
	}

	return sega_315_5195_io_read(offset);
}

static void LghostWriteIO(UINT32 offset, UINT8 d)
{
	switch (offset) {
		case 0x1808: {
			//LghostValue = ~BurnGunReturnY(0);
			UINT8 pos_value_x = BurnGunReturnX(0);
			UINT8 pos_value_y = 255 - BurnGunReturnY(0);

			// Depending of the position on the X axis, we need to calculate the Y offset accordingly
			if (pos_value_x >= 50 && pos_value_x <= 99)
			{
				// Linear function (decreasing)
				if (pos_value_y >= 130 && pos_value_y <= 225)
					LghostValue = round(pos_value_y * 0.94 + 0.80);
				// Keep real value as no offset is needed
				else
					LghostValue = pos_value_y;
			}
			else if (pos_value_x >= 100 && pos_value_x <= 199)
			{
				// Linear function (decreasing)
				if (pos_value_y >= 100 && pos_value_y <= 225)
					LghostValue = round(pos_value_y * 0.89 + 6.00);
				// Keep real value as no offset is needed
				else
					LghostValue = pos_value_y;
			}
			else if (pos_value_x >= 200 && pos_value_x <= 249)
			{
				// Linear function (decreasing) #1
				if (pos_value_y >= 30 && pos_value_y <= 55)
					LghostValue = round(pos_value_y * 0.78 + 18.28);

				// Linear function (decreasing) #2
				else if (pos_value_y >= 100 && pos_value_y <= 205)
					LghostValue = round(pos_value_y * 0.70 + 28.00);
				// Linear function (decreasing) #2
				else if (pos_value_y >= 206 && pos_value_y <= 225)
					LghostValue = round(pos_value_y * 1.58 - 151.48);
				// Keep real value as no offset is needed
				else
					LghostValue = pos_value_y;
			}
			// if crosshair is near the left edge, keep real value as no offset is needed
			else
				LghostValue = pos_value_y;

			return;
		}

		case 0x1809: {
			//LghostValue = BurnGunReturnX(0);
			UINT8 pos_value_x = BurnGunReturnX(0);

			// Here, linear functions (increasing) are used
			// The line is divided in two parts to get more precise results

			// Linear function (increasing) #1
			if (pos_value_x >= 26 && pos_value_x <= 85)
				LghostValue = round(pos_value_x * 1.13 + 0.95);

			// Linear function (increasing) #2
			else if (pos_value_x >= 86 && pos_value_x <= 140)
				LghostValue = round(pos_value_x * 1.10 + 4.00);

			// Here, linear functions (decreasing) are used
			// The line is divided in two parts to get more precise results

			// Linear function (decreasing) #1
			else if (pos_value_x >= 141 && pos_value_x <= 190)
				LghostValue = round(pos_value_x * 1.02 + 11.20);

			// Linear function (decreasing) #2
			else if (pos_value_x >= 191 && pos_value_x <= 240)
				LghostValue = round(pos_value_x * 0.76 + 62.60);

			// if crosshair is near the edges, keep real value as no offset is needed
			else
				LghostValue = pos_value_x;

			return;
		}

		case 0x180a: {
			//LghostValue = (System16AnalogSelect) ? ~BurnGunReturnY(2) : ~BurnGunReturnY(1);
			// Player 3, Y axis
			if (System16AnalogSelect)
			{
				UINT8 pos_value_x = BurnGunReturnX(2);
				UINT8 pos_value_y = 255 - BurnGunReturnY(2);

				// Depending of the position on the X axis, we need to calculate the Y offset accordingly
				if (pos_value_x >= 128 && pos_value_x <= 255)
				{
					// Linear function (increasing)
					if (pos_value_y >= 30 && pos_value_y <= 125)
						LghostValue = round(pos_value_y * 1.01 + 11.82);
					// Linear function (decreasing)
					else if (pos_value_y >= 126 && pos_value_y <= 235)
						LghostValue = round(pos_value_y * 0.94 + 21.90);
					// Keep real value as no offset is needed
					else
						LghostValue = pos_value_y;
				}
				else if (pos_value_x >= 17 && pos_value_x <= 127)
				{
					// Linear function (increasing)
					if (pos_value_y >= 40 && pos_value_y <= 145)
						LghostValue = round(pos_value_y * 0.82 + 31.80);
					// Linear function (decreasing)
					else if (pos_value_y >= 200 && pos_value_y <= 225)
						LghostValue = round(pos_value_y * 0.83 + 29.95);
					// Keep real value as no offset is needed
					else
						LghostValue = pos_value_y;
				}
				// Keep real value as no offset is needed
				else
					LghostValue = pos_value_y;
			}
			// Player 2, Y axis. It doesn't need any offset adjustement.
			else
				LghostValue = 255 - BurnGunReturnY(1);
			return;
		}

		case 0x180b: {
			//LghostValue = (System16AnalogSelect) ? BurnGunReturnX(2) : BurnGunReturnX(1);
			// Player 3, X axis
			if (System16AnalogSelect)
			{
				UINT8 pos_value_x = BurnGunReturnX(2);

				// Right edge of screen, constant value
				if (pos_value_x >= 17 && pos_value_x <= 34)
					LghostValue = pos_value_x - 17;

				// Linear function (increasing)
				else if (pos_value_x >= 35 && pos_value_x <= 110)
					LghostValue = round(pos_value_x * 0.94 - 14.08);

				// Linear function (decreasing) #1
				else if (pos_value_x >= 111 && pos_value_x <= 225)
					LghostValue = round(pos_value_x * 1.15 - 35.65);

				// if crosshair is near the edges, keep real value as no offset is needed*/
				else
					LghostValue = pos_value_x;
				break;
			}
			// Player 2, X axis. It doesn't need any offset adjustement.
			else
				LghostValue = BurnGunReturnX(1);
			return;
		}

		case 0x1810: {
			System16AnalogSelect = d & 1;
			return;
		}

		case 0x181a: {
			// ???
			return;
		}
	}

	sega_315_5195_io_write(offset, d);
}

static UINT8 WwallyReadIO(UINT32 offset)
{
	switch (offset) {
		case 0x1800: {
			return BurnTrackballRead(0, 0);
		}

		case 0x1802: {
			return BurnTrackballRead(0, 1);
		}

		case 0x1804: {
			return BurnTrackballRead(1, 0);
		}

		case 0x1806: {
			return BurnTrackballRead(1, 1);
		}

		case 0x1808: {
			return BurnTrackballRead(2, 0);
		}

		case 0x180a: {
			return BurnTrackballRead(2, 1);
		}
	}

	return sega_315_5195_io_read(offset);
}

static void WwallyWriteIO(UINT32 offset, UINT8 d)
{
	switch (offset) {
		case 0x1800: {
			BurnTrackballReadReset(0, 0);
			BurnTrackballReadReset(0, 1);
			return;
		}

		case 0x1804: {
			BurnTrackballReadReset(1, 0);
			BurnTrackballReadReset(1, 1);
			return;
		}

		case 0x1808: {
			BurnTrackballReadReset(2, 0);
			BurnTrackballReadReset(2, 1);
			return;
		}
	}

	sega_315_5195_io_write(offset, d);
}

void System18GfxBankWrite(UINT32 offset, UINT16 d)
{
	// Tile Banking
	if (offset < 8) {
		INT32 MaxBanks = System16NumTiles / 1024;
		if (d >= MaxBanks) d %= MaxBanks;
		if (System16TileBanks[offset] != d) {
			System16TileBanks[offset] = d;
			System16RecalcBgTileMap = 1;
			System16RecalcBgAltTileMap = 1;
			System16RecalcFgTileMap = 1;
			System16RecalcFgAltTileMap = 1;
		}
	} else {
		// Sprite Banking
		INT32 MaxBanks = System16SpriteRomSize / 0x40000;
		if (d >= MaxBanks) d = 255;
		System16SpriteBanks[(offset - 8) * 2 + 0] = d * 2 + 0;
		System16SpriteBanks[(offset - 8) * 2 + 1] = d * 2 + 1;
	}
}

void HamawayGfxBankWrite(UINT32 offset, UINT16 d)
{
	if (offset < 8) {
		d &= 0x9f;
		if (d & 0x80) d += 0x20;
		d &= 0x3f;
		if (System16TileBanks[offset] != d) {
			System16TileBanks[offset] = d;
			System16RecalcBgTileMap = 1;
			System16RecalcBgAltTileMap = 1;
			System16RecalcFgTileMap = 1;
			System16RecalcFgAltTileMap = 1;
		}
	}
}

static UINT16 __fastcall System18ReadWord(UINT32 a)
{
	if (a >= 0xc00000 && a <= 0xc0000f) {
		return GenesisVDPRead((a & 0xf) >> 1);
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Read Word -> 0x%06X\n"), a);
#endif

	return 0xffff;
}

static UINT8 __fastcall System18ReadByte(UINT32 a)
{
	if (a >= 0xa40000 && a <= 0xa4001f) {
		return system18_io_chip_r((a & 0x1f) >> 1);
	}

	if (a >= 0xe40000 && a <= 0xe4001f) {
		return system18_io_chip_r((a & 0x1f) >> 1);
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Read Byte -> 0x%06X\n"), a);
#endif

	return 0xff;
}

static void __fastcall System18WriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x400000 && a <= 0x40ffff) {
		System16BTileWordWrite(a & 0xffff, d);
		return;
	}

	if (a >= 0xc00000 && a <= 0xc0000f) {
		GenesisVDPWrite((a & 0xf) >> 1, d);
		return;
	}

	switch (a) {
		case 0xe42000: {
			System18VdpMixing = d & 0xff;
			return;
		}
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Write Word -> 0x%06X, 0x%04X\n"), a, d);
#endif
}

static void __fastcall System18WriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x400000 && a <= 0x40ffff) {
		System16BTileByteWrite((a & 0xffff) ^ 1, d);
		return;
	}

	if (a >= 0x3e0000 && a <= 0x3e001f) {
		System18GfxBankWrite((a & 0x1f) >> 1, d);
		return;
	}

	if (a >= 0xa40000 && a <= 0xa41fff) {
		system18_io_chip_w((a & 0x1fff) >> 1, d);
		return;
	}

	if (a >= 0xc00000 && a <= 0xc0000f) {
		GenesisVDPWrite((a & 0xf) >> 1, d);
		return;
	}

	if (a >= 0xe40000 && a <= 0xe41fff) {
		system18_io_chip_w((a & 0x1fff) >> 1, d);
		return;
	}

	switch (a) {
		case 0xa42001:
		case 0xe42001: {
			System18VdpMixing = d & 0xff;
			return;
		}

		case 0xfe0007: {
			System16SoundLatch = d & 0xff;
			ZetOpen(0);
			ZetNmi();
			ZetClose();
			return;
		}
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Write Byte -> 0x%06X, 0x%02X\n"), a, d);
#endif
}

static UINT8 __fastcall Mwalkbl2ReadByte(UINT32 a)
{
	switch (a) {
		case 0xc40001: {
			return System16Dip[0];
		}

		case 0xc40003: {
			return System16Dip[1];
		}

		case 0xc41001: {
			return 0xff - System16Input[0];
		}

		case 0xc41003: {
			return 0xff - System16Input[1];
		}

		case 0xc41005: {
			return 0xff - System16Input[2];
		}

		case 0xc41007: {
			return 0xff - System16Input[3];
		}
	}

	return 0xff;
}

static void __fastcall Mwalkbl2WriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0xc40007: {
			System16SoundLatch = d & 0xff;
			ZetOpen(0);
			ZetNmi();
			ZetClose();
			return;
		}

		case 0xc46601: {
			System16VideoEnable = d & 0x02;
			return;
		}

		case 0xc46801: {
			for (INT32 i = 0; i < 4; i++) {
				if (System16TileBanks[0 + i] != ((d & 0xf) * 4 + i)) {
					System16TileBanks[0 + i] = (d & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
				if (System16TileBanks[4 + i] != (((d >> 4) & 0xf) * 4 + i)) {
					System16TileBanks[4 + i] = ((d >> 4) & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
			}
			return;
		}
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Write Byte -> 0x%06X, 0x%02X\n"), a, d);
#endif
}

static void __fastcall Mwalkbl2WriteWord(UINT32 a, UINT16 d)
{
	switch (a) {
		case 0xc46000: {
			System16ScrollY[1] = d & 0xff;
			return;
		}

		case 0xc46200: {
			System16ScrollY[0] = d & 0xff;
			return;
		}

		case 0xc46400: {
			BootlegBgPage[0] = (d >> 8) & 0xf;
			System16ScrollX[0] = d & 0xff;
			return;
		}
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Write Word -> 0x%06X, 0x%04X\n"), a, d);
#endif
}

static UINT8 __fastcall ShdancblReadByte(UINT32 a)
{
	switch (a) {
		case 0xc40001: {
			return System16Dip[0];
		}

		case 0xc40003: {
			return System16Dip[1];
		}

		case 0xc41001: {
			return 0xff - System16Input[0];
		}

		case 0xc41003: {
			return 0xff - System16Input[1];
		}

		case 0xc41005: {
			return 0xff - System16Input[2];
		}

		case 0xc41007: {
			return 0xff - System16Input[3];
		}
	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Read Byte -> 0x%06X\n"), a);
#endif

	return 0xff;
}

static void __fastcall ShdancblWriteByte(UINT32 a, UINT8 d)
{
	switch (a) {
		case 0xc40007: {
			System16SoundLatch = d & 0xff;
			ZetOpen(0);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);;
			ZetClose();
			return;
		}

		case 0xe4001d: {
			//System16VideoEnable = d & 0x02;
			return;
		}

		case 0xe4000f: {
			for (INT32 i = 0; i < 4; i++) {
				if (System16TileBanks[0 + i] != ((d & 0xf) * 4 + i)) {
					System16TileBanks[0 + i] = (d & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
				if (System16TileBanks[4 + i] != (((d >> 4) & 0xf) * 4 + i)) {
					System16TileBanks[4 + i] = ((d >> 4) & 0xf) * 4 + i;
					System16RecalcBgTileMap = 1;
					System16RecalcBgAltTileMap = 1;
					System16RecalcFgTileMap = 1;
					System16RecalcFgAltTileMap = 1;
				}
			}
			return;
		}

	}

#if 0 && defined FBNEO_DEBUG
	bprintf(PRINT_NORMAL, _T("68000 Write Byte -> 0x%06X, 0x%02X\n"), a, d);
#endif
}

/*====================================================
Driver inits
====================================================*/

static void WwallyMakeAnalogInputs()
{
	BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
	BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x3f);
	BurnTrackballUDLR(0, System16InputPort4[3], System16InputPort4[4], System16InputPort4[1], System16InputPort4[2], 8);
	BurnTrackballUpdate(0);

	BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
	BurnTrackballFrame(1, Analog[2], Analog[3], 0x01, 0x3f);
	BurnTrackballUDLR(1, System16InputPort5[3], System16InputPort5[4], System16InputPort5[1], System16InputPort5[2], 8);
	BurnTrackballUpdate(1);

	BurnTrackballConfig(2, AXIS_REVERSED, AXIS_NORMAL);
	BurnTrackballFrame(2, Analog[4], Analog[5], 0x01, 0x3f);
	BurnTrackballUDLR(2, System16InputPort6[3], System16InputPort6[4], System16InputPort6[1], System16InputPort6[2], 8);
	BurnTrackballUpdate(2);
}

static INT32 System18Bank40000Init()
{
	System16SpriteRomSize = 0x800000 - 0x400000;

	INT32 nRet = System16Init();

	if (!nRet) {
		UINT8 *pTemp = (UINT8*)BurnMalloc(0x400000);
		if (pTemp) {
			memcpy(pTemp, System16Sprites, 0x400000);
			memset(System16Sprites, 0, System16SpriteRomSize);
			memcpy(System16Sprites + 0x000000, pTemp + 0x000000, 0x100000);
			memcpy(System16Sprites + 0x200000, pTemp + 0x100000, 0x100000);
			memcpy(System16Sprites + 0x400000, pTemp + 0x200000, 0x100000);
			memcpy(System16Sprites + 0x600000, pTemp + 0x300000, 0x100000);
		} else {
			nRet = 1;
		}
		BurnFree(pTemp);
	}

	return nRet;
}

static INT32 DdcrewInit()
{
	sega_315_5195_custom_io_do = DdcrewReadIO;

	return System18Bank40000Init();
}

static INT32 HamawayInit()
{
	INT32 nRet = System16Init();

	if (!nRet) {
		HammerAway = true;
		// game is loud - bring the volumes down
		BurnYM3438SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
		BurnYM3438SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
		RF5C68PCMSetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	}

	return nRet;
}

static INT32 LghostInit()
{
	BurnGunInit(3, true);

	sega_315_5195_custom_io_do = LghostReadIO;
	sega_315_5195_custom_io_write_do = LghostWriteIO;

	LaserGhost = true;

	return System18Bank40000Init();
}

static INT32 Mwalkbl2PatchRom()
{
	*((UINT16*)(System16Rom + 0x070212)) = 0x4e71;
	*((UINT16*)(System16Rom + 0x070116)) = 0x4e71;
	*((UINT16*)(System16Rom + 0x00314a)) = 0x4642;
	System16Rom[0x00311a] = 0x3f;
	System16Rom[0x070103] = 0x00;
	System16Rom[0x070109] = 0x00;
	System16Rom[0x007727] = 0x00;
	System16Rom[0x007729] = 0x00;
	System16Rom[0x00780d] = 0x00;
	System16Rom[0x00780f] = 0x00;
	System16Rom[0x007861] = 0x00;
	System16Rom[0x007863] = 0x00;
	System16Rom[0x007d47] = 0x00;
	System16Rom[0x007863] = 0x00;
	System16Rom[0x008533] = 0x00;
	System16Rom[0x008535] = 0x00;
	System16Rom[0x0085bd] = 0x00;
	System16Rom[0x0085bf] = 0x00;
	System16Rom[0x009a4b] = 0x00;
	System16Rom[0x009a4d] = 0x00;
	System16Rom[0x009b2f] = 0x00;
	System16Rom[0x009b31] = 0x00;
	System16Rom[0x00a05b] = 0x00;
	System16Rom[0x00a05d] = 0x00;
	System16Rom[0x00a23f] = 0x00;
	System16Rom[0x00a241] = 0x00;
	System16Rom[0x010159] = 0x00;
	System16Rom[0x01015b] = 0x00;
	System16Rom[0x0109fb] = 0x00;
	System16Rom[0x0109fd] = 0x00;

	return 0;
}

static void Mwalkbl2Map68K()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(System16Rom           , 0x000000, 0x0fffff, MAP_READ);
	SekMapMemory(System16Code          , 0x000000, 0x0fffff, MAP_FETCH);
	SekMapMemory(System16TileRam       , 0x400000, 0x40ffff, MAP_READ);
	SekMapMemory(System16TextRam       , 0x410000, 0x410fff, MAP_RAM);
	SekMapMemory(System16SpriteRam     , 0x440000, 0x4407ff, MAP_RAM);
	SekMapMemory(System16PaletteRam    , 0x840000, 0x840fff, MAP_RAM);
	SekMapMemory(System16Ram           , 0xffc000, 0xffffff, MAP_RAM);

	SekSetReadWordHandler(0, System18ReadWord);
	SekSetWriteWordHandler(0, System18WriteWord);
	SekSetReadByteHandler(0, System18ReadByte);
	SekSetWriteByteHandler(0, System18WriteByte);
	SekClose();
}

static INT32 Mwalkbl2Init()
{
	System16CustomLoadRomDo = Mwalkbl2PatchRom;
	System16Map68KDo = Mwalkbl2Map68K;

	System16SpriteXOffset = 128;

	INT32 nRet = System16Init();

	if (!nRet) {
		SekOpen(0);
		SekMapHandler(1, 0xc40000, 0xc41009, MAP_READ);
		SekSetReadByteHandler(1, Mwalkbl2ReadByte);
		SekMapHandler(2, 0xc40006, 0xc46801, MAP_WRITE);
		SekSetWriteByteHandler(2, Mwalkbl2WriteByte);
		SekSetWriteWordHandler(2, Mwalkbl2WriteWord);
		SekClose();
	}

	return nRet;
}

static void ShdancblMap68K()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(System16Rom           , 0x000000, 0x0fffff, MAP_READ);
	SekMapMemory(System16Code          , 0x000000, 0x0fffff, MAP_FETCH);
	SekMapMemory(System16TileRam       , 0x400000, 0x40ffff, MAP_READ);
	SekMapMemory(System16TextRam       , 0x410000, 0x410fff, MAP_RAM);
	SekMapMemory(System16SpriteRam     , 0x440000, 0x4407ff, MAP_RAM);
	SekMapMemory(System16PaletteRam    , 0x840000, 0x840fff, MAP_RAM);
	SekMapMemory(System16Ram           , 0xffc000, 0xffffff, MAP_RAM);

	SekSetReadWordHandler(0, System18ReadWord);
	SekSetWriteWordHandler(0, System18WriteWord);
	SekSetReadByteHandler(0, System18ReadByte);
	SekSetWriteByteHandler(0, System18WriteByte);
	SekClose();
}

static INT32 ShdancblInit()
{
	System16SpriteXOffset = 112;
	System16Map68KDo = ShdancblMap68K;

	INT32 nRet = System16Init();

	if (!nRet) {
		SekOpen(0);
		SekSetReadByteHandler(0, ShdancblReadByte);
		SekSetWriteByteHandler(0, ShdancblWriteByte);
		SekClose();

		System16VideoEnable = 1;
	}

	return nRet;
}

static INT32 WwallyInit()
{
	System16MakeAnalogInputsDo = WwallyMakeAnalogInputs;
	sega_315_5195_custom_io_do = WwallyReadIO;
	sega_315_5195_custom_io_write_do = WwallyWriteIO;

	BurnTrackballInit(3);

	return System16Init();
}

static INT32 System18Exit()
{
	memset(misc_io_data, 0, sizeof(misc_io_data));
	HammerAway = false;

	GenesisVDPExit();

	return System16Exit();
}

static INT32 System18Scan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {					// Return minimum compatible version
		*pnMin =  0x029660;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(misc_io_data);
		GenesisVDPScan();
	}

	return System16Scan(nAction, pnMin);;
}

static INT32 LghostExit()
{
	LghostValue = 0;

	return System18Exit();
}

static INT32 LghostScan(INT32 nAction,INT32 *pnMin)
{
	if (pnMin != NULL) {					// Return minimum compatible version
		*pnMin =  0x029660;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(LghostValue);
	}

	return System18Scan(nAction, pnMin);;
}

static INT32 WwallyExit()
{
	BurnTrackballExit();

	return System18Exit();
}

static INT32 WwallyScan(INT32 nAction,INT32 *pnMin)
{
	if (pnMin != NULL) {					// Return minimum compatible version
		*pnMin =  0x029660;
	}

	if (nAction & ACB_DRIVER_DATA) {
		BurnTrackballScan();
	}

	return System18Scan(nAction, pnMin);;
}

/*====================================================
Driver defs
====================================================*/

struct BurnDriver BurnDrvAstorm = {
	"astorm", NULL, NULL, NULL, "1990",
	"Alien Storm (set 4, World, 2 Players, FD1094 317-0154)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, AstormRomInfo, AstormRomName, NULL, NULL, NULL, NULL, System18InputInfo, Astorm2pDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstorm3 = {
	"astorm3", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 3, World, 3 Players, FD1094 317-0148)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, Astorm3RomInfo, Astorm3RomName, NULL, NULL, NULL, NULL, AstormInputInfo, AstormDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstorm3d = {
	"astorm3d", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 3, World, 3 Players, FD1094 317-0148 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, Astorm3dRomInfo, Astorm3dRomName, NULL, NULL, NULL, NULL, AstormInputInfo, AstormDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstormj = {
	"astormj", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 1, Japan Rev B, 2 Players, FD1094 317-0146)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, AstormjRomInfo, AstormjRomName, NULL, NULL, NULL, NULL, System18InputInfo, Astorm2pDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstormjd = {
	"astormjd", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 1, Japan Rev B, 2 Players, FD1094 317-0146 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, AstormjdRomInfo, AstormjdRomName, NULL, NULL, NULL, NULL, System18InputInfo, Astorm2pDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstormu = {
	"astormu", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 2, US, 3 Players, FD1094 317-0147)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, AstormuRomInfo, AstormuRomName, NULL, NULL, NULL, NULL, AstormInputInfo, AstormDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvAstormud = {
	"astormud", "astorm", NULL, NULL, "1990",
	"Alien Storm (set 2, US, 3 Players, FD1094 317-0147 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, AstormudRomInfo, AstormudRomName, NULL, NULL, NULL, NULL, AstormInputInfo, AstormDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvBloxeed = {
	"bloxeed", NULL, NULL, NULL, "1989",
	"Bloxeed (Japan, FD1094 317-0139)\0", NULL, "Sega / Elorg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PUZZLE, 0,
	NULL, BloxeedRomInfo, BloxeedRomName, NULL, NULL, NULL, NULL, System18InputInfo, BloxeedDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvBloxeeda = {
	"bloxeeda", "bloxeed", NULL, NULL, "1989",
	"Bloxeed (Japan, earlier) (FD1094 317-0139)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PUZZLE, 0,
	NULL, BloxeedaRomInfo, BloxeedaRomName, NULL, NULL, NULL, NULL, System18InputInfo, BloxeedDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvBloxeedd = {
	"bloxeedd", "bloxeed", NULL, NULL, "1990",
	"Bloxeed (Japan, FD1094 317-0139 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PUZZLE, 0,
	NULL, BloxeeddRomInfo, BloxeeddRomName, NULL, NULL, NULL, NULL, System18InputInfo, BloxeedDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvCltchitr = {
	"cltchitr", NULL, NULL, NULL, "1991",
	"Clutch Hitter (set 2, US, FD1094 317-0176)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SPORTSMISC, 0,
	NULL, CltchitrRomInfo, CltchitrRomName, NULL, NULL, NULL, NULL, System18InputInfo, CltchitrDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvCltchitrd = {
	"cltchitrd", "cltchitr", NULL, NULL, "1991",
	"Clutch Hitter (set 2, US, FD1094 317-0176 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SPORTSMISC, 0,
	NULL, CltchitrdRomInfo, CltchitrdRomName, NULL, NULL, NULL, NULL, System18InputInfo, CltchitrDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvCltchitrj = {
	"cltchitrj", "cltchitr", NULL, NULL, "1991",
	"Clutch Hitter (set 1, Japan, FD1094 317-0175)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SPORTSMISC, 0,
	NULL, CltchitrjRomInfo, CltchitrjRomName, NULL, NULL, NULL, NULL, System18InputInfo, CltchitrDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvCltchitrjd = {
	"cltchitrjd", "cltchitr", NULL, NULL, "1991",
	"Clutch Hitter (set 1, Japan, FD1094 317-0175 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SPORTSMISC, 0,
	NULL, CltchitrjdRomInfo, CltchitrjdRomName, NULL, NULL, NULL, NULL, System18InputInfo, CltchitrDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrew = {
	"ddcrew", NULL, NULL, NULL, "1991",
	"D. D. Crew (set 4, World, 3 Players, FD1094 317-0190)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewRomInfo, DdcrewRomName, NULL, NULL, NULL, NULL, DdcrewInputInfo, DdcrewDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewd = {
	"ddcrewd", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 4, World, 3 Players, FD1094 317-0190 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewdRomInfo, DdcrewdRomName, NULL, NULL, NULL, NULL, DdcrewInputInfo, DdcrewDIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrew1 = {
	"ddcrew1", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 1, World, 4 Players, FD1094 317-?)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrew1RomInfo, Ddcrew1RomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrew1d = {
	"ddcrew1d", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 1, World, 4 Players, FD1094 317-? decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrew1dRomInfo, Ddcrew1dRomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrew2 = {
	"ddcrew2", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 2, World, 2 Players, FD1094 317-0184)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrew2RomInfo, Ddcrew2RomName, NULL, NULL, NULL, NULL, Ddcrew2InputInfo, Ddcrew2DIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrew2d = {
	"ddcrew2d", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 2, World, 2 Players, FD1094 317-0184 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrew2dRomInfo, Ddcrew2dRomName, NULL, NULL, NULL, NULL, Ddcrew2InputInfo, Ddcrew2DIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewj = {
	"ddcrewj", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 5, Japan, 4 Players, FD1094 317-0185)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewjRomInfo, DdcrewjRomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewjd = {
	"ddcrewjd", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 5, Japan, 4 Players, FD1094 317-0185 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewjdRomInfo, DdcrewjdRomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewj2 = {
	"ddcrewj2", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 6, Japan, 2 Players, FD1094 317-0182)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrewj2RomInfo, Ddcrewj2RomName, NULL, NULL, NULL, NULL, Ddcrew2InputInfo, Ddcrew2DIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewj2d = {
	"ddcrewj2d", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 6, Japan, 2 Players, FD1094 317-0182 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, Ddcrewj2dRomInfo, Ddcrewj2dRomName, NULL, NULL, NULL, NULL, Ddcrew2InputInfo, Ddcrew2DIPInfo,
	System18Bank40000Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewu = {
	"ddcrewu", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 3, US, 4 Players, FD1094 317-0186)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewuRomInfo, DdcrewuRomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDdcrewud = {
	"ddcrewud", "ddcrew", NULL, NULL, "1991",
	"D. D. Crew (set 3, US, 4 Players, FD1094 317-0186 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SCRFIGHT, 0,
	NULL, DdcrewudRomInfo, DdcrewudRomName, NULL, NULL, NULL, NULL, DdcrewuInputInfo, DdcrewuDIPInfo,
	DdcrewInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvDesertbr = {
	"desertbr", NULL, NULL, NULL, "1992",
	"Desert Breaker (World, FD1094 317-0196)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_RUNGUN, 0,
	NULL, DesertbrRomInfo, DesertbrRomName, NULL, NULL, NULL, NULL, DesertbrInputInfo, DesertbrDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 224, 320, 3, 4
};

struct BurnDriver BurnDrvDesertbrd = {
	"desertbrd", "desertbr", NULL, NULL, "1992",
	"Desert Breaker (World, FD1094 317-0196 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_RUNGUN, 0,
	NULL, DesertbrdRomInfo, DesertbrdRomName, NULL, NULL, NULL, NULL, DesertbrInputInfo, DesertbrDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 224, 320, 3, 4
};

struct BurnDriver BurnDrvDesertbrj = {
	"desertbrj", "desertbr", NULL, NULL, "1992",
	"Desert Breaker (Japan, FD1094 317-0194)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_RUNGUN, 0,
	NULL, DesertbrjRomInfo, DesertbrjRomName, NULL, NULL, NULL, NULL, DesertbrInputInfo, DesertbrDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 224, 320, 3, 4
};

struct BurnDriver BurnDrvDesertbrjd = {
	"desertbrjd", "desertbr", NULL, NULL, "1992",
	"Desert Breaker (Japan, FD1094 317-0194 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_RUNGUN, 0,
	NULL, DesertbrjdRomInfo, DesertbrjdRomName, NULL, NULL, NULL, NULL, DesertbrInputInfo, DesertbrDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 224, 320, 3, 4
};

struct BurnDriver BurnDrvHamaway = {
	"hamaway", NULL, NULL, NULL, "1992",
	"Hammer Away (prototype)\0", NULL, "Sega / Santos", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_837_7525, GBF_PUZZLE, 0,
	NULL, HamawayRomInfo, HamawayRomName, NULL, NULL, NULL, NULL, System18InputInfo, HamawayDIPInfo,
	HamawayInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 224, 320, 3, 4
};

struct BurnDriver BurnDrvLghost = {
	"lghost", NULL, NULL, NULL, "1990",
	"Laser Ghost (World, 317-0166)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SHOOT, 0,
	NULL, LghostRomInfo, LghostRomName, NULL, NULL, NULL, NULL, LghostInputInfo, LghostDIPInfo,
	LghostInit, LghostExit, System18Frame, System18Render, LghostScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvLghostd = {
	"lghostd", "lghost", NULL, NULL, "1990",
	"Laser Ghost (World, 317-0166 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SHOOT, 0,
	NULL, LghostdRomInfo, LghostdRomName, NULL, NULL, NULL, NULL, LghostInputInfo, LghostDIPInfo,
	LghostInit, LghostExit, System18Frame, System18Render, LghostScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvLghostj = {
	"lghostj", "lghost", NULL, NULL, "1990",
	"Laser Ghost (Japan, 317-0164)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SHOOT, 0,
	NULL, LghostjRomInfo, LghostjRomName, NULL, NULL, NULL, NULL, LghostInputInfo, LghostDIPInfo,
	LghostInit, LghostExit, System18Frame, System18Render, LghostScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvLghostu = {
	"lghostu", "lghost", NULL, NULL, "1990",
	"Laser Ghost (US, 317-0165)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_SHOOT, 0,
	NULL, LghostuRomInfo, LghostuRomName, NULL, NULL, NULL, NULL, LghostInputInfo, LghostDIPInfo,
	LghostInit, LghostExit, System18Frame, System18Render, LghostScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvLghostud = {
	"lghostud", "lghost", NULL, NULL, "1990",
	"Laser Ghost (US, 317-0165 decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_SHOOT, 0,
	NULL, LghostudRomInfo, LghostudRomName, NULL, NULL, NULL, NULL, LghostInputInfo, LghostDIPInfo,
	LghostInit, LghostExit, System18Frame, System18Render, LghostScan,
	NULL, 0x1800, 320, 224, 4, 3
};

static INT32 MwalkInit()
{
	System16MakeInputsDo = MwalkFixInputs;

	return System16Init();
}

static INT32 MwalkudInit()
{
	System16MakeInputsDo = MwalkudFixInputs;

	return System16Init();
}

struct BurnDriver BurnDrvMWalk = {
	"mwalk", NULL, NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (set 3, World, FD1094/8751 317-0159)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkRomInfo, MwalkRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	MwalkInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalkd = {
	"mwalkd", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (World) (bootleg of FD1094/8751 317-0159 set)\0", NULL, "bootleg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkdRomInfo, MwalkdRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	MwalkInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalkj = {
	"mwalkj", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (Japan) (FD1094/8751 317-0157)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkjRomInfo, MwalkjRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	MwalkInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalkjd = {
	"mwalkjd", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (Japan) (bootleg of FD1094/8751 317-0157 set)\0", NULL, "bootleg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkjdRomInfo, MwalkjdRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	MwalkInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalku = {
	"mwalku", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (US) (FD1094/8751 317-0158)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkuRomInfo, MwalkuRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	MwalkInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalkud = {
	"mwalkud", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (US) (bootleg of FD1094/8751 317-0158 set)\0", NULL, "bootleg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5874, GBF_PLATFORM, 0,
	NULL, MwalkudRomInfo, MwalkudRomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkudDIPInfo,
	MwalkudInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvMWalkbl2 = {
	"mwalkbl2", "mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (bootleg)\0", NULL, "bootleg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 3, HARDWARE_SEGA_SYSTEM18, GBF_PLATFORM, 0,
	NULL, Mwalkbl2RomInfo, Mwalkbl2RomName, NULL, NULL, NULL, NULL, MwalkInputInfo, MwalkDIPInfo,
	Mwalkbl2Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvShdancer = {
	"shdancer", NULL, NULL, NULL, "1989",
	"Shadow Dancer (set 3, US)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_SHADOW, GBF_PLATFORM, 0,
	NULL, ShdancerRomInfo, ShdancerRomName, NULL, NULL, NULL, NULL, System18InputInfo, ShdancerDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvShdancer1 = {
	"shdancer1", "shdancer", NULL, NULL, "1989",
	"Shadow Dancer (set 1)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_SHADOW, GBF_PLATFORM, 0,
	NULL, Shdancer1RomInfo, Shdancer1RomName, NULL, NULL, NULL, NULL, System18InputInfo, ShdancerDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvShdancerj = {
	"shdancerj", "shdancer", NULL, NULL, "1989",
	"Shadow Dancer (set 2, Japan)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_SHADOW, GBF_PLATFORM, 0,
	NULL, ShdancerjRomInfo, ShdancerjRomName, NULL, NULL, NULL, NULL, System18InputInfo, ShdancerDIPInfo,
	System16Init, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriverD BurnDrvShdancbl = {
	"shdancbl", "shdancer", NULL, NULL, "1989",
	"Shadow Dancer (bootleg)\0", NULL, "bootleg", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_INVERT_TILES, GBF_PLATFORM, 0,
	NULL, ShdancblRomInfo, ShdancblRomName, NULL, NULL, NULL, NULL, System18InputInfo, ShdancerDIPInfo,
	ShdancblInit, System18Exit, System18Frame, System18Render, System18Scan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvWwallyj = {
	"wwallyj", NULL, NULL, NULL, "1992",
	"Wally wo Sagase! (rev B, Japan, 2 players, FD1094 317-0197B)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_PUZZLE, 0,
	NULL, WwallyjRomInfo, WwallyjRomName, NULL, NULL, NULL, NULL, WwallyInputInfo, WwallyDIPInfo,
	WwallyInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvWwallyjd = {
	"wwallyjd", "wwallyj", NULL, NULL, "1992",
	"Wally wo Sagase! (rev B, Japan, 2 players, FD1094 317-0197B decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_PUZZLE, 0,
	NULL, WwallyjdRomInfo, WwallyjdRomName, NULL, NULL, NULL, NULL, WwallyInputInfo, WwallyDIPInfo,
	WwallyInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvWwallyja = {
	"wwallyja", "wwallyj", NULL, NULL, "1992",
	"Wally wo Sagase! (rev A, Japan, 2 players, FD1094 317-0197A)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_PUZZLE, 0,
	NULL, WwallyjaRomInfo, WwallyjaRomName, NULL, NULL, NULL, NULL, WwallyInputInfo, WwallyDIPInfo,
	WwallyInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvWwallyjad = {
	"wwallyjad", "wwallyj", NULL, NULL, "1992",
	"Wally wo Sagase! (rev A, Japan, 2 players, FD1094 317-0197A decrypted)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_171_5987, GBF_PUZZLE, 0,
	NULL, WwallyjadRomInfo, WwallyjadRomName, NULL, NULL, NULL, NULL, WwallyInputInfo, WwallyDIPInfo,
	WwallyInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 320, 224, 4, 3
};

struct BurnDriver BurnDrvWwallyja3p = {
	"wwallyja3p", "wwallyj", NULL, NULL, "1992",
	"Wally wo Sagase! (rev A, Japan, 3 players, FD1094 317-0198A)\0", NULL, "Sega", "System 18",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SYSTEM18 | HARDWARE_SEGA_FD1094_ENC | HARDWARE_SEGA_171_5987, GBF_PUZZLE, 0,
	NULL, Wwallyja3pRomInfo, Wwallyja3pRomName, NULL, NULL, NULL, NULL, Wwally3pInputInfo, Wwally3pDIPInfo,
	WwallyInit, WwallyExit, System18Frame, System18Render, WwallyScan,
	NULL, 0x1800, 320, 224, 4, 3
};
