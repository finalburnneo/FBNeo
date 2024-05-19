// FB Neo Taito X driver
// Based on MAME drivers by Howie Cohen, Yochizo

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "taito.h"
#include "taito_ic.h"
#include "burn_ym2151.h"
#include "burn_ym2610.h"

static INT32 is_twinhawk = 0;

static INT32 nCyclesExtra[3];

static struct BurnInputInfo SupermanInputList[]=
{
	{"P1 Coin"     , BIT_DIGITAL,	TaitoInputPort2 + 0, "p1 coin"     },
	{"P1 Start"    , BIT_DIGITAL,	TaitoInputPort0 + 7, "p1 start"    },
	{"P1 Up"       , BIT_DIGITAL,	TaitoInputPort0 + 0, "p1 up"       },
	{"P1 Down"     , BIT_DIGITAL,	TaitoInputPort0 + 1, "p1 down"     },
	{"P1 Left"     , BIT_DIGITAL,	TaitoInputPort0 + 2, "p1 left"     },
	{"P1 Right"    , BIT_DIGITAL,	TaitoInputPort0 + 3, "p1 right"    },
	{"P1 Button 1" , BIT_DIGITAL,	TaitoInputPort0 + 4, "p1 fire 1"   },
	{"P1 Button 2" , BIT_DIGITAL,	TaitoInputPort0 + 5, "p1 fire 2"   },

	{"P2 Coin"     , BIT_DIGITAL,	TaitoInputPort2 + 1, "p2 coin"     },
	{"P2 Start"    , BIT_DIGITAL,	TaitoInputPort1 + 7, "p2 start"    },
	{"P2 Up"       , BIT_DIGITAL,	TaitoInputPort1 + 0, "p2 up"       },
	{"P2 Down"     , BIT_DIGITAL,	TaitoInputPort1 + 1, "p2 down"     },
	{"P2 Left"     , BIT_DIGITAL,	TaitoInputPort1 + 2, "p2 left"     },
	{"P2 Right"    , BIT_DIGITAL,	TaitoInputPort1 + 3, "p2 right"    },
	{"P2 Button 1" , BIT_DIGITAL,	TaitoInputPort1 + 4, "p2 fire 1"   },
	{"P2 Button 2" , BIT_DIGITAL,	TaitoInputPort1 + 5, "p2 fire 2"   },

	{"Reset"       , BIT_DIGITAL,	&TaitoReset        , "reset"       },
	{"Service"     , BIT_DIGITAL,	TaitoInputPort2 + 2, "service"     },
	{"Tilt"        , BIT_DIGITAL,	TaitoInputPort2 + 7, "tilt"        },
	{"Dip 1"       , BIT_DIPSWITCH,	TaitoDip + 0       , "dip"         },
	{"Dip 2"       , BIT_DIPSWITCH,	TaitoDip + 1       , "dip"         },
};

STDINPUTINFO(Superman)

static struct BurnInputInfo TwinhawkInputList[]=
{
	{"P1 Coin"     , BIT_DIGITAL,	TaitoInputPort2 + 0, "p1 coin"     },
	{"P1 Start"    , BIT_DIGITAL,	TaitoInputPort0 + 7, "p1 start"    },
	{"P1 Up"       , BIT_DIGITAL,	TaitoInputPort0 + 0, "p1 up"       },
	{"P1 Down"     , BIT_DIGITAL,	TaitoInputPort0 + 1, "p1 down"     },
	{"P1 Left"     , BIT_DIGITAL,	TaitoInputPort0 + 2, "p1 left"     },
	{"P1 Right"    , BIT_DIGITAL,	TaitoInputPort0 + 3, "p1 right"    },
	{"P1 Button 1" , BIT_DIGITAL,	TaitoInputPort0 + 4, "p1 fire 1"   },
	{"P1 Button 2" , BIT_DIGITAL,	TaitoInputPort0 + 5, "p1 fire 2"   },
	{"P1 Button 3" , BIT_DIGITAL,	TaitoInputPort0 + 6, "p1 fire 3"   },

	{"P2 Coin"     , BIT_DIGITAL,	TaitoInputPort2 + 1, "p2 coin"     },
	{"P2 Start"    , BIT_DIGITAL,	TaitoInputPort1 + 7, "p2 start"    },
	{"P2 Up"       , BIT_DIGITAL,	TaitoInputPort1 + 0, "p2 up"       },
	{"P2 Down"     , BIT_DIGITAL,	TaitoInputPort1 + 1, "p2 down"     },
	{"P2 Left"     , BIT_DIGITAL,	TaitoInputPort1 + 2, "p2 left"     },
	{"P2 Right"    , BIT_DIGITAL,	TaitoInputPort1 + 3, "p2 right"    },
	{"P2 Button 1" , BIT_DIGITAL,	TaitoInputPort1 + 4, "p2 fire 1"   },
	{"P2 Button 2" , BIT_DIGITAL,	TaitoInputPort1 + 5, "p2 fire 2"   },
	{"P2 Button 3" , BIT_DIGITAL,	TaitoInputPort1 + 6, "p2 fire 3"   },

	{"Reset"       , BIT_DIGITAL,	&TaitoReset        , "reset"       },
	{"Service"     , BIT_DIGITAL,	TaitoInputPort2 + 2, "service"     },
	{"Tilt"        , BIT_DIGITAL,	TaitoInputPort2 + 3, "tilt"        },
	{"Dip 1"       , BIT_DIPSWITCH,	TaitoDip + 0       , "dip"         },
	{"Dip 2"       , BIT_DIPSWITCH,	TaitoDip + 1       , "dip"         },
};

STDINPUTINFO(Twinhawk)

static void TaitoXMakeInputs()
{
	TaitoInput[0] = TaitoInput[1] = TaitoInput[2] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
		TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
		TaitoInput[2] ^= (TaitoInputPort2[i] & 1) << i;
	}

	if (cchip_active) {
		cchip_loadports(TaitoInput[0], TaitoInput[1], 0, TaitoInput[2]);
	}
}

static struct BurnDIPInfo BallbrosDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xdf, NULL                             },
	{0x16, 0xff, 0xff, 0x86, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x03, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"              },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x15, 0x01, 0x60, 0x60, "Easy"                           },
	{0x15, 0x01, 0x60, 0x40, "Medium"                         },
	{0x15, 0x01, 0x60, 0x20, "Hard"                           },
	{0x15, 0x01, 0x60, 0x00, "Hardest"                        },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x16, 0x01, 0x01, 0x01, "Off"                            },
	{0x16, 0x01, 0x01, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x16, 0x01, 0x02, 0x02, "Off"                            },
	{0x16, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x04, 0x00, "Off"                            },
	{0x16, 0x01, 0x04, 0x04, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Language"                       },
	{0x16, 0x01, 0x20, 0x00, "English"                        },
	{0x16, 0x01, 0x20, 0x20, "Japanese"                       },
	
	{0   , 0xfe, 0   , 2   , "Colour Change"                  },
	{0x16, 0x01, 0x40, 0x00, "Less"                           },
	{0x16, 0x01, 0x40, 0x40, "More"                           },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Ballbros)

