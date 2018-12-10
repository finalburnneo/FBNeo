#include "driver.h"
#include "burnint.h"
#include "midtunit.h"

static struct BurnInputInfo MkInputList[] = {
	{ "P1 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 0,	 "p1 coin"   },
	{ "P1 Start",		BIT_DIGITAL,	nTUnitJoy2 + 2,	 "p1 start"  },
	{ "P1 Up",			BIT_DIGITAL,	nTUnitJoy1 + 0,	 "p1 up"     },
	{ "P1 Down",		BIT_DIGITAL,	nTUnitJoy1 + 1,	 "p1 down"   },
	{ "P1 Left",		BIT_DIGITAL,	nTUnitJoy1 + 2,	 "p1 left"   },
	{ "P1 Right",		BIT_DIGITAL,	nTUnitJoy1 + 3,	 "p1 right"  },
	{ "P1 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 4,	 "p1 fire 1" },
	{ "P1 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 5,	 "p1 fire 2" },
	{ "P1 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 6,	 "p1 fire 3" },
	{ "P1 Button 4",	BIT_DIGITAL,	nTUnitJoy2 + 12, "p1 fire 4" },
	{ "P1 Button 5",	BIT_DIGITAL,	nTUnitJoy2 + 13, "p1 fire 5" },
	{ "P1 Button 6",	BIT_DIGITAL,	nTUnitJoy2 + 15, "p1 fire 6" },
	
	{ "P2 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 1,	 "p2 coin"   },
	{ "P2 Start",		BIT_DIGITAL,	nTUnitJoy2 + 5,	 "p2 start"  },
	{ "P2 Up",			BIT_DIGITAL,	nTUnitJoy1 + 8,	 "p2 up"     },
	{ "P2 Down",		BIT_DIGITAL,	nTUnitJoy1 + 9,	 "p2 down"   },
	{ "P2 Left",		BIT_DIGITAL,	nTUnitJoy1 + 10, "p2 left"   },
	{ "P2 Right",		BIT_DIGITAL,	nTUnitJoy1 + 11, "p2 right"  },
	{ "P2 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 12, "p2 fire 1" },
	{ "P2 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 13, "p2 fire 2" },
	{ "P2 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 14, "p2 fire 3" },
	{ "P2 Button 4",	BIT_DIGITAL,	nTUnitJoy2 + 9,  "p2 fire 4" },
	{ "P2 Button 5",	BIT_DIGITAL,	nTUnitJoy2 + 10, "p2 fire 5" },
	{ "P2 Button 6",	BIT_DIGITAL,	nTUnitJoy2 + 11, "p2 fire 6" },

	{ "P3 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 7,	 "p3 coin"   },
	{ "P4 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 8,	 "p4 coin"   },

	{ "Reset",			BIT_DIGITAL,	&nTUnitReset,    "reset"     },
	{ "Service",		BIT_DIGITAL,	nTUnitJoy2 + 6,  "service"   },
	{ "Service Mode",	BIT_DIGITAL,	nTUnitJoy2 + 4,  "diag"      },
	{ "Tilt",			BIT_DIGITAL,	nTUnitJoy2 + 3,  "tilt"      },
	{ "Dip A",			BIT_DIPSWITCH,	nTUnitDSW + 0,   "dip"       },
	{ "Dip B",			BIT_DIPSWITCH,	nTUnitDSW + 1,   "dip"       },
};

STDINPUTINFO(Mk)

static struct BurnInputInfo Mk2InputList[] = {
	{ "P1 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 0,	 "p1 coin"   },
	{ "P1 Start",		BIT_DIGITAL,	nTUnitJoy2 + 2,	 "p1 start"  },
	{ "P1 Up",			BIT_DIGITAL,	nTUnitJoy1 + 0,	 "p1 up"     },
	{ "P1 Down",		BIT_DIGITAL,	nTUnitJoy1 + 1,	 "p1 down"   },
	{ "P1 Left",		BIT_DIGITAL,	nTUnitJoy1 + 2,	 "p1 left"   },
	{ "P1 Right",		BIT_DIGITAL,	nTUnitJoy1 + 3,	 "p1 right"  },
	{ "P1 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 4,	 "p1 fire 1" },
	{ "P1 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 5,	 "p1 fire 2" },
	{ "P1 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 6,	 "p1 fire 3" },
	{ "P1 Button 4",	BIT_DIGITAL,	nTUnitJoy3 + 0,  "p1 fire 4" },
	{ "P1 Button 5",	BIT_DIGITAL,	nTUnitJoy3 + 1,  "p1 fire 5" },
	{ "P1 Button 6",	BIT_DIGITAL,	nTUnitJoy3 + 2,  "p1 fire 6" },
	
	{ "P2 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 1,	 "p2 coin"   },
	{ "P2 Start",		BIT_DIGITAL,	nTUnitJoy2 + 5,	 "p2 start"  },
	{ "P2 Up",			BIT_DIGITAL,	nTUnitJoy1 + 8,	 "p2 up"     },
	{ "P2 Down",		BIT_DIGITAL,	nTUnitJoy1 + 9,	 "p2 down"   },
	{ "P2 Left",		BIT_DIGITAL,	nTUnitJoy1 + 10, "p2 left"   },
	{ "P2 Right",		BIT_DIGITAL,	nTUnitJoy1 + 11, "p2 right"  },
	{ "P2 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 12, "p2 fire 1" },
	{ "P2 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 13, "p2 fire 2" },
	{ "P2 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 14, "p2 fire 3" },
	{ "P2 Button 4",	BIT_DIGITAL,	nTUnitJoy3 + 4,  "p2 fire 4" },
	{ "P2 Button 5",	BIT_DIGITAL,	nTUnitJoy3 + 5,  "p2 fire 5" },
	{ "P2 Button 6",	BIT_DIGITAL,	nTUnitJoy3 + 6,  "p2 fire 6" },

	{ "P3 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 7,	 "p3 coin"   },
	{ "P4 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 8,	 "p4 coin"   },

	{ "Reset",			BIT_DIGITAL,	&nTUnitReset,    "reset"     },
	{ "Service",		BIT_DIGITAL,	nTUnitJoy2 + 6,  "service"   },
	{ "Service Mode",	BIT_DIGITAL,	nTUnitJoy2 + 4,  "diag"      },
	{ "Tilt",			BIT_DIGITAL,	nTUnitJoy2 + 3,  "tilt"      },
	{ "Volume Down",	BIT_DIGITAL,	nTUnitJoy2 + 10, "p1 fire 7" },
	{ "Volume Up",		BIT_DIGITAL,	nTUnitJoy2 + 11, "p1 fire 8" },
	{ "Dip A",			BIT_DIPSWITCH,	nTUnitDSW + 0,   "dip"       },
	{ "Dip B",			BIT_DIPSWITCH,	nTUnitDSW + 1,   "dip"       },
};

STDINPUTINFO(Mk2)

static struct BurnInputInfo NbajamInputList[] = {
	{ "P1 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 0,	 "p1 coin"   },
	{ "P1 Start",		BIT_DIGITAL,	nTUnitJoy2 + 2,	 "p1 start"  },
	{ "P1 Up",			BIT_DIGITAL,	nTUnitJoy1 + 0,	 "p1 up"     },
	{ "P1 Down",		BIT_DIGITAL,	nTUnitJoy1 + 1,	 "p1 down"   },
	{ "P1 Left",		BIT_DIGITAL,	nTUnitJoy1 + 2,	 "p1 left"   },
	{ "P1 Right",		BIT_DIGITAL,	nTUnitJoy1 + 3,	 "p1 right"  },
	{ "P1 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 6,	 "p1 fire 1" },
	{ "P1 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 4,	 "p1 fire 2" },
	{ "P1 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 5,	 "p1 fire 3" },
	
	{ "P2 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 1,	 "p2 coin"   },
	{ "P2 Start",		BIT_DIGITAL,	nTUnitJoy2 + 5,	 "p2 start"  },
	{ "P2 Up",			BIT_DIGITAL,	nTUnitJoy1 + 8,	 "p2 up"     },
	{ "P2 Down",		BIT_DIGITAL,	nTUnitJoy1 + 9,	 "p2 down"   },
	{ "P2 Left",		BIT_DIGITAL,	nTUnitJoy1 + 10, "p2 left"   },
	{ "P2 Right",		BIT_DIGITAL,	nTUnitJoy1 + 11, "p2 right"  },
	{ "P2 Button 1",	BIT_DIGITAL,	nTUnitJoy1 + 14, "p2 fire 1" },
	{ "P2 Button 2",	BIT_DIGITAL,	nTUnitJoy1 + 12, "p2 fire 2" },
	{ "P2 Button 3",	BIT_DIGITAL,	nTUnitJoy1 + 13, "p2 fire 3" },

	{ "P3 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 7,	 "p3 coin"   },
	{ "P3 Start",		BIT_DIGITAL,	nTUnitJoy2 + 9,	 "p3 start"  },
	{ "P3 Up",			BIT_DIGITAL,	nTUnitJoy3 + 0,	 "p3 up"     },
	{ "P3 Down",		BIT_DIGITAL,	nTUnitJoy3 + 1,	 "p3 down"   },
	{ "P3 Left",		BIT_DIGITAL,	nTUnitJoy3 + 2,  "p3 left"   },
	{ "P3 Right",		BIT_DIGITAL,	nTUnitJoy3 + 3,  "p3 right"  },
	{ "P3 Button 1",	BIT_DIGITAL,	nTUnitJoy3 + 6,  "p3 fire 1" },
	{ "P3 Button 2",	BIT_DIGITAL,	nTUnitJoy3 + 4,  "p3 fire 2" },
	{ "P3 Button 3",	BIT_DIGITAL,	nTUnitJoy3 + 5,  "p3 fire 3" },
	
	{ "P4 Coin",		BIT_DIGITAL,	nTUnitJoy2 + 8,	 "p4 coin"   },
	{ "P4 Start",		BIT_DIGITAL,	nTUnitJoy2 + 10, "p4 start"  },
	{ "P4 Up",			BIT_DIGITAL,	nTUnitJoy3 + 8,	 "p4 up"     },
	{ "P4 Down",		BIT_DIGITAL,	nTUnitJoy3 + 9,	 "p4 down"   },
	{ "P4 Left",		BIT_DIGITAL,	nTUnitJoy3 + 10, "p4 left"   },
	{ "P4 Right",		BIT_DIGITAL,	nTUnitJoy3 + 11, "p4 right"  },
	{ "P4 Button 1",	BIT_DIGITAL,	nTUnitJoy3+ 14, "p4 fire 1" },
	{ "P4 Button 2",	BIT_DIGITAL,	nTUnitJoy3 + 12, "p4 fire 2" },
	{ "P4 Button 3",	BIT_DIGITAL,	nTUnitJoy3 + 13, "p4 fire 3" },

	{ "Reset",			BIT_DIGITAL,	&nTUnitReset,    "reset"     },
	{ "Service",		BIT_DIGITAL,	nTUnitJoy2 + 6,  "service"   },
	{ "Service Mode",	BIT_DIGITAL,	nTUnitJoy2 + 4,  "diag"      },
	{ "Tilt",			BIT_DIGITAL,	nTUnitJoy2 + 3,  "tilt"      },
	{ "Volume Down",	BIT_DIGITAL,	nTUnitJoy2 + 11, "p1 fire 7" },
	{ "Volume Up",		BIT_DIGITAL,	nTUnitJoy2 + 12, "p1 fire 8" },
	{ "Dip A",			BIT_DIPSWITCH,	nTUnitDSW + 0,   "dip"       },
	{ "Dip B",			BIT_DIPSWITCH,	nTUnitDSW + 1,   "dip"       },
};

STDINPUTINFO(Nbajam)

static struct BurnDIPInfo MkDIPList[]=
{
	{0x1e, 0xff, 0xff, 0x7d, NULL                },
	{0x1f, 0xff, 0xff, 0xf0, NULL                },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x1e, 0x01, 0x01, 0x01, "Off"               },
	{0x1e, 0x01, 0x01, 0x00, "On"                },

	{0   , 0xfe, 0   ,    2, "Counters"          },
	{0x1e, 0x01, 0x02, 0x02, "One"               },
	{0x1e, 0x01, 0x02, 0x00, "Two"               },

	{0   , 0xfe, 0   ,   19, "Coinage"           },
	{0x1e, 0x01, 0x7c, 0x7c, "USA-1"             },
	{0x1e, 0x01, 0x7c, 0x3c, "USA-2"             },
	{0x1e, 0x01, 0x7c, 0x5c, "USA-3"             },
	{0x1e, 0x01, 0x7c, 0x1c, "USA-4"             },
	{0x1e, 0x01, 0x7c, 0x6c, "USA-ECA"           },
	{0x1e, 0x01, 0x7c, 0x0c, "USA-Free Play"     },
	{0x1e, 0x01, 0x7c, 0x74, "German-1"          },
	{0x1e, 0x01, 0x7c, 0x34, "German-2"          },
	{0x1e, 0x01, 0x7c, 0x54, "German-3"          },
	{0x1e, 0x01, 0x7c, 0x14, "German-4"          },
	{0x1e, 0x01, 0x7c, 0x64, "German-5"          },
	{0x1e, 0x01, 0x7c, 0x24, "German-ECA"        },
	{0x1e, 0x01, 0x7c, 0x04, "German-Free Play"  },
	{0x1e, 0x01, 0x7c, 0x78, "French-1"          },
	{0x1e, 0x01, 0x7c, 0x38, "French-2"          },
	{0x1e, 0x01, 0x7c, 0x58, "French-3"          },
	{0x1e, 0x01, 0x7c, 0x18, "French-4"          },
	{0x1e, 0x01, 0x7c, 0x68, "French-ECA"        },
	{0x1e, 0x01, 0x7c, 0x08, "French-Free Play"  },

	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x1e, 0x01, 0x80, 0x80, "Dipswitch"         },
	{0x1e, 0x01, 0x80, 0x00, "CMOS"              },

	{0   , 0xfe, 0   ,    2, "Skip Powerup Test" },
	{0x1f, 0x01, 0x01, 0x01, "Off"               },
	{0x1f, 0x01, 0x01, 0x00, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Comic Book Offer"  },
	{0x1f, 0x01, 0x08, 0x00, "Off"               },
	{0x1f, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Attract Sound"     },
	{0x1f, 0x01, 0x10, 0x00, "Off"               },
	{0x1f, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Low Blows"         },
	{0x1f, 0x01, 0x20, 0x00, "Off"               },
	{0x1f, 0x01, 0x20, 0x20, "On"                },

	{0   , 0xfe, 0   ,    2, "Blood"             },
	{0x1f, 0x01, 0x40, 0x00, "Off"               },
	{0x1f, 0x01, 0x40, 0x40, "On"                },

	{0   , 0xfe, 0   ,    2, "Violence"          },
	{0x1f, 0x01, 0x80, 0x00, "Off"               },
	{0x1f, 0x01, 0x80, 0x80, "On"                },
};

STDDIPINFO(Mk)

static struct BurnDIPInfo Mk2DIPList[]=
{
	{0x20, 0xff, 0xff, 0x7d, NULL                },
	{0x21, 0xff, 0xff, 0xfc, NULL                },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x20, 0x01, 0x01, 0x01, "Off"               },
	{0x20, 0x01, 0x01, 0x00, "On"                },

	{0   , 0xfe, 0   ,    2, "Counters"          },
	{0x20, 0x01, 0x02, 0x02, "One"               },
	{0x20, 0x01, 0x02, 0x00, "Two"               },

	{0   , 0xfe, 0   ,   19, "Coinage"           },
	{0x20, 0x01, 0x7c, 0x7c, "USA-1"             },
	{0x20, 0x01, 0x7c, 0x3c, "USA-2"             },
	{0x20, 0x01, 0x7c, 0x5c, "USA-3"             },
	{0x20, 0x01, 0x7c, 0x1c, "USA-4"             },
	{0x20, 0x01, 0x7c, 0x6c, "USA-ECA"           },
	{0x20, 0x01, 0x7c, 0x0c, "USA-Free Play"     },
	{0x20, 0x01, 0x7c, 0x74, "German-1"          },
	{0x20, 0x01, 0x7c, 0x34, "German-2"          },
	{0x20, 0x01, 0x7c, 0x54, "German-3"          },
	{0x20, 0x01, 0x7c, 0x14, "German-4"          },
	{0x20, 0x01, 0x7c, 0x64, "German-5"          },
	{0x20, 0x01, 0x7c, 0x24, "German-ECA"        },
	{0x20, 0x01, 0x7c, 0x04, "German-Free Play"  },
	{0x20, 0x01, 0x7c, 0x78, "French-1"          },
	{0x20, 0x01, 0x7c, 0x38, "French-2"          },
	{0x20, 0x01, 0x7c, 0x58, "French-3"          },
	{0x20, 0x01, 0x7c, 0x18, "French-4"          },
	{0x20, 0x01, 0x7c, 0x68, "French-ECA"        },
	{0x20, 0x01, 0x7c, 0x08, "French-Free Play"  },

	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x20, 0x01, 0x80, 0x80, "Dipswitch"         },
	{0x20, 0x01, 0x80, 0x00, "CMOS"              },

	{0   , 0xfe, 0   ,    2, "Circuit Boards"    },
	{0x21, 0x01, 0x01, 0x00, "1"                 },
	{0x21, 0x01, 0x01, 0x01, "2"                 },
	
	{0   , 0xfe, 0   ,    2, "Powerup Test"      },
	{0x21, 0x01, 0x02, 0x00, "Off"               },
	{0x21, 0x01, 0x02, 0x02, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Bill Validator"    },
	{0x21, 0x01, 0x04, 0x00, "Installed"         },
	{0x21, 0x01, 0x04, 0x04, "Not Present"       },
	
	{0   , 0xfe, 0   ,    2, "Comic Book Offer"  },
	{0x21, 0x01, 0x08, 0x00, "Off"               },
	{0x21, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Attract Sound"     },
	{0x21, 0x01, 0x10, 0x00, "Off"               },
	{0x21, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Blood"             },
	{0x21, 0x01, 0x40, 0x00, "Off"               },
	{0x21, 0x01, 0x40, 0x40, "On"                },

	{0   , 0xfe, 0   ,    2, "Violence"          },
	{0x21, 0x01, 0x80, 0x00, "Off"               },
	{0x21, 0x01, 0x80, 0x80, "On"                },
};

STDDIPINFO(Mk2)

static struct BurnDIPInfo NbajamDIPList[]=
{
	{0x2a, 0xff, 0xff, 0xfd, NULL                },
	{0x2b, 0xff, 0xff, 0x7f, NULL                },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x2a, 0x01, 0x01, 0x01, "Off"               },
	{0x2a, 0x01, 0x01, 0x00, "On"                },

	{0   , 0xfe, 0   ,    2, "Powerup Test"      },
	{0x2a, 0x01, 0x02, 0x00, "Off"               },
	{0x2a, 0x01, 0x02, 0x02, "On"                },

	{0   , 0xfe, 0   ,    2, "Video"             },
	{0x2a, 0x01, 0x20, 0x00, "Skip"              },
	{0x2a, 0x01, 0x20, 0x20, "Show"              },
	
	{0   , 0xfe, 0   ,    2, "Validator"         },
	{0x2a, 0x01, 0x40, 0x00, "Installed"         },
	{0x2a, 0x01, 0x40, 0x40, "Not Present"       },
	
	{0   , 0xfe, 0   ,    2, "Players"           },
	{0x2a, 0x01, 0x80, 0x00, "2"                 },
	{0x2a, 0x01, 0x80, 0x80, "4"                 },
	
	{0   , 0xfe, 0   ,    3, "Coin Counters"            },
	{0x2b, 0x01, 0x03, 0x03, ""                         },
	{0x2b, 0x01, 0x03, 0x02, "1 Counter, Totalizing"    },
	{0x2b, 0x01, 0x03, 0x01, "2 Counters, 1 count/coin" },
	
	{0   , 0xfe, 0   ,    3, "Country"           },
	{0x2b, 0x01, 0x0c, 0x0c, "USA"               },
	{0x2b, 0x01, 0x0c, 0x08, "French"            },
	{0x2b, 0x01, 0x0c, 0x04, "German"            },
	
	{0   , 0xfe, 0   ,    6, "Coinage"           },
	{0x2b, 0x01, 0x70, 0x70, "1"                 },
	{0x2b, 0x01, 0x70, 0x30, "2"                 },
	{0x2b, 0x01, 0x70, 0x50, "3"                 },
	{0x2b, 0x01, 0x70, 0x10, "4"                 },
	{0x2b, 0x01, 0x70, 0x60, "ECA"               },
	{0x2b, 0x01, 0x70, 0x00, "Free Play"         },

	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x2b, 0x01, 0x80, 0x80, "Dipswitch"         },
	{0x2b, 0x01, 0x80, 0x00, "CMOS"              },
};

STDDIPINFO(Nbajam)

static struct BurnRomInfo mkRomDesc[] = {
	{ "mkt-uj12.bin",	0x080000, 0xf4990bf2, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mkt-ug12.bin",	0x080000, 0xb06aeac1, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "sl1_mortal_kombat_u3_sound_rom.u3",	0x040000, 0xc615844c, 4 | BRF_PRG | BRF_ESS  }, // 2 Sound CPU
	
	{ "sl1_mortal_kombat_u12_sound_rom.u12",	0x040000, 0x258bd7f9, 2 | BRF_PRG | BRF_ESS }, //  3 ADPCM sound banks
	{ "sl1_mortal_kombat_u13_sound_rom.u13",	0x040000, 0x7b7ec3b6, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "mkt-ug14.bin",	0x080000, 0x9e00834e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  5 GFX
	{ "mkt-uj14.bin",	0x080000, 0xf4b0aaa7, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  6
	{ "mkt-ug19.bin",	0x080000, 0x2d8c7ba1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  7
	{ "mkt-uj19.bin",	0x080000, 0x33b9b7a4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  8

	{ "mkt-ug16.bin",	0x080000, 0x52c9d1e5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 0) }, //  9
	{ "mkt-uj16.bin",	0x080000, 0xc94c58cf, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 1) }, // 10
	{ "mkt-ug20.bin",	0x080000, 0x2f7e55d3, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 2) }, // 11
	{ "mkt-uj20.bin",	0x080000, 0xeae96df0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 3) }, // 12

	{ "mkt-ug17.bin",	0x080000, 0xe34fe253, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, // 13
	{ "mkt-uj17.bin",	0x080000, 0xa56e12f5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, // 14
	{ "mkt-ug22.bin",	0x080000, 0xb537bb4e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 15
	{ "mkt-uj22.bin",	0x080000, 0x5e12523b, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 16
};

STD_ROM_PICK(mk)
STD_ROM_FN(mk)

static INT32 MkInit()
{
	TUnitIsMK = 1;
	
	return TUnitInit();
}

struct BurnDriverD BurnDrvMk = {
	"mk", NULL, NULL, NULL, "1992",
	"Mortal Kombat (rev 5.0 T-Unit 03/19/93)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mkRomInfo, mkRomName, NULL, NULL, MkInputInfo, MkDIPInfo,
    MkInit, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mkr4RomDesc[] = {
	{ "mkr4uj12.bin",	0x080000, 0xa1b6635a, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mkr4ug12.bin",	0x080000, 0xaa94f7ea, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "sl1_mortal_kombat_u3_sound_rom.u3",	0x040000, 0xc615844c, 4 | BRF_PRG | BRF_ESS  }, // 2 Sound CPU
	
	{ "sl1_mortal_kombat_u12_sound_rom.u12",	0x040000, 0x258bd7f9, 2 | BRF_PRG | BRF_ESS }, //  3 ADPCM sound banks
	{ "sl1_mortal_kombat_u13_sound_rom.u13",	0x040000, 0x7b7ec3b6, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "mkt-ug14.bin",	0x080000, 0x9e00834e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  5 GFX
	{ "mkt-uj14.bin",	0x080000, 0xf4b0aaa7, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  6
	{ "mkt-ug19.bin",	0x080000, 0x2d8c7ba1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  7
	{ "mkt-uj19.bin",	0x080000, 0x33b9b7a4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  8

	{ "mkt-ug16.bin",	0x080000, 0x52c9d1e5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 0) }, //  9
	{ "mkt-uj16.bin",	0x080000, 0xc94c58cf, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 1) }, // 10
	{ "mkt-ug20.bin",	0x080000, 0x2f7e55d3, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 2) }, // 11
	{ "mkt-uj20.bin",	0x080000, 0xeae96df0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 3) }, // 12

	{ "mkt-ug17.bin",	0x080000, 0xe34fe253, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, // 13
	{ "mkt-uj17.bin",	0x080000, 0xa56e12f5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, // 14
	{ "mkt-ug22.bin",	0x080000, 0xb537bb4e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 15
	{ "mkt-uj22.bin",	0x080000, 0x5e12523b, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 16
};

STD_ROM_PICK(mkr4)
STD_ROM_FN(mkr4)

struct BurnDriverD BurnDrvMkr4 = {
	"mkr4", "mk", NULL, NULL, "1992",
	"Mortal Kombat (rev 4.0 T-Unit 02/11/93)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mkr4RomInfo, mkr4RomName, NULL, NULL, MkInputInfo, MkDIPInfo,
    MkInit, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mkturboRomDesc[] = {
	{ "kombo-rom-uj-12.bin",	0x080000, 0x7a441f2d, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "kombo-rom-ug-12.bin",	0x080000, 0x45bed5a1, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "sl1_mortal_kombat_u3_sound_rom.u3",	0x040000, 0xc615844c, 4 | BRF_PRG | BRF_ESS  }, // 2 Sound CPU
	
	{ "sl1_mortal_kombat_u12_sound_rom.u12",	0x040000, 0x258bd7f9, 2 | BRF_PRG | BRF_ESS }, //  3 ADPCM sound banks
	{ "sl1_mortal_kombat_u13_sound_rom.u13",	0x040000, 0x7b7ec3b6, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "mkt-ug14.bin",	0x080000, 0x9e00834e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  5 GFX
	{ "mkt-uj14.bin",	0x080000, 0xf4b0aaa7, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  6
	{ "mkt-ug19.bin",	0x080000, 0x2d8c7ba1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  7
	{ "mkt-uj19.bin",	0x080000, 0x33b9b7a4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  8

	{ "mkt-ug16.bin",	0x080000, 0x52c9d1e5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 0) }, //  9
	{ "mkt-uj16.bin",	0x080000, 0xc94c58cf, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 1) }, // 10
	{ "mkt-ug20.bin",	0x080000, 0x2f7e55d3, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 2) }, // 11
	{ "mkt-uj20.bin",	0x080000, 0xeae96df0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 3) }, // 12

	{ "mkt-ug17.bin",	0x080000, 0xe34fe253, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, // 13
	{ "mkt-uj17.bin",	0x080000, 0xa56e12f5, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, // 14
	{ "mkt-ug22.bin",	0x080000, 0xb537bb4e, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 15
	{ "mkt-uj22.bin",	0x080000, 0x5e12523b, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 16
};

STD_ROM_PICK(mkturbo)
STD_ROM_FN(mkturbo)

static INT32 MkTurboInit()
{
	TUnitIsMKTurbo = 1;
	
	return MkInit();
}

struct BurnDriverD BurnDrvMkturbo = {
	"mkturbo", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Turbo Ninja T-Unit 03/19/93, hack)\0", NULL, "Hack", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mkturboRomInfo, mkturboRomName, NULL, NULL, MkInputInfo, MkDIPInfo,
    MkTurboInit, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2RomDesc[] = {
	{ "l3.1_mortal_kombat_ii_game_rom_uj12.uj12",	0x080000, 0xcf100a75, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l3.1_mortal_kombat_ii_game_rom_ug12.ug12",	0x080000, 0x582c7dfd, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2)
STD_ROM_FN(mk2)

static INT32 Mk2Init()
{
	TUnitIsMK2 = 1;
	
	return TUnitInit();
}

struct BurnDriverD BurnDrvMk2 = {
	"mk2", NULL, NULL, NULL, "1993",
	"Mortal Kombat II (rev L3.1)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2RomInfo, mk2RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r32eRomDesc[] = {
	{ "uj12.l32e",		0x080000, 0x43f773a6, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "ug12.l32e",		0x080000, 0xdcde9619, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r32e)
STD_ROM_FN(mk2r32e)

struct BurnDriverD BurnDrvMk2r32e = {
	"mk2r32e", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L3.2 (European))\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r32eRomInfo, mk2r32eRomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r31eRomDesc[] = {
	{ "uj12.l31e",		0x080000, 0xf64306d1, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "ug12.l31e",		0x080000, 0x4adeae7e, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r31e)
STD_ROM_FN(mk2r31e)

struct BurnDriverD BurnDrvMk2r31e = {
	"mk2r31e", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L3.1 (European))\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r31eRomInfo, mk2r31eRomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r30RomDesc[] = {
	{ "l3_mortal_kombat_ii_game_rom_uj12.uj12.l30",		0x080000, 0x93440895, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l3_mortal_kombat_ii_game_rom_ug12.ug12.l30",		0x080000, 0x6153c2d8, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r30)
STD_ROM_FN(mk2r30)

struct BurnDriverD BurnDrvMk2r30 = {
	"mk2r30", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L3.0)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r30RomInfo, mk2r30RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r21RomDesc[] = {
	{ "l2.1_mortal_kombat_ii_game_rom_uj12.uj12",		0x080000, 0xd6a35699, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l2.1_mortal_kombat_ii_game_rom_ug12.ug12",		0x080000, 0xaeb703ff, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r21)
STD_ROM_FN(mk2r21)

struct BurnDriverD BurnDrvMk2r21 = {
	"mk2r21", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L2.1)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r21RomInfo, mk2r21RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r20RomDesc[] = {
	{ "l2_mortal_kombat_ii_game_rom_uj12.uj12",		0x080000, 0x72071550, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l2_mortal_kombat_ii_game_rom_ug12.ug12",		0x080000, 0x86c3ce65, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r20)
STD_ROM_FN(mk2r20)

struct BurnDriverD BurnDrvMk2r20 = {
	"mk2r20", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L2.0)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r20RomInfo, mk2r20RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r14RomDesc[] = {
	{ "l1.4_mortal_kombat_ii_game_rom_uj12.uj12",		0x080000, 0x6d43bc6d, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l1.4_mortal_kombat_ii_game_rom_ug12.ug12",		0x080000, 0x42b0da21, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r14)
STD_ROM_FN(mk2r14)

struct BurnDriverD BurnDrvMk2r14 = {
	"mk2r14", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L1.4)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r14RomInfo, mk2r14RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r11RomDesc[] = {
	{ "l1.1_mortal_kombat_ii_game_rom_uj12.uj12",		0x080000, 0x01daff19, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l1.1_mortal_kombat_ii_game_rom_ug12.ug12",		0x080000, 0x54042eb7, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r11)
STD_ROM_FN(mk2r11)

struct BurnDriverD BurnDrvMk2r11 = {
	"mk2r11", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L1.1)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_CLONE, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r11RomInfo, mk2r11RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r42RomDesc[] = {
	{ "mk242j12.bin",		0x080000, 0xc7fb1525, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mk242g12.bin",		0x080000, 0x443d0e0a, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r42)
STD_ROM_FN(mk2r42)

struct BurnDriverD BurnDrvMk2r42 = {
	"mk2r42", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L4.2, hack)\0", NULL, "hack", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r42RomInfo, mk2r42RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2r91RomDesc[] = {
	{ "uj12.l91",		0x080000, 0x41953903, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "ug12.l91",		0x080000, 0xc07f745a, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2r91)
STD_ROM_FN(mk2r91)

struct BurnDriverD BurnDrvMk2r91 = {
	"mk2r91", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II (rev L9.1, hack)\0", NULL, "hack", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2r91RomInfo, mk2r91RomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo mk2chalRomDesc[] = {
	{ "uj12.chl",		0x080000, 0x2d5c04e6, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "ug12.chl",		0x080000, 0x3e7a4bad, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l1_mortal_kombat_ii_sound_rom_u2.u2",	0x080000, 0x5f23d71d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_ii_sound_rom_u3.u3",	0x080000, 0xd6d92bf9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_ii_sound_rom_u4.u4",	0x080000, 0xeebc8e0f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_ii_sound_rom_u5.u5",	0x080000, 0x2b0b7961, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "l1_mortal_kombat_ii_sound_rom_u6.u6",	0x080000, 0xf694b27f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "l1_mortal_kombat_ii_sound_rom_u7.u7",	0x080000, 0x20387e0a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "l1_mortal_kombat_ii_game_rom_ug14.ug14",	0x100000, 0x01e73af6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  8 GFX
	{ "l1_mortal_kombat_ii_game_rom_uj14.uj14",	0x100000, 0xd4985cbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  9
	{ "l1_mortal_kombat_ii_game_rom_ug19.ug19",	0x100000, 0xfec137be, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  10
	{ "l1_mortal_kombat_ii_game_rom_uj19.uj19",	0x100000, 0x2d763156, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  11

	{ "l1_mortal_kombat_ii_game_rom_ug16.ug16",	0x100000, 0x8ba6ae18, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, //  12
	{ "l1_mortal_kombat_ii_game_rom_uj16.uj16",	0x100000, 0x39d885b4, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, //  13
	{ "l1_mortal_kombat_ii_game_rom_ug20.ug20",	0x100000, 0x809118c1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 14
	{ "l1_mortal_kombat_ii_game_rom_uj20.uj20",	0x100000, 0xb96824f0, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 15

	{ "l1_mortal_kombat_ii_game_rom_ug17.ug17",	0x100000, 0x937d8620, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 0) }, // 16
	{ "l1_mortal_kombat_ii_game_rom_uj17.uj17",	0x100000, 0x218de160, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 1) }, // 17
	{ "l1_mortal_kombat_ii_game_rom_ug22.ug22",	0x100000, 0x154d53b1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 2) }, // 18
	{ "l1_mortal_kombat_ii_game_rom_uj22.uj22",	0x100000, 0x8891d785, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x08, 3) }, // 19
};

STD_ROM_PICK(mk2chal)
STD_ROM_FN(mk2chal)

struct BurnDriverD BurnDrvMk2chal = {
	"mk2chal", "mk2", NULL, NULL, "1993",
	"Mortal Kombat II Challenger (hack)\0", NULL, "hack", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, mk2chalRomInfo, mk2chalRomName, NULL, NULL, Mk2InputInfo, Mk2DIPInfo,
    Mk2Init, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};

static struct BurnRomInfo nbajamRomDesc[] = {
	{ "l3_nba_jam_game_rom_uj12.uj1",			0x080000, 0xb93e271c, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l3_nba_jam_game_rom_ug12.ug12",			0x080000, 0x407d3390, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "l2_nba_jam_u3_sound_rom.u3",				0x020000, 0x3a3ea480, 4 | BRF_PRG | BRF_ESS  }, // 2 Sound CPU
	
	{ "sl1_mortal_kombat_u12_sound_rom.u12",	0x080000, 0xb94847f1, 2 | BRF_PRG | BRF_ESS }, //  3 ADPCM sound banks
	{ "sl1_mortal_kombat_u13_sound_rom.u13",	0x080000, 0xb6fe24bd, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_nba_jam_game_rom_ug14.ug14",			0x080000, 0x04bb9f64, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 0) }, //  5 GFX
	{ "l1_nba_jam_game_rom_uj14.uj14",			0x080000, 0xb34b7af3, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 1) }, //  6
	{ "l1_nba_jam_game_rom_ug19.ug19",			0x080000, 0xa8f22fbb, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 2) }, //  7
	{ "l1_nba_jam_game_rom_uj19.uj19",			0x080000, 0x8130a8a2, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x00, 3) }, //  8

	{ "l1_nba_jam_game_rom_ug16.ug16",			0x080000, 0x8591c572, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 0) }, //  9
	{ "l1_nba_jam_game_rom_uj16.uj16",			0x080000, 0xd2e554f1, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 1) }, // 10
	{ "l1_nba_jam_game_rom_ug20.ug20",			0x080000, 0x44fd6221, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 2) }, // 11
	{ "l1_nba_jam_game_rom_uj20.uj20",			0x080000, 0xf9cebbb6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x02, 3) }, // 12

	{ "l1_nba_jam_game_rom_ug17.ug17",			0x080000, 0x6f921886, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 0) }, // 13
	{ "l1_nba_jam_game_rom_uj17.uj17",			0x080000, 0xb2e14981, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 1) }, // 14
	{ "l1_nba_jam_game_rom_ug22.ug22",			0x080000, 0xab05ed89, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 2) }, // 15
	{ "l1_nba_jam_game_rom_uj22.uj22",			0x080000, 0x59a95878, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x04, 3) }, // 16
	
	{ "l1_nba_jam_game_rom_ug18.ug18",			0x080000, 0x5162d3d6, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x06, 0) }, // 17
	{ "l1_nba_jam_game_rom_uj18.uj18",			0x080000, 0xfdee0037, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x06, 1) }, // 18
	{ "l1_nba_jam_game_rom_ug23.ug23",			0x080000, 0x7b934c7a, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x06, 2) }, // 19
	{ "l1_nba_jam_game_rom_uj23.uj23",			0x080000, 0x427d2eee, 3 | BRF_GRA | BRF_ESS | TUNIT_GFX(0x06, 3) }, // 20
};

STD_ROM_PICK(nbajam)
STD_ROM_FN(nbajam)

static INT32 NbajamInit()
{
	TUnitIsNbajam = 1;
	
	return TUnitInit();
}

struct BurnDriverD BurnDrvNbajam = {
	"nbajam", NULL, NULL, NULL, "1993",
	"NBA Jam (rev 3.01 04/07/93)\0", NULL, "Midway", "Midway T-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MIDWAY_TUNIT, GBF_MISC, 0,
	NULL, nbajamRomInfo, nbajamRomName, NULL, NULL, NbajamInputInfo, NbajamDIPInfo,
    NbajamInit, TUnitExit, TUnitFrame, TUnitDraw, TUnitScan, &nTUnitRecalc, 0x8000,
    400, 254, 4, 3
};