static struct BurnDIPInfo GigandesDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xdf, NULL                             },
	{0x16, 0xff, 0xff, 0x98, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x03, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"              },
	
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x15, 0x01, 0x60, 0x60, "Easy"                           },
	{0x15, 0x01, 0x60, 0x40, "Medium"                         },
	{0x15, 0x01, 0x60, 0x20, "Hard"                           },
	{0x15, 0x01, 0x60, 0x00, "Hardest"                        },
	
	{0   , 0xfe, 0   , 2   , "Debug Mode"                     },
	{0x15, 0x01, 0x80, 0x80, "Off"                            },
	{0x15, 0x01, 0x80, 0x00, "On"                             },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x16, 0x01, 0x01, 0x01, "Off"                            },
	{0x16, 0x01, 0x01, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x16, 0x01, 0x02, 0x00, "Off"                            },
	{0x16, 0x01, 0x02, 0x02, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x04, 0x04, "Off"                            },
	{0x16, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x16, 0x01, 0x18, 0x18, "3"                              },
	{0x16, 0x01, 0x18, 0x10, "4"                              },
	{0x16, 0x01, 0x18, 0x08, "5"                              },
	{0x16, 0x01, 0x18, 0x00, "6"                              },
	
	{0   , 0xfe, 0   , 2   , "Controls"                       },
	{0x16, 0x01, 0x20, 0x20, "Single"                         },
	{0x16, 0x01, 0x20, 0x00, "Dual"                           },
	
	{0   , 0xfe, 0   , 2   , "Language"                       },
	{0x16, 0x01, 0x40, 0x00, "English"                        },
	{0x16, 0x01, 0x40, 0x40, "Japanese"                       },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Gigandes)

static struct BurnDIPInfo KyustrkrDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xdf, NULL                             },
	{0x16, 0xff, 0xff, 0x80, NULL                             },
		
	// Dip 1
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x03, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"              },
		
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x15, 0x01, 0x60, 0x60, "Easy"                           },
	{0x15, 0x01, 0x60, 0x40, "Medium"                         },
	{0x15, 0x01, 0x60, 0x20, "Hard"                           },
	{0x15, 0x01, 0x60, 0x00, "Hardest"                        },
	
	{0   , 0xfe, 0   , 2   , "Debug Mode"                     },
	{0x15, 0x01, 0x80, 0x80, "Off"                            },
	{0x15, 0x01, 0x80, 0x00, "On"                             },
	
	// Dip 2
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x16, 0x01, 0x01, 0x01, "Off"                            },
	{0x16, 0x01, 0x01, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x16, 0x01, 0x02, 0x00, "Off"                            },
	{0x16, 0x01, 0x02, 0x02, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x04, 0x04, "Off"                            },
	{0x16, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x16, 0x01, 0x18, 0x18, "3"                              },
	{0x16, 0x01, 0x18, 0x10, "4"                              },
	{0x16, 0x01, 0x18, 0x08, "5"                              },
	{0x16, 0x01, 0x18, 0x00, "6"                              },
	
	{0   , 0xfe, 0   , 2   , "Controls"                       },
	{0x16, 0x01, 0x20, 0x20, "Single"                         },
	{0x16, 0x01, 0x20, 0x00, "Dual"                           },
	
	{0   , 0xfe, 0   , 2   , "Language"                       },
	{0x16, 0x01, 0x40, 0x00, "English"                        },
	{0x16, 0x01, 0x40, 0x40, "Japanese"                       },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Kyustrkr)

static struct BurnDIPInfo SupermanDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                             },
	{0x14, 0xff, 0xff, 0xff, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x13, 0x01, 0x02, 0x02, "Off"                            },
	{0x13, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x13, 0x01, 0x04, 0x04, "Off"                            },
	{0x13, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x13, 0x01, 0x08, 0x00, "Off"                            },
	{0x13, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin 2 Credits"               },
	{0x13, 0x01, 0xc0, 0x80, "1 Coin 3 Credits"               },
	{0x13, 0x01, 0xc0, 0x40, "1 Coin 4 Credits"               },
	{0x13, 0x01, 0xc0, 0x00, "1 Coin 6 Credits"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x14, 0x01, 0x03, 0x02, "Easy"                           },
	{0x14, 0x01, 0x03, 0x03, "Medium"                         },
	{0x14, 0x01, 0x03, 0x01, "Hard"                           },
	{0x14, 0x01, 0x03, 0x00, "Hardest"                        },

	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x14, 0x01, 0x30, 0x20, "2"                              },
	{0x14, 0x01, 0x30, 0x30, "3"                              },
	{0x14, 0x01, 0x30, 0x10, "4"                              },
	{0x14, 0x01, 0x30, 0x00, "5"                              },
};

STDDIPINFO(Superman)

static struct BurnDIPInfo SupermanuDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                             },
	{0x14, 0xff, 0xff, 0xff, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x13, 0x01, 0x02, 0x02, "Off"                            },
	{0x13, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x13, 0x01, 0x04, 0x04, "Off"                            },
	{0x13, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x13, 0x01, 0x08, 0x00, "Off"                            },
	{0x13, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coinage"                        },
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },

	{0   , 0xfe, 0   , 4   , "Price to Continue"              },
	{0x13, 0x01, 0xc0, 0x00, "3 Coins 2 Credit"               },
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 3 Credit"               },
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  1 Credit"               },
	{0x13, 0x01, 0xc0, 0xc0, "Same as Start"                  },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x14, 0x01, 0x03, 0x02, "Easy"                           },
	{0x14, 0x01, 0x03, 0x03, "Medium"                         },
	{0x14, 0x01, 0x03, 0x01, "Hard"                           },
	{0x14, 0x01, 0x03, 0x00, "Hardest"                        },

	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x14, 0x01, 0x30, 0x20, "2"                              },
	{0x14, 0x01, 0x30, 0x30, "3"                              },
	{0x14, 0x01, 0x30, 0x10, "4"                              },
	{0x14, 0x01, 0x30, 0x00, "5"                              },
};

STDDIPINFO(Supermanu)

static struct BurnDIPInfo SupermanjDIPList[]=
{
	// Default Values
	{0x13, 0xff, 0xff, 0xff, NULL                             },
	{0x14, 0xff, 0xff, 0xff, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x13, 0x01, 0x02, 0x02, "Off"                            },
	{0x13, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x13, 0x01, 0x04, 0x04, "Off"                            },
	{0x13, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x13, 0x01, 0x08, 0x00, "Off"                            },
	{0x13, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credit"               },
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"              },
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"               },
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"               },
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"              },
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"              },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x14, 0x01, 0x03, 0x02, "Easy"                           },
	{0x14, 0x01, 0x03, 0x03, "Medium"                         },
	{0x14, 0x01, 0x03, 0x01, "Hard"                           },
	{0x14, 0x01, 0x03, 0x00, "Hardest"                        },

	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x14, 0x01, 0x30, 0x20, "2"                              },
	{0x14, 0x01, 0x30, 0x30, "3"                              },
	{0x14, 0x01, 0x30, 0x10, "4"                              },
	{0x14, 0x01, 0x30, 0x00, "5"                              },
};

STDDIPINFO(Supermanj)


static struct BurnDIPInfo TwinhawkDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xff, NULL                             },
	{0x16, 0xff, 0xff, 0x7f, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x15, 0x01, 0x02, 0x02, "Off"                            },
	{0x15, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x15, 0x01, 0x04, 0x04, "Off"                            },
	{0x15, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x15, 0x01, 0x08, 0x00, "Off"                            },
	{0x15, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x30, 0x00, "4 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x10, "3 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x20, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0xc0, 0xc0, "1 Coin 2 Credits"               },
	{0x15, 0x01, 0xc0, 0x80, "1 Coin 3 Credits"               },
	{0x15, 0x01, 0xc0, 0x40, "1 Coin 4 Credits"               },
	{0x15, 0x01, 0xc0, 0x00, "1 Coin 6 Credits"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x16, 0x01, 0x03, 0x02, "Easy"                           },
	{0x16, 0x01, 0x03, 0x03, "Medium"                         },
	{0x16, 0x01, 0x03, 0x01, "Hard"                           },
	{0x16, 0x01, 0x03, 0x00, "Hardest"                        },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"                     },
	{0x16, 0x01, 0x0c, 0x0c, "50k and every 150k"             },
	{0x16, 0x01, 0x0c, 0x08, "70k and every 200k"             },
	{0x16, 0x01, 0x0c, 0x04, "50k only"                       },
	{0x16, 0x01, 0x0c, 0x00, "100k only"                      },
	
	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x16, 0x01, 0x30, 0x00, "2"                              },
	{0x16, 0x01, 0x30, 0x30, "3"                              },
	{0x16, 0x01, 0x30, 0x10, "4"                              },
	{0x16, 0x01, 0x30, 0x20, "5"                              },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Twinhawk)

static struct BurnDIPInfo TwinhawkuDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xff, NULL                             },
	{0x16, 0xff, 0xff, 0x7f, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x15, 0x01, 0x02, 0x02, "Off"                            },
	{0x15, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x15, 0x01, 0x04, 0x04, "Off"                            },
	{0x15, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x15, 0x01, 0x08, 0x00, "Off"                            },
	{0x15, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x30, 0x00, "4 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x10, "3 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x20, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },
	
	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0xc0, 0x00, "4 Coins 1 Credit"               },
	{0x15, 0x01, 0xc0, 0x40, "3 Coins 1 Credit"               },
	{0x15, 0x01, 0xc0, 0x80, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"               },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x16, 0x01, 0x03, 0x02, "Easy"                           },
	{0x16, 0x01, 0x03, 0x03, "Medium"                         },
	{0x16, 0x01, 0x03, 0x01, "Hard"                           },
	{0x16, 0x01, 0x03, 0x00, "Hardest"                        },
	
	{0   , 0xfe, 0   , 4   , "Bonus Life"                     },
	{0x16, 0x01, 0x0c, 0x0c, "50k and every 150k"             },
	{0x16, 0x01, 0x0c, 0x08, "70k and every 200k"             },
	{0x16, 0x01, 0x0c, 0x04, "50k only"                       },
	{0x16, 0x01, 0x0c, 0x00, "100k only"                      },
	
	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x16, 0x01, 0x30, 0x00, "2"                              },
	{0x16, 0x01, 0x30, 0x30, "3"                              },
	{0x16, 0x01, 0x30, 0x10, "4"                              },
	{0x16, 0x01, 0x30, 0x20, "5"                              },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Twinhawku)

static struct BurnDIPInfo DaisenpuDIPList[]=
{
	// Default Values
	{0x15, 0xff, 0xff, 0xfe, NULL                             },
	{0x16, 0xff, 0xff, 0x7f, NULL                             },
	
	// Dip 1
	{0   , 0xfe, 0   , 2   , "Cabinet"                        },
	{0x15, 0x01, 0x01, 0x00, "Upright"                        },
	{0x15, 0x01, 0x01, 0x01, "Cocktail"                       },
	
	{0   , 0xfe, 0   , 2   , "Flip Screen"                    },
	{0x15, 0x01, 0x02, 0x02, "Off"                            },
	{0x15, 0x01, 0x02, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Service Mode"                   },
	{0x15, 0x01, 0x04, 0x04, "Off"                            },
	{0x15, 0x01, 0x04, 0x00, "On"                             },
	
	{0   , 0xfe, 0   , 2   , "Demo Sounds"                    },
	{0x15, 0x01, 0x08, 0x00, "Off"                            },
	{0x15, 0x01, 0x08, 0x08, "On"                             },
	
	{0   , 0xfe, 0   , 4   , "Coin A"                         },
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0x30, 0x30, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0x30, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0x30, 0x20, "1 Coin  2 Credits"              },

	{0   , 0xfe, 0   , 4   , "Coin B"                         },
	{0x15, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"               },
	{0x15, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"               },
	{0x15, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"              },
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"              },
	
	// Dip 2
	{0   , 0xfe, 0   , 4   , "Difficulty"                     },
	{0x16, 0x01, 0x03, 0x02, "Easy"                           },
	{0x16, 0x01, 0x03, 0x03, "Medium"                         },
	{0x16, 0x01, 0x03, 0x01, "Hard"                           },
	{0x16, 0x01, 0x03, 0x00, "Hardest"                        },

	{0   , 0xfe, 0   , 4   , "Bonus Life"                     },
	{0x16, 0x01, 0x0c, 0x08, "50k and every 150k"             },
	{0x16, 0x01, 0x0c, 0x0c, "70k and every 200k"             },
	{0x16, 0x01, 0x0c, 0x04, "100k only"                      },
	{0x16, 0x01, 0x0c, 0x00, "None"                           },
	
	{0   , 0xfe, 0   , 4   , "Lives"                          },
	{0x16, 0x01, 0x30, 0x00, "2"                              },
	{0x16, 0x01, 0x30, 0x30, "3"                              },
	{0x16, 0x01, 0x30, 0x10, "4"                              },
	{0x16, 0x01, 0x30, 0x20, "5"                              },
	
	{0   , 0xfe, 0   , 2   , "Allow Continue"                 },
	{0x16, 0x01, 0x80, 0x80, "Off"                            },
	{0x16, 0x01, 0x80, 0x00, "On"                             },
};

STDDIPINFO(Daisenpu)

// Taito C-Chip BIOS
static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo cchipRomDesc[] = {
#if !defined ROM_VERIFY
	{ "cchip_upd78c11.bin",		0x01000, 0x43021521, BRF_BIOS | TAITO_CCHIP_BIOS},
#endif
};

static struct BurnRomInfo BallbrosRomDesc[] = {
	{ "10a",           0x20000, 0x4af0e858, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "5a",            0x20000, 0x0b983a69, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "8d",            0x10000, 0xd1c515af, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "3",             0x20000, 0xec3e0537, BRF_GRA | TAITO_SPRITESA },
	{ "2",             0x20000, 0xbb441717, BRF_GRA | TAITO_SPRITESA },
	{ "1",             0x20000, 0x8196d624, BRF_GRA | TAITO_SPRITESA },	
	{ "0",             0x20000, 0x1cc584e5, BRF_GRA | TAITO_SPRITESA },

	{ "east-11",       0x80000, 0x92111f96, BRF_SND | TAITO_YM2610A },
	{ "east-10",       0x80000, 0xca0ac419, BRF_SND | TAITO_YM2610B },
};

STD_ROM_PICK(Ballbros)
STD_ROM_FN(Ballbros)

static struct BurnRomInfo GigandesRomDesc[] = {
	{ "east_1.10a",    0x20000, 0xae74e4e5, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east_3.5a",     0x20000, 0x8bcf2116, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east_2.8a",     0x20000, 0xdd94b4d0, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east_4.3a",     0x20000, 0xa647310a, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "east_5.17d",    0x10000, 0xb24ab5f4, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "east_8.3f",     0x80000, 0x75eece28, BRF_GRA | TAITO_SPRITESA },
	{ "east_7.3h",     0x80000, 0xb179a76a, BRF_GRA | TAITO_SPRITESA },
	{ "east_9.3j",     0x80000, 0x5c5e6898, BRF_GRA | TAITO_SPRITESA },	
	{ "east_6.3k",     0x80000, 0x52db30e9, BRF_GRA | TAITO_SPRITESA },

	{ "east-11.16f",   0x80000, 0x92111f96, BRF_SND | TAITO_YM2610A },
	{ "east-10.16e",   0x80000, 0xca0ac419, BRF_SND | TAITO_YM2610B },
};

STD_ROM_PICK(Gigandes)
STD_ROM_FN(Gigandes)

static struct BurnRomInfo GigandesaRomDesc[] = {
	{ "east-1.10a",    0x20000, 0x290c50e0, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east-3.5a",     0x20000, 0x9cef82af, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east_2.8a",     0x20000, 0xdd94b4d0, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "east_4.3a",     0x20000, 0xa647310a, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "east_5.17d",    0x10000, 0xb24ab5f4, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "east_8.3f",     0x80000, 0x75eece28, BRF_GRA | TAITO_SPRITESA },
	{ "east_7.3h",     0x80000, 0xb179a76a, BRF_GRA | TAITO_SPRITESA },
	{ "east_9.3j",     0x80000, 0x5c5e6898, BRF_GRA | TAITO_SPRITESA },	
	{ "east_6.3k",     0x80000, 0x52db30e9, BRF_GRA | TAITO_SPRITESA },

	{ "east-11.16f",   0x80000, 0x92111f96, BRF_SND | TAITO_YM2610A },
	{ "east-10.16e",   0x80000, 0xca0ac419, BRF_SND | TAITO_YM2610B },
};

STD_ROM_PICK(Gigandesa)
STD_ROM_FN(Gigandesa)

static struct BurnRomInfo KyustrkrRomDesc[] = {
	{ "pe.9a",         0x20000, 0x082b5f96, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "po.4a",         0x20000, 0x0100361e, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "ic.18d",        0x10000, 0x92cfb788, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "m-8-3.u3",      0x20000, 0x1c4084e6, BRF_GRA | TAITO_SPRITESA },
	{ "m-8-2.u4",      0x20000, 0xada21c4d, BRF_GRA | TAITO_SPRITESA },
	{ "m-8-1.u5",      0x20000, 0x9d95aad6, BRF_GRA | TAITO_SPRITESA },	
	{ "m-8-0.u6",      0x20000, 0x0dfb6ed3, BRF_GRA | TAITO_SPRITESA },

	{ "m-8-5.u2",      0x20000, 0xd9d90e0a, BRF_SND | TAITO_YM2610A },
	{ "m-8-4.u1",      0x20000, 0xd3f6047a, BRF_SND | TAITO_YM2610B },
};

STD_ROM_PICK(Kyustrkr)
STD_ROM_FN(Kyustrkr)

static struct BurnRomInfo SupermanRomDesc[] = {
	{ "b61_09.a10",    0x20000, 0x640f1d58, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_07.a5",     0x20000, 0xfddb9953, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_08.a8",     0x20000, 0x79fc028e, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_13.a3",     0x20000, 0x9f446a44, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b61_10.d18",    0x10000, 0x6efe79e8, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b61-14.f1",     0x80000, 0x89368c3e, BRF_GRA | TAITO_SPRITESA },
	{ "b61-15.h1",     0x80000, 0x910cc4f9, BRF_GRA | TAITO_SPRITESA },
	{ "b61-16.j1",     0x80000, 0x3622ed2f, BRF_GRA | TAITO_SPRITESA },	
	{ "b61-17.k1",     0x80000, 0xc34f27e0, BRF_GRA | TAITO_SPRITESA },

	{ "b61-01.e18",    0x80000, 0x3cf99786, BRF_SND | TAITO_YM2610B },
	
	{ "b61_11.m11",    0x02000, 0x3bc5d44b, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(Superman, Superman, cchip)
STD_ROM_FN(Superman)

static struct BurnRomInfo SupermanuRomDesc[] = {
	{ "b61_09.a10",    0x20000, 0x640f1d58, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_07.a5",     0x20000, 0xfddb9953, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_08.a8",     0x20000, 0x79fc028e, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_12.a3",     0x20000, 0x064d3bfe, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b61_10.d18",    0x10000, 0x6efe79e8, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b61-14.f1",     0x80000, 0x89368c3e, BRF_GRA | TAITO_SPRITESA },
	{ "b61-15.h1",     0x80000, 0x910cc4f9, BRF_GRA | TAITO_SPRITESA },
	{ "b61-16.j1",     0x80000, 0x3622ed2f, BRF_GRA | TAITO_SPRITESA },	
	{ "b61-17.k1",     0x80000, 0xc34f27e0, BRF_GRA | TAITO_SPRITESA },

	{ "b61-01.e18",    0x80000, 0x3cf99786, BRF_SND | TAITO_YM2610B },
	
	{ "b61_11.m11",    0x02000, 0x3bc5d44b, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(Supermanu, Supermanu, cchip)
STD_ROM_FN(Supermanu)

static struct BurnRomInfo SupermanjRomDesc[] = {
	{ "b61_09.a10",    0x20000, 0x640f1d58, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_07.a5",     0x20000, 0xfddb9953, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_08.a8",     0x20000, 0x79fc028e, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b61_06.a3",     0x20000, 0x714a0b68, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b61_10.d18",    0x10000, 0x6efe79e8, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b61-14.f1",     0x80000, 0x89368c3e, BRF_GRA | TAITO_SPRITESA },
	{ "b61-15.h1",     0x80000, 0x910cc4f9, BRF_GRA | TAITO_SPRITESA },
	{ "b61-16.j1",     0x80000, 0x3622ed2f, BRF_GRA | TAITO_SPRITESA },	
	{ "b61-17.k1",     0x80000, 0xc34f27e0, BRF_GRA | TAITO_SPRITESA },

	{ "b61-01.e18",    0x80000, 0x3cf99786, BRF_SND | TAITO_YM2610B },
	
	{ "b61_11.m11",    0x02000, 0x3bc5d44b, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(Supermanj, Supermanj, cchip)
STD_ROM_FN(Supermanj)

static struct BurnRomInfo TwinhawkRomDesc[] = {
	{ "b87-11.u7",     0x20000, 0xfc84a399, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b87-10.u5",     0x20000, 0x17181706, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b87-07.13e",    0x08000, 0xe2e0efa0, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b87-02.3h",     0x80000, 0x89ad43a0, BRF_GRA | TAITO_SPRITESA },
	{ "b87-01.3f",     0x80000, 0x81e82ae1, BRF_GRA | TAITO_SPRITESA },
	{ "b87-04.3k",     0x80000, 0x958434b6, BRF_GRA | TAITO_SPRITESA },	
	{ "b87-03.3j",     0x80000, 0xce155ae0, BRF_GRA | TAITO_SPRITESA },

};

STD_ROM_PICK(Twinhawk)
STD_ROM_FN(Twinhawk)

static struct BurnRomInfo TwinhawkuRomDesc[] = {
	{ "b87-09.u7",     0x20000, 0x7e6267c7, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b87-08.u5",     0x20000, 0x31d9916f, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b87-07.13e",    0x08000, 0xe2e0efa0, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b87-02.3h",     0x80000, 0x89ad43a0, BRF_GRA | TAITO_SPRITESA },
	{ "b87-01.3f",     0x80000, 0x81e82ae1, BRF_GRA | TAITO_SPRITESA },
	{ "b87-04.3k",     0x80000, 0x958434b6, BRF_GRA | TAITO_SPRITESA },	
	{ "b87-03.3j",     0x80000, 0xce155ae0, BRF_GRA | TAITO_SPRITESA },

};

STD_ROM_PICK(Twinhawku)
STD_ROM_FN(Twinhawku)

static struct BurnRomInfo DaisenpuRomDesc[] = {
	{ "b87-06.u7",     0x20000, 0xcf236100, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },
	{ "b87-05.u5",     0x20000, 0x7f15edc7, BRF_ESS | BRF_PRG | TAITO_68KROM1_BYTESWAP },

	{ "b87-07.13e",    0x08000, 0xe2e0efa0, BRF_ESS | BRF_PRG | TAITO_Z80ROM1 },

	{ "b87-02.3h",     0x80000, 0x89ad43a0, BRF_GRA | TAITO_SPRITESA },
	{ "b87-01.3f",     0x80000, 0x81e82ae1, BRF_GRA | TAITO_SPRITESA },
	{ "b87-04.3k",     0x80000, 0x958434b6, BRF_GRA | TAITO_SPRITESA },	
	{ "b87-03.3j",     0x80000, 0xce155ae0, BRF_GRA | TAITO_SPRITESA },

};

STD_ROM_PICK(Daisenpu)
STD_ROM_FN(Daisenpu)

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1                    = Next; Next += Taito68KRom1Size;
	TaitoZ80Rom1                    = Next; Next += TaitoZ80Rom1Size;
	TaitoYM2610ARom                 = Next; Next += TaitoYM2610ARomSize;
	TaitoYM2610BRom                 = Next; Next += TaitoYM2610BRomSize;

	cchip_rom                       = Next; Next += TaitoCCHIPBIOSSize;
	cchip_eeprom                    = Next; Next += TaitoCCHIPEEPROMSize;
	
	TaitoRamStart                   = Next;

	Taito68KRam1                    = Next; Next += 0x004000;
	TaitoZ80Ram1                    = Next; Next += 0x002000;
	TaitoPaletteRam                 = Next; Next += 0x001000;
	TaitoSpriteRam                  = Next; Next += 0x000800;
	TaitoSpriteRam2                 = Next; Next += 0x004000;
	
	TaitoRamEnd                     = Next;

	TaitoSpritesA                   = Next; Next += TaitoNumSpriteA * TaitoSpriteAWidth * TaitoSpriteAHeight;
	TaitoPalette                    = (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	TaitoMemEnd                     = Next;

	return 0;
}

static UINT8 __fastcall TaitoX68KReadByte(UINT32 a)
{
	if (cchip_active) {
		CCHIP_READ(0x900000)
	}

	switch (a) {
		case 0x500001: {
			return TaitoDip[0] & 0x0f;
		}
		
		case 0x500003: {
			return (TaitoDip[0] & 0xf0) >> 4;
		}
		
		case 0x500005: {
			return TaitoDip[1] & 0x0f;
		}
		
		case 0x500007: {
			return (TaitoDip[1] & 0xf0) >> 4;
		}
		
		case 0x800003: {
			return TC0140SYTCommRead();
		}

		case 0x900001: {
			return TaitoInput[0];
		}

		case 0x900003: {
			return TaitoInput[1];
		}

		case 0x900005: {
			return TaitoInput[2];
		}

	    default: {
			bprintf(PRINT_NORMAL, _T("68K #1 Read byte => %06X\n"), a);
		}
	}
	
	return 0;
}

static void __fastcall TaitoX68KWriteByte(UINT32 a, UINT8 d)
{
	if (cchip_active) {
		CCHIP_WRITE(0x900000)
	}

	switch (a) {
		case 0x300000:
		case 0x300001: {
			//???
			return;
		}
		
		case 0x400000:
		case 0x400001: {
			//nop
			return;
		}
		
		case 0x600000:
		case 0x600001: {
			//nop
			return;
		}
		
		case 0x700000:
		case 0x700001: {
			//nop
			return;
		}
		
		case 0x800001: {
			TC0140SYTPortWrite(d);
			return;
		}
		
		case 0x800003: {
			TC0140SYTCommWrite(d);
			return;
		}

		case 0x900009: {
			//coin counter/lockout etc.
			return;
		}

		case 0xc00000:
		case 0xc00001: {
			//???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K #1 Write byte => %06X, %02X\n"), a, d);
		}
	}
}

static UINT16 __fastcall TaitoX68KReadWord(UINT32 a)
{
	switch (a) {
		case 0x500000: {
			return TaitoDip[0] & 0x0f;
		}
		
		case 0x500002: {
			return (TaitoDip[0] & 0xf0) >> 4;
		}
		
		case 0x500004: {
			return TaitoDip[1] & 0x0f;
		}
		
		case 0x500006: {
			return (TaitoDip[1] & 0xf0) >> 4;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("68K #1 Read word => %06X\n"), a);
		}
	}
	
	return 0;
}

static void bank_switch()
{
	ZetMapMemory(TaitoZ80Rom1 + 0x4000 + (TaitoZ80Bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static UINT8 __fastcall TaitoXZ80Read(UINT16 a)
{
	switch (a) {
		case 0xe000:
		case 0xe001:
		case 0xe002: {
			return BurnYM2610Read(a & 3);
		}
		
		case 0xe201: {
			return TC0140SYTSlaveCommRead();
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall TaitoXZ80Write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003: {
			BurnYM2610Write(a & 3, d);
			return;
		}
		
		case 0xe200: {
			TC0140SYTSlavePortWrite(d);
			return;
		}
		
		case 0xe201: {
			TC0140SYTSlaveCommWrite(d);
			return;
		}
		
		case 0xe400:
		case 0xe401:
		case 0xe402:
		case 0xe403: {
			//nop
			return;
		}
		
		case 0xe600: {
			//???
			return;
		}
		
		case 0xee00: {
			//nop
			return;
		}
		
		case 0xf000: {
			//nop
			return;
		}
		
		case 0xf200: {
			TaitoZ80Bank = (d - 1) & 3;
			bank_switch();
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall TwinhawkZ80Read(UINT16 a)
{
	switch (a) {
		case 0xe001: {
			return BurnYM2151Read();
		}
		
		case 0xe201: {
			return TC0140SYTSlaveCommRead();
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall TwinhawkZ80Write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xe000:
		case 0xe001: {
			BurnYM2151Write(a & 1, d);
			return;
		}
		
		case 0xe200: {
			TC0140SYTSlavePortWrite(d);
			return;
		}
		
		case 0xe201: {
			TC0140SYTSlaveCommWrite(d);
			return;
		}
		
		case 0xf200: {
			TaitoZ80Bank = (d - 1) & 3;
			bank_switch();
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static INT32 TaitoXDoReset()
{
	TaitoDoReset();

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	HiscoreReset();

	return 0;
}

static void TaitoXFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void TaitoXYM2151IRQHandler(INT32 Irq)
{
	ZetSetIRQLine(0, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 SpritePlaneOffsets[4]    = { 0x800008, 0x800000, 8, 0 };
static INT32 SpriteXOffsets[16]       = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
static INT32 SpriteYOffsets[16]       = { 0, 16, 32, 48, 64, 80, 96, 112, 256, 272, 288, 304, 320, 336, 352, 368 };
static INT32 BallbrosPlaneOffsets[4]  = { 0x300000, 0x200000, 0x100000, 0 };
static INT32 BallbrosXOffsets[16]     = { 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71 };
static INT32 BallbrosYOffsets[16]     = { 0, 8, 16, 24, 32, 40, 48, 56, 128, 136, 144, 152, 160, 168, 176, 184 };

static INT32 TaitoXInit(INT32 nSoundType)
{
	INT32 nLen;
	
	TaitoNum68Ks = 1;
	TaitoNumZ80s = 1;
	if (nSoundType == 1) {
		TaitoNumYM2151 = 1;
	} else {
		TaitoNumYM2610 = 1;
	}
	
	TaitoLoadRoms(0);
	
	// Allocate and Blank all required memory
	TaitoMem = NULL;
	MemIndex();
	nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();
	
	if (TaitoLoadRoms(1)) return 1;
	
	TC0140SYTInit(0);
	
	// Setup the 68000 emulation
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1            , 0x000000, Taito68KRom1Size - 1, MAP_ROM);
	SekMapMemory(TaitoPaletteRam         , 0xb00000, 0xb00fff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam          , 0xd00000, 0xd007ff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam2         , 0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Taito68KRam1            , 0xf00000, 0xf03fff, MAP_RAM);
	SekSetReadByteHandler(0, TaitoX68KReadByte);
	SekSetWriteByteHandler(0, TaitoX68KWriteByte);
	SekSetReadWordHandler(0, TaitoX68KReadWord);
	SekClose();
	
	// Setup the Z80 emulation
	ZetInit(0);
	ZetOpen(0);
	if (nSoundType == 1) {
		ZetSetReadHandler(TwinhawkZ80Read);
		ZetSetWriteHandler(TwinhawkZ80Write);
	} else {
		ZetSetReadHandler(TaitoXZ80Read);
		ZetSetWriteHandler(TaitoXZ80Write);
	}
	ZetMapArea(0x0000, 0x3fff, 0, TaitoZ80Rom1                );
	ZetMapArea(0x0000, 0x3fff, 2, TaitoZ80Rom1                );
	ZetMapArea(0x4000, 0x7fff, 0, TaitoZ80Rom1 + 0x4000       );
	ZetMapArea(0x4000, 0x7fff, 2, TaitoZ80Rom1 + 0x4000       );
	ZetMapArea(0xc000, 0xdfff, 0, TaitoZ80Ram1                );
	ZetMapArea(0xc000, 0xdfff, 1, TaitoZ80Ram1                );
	ZetMapArea(0xc000, 0xdfff, 2, TaitoZ80Ram1                );
	ZetClose();
	
	if (nSoundType == 1) {
		BurnYM2151InitBuffered(4000000, 1, NULL, 0);
		BurnYM2151SetIrqHandler(&TaitoXYM2151IRQHandler);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);
		BurnTimerAttachZet(4000000);
	} else {
		if (nSoundType == 2) {
			BurnYM2610Init(8000000, TaitoYM2610BRom, (INT32*)&TaitoYM2610BRomSize, TaitoYM2610ARom, (INT32*)&TaitoYM2610ARomSize, NULL, 0);
		} else {
			BurnYM2610Init(8000000, TaitoYM2610BRom, (INT32*)&TaitoYM2610BRomSize, TaitoYM2610ARom, (INT32*)&TaitoYM2610ARomSize, &TaitoXFMIRQHandler, 0);
		}
		BurnTimerAttachZet(4000000);
		BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
		BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
		BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);
	}
	
	GenericTilesInit();
	
	nTaitoCyclesTotal[0] = (8000000) / 60;
	nTaitoCyclesTotal[1] = (4000000) / 60;
	
	if (nScreenHeight == 224) TaitoYOffset = 16;
	if (nScreenHeight == 240) TaitoYOffset = 8;
	TaitoIrqLine = 2;

	TaitoXDoReset();

	return 0;
}

static INT32 BallbrosInit()
{
	TaitoSpriteAModulo = 0x100;
	TaitoSpriteANumPlanes = 4;
	TaitoSpriteAWidth = 16;
	TaitoSpriteAHeight = 16;
	TaitoSpriteAPlaneOffsets = BallbrosPlaneOffsets;
	TaitoSpriteAXOffsets = BallbrosXOffsets;
	TaitoSpriteAYOffsets = BallbrosYOffsets;
	TaitoNumSpriteA = 0x1000;
	
	return TaitoXInit(2);
}

static INT32 GigandesInit()
{
	TaitoSpriteAModulo = 0x200;
	TaitoSpriteANumPlanes = 4;
	TaitoSpriteAWidth = 16;
	TaitoSpriteAHeight = 16;
	TaitoSpriteAPlaneOffsets = SpritePlaneOffsets;
	TaitoSpriteAXOffsets = SpriteXOffsets;
	TaitoSpriteAYOffsets = SpriteYOffsets;
	TaitoNumSpriteA = 0x4000;
	
	return TaitoXInit(2);
}

static INT32 SupermanInit()
{
	INT32 nRet;
	
	TaitoSpriteAModulo = 0x200;
	TaitoSpriteANumPlanes = 4;
	TaitoSpriteAWidth = 16;
	TaitoSpriteAHeight = 16;
	TaitoSpriteAPlaneOffsets = SpritePlaneOffsets;
	TaitoSpriteAXOffsets = SpriteXOffsets;
	TaitoSpriteAYOffsets = SpriteYOffsets;
	TaitoNumSpriteA = 0x4000;
	
	nRet = TaitoXInit(0);

	// slight overclock (+1mhz) to get rid of background breaking up (vertical black lines)
	// in the flying level (second loop of attract, when the red steel girders appear).
	// note1: game should run at 8mhz / 57.43
	// note2: maybe video needs updating?  otheremu doesn't have this problem
	//        at 8mhz/57.43
	nTaitoCyclesTotal[0] = (9000000) / 60;

	cchip_init();

	TaitoIrqLine = 6;
	
	return nRet;
}

static INT32 TwinhawkInit()
{
	TaitoSpriteAModulo = 0x200;
	TaitoSpriteANumPlanes = 4;
	TaitoSpriteAWidth = 16;
	TaitoSpriteAHeight = 16;
	TaitoSpriteAPlaneOffsets = SpritePlaneOffsets;
	TaitoSpriteAXOffsets = SpriteXOffsets;
	TaitoSpriteAYOffsets = SpriteYOffsets;
	TaitoNumSpriteA = 0x4000;

	is_twinhawk = 1;

	return TaitoXInit(1);
}

static INT32 TaitoXExit()
{
	is_twinhawk = 0;

	return TaitoExit();
}

static inline UINT8 pal5bit(UINT8 bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = pal5bit(BURN_ENDIAN_SWAP_INT16(nColour) >> 10);
	g = pal5bit(BURN_ENDIAN_SWAP_INT16(nColour) >>  5);
	b = pal5bit(BURN_ENDIAN_SWAP_INT16(nColour) >>  0);

	return BurnHighCol(r, g, b, 0);
}

static void TaitoXCalcPalette()
{
	INT32 i;
	UINT16* ps;
	UINT32* pd;

	for (i = 0, ps = (UINT16*)TaitoPaletteRam, pd = TaitoPalette; i < 0x0800; i++, ps++, pd++) {
		*pd = CalcCol(*ps);
	}
}

static void TaitoXDrawBgSprites()
{
	UINT16 *SpriteRam = (UINT16*)TaitoSpriteRam;
	UINT16 *SpriteRam2 = (UINT16*)TaitoSpriteRam2;
	
	INT32 Offs, Col, x, y, Code, Colour, xFlip, yFlip, sx, sy, yOffs;

	INT32 Ctrl = BURN_ENDIAN_SWAP_INT16(SpriteRam[0x300]);
	INT32 Ctrl2 = BURN_ENDIAN_SWAP_INT16(SpriteRam[0x301]);

	INT32 Flip = Ctrl & 0x40;
	INT32 NumCol = Ctrl2 & 0x000f;

	UINT16 *src = SpriteRam2 + (((Ctrl2 ^ (~Ctrl2 << 1)) & 0x40) ? 0x1000 : 0 );

	INT32 Upper = (BURN_ENDIAN_SWAP_INT16(SpriteRam[0x302]) & 0xff) + (BURN_ENDIAN_SWAP_INT16(SpriteRam[0x303]) & 0xff) * 256;
	INT32 Col0;
	switch (Ctrl & 0x0f) {
		case 0x01: Col0	= 0x4; break;
		case 0x06: Col0	= 0x8; break;
		
		default: Col0 = 0x0;
	}
	
	yOffs = Flip ? 1 : -1;

	if (NumCol == 1) NumCol = 16;
	
	for (Col = 0; Col < NumCol; Col++) {
		x = BURN_ENDIAN_SWAP_INT16(SpriteRam[(Col * 0x20 + 0x08 + 0x400) / 2]) & 0xff;
		y = BURN_ENDIAN_SWAP_INT16(SpriteRam[(Col * 0x20 + 0x00 + 0x400) / 2]) & 0xff;
		
		for (Offs = 0; Offs < 0x20; Offs++) {
			Code = BURN_ENDIAN_SWAP_INT16(src[((Col + Col0) & 0x0f) * 0x20 + Offs + 0x400]);
			Colour = BURN_ENDIAN_SWAP_INT16(src[((Col + Col0) & 0x0f) * 0x20 + Offs + 0x600]);

			xFlip = Code & 0x8000;
			yFlip = Code & 0x4000;

			sx = x + (Offs & 1) * 16;
			sy = -(y + yOffs) + (Offs / 2) * 16;

			if (Upper & (1 << Col))	sx += 256;
			
			if (Flip) {
				sy = 0xf0 - sy;
				xFlip = !xFlip;
				yFlip = !yFlip;
			}

			Colour = (Colour >> (16 - 5)) % 0x100;
			Code &= (TaitoNumSpriteA - 1);
			
			sx = ((sx + 16) & 0x1ff) - 16;
			sy = ((sy + 8) & 0xff) - 8;

			sy -= TaitoYOffset;

			Draw16x16MaskTile(pTransDraw, Code, sx, sy, xFlip, yFlip, Colour, 4, 0, 0, TaitoSpritesA);
		}
	}
}

static void TaitoXDrawSprites()
{
	UINT16 *SpriteRam = (UINT16*)TaitoSpriteRam;
	UINT16 *SpriteRam2 = (UINT16*)TaitoSpriteRam2;

	INT32 Offset, Code, x, y, xFlip, yFlip, Colour;
	INT32 Ctrl = BURN_ENDIAN_SWAP_INT16(SpriteRam[0x300]);
	INT32 Ctrl2 = BURN_ENDIAN_SWAP_INT16(SpriteRam[0x301]);
	INT32 Flip = Ctrl & 0x40;
	UINT16 *src = SpriteRam2 + (((Ctrl2 ^ (~Ctrl2 << 1)) & 0x40) ? 0x1000 : 0);
	
	for (Offset = (0x400 - 2) / 2; Offset >= 0; Offset-- ) {
		Code = BURN_ENDIAN_SWAP_INT16(src[Offset]);
		x = BURN_ENDIAN_SWAP_INT16(src[Offset + 0x200]);
		y = BURN_ENDIAN_SWAP_INT16(SpriteRam[Offset]) & 0xff;
		xFlip = Code & 0x8000;
		yFlip = Code & 0x4000;

		Colour = (x >> (16 - 5) ) % 0x100;

		Code &= (TaitoNumSpriteA - 1);
		
		if (Flip) {
			y = 0xf0 - y;
			xFlip = !xFlip;
			yFlip = !yFlip;
		}

		y = 0xf0 - y;		
		x = ((x + 16) & 0x1ff) - 16;
		y = ((y + 8) & 0xff) - 8;
		
		y -= TaitoYOffset - 2;

		Draw16x16MaskTile(pTransDraw, Code, x, y, xFlip, yFlip, Colour, 4, 0, 0, TaitoSpritesA);
	}
}

static INT32 TaitoXDraw()
{
	TaitoXCalcPalette();

	BurnTransferClear(0x1f0);

	if (nBurnLayer & 1) TaitoXDrawBgSprites();
	if (nSpriteEnable & 1) TaitoXDrawSprites();
	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 TaitoXFrame()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[3] = { nTaitoCyclesTotal[0], nTaitoCyclesTotal[1], 8000000 / 60 /*c-chip*/};
	INT32 nCyclesDone[3] = { nCyclesExtra[0], 0, nCyclesExtra[2] };

	if (TaitoReset) TaitoXDoReset();

	TaitoXMakeInputs();

	SekNewFrame();
	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(TaitoIrqLine, CPU_IRQSTATUS_AUTO);
		SekClose();

		ZetOpen(0);
		CPU_RUN_TIMER(1);
		ZetClose();

		if (cchip_active) { // superman
			CPU_RUN(2, cchip_);
			if (i == (nInterleave - 1)) cchip_interrupt();
		}
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	// 1 - timer (BurnTimer keeps track)
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		if (is_twinhawk) {
			BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) TaitoXDraw();

	return 0;
}

static INT32 TaitoXScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029683;
	}
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = TaitoRamStart;
		ba.nLen	  = TaitoRamEnd-TaitoRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	TaitoICScan(nAction);
	
	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		if (TaitoNumYM2610) BurnYM2610Scan(nAction, pnMin);
		if (TaitoNumYM2151) BurnYM2151Scan(nAction, pnMin);
		
		SCAN_VAR(TaitoZ80Bank);
		SCAN_VAR(TaitoSoundLatch);

		SCAN_VAR(nCyclesExtra);
	}
	
	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bank_switch();
		ZetClose();
	}
	
	return 0;
}

struct BurnDriver BurnDrvBallbros = {
	"ballbros", NULL, NULL, NULL, "1992",
	"Balloon Brothers\0", NULL, "East Technology", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_PUZZLE, 0,
	NULL, BallbrosRomInfo, BallbrosRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, BallbrosDIPInfo,
	BallbrosInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvGigandes = {
	"gigandes", NULL, NULL, NULL, "1989",
	"Gigandes\0", NULL, "East Technology", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_HORSHOOT, 0,
	NULL, GigandesRomInfo, GigandesRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, GigandesDIPInfo,
	GigandesInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvGigandesa = {
	"gigandesa", "gigandes", NULL, NULL, "1989",
	"Gigandes (earlier)\0", NULL, "East Technology", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_HORSHOOT, 0,
	NULL, GigandesaRomInfo, GigandesaRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, GigandesDIPInfo,
	GigandesInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvKyustrkr = {
	"kyustrkr", NULL, NULL, NULL, "1989",
	"Last Striker / Kyuukyoku no Striker\0", NULL, "East Technology", "Taito X",
	L"Last Striker\0Final \u7A76\u6975 \u306E Striker\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_SPORTSFOOTBALL, 0,
	NULL, KyustrkrRomInfo, KyustrkrRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, KyustrkrDIPInfo,
	BallbrosInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvSuperman = {
	"superman", NULL, "cchip", NULL, "1988",
	"Superman (World)\0", NULL, "Taito Corporation", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_SCRFIGHT, 0,
	NULL, SupermanRomInfo, SupermanRomName, NULL, NULL, NULL, NULL, SupermanInputInfo, SupermanDIPInfo,
	SupermanInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvSupermanu = {
	"supermanu", "superman", "cchip", NULL, "1988",
	"Superman (US)\0", NULL, "Taito Corporation", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_SCRFIGHT, 0,
	NULL, SupermanuRomInfo, SupermanuRomName, NULL, NULL, NULL, NULL, SupermanInputInfo, SupermanuDIPInfo,
	SupermanInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvSupermanj = {
	"supermanj", "superman", "cchip", NULL, "1988",
	"Superman (Japan)\0", NULL, "Taito Corporation", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_SCRFIGHT, 0,
	NULL, SupermanjRomInfo, SupermanjRomName, NULL, NULL, NULL, NULL, SupermanInputInfo, SupermanjDIPInfo,
	SupermanInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 384, 240, 4, 3
};

struct BurnDriver BurnDrvTwinhawk = {
	"twinhawk", NULL, NULL, NULL, "1989",
	"Twin Hawk (World)\0", NULL, "Taito Corporation Japan", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_VERSHOOT, 0,
	NULL, TwinhawkRomInfo, TwinhawkRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, TwinhawkDIPInfo,
	TwinhawkInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 224, 384, 3, 4
};

struct BurnDriver BurnDrvTwinhawku = {
	"twinhawku", "twinhawk", NULL, NULL, "1989",
	"Twin Hawk (US)\0", NULL, "Taito America Corporation", "Taito X",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_VERSHOOT, 0,
	NULL, TwinhawkuRomInfo, TwinhawkuRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, TwinhawkuDIPInfo,
	TwinhawkInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 224, 384, 3, 4
};

struct BurnDriver BurnDrvDaisenpu = {
	"daisenpu", "twinhawk", NULL, NULL, "1989",
	"Daisenpu (Japan)\0", NULL, "Taito Corporation", "Taito X",
	L"\u5927\u65CB\u98A8 (Japan)\0Daisenpu\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_TAITOX, GBF_VERSHOOT, 0,
	NULL, DaisenpuRomInfo, DaisenpuRomName, NULL, NULL, NULL, NULL, TwinhawkInputInfo, DaisenpuDIPInfo,
	TwinhawkInit, TaitoXExit, TaitoXFrame, TaitoXDraw, TaitoXScan,
	NULL, 0x800, 224, 384, 3, 4
};
