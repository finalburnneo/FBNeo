// FinalBurn Neo IGS PolyGameMaster driver module
// Original FB Neo port by OopsWare
// Based on information from ElSemi, Haze (David Haywood), and XingXing

#include "pgm.h"
#include "ics2115.h"

UINT8 HackCodeDip = 0, VerSwitcher = 0;

static struct BurnInputInfo pgmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	PgmBtn1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	PgmJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	PgmJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	PgmJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	PgmJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	PgmJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	PgmJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	PgmJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	PgmJoy1 + 7,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	PgmBtn2 + 0,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	PgmBtn1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	PgmJoy2 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	PgmJoy2 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	PgmJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	PgmJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	PgmJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	PgmJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	PgmJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	PgmJoy2 + 7,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	PgmBtn2 + 1,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	PgmBtn1 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	PgmJoy3 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	PgmJoy3 + 1,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	PgmJoy3 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	PgmJoy3 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	PgmJoy3 + 4,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	PgmJoy3 + 5,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	PgmJoy3 + 6,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	PgmJoy3 + 7,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	PgmBtn2 + 2,	"p3 fire 4"	},

	{"P4 Coin",			BIT_DIGITAL,	PgmBtn1 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	PgmJoy4 + 0,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	PgmJoy4 + 1,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	PgmJoy4 + 2,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	PgmJoy4 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	PgmJoy4 + 4,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	PgmJoy4 + 5,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	PgmJoy4 + 6,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	PgmJoy4 + 7,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	PgmBtn2 + 3,	"p4 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&PgmReset,		"reset"		},
	{"Diagnostics 1",	BIT_DIGITAL,	PgmBtn1 + 4,	"diag"		},
	{"Diagnostics 2",	BIT_DIGITAL,	PgmBtn1 + 6,	"diag2"		},
	{"Service 1",		BIT_DIGITAL,	PgmBtn1 + 5,	"service"	},
	{"Service 2",		BIT_DIGITAL,	PgmBtn1 + 7,	"service2"	},

	{"Dip A",			BIT_DIPSWITCH,	PgmInput + 6,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	PgmInput + 7,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,  PgmInput + 8,	"dip"		},
};

STDINPUTINFO(pgm)

static struct BurnInputInfo pgmhInputList[] = {
	{"Dip D",			BIT_DIPSWITCH,  &HackCodeDip,	"dip"		},
};

static struct BurnInputInfo oldsplusnrInputList[] = {
	{"Dip D",			BIT_DIPSWITCH,  &HackCodeDip,	"dip"		},
	{"Dip Ex",			BIT_DIPSWITCH,  &VerSwitcher,	"dip"		},
};

STDINPUTINFOEXT(pgmh,		pgm,	pgmh		)
STDINPUTINFOEXT(oldsplusnr,	pgm,	oldsplusnr	)

static struct BurnDIPInfo pgmDIPList[] = {
	{0x2D,	0xFF, 0xFF,	0x00, NULL								},
	{0x2F,  0xFF, 0x01, 0x01, NULL								},

	{0,		0xFE, 0,	2,    "Test mode"						},
	{0x2D,	0x01, 0x01,	0x00, "Off"								},
	{0x2D,	0x01, 0x01,	0x01, "On"								},

	{0,		0xFE, 0,	2,    "Music"							},
	{0x2D,	0x01, 0x02,	0x02, "Off"								},
	{0x2D,	0x01, 0x02,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Voice"							},
	{0x2D,	0x01, 0x04,	0x04, "Off"								},
	{0x2D,	0x01, 0x04,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Free play"						},
	{0x2D,	0x01, 0x08,	0x00, "Off"								},
	{0x2D,	0x01, 0x08,	0x08, "On"								},

	{0,		0xFE, 0,	2,    "Stop mode"						},
	{0x2D,	0x01, 0x10,	0x00, "Off"								},
	{0x2D,	0x01, 0x10,	0x10, "On"								},

	{0,     0xFE, 0,	4,    "Bios select (Fake)"				},
	{0x2F,  0x01, 0x0f,	0x00, "Older"							},
	{0x2F,  0x01, 0x0f,	0x01, "Newer"							},
	{0x2F,  0x01, 0x0f,	0x02, "Newer (no intro, calendar)"		},
	{0x2F,  0x01, 0x0f,	0x03, "Newer (no intro)"				},
};

STDDIPINFO(pgm)

static struct BurnDIPInfo jammaDIPList[] = {
	{0x2D,	0xFF, 0xFF,	0x00, NULL								},
	{0x2F,  0xFF, 0x01, 0x00, NULL								},

	{0,		0xFE, 0,	2,    "Test mode"						},
	{0x2D,	0x01, 0x01,	0x00, "Off"								},
	{0x2D,	0x01, 0x01,	0x01, "On"								},
};

STDDIPINFO(jamma)

static struct BurnDIPInfo pgmhDIPList[] = {
	{0x2D, 0xFF, 0xFF, 0x00, NULL                               },
	{0x2F, 0xFF, 0x01, 0x01, NULL                               },
	{0x30, 0xFF, 0x01, 0x01, NULL                               },

	{0,    0xFE, 0,    2,    "Test mode"                        },
	{0x2D, 0x01, 0x01, 0x00, "Off"                              },
	{0x2D, 0x01, 0x01, 0x01, "On"                               },

	{0,    0xFE, 0,    2,    "Music"                            },
	{0x2D, 0x01, 0x02, 0x02, "Off"                              },
	{0x2D, 0x01, 0x02, 0x00, "On"                               },

	{0,    0xFE, 0,    2,    "Voice"                            },
	{0x2D, 0x01, 0x04, 0x04, "Off"                              },
	{0x2D, 0x01, 0x04, 0x00, "On"                               },

	{0,    0xFE, 0,    2,    "Free play"                        },
	{0x2D, 0x01, 0x08, 0x00, "Off"                              },
	{0x2D, 0x01, 0x08, 0x08, "On"                               },

	{0,    0xFE, 0,    2,    "Stop mode"                        },
	{0x2D, 0x01, 0x10, 0x00, "Off"                              },
	{0x2D, 0x01, 0x10, 0x10, "On"                               },

	{0,    0xFE, 0,    4,    "Bios select (Fake)"               },
	{0x2F, 0x01, 0x0f, 0x00, "Older"                            },
	{0x2F, 0x01, 0x0f, 0x01, "Newer"                            },
	{0x2F, 0x01, 0x0f, 0x02, "Newer (no intro, calendar)"       },
	{0x2F, 0x01, 0x0f, 0x03, "Newer (no intro)"                 },

	// Fake Dip
	{0,    0xFE, 0,    2,    "Code select (Must reload)"        },
	{0x30, 0x01, 0x01, 0x00, "Real hardware"                    },
	{0x30, 0x01, 0x01, 0x01, "Non-real hardware"                },
};

static struct BurnDIPInfo jammahDIPList[] = {
	{0x2D, 0xFF, 0xFF, 0x00, NULL                               },
	{0x2F, 0xFF, 0x01, 0x00, NULL                               },
	{0x30, 0xFF, 0x01, 0x01, NULL                               },

	{0,    0xFE, 0,    2,    "Test mode"                        },
	{0x2D, 0x01, 0x01, 0x00, "Off"                              },
	{0x2D, 0x01, 0x01, 0x01, "On"                               },

	// Fake Dip
	{0,    0xFE, 0,    2,    "Code select (Must reload)"        },
	{0x30, 0x01, 0x01, 0x00, "Real hardware"                    },
	{0x30, 0x01, 0x01, 0x01, "Non-real hardware"                },
};

static struct BurnDIPInfo orlegendDIPList[] = {

	DIP_OFFSET(0x2d)
	
	{0x00,	0xFF, 0xFF,	0x00, NULL								},
	{0x01,	0xFF, 0xFF,	0x05, NULL								}, // World
	{0x02,  0xFF, 0x01, 0x01, NULL								},

	{0,		0xFE, 0,	2,    "Test mode"						},
	{0x00,	0x01, 0x01,	0x00, "Off"								},
	{0x00,	0x01, 0x01,	0x01, "On"								},

	{0,		0xFE, 0,	2,    "Music"							},
	{0x00,	0x01, 0x02,	0x02, "Off"								},
	{0x00,	0x01, 0x02,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Voice"							},
	{0x00,	0x01, 0x04,	0x04, "Off"								},
	{0x00,	0x01, 0x04,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Free play"						},
	{0x00,	0x01, 0x08,	0x00, "Off"								},
	{0x00,	0x01, 0x08,	0x08, "On"								},

	{0,		0xFE, 0,	2,    "Stop mode"						},
	{0x00,	0x01, 0x10,	0x00, "Off"								},
	{0x00,	0x01, 0x10,	0x10, "On"								},

	{0,		0xFE, 0,	2,    "Disable Ba-Chieh"				},
	{0x01,	0x02, 0x01,	0x00, "Off"								},
	{0x01,	0x02, 0x01,	0x01, "On"								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x01,	0x01, 0x07,	0x00, "Taiwan"							},
	{0x01,	0x01, 0x07,	0x01, "China"							},
	{0x01,	0x01, 0x07,	0x02, "Japan"							},
	{0x01,	0x01, 0x07,	0x03, "Korea"							},
	{0x01,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x01,	0x01, 0x07,	0x05, "World"							},

	{0,     0xFE, 0,	4,    "Bios select (Fake)"				},
	{0x02,  0x01, 0x0f,	0x00, "Older"							},
	{0x02,  0x01, 0x0f,	0x01, "Newer"							},
	{0x02,  0x01, 0x0f,	0x02, "Newer (no intro, calendar)"		},
	{0x02,  0x01, 0x0f,	0x03, "Newer (no intro)"				},
};

STDDIPINFO(orlegend)

static struct BurnDIPInfo orlegendcnDIPList[] = {
	{0x01,	0xFF, 0xFF,	0x01, NULL								}, // China
};

static struct BurnDIPInfo orlegendtwDIPList[] = {
	{0x01,	0xFF, 0xFF,	0x00, NULL								}, // Taiwan
};

static struct BurnDIPInfo orlegendkrDIPList[] = {
	{0x01,	0xFF, 0xFF,	0x03, NULL								}, // Korea
};

static struct BurnDIPInfo killbldDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x17, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0xFF,	0x16, "Taiwan"							},
	{0x2E,	0x01, 0xFF,	0x17, "China"							},
	{0x2E,	0x01, 0xFF,	0x18, "Hong Kong"						},
	{0x2E,	0x01, 0xFF,	0x19, "Japan"							},
	{0x2E,	0x01, 0xFF,	0x20, "Korea"							},
	{0x2E,	0x01, 0xFF,	0x21, "World"							},
};

static struct BurnDIPInfo killbld104DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x21, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0xFF,	0x16, "Taiwan"							},
	{0x2E,	0x01, 0xFF,	0x17, "China"							},
	{0x2E,	0x01, 0xFF,	0x18, "Hong Kong"						},
	{0x2E,	0x01, 0xFF,	0x19, "Japan"							},
	{0x2E,	0x01, 0xFF,	0x20, "Korea"							},
	{0x2E,	0x01, 0xFF,	0x21, "World"							},
};

static struct BurnDIPInfo killbld100DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x16, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0xFF,	0x16, "Taiwan"							},
	{0x2E,	0x01, 0xFF,	0x17, "China"							},
	{0x2E,	0x01, 0xFF,	0x18, "Hong Kong"						},
	{0x2E,	0x01, 0xFF,	0x19, "Japan"							},
	{0x2E,	0x01, 0xFF,	0x20, "Korea"							},
	{0x2E,	0x01, 0xFF,	0x21, "World"							},
};

static struct BurnDIPInfo pgm3in1DIPList[] = {
	{0x2e,	0xFF, 0xFF,	0x03, NULL								}, // World

	{0,		0xFE, 0,	4,    "Region (Fake)"					},
	{0x2e,	0x01, 0x03,	0x00, "China"							},
	{0x2e,	0x01, 0x03,	0x01, "Taiwan"							},
	{0x2e,	0x01, 0x03,	0x02, "Hong Kong"						},
	{0x2e,	0x01, 0x03,	0x03, "World"							},
};

static struct BurnDIPInfo photoy2kDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x01, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "World"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "Singapore / Malaysia"			},
};

static struct BurnDIPInfo py2k104DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x03, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "World"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "Singapore / Malaysia"			},
};

static struct BurnDIPInfo py2k103DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "World"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "Singapore / Malaysia"			},
};

static struct BurnDIPInfo py2k2DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x03, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "World"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "Singapore / Malaysia"			},
};

static struct BurnDIPInfo py2k2100DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "World"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "Singapore / Malaysia"			},
};

static struct BurnDIPInfo oldsDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "China"							},
	{0x2E,	0x01, 0x0F,	0x03, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "World"							},
};

static struct BurnDIPInfo olds100DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "China"							},
	{0x2E,	0x01, 0x0F,	0x03, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x04, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x05, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x06, "World"							},
};

static struct BurnDIPInfo olds103tDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},
};

static struct BurnDIPInfo oldsplusnrDIPList[] = {
	{0x2E, 0xFF, 0xFF, 0x01, NULL                                       },
	{0x31, 0xFF, 0xFF, 0x00, NULL                                       },

	{0,    0xFE, 0,    2,    "Version change (Must reload)"             },
	{0x31, 0x01, 0x01, 0x00, "v211"                                     },
	{0x31, 0x01, 0x01, 0x01, "v208"                                     },
};

static struct BurnDIPInfo puzzli2DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x05, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x01, "China"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kovDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov111DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov115DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x05, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov114DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kovshp101DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kovassgDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov2DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan (Alta license)"			},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov2104DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan (Alta license)"			},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov2p202DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan (Alta license)"			},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo kov2p203DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x03, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan (Alta license)"			},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo martmastDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x06, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "World"							},
	{0x2E,	0x01, 0x07,	0x06, "USA (Andamiro USA  license)"		},
};

static struct BurnDIPInfo martmast102DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "World"							},
//	{0x2E,  0x01, 0x07, 0x06, "USA"								},
};

static struct BurnDIPInfo thegladDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
};

static struct BurnDIPInfo theglad100DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x01, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
};

static struct BurnDIPInfo theglad104DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x06, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
};

static struct BurnDIPInfo thegladpcbDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
};

static struct BurnDIPInfo dmnfrntpcbDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x03, NULL								},

	{0,		0xFE, 0,	1,    "Region (Cannot Change)"			},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
};

static struct BurnDIPInfo dmnfrntpcbaDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	6,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x03, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "World"							},
};

static struct BurnDIPInfo ddp2DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x05, NULL								},
};

static struct BurnDIPInfo ddp2hkDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},
};

static struct BurnDIPInfo ddp2kDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x03, NULL								},
};

static struct BurnDIPInfo ddp2jDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},
};

static struct BurnDIPInfo ddp2tDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x01, NULL								},
};

static struct BurnDIPInfo ddp2cDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},
};

static struct BurnDIPInfo happy6DIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	5,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x03, "Singapore"						},
	{0x2E,	0x01, 0x0F,	0x04, "World"							},
};

static struct BurnDIPInfo happy6hkDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x02, NULL								},

	{0,	    0xFE, 0,	5,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "China"							},
	{0x2E,	0x01, 0x0F,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x03, "Singapore"						},
	{0x2E,	0x01, 0x0F,	0x04, "World"							},
};

static struct BurnDIPInfo drgw3DIPList[] = {
    {0x2E,  0xFF, 0xFF, 0x06, NULL                              },

    {0,     0xFE, 0,    7,    "Region (Fake)"                   },
    {0x2E,  0x01, 0x0F, 0x01, "Japan (Alta License)"            },
    {0x2E,  0x01, 0x0F, 0x02, "Korea"                           },
    {0x2E,  0x01, 0x0F, 0x03, "Taiwan"                          },
    {0x2E,  0x01, 0x0F, 0x04, "Hong Kong"                       },
    {0x2E,  0x01, 0x0F, 0x05, "China"                           },
    {0x2E,  0x01, 0x0F, 0x06, "World"                           },
    {0x2E,  0x01, 0x0F, 0x07, "Singapore"                       },
};

static struct BurnDIPInfo drgw3106cnDIPList[] = {
    {0x2E,  0xFF, 0xFF, 0x05, NULL                              },

    {0,     0xFE, 0,    7,    "Region (Fake)"                   },
    {0x2E,  0x01, 0x0F, 0x01, "Japan (Alta License)"            },
    {0x2E,  0x01, 0x0F, 0x02, "Korea"                           },
    {0x2E,  0x01, 0x0F, 0x03, "Taiwan"                          },
    {0x2E,  0x01, 0x0F, 0x04, "Hong Kong"                       },
    {0x2E,  0x01, 0x0F, 0x05, "China"                           },
    {0x2E,  0x01, 0x0F, 0x06, "World"                           },
    {0x2E,  0x01, 0x0F, 0x07, "Singapore"                       },
};

static struct BurnDIPInfo drgw3101jaDIPList[] = {
    {0x2E,  0xFF, 0xFF, 0x01, NULL                              },

    {0,     0xFE, 0,    7,    "Region (Fake)"                   },
    {0x2E,  0x01, 0x0F, 0x01, "Japan (Alta License)"            },
    {0x2E,  0x01, 0x0F, 0x02, "Korea"                           },
    {0x2E,  0x01, 0x0F, 0x03, "Taiwan"                          },
    {0x2E,  0x01, 0x0F, 0x04, "Hong Kong"                       },
    {0x2E,  0x01, 0x0F, 0x05, "China"                           },
    {0x2E,  0x01, 0x0F, 0x06, "World"                           },
    {0x2E,  0x01, 0x0F, 0x07, "Singapore"                       },
};

static struct BurnDIPInfo dwexDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x06, NULL								},

	{0,  	0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Don't change"					},
	{0x2E,	0x01, 0x0F,	0x01, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x03, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "China"							},
	{0x2E,	0x01, 0x0F,	0x06, "World"							},
};

static struct BurnDIPInfo dwex101cnDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x05, NULL								},

	{0,	    0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x0F,	0x00, "Don't change"					},
	{0x2E,	0x01, 0x0F,	0x01, "Japan"							},
	{0x2E,	0x01, 0x0F,	0x02, "Korea"							},
	{0x2E,	0x01, 0x0F,	0x03, "Taiwan"							},
	{0x2E,	0x01, 0x0F,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x0F,	0x05, "China"							},
	{0x2E,	0x01, 0x0F,	0x06, "World"							},
};

static struct BurnDIPInfo svgDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x00, NULL								},

	{0,		0xFE, 0,	7,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
};

static struct BurnDIPInfo svgtwDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x01, NULL								},

	{0,		0xFE, 0,	8,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
	{0x2E,  0x01, 0x07, 0xff, "Don't Change"					},
};

static struct BurnDIPInfo svghkDIPList[] = {
	{0x2E,	0xFF, 0xFF,	0x04, NULL								},

	{0,		0xFE, 0,	8,    "Region (Fake)"					},
	{0x2E,	0x01, 0x07,	0x00, "China"							},
	{0x2E,	0x01, 0x07,	0x01, "Taiwan"							},
	{0x2E,	0x01, 0x07,	0x02, "Japan"							},
	{0x2E,	0x01, 0x07,	0x03, "Korea"							},
	{0x2E,	0x01, 0x07,	0x04, "Hong Kong"						},
	{0x2E,	0x01, 0x07,	0x05, "Spanish Territories"				},
	{0x2E,  0x01, 0x07, 0x06, "World"							},
	{0x2E,  0x01, 0x07, 0xff, "Don't Change"					},
};

static struct BurnDIPInfo kov2dzxxDIPList[] = {
	{0,		0xFE, 0,	2,    "Music"							},
	{0x2D,	0x01, 0x02,	0x02, "Off"								},
	{0x2D,	0x01, 0x02,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Voice"							},
	{0x2D,	0x01, 0x04,	0x04, "Off"								},
	{0x2D,	0x01, 0x04,	0x00, "On"								},

	{0,		0xFE, 0,	2,    "Free play"						},
	{0x2D,	0x01, 0x08,	0x00, "Off"								},
	{0x2D,	0x01, 0x08,	0x08, "On"								},

	{0,		0xFE, 0,	2,    "Stop mode"						},
	{0x2D,	0x01, 0x10,	0x00, "Off"								},
	{0x2D,	0x01, 0x10,	0x10, "On"								},
};

STDDIPINFOEXT(orlegendcn,	orlegend,	orlegendcn	)
STDDIPINFOEXT(orlegendtw,	orlegend,	orlegendtw	)
STDDIPINFOEXT(orlegendkr,	orlegend,	orlegendkr	)
STDDIPINFOEXT(kov,			pgm,		kov			)
STDDIPINFOEXT(kov111,		pgm,		kov111		)
STDDIPINFOEXT(kov114,		pgm,		kov114		)
STDDIPINFOEXT(kov115,		pgm,		kov115		)
STDDIPINFOEXT(kovshp101,	pgm,		kovshp101	)
STDDIPINFOEXT(kov2,			pgm,		kov2		)
STDDIPINFOEXT(kov2104,		pgm,		kov2104		)
STDDIPINFOEXT(kov2p202,		pgm,		kov2p202	)
STDDIPINFOEXT(kov2p203,		pgm,		kov2p203	)
STDDIPINFOEXT(kovassg,		pgm,		kovassg		)
STDDIPINFOEXT(killbld,		pgm,		killbld		)
STDDIPINFOEXT(killbld104,	pgm,		killbld104	)
STDDIPINFOEXT(killbld100,	pgm,		killbld100	)
STDDIPINFOEXT(pgm3in1,		pgm,		pgm3in1		)
STDDIPINFOEXT(photoy2k,		pgm,		photoy2k	)
STDDIPINFOEXT(py2k104,		pgm,		py2k104		)
STDDIPINFOEXT(py2k103,		pgm,		py2k103		)
STDDIPINFOEXT(py2k2,		pgm,		py2k2		)
STDDIPINFOEXT(py2k2100,		pgm,		py2k2100	)
STDDIPINFOEXT(puzzli2,		pgm,		puzzli2		)
STDDIPINFOEXT(martmast,		pgm,		martmast	)
STDDIPINFOEXT(martmast102,	pgm,		martmast102	)
STDDIPINFOEXT(olds,			pgm,		olds		)
STDDIPINFOEXT(olds100,		pgm,		olds100		)
STDDIPINFOEXT(olds103t,		pgm,		olds103t	) 
STDDIPINFOEXT(ddp2,			pgm,		ddp2		)
STDDIPINFOEXT(ddp2hk,		pgm,		ddp2hk		)
STDDIPINFOEXT(ddp2k,		pgm,		ddp2k		)
STDDIPINFOEXT(ddp2j,		pgm,		ddp2j		)
STDDIPINFOEXT(ddp2t,		pgm,		ddp2t		)
STDDIPINFOEXT(ddp2c,		pgm,		ddp2c		)
STDDIPINFOEXT(theglad,		pgm,		theglad		)
STDDIPINFOEXT(theglad100,	pgm,		theglad100 	)
STDDIPINFOEXT(theglad104,	pgm,		theglad104 	)
STDDIPINFOEXT(happy6,		pgm,		happy6	 	)
STDDIPINFOEXT(happy6hk,		pgm,		happy6hk	)
STDDIPINFOEXT(drgw3,		pgm,		drgw3		)
STDDIPINFOEXT(drgw3106cn,	pgm,		drgw3106cn	)
STDDIPINFOEXT(drgw3101ja,	pgm,		drgw3101ja	)
STDDIPINFOEXT(dwex,			pgm,		dwex		)
STDDIPINFOEXT(dwex101cn,	pgm,		dwex101cn	)
STDDIPINFOEXT(svg,			pgm,		svg			)
STDDIPINFOEXT(svgtw,		pgm,		svgtw		)
STDDIPINFOEXT(svghk,		pgm,		svghk		)
STDDIPINFOEXT(dmnfrntpcb,	jamma,		dmnfrntpcb	)
STDDIPINFOEXT(dmnfrntpcba,	jamma,		dmnfrntpcba	)
STDDIPINFOEXT(thegladpcb,	jamma,		thegladpcb	)
STDDIPINFOEXT(oldschs,		pgmh,		olds100		)
STDDIPINFOEXT(oldsplusnr,	pgmh,		oldsplusnr	) 
STDDIPINFOEXT(kovchs,		pgmh,		kovassg		)
STDDIPINFOEXT(kov2dzxx,		jammah,		kov2dzxx	) // The selection of [Region] and [BIOS] is shielded

// ----------------------------------------------------------------------------
// Loading incremental roms

static void RomDiffPatch(UINT8* pSrc, INT32 nIndex, INT32 nLen)
{
	if (nLen > 0x2000000) return;
	if (nLen < 0) nLen = 0;

	UINT8* pTemp = NULL;

	if (0 != nLen)
		pTemp = BurnMalloc(0x2000000);

	if (((NULL != pTemp) && (nLen > 0)) || (0 == nLen)) {
		if (0 == nLen) pTemp = pSrc;

		BurnLoadRom(pTemp, nIndex + 0, 1);

		// pTemp = BurnMalloc(0x2000000);
		// Don't use ips patch in diff mode anymore!
		if (0 != nLen) {
			for (INT32 i = 0; i < nLen; i++) {
				// 0 when same, xor when different
				if ((UINT8)0 != pTemp[i]) pSrc[i] ^= pTemp[i];
			}

			BurnFree(pTemp);
		}
	}
}

// -----------------------------------------------------------------------------
// BIOS

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

// PGM (Polygame Master) System BIOS

static struct BurnRomInfo pgmRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles
	{ "pgm_m01s.rom", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "pgm_p01s.u20", 				0x0020000, 0xe42b166e, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001,     older  - 02/26/97 - 11:14:09)
	{ "pgm_p02s.u20", 				0x0020000, 0x78c15fa2, BRF_PRG | BRF_BIOS },	// 0x83 - 68K BIOS (V0001,     newer  - 07/10/97 - 16:36:08)
#if !defined (ROM_VERIFY)
	{ "ddp3_bios.u37",				0x0080000, 0xb3cc5c8f, BRF_PRG | BRF_BIOS }, 	// 0x84 - 68K BIOS (V0001,     custom - 07/17/97 - 19:44:59)
	{ "bios.u42",     				0x0020000, 0x517cf7a2, BRF_PRG | BRF_BIOS },	// 0x85 - 68K BIOS (V0001-01J, custom - 02/25/03 - 17:51:01)
#else
	{ "",             				0x000000, 0x00000000, 0                  },		// 0x84
	{ "",             				0x000000, 0x00000000, 0                  },		// 0x85
#endif
};

STD_ROM_PICK(pgm)
STD_ROM_FN(pgm)

struct BurnDriver BurnDrvPgm = {
	"pgm", NULL, NULL, NULL, "1997",
	"PGM (Polygame Master) System BIOS\0", "BIOS only", "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_IGS_PGM, GBF_BIOS, 0,
	NULL, pgmRomInfo, pgmRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	pgmInit, pgmExit, pgmFrame, pgmDraw, pgmScan,  &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};

static struct BurnRomInfo thegladBIOSRomDesc[] = {
	{ "pgm_t01s.u72", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles

	{ "pgm_m01s.u4", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "bios.u42",      				0x0020000, 0x517cf7a2, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001-01J, custom 02/25/03 17:51:01)
};

static struct BurnRomInfo dmnfrntBIOSRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles

	{ "pgm_m01s.rom", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "pgm_p02s.u42", 				0x0020000, 0x78c15fa2, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001, newer - 07/10/97 - 16:36:08)
};

static struct BurnRomInfo svgpcbBIOSRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles

	{ "pgm_m01s.rom", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "svg_bios.u49", 				0x0020000, 0x3346401f, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001, custom 05/13/05 16:00:00)
};

static struct BurnRomInfo ketsuiBiosRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles
	{ "",		  					0,        0,          0 },						// 0x81 - Samples

	{ "",		  					0,        0,          0 },						// 0x82 - 68K BIOS
	{ "",		  					0,        0,          0 },						// 0x83 - 68K BIOS
};

static struct BurnRomInfo espgalBiosRomDesc[] = {
	{ "t01s.u18", 					0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles
	{ "",		  					0,        0,          0 },						// 0x81 - Samples

	{ "",		  					0,        0,          0 },						// 0x82 - 68K BIOS
	{ "",		  					0,        0,          0 },						// 0x83 - 68K BIOS
};

static struct BurnRomInfo ddp3BiosRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS }, 	// 0x80 - 8x8 Text Layer Tiles

	{ "pgm_m01s.rom", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "ddp3_bios.u37",				0x0080000, 0xb3cc5c8f, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001, custom? 07/17/97 19:44:59)
};

static struct BurnRomInfo kov2dzxxBiosRomDesc[] = {
	{ "dzxx_t01s.rom", 				0x0200000, 0xdf276585, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles

	{ "pgm_m01s.rom", 				0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },	// 0x81 - Samples

	{ "dzxx_p02s.u20", 				0x0080000, 0xcb060565, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (hack)
};

static struct BurnRomInfo kovp12dwBiosRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles
	{ "12dw_m01s.rom", 				0x0200000, 0x692511a5, BRF_SND | BRF_BIOS },	// 0x81 - Samples (hack)

	{ "pgm_p01s.u20", 				0x0020000, 0xe42b166e, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001,     older  - 02/26/97 - 11:14:09)
	{ "pgm_p02s.u20", 				0x0020000, 0x78c15fa2, BRF_PRG | BRF_BIOS },	// 0x83 - 68K BIOS (V0001,     newer  - 07/10/97 - 16:36:08)
};

static struct BurnRomInfo kovp12emBiosRomDesc[] = {
	{ "pgm_t01s.rom", 				0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },	// 0x80 - 8x8 Text Layer Tiles
	{ "12em_m01s.rom", 				0x0200000, 0xbc785efc, BRF_SND | BRF_BIOS },	// 0x81 - Samples (hack)

	{ "pgm_p01s.u20", 				0x0020000, 0xe42b166e, BRF_PRG | BRF_BIOS },	// 0x82 - 68K BIOS (V0001,     older  - 02/26/97 - 11:14:09)
	{ "pgm_p02s.u20", 				0x0020000, 0x78c15fa2, BRF_PRG | BRF_BIOS },	// 0x83 - 68K BIOS (V0001,     newer  - 07/10/97 - 16:36:08)
};


// -----------------------------------------------------------------------------
// Normal Games

  
// Oriental Legend / Xiyou Shi E Zhuan (ver. 126)

/* ORIENTAL LEGEND V0001 01/14/98 18:16:38
   Shows V126T, V126H, V126X, V126Np, V106MK @ 161150 */

static struct BurnRomInfo orlegendRomDesc[] = {
	{ "pgm_p0103.u2",				0x0200000, 0xd5e93543, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend, orlegend, pgm)
STD_ROM_FN(orlegend)

static INT32 orlegendInit()
{
	pPgmProtCallback = install_protection_asic3_orlegend;

	return pgmInit();
}

struct BurnDriver BurnDrvOrlegend = {
	"orlegend", NULL, "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 126)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 126)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegendRomInfo, orlegendRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 1)

/* ORIENTAL LEGEND V0001 07/14/97 11:19:45
   V113T, V112C, V112J, V112K, V112H, V112X, V112NP, V104MK @ 161160 */
	
static struct BurnRomInfo orlegend112eRomDesc[] = {
	{ "pgm_p0102.u2",				0x0200000, 0x4d0f6cc5, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend112e, orlegend112e, pgm)
STD_ROM_FN(orlegend112e)

struct BurnDriver BurnDrvOrlegend112e = {
	"orlegende", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 1)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 112, set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend112eRomInfo, orlegend112eRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 2)

/* ORIENTAL LEGEND V0001 05/05/97 10:08:21
   V113T, V112C, V112J, V112K, V112H, V112X, V112NP, V103MK @ 160fe0 */
	
static struct BurnRomInfo orlegend112eaRomDesc[] = {
	{ "p0101_v103mk.u2",			0x0200000, 0x13e7486d, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend112ea, orlegend112ea, pgm)
STD_ROM_FN(orlegend112ea)

struct BurnDriver BurnDrvOrlegend112ea = {
	"orlegendea", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 2)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 112, set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend112eaRomInfo, orlegend112eaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 112, Chinese Board)

/* ORIENTAL LEGEND V0001 05/05/97 10:08:21
   V113T, V112C, V112J, V112K, V112H, V112X, V112NP, V103MK @ 160fe0 */
	
static struct BurnRomInfo orlegend112cRomDesc[] = {
	{ "pgm_p0101.u2",			    0x0200000, 0xb24f0c1e, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	/* Different labels comparing to others sets except for PGM B0102 in U15 and PGM M0100 in U1; it probably needs to be dumped */
	{ "pgm_a0100-1.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101-1.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102-1.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103-1.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104-1.u11",			0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105-1.u12",			0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100-1.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101-1.u10",			0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10
	
	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend112c, orlegend112c, pgm)
STD_ROM_FN(orlegend112c)

struct BurnDriver BurnDrvOrlegend112c = {
	"orlegendc", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 112, Chinese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 112, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend112cRomInfo, orlegend112cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendcnDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. ???, Chinese Board)

/* ORIENTAL LEGEND V0001 04/02/97 13:35:43
   V112T, V112C, V112J, V112K, V112H, V112X, V112NP, V102MK @ 160f90 */
   
static struct BurnRomInfo orlegend112caRomDesc[] = {
	{ "p0101.102",					0x0200000, 0x7a22e1cb, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend112ca, orlegend112ca, pgm)
STD_ROM_FN(orlegend112ca)

struct BurnDriver BurnDrvOrlegend112ca = {
	"orlegendca", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. ???, Chinese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. ???, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend112caRomInfo, orlegend112caRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendcnDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Chinese Board)

/* ORIENTAL LEGEND V0001 no date
   V111T, V111C, V111J, V111K, V111H, V111X, V111NP, MRV100 @ 160cc0 */

static struct BurnRomInfo orlegend111cRomDesc[] = {
	{ "olv_111_china_u6.u6",		0x0080000, 0x5fb86373, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "olv_111_china_u9.u9",		0x0080000, 0x83cf09c8, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "olv_111_china_u7.u7",		0x0080000, 0x6ee79faf, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "olv_111_china_u11.u11",		0x0080000, 0xb80ddd3c, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend111c, orlegend111c, pgm)
STD_ROM_FN(orlegend111c)

struct BurnDriver BurnDrvOrlegend111c = {
	"orlegend111c", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Chinese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 111, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend111cRomInfo, orlegend111cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendcnDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Taiwanese Board)

/* ORIENTAL LEGEND V0001 no date
   V111T, V111C, V111J, V111K, V111H, V111X, V111NP, MRV100 @ 160cc0 */

static struct BurnRomInfo orlegend111tRomDesc[] = {
	{ "olv_111_taiwan_u6.u6",		0x0080000, 0xb205a733, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "olv_111_taiwan_u9.u9",		0x0080000, 0x6d9d29b4, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "olv_111_taiwan_u7.u7",		0x0080000, 0x27628e87, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "olv_111_taiwan_u11.u11",		0x0080000, 0x23f33bc9, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	/* Different labels comparing to others sets except for PGM B0102 in U2 and PGM M0100 in U1; it probably needs to be dumped */
	{ "pgm_a0100b.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101b.u8",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102b.u6",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103b.u9",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104b.u7",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105b.u10",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100b.u11",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101b.u12",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u2",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend111t, orlegend111t, pgm)
STD_ROM_FN(orlegend111t)

struct BurnDriver BurnDrvOrlegend111t = {
	"orlegend111t", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Taiwanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 111, Taiwanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend111tRomInfo, orlegend111tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendtwDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Korean Board)

/* ORIENTAL LEGEND V0001 no date
   V111T, V111C, V111J, V111K, V111H, V111X, V111NP, MRV100 @ 160cc0
   IGS PCB N0-0134-2 MADE IN TAIWAN
   SCREEN VER PIC: THERE IS NO VERSION NUMBER ON BOOT SCREEN.
   ROM TYPES: U6 U7 U9 U11  MX  MX27C4000DC
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo orlegend111kRomDesc[] = {
	{ "olv_111_korea_u6.u6",		0x0080000, 0x1ff35baa, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "olv_111_korea_u9.u9",		0x0080000, 0x87b6d202, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "olv_111_korea_u7.u7",		0x0080000, 0x27628e87, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "olv_111_korea_u11.u11",		0x0080000, 0x23f33bc9, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend111k, orlegend111k, pgm)
STD_ROM_FN(orlegend111k)

struct BurnDriver BurnDrvOrlegend111k = {
	"orlegend111k", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Korean Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 111, Korean Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend111kRomInfo, orlegend111kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendkrDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Korean Board)

/* ORIENTAL LEGEND V0000 no date
   V105T, V105C, V105J, V105K, V105H, V105X, V105NP @15f650 */
	
static struct BurnRomInfo orlegend105kRomDesc[] = {
	{ "olv_105_korea_u6.u6",		0x0080000, 0xb86703fe, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "olv_105_korea_u9.u9",		0x0080000, 0x5a108e39, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "olv_105_korea_u7.u7",		0x0080000, 0x5712facc, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "olv_105_korea_u11.u11",		0x0080000, 0x40ae4d9e, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend105k, orlegend105k, pgm)
STD_ROM_FN(orlegend105k)

struct BurnDriver BurnDrvOrlegend105k = {
	"orlegend105k", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Korean Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 105, Korean Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend105kRomInfo, orlegend105kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendkrDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Taiwanese Board)

/* ORIENTAL LEGEND V0000 no date
   V105T, V105C, V105J, V105K, V105H, V105X, V105NP @15f650 
   IGS PCB N0-0134-2 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: THERE IS NO VERSION NUMBER ON BOOT SCREEN.
   ROM TYPES: U6 U7 U9 U11  MX  MX27C4000DC
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo orlegend105tRomDesc[] = {
	{ "olv_105_taiwan_u6.u6",		0x0080000, 0x2b14331f, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "olv_105_taiwan_u9.u9",		0x0080000, 0xae9d71e4, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "olv_105_taiwan_u7.u7",		0x0080000, 0x5712facc, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "olv_105_taiwan_u11.u11",		0x0080000, 0x40ae4d9e, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0100.u8",				0x0400000, 0x61425e1e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0100.u5",				0x0400000, 0x8b3bd88a, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0101.u6",				0x0400000, 0x3b9e9644, 3 | BRF_GRA },			//  3	  
	{ "pgm_a0102.u7",				0x0400000, 0x069e2c38, 3 | BRF_GRA },			//  4
	{ "pgm_a0103.u8",				0x0400000, 0x4460a3fd, 3 | BRF_GRA },			//  5
	{ "pgm_a0104.u11",				0x0400000, 0x5f8abb56, 3 | BRF_GRA },			//  6
	{ "pgm_a0105.u12",				0x0400000, 0xa17a7147, 3 | BRF_GRA },			//  7

	{ "pgm_b0100.u9",				0x0400000, 0x69d2e48c, 4 | BRF_GRA },			//  8 Sprite Masks & Color Indexes
	{ "pgm_b0101.u10",				0x0400000, 0x0d587bf3, 4 | BRF_GRA },			//  9
	{ "pgm_b0102.u15",				0x0400000, 0x43823c1e, 4 | BRF_GRA },			// 10

	{ "pgm_m0100.u1",				0x0200000, 0xe5c36c83, 5 | BRF_SND },			// 11 Samples
};

STDROMPICKEXT(orlegend105t, orlegend105t, pgm)
STD_ROM_FN(orlegend105t)

struct BurnDriver BurnDrvOrlegend105t = {
	"orlegend105t", "orlegend", "pgm", NULL, "1997",
	"Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Taiwanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Oriental Legend\0\u897f\u6e38\u91ca\u5384\u4f20\0\u897f\u904a\u91cb\u5384\u4f1d (ver. 105, Taiwanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, orlegend105tRomInfo, orlegend105tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, orlegendtwDIPInfo,
	orlegendInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World II (ver. 110X, Export)

/* IGS PCB N0- 0162 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION.
   ROM TYPES: U2 AMD AM27C4096
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw2RomDesc[] = {
	{ "dragon_ii_v-110x.u2",		0x0080000, 0x1978106b, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2, drgw2, pgm)
STD_ROM_FN(drgw2)

static void drgw2_patch()
{
	pgm_decrypt_dw2();

	UINT16 *mem16 = (UINT16*)PGM68KROM;

	// This is a hack! Overlay does not seem to exist on hardware!
	for (INT32 i = 0; i < 0x80000/2; i++) {
		if (mem16[i] == 0x4e90 && mem16[i+1] == 0x207c && (mem16[i+2] & 0xfff8) == 0x0010) {
			mem16[i] = 0x4e93;	// jsr (A0) -> (A3)
			bprintf (0, _T("%5.5x\n"), i*2);
		}
	}

	SekOpen(0);
	SekMapMemory(PGM68KROM,		0x180000, 0x1fffff, MAP_ROM); // mirror main rom
	SekClose();
}

static INT32 drgw2Init()
{
	pPgmInitCallback = drgw2_patch;
	pPgmProtCallback = install_protection_asic25_asic12_dw2;

	return pgmInit();
}

struct BurnDriver BurnDrvDrgw2 = {
	"drgw2", NULL, "pgm", NULL, "1997",
	"Dragon World II (ver. 110X, Export)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2RomInfo, drgw2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Zhongguo Long II (ver. 101C, China)

/* IGS PCB N0- 0162 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION.
   ROM TYPES: U2 AMD AM27C4096
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw2101cRomDesc[] = {
	{ "dragon_ii_v-101c.u2",		0x0080000, 0xb0c592fa, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2101c, drgw2101c, pgm)
STD_ROM_FN(drgw2101c)

struct BurnDriver BurnDrvDrgw2101c = {
	"drgw2101c", "drgw2", "pgm", NULL, "1997",
	"Zhongguo Long II (ver. 101C, China)\0", NULL, "IGS", "PolyGameMaster",
	L"\u4E2D\u570B\u9F8D\u2161 (ver. 101C, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2101cRomInfo, drgw2101cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Chuugokuryuu II (ver. 101J, Japan)

/* IGS PCB N0 -0162 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION.
   ROM TYPES: U2 AMD AM27C4096
   ROM TYPES:
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw2101jRomDesc[] = {
	{ "dragon_ii_v101j.u2",			0x0080000, 0xf8f8393e, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	// Dumped from a Japanese cart. ROM label on sticker is DRAGON II V101J; same data as v100J. 
	// Is this correct or wrong sticker applied? Needs dump from another V101 Japanese cart to confirm.
	
	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2101j, drgw2101j, pgm)
STD_ROM_FN(drgw2101j)

struct BurnDriver BurnDrvDrgw2101j = {
	"drgw2101j", "drgw2", "pgm", NULL, "1997",
	"Chuugokuryuu II (ver. 101J, Japan)\0", NULL, "IGS", "PolyGameMaster",
	L"\u4E2D\u570B\u9F8D\u2161 (ver. 101J, Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2101jRomInfo, drgw2101jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Zhongguo Long II (ver. 100C, China)

/* IGS PCB N0- 0162 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION.
   ROM TYPES: U2 AMD AM27C4096
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw2100cRomDesc[] = {
	{ "dragon_ii_v-100c.u2",		0x0080000, 0x67467981, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2100c, drgw2100c, pgm)
STD_ROM_FN(drgw2100c)

struct BurnDriver BurnDrvDrgw2100c = {
	"drgw2100c", "drgw2", "pgm", NULL, "1997",
	"Zhongguo Long II (ver. 100C, China)\0", NULL, "IGS", "PolyGameMaster",
	L"\u4E2D\u570B\u9F8D\u2161 (ver. 100C, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2100cRomInfo, drgw2100cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Chuugokuryuu II (ver. 100J, Japan)

/* IGS PCB N0- 0162 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION.
   ROM TYPES: U2 AMD AM27C4096
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw2100jRomDesc[] = {
	{ "dragon_ii_v100j.u2",			0x0080000, 0xf8f8393e, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2100j, drgw2100j, pgm)
STD_ROM_FN(drgw2100j)

struct BurnDriver BurnDrvDrgw2100j = {
	"drgw2100j", "drgw2", "pgm", NULL, "1997",
	"Chuugokuryuu II (ver. 100J, Japan)\0", NULL, "IGS (Alta license)", "PolyGameMaster",
	L"\u4E2D\u570B\u9F8D\u2161 (ver. 100J, Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2100jRomInfo, drgw2100jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World II (ver. 100X, Export)

static struct BurnRomInfo drgw2100xRomDesc[] = {
	{ "dragon_ii_v-100x.u2",		0x0080000, 0x5e71851d, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2100x, drgw2100x, pgm)
STD_ROM_FN(drgw2100x)

struct BurnDriver BurnDrvDrgw2100x = {
	"drgw2100x", "drgw2", "pgm", NULL, "1997",
	"Dragon World II (ver. 100X, Export)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2100xRomInfo, drgw2100xRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dungfong Zi Zyu II (ver. 100H, Hong Kong)

static struct BurnRomInfo drgw2100hkRomDesc[] = {
	{ "dragon_ii_v-100-h.u2",		0x0080000, 0xc6e2e6ec, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0200.u7",				0x0400000, 0xb0f6534d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0200.u5",				0x0400000, 0x13b95069, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0200.u9",				0x0400000, 0x932d0f13, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes
};

STDROMPICKEXT(drgw2100hk, drgw2100hk, pgm)
STD_ROM_FN(drgw2100hk)

struct BurnDriver BurnDrvDrgw2100hk = {
	"drgw2100hk", "drgw2", "pgm", NULL, "1997",
	"Dungfong Zi Zyu II (ver. 100H, Hong Kong)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6771\u65b9\u4e4b\u73e0\u2161 (ver. 100H, Hong Kong)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw2100hkRomInfo, drgw2100hkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	drgw2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade / Ao Jian Kuang Dao (ver. 109, Chinese Board)

static struct BurnRomInfo killbldRomDesc[] = {
	{ "p0300_v109.u9",				0x0200000, 0x2fcee215, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0300.u14",				0x0400000, 0x0922f7d9, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0300.u9",				0x0400000, 0x3f9455d3, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0301.u10",				0x0400000, 0x92776889, 3 | BRF_GRA },			//  3
	{ "pgm_a0303.u11",				0x0400000, 0x33f5cc69, 3 | BRF_GRA },			//  4
	{ "pgm_a0306.u12",				0x0400000, 0xcc018a8e, 3 | BRF_GRA },			//  5
	{ "pgm_a0307.u2",				0x0400000, 0xbc772e39, 3 | BRF_GRA },			//  6

	{ "pgm_b0300.u13",				0x0400000, 0x7f876981, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0302.u14",				0x0400000, 0xeea9c502, 4 | BRF_GRA },			//  8
	{ "pgm_b0303.u15",				0x0200000, 0x77a9652e, 4 | BRF_GRA },			//  9

	{ "pgm_m0300.u1",				0x0400000, 0x93159695, 5 | BRF_SND },			// 10 Samples

	{ "kb_u2_v109.u2",				0x0010000, 0xde3eae63, 9 | BRF_PRG | BRF_ESS },	// 11 Protection Data
};

STDROMPICKEXT(killbld, killbld, pgm)
STD_ROM_FN(killbld)

static INT32 killbldInit()
{
	pPgmInitCallback = pgm_decrypt_killbld;
	pPgmProtCallback = install_protection_asic25_asic22_killbld;

	return pgmInit();
}

struct BurnDriver BurnDrvKillbld = {
	"killbld", NULL, "pgm", NULL, "1998",
	"The Killing Blade / Ao Jian Kuang Dao (ver. 109, Chinese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade\0\u50B2\u5251\u72C2\u5200\0\u50b2\u528d\u72c2\u5200 (ver. 109, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbldRomInfo, killbldRomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbldDIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};

 
// The Killing Blade (ver. 109 alt, Chinese Board)

/* IGS PCB N0-0179-2 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   SCREEN VER PIC: GAME DOES NOT SHOW VERSION ON SCREEN.
   ROM TYPES: U2  ST M27C512
   ROM TYPES: U3 U4 U5 U6  MX MX27C4000
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo killbld109RomDesc[] = {
	{ "kb_u3_v109.u3",				0x0080000, 0xfe028dd5, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "kb_u6_v109.u6",				0x0080000, 0xe50dba01, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "kb_u4_v109.u4",				0x0080000, 0x6ac58bb3, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "kb_u5_v109.u5",				0x0080000, 0x7dabf576, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0300.u14",				0x0400000, 0x0922f7d9, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0300.u9",				0x0400000, 0x3f9455d3, 3 | BRF_GRA },			//  5 Sprite Color Data
	{ "pgm_a0301.u10",				0x0400000, 0x92776889, 3 | BRF_GRA },			//  6
	{ "pgm_a0303.u11",				0x0400000, 0x33f5cc69, 3 | BRF_GRA },			//  7
	{ "pgm_a0306.u12",				0x0400000, 0xcc018a8e, 3 | BRF_GRA },			//  8
	{ "pgm_a0307.u2",				0x0400000, 0xbc772e39, 3 | BRF_GRA },			//  9

	{ "pgm_b0300.u13",				0x0400000, 0x7f876981, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0302.u14",				0x0400000, 0xeea9c502, 4 | BRF_GRA },			// 11
	{ "pgm_b0303.u15",				0x0200000, 0x77a9652e, 4 | BRF_GRA },			// 12

	{ "pgm_m0300.u1",				0x0400000, 0x93159695, 5 | BRF_SND },			// 13 Samples

	{ "kb_u2_v109.u2",				0x0010000, 0xde3eae63, 9 | BRF_PRG | BRF_ESS },	// 14 Protection Data
};

STDROMPICKEXT(killbld109, killbld109, pgm)
STD_ROM_FN(killbld109)

struct BurnDriver BurnDrvKillbld109 = {
	"killbld109", "killbld", "pgm", NULL, "1998",
	"The Killing Blade (ver. 109 alt, Chinese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade\0\u50B2\u5251\u72C2\u5200\0\u50b2\u528d\u72c2\u5200 (ver. 109 alt, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbld109RomInfo, killbld109RomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbldDIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade / Ao Jian Kuang Dao (ver. 106)

static struct BurnRomInfo killbld106RomDesc[] = {
	{ "kb_u3_v106.u3",				0x0080000, 0x33b9111a, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "kb_u6_v106.u6",				0x0080000, 0x1c957bd7, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "kb_u4_v106.u4",				0x0080000, 0x169bbaaf, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "kb_u5_v106.u5",				0x0080000, 0xdf85abd4, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0300.u14",				0x0400000, 0x0922f7d9, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0300.u9",				0x0400000, 0x3f9455d3, 3 | BRF_GRA },			//  5 Sprite Color Data
	{ "pgm_a0301.u10",				0x0400000, 0x92776889, 3 | BRF_GRA },			//  6
	{ "pgm_a0303.u11",				0x0400000, 0x33f5cc69, 3 | BRF_GRA },			//  7
	{ "pgm_a0306.u12",				0x0400000, 0xcc018a8e, 3 | BRF_GRA },			//  8
	{ "pgm_a0307.u2",				0x0400000, 0xbc772e39, 3 | BRF_GRA },			//  9

	{ "pgm_b0300.u13",				0x0400000, 0x7f876981, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0302.u14",				0x0400000, 0xeea9c502, 4 | BRF_GRA },			// 11
	{ "pgm_b0303.u15",				0x0200000, 0x77a9652e, 4 | BRF_GRA },			// 12

	{ "pgm_m0300.u1",				0x0400000, 0x93159695, 5 | BRF_SND },			// 13 Samples

	{ "kb_u2_v106.u2",				0x0010000, 0x5df8cf51, 9 | BRF_PRG | BRF_ESS },	// 14 Protection Data
};

STDROMPICKEXT(killbld106, killbld106, pgm)
STD_ROM_FN(killbld106)

struct BurnDriver BurnDrvKillbld106 = {
	"killbld106", "killbld", "pgm", NULL, "1998",
	"The Killing Blade / Ao Jian Kuang Dao (ver. 106)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade\0\u50B2\u5251\u72C2\u5200\0\u50b2\u528d\u72c2\u5200 (ver. 106)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbld106RomInfo, killbld106RomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbld104DIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade / Ao Jian Kuang Dao (ver. 104)

static struct BurnRomInfo killbld104RomDesc[] = {
	{ "kb_u3_v104.u3",				0x0080000, 0x6db1d719, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "kb_u6_v104.u6",				0x0080000, 0x31ecc978, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "kb_u4_v104.u4",				0x0080000, 0x1ed8b2e7, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "kb_u5_v104.u5",				0x0080000, 0xa0bafc29, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0300.u14",				0x0400000, 0x0922f7d9, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0300.u9",				0x0400000, 0x3f9455d3, 3 | BRF_GRA },			//  5 Sprite Color Data
	{ "pgm_a0301.u10",				0x0400000, 0x92776889, 3 | BRF_GRA },			//  6
	{ "pgm_a0303.u11",				0x0400000, 0x33f5cc69, 3 | BRF_GRA },			//  7
	{ "pgm_a0306.u12",				0x0400000, 0xcc018a8e, 3 | BRF_GRA },			//  8
	{ "pgm_a0307.u2",				0x0400000, 0xbc772e39, 3 | BRF_GRA },			//  9

	{ "pgm_b0300.u13",				0x0400000, 0x7f876981, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0302.u14",				0x0400000, 0xeea9c502, 4 | BRF_GRA },			// 11
	{ "pgm_b0303.u15",				0x0200000, 0x77a9652e, 4 | BRF_GRA },			// 12

	{ "pgm_m0300.u1",				0x0400000, 0x93159695, 5 | BRF_SND },			// 13 Samples

	{ "kb_u2_v104.u2",				0x0010000, 0xc970f6d5, 9 | BRF_PRG | BRF_ESS },	// 14 Protection Data
};

STDROMPICKEXT(killbld104, killbld104, pgm)
STD_ROM_FN(killbld104)

struct BurnDriver BurnDrvKillbld104 = {
	"killbld104", "killbld", "pgm", NULL, "1998",
	"The Killing Blade / Ao Jian Kuang Dao (ver. 104)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade\0\u50B2\u5251\u72C2\u5200\0\u50b2\u528d\u72c2\u5200 (ver. 104)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbld104RomInfo, killbld104RomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbld104DIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade / Ao Jian Kuang Dao (ver. 100, Taiwanese Board)

/* IGS PCB N0-0179 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   SCREEN VER PIC: GAME DOES NOT SHOW VERSION ON SCREEN.
   ROM TYPES: U2  ST M27C512
   ROM TYPES: U3 U4 U5 U6  MX MX27C4000
   CREDITS TO: "XeD" */

static struct BurnRomInfo killbld100RomDesc[] = {
	{ "kb_u3_v100.u3",				0x0080000, 0xba3233a9, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "kb_u6_v100.u6",				0x0080000, 0x55ae1d39, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "kb_u4_v100.u4",				0x0080000, 0xd56a8407, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "kb_u5_v100.u5",				0x0080000, 0x99afff2b, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "pgm_t0300.u14",				0x0400000, 0x0922f7d9, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0300.u9",				0x0400000, 0x3f9455d3, 3 | BRF_GRA },			//  5 Sprite Color Data
	{ "pgm_a0301.u10",				0x0400000, 0x92776889, 3 | BRF_GRA },			//  6
	{ "pgm_a0303.u11",				0x0400000, 0x33f5cc69, 3 | BRF_GRA },			//  7
	{ "pgm_a0306.u12",				0x0400000, 0xcc018a8e, 3 | BRF_GRA },			//  8
	{ "pgm_a0307.u2",				0x0400000, 0xbc772e39, 3 | BRF_GRA },			//  9

	{ "pgm_b0300.u13",				0x0400000, 0x7f876981, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0302.u14",				0x0400000, 0xeea9c502, 4 | BRF_GRA },			// 11
	{ "pgm_b0303.u15",				0x0200000, 0x77a9652e, 4 | BRF_GRA },			// 12

	{ "pgm_m0300.u1",				0x0400000, 0x93159695, 5 | BRF_SND },			// 13 Samples

	{ "kb_u2_v100.u2",				0x0010000, 0x6fbbdcb7, 9 | BRF_PRG | BRF_ESS },	// 14 Protection Data
};

STDROMPICKEXT(killbld100, killbld100, pgm)
STD_ROM_FN(killbld100)

struct BurnDriver BurnDrvKillbld100 = {
	"killbld100", "killbld", "pgm", NULL, "1998",
	"The Killing Blade / Ao Jian Kuang Dao (ver. 100, Taiwanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade\0\u50B2\u5251\u72C2\u5200\0\u50b2\u528d\u72c2\u5200 (ver. 100, Taiwanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbld100RomInfo, killbld100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbld100DIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World 3 / Zhongguo Long 3 / Dungfong Zi Zyu 3 / Chuugokuryuu 3 Special (ver. 106)

/* IGS PCB N0-0189-1 MADE IN TAIWAN
   IGS PCN N0-0178 MADE ON TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION
   ROM TYPES: U12 U13 TI TMS27C040
   ROM TYPES: U15 ST M27C512 (SP DATA)
   CREDITS TO: "GC8TECH.COM" */
   
static struct BurnRomInfo drgw3RomDesc[] = {
	{ "dw3_v106_u12.u12",  			0x0080000, 0xc3f6838b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dw3_v106_u13.u13",  			0x0080000, 0x28284e22, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0400000, 0xb70f3357, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0400000, 0xcab6557f, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0400000, 0x031eb9ce, 5 | BRF_SND },	        //  6 Samples

	{ "dw3_text_u15.u15",  			0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(drgw3, drgw3, pgm)
STD_ROM_FN(drgw3)

static INT32 drgw3Init()
{
	pPgmInitCallback = pgm_decrypt_dw3;
	pPgmProtCallback = install_protection_asic25_asic22_drgw3;

	return pgmInit();
}

struct BurnDriver BurnDrvDrgw3 = {
	"drgw3", NULL, "pgm", NULL, "1998",
	"Dragon World 3 / Zhongguo Long 3 / Dungfong Zi Zyu 3 / Chuugokuryuu 3 Special (ver. 106)\0", NULL, "IGS", "PolyGameMaster",
	L"Dragon World 3\0\u4e2d\u570b\u9f8d 3\0\u6771\u65b9\u4e4b\u73e0 3\0\u4e2d\u570b\u9f8d 3 \u30b9\u30da\u30b7\u30e3\u30eb\0 (ver. 106)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw3RomInfo, drgw3RomName, NULL, NULL, NULL, NULL, pgmInputInfo, drgw3106cnDIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World 3 / Zhongguo Long 3 / Dungfong Zi Zyu 3 / Chuugokuryuu 3 Special (ver. 105)

static struct BurnRomInfo drgw3105RomDesc[] = {
	{ "dw3_v105_u12.u12",  			0x0080000, 0xc5e24318, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dw3_v105_u13.u13",  			0x0080000, 0x8d6c9d39, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0400000, 0xb70f3357, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0400000, 0xcab6557f, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0400000, 0x031eb9ce, 5 | BRF_SND },	        //  6 Samples

	{ "dw3_v100_u15.u15",  			0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(drgw3105, drgw3105, pgm)
STD_ROM_FN(drgw3105)

struct BurnDriver BurnDrvDrgw3105 = {
	"drgw3105", "drgw3", "pgm", NULL, "1998",
	"Dragon World 3 / Zhongguo Long 3 / Dungfong Zi Zyu 3 / Chuugokuryuu 3 Special (ver. 105)\0", NULL, "IGS", "PolyGameMaster",
	L"Dragon World 3\0\u4e2d\u570b\u9f8d 3\0\u6771\u65b9\u4e4b\u73e0 3\0\u4e2d\u570b\u9f8d 3 \u30b9\u30da\u30b7\u30e3\u30eb\0 (ver. 105)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw3105RomInfo, drgw3105RomName, NULL, NULL, NULL, NULL, pgmInputInfo, drgw3DIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Chuugokuryuu 3 Special (Japan, ver. 103)
// Japan only, has an extra game mode option!

/* IGS PCB N0-0189 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION */
   
static struct BurnRomInfo drgw3103RomDesc[] = {
	{ "dragon_iii_v103j_u12.u12",  	0x0080000, 0x275b39a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dragon_iii_v103j_u13.u13",  	0x0080000, 0x9aa56e8f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0400000, 0xb70f3357, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0400000, 0xcab6557f, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0400000, 0x031eb9ce, 5 | BRF_SND },	        //  6 Samples

	{ "dw3_v100_u15.u15",  			0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(drgw3103, drgw3103, pgm)
STD_ROM_FN(drgw3103)

struct BurnDriver BurnDrvDrgw3103 = {
	"drgw3103", "drgw3", "pgm", NULL, "1998",
	"Chuugokuryuu 3 Special (Japan, ver. 103)\0", NULL, "IGS", "PolyGameMaster",
	L"\u4e2d\u570b\u9f8d 3 \u30b9\u30da\u30b7\u30e3\u30eb\0 (Japan, ver. 103)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw3103RomInfo, drgw3103RomName, NULL, NULL, NULL, NULL, pgmInputInfo, drgw3101jaDIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Chuugokuryuu 3 Special (Japan, ver. 100)
// Japan only, has an extra game mode option!

/* IGS PCB N0-0189 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN DOES NOT SHOW VERSION
   ROM TYPES: U12 U13 TI TMS27C040
   ROM TYPES: U15 ST M27C512 (SP DATA)
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo drgw3100RomDesc[] = {
	{ "dw3_v100_u12.u12",  			0x0080000, 0x47243906, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dw3_v100_u13.u13",  			0x0080000, 0xb7cded21, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0400000, 0xb70f3357, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0400000, 0xcab6557f, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0400000, 0x031eb9ce, 5 | BRF_SND },	        //  6 Samples

	{ "dw3_v100_u15.u15",  			0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(drgw3100, drgw3100, pgm)
STD_ROM_FN(drgw3100)

struct BurnDriver BurnDrvDrgw3100 = {
	"drgw3100", "drgw3", "pgm", NULL, "1998",
	"Chuugokuryuu 3 Special (Japan, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	L"\u4e2d\u570b\u9f8d 3 \u30b9\u30da\u30b7\u30e3\u30eb\0 (Japan, ver. 100)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, drgw3100RomInfo, drgw3100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, drgw3101jaDIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World 3 EX / Zhongguo Long 3 EX / Dungfong Zi Zyu 3 EX / Chuugokuryuu 3 EX (ver. 100)

/* IGS PCB N0-0189-1 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   LABEL: DRAGON WORLD EX
   SCREEN VER PIC: BOOT SCREEN IS WORLD WITH OUT A VERSION NUMBER
   ROM TYPES: U12 U13 TI TMS27C040
   ROM TYPES: U15 ST M27C512 (SP DATA)
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo dwexRomDesc[] = {
	{ "dwex_v100.u12",   			0x0080000, 0xbc171799, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dwex_v100.u13",   			0x0080000, 0x7afe6322, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0200000, 0x9ecc950d, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0200000, 0xd36c06a4, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0200000, 0x42d54fd5, 5 | BRF_SND },	        //  6 Samples

	{ "dwiii_data_u15.u15", 		0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(dwex, dwex, pgm)
STD_ROM_FN(dwex)

struct BurnDriver BurnDrvDwex = {
	"dwex", NULL, "pgm", NULL, "2000",
	"Dragon World 3 EX / Zhongguo Long 3 EX / Dungfong Zi Zyu 3 EX / Chuugokuryuu 3 EX (ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	L"Dragon World 3 EX\0\u4e2d\u570b\u9f8d 3 EX\0\u6771\u65b9\u4e4b\u73e0 3 EX (ver. 100)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, dwexRomInfo, dwexRomName, NULL, NULL, NULL, NULL, pgmInputInfo, dwexDIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Dragon World 3 EX / Zhongguo Long 3 EX / Dungfong Zi Zyu 3 EX / Chuugokuryuu 3 EX (ver. 101CN)

/* IGS PCB N0-0189-1 MADE IN TAIWAN
   IGS PCB N0-0178 MADE IN TAIWAN
   LABEL: DRAGON WORLD EX
   SCREEN VER PIC: BOOT SCREEN IS WORLD WITH OUT A VERSION NUMBER
   ROM TYPES: U12 U13 TI TMS27C040
   ROM TYPES: U15 ST M27C512 (SP DATA)
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo dwex101cnRomDesc[] = {
	{ "dwex_v101cn.u12",   			0x0080000, 0x4f951f42, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dwex_v101cn.u13",   			0x0080000, 0x66172511, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0400.u18",  			0x0200000, 0x9ecc950d, 2 | BRF_GRA },	        //  2 Tile data

	{ "pgm_a0400.u9",   			0x0400000, 0xdd7bfd40, 3 | BRF_GRA },	        //  3 Sprite Color Data
	{ "pgm_a0401.u10",  			0x0200000, 0xd36c06a4, 3 | BRF_GRA },	        //  4

	{ "pgm_b0400.u13",  			0x0400000, 0x4bb87cc0, 4 | BRF_GRA },	        //  5 Sprite Masks & Color Indexes

	{ "pgm_m0400.u1",   			0x0200000, 0x42d54fd5, 5 | BRF_SND },	        //  6 Samples

	{ "dwiii_data_u15.u15",   		0x0010000, 0x03dc4fdf, 9 | BRF_PRG | BRF_ESS }, //  7 Protection data
};

STDROMPICKEXT(dwex101cn, dwex101cn, pgm)
STD_ROM_FN(dwex101cn)

struct BurnDriver BurnDrvDwex101cn = {
	"dwex101cn", "dwex", "pgm", NULL, "2001",
	"Dragon World 3 EX / Zhongguo Long 3 EX / Dungfong Zi Zyu 3 EX / Chuugokuryuu 3 EX (ver. 101CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Dragon World 3 EX\0\u4e2d\u570b\u9f8d 3 EX\0\u6771\u65b9\u4e4b\u73e0 3 EX (ver. 100CN)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, dwex101cnRomInfo, dwex101cnRomName, NULL, NULL, NULL, NULL, pgmInputInfo, dwex101cnDIPInfo,
	drgw3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend Super / Xiyou Shi E Zhuan Super (ver. 101, Korean Board)

static struct BurnRomInfo oldsRomDesc[] = {
	{ "sp_v101_u2.u2",				0x0080000, 0x08eb9661, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sp_v101_u3.u3",				0x0080000, 0x0a358c1e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sp_v101_u4.u4",				0x0080000, 0x766570e0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sp_v101_u5.u5",				0x0080000, 0x58662e12, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sp_v101_u1.u1",				0x0080000, 0x2b2f4f1e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pgm_t0500.u18",	  			0x0400000, 0xd881726c, 2 | BRF_GRA },			//  5 Tile data
	{ "pgm_t0501.u19",	   			0x0200000, 0xd2106864, 2 | BRF_GRA },			//  6

	{ "pgm_a0500.u5",	   			0x0400000, 0x80a59197, 3 | BRF_GRA },			//  7 Sprite Color Data
	{ "pgm_a0501.u6",	   			0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  8
	{ "pgm_a0502.u7",	   			0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  9
	{ "pgm_a0503.u8",	   			0x0400000, 0x066dffec, 3 | BRF_GRA },			// 10
	{ "pgm_a0504.u11",	   			0x0400000, 0x45337583, 3 | BRF_GRA },			// 11
	{ "pgm_a0505.u12",	   			0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			// 12
	{ "pgm_a0506.u13",	   			0x0400000, 0x087ac60c, 3 | BRF_GRA },			// 13

	{ "pgm_b0500.u9",	   			0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 14 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",	   			0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 15
	{ "pgm_b0502.u15",	   			0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 16
	{ "pgm_b0503.u16",	   			0x0400000, 0xe41d98e4, 4 | BRF_GRA },			// 17

	{ "pgm_m0500.u1",	   			0x0200000, 0x37928cdd, 5 | BRF_SND },			// 18 Samples

	{ "sp_v101_u6.u6",				0x0010000, 0x097046bc, 9 | BRF_PRG | BRF_ESS }, // 19 Protection Rom
	
#if defined (ROM_VERIFY)
	{ "ram_dump",	   				0x0040000, 0x280cfb4e, 0 | BRF_OPT },			// 20 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(olds, olds, pgm)
STD_ROM_FN(olds)

static INT32 oldsInit()
{
	pPgmProtCallback = install_protection_asic25_asic28_olds;

	return pgmInit();
}

struct BurnDriver BurnDrvOlds = {
	"olds", NULL, "pgm", NULL, "1998",
	"Oriental Legend Super / Xiyou Shi E Zhuan Super (ver. 101, Korean Board)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Oriental Legend Super\0\u897f\u6e38\u91ca\u5384\u4f1d Super (ver. 101, Korean Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, oldsRomInfo, oldsRomName, NULL, NULL, NULL, NULL, pgmInputInfo, oldsDIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 1)

/* IGS PCB N0- 0191 MADE IN TAIWAN
   IGS PCB N0- 0135 MADE IN TAIWAN
   SCREEN VER PIC: THE ROM DOES NOT DISPLAY ITS VERSION ON SCREEN.
   ROM TYPES: U2 U3 U4 U5 MX 27C4000
   ROM TYPES: U6 ST M27C512
   ROM TYPES: U1 MX 27C4096
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo olds100RomDesc[] = {
	{ "sp_v100_u2.u2",  			0x0080000, 0x517c2a06, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sp_v100_u3.u3",  			0x0080000, 0xd0e2b741, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sp_v100_u4.u4",  			0x0080000, 0x32a6bdbd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sp_v100_u5.u5",  			0x0080000, 0xb4a1cafb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sp_v100_u1.u1",  			0x0080000, 0x37ea4e75, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pgm_t0500.u18",	  			0x0400000, 0xd881726c, 2 | BRF_GRA },			//  5 Tile data
	{ "pgm_t0501.u19",	   			0x0200000, 0xd2106864, 2 | BRF_GRA },			//  6

	{ "pgm_a0500.u5",	   			0x0400000, 0x80a59197, 3 | BRF_GRA },			//  7 Sprite Color Data
	{ "pgm_a0501.u6",	   			0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  8
	{ "pgm_a0502.u7",	   			0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  9
	{ "pgm_a0503.u8",	   			0x0400000, 0x066dffec, 3 | BRF_GRA },			// 10
	{ "pgm_a0504.u11",	   			0x0400000, 0x45337583, 3 | BRF_GRA },			// 11
	{ "pgm_a0505.u12",	   			0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			// 12
	{ "pgm_a0506.u13",	   			0x0400000, 0x087ac60c, 3 | BRF_GRA },			// 13

	{ "pgm_b0500.u9",	   			0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 14 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",	   			0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 15
	{ "pgm_b0502.u15",	   			0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 16
	{ "pgm_b0503.u16",	   			0x0400000, 0xe41d98e4, 4 | BRF_GRA },			// 17

	{ "pgm_m0500.u1",	   			0x0200000, 0x37928cdd, 5 | BRF_SND },			// 18 Samples

	{ "sp_v100_u6.u6",				0x010000, 0xe7613dda, 9 | BRF_PRG | BRF_ESS },  // 19 Protection Rom
	
#if defined (ROM_VERIFY)
	{ "ram_dump",	   				0x040000, 0x280cfb4e, 0 | BRF_OPT },            // 20 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(olds100, olds100, pgm)
STD_ROM_FN(olds100)

struct BurnDriver BurnDrvOlds100 = {
	"olds100", "olds", "pgm", NULL, "1998",
	"Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 1)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Oriental Legend Super\0\u897f\u6e38\u91ca\u5384\u4f1d Super (ver. 100, set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, olds100RomInfo, olds100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, olds100DIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 2)

/* IGS PCB N0- 0191-2 MADE IN TAIWAN
   IGS PCB N0- 0191-1 MADE IN TAIWAN 
   IGS PCB N0- 0135 MADE IN TAIWAN 
   LABEL: ORIENTAL LEGEND SUPER*/

static struct BurnRomInfo olds100aRomDesc[] = {
	{ "pgm_p0500_v100.u24",			0x0400000, 0x8981fc87, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0500.u18",	  			0x0400000, 0xd881726c, 2 | BRF_GRA },			//  5 Tile data
	{ "pgm_t0501.u19",	   			0x0200000, 0xd2106864, 2 | BRF_GRA },			//  6

	{ "pgm_a0500.u5",	   			0x0400000, 0x80a59197, 3 | BRF_GRA },			//  7 Sprite Color Data
	{ "pgm_a0501.u6",	   			0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  8
	{ "pgm_a0502.u7",	   			0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  9
	{ "pgm_a0503.u8",	   			0x0400000, 0x066dffec, 3 | BRF_GRA },			// 10
	{ "pgm_a0504.u11",	   			0x0400000, 0x45337583, 3 | BRF_GRA },			// 11
	{ "pgm_a0505.u12",	   			0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			// 12
	{ "pgm_a0506.u13",	   			0x0400000, 0x087ac60c, 3 | BRF_GRA },			// 13

	{ "pgm_b0500.u9",	   			0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 14 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",	   			0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 15
	{ "pgm_b0502.u15",	   			0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 16
	{ "pgm_b0503.u16",	   			0x0400000, 0xe41d98e4, 4 | BRF_GRA },			// 17

	{ "pgm_m0500.u1",	   			0x0200000, 0x37928cdd, 5 | BRF_SND },			// 18 Samples
	
#if !defined (ROM_VERIFY)
	// ROM label SP 西遊記 DATA on sticker
	{ "sp_data.u6",	   				0x010000, 0xe7613dda, 9 | BRF_PRG | BRF_ESS },  // 15 Protection Rom
#else
	{ "ram_dump",	   				0x040000, 0x280cfb4e, 0 | BRF_OPT },			// 16 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(olds100a, olds100a, pgm)
STD_ROM_FN(olds100a)

struct BurnDriver BurnDrvOlds100a = {
	"olds100a", "olds", "pgm", NULL, "1998",
	"Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 2)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Oriental Legend Super\0\u897f\u6e38\u91ca\u5384\u4f1d Super (ver. 100, set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, olds100aRomInfo, olds100aRomName, NULL, NULL, NULL, NULL, pgmInputInfo, olds100DIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Xiyou Shi E Zhuan Super (ver. 103, China, Tencent) (unprotected)

static struct BurnRomInfo olds103tRomDesc[] = {
	{ "p0500.v103",	   				0x0400000, 0x17e32e14, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0500.u18",	  			0x0400000, 0xd881726c, 2 | BRF_GRA },			//  5 Tile data
	{ "pgm_t0501.u19",	   			0x0200000, 0xd2106864, 2 | BRF_GRA },			//  6

	{ "pgm_a0500.u5",	   			0x0400000, 0x80a59197, 3 | BRF_GRA },			//  7 Sprite Color Data
	{ "pgm_a0501.u6",	   			0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  8
	{ "pgm_a0502.u7",	   			0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  9
	{ "pgm_a0503.u8",	   			0x0400000, 0x066dffec, 3 | BRF_GRA },			// 10
	{ "pgm_a0504.u11",	   			0x0400000, 0x45337583, 3 | BRF_GRA },			// 11
	{ "pgm_a0505.u12",	   			0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			// 12
	{ "pgm_a0506.u13",	   			0x0400000, 0x087ac60c, 3 | BRF_GRA },			// 13

	{ "pgm_b0500.u9",	   			0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 14 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",	   			0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 15
	{ "pgm_b0502.u15",	   			0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 16
	{ "pgm_b0503.u16",	   			0x0400000, 0xe41d98e4, 4 | BRF_GRA },			// 17

	{ "pgm_m0500.u1",	   			0x0200000, 0x37928cdd, 5 | BRF_SND },			// 18 Samples
};

STDROMPICKEXT(olds103t, olds103t, pgm)
STD_ROM_FN(olds103t)

struct BurnDriver BurnDrvOlds103t = {
	"olds103t", "olds", "pgm", NULL, "1998",
	"Xiyou Shi E Zhuan Super (ver. 103, China, Tencent) (unprotected)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u897f\u6e38\u91ca\u5384\u4f1d Super (ver. 103, China, Tencent) (unprotected)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, olds103tRomInfo, olds103tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, olds103tDIPInfo,
	pgmInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 117, Hong Kong)

/* IGS PCB N0-0212-2 MADE IN TAIWAN
   IGS PCB N0-0213T MADE IN TAIWAN 
   SCREEN VER PIC: THE ROM DOES NOT DISPLAY ITS VERSION ON SCREEN.
   ROM TYPES: U1 ST M27C322
   CREDITS TO: "XeD" */
   
static struct BurnRomInfo kovRomDesc[] = {
	{ "pgm_p0601_v117.u1",     		0x0400000, 0xc4d19fe6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kov_igs027a.bin", 			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kov, kov, pgm)
STD_ROM_FN(kov)

static INT32 kovInit()
{
	pPgmInitCallback = pgm_decrypt_kov;
	pPgmProtCallback = install_protection_asic27_kov; // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvKov = {
	"kov", NULL, "pgm", NULL, "1999",
	"Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 117, Hong Kong)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour\0\u4e09\u56fd\u6218\u7eaa\0\u4e09\u570b\u6230\u7d00 (ver. 117, Hong Kong)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovRomInfo, kovRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 115)

static struct BurnRomInfo kov115RomDesc[] = {
	{ "p0600.115",     				0x0400000, 0x527a2924, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kov_igs027a.bin", 			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(kov115, kov115, pgm)
STD_ROM_FN(kov115)

struct BurnDriver BurnDrvKov115 = {
	"kov115", "kov", "pgm", NULL, "1999",
	"Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 115)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour\0\u4e09\u56fd\u6218\u7eaa\0\u4e09\u570b\u6230\u7d00 (ver. 115)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kov115RomInfo, kov115RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 114, Hong Kong)

/* IGS PCB N0-0212-1 MADE IN TAIWAN
   IGS PCB N0-0213T MADE IN TAIWAN 
   SCREEN VER PIC: THE ROM DOES NOT DISPLAY ITS VERSION ON SCREEN.
   ROM TYPES: U4 U5 U7 U8 ALL TI TMS27C040
   ROM TYPES: U10 TI TMS27C240
   CREDITS TO: "XeD" */
   
static struct BurnRomInfo kov114RomDesc[] = {
	{ "sav114_u4.u4",     			0x0080000, 0x4db3d4d3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sav114_u7.u7",     			0x0080000, 0xb8d12b0c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sav114_u5.u5",     			0x0080000, 0x9e586dab, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sav114_u8.u8",     			0x0080000, 0xab129997, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sav114_u10.u10",    			0x0080000, 0x8f84ecfd, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  5 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  6 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  7
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  8
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  9
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			// 11
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 12 Samples
	
	// IGS 027 55857E 100 9901 HONG KONG
	{ "kov_igs027.bin",				0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 13 Internal ARM7 Rom
};

STDROMPICKEXT(kov114, kov114, pgm)
STD_ROM_FN(kov114)

struct BurnDriver BurnDrvKov114 = {
	"kov114", "kov", "pgm", NULL, "1999",
	"Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 114, Hong Kong)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour\0\u4e09\u56fd\u6218\u7eaa\0\u4e09\u570b\u6230\u7d00 (ver. 114, Hong Kong)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kov114RomInfo, kov114RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov114DIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 111, Japanese Board)

/* IGS PCB N0-0212-1 MADE IN TAIWAN
   IGS PCB N0-0213T MADE IN TAIWAN
   三国战纪 SAV111_U4  on sticker
   三国战纪 SAV111_U5  on sticker
   三国战纪 SAV111_U7  on sticker
   三国战纪 SAV111_U8  on sticker
   三国战纪 SAV111_U10 on sticker
   SCREEN VER PIC: THE ROM DOES NOT DISPLAY ITS VERSION ON SCREEN.
   ROM TYPES: U4 U5 U7 U8 ALL TI TMS27C040
   ROM TYPES: U10 TI TMS27C240
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kov111RomDesc[] = {
	{ "sav111_u4.u4",     			0x0080000, 0xae2f1b4e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sav111_u7.u7",     			0x0080000, 0x95eedf0e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sav111_u5.u5",     			0x0080000, 0x5fdd4aa8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sav111_u8.u8",     			0x0080000, 0x003cbf49, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sav111_u10.u10",    			0x0080000, 0xd5536107, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  5 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  6 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  7
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  8
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  9
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			// 11
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 12 Samples	
	
	{ "kov_igs027a.bin", 			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 13 Internal ARM7 Rom
};

STDROMPICKEXT(kov111, kov111, pgm)
STD_ROM_FN(kov111)

struct BurnDriver BurnDrvKov111 = {
	"kov111", "kov", "pgm", NULL, "1999",
	"Knights of Valour / Sanguo Zhan Ji / Sangoku Senki (ver. 111, Japanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour\0\u4e09\u56fd\u6218\u7eaa\0\u4e09\u570b\u6230\u7d00 (ver. 111, Japanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kov111RomInfo, kov111RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov111DIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus / Sanguo Zhan Ji Zhengzong Plus / Sangoku Senki Masamune Plus (ver. 119, set 1)

static struct BurnRomInfo kovplusRomDesc[] = {
	{ "pgm_p0603_v119.u1",     		0x0400000, 0xe4b0875d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kov_igs027a.bin", 			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovplus, kovplus, pgm)
STD_ROM_FN(kovplus)

struct BurnDriver BurnDrvKovplus = {
	"kovplus", NULL, "pgm", NULL, "1999",
	"Knights of Valour Plus / Sanguo Zhan Ji Zhengzong Plus / Sangoku Senki Masamune Plus (ver. 119, set 1)\0", "no PLUS on screen when set to KOREA", "IGS", "PolyGameMaster",
	L"Knights of Valour Plus\0\u4e09\u56fd\u6218\u7eaa \u6b63\u5b97 Plus\0\u4e09\u570b\u6230\u7d00 \u6b63\u5b97 Plus (ver. 119, set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovplusRomInfo, kovplusRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus / Sanguo Zhan Ji Zhengzong Plus / Sangoku Senki Masamune Plus (ver. 119, set 2)

/* IGS PCB N0-0212-1 MADE IN TAIWAN
   IGS PCB N0-0213 MADE IN TAIWAN
   THERE WERE NO STICKERS ON ANY OF THE EPROMS. 
   SCREEN VER PIC: V119 KOREA
   ROM TYPES: U4 U5 U7 U8 AMD AM27C040
   ROM TYPES: U10 HIT HN27C4096HG
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kovplusaRomDesc[] = {
	{ "v119.u4",       				0x0080000, 0x6750388f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v119.u7",       				0x0080000, 0xd4101ffd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v119.u5",       				0x0080000, 0x8200ece6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v119.u8",       				0x0080000, 0x71e28f27, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "v119.u10",	   				0x0080000, 0x29588ef2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  5 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  6 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  7
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  8
	{ "pgm_a0603.u9",     			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  9
	
	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			// 11
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 12 Samples	
	
	{ "kov_igs027a.bin", 			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  // 13 Internal ARM7 Rom
};

STDROMPICKEXT(kovplusa, kovplusa, pgm)
STD_ROM_FN(kovplusa)

struct BurnDriver BurnDrvKovplusa = {
	"kovplusa", "kovplus", "pgm", NULL, "1999",
	"Knights of Valour Plus / Sanguo Zhan Ji Zhengzong Plus / Sangoku Senki Masamune Plus (ver. 119, set 2)\0", "no PLUS on screen when set to KOREA", "IGS", "PolyGameMaster",
	L"Knights of Valour Plus\0\u4e09\u56fd\u6218\u7eaa \u6b63\u5b97 Plus\0\u4e09\u570b\u6230\u7d00 \u6b63\u5b97 Plus (ver. 119, set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovplusaRomInfo, kovplusaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
    kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 104, CN)

/* IGS PCB N0-0222-1 MADE IN TAIWAN
   IGS PCB-N0-213 MADE IN TAIWAN
   SCREEN VER PIC: V104 CHINA
   ROM TYPES: U1 ST M27C322
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo kovshRomDesc[] = {
	{ "pgm_p0605_v104.u1",			0x0400000, 0x7c78e5f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
//	This is the same as above, but with the last 1mb 0-filled. 
//	{ "p0600.322",					0x0400000, 0x4e2ba39b, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovsh, kovsh, pgm)
STD_ROM_FN(kovsh)

static INT32 kovshInit()
{
	pPgmInitCallback = pgm_decrypt_kovsh;
	pPgmProtCallback = install_protection_asic27a_kovsh;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriver BurnDrvKovsh = {
	"kovsh", NULL, "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 104, CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (ver. 104, CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshRomInfo, kovshRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 103, CN)

static struct BurnRomInfo kovsh103RomDesc[] = {
	{ "pgm_p0604_v103.u1",			0x0400000, 0xf0b3da82, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovsh103, kovsh103, pgm)
STD_ROM_FN(kovsh103)

struct BurnDriver BurnDrvKovsh103 = {
	"kovsh103", "kovsh", "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 103, CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (ver. 103, CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovsh103RomInfo, kovsh103RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 102, CN)

static struct BurnRomInfo kovsh102RomDesc[] = {
	{ "p0600.102",	   				0x0400000, 0xfdd4fb0f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovsh102, kovsh102, pgm)
STD_ROM_FN(kovsh102)

struct BurnDriver BurnDrvKovsh102 = {
	"kovsh102", "kovsh", "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 102, CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (ver. 102, CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovsh102RomInfo, kovsh102RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 101, CN)

static struct BurnRomInfo kovsh101RomDesc[] = {
	{ "p0600.101",	   				0x0400000, 0x517a9bcf, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovsh101, kovsh101, pgm)
STD_ROM_FN(kovsh101)

struct BurnDriver BurnDrvKovsh101 = {
	"kovsh101", "kovsh", "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 101, CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (ver. 101, CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovsh101RomInfo, kovsh101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 100, CN)

static struct BurnRomInfo kovsh100RomDesc[] = {
	{ "kovsh-v0100-u1.bin", 		0x0400000, 0xd145c1ca, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovsh100, kovsh100, pgm)
STD_ROM_FN(kovsh100)

struct BurnDriver BurnDrvKovsh100 = {
	"kovsh100", "kovsh", "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (ver. 100, CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (ver. 100, CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovsh100RomInfo, kovsh100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 105)

/* IGS PCB N0-0220-1 MADE IN TAIWAN
   IGS PCB N0 -0221-1 MADE IN TAIWAN
   SCREEN VER PIC: V105 CHINA
   ROM TYPES: U2 MASK DUMP AS M27C160
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo photoy2kRomDesc[] = {
	{ "pgm_p0701_v105.u2", 			0x0200000, 0xfab142e0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0700.u11",     			0x0080000, 0x93943b4d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0700.u2",       		0x0800000, 0x503c855b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0701.u4",       		0x0800000, 0x845e11a8, 3 | BRF_GRA },			//  3
	{ "pgm_a0702.u3", 				0x0080000, 0x42239e1b, 3 | BRF_GRA },			//  4
	
	{ "pgm_b0700.u7",       		0x0800000, 0x8cd027f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "photo_y2k_cg_v101_u6.u6",    0x0020000, 0xda02ec3e, 4 | BRF_GRA },			//  6

	{ "pgm_m0700.u5",     			0x0080000, 0xacc7afce, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k_v100_china.asic",	0x0004000, 0x1a0b68f6, 7 | BRF_PRG | BRF_ESS },  //  8 Internal ARM7 Rom
};

STDROMPICKEXT(photoy2k, photoy2k, pgm)
STD_ROM_FN(photoy2k)

static INT32 photoy2kInit()
{
	pPgmInitCallback = pgm_decrypt_photoy2k;
	pPgmProtCallback = install_protection_asic27a_kovsh;

	nPgmAsicRegionHackAddress = 0x17b1;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x000001f4);

	return nRet;
}

struct BurnDriver BurnDrvPhotoy2k = {
	"photoy2k", NULL, "pgm", NULL, "1999",
	"Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 105)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4\0\u5927\u5bb6\u6765\u627e\u78b4\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af (ver. 105)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, photoy2kRomInfo, photoy2kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, photoy2kDIPInfo,
	photoy2kInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 104)

static struct BurnRomInfo photoy2k104RomDesc[] = {
	{ "v104.16m",      				0x0200000, 0xe051070f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0700.u11",     			0x0080000, 0x93943b4d, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0700.u2",       		0x0800000, 0x503c855b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0701.u4",       		0x0800000, 0x845e11a8, 3 | BRF_GRA },			//  3
	{ "pgm_a0702.u3", 				0x0080000, 0x42239e1b, 3 | BRF_GRA },			//  4
	
	{ "pgm_b0700.u7",       		0x0800000, 0x8cd027f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "photo_y2k_cg_v101_u6.u6",    0x0020000, 0xda02ec3e, 4 | BRF_GRA },			//  6

	{ "pgm_m0700.u5",     			0x0080000, 0xacc7afce, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k_v100_china_alt.asic",	0x0004000, 0x6dd7f257, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom
};

STDROMPICKEXT(photoy2k104, photoy2k104, pgm)
STD_ROM_FN(photoy2k104)

struct BurnDriver BurnDrvPhotoy2k104 = {
	"photoy2k104", "photoy2k", "pgm", NULL, "1999",
	"Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 104)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4\0\u5927\u5bb6\u6765\u627e\u78b4\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af (ver. 104)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, photoy2k104RomInfo, photoy2k104RomName, NULL, NULL, NULL, NULL, pgmInputInfo, py2k104DIPInfo,
	photoy2kInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 103, Japanese Board)

static struct BurnRomInfo photoy2k103jRomDesc[] = {
	{ "photo_y2k_v103_u4.u4",		0x0080000, 0xc16dc699, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "photo_y2k_v103_u6.u6",   	0x0080000, 0x2e2671a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "photo_y2k_v103_u5.u5",   	0x0080000, 0x97839a61, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "photo_y2k_v103_u8.u8",   	0x0080000, 0x43af9664, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pgm_t0700.u11",				0x0080000, 0x93943b4d, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0700.u2",       		0x0800000, 0x503c855b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0701.u4",       		0x0800000, 0x845e11a8, 3 | BRF_GRA },			//  3
	{ "photo_yk2_cg_v101_u3.u3",	0x0080000, 0x42239e1b, 3 | BRF_GRA },			//  4

	{ "pgm_b0700.u7",       		0x0800000, 0x8cd027f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "photo_y2k_cg_v101_u6.u6",    0x0020000, 0xda02ec3e, 4 | BRF_GRA },			//  6

	{ "photo_y2k_sp_v102_u5.u5",	0x0080000, 0xacc7afce, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k_v100_china.asic",	0x0004000, 0x1a0b68f6, 7 | BRF_PRG | BRF_ESS }, //  8 Internal ARM7 Rom
};

STDROMPICKEXT(photoy2k103j, photoy2k103j, pgm)
STD_ROM_FN(photoy2k103j)

struct BurnDriver BurnDrvphotoy2k103j = {
	"photoy2k103j", "photoy2k", "pgm", NULL, "1999",
	"Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 103, Japanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4\0\u5927\u5bb6\u6765\u627e\u78b4\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af (ver. 103, Japanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, photoy2k103jRomInfo, photoy2k103jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, py2k103DIPInfo,
	photoy2kInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 102, Japanese Board)

/* IGS PCB N0-0220 MADE IN TAIWAN
   IGS PCB N0-0221 MADE IN TAIWAN
   SCREEN VER PIC: V102 JAPAN
   ROM TYPES: U4 U5 U6 U8 TI TMS27C040
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo photoy2k102jRomDesc[] = {
	{ "photo_y2k_v102_u4.u4",		0x0080000, 0xa65eda9f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "photo_y2k_v102_u6.u6",   	0x0080000, 0xb9ca5504, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "photo_y2k_v102_u5.u5",   	0x0080000, 0x9201621b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "photo_y2k_v102_u8.u8",   	0x0080000, 0x3be22b8f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pgm_t0700.u11",				0x0080000, 0x93943b4d, 2 | BRF_GRA },			//  4 Tile data

	{ "pgm_a0700.u2",       		0x0800000, 0x503c855b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0701.u4",       		0x0800000, 0x845e11a8, 3 | BRF_GRA },			//  3
	{ "photo_yk2_cg_v101_u3.u3", 	0x0080000, 0x42239e1b, 3 | BRF_GRA },			//  4
	
	{ "pgm_b0700.u7",       		0x0800000, 0x8cd027f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "photo_y2k_cg_v101_u6.u6",    0x0020000, 0xda02ec3e, 4 | BRF_GRA },			//  6

	{ "photo_y2k_sp_v102_u5.u5",    0x0080000, 0xacc7afce, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k_v100_china_alt.asic",	0x0004000, 0x6dd7f257, 7 | BRF_PRG | BRF_ESS }, // 14 Internal ARM7 Rom
};

STDROMPICKEXT(photoy2k102j, photoy2k102j, pgm)
STD_ROM_FN(photoy2k102j)

struct BurnDriver BurnDrvphotoy2k102j = {
	"photoy2k102j", "photoy2k", "pgm", NULL, "1999",
	"Photo Y2K / Chaoji Bi Yi Bi / Dajia Lai Zhao Cha / Real and Fake (ver. 102, Japanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4\0\u5927\u5bb6\u6765\u627e\u78b4\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af (ver. 102, Japanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, photoy2k102jRomInfo, photoy2k102jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, py2k103DIPInfo,
	photoy2kInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Puzzle Star / Mohuan Xingzuo (ver. 100MG, 09/30/99 build)
// V100MG 09/30/99 11:39:23

/* IGS PCB N0- T0236 MADE IN TAIWAN
   IGS PCB N0- T0237 MADE IN TAIWAN
   SCREEN VER PIC: V100MG CHINA
   ROM TYPES: U2 U1  MX 27C4000DC-90
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo puzlstarRomDesc[] = {
	{ "puzzlestar_u2_v100mg.u2",	0x0080000, 0x42aa03ce, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "puzzlestar_u1_v100mg.u1",	0x0080000, 0xc6ee43d7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0800.u5",	   			0x0200000, 0xf9d84e59, 2 | BRF_GRA }, 	        //  2 Tile data

	{ "pgm_a0800.u1",	   			0x0400000, 0xe1e6ec40, 3 | BRF_GRA },			//  3 Sprite Color Data

	{ "pgm_b0800.u3",	   			0x0200000, 0x52e7bef5, 4 | BRF_GRA }, 	  		//  4 Sprite Masks & Color Indexes

	{ "pgm_m0800.u2",	   			0x0400000, 0xe1a46541, 5 | BRF_SND },			//  5 Samples

	{ "puzlstar_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
};

STDROMPICKEXT(puzlstar, puzlstar, pgm)
STD_ROM_FN(puzlstar)

static INT32 puzlstarInit()
{
	pPgmInitCallback = pgm_decrypt_puzlstar;
	pPgmProtCallback = install_protection_asic27a_puzlstar; // simulation
//	pPgmProtCallback = install_protection_asic27a_kovsh;

	return pgmInit();
}

struct BurnDriver BurnDrvPuzlstar = {
	"puzlstar", NULL, "pgm", NULL, "1999",
	"Puzzle Star / Mohuan Xingzuo (ver. 100MG, 09/30/99 build)\0", "Incomplete dump", "IGS (Metro license)", "PolyGameMaster",
	L"Puzzle Star\0\u30D1\u30BA\u30EB\u30B9\u30BF\u30FC \u9B54\u5E7B\u661F\u5EA7\0\u9B54\u5E7B\u661F\u5EA7 (ver. 100MG, 09/30/99 build)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, puzlstarRomInfo, puzlstarRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	puzlstarInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Puzzle Star / Mohuan Xingzuo (ver. 100MG, 09/20/99 build)
// V100MG 09/20/99 15:16:02

static struct BurnRomInfo puzlstaraRomDesc[] = {
	{ "v100mg.u2",					0x0080000, 0x4c79d979, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v100mg.u1",					0x0080000, 0x5788b77d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0800.u5",	   			0x0200000, 0xf9d84e59, 2 | BRF_GRA }, 	        //  2 Tile data

	{ "pgm_a0800.u1",	   			0x0400000, 0xe1e6ec40, 3 | BRF_GRA },			//  3 Sprite Color Data

	{ "pgm_b0800.u3",	   			0x0200000, 0x52e7bef5, 4 | BRF_GRA }, 	  		//  4 Sprite Masks & Color Indexes

	{ "pgm_m0800.u2",	   			0x0400000, 0xe1a46541, 5 | BRF_SND },			//  5 Samples

	{ "puzlstar_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
};

STDROMPICKEXT(puzlstara, puzlstara, pgm)
STD_ROM_FN(puzlstara)

struct BurnDriver BurnDrvPuzlstara = {
	"puzlstara", "puzlstar", "pgm", NULL, "1999",
	"Puzzle Star / Mohuan Xingzuo (ver. 100MG, 09/20/99 build)\0", "Incomplete dump", "IGS (Metro license)", "PolyGameMaster",
	L"Puzzle Star\0\u30D1\u30BA\u30EB\u30B9\u30BF\u30FC \u9B54\u5E7B\u661F\u5EA7\0\u9B54\u5E7B\u661F\u5EA7 (ver. 100MG, 09/20/99 build)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, puzlstaraRomInfo, puzlstaraRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	puzlstarInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Puzzli 2 / Pao Pao Yu (ver. 100)

/* IGS PCB N0- 0259 MADE IN TAIWAN
   IGS PCB N0- 0258 MADE IN TAIWAN
   SCREEN VER PIC: BOOT SCREEN SAYS CHINA WITH NO VERSION NUMBER.
   ROM TYPES: U5 (MASK) OKI M27C3202CZTK
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo puzzli2RomDesc[] = {
	{ "v100_u5.u5",   				0x0200000, 0x1abb4595, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0900.u9",	   			0x0200000, 0x70615611, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0900.u3",	   			0x0400000, 0x14911251, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "pgm_b0900.u4",	   			0x0200000, 0x6f0638b6, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes

	{ "pgm_m0900.u2",	   			0x0400000, 0x9ea7af2e, 5 | BRF_SND },			//  4 Samples

	{ "puzzli2_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  5 Internal ARM7 Rom
};

STDROMPICKEXT(puzzli2, puzzli2, pgm)
STD_ROM_FN(puzzli2)

static INT32 puzzli2Init()
{
	nPGMDisableIRQ4 = 1;
	pPgmInitCallback = pgm_decrypt_puzzli2;
	pPgmProtCallback = install_protection_asic27a_puzzli2; // simulation
//	pPgmProtCallback = install_protection_asic27a_kovsh;

	return pgmInit();
}

struct BurnDriver BurnDrvPuzzli2 = {
	"puzzli2", NULL, "pgm", NULL, "1999",
	"Puzzli 2 / Pao Pao Yu (ver. 100)\0", "Incomplete dump", "IGS (Metro license)", "PolyGameMaster",
	L"Puzzli 2\0\u6ce1\u6ce1\u9c7c\0Biang Biang \u91e3\u9b5a\u5cf6\0\u30d1\u30c5\u30ea\u2161 (ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, puzzli2RomInfo, puzzli2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, puzzli2DIPInfo,
	puzzli2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Puzzli 2 Super / Pao Pao Yu Super (ver. 200)

/* IGS PCB NO- 0259 MADE IN TAIWAN
   IGS PCB NO- 0258 MADE IN TAIWAN */
   
static struct BurnRomInfo puzzli2sRomDesc[] = {
	{ "puzzli2s_v200_u4.u4",   		0x0080000, 0xfa5c86c1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "puzzli2s_v200_u3.u3",   		0x0080000, 0x2a5ba8a6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgm_t0900.u9",	   			0x0200000, 0x70615611, 2 | BRF_GRA },			//  2 Tile data

	{ "pgm_a0900.u3",	   			0x0400000, 0x14911251, 3 | BRF_GRA },			//  3 Sprite Color Data

	{ "pgm_b0900.u4",	   			0x0200000, 0x6f0638b6, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "pgm_m0900.u2",	   			0x0400000, 0x9ea7af2e, 5 | BRF_SND },			//  5 Samples

	{ "puzzli2_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
};

STDROMPICKEXT(puzzli2s, puzzli2s, pgm)
STD_ROM_FN(puzzli2s)

struct BurnDriver BurnDrvPuzzli2s = {
	"puzzli2s", "puzzli2", "pgm", NULL, "2001",
	"Puzzli 2 Super / Pao Pao Yu Super (ver. 200)\0", "Incomplete dump", "IGS (Metro license)", "PolyGameMaster",
	L"Puzzli 2 Super\0\u6ce1\u6ce1\u9c7c Super\0Biang Biang  \u91e3\u9b5a\u5cf6 (ver. 200)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, puzzli2sRomInfo, puzzli2sRomName, NULL, NULL, NULL, NULL, pgmInputInfo, puzzli2DIPInfo,
	puzzli2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters / Xing Yi Quan (ver. 104, 102, 102US)

static struct BurnRomInfo martmastRomDesc[] = {
	{ "v104_32m.u9",   				0x0400000, 0xcfd9dff4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10

	{ "martial_masters_v102_usa.asic",	0x0004000, 0xa6c0828c, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "v102_16m.u10",  				0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(martmast, martmast, pgm)
STD_ROM_FN(martmast)

static INT32 martmastInit()
{
	pPgmInitCallback = pgm_decrypt_martmast;
	pPgmProtCallback = install_protection_asic27a_martmast;

	nPgmAsicRegionHackAddress = 0x37c1;
	
	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x800039e);

	return nRet;
}

struct BurnDriver BurnDrvMartmast = {
	"martmast", NULL, "pgm", NULL, "2001",
	"Martial Masters / Xing Yi Quan (ver. 104, 102, 102US)\0", NULL, "IGS", "PolyGameMaster",
	L"Martial Masters\0\u5f62\u610f\u62f3 (ver. 104, 102, 102US)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, martmastRomInfo, martmastRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmastDIPInfo,
	martmastInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters / Xing Yi Quan (ver. 104, 102, 101CN)
// Needs proper martial_masters_v102_cn.asic to be dumped

/* IGS PCB-0293-01 MADE IN TAIWAN
   IGS PCB-0292-00 MADE IN TAIWAN
   形意拳 V102_16M on sticker IGS PCB-0301-00 MADE IN TAIWAN
   形意拳 V104_32M on sticker IGS PCB N0-0268-2 MADE IN TAIWAN */

static struct BurnRomInfo martmast104cRomDesc[] = {
	{ "v104_32m.u9",   				0x0400000, 0xcfd9dff4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10

//	{ "martial_masters_v102_cn.asic",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  // 11 Internal ARM7 Rom	
	{ "martial_masters_v101_cn.asic",	0x0004000, 0xb3e25b7d, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "v102_16m.u10",  				0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(martmast104c, martmast104c, pgm)
STD_ROM_FN(martmast104c)

static INT32 martmastcInit()
{
	pPgmInitCallback = pgm_decrypt_martmast;
	pPgmProtCallback = install_protection_asic27a_martmast;

	nPgmAsicRegionHackAddress = 0x2cb5;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x800039e);

	return nRet;
}

struct BurnDriver BurnDrvmartmast104c = {
	"martmast104c", "martmast", "pgm", NULL, "2001",
	"Martial Masters / Xing Yi Quan (ver. 104, 102, 101CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Martial Masters\0\u5f62\u610f\u62f3 (ver. 104, 102, 101CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, martmast104cRomInfo, martmast104cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmast102DIPInfo,
	martmastcInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters / Xing Yi Quan (ver. 103, 102, 101CN)
// Needs proper martial_masters_v102_cn.asic to be dumped

static struct BurnRomInfo martmast103cRomDesc[] = {
	{ "v103_32m.u9",   				0x0400000, 0xdf5ffbe9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10

//	{ "martial_masters_v102_cn.asic",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  // 11 Internal ARM7 Rom	
	{ "martial_masters_v101_cn.asic",	0x0004000, 0xb3e25b7d, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "v102_16m.u10",  				0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(martmast103c, martmast103c, pgm)
STD_ROM_FN(martmast103c)

struct BurnDriver BurnDrvmartmast103c = {
	"martmast103c", "martmast", "pgm", NULL, "2001",
	"Martial Masters / Xing Yi Quan (ver. 103, 102, 101CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Martial Masters\0\u5f62\u610f\u62f3 (ver. 103, 102, 101CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, martmast103cRomInfo, martmast103cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmast102DIPInfo,
	martmastcInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters / Xing Yi Quan (ver. 102, 101, 101CN)

/* IGS PCB-0293-00T MADE IN TAIWAN
   IGS PCB-0292-00 MADE IN TAIWAN
   SCREEN VER PIC: V102, V101, V101
   ROM TYPES: U9 U10 OKI M27C3202CZ */

static struct BurnRomInfo martmast102cRomDesc[] = {
	{ "mm_v102_u9.u9",			    0x0400000, 0xbb24b92a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10
	
	{ "martial_masters_v101_cn.asic",	0x0004000, 0xb3e25b7d, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "mm_v101_u10.u10",		    0x0400000, 0x41b9497c, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(martmast102c, martmast102c, pgm)
STD_ROM_FN(martmast102c)

struct BurnDriver BurnDrvmartmast102c = {
	"martmast102c", "martmast", "pgm", NULL, "2001",
	"Martial Masters / Xing Yi Quan (ver. 102, 101, 101CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Martial Masters\0\u5f62\u610f\u62f3 (ver. 102, 101, 101CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, martmast102cRomInfo, martmast102cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmast102DIPInfo,
	martmastcInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K 2 / Chaoji Bi Yi Bi 2 / Dajia Lai Zhao Cha 2 / Real and Fake 2 Photo Y2K (M101XX 05/25/01 11:02:54)

/* IGS PCB-0313-00 MADE IN TAIWAN
   IGS PCB-0314-00 MADE IN TAIWAN
   SCREEN VER PIC: M101XX CHINA
   ROM TYPES: U1 ST M27C160
   CREDITS TO: "GC8TECH.COM"*/
   
static struct BurnRomInfo py2k2RomDesc[] = {
	{ "y2k2_m-101xx.u1",			0x0200000, 0xc47795f1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_a1100.u6", 				0x0800000, 0xe32ce499, 3 | BRF_GRA },			//  1 Sprite Color Data
	{ "pgm_a1101.u7", 				0x0800000, 0x4e7568bc, 3 | BRF_GRA },			//  2
	{ "pgm_a1102.u8", 				0x0800000, 0x6da7c143, 3 | BRF_GRA },			//  3
	{ "pgm_a1103.u9", 				0x0800000, 0x0ebebfdc, 3 | BRF_GRA },			//  4

	{ "pgm_b1100.u4", 				0x0800000, 0xfa53d6f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "pgm_b1101.u5", 				0x0800000, 0x001e4c81, 4 | BRF_GRA },			//  6

	{ "pgm_m1100.u3", 				0x0200000, 0xfb1515f8, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k2.asic",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  8 Internal ARM7 Rom
};

STDROMPICKEXT(py2k2, py2k2, pgm)
STD_ROM_FN(py2k2)

static INT32 py2k2Init()
{
	pPgmInitCallback = pgm_decrypt_py2k2;
	pPgmProtCallback = install_protection_asic27a_py2k2;

	return pgmInit();
}

struct BurnDriver BurnDrvPy2k2 = {
	"py2k2", NULL, "pgm", NULL, "2001",
	"Photo Y2K 2 / Chaoji Bi Yi Bi 2 / Dajia Lai Zhao Cha 2 / Real and Fake 2 Photo Y2K (M101XX 05/25/01 11:02:54)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K 2\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4 2\0\u5927\u5bb6\u6765\u627e\u78b4 2\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af 2 Photo Y2K (M101XX 05/25/01 11:02:54)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, py2k2RomInfo, py2k2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, py2k2DIPInfo,
	py2k2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Photo Y2K 2 / Chaoji Bi Yi Bi 2 / Dajia Lai Zhao Cha 2 / Real and Fake 2 Photo Y2K (ver. 100, Japanese Board)

/* IGS PCB-0313-00 MADE IN TAIWAN
   IGS PCB-0314-00 MADE IN TAIWAN
   SCREEN VER PIC: JAPAN V100
   ROM TYPES: U1 ST M27C800
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo py2k2100RomDesc[] = {
	{ "y2kii_v100.u1",				0x0100000, 0x7a1e36ac, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_a1100.u6", 				0x0800000, 0xe32ce499, 3 | BRF_GRA },			//  1 Sprite Color Data
	{ "pgm_a1101.u7", 				0x0800000, 0x4e7568bc, 3 | BRF_GRA },			//  2
	{ "pgm_a1102.u8", 				0x0800000, 0x6da7c143, 3 | BRF_GRA },			//  3
	{ "pgm_a1103.u9", 				0x0800000, 0x0ebebfdc, 3 | BRF_GRA },			//  4

	{ "pgm_b1100.u4", 				0x0800000, 0xfa53d6f6, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "pgm_b1101.u5", 				0x0800000, 0x001e4c81, 4 | BRF_GRA },			//  6

	{ "pgm_m1100.u3", 				0x0200000, 0xfb1515f8, 5 | BRF_SND },			//  7 Samples

	{ "igs027a_photoy2k2.asic",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  8 Internal ARM7 Rom
};

STDROMPICKEXT(py2k2100, py2k2100, pgm)
STD_ROM_FN(py2k2100)

struct BurnDriver BurnDrvPy2k2100 = {
	"py2k2100", "py2k2", "pgm", NULL, "2000",
	"Photo Y2K 2 / Chaoji Bi Yi Bi 2 / Dajia Lai Zhao Cha 2 / Real and Fake 2 Photo Y2K (ver. 100, Japanese Board)\0", NULL, "IGS", "PolyGameMaster",
	L"Photo Y2K 2\0\u8d85\u7d1a\u6bd4\u4e00\u6bd4 2\0\u5927\u5bb6\u6765\u627e\u78b4 2\0\u30ea\u30a2\u30eb\u30a2\u30f3\u30c9\u30d5\u30a7\u30a4\u30af 2 Photo Y2K (ver. 100, Japanese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, py2k2100RomInfo, py2k2100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, py2k2100DIPInfo,
	py2k2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 107, 102, 100HK)
/* SCREEN VER PIC: 05/10/01 14:24:08 V107 */

static struct BurnRomInfo kov2RomDesc[] = {
	{ "v107_u18.u18",				0x0400000, 0x661a5b2c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic", 	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v102_u19.u19",				0x0200000, 0x462e2980, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2, kov2, pgm)
STD_ROM_FN(kov2)

static INT32 kov2Init()
{
	pPgmInitCallback = pgm_decrypt_kov2;
	pPgmProtCallback = install_protection_asic27a_martmast;

	nPgmAsicRegionHackAddress = 0x23e5;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x80000ae);

	return nRet;
}

static INT32 kov2107Init()
{
	INT32 nRet = kov2Init();
	
	// Fixed the bug that Xia Houyuan didn't move and the plot didn't advance after 107 version of recruiting Xia Houyuan
	if(!bDoIpsPatch) PGM68KROM[0x0a7379] = 0x67;

	return nRet;
}
struct BurnDriver BurnDrvKov2 = {
	"kov2", NULL, "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 107, 102, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 107, 102, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2RomInfo, kov2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2107Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 106, 102, 100HK)
/* SCREEN VER PIC: 02/27/01 13:26:46 V106 */

static struct BurnRomInfo kov2106RomDesc[] = {
	{ "v106_u18.u18",				0x0400000, 0x40051ad9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic", 	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v102_u19.u19",				0x0200000, 0x462e2980, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2106, kov2106, pgm)
STD_ROM_FN(kov2106)

struct BurnDriver BurnDrvKov2106 = {
	"kov2106", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 106, 102, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 106, 102, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2106RomInfo, kov2106RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 104, 102, 100HK)

/* IGS PCB-0333-01-FI MADE IN TAIWAN
   IGS PCB-0334-00 MADE IN TAIWAN
   三国战纪 2 V104_U18 on sticker 
   三国战纪 2 v102_U19 on sticker IGS PCB-0301-00 MADE IN TAIWAN
   SCREEN VER PIC: 01/19/01 10:19:38 V104
   ROM TYPES: U19 ST M27C160
   ROM TYPES: U18 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kov2104RomDesc[] = {
	{ "v104_u18.u18",				0x0400000, 0x0fdb050b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v102_u19.u19",				0x0200000, 0x462e2980, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2104, kov2104, pgm)
STD_ROM_FN(kov2104)

struct BurnDriver BurnDrvKov2104 = {
	"kov2104", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 104, 102, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 104, 102, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2104RomInfo, kov2104RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2104DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 103, 101, 100HK)
/* SCREEN VER PIC: 12/28/00 15:09:31 V103 */

static struct BurnRomInfo kov2103RomDesc[] = {
	{ "v103_u18.u18",				0x0400000, 0x98c32f76, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v101_u19.u19",				0x0200000, 0x8c35f2fe, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2103, kov2103, pgm)
STD_ROM_FN(kov2103)

struct BurnDriver BurnDrvKov2103 = {
	"kov2103", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 103, 101, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 103, 101, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2103RomInfo, kov2103RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 102, 102, 100HK)
/* SCREEN VER PIC: 12/14/00 10:33:36 V102 */

static struct BurnRomInfo kov21022RomDesc[] = {
	{ "v102_u18.u18",				0x0400000, 0xa2489c37, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v102_u19.u19",				0x0200000, 0x462e2980, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov21022, kov21022, pgm)
STD_ROM_FN(kov21022)

struct BurnDriver BurnDrvKov21022 = {
	"kov21022", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 102, 102, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 102, 102, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov21022RomInfo, kov21022RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 102, 101, 100HK)
/* SCREEN VER PIC: 12/14/00 10:33:36 V102 */

static struct BurnRomInfo kov2102RomDesc[] = {
	{ "v102_u18.u18",				0x0400000, 0xa2489c37, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v101_u19.u19",				0x0200000, 0x8c35f2fe, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2102, kov2102, pgm)
STD_ROM_FN(kov2102)

struct BurnDriver BurnDrvKov2102 = {
	"kov2102", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 102, 101, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 102, 101, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2102RomInfo, kov2102RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 101, 101, 100HK)
/* SCREEN VER PIC: 12/07/00 16:40:30 V101 */

static struct BurnRomInfo kov2101RomDesc[] = {
	{ "v101_u18.u18",				0x0400000, 0xc9926f35, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v101_u19.u19",	   			0x0200000, 0x8c35f2fe, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2101, kov2101, pgm)
STD_ROM_FN(kov2101)

struct BurnDriver BurnDrvKov2101 = {
	"kov2101", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 101, 101, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 101, 101, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2101RomInfo, kov2101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 100, 100, 100HK)
/* SCREEN VER PIC: 11/29/00 11:03:08 V100 */

static struct BurnRomInfo kov2100RomDesc[] = {
	{ "v100_u18.u18",   			0x0400000, 0x86205879, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2_v100_hongkong.asic",	0x0004000, 0xe0d7679f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v100_u19.u19",				0x0200000, 0xedd59922, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2100, kov2100, pgm)
STD_ROM_FN(kov2100)

struct BurnDriver BurnDrvKov2100 = {
	"kov2100", "kov2", "pgm", NULL, "2000",
	"Knights of Valour 2 / Sanguo Zhan Ji 2 / Sangoku Senki 2 (ver. 100, 100, 100HK)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2\0\u4e09\u570b\u6230\u7d00 2 (ver. 100, 100, 100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2100RomInfo, kov2100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2DIPInfo,
	kov2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M205XX, 200, 100CN)

/* IGS PCB-0333-03-FI MADE IN TAIWAN
   IGS-PCB-0334-00 MADE IN TAIWAN
   SCREEN VER PIC: M205XX 04/25/02 17:48:27
   ROM TYPES: U8  ST M27C322
   ROM TYPES: U23  ST M27C160
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kov2pRomDesc[] = {
	{ "v205_32m.u8",  				0x0400000, 0x3a2cc0de, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v200_16m.u23",  				0x0200000, 0x16a0c11f, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2p, kov2p, pgm)
STD_ROM_FN(kov2p)

static INT32 kov2pInit()
{
	pPgmInitCallback = pgm_decrypt_kov2p;
	pPgmProtCallback = install_protection_asic27a_martmast;

	nPgmAsicRegionHackAddress = 0x26ed;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x80000a6);

	return nRet;
}

struct BurnDriver BurnDrvKov2p = {
	"kov2p", NULL, "pgm", NULL, "2001",
	"Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M205XX, 200, 100CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Nine Dragons\0\u4e09\u570b\u6230\u7d00 2 - \u7fa4\u96c4\u4e89\u9738\0\u4e09\u570b\u6230\u7d00 2 - \u98db\u9f8d\u5728\u5929\0\u4e09\u570b\u6230\u7d00 2 - \u6b66\u5c06\u4e89\u9738 Busho Souha (ver. M205XX, 200, 100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2pRomInfo, kov2pRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M204XX, 200, 100CN)

/* IGS PCB-0333-03-FI MADE IN TAIWAN
   IGS-PCB-0334-00 MADE IN TAIWAN
   SCREEN VER PIC: M204XX 08/28/01 09:11:49
   ROM TYPES: U8  ST M27C322
   ROM TYPES: U23  ST M27C160
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo kov2p204RomDesc[] = {
	{ "v204_32m.u8",  				0x0400000, 0x583e0650, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v200_16m.u23",				0x0200000, 0x16a0c11f, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2p204, kov2p204, pgm)
STD_ROM_FN(kov2p204)

struct BurnDriver BurnDrvKov2p204 = {
	"kov2p204", "kov2p", "pgm", NULL, "2001",
	"Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M204XX, 200, 100CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Nine Dragons\0\u4e09\u570b\u6230\u7d00 2 - \u7fa4\u96c4\u4e89\u9738\0\u4e09\u570b\u6230\u7d00 2 - \u98db\u9f8d\u5728\u5929\0\u4e09\u570b\u6230\u7d00 2 - \u6b66\u5c06\u4e89\u9738 Busho Souha (ver. M204XX, 200, 100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2p204RomInfo, kov2p204RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M203XX, 200, 100CN)

/* IGS PCB-0333-02-FI MADE IN TAIWAN
   IGS PCB-0334-00 MADE IN TAIWAN
   SCREEN VER PIC: M203XX KOREA 08/13/01 16:57:32
   ROM TYPES: U8  ST M27C322
   ROM TYPES: U23  ST M27C160
   CREDITS TO: "GC8TECH.COM" */
   
static struct BurnRomInfo kov2p203RomDesc[] = {
	{ "v203_32m.u8",  				0x0400000, 0x11416886, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v200_16m.u23",				0x0200000, 0x16a0c11f, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2p203, kov2p203, pgm)
STD_ROM_FN(kov2p203)

struct BurnDriver BurnDrvKov2p203 = {
	"kov2p203", "kov2p", "pgm", NULL, "2001",
	"Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M203XX, 200, 100CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Nine Dragons\0\u4e09\u570b\u6230\u7d00 2 - \u7fa4\u96c4\u4e89\u9738\0\u4e09\u570b\u6230\u7d00 2 - \u98db\u9f8d\u5728\u5929\0\u4e09\u570b\u6230\u7d00 2 - \u6b66\u5c06\u4e89\u9738 Busho Souha (ver. M203XX, 200, 100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2p203RomInfo, kov2p203RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2p203DIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M202XX, 200, 100CN)

/* IGS PCB-0333-02-FI MADE IN TAIWAN
   IGS PCB-0334-00 MADE IN TAIWAN
   SCREEN VER PIC: M202XX 07/09/01 11:03:50
   ROM TYPES: U8  ST M27C322
   ROM TYPES: U23  ST M27C160
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kov2p202RomDesc[] = {
	{ "v202_32m.u8",  				0x0400000, 0xe9b5aa0c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v200_16m.u23",				0x0200000, 0x16a0c11f, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2p202, kov2p202, pgm)
STD_ROM_FN(kov2p202)

struct BurnDriver BurnDrvKov2p202 = {
	"kov2p202", "kov2p", "pgm", NULL, "2001",
	"Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M202XX, 200, 100CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Nine Dragons\0\u4e09\u570b\u6230\u7d00 2 - \u7fa4\u96c4\u4e89\u9738\0\u4e09\u570b\u6230\u7d00 2 - \u98db\u9f8d\u5728\u5929\0\u4e09\u570b\u6230\u7d00 2 - \u6b66\u5c06\u4e89\u9738 Busho Souha (ver. M202XX, 200, 100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2p202RomInfo, kov2p202RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov2p202DIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M200XX, 200, 100CN)

static struct BurnRomInfo kov2p200RomDesc[] = {
	{ "v200_32m.u8",   				0x0400000, 0x9a09fd61, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",	   			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",	   			0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "v200_16m.u23",				0x0200000, 0x16a0c11f, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2p200, kov2p200, pgm)
STD_ROM_FN(kov2p200)

struct BurnDriver BurnDrvKov2p200 = {
	"kov2p200", "kov2p", "pgm", NULL, "2001",
	"Knights of Valour 2 Plus - Nine Dragons / Sanguo Zhan Ji 2 Qunxiong Zhengba / Sanguo Zhan Ji 2 Feilong Zai Tian / Sangoku Senki Busyou Souha (ver. M200XX, 200, 100CN)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Nine Dragons\0\u4e09\u570b\u6230\u7d00 2 - \u7fa4\u96c4\u4e89\u9738\0\u4e09\u570b\u6230\u7d00 2 - \u98db\u9f8d\u5728\u5929\0\u4e09\u570b\u6230\u7d00 2 - \u6b66\u5c06\u4e89\u9738 Busho Souha (ver. M200XX, 200, 100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2p200RomInfo, kov2p200RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Bee Storm - DoDonPachi II definitions

#define DDP2_PROGRAM_102 	{ "v102.u8",       					0x0200000, 0x5a9ea040, 1 | BRF_PRG | BRF_ESS },
#define DDP2_PROGRAM_101 	{ "v101_16m.u8",   					0x0200000, 0x5e5786fd, 1 | BRF_PRG | BRF_ESS },
#define DDP2_PROGRAM_100 	{ "v100_u8.u8",	   					0x0200000, 0x0c8aa8ea, 1 | BRF_PRG | BRF_ESS },

#define DDP2_COMMON_ROMS \
	{ "pgm_t1300.u21",	   			0x0800000, 0xe748f0cb, 2 | BRF_GRA },			\
																					\
	{ "pgm_a1300.u1",	   			0x0800000, 0xfc87a405, 3 | BRF_GRA },			\
	{ "pgm_a1301.u2",	   			0x0800000, 0x0c8520da, 3 | BRF_GRA },			\
																					\
	{ "pgm_b1300.u7",	   			0x0800000, 0xef646604, 4 | BRF_GRA },			\
																					\
	{ "pgm_m1300.u5",	   			0x0400000, 0x82d4015d, 5 | BRF_SND },			\
																					\
	{ "v100_210.u23", 	   			0x0020000, 0x06c3dd29, 8 | BRF_PRG | BRF_ESS },

#define DDP2_ASIC027_CHINA 		{ "ddp2_igs027a_china.bin",		0x0004000, 0x8c566319, 7 | BRF_PRG | BRF_ESS },
#define DDP2_ASIC027_TAIWAN 	{ "ddp2_igs027a_taiwan.bin",	0x0004000, 0x1dd34bdc, 7 | BRF_PRG | BRF_ESS },
#define DDP2_ASIC027_JAPAN 		{ "ddp2_igs027a_japan.bin",		0x0004000, 0x742d34d2, 7 | BRF_PRG | BRF_ESS },
#define DDP2_ASIC027_KOREA 		{ "ddp2_igs027a_korea.bin",		0x0004000, 0xe5a81c17, 7 | BRF_PRG | BRF_ESS },
#define DDP2_ASIC027_HONG_KONG 	{ "ddp2_igs027a_hong_kong.bin",	0x0004000, 0xa7d1cace, 7 | BRF_PRG | BRF_ESS },
#define DDP2_ASIC027_WORLD 		{ "ddp2_igs027a_world.bin",		0x0004000, 0x3654e20b, 7 | BRF_PRG | BRF_ESS },

// DoDonPachi II - Bee Storm (World, ver. 102)

static struct BurnRomInfo ddp2RomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_WORLD
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2, ddp2, pgm)
STD_ROM_FN(ddp2)

static INT32 ddp2Init()
{
	pPgmInitCallback = pgm_decrypt_ddp2;
	pPgmProtCallback = install_protection_asic27a_martmast;

//	nPgmAsicRegionHackAddress = 0x2882; // 2883?

	INT32 nRet = pgmInit();

	if (!nRet) {
		Arm7SetIdleLoopAddress(0x8010998);
	}

	return nRet;
}

struct BurnDriver BurnDrvDdp2 = {
	"ddp2", NULL, "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (World, ver. 102)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2RomInfo, ddp2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2DIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Bee Storm (World, ver. 101)

static struct BurnRomInfo ddp2101RomDesc[] = {
	DDP2_PROGRAM_101
	DDP2_ASIC027_WORLD
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101, ddp2101, pgm)
STD_ROM_FN(ddp2101)

struct BurnDriver BurnDrvDdp2101 = {
	"ddp2101", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (World, ver. 101)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101RomInfo, ddp2101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2DIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Bee Storm (World, ver. 100)

static struct BurnRomInfo ddp2100RomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_WORLD
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100, ddp2100, pgm)
STD_ROM_FN(ddp2100)

struct BurnDriver BurnDrvDdp2100 = {
	"ddp2100", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (World, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100RomInfo, ddp2100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2DIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Fung Bou (Hong Kong, ver. 102)

static struct BurnRomInfo ddp2hkRomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_HONG_KONG
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2hk, ddp2hk, pgm)
STD_ROM_FN(ddp2hk)

struct BurnDriver BurnDrvDdp2hk = {
	"ddp2hk", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Fung Bou (Hong Kong, ver. 102)\0", NULL, "IGS", "PolyGameMaster",
	L"DoDonPachi II - \u8702\u66b4 (Hong Kong, ver. 102)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2hkRomInfo, ddp2hkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2hkDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Fung Bou (Hong Kong, ver. 101)

static struct BurnRomInfo ddp2101hkRomDesc[] = {
	DDP2_PROGRAM_101
	DDP2_ASIC027_HONG_KONG
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101hk, ddp2101hk, pgm)
STD_ROM_FN(ddp2101hk)

struct BurnDriver BurnDrvDdp2101hk = {
	"ddp2101hk", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Fung Bou (Hong Kong, ver. 101)\0", NULL, "IGS", "PolyGameMaster",
	L"DoDonPachi II - \u8702\u66b4 (Hong Kong, ver. 101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101hkRomInfo, ddp2101hkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2hkDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Fung Bou (Hong Kong, ver. 100)

static struct BurnRomInfo ddp2100hkRomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_HONG_KONG
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100hk, ddp2100hk, pgm)
STD_ROM_FN(ddp2100hk)

struct BurnDriver BurnDrvDdp2100hk = {
	"ddp2100hk", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Fung Bou (Hong Kong, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	L"DoDonPachi II - \u8702\u66b4 (Hong Kong, ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100hkRomInfo, ddp2100hkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2hkDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Bee Storm (Korea, ver. 102)

static struct BurnRomInfo ddp2kRomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_KOREA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2k, ddp2k, pgm)
STD_ROM_FN(ddp2k)

struct BurnDriver BurnDrvDdp2k = {
	"ddp2k", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (Korea, ver. 102)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2kRomInfo, ddp2kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2kDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Bee Storm (Korea, ver. 101)

static struct BurnRomInfo ddp2101kRomDesc[] = {
	DDP2_PROGRAM_101 
	DDP2_ASIC027_KOREA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101k, ddp2101k, pgm)
STD_ROM_FN(ddp2101k)

struct BurnDriver BurnDrvDdp2101k = {
	"ddp2101k", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (Korea, ver. 101)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101kRomInfo, ddp2101kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2kDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II - Bee Storm (Korea, ver. 100)

static struct BurnRomInfo ddp2100kRomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_KOREA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100k, ddp2100k, pgm)
STD_ROM_FN(ddp2100k)

struct BurnDriver BurnDrvDdp2100k = {
	"ddp2100k", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II - Bee Storm (Korea, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100kRomInfo, ddp2100kRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2kDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II (Japan, ver. 102)

static struct BurnRomInfo ddp2jRomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_JAPAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2j, ddp2j, pgm)
STD_ROM_FN(ddp2j)

struct BurnDriver BurnDrvDdp2j = {
	"ddp2j", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II (Japan, ver. 102)\0", NULL, "IGS (Cave license)", "PolyGameMaster",
	L"DoDonPachi II - \u6012\u9996\u9818\u8702 (Japan, ver. 102)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2jRomInfo, ddp2jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2jDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II (Japan, ver. 101)

static struct BurnRomInfo ddp2101jRomDesc[] = {
	DDP2_PROGRAM_101
	DDP2_ASIC027_JAPAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101j, ddp2101j, pgm)
STD_ROM_FN(ddp2101j)

struct BurnDriver BurnDrvDdp2101j = {
	"ddp2101j", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II (Japan, ver. 101)\0", NULL, "IGS (Cave license)", "PolyGameMaster",
	L"DoDonPachi II - \u6012\u9996\u9818\u8702 (Japan, ver. 101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101jRomInfo, ddp2101jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2jDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi II (Japan, ver. 100)

static struct BurnRomInfo ddp2100jRomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_JAPAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100j, ddp2100j, pgm)
STD_ROM_FN(ddp2100j)

struct BurnDriver BurnDrvDdp2100j = {
	"ddp2100j", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi II (Japan, ver. 100)\0", NULL, "IGS (Cave license)", "PolyGameMaster",
	L"DoDonPachi II - \u6012\u9996\u9818\u8702 (Japan, ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100jRomInfo, ddp2100jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2jDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (Taiwan, ver. 102)

static struct BurnRomInfo ddp2tRomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_TAIWAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2t, ddp2t, pgm)
STD_ROM_FN(ddp2t)

struct BurnDriver BurnDrvDdp2t = {
	"ddp2t", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (Taiwan, ver. 102)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9818\u8702 - \u8702\u66b4 (Taiwan, ver. 102)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2tRomInfo, ddp2tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2tDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (Taiwan, ver. 101)

static struct BurnRomInfo ddp2101tRomDesc[] = {
	DDP2_PROGRAM_101
	DDP2_ASIC027_TAIWAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101t, ddp2101t, pgm)
STD_ROM_FN(ddp2101t)

struct BurnDriver BurnDrvDdp2101t = {
	"ddp2101t", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (Taiwan, ver. 101)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9818\u8702 - \u8702\u66b4 (Taiwan, ver. 101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101tRomInfo, ddp2101tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2tDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (Taiwan, ver. 100)

static struct BurnRomInfo ddp2100tRomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_TAIWAN
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100t, ddp2100t, pgm)
STD_ROM_FN(ddp2100t)

struct BurnDriver BurnDrvDdp2100t = {
	"ddp2100t", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (Taiwan, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9818\u8702 - \u8702\u66b4 (Taiwan, ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100tRomInfo, ddp2100tRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2tDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (China, ver. 102)

static struct BurnRomInfo ddp2cRomDesc[] = {
	DDP2_PROGRAM_102
	DDP2_ASIC027_CHINA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2c, ddp2c, pgm)
STD_ROM_FN(ddp2c)

struct BurnDriver BurnDrvDdp2c = {
	"ddp2c", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (China, ver. 102)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9886\u8702 - \u8702\u66b4 (China, ver. 102)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2cRomInfo, ddp2cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2cDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (China, ver. 101)

static struct BurnRomInfo ddp2101cRomDesc[] = {
	DDP2_PROGRAM_101
	DDP2_ASIC027_CHINA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2101c, ddp2101c, pgm)
STD_ROM_FN(ddp2101c)

struct BurnDriver BurnDrvDdp2101c = {
	"ddp2101c", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (China, ver. 101)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9886\u8702 - \u8702\u66b4 (China, ver. 101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2101cRomInfo, ddp2101cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2cDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// DoDonPachi - Feng Bao (China, ver. 100)

static struct BurnRomInfo ddp2100cRomDesc[] = {
	DDP2_PROGRAM_100
	DDP2_ASIC027_CHINA
	DDP2_COMMON_ROMS
};

STDROMPICKEXT(ddp2100c, ddp2100c, pgm)
STD_ROM_FN(ddp2100c)

struct BurnDriver BurnDrvDdp2100c = {
	"ddp2100c", "ddp2", "pgm", NULL, "2001",
	"DoDonPachi - Feng Bao (China, ver. 100)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6012\u9996\u9886\u8702 - \u8702\u66b4 (China, ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp2100cRomInfo, ddp2100cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, ddp2cDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224,448,3,4
};


// Chuugokuryuu 2001 [Dragon World 2001] (V100 02/21/01 16:05:16, Japan)
// All ROMs labels are on stickers

/* IGS PCB-0349-01-FL MADE ON TAIWAN
   IGS PCB-0350-00T-FL-A MADE IN TAIWAN */

static struct BurnRomInfo dw2001RomDesc[] = {
	
	{ "dw2001_u22.u22",	   			0x0080000, 0x5cabed92, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "dw2001_u11.u11",	   			0x0200000, 0xb27cf093, 2 | BRF_GRA },			//  1 Tile data

	{ "dw2001_u2.u2",	   			0x0200000, 0xd11c733c, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "dw2001_u3.u3",	   			0x0200000, 0x1435aef2, 3 | BRF_GRA },			//  4

	{ "dw2001_u9.u9",	   			0x0200000, 0xccbca572, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "dw2001_u7.u7",	   			0x0200000, 0x4ea62f21, 5 | BRF_SND },			//  6 Samples

	{ "dw2001_igs027a_japan.bin",	0x0004000, 0x3a79159b, 7 | BRF_PRG },			//  7 Internal ARM7 rom

	{ "dw2001_u12.u12",	   			0x0080000, 0x973db1ab, 8 | BRF_PRG | BRF_ESS },	//  8 External ARM7 rom
};

STDROMPICKEXT(dw2001, dw2001, pgm)
STD_ROM_FN(dw2001)

static INT32 dw2001Init()
{
	pPgmInitCallback = pgm_decrypt_martmast;
	pPgmProtCallback = install_protection_asic27a_martmast;

	INT32 nRet = pgmInit();

	if (nRet == 0)
	{
		SekOpen(0);
		for (INT32 i = 0x180000; i < 0x500000; i+= 0x80000)
		{
			SekMapMemory(PGM68KROM, i, i + 0x7ffff, MAP_ROM); // mirrors
		}
		SekClose();

		Arm7SetIdleLoopAddress(0x8000228);
	}

	return nRet;
}

struct BurnDriver BurnDrvDw2001 = {
	"dw2001", NULL, "pgm", NULL, "2001",
	"Chuugokuryuu 2001 [Dragon World 2001] (V100 02/21/01 16:05:16, Japan)\0", "Bad sound?", "IGS", "PolyGameMaster",
	L"\u4e2d\u570b\u9f8d 2001 (V100 02/21/01 16:05:16, Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, dw2001RomInfo, dw2001RomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	dw2001Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Zhongguo Long Pretty Chance [Dragon World Pretty Chance] (V110 03/19/02 11:13:16, China)
// All ROMs labels are on stickers

/* IGS PCB-0349-01-FL MADE IN TAIWAN
   IGS PCB-0350-00T-FL-A MADE IN TAIWAN 
   SCREEN VER PIC: V110 CN
   ROM TYPES: U22(PROG) U12(ARM) MX MX27C4096DC.
   ROM TYPES: U11 ST M27C322    BACKGROUND DATA.
   ROM TYPES: U9 U7 U3 U2 ST M27C160   GFX DATA.
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo dwpcRomDesc[] = {
	{ "dwpc_v110cn_u22.u22",		0x0080000, 0x64f22362, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "dwpc_v110cn_u11.u11",		0x0400000, 0xdb219cb8, 2 | BRF_GRA },			//  1 Tile data

	{ "dwpc_v101xx_u2.u2",			0x0200000, 0x48b2f407, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "dwpc_v101xx_u3.u3",			0x0200000, 0x3bb45a97, 3 | BRF_GRA },			//  4

	{ "dwpc_v101xx_u9.u9",			0x0200000, 0x481b89b1, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "dwpc_v101xx_u7.u7",			0x0200000, 0x5cf9bada, 5 | BRF_SND },			//  6 Samples

	{ "dw2001_igs027a_japan.bin",	0x0004000, 0x3a79159b, 7 | BRF_PRG },  		    //  7 Internal ARM7 rom

	{ "dwpc_v110cn_u12.u12",		0x0080000, 0x5bb1ee6a, 8 | BRF_PRG | BRF_ESS },	//  8 External ARM7 rom
};

STDROMPICKEXT(dwpc, dwpc, pgm)
STD_ROM_FN(dwpc)

static INT32 dwpcInit()
{
	pPgmInitCallback = pgm_decrypt_dwpc;
	pPgmProtCallback = install_protection_asic27a_martmast;

	INT32 nRet = pgmInit();

	PGMARMROM[0x3c8] = 1; // Patch internal ASIC ROM region to China

	Arm7SetIdleLoopAddress(0x8000228);

	return nRet;
}

struct BurnDriver BurnDrvdwpc = {
	"dwpc", NULL, "pgm", NULL, "2001",
	"Zhongguo Long Pretty Chance [Dragon World Pretty Chance] (V110 03/19/02 11:13:16, China)\0", "Bad sound?", "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, dwpcRomInfo, dwpcRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	dwpcInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Chuugokuryuu Pretty Chance [Dragon World Pretty Chance] (V101 09/26/01 10:23:26, Japan)
// All ROMs labels are on stickers

/* IGS PCB-0349-01-FL MADE IN TAIWAN
   IGS PCB-0350-00T-FL-A MADE IN TAIWAN
   SCREEN VER PIC: DOES NOT SHOW VERSION NUMBER ON BOOT SCREEN.
   ROM TYPES: U22(PROG) U12(ARM) MX MX27C4096DC
   ROM TYPES: U11 (MASK) OKI M27C3202CZTK (BACKGROUND DATA).
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo dwpc101jRomDesc[] = {
	{ "dwpc_v101jp_u22.u22",		0x0080000, 0xb93027c0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "dwpc_v100jp_u11.u11",		0x0400000, 0xc29d8831, 2 | BRF_GRA },			//  1 Tile data

	{ "dwpc_v100jp_u2.u2",			0x0200000, 0xe7115763, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "dwpc_v100jp_u3.u3",			0x0200000, 0x49c184a4, 3 | BRF_GRA },			//  4

	{ "dwpc_v100jp_u9.u9",			0x0200000, 0x412b9913, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "dwpc_v100jp_u7.u7",			0x0200000, 0x5cf9bada, 5 | BRF_SND },			//  6 Samples

	{ "dw2001_igs027a_japan.bin",	0x0004000, 0x3a79159b, 7 | BRF_PRG },  		    //  7 Internal ARM7 rom

	{ "dwpc_v100jp_u12.u12",		0x0080000, 0x0d112126, 8 | BRF_PRG | BRF_ESS },	//  8 External ARM7 rom
};

STDROMPICKEXT(dwpc101j, dwpc101j, pgm)
STD_ROM_FN(dwpc101j)

struct BurnDriver BurnDrvdwpc101j = {
	"dwpc101j", "dwpc", "pgm", NULL, "2001",
	"Chuugokuryuu Pretty Chance [Dragon World Pretty Chance] (V101 09/26/01 10:23:26, Japan)\0", "Bad sound?", "IGS", "PolyGameMaster",
	L"\u4e2d\u570b\u9f8d \u30d7\u30ea\u30c6\u30a3\u30c1\u30e3\u30f3\u30b9 (V101 09/26/01 10:23:26, Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_PUZZLE, 0,
	NULL, dwpc101jRomInfo, dwpc101jRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	dw2001Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Demon Front / Moyu Zhanxian (68k label V105, ROM M105XX 08/05/02) (ARM label V105, ROM 08/05/02 S105XX)

/* DEMON FRONT "D12" VERSION (CART)
   IGS PCB-0387-03-FV MADE IN TAIWAN
   IGS PCB-0390-00-FV-A MADE IN TAIWAN
   SCREEN VER PIC: M105XX, S105XX, S100CN.
   ROM TYPES: ST M27C160 U5
   ROM TYPES: ST M27C322 U26
   CREDITS TO: "IGSPGM.COM" 
   
   DEMON FRONT "U1" VERSION (CART)
   IGS PCB-0387-03-FV MADE IN TAIWAN
   IGS PCB-0390-00-FV-A MADE IN TAIWAN
   SCREEN VER PIC: M105XX, S105XX, S100FA.
   ROM TYPES: ST M27C160 U5
   ROM TYPES: ST M27C322 U26
   CREDITS TO: "IGSPGM.COM" */
   
static struct BurnRomInfo dmnfrntRomDesc[] = {
	{ "v105_16m.u5",   			0x0200000, 0xbda083bd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs_t04501w064.u29",		0x0800000, 0x900eaaac, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04501w064.u3",  	0x0800000, 0x9741bea6, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04502w064.u4",  	0x0800000, 0xe104f405, 3 | BRF_GRA },			//  3
	{ "igs_a04503w064.u6",  	0x0800000, 0xbfd5cfe3, 3 | BRF_GRA },			//  4

	{ "igs_b04501w064.u9",		0x0800000, 0x29320b7d, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04502w016.u11",		0x0200000, 0x578c00e9, 4 | BRF_GRA },			//  6

	{ "igs_w04501b064.u5",  	0x0800000, 0x3ab58137, 5 | BRF_SND },			//  7 Samples

	{ "dmnfrnt_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom

	{ "v105_32m.u26",			0x0400000, 0xc798c2ef, 8 | BRF_PRG | BRF_ESS }, //  9 External ARM7 Rom
};

STDROMPICKEXT(dmnfrnt, dmnfrnt, pgm)
STD_ROM_FN(dmnfrnt)

static void pgm_fake_internal_asic()
{
	UINT16 *mem16 = (UINT16 *)PGMARMROM;

	for (INT32 i = 0; i < 0x4000/2; i+=2) {
		mem16[i + 0] = BURN_ENDIAN_SWAP_INT16(0xff1e);
		mem16[i + 1] = BURN_ENDIAN_SWAP_INT16(0xe12f);
	}

	mem16[0x000000 / 2] = BURN_ENDIAN_SWAP_INT16(0xd088);
	mem16[0x000002 / 2] = BURN_ENDIAN_SWAP_INT16(0xe59f);
	mem16[0x000004 / 2] = BURN_ENDIAN_SWAP_INT16(0x0680);
	mem16[0x000006 / 2] = BURN_ENDIAN_SWAP_INT16(0xe3a0);
	mem16[0x000008 / 2] = BURN_ENDIAN_SWAP_INT16(0xff10);
	mem16[0x00000a / 2] = BURN_ENDIAN_SWAP_INT16(0xe12f);
	mem16[0x000090 / 2] = BURN_ENDIAN_SWAP_INT16(0x0400);
	mem16[0x000092 / 2] = BURN_ENDIAN_SWAP_INT16(0x1000);
}

static void dmfrntCallback()
{
	pgm_decrypt_dfront();
	pgm_fake_internal_asic();
}

static INT32 dmnfrntInit()
{
	pPgmInitCallback = dmfrntCallback;
	pPgmProtCallback = install_protection_asic27a_svg;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x8000fb4);

	return nRet;
}

struct BurnDriver BurnDrvDmnfrnt = {
	"dmnfrnt", NULL, "pgm", NULL, "2002",
	"Demon Front / Moyu Zhanxian (68k label V105, ROM M105XX 08/05/02) (ARM label V105, ROM 08/05/02 S105XX)\0", NULL, "IGS", "PolyGameMaster",
	L"Demon Front\0\u9B54\u57DF\u6218\u7EBF\0\u9B54\u57DF\u6230\u7DDA (68k label V105, ROM M105XX 08/05/02) (ARM label V105, ROM 08/05/02 S105XX)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_RUNGUN, 0,
	NULL, dmnfrntRomInfo, dmnfrntRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	dmnfrntInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Demon Front / Moyu Zhanxian (68k label V103, ROM M103XX 07/05/02) (ARM label V103, ROM 07/05/02 S103XX)

static struct BurnRomInfo dmnfrnt103RomDesc[] = {
	{ "v103_16m.u5",   				0x0200000, 0x2ddafb3d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs_t04501w064.u29",			0x0800000, 0x900eaaac, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04501w064.u3",  		0x0800000, 0x9741bea6, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04502w064.u4",  		0x0800000, 0xe104f405, 3 | BRF_GRA },			//  3
	{ "igs_a04503w064.u6",  		0x0800000, 0xbfd5cfe3, 3 | BRF_GRA },			//  4

	{ "igs_b04501w064.u9",			0x0800000, 0x29320b7d, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04502w016.u11",			0x0200000, 0x578c00e9, 4 | BRF_GRA },			//  6

	{ "igs_w04501b064.u5",  		0x0800000, 0x3ab58137, 5 | BRF_SND },			//  7 Samples

	{ "dmnfrnt_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom

	{ "v103_32m.u26",  				0x0400000, 0xe78383a3, 8 | BRF_PRG | BRF_ESS }, //  9 External ARM7 Rom
};

STDROMPICKEXT(dmnfrnt103, dmnfrnt103, pgm)
STD_ROM_FN(dmnfrnt103)

struct BurnDriver BurnDrvdmnfrnt103 = {
	"dmnfrnt103", "dmnfrnt", "pgm", NULL, "2002",
	"Demon Front / Moyu Zhanxian (68k label V103, ROM M103XX 07/05/02) (ARM label V103, ROM 07/05/02 S103XX)\0", NULL, "IGS", "PolyGameMaster",
	L"Demon Front\0\u9B54\u57DF\u6218\u7EBF\0\u9B54\u57DF\u6230\u7DDA (68k label V103, ROM M103XX 07/05/02) (ARM label V103, ROM 07/05/02 S103XX)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_RUNGUN, 0,
	NULL, dmnfrnt103RomInfo, dmnfrnt103RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	dmnfrntInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Demon Front / Moyu Zhanxian (68k label V102, ROM M102XX 06/19/02) (ARM label V102, ROM 05/24/02 S101XX)

/* IGS PCB-0387-02-FV MADE IN TAIWAN
   IGS PCB-0390-00-FV-A MADE IN TAIWAN
   REDUMP FROM A JAPANESE CART 
   SCREEN VER PIC: M102XX, S101XX, S100JP
   ROM TYPES: U5 ST M27C160
   ROM TYPES: U26 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo dmnfrnt102RomDesc[] = {
	{ "v102_16m.u5",   				0x0200000, 0x3d4d481a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "igs_t04501w064.u29",			0x0800000, 0x900eaaac, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04501w064.u3",  		0x0800000, 0x9741bea6, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04502w064.u4",  		0x0800000, 0xe104f405, 3 | BRF_GRA },			//  3
	{ "igs_a04503w064.u6",  		0x0800000, 0xbfd5cfe3, 3 | BRF_GRA },			//  4

	{ "igs_b04501w064.u9",			0x0800000, 0x29320b7d, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04502w016.u11",			0x0200000, 0x578c00e9, 4 | BRF_GRA },			//  6

	{ "igs_w04501b064.u5",  		0x0800000, 0x3ab58137, 5 | BRF_SND },			//  7 Samples

	{ "dmnfrnt_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom

	{ "v101_32m.u26",  				0x0400000, 0x93965281, 8 | BRF_PRG | BRF_ESS }, //  9 External ARM7 Rom
};

STDROMPICKEXT(dmnfrnt102, dmnfrnt102, pgm)
STD_ROM_FN(dmnfrnt102)

struct BurnDriver BurnDrvdmnfrnta = {
	"dmnfrnt102", "dmnfrnt", "pgm", NULL, "2002",
	"Demon Front / Moyu Zhanxian (68k label V102, ROM M102XX 06/19/02) (ARM label V102, ROM 05/24/02 S101XX)\0", NULL, "IGS", "PolyGameMaster",
	L"Demon Front\0\u9B54\u57DF\u6218\u7EBF\0\u9B54\u57DF\u6230\u7DDA (68k label V102, ROM M102XX 06/19/02) (ARM label V102, ROM 05/24/02 S101XX)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_RUNGUN, 0,
	NULL, dmnfrnt102RomInfo, dmnfrnt102RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	dmnfrntInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V101) (ARM label V107, ROM 06/06/03 SHEN JIAN V107)

/* IGS PCB-0361-02-F0 MADE IN TAIWAN
   IGS PCB-0421-00-F0-A MADE IN TAIWAN
   SCREEN VER PIC: M68K VERSION V101 06/06/03 16:17:27 V107
   ROM TYPES: U6 V101 MX MX27C4096DC
   ROM TYPES: U26 V107 ST M27C160
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo thegladRomDesc[] = {
	{ "v101_u6.u6",					0x0080000, 0xf799e866, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u33",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u2",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u4",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u6",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u11",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u12",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u1",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples

//	{ "theglad_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_execute_only_area",	    0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_v100_overseas.bin",      0x0003e78, 0x02fe6f52, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "v107_u26.u26",				0x0200000, 0xf7c61357, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(theglad, theglad, pgm)
STD_ROM_FN(theglad)

static void pgm_create_theglad_EO_data()
{
	// Replace undumpable area of the arm7 internal rom with a custom-built
	// version created by David Haywood in order to make game playable
	static UINT16 thegladEOHackData[0x188/2] = {
		0x000a, 0xea00, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xf000, 0xe59f,
		0x0010, 0x0800, 0x0010, 0x0800, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0x00d2, 0xe3a0, 0xf000, 0xe121, 0x4001, 0xe3a0, 0x4b06, 0xe284,
		0x0cfa, 0xe3a0, 0xd804, 0xe080, 0x00d1, 0xe3a0, 0xf000, 0xe121,
		0x0cf6, 0xe3a0, 0xd804, 0xe080, 0x00d7, 0xe3a0, 0xf000, 0xe121,
		0x0cff, 0xe3a0, 0xd804, 0xe080, 0x00db, 0xe3a0, 0xf000, 0xe121,
		0x4140, 0xe1c4, 0x0cfe, 0xe3a0, 0xd804, 0xe080, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0x4a01, 0xe3a0, 0x0b01, 0xe3a0, 0xd804, 0xe080,
		0x5a0f, 0xe3a0, 0x0008, 0xe3a0, 0x8805, 0xe080, 0x0010, 0xe3a0,
		0x0000, 0xe5c8, 0x7805, 0xe1a0, 0x6a01, 0xe3a0, 0x0012, 0xe3a0,
		0x0a02, 0xe280, 0x6806, 0xe080, 0x6000, 0xe587, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0x4001, 0xe3a0, 0x4b06, 0xe284, 0x0cf2, 0xe3a0,
		0xd804, 0xe080, 0x0013, 0xe3a0, 0xf000, 0xe121, 0x0028, 0xea00,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xe004, 0xe52d, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0xe004, 0xe49d, 0xff1e, 0xe12f, 0xe004, 0xe52d,
		0x0013, 0xe3a0, 0xf000, 0xe121, 0xe004, 0xe49d, 0xff1e, 0xe12f,
		0x00d1, 0xe3a0, 0xf000, 0xe121, 0xd0b8, 0xe59f, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0xd0b0, 0xe59f, 0x10b8, 0xe59f, 0x0000, 0xe3a0,
		0x0000, 0xe581, 0xf302, 0xe3a0, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xff1e, 0xe12f, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0x105c, 0xe59f
	};

	for (int i = 0; i < 0x188/2; i++) {
		thegladEOHackData[i] = BURN_ENDIAN_SWAP_INT16(thegladEOHackData[i]);
	}

	memcpy(PGMARMROM, thegladEOHackData, 0x188);
}

static void thegladPatch()
{
	pgm_decrypt_theglad();
	pgm_create_theglad_EO_data();
}

static INT32 thegladInit()
{
	nPgmAsicRegionHackAddress = 0x3316;
	pPgmInitCallback = thegladPatch;
	pPgmProtCallback = install_protection_asic27a_svg;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x000007c4);

	return nRet;
}

struct BurnDriver BurnDrvTheglad = {
	"theglad", NULL, "pgm", NULL, "2003",
	"The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V101) (ARM label V107, ROM 06/06/03 SHEN JIAN V107)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"The Gladiator\0\u795e\u528d\u4f0f\u9b54\u5f55\0\u795e\u528d\u98a8\u96f2\0\u95d8\u5e7b\u72c2 - Road of the Sword\0\uc2e0\uac80\uc758\ud48d\uc6b4 (M68k label V101) (ARM label V107, ROM 06/06/03 SHEN JIAN V107)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, thegladRomInfo, thegladRomName, NULL, NULL, NULL, NULL, pgmInputInfo, thegladDIPInfo,
	thegladInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V104, ROM 04/02/03 SHEN JIAN V104)

/* IGS PCB-0361-02-F0 MADE IN TAIWAN
   IGS PCB-0421-00-F0-A MADE IN TAIWAN
   SCREEN VER PIC: 04/02/03 09:39:46 V104 */

static struct BurnRomInfo theglad104RomDesc[] = {
	{ "v100_u6.u6",					0x0080000, 0xbcf3b172, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u33",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u2",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u4",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u6",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u11",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u12",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u1",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples

//	{ "theglad_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_execute_only_area",	    0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_v100_overseas.bin",      0x0003e78, 0x02fe6f52, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "v104_u26.u26",				0x0200000, 0x81b5df6d, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(theglad104, theglad104, pgm)
STD_ROM_FN(theglad104)

struct BurnDriver BurnDrvTheglad104 = {
	"theglad104", "theglad", "pgm", NULL, "2003",
	"The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V104, ROM 04/02/03 SHEN JIAN V104)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"The Gladiator\0\u795e\u528d\u4f0f\u9b54\u5f55\0\u795e\u528d\u98a8\u96f2\0\u95d8\u5e7b\u72c2 - Road of the Sword\0\uc2e0\uac80\uc758\ud48d\uc6b4 (M68k label V100) (ARM label V104, ROM 04/02/03 SHEN JIAN V104)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, theglad104RomInfo, theglad104RomName, NULL, NULL, NULL, NULL, pgmInputInfo, theglad104DIPInfo,
	thegladInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN)

/* IGS PCB-0361-02-F0 MADE IN TAIWAN
   IGS PCB-0421-00-F0-A MADE IN TAIWAN
   SCREEN VER PIC: 03/13/03 14:06:44 V101 */

static struct BurnRomInfo theglad101RomDesc[] = {
	{ "v100_u6.u6",					0x0080000, 0xbcf3b172, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u33",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u2",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u4",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u6",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u11",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u12",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u1",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples

//	{ "theglad_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_execute_only_area",	    0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_v100_overseas.bin",      0x0003e78, 0x02fe6f52, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "v101_u26.u26",					0x0200000, 0x23faec02, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(theglad101, theglad101, pgm)
STD_ROM_FN(theglad101)

struct BurnDriver BurnDrvTheglad101 = {
	"theglad101", "theglad", "pgm", NULL, "2003",
	"The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"The Gladiator\0\u795e\u528d\u4f0f\u9b54\u5f55\0\u795e\u528d\u98a8\u96f2\0\u95d8\u5e7b\u72c2 - Road of the Sword\0\uc2e0\uac80\uc758\ud48d\uc6b4 (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, theglad101RomInfo, theglad101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, thegladDIPInfo,
	thegladInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V100, ROM 01/16/03 SHEN JIAN)

/* IGS PCB-0361-02-F0 MADE IN TAIWAN
   IGS PCB-0421-00-F0-A MADE IN TAIWAN 	
   SERIAL N0: 460050
   SCREEN VER PIC: 01/16/03 10:39:25 V100 */

static struct BurnRomInfo theglad100RomDesc[] = {
	{ "u6.rom",						0x0080000, 0x14c85212, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u33",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u2",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u4",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u6",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u11",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u12",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u1",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples

//	{ "theglad_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_execute_only_area",	    0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "theglad_igs027a_older.bin",              0x0003e78, 0x02fe6f52, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "u2.rom",						0x0200000, 0xc7bcf2ae, 8 | BRF_PRG | BRF_ESS },	//  9 External ARM7 Rom
};

STDROMPICKEXT(theglad100, theglad100, pgm)
STD_ROM_FN(theglad100)

static void theglad100Patch()
{
	pgm_decrypt_theglad();
	pgm_create_theglad_EO_data();

	// Hack the jump table in the external rom to work correctly with the internal rom we have...
	static const UINT16 subroutine_addresses[] = {
		0x00FC, 0x00E8, 0x0110, 0x0150, 0x0194, 0x06C8, 0x071C, 0x0728,
		0x0734, 0x0740, 0x0784, 0x0794, 0x07FC, 0x0840, 0x086C, 0x0988,
		0x0A54, 0x0AA8, 0x0AD4, 0x0EB8, 0x0EF8, 0x0F2C, 0x0F3C, 0x0F78,
		0x0FA8, 0x0FD8, 0x1028, 0x1038, 0x1048, 0x1058, 0x1068, 0x1070,
		0x1090, 0x10B0, 0x10D4, 0x1100, 0x113C, 0x1198, 0x1234, 0x1258,
		0x127C, 0x12A8, 0x12E4, 0x1368, 0x142C, 0x0B10, 0x0B54, 0x0B74,
		0x0C08, 0x0C90, 0x0D18, 0x0D90, 0x1570, 0x1600, 0x1640, 0x1694,
		0x1730, 0x176C, 0x17AC, 0x17D8, 0x18C4, 0x18E0, 0x1904, 0x1930,
		0x19D8, 0x1A38, 0x1950, 0x1970, 0x1990, 0x19B8, 0x19C8, 0x1A9C,
		0x1AC4, 0x1AE8, 0x1B20, 0x1B48, 0x1B70, 0x1B8C, 0x1BB4, 0x1BD8,
		0x1BFC, 0x1C10, 0x1C24, 0x1CA0, 0x1D5C, 0x1D7C, 0x1D8C, 0x1DAC,
		0x1DCC, 0x1DE0, 0x1DF4, 0x1E1C, 0x1E2C, 0x1E60, 0x1E94, 0x1EA4,
		0x1ECC, 0x1ED8, 0x1EE4, 0x1F14, 0x1F44, 0x1FB4, 0x1FC4, 0x2040,
		0x20BC, 0x2140, 0x21C4, 0x2240, 0x22BC, 0x2340, 0x23C4, 0x23D0,
		0x2400, 0x2430, 0x244C, 0x245C, 0x246C, 0x2FCC, 0x3000, 0x3028,
		0x3050, 0x30A4, 0x30F8, 0x3120, 0x249C, 0x24C0, 0x27BC, 0x2B40,
		0x2BF4, 0x2CD8, 0x2E2C
	};

	UINT16 *extprot = (UINT16 *)PGMUSER0;

	for (int i = 0; i < 131; i++)
	{
		extprot[((0x82078 + (i * 4)) / 2)] = (subroutine_addresses[i]);
	}
}

static INT32 theglad100Init()
{
	nPgmAsicRegionHackAddress = 0x3316;
	pPgmInitCallback = theglad100Patch;
	pPgmProtCallback = install_protection_asic27a_svg;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x000007c4);

	return nRet;
}

struct BurnDriver BurnDrvtheglada = {
	"theglad100", "theglad", "pgm", NULL, "2003",
	"The Gladiator / Shenjian Fumo Lu / Shenjian Fengyun (M68k label V100) (ARM label V100, ROM 01/16/03 SHEN JIAN)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"The Gladiator\0\u795e\u528d\u4f0f\u9b54\u5f55\0\u795e\u528d\u98a8\u96f2\0\u95d8\u5e7b\u72c2 - Road of the Sword\0\uc2e0\uac80\uc758\ud48d\uc6b4 (M68k label V100) (ARM label V100, ROM 01/16/03 SHEN JIAN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, theglad100RomInfo, theglad100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, theglad100DIPInfo,
	theglad100Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Shanliang San He Yi (Flash 3-in-1) (V102 08/23/04 13:03:26)

/* IGS PCB 0487-00 GX-1 MADE IN TAIWAN
   IGS PCB-0484-01 GX-A MADE IN TAIWAN
   SCREEN VER PIC: CHINA VERSION V102 08/23/04 13:03:26
   ROM TYPES: U3 AND U5 ST M27C160
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo pgm3in1RomDesc[] = {
	{ "v-102cn.u3",					0x0200000, 0x72e06b2b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	
	{ "u5.u5",	 	   				0x0200000, 0xda375a50, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1100.u4", 				0x0800000, 0xe32ce499, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1101.u5", 				0x0800000, 0x4e7568bc, 3 | BRF_GRA },			//  3
	{ "pgm_a1102.u6", 				0x0800000, 0x6da7c143, 3 | BRF_GRA },			//  4
	{ "pgm_a1103.u7", 				0x0800000, 0x0ebebfdc, 3 | BRF_GRA },			//  4
	{ "ext_bit_cg.u20", 			0x0400000, 0xfe314754, 3 | BRF_GRA },			//  6

	{ "pgm_b1100.u8", 				0x0800000, 0xfa53d6f6, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1101.u9", 				0x0800000, 0x001e4c81, 4 | BRF_GRA },			//  8
	{ "ext_bit_map.u21", 			0x0200000, 0xfe31dca6, 4 | BRF_GRA },			//  9

	{ "pgm_m1100.u17", 				0x0200000, 0xfb1515f8, 5 | BRF_SND },			// 10 Samples
	{ "u16.u16", 					0x0800000, 0x714c33e5, 5 | BRF_SND },			// 11 

	{ "igs027a_pgm3in1.asic",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 12 Internal ARM7 Rom
};

STDROMPICKEXT(pgm3in1, pgm3in1, pgm)
STD_ROM_FN(pgm3in1)

static INT32 pgm3in1Init()
{
	nPGMDisableIRQ4 = 1;
	pPgmInitCallback = pgm_decrypt_pgm3in1;
	pPgmProtCallback = install_protection_asic27a_py2k2;
//	pPgmProtCallback = install_protection_asic27a_kovsh;

	return pgmInit();
}

struct BurnDriver BurnDrvPgm3in1 = {
	"pgm3in1", NULL, "pgm", NULL, "2004",
	"Shanliang San He Yi (Flash 3-in-1) (V102 08/23/04 13:03:26)\0", NULL, "IGS", "PolyGameMaster",
	L"\u95ea\u4eae\u4e09\u5408\u4e00\0\u9583\u4eae\u4e09\u5408\u4e00 (Flash 3-in-1) (V102 08/23/04 13:03:26)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, pgm3in1RomInfo, pgm3in1RomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgm3in1DIPInfo,
	pgm3in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Shanliang San He Yi (Flash 3-in-1) (V100 07/13/04 12:09:20)

static struct BurnRomInfo pgm3in1c100RomDesc[] = {
	{ "v-100cn.u3",					0x0200000, 0xa39f59b4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	
	{ "u5.u5",	 	   				0x0200000, 0xda375a50, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1100.u4", 				0x0800000, 0xe32ce499, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1101.u5", 				0x0800000, 0x4e7568bc, 3 | BRF_GRA },			//  3
	{ "pgm_a1102.u6", 				0x0800000, 0x6da7c143, 3 | BRF_GRA },			//  4
	{ "pgm_a1103.u7", 				0x0800000, 0x0ebebfdc, 3 | BRF_GRA },			//  4
	{ "ext_bit_cg.u20", 			0x0400000, 0xfe314754, 3 | BRF_GRA },			//  6

	{ "pgm_b1100.u8", 				0x0800000, 0xfa53d6f6, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1101.u9", 				0x0800000, 0x001e4c81, 4 | BRF_GRA },			//  8
	{ "ext_bit_map.u21", 			0x0200000, 0xfe31dca6, 4 | BRF_GRA },			//  9

	{ "pgm_m1100.u17", 				0x0200000, 0xfb1515f8, 5 | BRF_SND },			// 10 Samples
	{ "u16.u16", 					0x0800000, 0x714c33e5, 5 | BRF_SND },			// 11 

	{ "igs027a_pgm3in1.asic",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 12 Internal ARM7 Rom
};

STDROMPICKEXT(pgm3in1c100, pgm3in1c100, pgm)
STD_ROM_FN(pgm3in1c100)

struct BurnDriver BurnDrvPgm3in1c100 = {
	"pgm3in1c100", "pgm3in1", "pgm", NULL, "2004",
	"Shanliang San He Yi (Flash 3-in-1) (V100 07/13/04 12:09:20)\0", NULL, "IGS", "PolyGameMaster",
	L"\u95ea\u4eae\u4e09\u5408\u4e00\0\u9583\u4eae\u4e09\u5408\u4e00 (Flash 3-in-1) (V100 07/13/04 12:09:20)\0", NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_PUZZLE, 0,
	NULL, pgm3in1c100RomInfo, pgm3in1c100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgm3in1DIPInfo,
	pgm3in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V101, ARM ver. V102CN)
// All ROMs labels are on stickers

/* TEST OK F0089535 BY IGS
   SCREEN VER PIC: CHINA - M68K VERSION 101 03/16/04 14:29:17 V102
   ROM TYPES: U5 ST M27C4002
   ROM TYPES: U26 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo happy6RomDesc[] = {
	{ "s101xx_u5.u5",				0x0080000, 0xaa4646e3, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "t01w64m_u29.u29",   			0x0800000, 0x2d3feb8b, 2 | BRF_GRA },			//  1 Tile data

	{ "a01w64m_u5.u5",	  			0x0800000, 0xbbaa3df3, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a02w64m_u6.u6",	  			0x0800000, 0xf8c9cd36, 3 | BRF_GRA },			//  3

	{ "b01w64m_u19.u19", 			0x0800000, 0x73f5f225, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "w01w64m_u17.u17",			0x0800000, 0x7e23e2be, 5 | BRF_SND },			//  5 Samples

//	{ "happy6_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "happy6_igs027a_execute_only_area",	0x0000188, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, 	//  6 Internal ARM7 Rom
	{ "happy6_igs027a_v100_china.bin",		0x0003e78, 0xed530445, 7 | BRF_PRG | BRF_ESS }, 	//  7

	{ "v-102cn_u26.u26",   			0x0400000, 0x310510fb, 8 | BRF_PRG | BRF_ESS },	// 8 External ARM7 Rom
};

STDROMPICKEXT(happy6, happy6, pgm)
STD_ROM_FN(happy6)

static void happy6Patch()
{
	PGMUSER0[0x411de] = 0xc0;
	pgm_decrypt_happy6();
	pgm_create_theglad_EO_data();
	pgm_descramble_happy6_data(PGMSPRMaskROM,         0x800000);
	pgm_descramble_happy6_data(ICSSNDROM + 0x0400000, 0x800000);
}

static INT32 happy6in1Init()
{
	pPgmInitCallback = happy6Patch;
	pPgmProtCallback = install_protection_asic27a_svg;
	pPgmTileDecryptCallback = pgm_descramble_happy6_data;
	pPgmColorDataDecryptcallback = pgm_descramble_happy6_data;

	nPgmAsicRegionHackAddress = 0x3586;

	INT32 nRet = pgmInit();
	
	if (nRet == 0)
	{
		SekOpen(0);
		for (INT32 i = 0x180000; i < 0x500000; i+= 0x80000) {
			SekMapMemory(PGM68KROM, i, i + 0x7ffff, MAP_ROM); // mirrors
		}
		SekClose();

		Arm7SetIdleLoopAddress(0x00000a08);
	}

	return nRet;
}

struct BurnDriver BurnDrvhappy6 = {
	"happy6", NULL, "pgm", NULL, "2004",
	"Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V101, ARM ver. V102CN)\0", NULL, "IGS", "PolyGameMaster",
	L"\u6b22\u4e50\u516d\u5408\u4e00\0\u6b61\u6a02\u516d\u5408\u4e00 (Happy 6-in-1) (M68K ver. V101, ARM ver. V102CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_CASINO, 0,
	NULL, happy6RomInfo, happy6RomName, NULL, NULL, NULL, NULL, pgmInputInfo, happy6DIPInfo,
	happy6in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100, ARM ver. V101CN)
// All ROMs labels are on stickers

/* IGS PCB-0464-010 -GT MADE IN TAIWAN
   IGS PCB-0468-01-01 GT-A MADE IN TAIWAN 
   ROM TYPES: U5 ST M27C4002
   ROM TYPES: U26 ST M27C322
   CREDIT TO: "GC8TECH.COM" */

static struct BurnRomInfo happy6101RomDesc[] = {
	{ "v100cn_u5.u5",				0x0080000, 0xa25418e8, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "t01w64m_u29.u29",   			0x0800000, 0x2d3feb8b, 2 | BRF_GRA },			//  1 Tile data

	{ "a01w64m_u5.u5",	  			0x0800000, 0xbbaa3df3, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a02w64m_u6.u6",	  			0x0800000, 0xf8c9cd36, 3 | BRF_GRA },			//  3

	{ "b01w64m_u19.u19", 			0x0800000, 0x73f5f225, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "w01w64m_u17.u17",			0x0800000, 0x7e23e2be, 5 | BRF_SND },			//  5 Samples

//	{ "happy6_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "happy6_igs027a_execute_only_area",	0x0000188, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, 	//  6 Internal ARM7 Rom
	{ "happy6_igs027a_v100_china.bin",		0x0003e78, 0xed530445, 7 | BRF_PRG | BRF_ESS }, 	//  7

	{ "v101cn_u26.u26",				0x0400000, 0x4a48ca1c, 8 | BRF_PRG | BRF_ESS },	// 8 External ARM7 Rom
};

STDROMPICKEXT(happy6101, happy6101, pgm)
STD_ROM_FN(happy6101)

struct BurnDriver BurnDrvhappy6101 = {
	"happy6101", "happy6", "pgm", NULL, "2004",
	"Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100, ARM ver. V101CN)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"\u6b22\u4e50\u516d\u5408\u4e00\0\u6b61\u6a02\u516d\u5408\u4e00 (Happy 6-in-1) (M68K ver. V100, ARM ver. V101CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_CASINO, 0,
	NULL, happy6101RomInfo, happy6101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, happy6DIPInfo,
	happy6in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100HK, ARM ver. V100HK)
// All ROMs labels are on stickers

/* IGS PCB-0464-01-GT MADE IN TAIWAN
   SCREEN VER PIC: HONGKONG - M68K VERSION V100 12/22/03 11:28:36 V100
   ROM TYPES: U5 MX MX27C4096DC
   ROM TYPES: U26 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo happy6100hkRomDesc[] = {
	{ "v100hk_u5.u5",				0x0080000, 0xa25418e8, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "t01w64m_u29.u29",   			0x0800000, 0x2d3feb8b, 2 | BRF_GRA },			//  1 Tile data

	{ "a01w64m_u5.u5",	  			0x0800000, 0xbbaa3df3, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a02w64m_u6.u6",	  			0x0800000, 0xf8c9cd36, 3 | BRF_GRA },			//  3

	{ "b01w64m_u19.u19", 			0x0800000, 0x73f5f225, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "w01w64m_u17.u17",			0x0800000, 0x7e23e2be, 5 | BRF_SND },			//  5 Samples

//	{ "happy6_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "happy6_igs027a_execute_only_area",	0x0000188, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, 	//  6 Internal ARM7 Rom
	{ "happy6_igs027a_v100_china.bin",	0x0003e78, 0xed530445, 7 | BRF_PRG | BRF_ESS }, 	//  7

	{ "v100hk_u26.u26",				0x0400000, 0x8f2feb1f, 8 | BRF_PRG | BRF_ESS },	// 8 External ARM7 Rom
};

STDROMPICKEXT(happy6100hk, happy6100hk, pgm)
STD_ROM_FN(happy6100hk)

struct BurnDriver BurnDrvhappy6100hk = {
	"happy6100hk", "happy6", "pgm", NULL, "2004",
	"Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100HK, ARM ver. V100HK)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"\u6b22\u4e50\u516d\u5408\u4e00\0\u6b61\u6a02\u516d\u5408\u4e00 (Happy 6-in-1) (M68K ver. V100HK, ARM ver. V100HK)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_CASINO, 0,
	NULL, happy6100hkRomInfo, happy6100hkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, happy6hkDIPInfo,
	happy6in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100, ARM ver. V100CN)
// All ROMs labels are on stickers

/* IGS PCB-0464-01-GT MADE IN TAIWAN
   SCREEN VER PIC: CHINA - M68K VERSION V100 12/22/03 11:28:36 V100
   ROM TYPES: U5 MX MX27C4096DC
   ROM TYPES: U26 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo happy6100cnRomDesc[] = {
	{ "v100cn_u5.u5",				0x0080000, 0xa25418e8, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "t01w64m_u29.u29",   			0x0800000, 0x2d3feb8b, 2 | BRF_GRA },			//  1 Tile data

	{ "a01w64m_u5.u5",	  			0x0800000, 0xbbaa3df3, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a02w64m_u6.u6",	  			0x0800000, 0xf8c9cd36, 3 | BRF_GRA },			//  3

	{ "b01w64m_u19.u19", 			0x0800000, 0x73f5f225, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "w01w64m_u17.u17",			0x0800000, 0x7e23e2be, 5 | BRF_SND },			//  5 Samples

//	{ "happy6_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "happy6_igs027a_execute_only_area",	0x0000188, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, 	//  6 Internal ARM7 Rom
	{ "happy6_igs027a_v100_china.bin",	0x0003e78, 0xed530445, 7 | BRF_PRG | BRF_ESS }, 	//  7

	{ "v100cn_u26.u26",				0x0400000, 0x9c29e482, 8 | BRF_PRG | BRF_ESS },	// 8 External ARM7 Rom
};

STDROMPICKEXT(happy6100cn, happy6100cn, pgm)
STD_ROM_FN(happy6100cn)

struct BurnDriver BurnDrvhappy6100cn = {
	"happy6100cn", "happy6", "pgm", NULL, "2004",
	"Huanle Liu He Yi (Happy 6-in-1) (M68K ver. V100, ARM ver. V100CN)\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"\u6b22\u4e50\u516d\u5408\u4e00\0\u6b61\u6a02\u516d\u5408\u4e00 (Happy 6-in-1) (M68K ver. V100, ARM ver. V100CN)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_CASINO, 0,
	NULL, happy6100cnRomInfo, happy6100cnRomName, NULL, NULL, NULL, NULL, pgmInputInfo, happy6DIPInfo,
	happy6in1Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 101)

static struct BurnRomInfo kovshpRomDesc[] = {
	{ "p0600h_101.rom",				0x0400000, 0xe1d89a19, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u2",    			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "pgm_a0602.u6",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "igs_a05401w064.u8",	   		0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05401w064.u11",	   		0x0800000, 0x60999757, 4 | BRF_GRA },			//  7

	{ "pgm_m0600.u5",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovshp, kovshp, pgm)
STD_ROM_FN(kovshp)

static void kovshp_asic_patch()
{
	*((UINT16*)(PGMARMROM + 0x2892)) = BURN_ENDIAN_SWAP_INT16(0x0101);
	*((UINT16*)(PGMARMROM + 0x289e)) = BURN_ENDIAN_SWAP_INT16(0x0107);
	*((UINT16*)(PGMARMROM + 0x28a4)) = BURN_ENDIAN_SWAP_INT16(0x0108);
	*((UINT16*)(PGMARMROM + 0x28a8)) = BURN_ENDIAN_SWAP_INT16(0x0101);
	*((UINT16*)(PGMARMROM + 0x2bf2)) = BURN_ENDIAN_SWAP_INT16(0x4810);
	*((UINT16*)(PGMARMROM + 0x2bf4)) = BURN_ENDIAN_SWAP_INT16(0x800e);
	*((UINT16*)(PGMARMROM + 0x2c92)) = BURN_ENDIAN_SWAP_INT16(0x400f);
	*((UINT16*)(PGMARMROM + 0x2ce0)) = BURN_ENDIAN_SWAP_INT16(0x6c1e);
	*((UINT16*)(PGMARMROM + 0x2ce2)) = BURN_ENDIAN_SWAP_INT16(0x0048);

	for (INT32 i = 0x2ce8; i < 0x2e48; i+=8) { // fix z80 data offsets (offset - 0x9e000)
		UINT16 d = ((PGMARMROM[i+4] << 8) + (PGMARMROM[i+7] << 0)) - 0x09e0;

		PGMARMROM[i+4] = d >> 8;
		PGMARMROM[i+7] = d & 0xff;
	};
}

static void kovshpCallback()
{
	pgm_decrypt_kovshp();
	kovshp_asic_patch();
}

static INT32 kovshpInit()
{
	pPgmInitCallback = kovshpCallback; // pgm_decrypt_kovshp;
	pPgmProtCallback = install_protection_asic27a_kovshp;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriver BurnDrvkovshp = {
	"kovshp", NULL, "pgm", NULL, "2004",
	"Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 101)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus\0\u4e09\u56fd\u6218\u7eaa \u4e71\u4e16\u67ad\u96c4\0\u4e09\u570b\u6230\u7d00 \u4e82\u4e16\u689f\u96c4 (ver. 101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshpRomInfo, kovshpRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 101, Chinese Board)

/* IGS PCB-0490-00-GZ
   IGS PCB-0491-00-GZ-A 
   SCREEN VER PIC: V101 CHINA
   ROM TYPES: U10 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kovshp101RomDesc[] = {
	{ "v-101cn.u10",	    		0x0400000, 0xe1d89a19, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	
	{ "s&k_fntcg_u3.u3",    		0x0400000, 0x164b3c94, 2 | BRF_GRA },			//  1 Tile data
	{ "s&k_fntcg_u7.u7",    		0x0400000, 0xb1fae5e8, 2 | BRF_GRA },			//  2

	{ "s&k_b-cg_u22.u22",			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "s&k_b-cg_u23.u23",			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  4 
	{ "s&k_b-cg_u24.u24",			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  5
	{ "s&k_b-cg_u25.u25",			0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  6

	{ "s&k_b-map_u15.u15",			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "s&k_b-map_u16.u16",			0x0800000, 0x60999757, 4 | BRF_GRA },			//  8

	{ "s&k_wave_u1.u1",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovshp101, kovshp101, pgm)
STD_ROM_FN(kovshp101)

struct BurnDriver BurnDrvkovshp101 = {
	"kovshp101", "kovshp", "pgm", NULL, "2004",
	"Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 101, Chinese Board)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus\0\u4e09\u56fd\u6218\u7eaa \u4e71\u4e16\u67ad\u96c4\0\u4e09\u570b\u6230\u7d00 \u4e82\u4e16\u689f\u96c4 (ver. 101, Chinese Board)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshp101RomInfo, kovshp101RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovshp101DIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


/* Knights of Valour Super Heroes Plus / Sangoku Senki Super Heroes Plus (V101 alt, China)

   IGS PCB-0490-00-GZ
   IGS PCB-0568-00-GZ-B 
   SCREEN VER PIC: V101 CHINA
   ROM TYPES: U10 ST M27C322
   CREDITS TO: "GC8TECH.COM"

static struct BurnRomInfo kovshp101aRomDesc[] = {
	{ "s__s_m-101cn.u10",	    	0x0400000, 0xe1d89a19, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	
	{ "s_s_fntcg_u3.u3",    		0x0400000, 0x164b3c94, 2 | BRF_GRA },			//  1 Tile data
	{ "s_s_fntcg_u7.u7",    		0x0400000, 0xb1fae5e8, 2 | BRF_GRA },			//  2

	{ "s_s_b-cg_u22.u22",			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "s_s_b-cg_u23.u23",			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  4 
	{ "s_s_b-cg_u24.u24",			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  5
	{ "s_s_b-cg_u25.u25",			0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  6

	{ "s_s_b-map_u15.u15",			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "s_s_b-map_u16.u16",			0x0800000, 0x60999757, 4 | BRF_GRA },			//  8

	{ "s_s_wave_u1.u1",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom
};
kovshp
STDROMPICKEXT(kovshp101a, kovshp101a, pgm)
STD_ROM_FN(kovshp101a)

struct BurnDriver BurnDrvkovshp101a = {
	"kovshp101a", "kovshp", "pgm", NULL, "2004",
	"Knights of Valour Super Heroes Plus / Sangoku Senki Super Heroes Plus (V101 alr, China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus\0\u4E09\u56FD\u6218\u7EAA\0\u4E71\u4E16\u67AD\u96C4 (V101 alt, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshp101aRomInfo, kovshp101aRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovshp101DIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
}; */


// Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 100)

/* IGS PCB-0490-00-GZ
   IGS PCB-0491-00-GZ-A 
   SCREEN VER PIC: V100 CHINA
   ROM TYPES: U10 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo kovshp100RomDesc[] = {
	{ "s__s_m-100cn.u10",			0x0400000, 0xe251e8e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code */
	
	{ "pgm_t0600.u2",    			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "pgm_a0602.u6",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "igs_a05401w064.u8",	   		0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05401w064.u11",	   		0x0800000, 0x60999757, 4 | BRF_GRA },			//  7

	{ "pgm_m0600.u5",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovshp100, kovshp100, pgm)
STD_ROM_FN(kovshp100)

struct BurnDriver BurnDrvkovshp100 = {
	"kovshp100", "kovshp", "pgm", NULL, "2004",
	"Knights of Valour Super Heroes Plus / Sanguo Zhan Ji Luanshi Xiaoxiong (ver. 100)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus\0\u4e09\u56fd\u6218\u7eaa \u4e71\u4e16\u67ad\u96c4\0\u4e09\u570b\u6230\u7d00 \u4e82\u4e16\u689f\u96c4 (ver. 100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshp100RomInfo, kovshp100RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovshp101DIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (SANGO EX+) (ver. 201 'Yitong Zhongyuan', China)

static struct BurnRomInfo kovytzyRomDesc[] = {
	{ "ytzy_v201cn.rom",			0x0400000, 0xf3705ea0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u2",    			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "pgm_a0602.u6",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "igs_a05401w064.u8",	   		0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05401w064.u11",	   		0x0800000, 0x60999757, 4 | BRF_GRA },			//  7

	{ "pgm_m0600.u5",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovytzy, kovytzy, pgm)
STD_ROM_FN(kovytzy)

static void kovytzysoundpatch()
{
/*
	ips_sound_patch_data [encrypted]

	68krom[0x90faa] = 0x3857;  // 0x754e
	68krom[0x90fd2] = 0x1c5b;  // 0x754e 八阵图
	68krom[0xf6eea] = 0x1895;  // 0x714e
	68krom[0xf6eec] = 0x100d;  // 0x714e
	68krom[0xf6fb4] = 0x14ed;  // 0x714e
*/
	*((UINT16*)(PGM68KROM + 0x90faa)) = BURN_ENDIAN_SWAP_INT16(0x4e75);
//	*((UINT16*)(PGM68KROM + 0x90fd2)) = BURN_ENDIAN_SWAP_INT16(0x4e75);  // 八阵图
	*((UINT16*)(PGM68KROM + 0xf6eea)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
	*((UINT16*)(PGM68KROM + 0xf6eec)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
	*((UINT16*)(PGM68KROM + 0xf6fb4)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
}

static INT32 kovytzyInit()
{
	INT32 nRet = kovshpInit();
	
	if (nRet == 0) kovytzysoundpatch();

	return nRet;
}

struct BurnDriver BurnDrvkovytzy = {
	"kovytzy", NULL, "pgm", NULL, "1999",
	"Knights of Valour Super Heroes / Sanguo Zhan Ji Fengyun Zaiqi / Sangoku Senki Super Heroes (SANGO EX+) (ver. 201 'Yitong Zhongyuan', China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour Super Heroes\0\u4e09\u56fd\u6218\u7eaa \u98ce\u4e91\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 \u98a8\u96f2\u518d\u8d77\0\u4e09\u570b\u6230\u7d00 Super Heroes (SANGO EX+) (ver. 201 '\u4e00\u7edf\u4e2d\u539f', China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovytzyRomInfo, kovytzyRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovytzyInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 205) [Oriental Ex]

static struct BurnRomInfo oldsplusRomDesc[] = {
	{ "v-205cn.u10",				0x0400000, 0x923f7246, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05301w064.u2",			0x0800000, 0x8257bbb0, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05301w064.u3",			0x0800000, 0x57946fd2, 3 | BRF_GRA },	 		//  2 Sprite Color Data
	{ "igs_a05302w064.u4",			0x0800000, 0x3459a0b8, 3 | BRF_GRA },			//  3
	{ "igs_a05303w064.u6",			0x0800000, 0x13475d85, 3 | BRF_GRA },			//  4
	{ "igs_a05304w064.u8",			0x0800000, 0xf03ef7a6, 3 | BRF_GRA },			//  5

	{ "igs_b05301w064.u9",			0x0800000, 0xfd98f503, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05302w064.u11",			0x0800000, 0x9f6094a8, 4 | BRF_GRA },			//  7

	{ "igs_w05301b032.u5",			0x0400000, 0x86ec83bc, 5 | BRF_SND },			//  8 Samples

	{ "oldsplus_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(oldsplus, oldsplus, pgm)
STD_ROM_FN(oldsplus)

static INT32 oldsplusInit()
{
	pPgmInitCallback = pgm_decrypt_oldsplus;
	pPgmProtCallback = install_protection_asic27a_oldsplus; // simulation
//	pPgmProtCallback = install_protection_asic27a_kovsh;

	return pgmInit();
}

struct BurnDriver BurnDrvoldsplus = {
	"oldsplus", NULL, "pgm", NULL, "2004",
	"Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 205) [Oriental Ex]\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"Oriental Legend 2 (Korea)\0\uc190\uc624\uacf5 2\0\u897f\u6e38\u91ca\u5384\u4f20 \u7fa4\u9b54\u4e71\u821e\0\u897f\u904a\u91cb\u5384\u50b3 \u7fa4\u9b54\u4e82\u821e (World, China, Japan, Hong Kong, Taiwan) (ver. 205) [Oriental Ex]\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, oldsplusRomInfo, oldsplusRomName, NULL, NULL, NULL, NULL, pgmInputInfo, puzzli2DIPInfo,
	oldsplusInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 203) [Oriental Ex]

/* IGS PCB-0490-00-G7 MADE IN TAIWAN
   IGS PCB-0491-00-GZ-A MADE IN TAIWAN
   PINK LABEL: 西游释厄传 - 群魔乱舞
   SCREEN VER PIC: V203 CHINA
   ROM TYPES: U10  ST  M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo oldsplus203RomDesc[] = {
	{ "v-203cn.u10",				0x0400000, 0xc728cadc, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05301w064.u2",			0x0800000, 0x8257bbb0, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05301w064.u3",			0x0800000, 0x57946fd2, 3 | BRF_GRA },	 		//  2 Sprite Color Data
	{ "igs_a05302w064.u4",			0x0800000, 0x3459a0b8, 3 | BRF_GRA },			//  3
	{ "igs_a05303w064.u6",			0x0800000, 0x13475d85, 3 | BRF_GRA },			//  4
	{ "igs_a05304w064.u8",			0x0800000, 0xf03ef7a6, 3 | BRF_GRA },			//  5

	{ "igs_b05301w064.u9",			0x0800000, 0xfd98f503, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05302w064.u11",			0x0800000, 0x9f6094a8, 4 | BRF_GRA },			//  7

	{ "igs_w05301b032.u5",			0x0400000, 0x86ec83bc, 5 | BRF_SND },			//  8 Samples

	{ "oldsplus_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(oldsplus203, oldsplus203, pgm)
STD_ROM_FN(oldsplus203)

struct BurnDriver BurnDrvoldsplus203 = {
	"oldsplus203", "oldsplus", "pgm", NULL, "2004",
	"Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 203) [Oriental Ex]\0", "Incomplete Dump", "IGS", "PolyGameMaster",
	L"Oriental Legend 2 (Korea)\0\uc190\uc624\uacf5 2\0\u897f\u6e38\u91ca\u5384\u4f20 \u7fa4\u9b54\u4e71\u821e\0\u897f\u904a\u91cb\u5384\u50b3 \u7fa4\u9b54\u4e82\u821e (World, China, Japan, Hong Kong, Taiwan) (ver. 203) [Oriental Ex]\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, oldsplus203RomInfo, oldsplus203RomName, NULL, NULL, NULL, NULL, pgmInputInfo, puzzli2DIPInfo,
	oldsplusInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade Plus / Ao Jian Kuang Dao Jiaqiang Ban (China, ver. 300)

/* IGS PCB-0361-02-F0 MADE IN TAIWAN
   IGS PCB-0491-00-GZ-A
   傲剑狂刀+ V300XX_U6 on sticker
   傲剑狂刀+ V300XXX_U26 on sticker
   SCREEN VER PIC: V300 CHINA
   ROM TYPES: U26 ST M27C160
   ROM TYPES: U6 ST M27C4002
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo killbldpRomDesc[] = {
	{ "v300xx_u6.u6",   			0x0080000, 0xb7fb8ec9, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "igs_t05701w032.u33", 		0x0400000, 0x567c714f, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05701w064.u3",			0x0800000, 0x8c0c992c, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "igs_a05702w064.u4",			0x0800000, 0x7e5b0f27, 3 | BRF_GRA },			//  4
	{ "igs_a05703w064.u6",			0x0800000, 0xaccbdb44, 3 | BRF_GRA },			//  5
	
	{ "igs_b05701w064.u9",			0x0800000, 0xa20cdcef, 4 | BRF_GRA },		 	//  6 Sprite Masks & Color Indexes
	{ "igs_b05702w016.u11", 		0x0200000, 0xfe7457df, 4 | BRF_GRA },			//  7
	
	{ "igs_w05701b032.u5",			0x0400000, 0x2d3ae593, 5 | BRF_SND },			//  8 Samples

// this is the original rom with the first 0x268 bytes from the bootleg - but it doesn't work?
//	{ "killbldp_igs027a.bin",	    0x0004000, 0xc7868d90, 7 | BRF_PRG },			//  9 Internal ARM7 Rom
// from a bootleg
	{ "killbldp_igs027a_alt.bin",	0x0004000, 0x98316b06, 7 | BRF_PRG },			//  9 Internal ARM7 Rom
	
	{ "v300xx_u26.u26",				0x0200000, 0x144388c8, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(killbldp, killbldp, pgm)
STD_ROM_FN(killbldp)

INT32 killbldpInit()
{
	pPgmInitCallback = pgm_decrypt_killbldp;
	pPgmProtCallback = install_protection_asic27a_svg;
	
	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x00007c4);

	return nRet;
}

struct BurnDriver BurnDrvKillbldp = {
	"killbldp", NULL, "pgm", NULL, "2005",
	"The Killing Blade Plus / Ao Jian Kuang Dao Jiaqiang Ban (China, ver. 300)\0", NULL, "IGS", "PolyGameMaster",
	L"The Killing Blade Plus \0\u50B2\u5251\u72C2\u5200 \u52A0\u5F3A\u7248 (China, ver. 300)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, killbldpRomInfo, killbldpRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	killbldpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V200) (ARM label V200, ROM 10/11/05 S.V.G V201)

/* IGS PCB-0529-02-HH
   IGS PCB-0532-00-HH-A
   SCREEN VER PIC: M68K VERSION V200 10/11/05 10:07:20 V201 CHINA 
   ROM TYPES: U30 MX MX27C4096DC
   ROM TYPES: U26 ST M27C322
   ROM TYPES: U36 ST M27C322
   CREDITS TO: "GC8TECH.COM" */

static struct BurnRomInfo svgRomDesc[] = {
	{ "svg_v200_u30.u30",			0x0080000, 0x34c18f3f, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05601w016.u29",			0x0200000, 0x03e110dc, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05601w064.u3",			0x0800000, 0xea6453e4, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a05602w064.u4",			0x0800000, 0x6d00621b, 3 | BRF_GRA },			//  3
	{ "igs_a05603w064.u6",			0x0800000, 0x7b71c64f, 3 | BRF_GRA },			//  4
	{ "igs_a05604w032.u8",			0x0400000, 0x9452a567, 3 | BRF_GRA },			//  5

	{ "igs_b05601w064.u9",			0x0800000, 0x35c0a489, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05602w064.u11",			0x0800000, 0x8aad3f85, 4 | BRF_GRA },			//  7

	{ "igs_w05601b064.u5",			0x0800000, 0xbfe61a71, 5 | BRF_SND },			//  8 Samples
	{ "igs_w05602b032.u7",			0x0400000, 0x0685166d, 5 | BRF_SND },			//  9

//	{ "svg_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svg_igs027a_execute_only_area", 	0x0000188, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svg_igs027a_v200_china.bin", 	0x0003e78, 0x72b73169, 7 | BRF_PRG | BRF_ESS },		// 11 Internal ARM7 Rom

    // eproms with no labels stickers
	{ "epr.u26",					0x0400000, 0x46826ec8, 8 | BRF_PRG | BRF_ESS },	// 12 External ARM7 Rom
	{ "epr.u36",					0x0400000, 0xfa5f3901, 8 | BRF_PRG | BRF_ESS },	// 13
};

STDROMPICKEXT(svg, svg, pgm)
STD_ROM_FN(svg)

static void pgm_create_svg_EO_data()
{
	// Replace undumpable area of the arm7 internal rom with a custom-built
	// version created by David Haywood in order to make game playable
	static UINT16 svgEOHackData[0x188/2] = {
		0x000a, 0xea00, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xf000, 0xe59f,
		0x0038, 0x0800, 0x0038, 0x0800, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0x00d2, 0xe3a0, 0xf000, 0xe121, 0x4001, 0xe3a0, 0x4b06, 0xe284,
		0x0cfa, 0xe3a0, 0xd804, 0xe080, 0x00d1, 0xe3a0, 0xf000, 0xe121,
		0x0cf6, 0xe3a0, 0xd804, 0xe080, 0x00d7, 0xe3a0, 0xf000, 0xe121,
		0x0cff, 0xe3a0, 0xd804, 0xe080, 0x00db, 0xe3a0, 0xf000, 0xe121,
		0x4140, 0xe1c4, 0x0cfe, 0xe3a0, 0xd804, 0xe080, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0x4a01, 0xe3a0, 0x0b01, 0xe3a0, 0xd804, 0xe080,
		0x5a0f, 0xe3a0, 0x0008, 0xe3a0, 0x8805, 0xe080, 0x0010, 0xe3a0,
		0x0000, 0xe5c8, 0x7805, 0xe1a0, 0x6a01, 0xe3a0, 0x0012, 0xe3a0,
		0x0a02, 0xe280, 0x6806, 0xe080, 0x6000, 0xe587, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0x4001, 0xe3a0, 0x4b06, 0xe284, 0x0cf2, 0xe3a0,
		0xd804, 0xe080, 0x0013, 0xe3a0, 0xf000, 0xe121, 0x0028, 0xea00,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xe004, 0xe52d, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0xe004, 0xe49d, 0xff1e, 0xe12f, 0xe004, 0xe52d,
		0x0013, 0xe3a0, 0xf000, 0xe121, 0xe004, 0xe49d, 0xff1e, 0xe12f,
		0x00d1, 0xe3a0, 0xf000, 0xe121, 0xd0b8, 0xe59f, 0x00d3, 0xe3a0,
		0xf000, 0xe121, 0xd0b0, 0xe59f, 0x10b8, 0xe59f, 0x0000, 0xe3a0,
		0x0000, 0xe581, 0xf0b0, 0xe59f, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xff1e, 0xe12f, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff, 0xfffe, 0xeaff,
		0xfffe, 0xeaff, 0x105c, 0xe59f
	};

	for (int i = 0; i < 0x188/2; i++) {
		svgEOHackData[i] = BURN_ENDIAN_SWAP_INT16(svgEOHackData[i]);
	}

	memcpy(PGMARMROM, svgEOHackData, 0x188);
}

static void svgPatch()
{
	pgm_decrypt_svg();
	pgm_create_svg_EO_data();
}

static INT32 svgInit()
{
	pPgmInitCallback = svgPatch;
	pPgmProtCallback = install_protection_asic27a_svg;

	nPgmAsicRegionHackAddress = 0x3c3e;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x0000B90);

	return nRet;
}

struct BurnDriver BurnDrvSvg = {
	"svg", NULL, "pgm", NULL, "2005",
	"S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V200) (ARM label V200, ROM 10/11/05 S.V.G V201)\0", NULL, "IGS / Idea Factory", "PolyGameMaster",
	L"S.V.G. - Spectral vs Generation\0\u5723\u9b54\u4e16\u7eaa\0\u8056\u9b54\u4e16\u7d00 (M68k label V200) (ARM label V200, ROM 10/11/05 S.V.G V201)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, svgRomInfo, svgRomName, NULL, NULL, NULL, NULL, pgmInputInfo, svgDIPInfo,
	svgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V101TW) (ARM label V101TW, ROM 06/20/05 S.V.G V100)

static struct BurnRomInfo svgtwRomDesc[] = {
	{ "v101tw_u30.u30",				0x0080000, 0x8d0405e4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05601w016.u29",			0x0200000, 0x03e110dc, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05601w064.u3",			0x0800000, 0xea6453e4, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a05602w064.u4",			0x0800000, 0x6d00621b, 3 | BRF_GRA },			//  3
	{ "igs_a05603w064.u6",			0x0800000, 0x7b71c64f, 3 | BRF_GRA },			//  4
	{ "igs_a05604w032.u8",			0x0400000, 0x9452a567, 3 | BRF_GRA },			//  5

	{ "igs_b05601w064.u9",			0x0800000, 0x35c0a489, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05602w064.u11",			0x0800000, 0x8aad3f85, 4 | BRF_GRA },			//  7

	{ "igs_w05601b064.u5",			0x0800000, 0xbfe61a71, 5 | BRF_SND },			//  8 Samples
	{ "igs_w05602b032.u7",			0x0400000, 0x0685166d, 5 | BRF_SND },			//  9

//	{ "svg_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgpcb_igs027a_execute_only_area", 	0x0000188, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgcpb_igs027a_v100_japan.bin", 	0x0003e78, 0x7a59da5d, 7 | BRF_PRG | BRF_ESS },		// 11 Internal ARM7 Rom

	{ "v101tw_u26.u26",				0x0400000, 0xcc24f542, 8 | BRF_PRG | BRF_ESS },	// 12 External ARM7 Rom
	{ "v101tw_u36.u36",				0x0400000, 0xf18283e2, 8 | BRF_PRG | BRF_ESS },	// 13
};

STDROMPICKEXT(svgtw, svgtw, pgm)
STD_ROM_FN(svgtw)

static void svgtwPatch()
{
	pgm_decrypt_svgpcb();
	pgm_create_theglad_EO_data();
}

static INT32 svgtwInit()
{
	pPgmInitCallback = svgtwPatch;
	pPgmProtCallback = install_protection_asic27a_svg;

	nPgmAsicRegionHackAddress = 0x3a8e;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x00009e0);

	return nRet;
}

struct BurnDriverD BurnDrvSvgtw = {
	"svgtw", "svg", "pgm", NULL, "2005",
	"S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V101TW) (ARM label V101TW, ROM 06/20/05 S.V.G V100)\0", "Incomplete dump", "IGS / Idea Factory", "PolyGameMaster",
	L"S.V.G. - Spectral vs Generation\0\u5723\u9b54\u4e16\u7eaa\0\u8056\u9b54\u4e16\u7d00 (M68k label V101TW) (ARM label V101TW, ROM 06/20/05 S.V.G V100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, svgtwRomInfo, svgtwRomName, NULL, NULL, NULL, NULL, pgmInputInfo, svgtwDIPInfo,
	svgtwInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V101HK) (ARM label V101HK, ROM 06/20/05 S.V.G V100)

/* IGS PCB-0529-02-HH
   IGS PCB-0532-00-HH-A
   SCREEN VER PIC: M68K VERSION V100 06/20/05 11:36:15 V100 
   ROM TYPES: U30 MX MX27C4096DC
   ROM TYPES: U26 ST M27C322
   ROM TYPES: U36 ST M27C322
   CREDITS TO: "XeD" */
   
static struct BurnRomInfo svghkRomDesc[] = {
	{ "v101hk_u30.u30",				0x0080000, 0x8d0405e4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05601w016.u29",			0x0200000, 0x03e110dc, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05601w064.u3",			0x0800000, 0xea6453e4, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a05602w064.u4",			0x0800000, 0x6d00621b, 3 | BRF_GRA },			//  3
	{ "igs_a05603w064.u6",			0x0800000, 0x7b71c64f, 3 | BRF_GRA },			//  4
	{ "igs_a05604w032.u8",			0x0400000, 0x9452a567, 3 | BRF_GRA },			//  5

	{ "igs_b05601w064.u9",			0x0800000, 0x35c0a489, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05602w064.u11",			0x0800000, 0x8aad3f85, 4 | BRF_GRA },			//  7

	{ "igs_w05601b064.u5",			0x0800000, 0xbfe61a71, 5 | BRF_SND },			//  8 Samples
	{ "igs_w05602b032.u7",			0x0400000, 0x0685166d, 5 | BRF_SND },			//  9

//	{ "svg_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgpcb_igs027a_execute_only_area", 	0x0000188, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgcpb_igs027a_v100_japan.bin", 	0x0003e78, 0x7a59da5d, 7 | BRF_PRG | BRF_ESS },		// 11 Internal ARM7 Rom

	{ "v101hk_u26.u26",				0x0400000, 0x42b0d5a9, 8 | BRF_PRG | BRF_ESS },	// 12 External ARM7 Rom
	{ "v101hk_u36.u36",				0x0400000, 0xbf15a47a, 8 | BRF_PRG | BRF_ESS },	// 13
};

STDROMPICKEXT(svghk, svghk, pgm)
STD_ROM_FN(svghk)

struct BurnDriver BurnDrvSvghk = {
	"svghk", "svg", "pgm", NULL, "2005",
	"S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V101HK) (ARM label V101HK, ROM 06/20/05 S.V.G V100)\0", "Incomplete dump", "IGS / Idea Factory", "PolyGameMaster",
	L"S.V.G. - Spectral vs Generation\0\u5723\u9b54\u4e16\u7eaa\0\u8056\u9b54\u4e16\u7d00 (M68k label V101HK) (ARM label V101HK, ROM 06/20/05 S.V.G V100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, svghkRomInfo, svghkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, svghkDIPInfo,
	svgtwInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// -----------------------------------------------------------------------------
// PCB Versions


// DoDonPachi III (World, 2002.05.15 Master Ver)

static struct BurnRomInfo ddp3RomDesc[] = {
	{ "ddp3_v101_16m.u36",			0x0200000, 0xfba2180e, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
	
	{ "ddp3_defaults.nv",			0x0020000, 0x571e96c0, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ddp3, ddp3, ddp3Bios) // custom bios
STD_ROM_FN(ddp3)

static INT32 ddp3Init()
{
	pPgmInitCallback = pgm_decrypt_py2k2;
	pPgmProtCallback = install_protection_asic27a_ddp3; // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvDdp3 = {
	"ddp3", NULL, NULL,  NULL, "2002",
	"DoDonPachi III (World, 2002.05.15 Master Ver)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi III\0\u6012\u9996\u9818\u8702\u2162 (World, 2002.05.15 Master Ver)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3RomInfo, ddp3RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05.Master Ver, 68k Label V101)

static struct BurnRomInfo ddp3aRomDesc[] = {
	{ "ddp3_v101.u36",				0x0200000, 0x195b5c1e, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
	
	{ "ddp3_defaults.nv",			0x0020000, 0x571e96c0, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ddp3a, ddp3a, ddp3Bios) // custom bios
STD_ROM_FN(ddp3a)

struct BurnDriver BurnDrvDdp3a = {
	"ddpdoj", "ddp3", NULL,  NULL, "2002",
	"DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05.Master Ver, 68k Label V101)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f (Japan, 2002.04.05.Master Ver, 68k Label V101)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3aRomInfo, ddp3aRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05 Master Ver, location test)

static struct BurnRomInfo ddpdojpRomDesc[] = {
	{ "pgmbios.u20.27c210",			0x0020000, 0x1d2a7c15, 0 | BRF_PRG | BRF_ESS }, //  0 68K BIOS (pgm location test)
	{ "ca008.cod_prom.u13.27c322",	0x0400000, 0x2ba7fa3b, 1 | BRF_PRG | BRF_ESS }, //  1 68K Code

	{ "fnt_cg.0_cgrom0.u8.27c322",	0x0400000, 0x1805e198, 2 | BRF_GRA },           //  2 Tile data
	{ "fnt_cg.1_cgrom1.u10.27c322",	0x0400000, 0xf06ce99c, 2 | BRF_GRA },           //  3

	{ "sp_cg.0_imcs0.u11.27c322",	0x0400000, 0xc66bdd8e, 3 | BRF_GRA },           //  4 Sprite Color Data
	{ "sp_cg.1_imcs1.u13.27c322",	0x0400000, 0xd30eac89, 3 | BRF_GRA },           //  5
	{ "sp_cg.2_imcs2.u15.27c322",	0x0400000, 0xf31b010e, 3 | BRF_GRA },           //  6
	{ "sp_cg.3_imcs3.u17.27c322",	0x0400000, 0x01ec23f5, 3 | BRF_GRA },           //  7

	{ "sp_mp.0_bitcs0.u5.27c322",	0x0400000, 0xe30494a4, 4 | BRF_GRA },           //  8 Sprite Masks & Color Indexes
	{ "sp_mp.1_bitcs1.u6.27c322",	0x0400000, 0x0239daec, 4 | BRF_GRA },           //  9

	{ "sndmask.rom_mcs1.u3.27c322",	0x0400000, 0x5a0dbd76, 5 | BRF_SND },           // 10 Samples

	{ "ddp3_defaults.nv",			0x0020000, 0x571e96c0, 0xA | BRF_PRG },         // 11 NV RAM
};

STDROMPICKEXT(ddpdojp, ddpdojp, ddp3Bios) // custom bios
STD_ROM_FN(ddpdojp)

static INT32 ddpdojpInit()
{
	INT32 nRet = pgmInit();

	if (!nRet) {
		BurnLoadRom(PGM68KBIOS, 0, 1);	// load custom 68k bios
	}

	return nRet;
}

struct BurnDriver BurnDrvDdpdojp = {
	"ddpdojp", "ddp3", NULL, NULL, "2002",
	"DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05 Master Ver, location test)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f (Japan, 2002.04.05 Master Ver, location test)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IGS_PGM, GBF_VERSHOOT, 0,
	NULL, ddpdojpRomInfo, ddpdojpRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddpdojpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05.Master Ver, 68k Label V100)

static struct BurnRomInfo ddp3bRomDesc[] = {
	{ "ddp3_d_d_1_0.u36",			0x0200000, 0x5d3f85ba, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ddp3_defaults.nv",			0x0020000, 0x571e96c0, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ddp3b, ddp3b, ddp3Bios) // custom bios
STD_ROM_FN(ddp3b)

struct BurnDriver BurnDrvDdp3b = {
	"ddpdoja", "ddp3", NULL,  NULL, "2002",
	"DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05.Master Ver, 68k Label V100)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f (Japan, 2002.04.05.Master Ver, 68k Label V100)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3bRomInfo, ddp3bRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05 Master Ver)

static struct BurnRomInfo ddp3cRomDesc[] = {
	{ "dd_v100.u36",				0x0200000, 0x7da0c1e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom
	
	{ "ddp3_defaults.nv",			0x0020000, 0x571e96c0, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ddp3c, ddp3c, ddp3Bios) // custom bios
STD_ROM_FN(ddp3c)

struct BurnDriver BurnDrvDdp3c = {
	"ddpdojb", "ddp3", NULL,  NULL, "2002",
	"DoDonPachi Dai-Ou-Jou (Japan, 2002.04.05 Master Ver)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f (Japan, 2002.04.05 Master Ver)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3cRomInfo, ddp3cRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07.Black Ver, newer)
// latest revision, BL version at 2nd ROM half was updated, require different NVRAM protection values, still display same 3-dot version text as older set

static struct BurnRomInfo ddp3blkRomDesc[] = {
	{ "ddb10_10_8_434f.u45",		0x0200000, 0xd21561db, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom

	{ "ddp3blk_defaults.nv",		0x0020000, 0xc2282720, 0xA | BRF_PRG },			//  7 NV RAM (patch below instead)
};

STDROMPICKEXT(ddp3blk, ddp3blk, ddp3Bios) // custom bios
STD_ROM_FN(ddp3blk)

static void ddp3blkPatchRAM()
{
	SekOpen(0);
	SekWriteLong(0x803800, 0x36982136);
	SekWriteLong(0x803804, 0x76349621);
	SekClose();

	// enable asic test
//	*((UINT16*)(PGM68KROM + 0x03c0f4)) = BURN_ENDIAN_SWAP_INT16(0x0012);
}

static INT32 ddp3blkInit()
{
	pPgmInitCallback = pgm_decrypt_py2k2;
	pPgmProtCallback = install_protection_asic27a_ddp3; // simulation

	INT32 nRet = pgmInit();

	if (nRet == 0) {
		ddp3blkPatchRAM();
	}

	return nRet;
}

struct BurnDriver BurnDrvDdp3blk = {
	"ddpdojblk", "ddp3", NULL, NULL, "2002",
	"DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07.Black Ver, newer)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou Black Label\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f Black Label (Japan, 2002.10.07.Black Ver, newer)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3blkRomInfo, ddp3blkRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3blkInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07.Black Ver, older)

static struct BurnRomInfo ddp3blkaRomDesc[] = {
	{ "ddb_1dot.u45",				0x0200000, 0x265f26cd, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom

	{ "ddp3blk_defaults.nv",		0x0020000, 0xa1651904, 0xA | BRF_PRG },			//  7 NV RAM (patch below instead)
};

STDROMPICKEXT(ddp3blka, ddp3blka, ddp3Bios) // custom bios
STD_ROM_FN(ddp3blka)

static void ddp3blkaPatchRAM()
{
	SekOpen(0);
	SekWriteLong(0x803800, 0x95804803);
	SekWriteLong(0x803804, 0x23879065);
	SekClose();

	// enable asic test
//	*((UINT16*)(PGM68KROM + 0x03c0f4)) = BURN_ENDIAN_SWAP_INT16(0x0012);
}

static INT32 ddp3blkaInit()
{
	pPgmInitCallback = pgm_decrypt_py2k2;
	pPgmProtCallback = install_protection_asic27a_ddp3; // simulation

	INT32 nRet = pgmInit();

	if (nRet == 0) {
		ddp3blkaPatchRAM();
	}

	return nRet;
}

struct BurnDriver BurnDrvDdp3blka = {
	"ddpdojblka", "ddp3", NULL, NULL, "2002",
	"DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07.Black Ver, older)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou Black Label\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f Black Label (Japan, 2002.10.07.Black Ver, older)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3blkaRomInfo, ddp3blkaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3blkaInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07 Black Ver)

static struct BurnRomInfo ddp3blkbRomDesc[] = {
	{ "ddb10.u45",					0x0200000, 0x72b35510, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04401w064.u19",		0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04401w064.u7",			0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04402w064.u8",			0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "cave_b04401w064.u1",			0x0800000, 0x17731c9d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04401b032.u17",		0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "ddp3_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  6 Internal ARM7 Rom

	{ "ddp3blk_defaults.nv",		0x0020000, 0xa1651904, 0xA | BRF_PRG },			//  7 NV RAM (patch below instead)
};

STDROMPICKEXT(ddp3blkb, ddp3blkb, ddp3Bios) // custom bios
STD_ROM_FN(ddp3blkb)

struct BurnDriver BurnDrvDdp3blkb = {
	"ddpdojblkb", "ddp3", NULL, NULL, "2002",
	"DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07 Black Ver)\0", NULL, "Cave (AMI license)", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou Black Label\0\u6012\u9996\u9818\u8702 \u5927\u5f80\u751f Black Label (Japan, 2002.10.07 Black Ver)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddp3blkbRomInfo, ddp3blkbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp3blkaInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver.)

static struct BurnRomInfo ketRomDesc[] = {
	{ "ketsui_v100.u38",			0x0200000, 0xdfe62f3b, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ket, ket, ketsuiBios) // custom bios
STD_ROM_FN(ket)

static INT32 ketsuiInit()
{
	pPgmInitCallback = pgm_decrypt_ketsui;
	pPgmProtCallback = install_protection_asic27a_ketsui;  // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvKet = {
	"ket", NULL, NULL, NULL, "2002",
	"Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver.)\0", NULL, "Cave (AMI license)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2003/01/01. Master Ver.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketRomInfo, ketRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver.) (alt rom fill)

static struct BurnRomInfo ket1RomDesc[] = {
	// alt version with 0xff fill in the unused area after 0x1443bc rather than random data, there are no code changes.
	{ "ketsui_v100_alt_fill.u38",	0x0200000, 0xe140f8a4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	
	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ket1, ket1, ketsuiBios) // custom bios
STD_ROM_FN(ket1)

struct BurnDriver BurnDrvKet1 = {
	"ket1", "ket", NULL, NULL, "2002",
	"Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver.) (alt rom fill)\0", NULL, "Cave (AMI license)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2003/01/01. Master Ver.) (alt rom fill)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ket1RomInfo, ket1RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2003/01/01 Master Ver.)

static struct BurnRomInfo ketaRomDesc[] = {
	{ "ketsui_prg_revised.bin",		0x0200000, 0x69fcf5eb, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(keta, keta, ketsuiBios) // custom bios
STD_ROM_FN(keta)

struct BurnDriver BurnDrvKeta = {
	"keta", "ket", NULL, NULL, "2002",
	"Ketsui: Kizuna Jigoku Tachi (2003/01/01 Master Ver.)\0", NULL, "Cave (AMI license)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2003/01/01 Master Ver.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketaRomInfo, ketaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2003/01/01 Master Ver)

static struct BurnRomInfo ketbRomDesc[] = {
	{ "ketsui_prg_original.bin",	0x0200000, 0xcca5e153, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketb, ketb, ketsuiBios) // custom bios
STD_ROM_FN(ketb)

struct BurnDriver BurnDrvKetb = {
	"ketb", "ket", NULL, NULL, "2002",
	"Ketsui: Kizuna Jigoku Tachi (2003/01/01 Master Ver)\0", NULL, "Cave (AMI license)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2003/01/01 Master Ver)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketbRomInfo, ketbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2014/07/16 ARRANGE 1.7 VER) (hack)

static struct BurnRomInfo ketarrRomDesc[] = {
	{ "ketarr17_v100.u38",			0x0200000, 0x2cb80b89, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom 
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarr, ketarr, ketsuiBios) // custom bios
STD_ROM_FN(ketarr)

struct BurnDriver BurnDrvketarr = {
	"ketarr", "ket", NULL, NULL, "2014",
	"Ketsui: Kizuna Jigoku Tachi (2014/07/16 ARRANGE 1.7 VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2014/07/16 ARRANGE 1.7 VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarrRomInfo, ketarrRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/04/17 ARRANGE VER) (hack)

static struct BurnRomInfo ketarr10RomDesc[] = {
	{ "ketarr_v100.u38",			0x0200000, 0xd4c7a8ab, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarr10, ketarr10, ketsuiBios) // custom bios
STD_ROM_FN(ketarr10)

struct BurnDriver BurnDrvketarr10 = {
	"ketarr10", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/04/17 ARRANGE VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/04/17 ARRANGE VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarr10RomInfo, ketarr10RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/06/26 ARRANGE 1.5 VER) (hack)

static struct BurnRomInfo ketarr15RomDesc[] = {
	{ "ketarr15_v100.u38",			0x0200000, 0x552a7d95, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarr15, ketarr15, ketsuiBios) // custom bios
STD_ROM_FN(ketarr15)

struct BurnDriver BurnDrvketarr15 = {
	"ketarr15", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/06/26 ARRANGE 1.5 VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/06/26 ARRANGE 1.5 VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarr15RomInfo, ketarr15RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/06/26 ARRANGE 1.51 VER) (hack)

static struct BurnRomInfo ketarr151RomDesc[] = {
	{ "ketarr151_v100.u38",			0x0200000, 0x2b7c030d, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarr151, ketarr151, ketsuiBios) // custom bios
STD_ROM_FN(ketarr151)

struct BurnDriver BurnDrvketarr151 = {
	"ketarr151", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/06/26 ARRANGE 1.51 VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/06/26 ARRANGE 1.51 VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarr151RomInfo, ketarr151RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/06/27 MR.STOIC 1.5 VER) (hack)

static struct BurnRomInfo ketarrs15RomDesc[] = {
	{ "ketarrs15_v100.u38",			0x0200000, 0xa95e71e0, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarrs15, ketarrs15, ketsuiBios) // custom bios
STD_ROM_FN(ketarrs15)

struct BurnDriver BurnDrvketarrs15 = {
	"ketarrs15", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/06/27 MR.STOIC 1.5 VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/06/27 MR.STOIC 1.5 VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarrs15RomInfo, ketarrs15RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/06/27 MR.STOIC 1.51 VER) (hack)

static struct BurnRomInfo ketarrs151RomDesc[] = {
	{ "ketarrs151_v100.u38",		0x0200000, 0x35c984e4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarrs151, ketarrs151, ketsuiBios) // custom bios
STD_ROM_FN(ketarrs151)

struct BurnDriver BurnDrvketarrs151 = {
	"ketarrs151", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/06/27 MR.STOIC 1.51 VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/06/27 MR.STOIC 1.51 VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarrs151RomInfo, ketarrs151RomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2012/04/17 FAST. VER) (hack)

static struct BurnRomInfo ketarrfRomDesc[] = {
	{ "ketarrf_v100.u38",			0x0200000, 0x6ad17aa4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "cave_t04701w064.u19", 		0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04701w064.u7", 		0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04702w064.u8", 		0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "cave_b04701w064.u1",			0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_m04701b032.u17",		0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ket_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
	
	{ "ket_defaults.nv",			0x0020000, 0x3ca892d8, 0xA | BRF_PRG },			//  7 NV RAM
};

STDROMPICKEXT(ketarrf, ketarrf, ketsuiBios) // custom bios
STD_ROM_FN(ketarrf)

struct BurnDriver BurnDrvketarrf = {
	"ketarrf", "ket", NULL, NULL, "2012",
	"Ketsui: Kizuna Jigoku Tachi (2012/04/17 FAST. VER) (hack)\0", NULL, "hack (trap15)", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2012/04/17 FAST. VER) (hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HACK | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, ketarrfRomInfo, ketarrfRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketsuiInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Espgaluda (2003/10/15 Master Ver)

static struct BurnRomInfo espgalRomDesc[] = {
	{ "espgaluda_v100.u38",			0x0200000, 0x08ecec34, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "cave_t04801w064.u19",	   	0x0800000, 0x6021c79e, 2 | BRF_GRA },			//  1 Tile data

	{ "cave_a04801w064.u7",			0x0800000, 0x26dd4932, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "cave_a04802w064.u8",	   		0x0800000, 0x0e6bf7a9, 3 | BRF_GRA },			//  3

	{ "cave_b04801w064.u1",	 		0x0800000, 0x98dce13a, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "cave_w04801b032.u17",		0x0400000, 0x60298536, 5 | BRF_SND },			//  5 Samples

	{ "espgal_igs027a.bin",			0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  6 Internal ARM7 Rom
};

STDROMPICKEXT(espgal, espgal, espgalBios) // custom bios
STD_ROM_FN(espgal)

static INT32 espgalInit()
{
	pPgmInitCallback = pgm_decrypt_espgaluda;
	pPgmProtCallback = install_protection_asic27a_ketsui; // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvEspgal = {
	"espgal", NULL, NULL, NULL, "2003",
	"Espgaluda (2003/10/15 Master Ver)\0", NULL, "Cave (AMI license)", "PolyGameMaster based",
	L"Espgaluda\0\u30A8\u30B9\u30D7\u30AC\u30EB\u30FC\u30C0 (2003/10/15 Master Ver)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_VERSHOOT, 0,
	NULL, espgalRomInfo, espgalRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	espgalInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Tougenkyou - Road of the Sword (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN) (Japan, JAMMA PCB)

/* SCREEN VER PIC: 03/13/03 14:06:44 V101 JAPAN 
   ROM TYPES: U29,U62 ST M27C160
   ROM TYPES: U43 MX27C4096
   ROM TYPES: U42 TI TMS27C210A
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo thegladpcbRomDesc[] = {
	{ "v100_u43.u43",				0x0080000, 0xbcf3b172, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u71",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u30",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u31",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u32",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u40",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u41",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u8",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples
	// these contain samples for the japan region
	{ "wave_u29.u29",     			0x0200000, 0x51acb395, 5 | BRF_SND },			//  8

//	{ "thegladpcb_igs027a.bin",		 0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "thegladpcb_igs027a_execute_only_area",   0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "thegladpcb_igs027a_v100_japan.bin",      0x0003e78, 0xd7f06e2d, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "v101_u62.u62",	   			0x0200000, 0x23faec02, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(thegladpcb, thegladpcb, thegladBIOS) // custom bios
STD_ROM_FN(thegladpcb)

struct BurnDriver BurnDrvThegladpcb = {
	"thegladpcb", "theglad", "pgm", NULL, "2003",
	"Tougenkyou - Road of the Sword (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN) (Japan, JAMMA PCB)\0", NULL, "IGS (Alta/AMI license)", "PolyGameMaster",
	L"\u95d8\u5e7b\u72c2 - Road of the Sword (M68k label V100) (ARM label V101, ROM 03/13/03 SHEN JIAN) (Japan, JAMMA PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, thegladpcbRomInfo, thegladpcbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, thegladpcbDIPInfo,
	theglad100Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Tougenkyou - Road of the Sword (M68k label V100) (ARM label V100, ROM 02/25/03 SHEN JIAN) (Japan, JAMMA PCB)

/* SCREEN VER PIC: 02/25/03 16:32:21 V100 JAPAN
   ROM TYPES: U29,U62 ST M27C160
   ROM TYPES: U43 MX27C4096
   ROM TYPES: U42 TI TMS27C210A
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo thegladpcbaRomDesc[] = {
	{ "v100_u43.u43",				0x0080000, 0xbcf3b172, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t04601w64m.u71",			0x0800000, 0xe5dab371, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a04601w64m.u30",			0x0800000, 0xd9b2e004, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a04602w64m.u31",			0x0800000, 0x14f22308, 3 | BRF_GRA },			//  3
	{ "igs_a04603w64m.u32",			0x0800000, 0x8f621e17, 3 | BRF_GRA },			//  4

	{ "igs_b04601w64m.u40",			0x0800000, 0xee72bccf, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes
	{ "igs_b04602w32m.u41",			0x0400000, 0x7dba9c38, 4 | BRF_GRA },			//  6

	{ "igs_w04601b64m.u8",			0x0800000, 0x5f15ddb3, 5 | BRF_SND },			//  7 Samples
	// these contain samples for the japan region
	{ "wave_u29.u29",     			0x0200000, 0x51acb395, 5 | BRF_SND },			//  8

//	{ "thegladpcb_igs027a.bin",		 0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "thegladpcb_igs027a_execute_only_area",   0x0000188, 0x00000000, 0 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom
	{ "thegladpcb_igs027a_v100_japan.bin",      0x0003e78, 0xd7f06e2d, 7 | BRF_PRG | BRF_ESS },  //  9

	{ "v100_u62.u62",	   			0x0200000, 0x0f3f511e, 8 | BRF_PRG | BRF_ESS },	// 10 External ARM7 Rom
};

STDROMPICKEXT(thegladpcba, thegladpcba, thegladBIOS) // custom bios
STD_ROM_FN(thegladpcba)

struct BurnDriver BurnDrvThegladpcba = {
	"thegladpcba", "theglad", "pgm", NULL, "2003",
	"Tougenkyou - Road of the Sword (M68k label V100) (ARM label V100, ROM 02/25/03 SHEN JIAN) (Japan, JAMMA PCB)\0", NULL, "IGS (Alta/AMI license)", "PolyGameMaster",
	L"\u95d8\u5e7b\u72c2 - Road of the Sword (M68k label V100) (ARM label V100, ROM 02/25/03 SHEN JIAN) (Japan, JAMMA PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, thegladpcbaRomInfo, thegladpcbaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, thegladpcbDIPInfo,
	theglad100Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Demon Front V1.1 / Moyu Zhanxian V1.1 (68k label V107KR, ROM M107KR 11/03/03) (ARM label V106KR, ROM 10/16/03 S106KR) (JAMMA PCB)

static struct BurnRomInfo dmnfrntpcbRomDesc[] = {
	{ "demon_front_v107kr_u43.u43",	0x0200000, 0x671d8a31, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "igs_t04501w064.u71",			0x0800000, 0x900eaaac, 2 | BRF_GRA },			 //  1 Tile data

	{ "igs_a04501w064.u30",			0x0800000, 0x9741bea6, 3 | BRF_GRA },			 //  2 Sprite Color Data
	{ "igs_a04502w064.u31",			0x0800000, 0xe104f405, 3 | BRF_GRA },			 //  3
	{ "igs_a04503w064.u32",			0x0800000, 0xbfd5cfe3, 3 | BRF_GRA },			 //  4

	{ "igs_b04501w064.u40",			0x0800000, 0x29320b7d, 4 | BRF_GRA },			 //  5 Sprite Masks & Color Indexes
	{ "igs_b04502w016.u41",			0x0200000, 0x578c00e9, 4 | BRF_GRA },			 //  6

	{ "igs_w04501b064.u8",			0x0800000, 0x3ab58137, 5 | BRF_SND },			 //  7 Samples

	{ "dmnfrnt_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom

	{ "demon_front_v107kr_u62.u62",	0x0400000, 0xcb94772e, 8 | BRF_PRG | BRF_ESS },	 //  9 External ARM7 Rom
};

STDROMPICKEXT(dmnfrntpcb, dmnfrntpcb, dmnfrntBIOS) // custom bios
STD_ROM_FN(dmnfrntpcb)

struct BurnDriver BurnDrvDmnfrntpcb = {
	"dmnfrntpcb", "dmnfrnt", "pgm", NULL, "2002",
	"Demon Front V1.1 / Moyu Zhanxian V1.1 (68k label V107KR, ROM M107KR 11/03/03) (ARM label V106KR, ROM 10/16/03 S106KR) (JAMMA PCB)\0", NULL, "IGS", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_RUNGUN, 0,
	NULL, dmnfrntpcbRomInfo, dmnfrntpcbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, dmnfrntpcbDIPInfo,
	dmnfrntInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Demon Front (VM105XX, S105XX, China, Single PCB Version)

/* IGS PCB-0454-00-GQ
   PCB PIC: NO EPROM STICKERS!
   SCREEN VER PIC: VM105XX 08/05/02 10:24:11
   ROM TYPES: U62 ST M27C322
   ROM TYPES: U43 ST M27C160
   CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo dmnfrntpcbaRomDesc[] = {
	{ "v105_u43.u43",				0x0200000, 0xbda083bd, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "igs_t04501w064.u71",			0x0800000, 0x900eaaac, 2 | BRF_GRA },			 //  1 Tile data

	{ "igs_a04501w064.u30",			0x0800000, 0x9741bea6, 3 | BRF_GRA },			 //  2 Sprite Color Data
	{ "igs_a04502w064.u31",			0x0800000, 0xe104f405, 3 | BRF_GRA },			 //  3
	{ "igs_a04503w064.u32",			0x0800000, 0xbfd5cfe3, 3 | BRF_GRA },			 //  4

	{ "igs_b04501w064.u40",			0x0800000, 0x29320b7d, 4 | BRF_GRA },			 //  5 Sprite Masks & Color Indexes
	{ "igs_b04502w016.u41",			0x0200000, 0x578c00e9, 4 | BRF_GRA },			 //  6

	{ "igs_w04501b064.u8",			0x0800000, 0x3ab58137, 5 | BRF_SND },			 //  7 Samples

	{ "dmnfrnt_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  8 Internal ARM7 Rom

	{ "v105_u62.u62",				0x0400000, 0xc798c2ef, 8 | BRF_PRG | BRF_ESS },	 //  9 External ARM7 Rom
};

STDROMPICKEXT(dmnfrntpcba, dmnfrntpcba, dmnfrntBIOS) // custom bios
STD_ROM_FN(dmnfrntpcba)

struct BurnDriver BurnDrvDmnfrntpcba = {
	"dmnfrntpcba", "dmnfrnt", "pgm", NULL, "2002",
	"Demon Front (VM105XX, S105XX, China, Single PCB Version)\0", NULL, "IGS", "PolyGameMaster",
	L"Demon Front\0\u9B54\u57DF\u6218\u7EBF\0\u9B54\u57DF\u6230\u7DDA (VM105XX, S105XX, China, Single PCB Version)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_RUNGUN, 0,
	NULL, dmnfrntpcbaRomInfo, dmnfrntpcbaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, dmnfrntpcbaDIPInfo,
	dmnfrntInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V100JP) (ARM label V100JP, ROM 05/12/05 S.V.G V100) (Japan, JAMMA PCB)

/*SCREEN VER PIC: M68K VERSION V100 05/12/05 15:31/35 V100 JAPAN
  ROM TYPES: U49 TI TMS27C210A
  ROM TYPES: U50 MX MX27C4096
  ROM TYPES: U64,U65 ST M27C322
  CREDITS TO: "GC8TECH.COM"*/

static struct BurnRomInfo svgpcbRomDesc[] = {
	{ "svg_v100jp_u50.u50",			0x0080000, 0x8d0405e4, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05601w016.u70",			0x0200000, 0x03e110dc, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05601w064.u44",			0x0800000, 0xea6453e4, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "igs_a05602w064.u45",			0x0800000, 0x6d00621b, 3 | BRF_GRA },			//  3
	{ "igs_a05603w064.u46",			0x0800000, 0x7b71c64f, 3 | BRF_GRA },			//  4
	{ "igs_a05604w032.u47",			0x0400000, 0x9452a567, 3 | BRF_GRA },			//  5

	{ "igs_b05601w064.u61",			0x0800000, 0x35c0a489, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05602w064.u62",			0x0800000, 0x8aad3f85, 4 | BRF_GRA },			//  7

	{ "igs_w05601b064.u30",			0x0800000, 0xbfe61a71, 5 | BRF_SND },			//  8 Samples
	{ "igs_w05602b032.u31",			0x0400000, 0x0685166d, 5 | BRF_SND },			//  9

//	{ "svgpcb_igs027a.bin",		    0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgpcb_igs027a_execute_only_area", 0x0000188, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },	// 10 Internal ARM7 Rom
	{ "svgcpb_igs027a_v100_japan.bin",    0x0003e78, 0x7a59da5d, 7 | BRF_PRG | BRF_ESS },	// 11 Internal ARM7 Rom

	{ "svg_v100jp_u64.u64",			0x0400000, 0x399d4a8b, 8 | BRF_PRG | BRF_ESS },	// 12 External ARM7 Rom
	{ "svg_v100jp_u65.u65",			0x0400000, 0x6e1c33b1, 8 | BRF_PRG | BRF_ESS },	// 13
};

STDROMPICKEXT(svgpcb, svgpcb, svgpcbBIOS) // custom bios
STD_ROM_FN(svgpcb)

static void svgpcbPatch()
{
	pgm_decrypt_svgpcb();
	pgm_create_theglad_EO_data();
}

static INT32 svgpcbInit()
{
	pPgmInitCallback = svgpcbPatch;
	pPgmProtCallback = install_protection_asic27a_svg;

	nPgmAsicRegionHackAddress = 0x3a8e;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x00009e0);

	return nRet;
}

struct BurnDriver BurnDrvSvgpcb = {
	"svgpcb", "svg", "pgm", NULL, "2005",
	"S.V.G. - Spectral vs Generation / Sheng Mo Shiji (M68k label V100JP) (ARM label V100JP, ROM 05/12/05 S.V.G V100) (Japan, JAMMA PCB)\0", NULL, "IGS / Idea Factory (AMI license)", "PolyGameMaster",
	L"S.V.G. - Spectral vs Generation\0\u30b9\u30da\u30af\u30c8\u30e9\u30eb vs \u30b8\u30a7\u30cd\u30ec\u30fc\u30b7\u30e7\u30f3 (M68k label V100JP) (ARM label V100JP, ROM 05/12/05 S.V.G V100) (Japan, JAMMA PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, svgpcbRomInfo, svgpcbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, thegladpcbDIPInfo,
	svgpcbInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// -----------------------------------------------------------------------------
// Homebrew


// Chaos (PGM Demo)

static struct BurnRomInfo pgmdemoRomDesc[] = {
	{ "p0103.rom",     				0x0200000, 0xd3f6ec45, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0100.rom",     				0x0400000, 0x0596a59a, 2 | BRF_GRA },			//  1 Tile data

	{ "a0100.rom",     				0x0400000, 0x5d0e8fa1, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "b0100.rom",     				0x0400000, 0x15dd191f, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes

	{ "m0100.rom",     				0x0200000, 0x8d89877e, 5 | BRF_SND },			//  4 Samples
};

STDROMPICKEXT(pgmdemo, pgmdemo, pgm)
STD_ROM_FN(pgmdemo)

struct BurnDriver BurnDrvPgmdemo = {
	"pgmdemo", NULL, "pgm", NULL, "2005",
	"Chaos (PGM Demo)\0", "Demo Game", "Raster", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO, 4, HARDWARE_IGS_PGM, GBF_MISC, 0,
	NULL, pgmdemoRomInfo, pgmdemoRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	pgmInit,pgmExit,pgmFrame,pgmDraw,NULL, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Frog Feast (PGM)

static struct BurnRomInfo pgmfrogRomDesc[] = {
	{ "p0103.rom",    				0x0200000, 0xcdec9e8d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0100.rom",     				0x0400000, 0x8f58b6d8, 2 | BRF_GRA },			//  1 Tile data

	{ "a0100.rom",     				0x0400000, 0xdc1eafe6, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "b0100.rom",     				0x0400000, 0x3d44b66f, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes

	{ "m0100.rom",     				0x0200000, 0x05e2f761, 5 | BRF_SND },			//  4 Samples
};

STDROMPICKEXT(pgmfrog, pgmfrog, pgm)
STD_ROM_FN(pgmfrog)

struct BurnDriver BurnDrvPgmfrog = {
	"pgmfrog", NULL, "pgm", NULL, "2006",
	"Frog Feast (PGM)\0", NULL, "RasterSoft", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 4, HARDWARE_IGS_PGM, GBF_ACTION, 0,
	NULL, pgmfrogRomInfo, pgmfrogRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	pgmInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// P-GeMeni (060123)

static struct BurnRomInfo pgemeniRomDesc[] = {
	{ "p0103.rom",     				0x0200000, 0x6cafa56b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0100.rom",     				0x0400000, 0x42b979dd, 2 | BRF_GRA },			//  1 Tile data

	{ "a0100.rom",     				0x0400000, 0x105d7cee, 3 | BRF_GRA },			//  2 Sprite Color Data

	{ "b0100.rom",     				0x0400000, 0xb4127373, 4 | BRF_GRA },			//  3 Sprite Masks & Color Indexes

	{ "m0100.rom",     				0x0200000, 0x8d89877e, 5 | BRF_SND },			//  4 Samples
};

STDROMPICKEXT(pgemeni, pgemeni, pgm)
STD_ROM_FN(pgemeni)

struct BurnDriver BurnDrvPgemeni = {
	"pgemeni", NULL, "pgm", NULL, "2006",
	"P-GeMeni (060123)\0", NULL, "blastar@gmx.net", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 4, HARDWARE_IGS_PGM, GBF_PUZZLE, 0,
	NULL, pgemeniRomInfo, pgemeniRomName, NULL, NULL, NULL, NULL, pgmInputInfo, pgmDIPInfo,
	pgmInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


//-----------------------------------------------------------------------------------------------------------------
// Bootlegs & hacks


// Heroes of the World Special Edition / Gaishi Xingxiong Tebie Ban (V302CN, China)

/*
   QCPASS: 2011-04-28
   SN:  WDF*ZSCS-0459
   VER: V300CN  V302CN
   DATE: DEC 29 2010
   TIME: 16:31:32

   WDF PCB NO-0908-1 2010-04-16 MADE IN CHINA
   IGS PCB NO-0213 2009-09-01 MADE IN TAIWAN
   ROM TYPES: U3 ST M27C322
   CREDITS TO: "GC8TECH.COM" AND IQ_132 */

static struct BurnRomInfo kovgsyxRomDesc[] = {
	{ "igs_m3004.u3",				0x0400000, 0x4018559f, 1 | BRF_PRG | BRF_ESS }, // 0 68K Code

	// none of the other roms aside from the program have been dumped, use parent's
	{ "pgm_t0600.u2",    			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "pgm_a0602.u6",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "igs_a05401w064.u8",	   		0x0800000, 0x4fd3413e, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05401w064.u11",	   		0x0800000, 0x60999757, 4 | BRF_GRA },			//  7

	{ "pgm_m0600.u5",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovgsyx, kovgsyx, pgm)
STD_ROM_FN(kovgsyx)

static INT32 kovgsyxInit()
{
	pPgmInitCallback = pgm_decrypt_kovgsyx;
	pPgmProtCallback = install_protection_asic27a_kovgsyx;
//	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
//	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();
		
	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriverD BurnDrvKovgsyx = {
	"kovgsyx", "kovshp", "pgm", NULL, "2004",
	"Heroes of the World Special Edition / Gaishi Xingxiong Tebie Ban (V302CN, China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Heroes of the World Special Edition\0\u76D6\u4E16\u82F1\u96C4 \u7279\u522B\u7248 (V302CN, China)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovgsyxRomInfo, kovgsyxRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovgsyxInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN, Nov 21 2008 19:03:49)

/* SN: WDF-QHSG-0097
   VER: V300CN  V303CN
   DATE: NOV 21  2008
   TIME: 19:03:49 */
   
static struct BurnRomInfo kovqhsgsRomDesc[] = {
	{ "qhsg_c51.rom",  				0x0400000, 0xe5cbac85, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "lsqh2_t01.rom",      		0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

	{ "qhsg_prot.c51",				0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },	//  7 Internal ARM7 Rom
};

STDROMPICKEXT(kovqhsgs, kovqhsgs, pgm)
STD_ROM_FN(kovqhsgs)

static INT32 kovqhsgsInit()
{
	pPgmInitCallback = pgm_decrypt_kovqhsgs;
	pPgmProtCallback = install_protection_asic27a_kovsh;
	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriver BurnDrvkovqhsgs = {
	"kovqhsgs", "kovsh", "pgm", NULL, "2008",
	"Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN, Nov 21 2008 19:03:49)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u62f3\u7687\u4e09\u56fd\u7279\u522b\u7248 (bootleg of Knights of Valour Super Heroes, V303CN, Nov 21 2008 19:03:49)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovqhsgsRomInfo, kovqhsgsRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovqhsgsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN alt, China)

/* SN: WDF-QHSG-0097
   VER: V300CN  V303CN
   DATE: NOV 21  2008
   TIME: 19:03:49 */
   
static struct BurnRomInfo kovqhsgsaRomDesc[] = {
	{ "qhsga_c51.rom",  			0x0400000, 0x4c5ea070, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "lsqh2_t01.rom",      		0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

	{ "qhsg_prot.c51",				0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },	//  7 Internal ARM7 Rom
};

STDROMPICKEXT(kovqhsgsa, kovqhsgsa, pgm)
STD_ROM_FN(kovqhsgsa)

struct BurnDriver BurnDrvkovqhsgsa = {
	"kovqhsgsa", "kovsh", "pgm", NULL, "1999",
	"Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN alt, China)\0", NULL, "IGS", "PolyGameMaster",
	L"\u62f3\u7687\u4e09\u56fd\u7279\u522b\u7248 (bootleg of Knights of Valour Super Heroes, V303CN alt, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovqhsgsaRomInfo, kovqhsgsaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovqhsgsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN, China, decrypted version)

/* SN: WDF-QHSG-0097
   VER: V300CN  V303CN
   DATE: NOV 21 2008
   TIME: 19:03:49 */
   
static struct BurnRomInfo kovqhsgsdRomDesc[] = {
	{ "de_qhsg_c51.rom",  			0x0400000, 0xe020fbd0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "lsqh2_t01.rom",      		0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "de_qhsg_a01.rom",           	0x2300000, 0x9ff4d569, 3 | BRF_GRA },           //  2 Sprite Color Data

    { "de_qhsg_b01.rom",            0x1000000, 0xd420fd17, 4 | BRF_GRA },           //  3 Sprite Masks & Color Indexes

    { "lsqh2_m01.rom",              0x1000000, 0x01af1b65, 5 | BRF_SND },           //  4 Samples

    { "qhsg_prot.c51",              0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  5 Internal ARM7 Rom
};

STDROMPICKEXT(kovqhsgsd, kovqhsgsd, pgm)
STD_ROM_FN(kovqhsgsd)

static INT32 kovqhsgsdInit()
{
    pPgmProtCallback = install_protection_asic27a_kovsh;
    pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;

    nPgmAsicRegionHackAddress = 0x3f0d;

    INT32 nRet = pgmInit();

    Arm7SetIdleLoopAddress(0x00000260);

    return nRet;
}

struct BurnDriver BurnDrvkovqhsgsd = {
	"kovqhsgsd", "kovsh", "pgm", NULL, "1999",
	"Quanhuang Sanguo Tebie Ban (bootleg of Knights of Valour Super Heroes, V303CN, China, decrypted version)\0", NULL, "IGS", "PolyGameMaster",
	L"\u62f3\u7687\u4e09\u56fd\u7279\u522b\u7248 (bootleg of Knights of Valour Super Heroes, V303CN, China, decrypted version)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovqhsgsdRomInfo, kovqhsgsdRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovqhsgsdInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luanshi Quanhuang 2 (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN)

static struct BurnRomInfo kovlsqh2RomDesc[] = {
	{ "lsqh2_prg.rom",				0x0400000, 0xd71e3d50, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

#if !defined ROM_VERIFY
	{ "lsqh2_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  7 Internal ARM7 Rom
#else
	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  7 Internal ARM7 Rom
#endif
};

STDROMPICKEXT(kovlsqh2, kovlsqh2, pgm)
STD_ROM_FN(kovlsqh2)

static INT32 kovlsqh2Init()
{
	pPgmInitCallback = pgm_decrypt_kovlsqh2;
//	pPgmProtCallback = install_protection_asic27a_kovsh;
	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;

	INT32 nRet = pgmInit();
	
//	Arm7SetIdleLoopAddress(~0);

	return nRet;
}

struct BurnDriverD BurnDrvkovlsqh2 = {
	"kovlsqh2", "kovshp", "pgm", NULL, "200?",
	"Luanshi Quanhuang 2 (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN)\0", "Incomplete dump", "bootleg", "PolyGameMaster",
	L"Knights of Valour: Luan Shi Quan Huang II\0\u4E09\u56FD\u6218\u7EAA\0\u4E71\u4E16\u62F3\u7687II (V200CN, China)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU */, GBF_SCRFIGHT, 0,
	NULL, kovlsqh2RomInfo, kovlsqh2RomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovlsqh2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luanshi Quanhuang (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN)

static struct BurnRomInfo kovlsqhRomDesc[] = {
	{ "lsqh_v200cn.rom",  			0x0400000, 0x9935a27a, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

#if !defined ROM_VERIFY
	{ "lsqh_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  7 Internal ARM7 Rom
#else
	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },	//  7 Internal ARM7 Rom
#endif
};

STDROMPICKEXT(kovlsqh, kovlsqh, pgm)
STD_ROM_FN(kovlsqh)

static INT32 kovlsqhInit()
{
	pPgmInitCallback = pgm_decrypt_kovlsqh2;
//	pPgmProtCallback = install_protection_asic27a_kovshp;
	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;


	INT32 nRet = pgmInit();
	
//	Arm7SetIdleLoopAddress(~0);

	return nRet;
}

struct BurnDriverD BurnDrvkovlsqh = {
	"kovlsqh", "kovshp", "pgm", NULL, "200?",
	"Luanshi Quanhuang (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN)\0", "Incomplete dump", "bootleg", "PolyGameMaster",
	L"Knights of Valour: Luan Shi Quan Huang\0\u4E09\u56FD\u6218\u7EAA\0\u4E71\u4E16\u62F3\u7687 (V200CN, China)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU */, GBF_SCRFIGHT, 0,
	NULL, kovlsqhRomInfo, kovlsqhRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovlsqhInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour: Luan Shi Quan Huang Te Bie Ban / Sangoku Senki: Luan Shi Quan Huang Te Bie Ban (V112CN, China)

// Nov 20, 2008 @ $3ffff0

static struct BurnRomInfo kovlsqhoRomDesc[] = {
	{ "lsqh_v112cn.bin",  			0x0400000, 0x092fcd3d, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

//	these were not dumped
	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

//#if !defined ROM_VERIFY
//	{ "lsqh_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  7 Internal ARM7 Rom
//#else
	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },	//  7 Internal ARM7 Rom
//#endif
};

STDROMPICKEXT(kovlsqho, kovlsqho, pgm)
STD_ROM_FN(kovlsqho)

static INT32 kovlsqhoInit()
{
	pPgmInitCallback = pgm_decrypt_kovlsqho;
	pPgmProtCallback = install_protection_asic27a_kovshp;
	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;

	INT32 nRet = pgmInit();

	if (nRet == 0)
	{
		kovshp_asic_patch();;
		Arm7SetIdleLoopAddress(0x260);
	}

	return nRet;
}

struct BurnDriver BurnDrvkovlsqho = {
	"kovlsqho", "kovshp", "pgm", NULL, "2009",
	"Knights of Valour: Luan Shi Quan Huang Te Bie Ban / Sangoku Senki: Luan Shi Quan Huang Te Bie Ban (V112CN, China)\0", "Incomplete dump", "IGS", "PolyGameMaster",
	L"Knights of Valour: Luan Shi Quan Huang Te Bie Ban\0\u4E09\u56FD\u6218\u7EAA\0\u4E71\u4E16\u62F3\u7687 \u7279\u522B\u7248 (V112CN, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovlsqhoRomInfo, kovlsqhoRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovlsqhoInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luanshi Jie Ba (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 1)

static struct BurnRomInfo kovlsjbRomDesc[] = {
	{ "lsjb_prg.rom",  				0x0400000, 0xadf06b37, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

#if !defined ROM_VERIFY
	{ "lsjb_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  7 Internal ARM7 Rom
#else
	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  7 Internal ARM7 Rom
#endif
};

STDROMPICKEXT(kovlsjb, kovlsjb, pgm)
STD_ROM_FN(kovlsjb)

struct BurnDriverD BurnDrvkovlsjb = {
	"kovlsjb", "kovshp", "pgm", NULL, "200?",
	"Luanshi Jie Ba (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 1)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u4e71\u4e16\u8857\u9738 (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 1)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovlsjbRomInfo, kovlsjbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovlsqh2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luanshi Jie Ba (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 2)

static struct BurnRomInfo kovlsjbaRomDesc[] = {
	{ "lsjba_prg.rom",  			0x0400000, 0x8b42f456, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

#if !defined ROM_VERIFY
	{ "lsjba_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  7 Internal ARM7 Rom
#else
	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },	//  7 Internal ARM7 Rom
#endif
};

STDROMPICKEXT(kovlsjba, kovlsjba, pgm)
STD_ROM_FN(kovlsjba)

struct BurnDriverD BurnDrvkovlsjba = {
	"kovlsjba", "kovshp", "pgm", NULL, "200?",
	"Luanshi Jie Ba (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 2)\0", "Incomplete dump", "bootleg", "PolyGameMaster",
	L"\u4e71\u4e16\u8857\u9738 (bootleg of Knights of Valour Super Heroes Plus, ver. 200CN, set 2)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovlsjbaRomInfo, kovlsjbaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovlsqh2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour: Ao Shi San Guo Plus / Sangoku Senki: Ao Shi San Guo Plus (V315CN, China)

// listed as assgplus in code

static struct BurnRomInfo kovassgplusRomDesc[] = {
	{ "assgplus_v315cn.rom",  		0x0400000, 0xb9885d5c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "lsqh2_t01.rom",				0x1000000, 0xd498d97f, 2 | BRF_GRA },			//  1 Tile data

	{ "lsqh2_a01.rom",				0x1000000, 0x25ae3efd, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "lsqh2_a23.rom",				0x1000000, 0x7a779112, 3 | BRF_GRA },			//  3
	{ "lsqh2_a45.rom",				0x1000000, 0x5d7de052, 3 | BRF_GRA },			//  4

	{ "lsqh2_b01.rom",				0x1000000, 0xdf7ca696, 4 | BRF_GRA },			//  5 Sprite Masks & Color Indexes

	{ "lsqh2_m01.rom",				0x1000000, 0x01af1b65, 5 | BRF_SND },			//  6 Samples

	{ "assg_prot.c51",				0x0010000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	// 7 Internal ARM7 Rom
};

STDROMPICKEXT(kovassgplus, kovassgplus, pgm)
STD_ROM_FN(kovassgplus)

static INT32 kovassgplusInit()
{
	pPgmInitCallback = pgm_decrypt_kovassgplus; // incomplete...
//	pPgmProtCallback = install_protection_asic27a_kovsh;
	pPgmTileDecryptCallback = pgm_decode_kovqhsgs_tile_data;
	pPgmColorDataDecryptcallback = pgm_decode_kovqhsgs_gfx;

	INT32 nRet = pgmInit();
	
//	Arm7SetIdleLoopAddress(~0);

	return nRet;
}

struct BurnDriverD BurnDrvkovassgplus = {
	"kovassgplus", "kovshp", "pgm", NULL, "2008",
	"Knights of Valour: Ao Shi San Guo Plus / Sangoku Senki: Ao Shi San Guo Plus(V315CN, China)\0", "Incomplete dump", "IGS", "PolyGameMaster",
	L"Knights of Valour: Ao Shi San Guo Plus\0\u4E09\u56FD\u6218\u7EAA\0\u50B2\u4E16\u4E09\u56FD Plus (V315CN, China)\0", NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM /* | HARDWARE_IGS_USE_ARM_CPU */, GBF_SCRFIGHT, 0,
	NULL, kovassgplusRomInfo, kovassgplusRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovassgplusInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao Shi San Guo (V202CN, China)

/* VER: V202CN
   DATE: OCT 6 2008
   TIME: 09:59:26 */
   
static struct BurnRomInfo kovassgRomDesc[] = {
	{ "assg_v202cn.rom",  			0x0400000, 0x3b7b627f, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "t0600.rom",     				0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600.rom",	   				0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601.rom",	   				0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "a0602.rom",	   				0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "a0603as.rom",  				0x0800000, 0x7057b37e, 3 | BRF_GRA }, 	        //  5

	{ "b0600.rom",	  				0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "b0601as.rom",  				0x0800000, 0x3784fb49, 4 | BRF_GRA },			//  7

	{ "m0600.rom",	   				0x0400000, 0x3ada4fd6, 5 | BRF_SND },	        //  8 Samples

	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovassg, kovassg, pgm)
STD_ROM_FN(kovassg)

static INT32 kovassgInit()
{
	pPgmInitCallback = kovshp_asic_patch; // hack
	pPgmProtCallback = install_protection_asic27a_kovshp;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();
	
	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriver BurnDrvkovassg = {
	"kovassg", "kovshp", "pgm", NULL, "2008",
	"Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao shi San Guo (V202CN, China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour: Ao Shi San Guo\0\u4E09\u56FD\u6218\u7EAA\0\u50B2\u4E16\u4E09\u56FD (V202CN, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovassgRomInfo, kovassgRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovassgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao Shi San Guo (Encrypted, V202CN, China)
/* VER: V202CN
   DATE: OCT 6 2008
   TIME: 09:59:26 */

static struct BurnRomInfo kovassgeRomDesc[] = {
//	samme data as kovassg, but is encrypted
	{ "aoshisango.bin",				0x0400000, 0x5b14a711, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600.rom",     				0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600.rom",	   				0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601.rom",	   				0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "a0602.rom",	   				0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "a0603as.rom",  				0x0800000, 0x7057b37e, 3 | BRF_GRA }, 	        //  5

	{ "b0600.rom",	  				0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "b0601as.rom",  				0x0800000, 0x3784fb49, 4 | BRF_GRA },			//  7

	{ "m0600.rom",	   				0x0400000, 0x3ada4fd6, 5 | BRF_SND },	        //  8 Samples

	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovassge, kovassge, pgm)
STD_ROM_FN(kovassge)

static INT32 kovassgeInit()
{
	pPgmInitCallback = pgm_decrypt_kovassge;
	pPgmProtCallback = install_protection_asic27a_kovshp;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x00000260);

	kovshp_asic_patch();

	return nRet;
}

struct BurnDriver BurnDrvkovassge = {
	"kovassge", "kovshp", "pgm", NULL, "2008",
	"Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao shi San Guo (Encrypted, V202CN, China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour: Ao Shi San Guo\0\u4E09\u56FD\u6218\u7EAA\0\u50B2\u4E16\u4E09\u56FD (Encrypted, V202CN, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovassgeRomInfo, kovassgeRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovassgeInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao Shi San Guo (Newer, V202CN, China)
/* VER: V202CN
   DATE: OCT 6 2008
   TIME: 09:59:26 */

static struct BurnRomInfo kovassgnRomDesc[] = {
//	different encryption than kovassge and has a small amount of code changes
	{ "aoshisango.bin",				0x0400000, 0xac633e43, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600.rom",     				0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600.rom",	   				0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601.rom",	   				0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "a0602.rom",	   				0x0800000, 0xe7a32959, 3 | BRF_GRA }, 	        //  4
	{ "a0603as.rom",  				0x0800000, 0x7057b37e, 3 | BRF_GRA }, 	        //  5

	{ "b0600.rom",	  				0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "b0601as.rom",  				0x0800000, 0x3784fb49, 4 | BRF_GRA },			//  7

	{ "m0600.rom",	   				0x0400000, 0x3ada4fd6, 5 | BRF_SND },	        //  8 Samples

	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovassgn, kovassgn, pgm)
STD_ROM_FN(kovassgn)

static INT32 kovassgnInit()
{
	pPgmInitCallback = pgm_decrypt_kovassgn;
	pPgmProtCallback = install_protection_asic27a_kovshp;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x00000260);

	kovshp_asic_patch();

	return nRet;
}

struct BurnDriver BurnDrvkovassgn = {
	"kovassgn", "kovshp", "pgm", NULL, "2008",
	"Knights of Valour: Ao Shi San Guo / Sangoku Senki: Ao shi San Guo (Newer, V202CN, China)\0", "Imperfect Protection Emulation", "IGS", "PolyGameMaster",
	L"Knights of Valour: Ao Shi San Guo\0\u4E09\u56FD\u6218\u7EAA\0\u50B2\u4E16\u4E09\u56FD (Newer, V202CN, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovassgnRomInfo, kovassgnRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kov115DIPInfo,
	kovassgnInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 1)

static struct BurnRomInfo kovsgqyzRomDesc[] = {
	{ "pgm_sgqyz.rom", 				0x0400000, 0x18e1eed9, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "qyza0603.rom",  				0x0800000, 0xc8b92220, 3 | BRF_GRA },			//  5

	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "qyzb0601.rom",  				0x0800000, 0x64f55882, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples	
	
	{ "kovsgqyz_igs027a.bin", 		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovsgqyz, kovsgqyz, pgm)
STD_ROM_FN(kovsgqyz)

static INT32 kovsgqyzInit()
{
	pPgmProtCallback = install_protection_asic27_kov; // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvKovsgqyz = {
	"kovsgqyz", "kovplus", "pgm", NULL, "1999",
	"Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 1)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u4e09\u570b\u7fa4\u82f1\u4f20 \u6b63\u5b97 Plus (bootleg of Knights of Valour Plus, set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovsgqyzRomInfo, kovsgqyzRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovsgqyzInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 2)

static struct BurnRomInfo kovsgqyzaRomDesc[] = {
	{ "pgm_sgqyza.rom",				0x0400000, 0x5a30dcb7, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "qyza0603.rom",  				0x0800000, 0xc8b92220, 3 | BRF_GRA },			//  5

	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "qyzb0601.rom",  				0x0800000, 0x64f55882, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples	
	
	{ "kovsgqyza_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovsgqyza, kovsgqyza, pgm)
STD_ROM_FN(kovsgqyza)

struct BurnDriver BurnDrvKovsgqyza = {
	"kovsgqyza", "kovplus", "pgm", NULL, "1999",
	"Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 2)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u4e09\u570b\u7fa4\u82f1\u4f20 \u6b63\u5b97 Plus (bootleg of Knights of Valour Plus, set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovsgqyzaRomInfo, kovsgqyzaRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovsgqyzInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 3)

static struct BurnRomInfo kovsgqyzbRomDesc[] = {
	{ "pgm_sgqyzb.rom",				0x0400000, 0x18b8b9c0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "qyza0603.rom",  				0x0800000, 0xc8b92220, 3 | BRF_GRA },			//  5

	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "qyzb0601.rom",  				0x0800000, 0x64f55882, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kovsgqyzb_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovsgqyzb, kovsgqyzb, pgm)
STD_ROM_FN(kovsgqyzb)

struct BurnDriver BurnDrvKovsgqyzb = {
	"kovsgqyzb", "kovplus", "pgm", NULL, "1999",
	"Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 3)\0", NULL, "bootleg", "PolyGameMaster",
	L"\u4e09\u570b\u7fa4\u82f1\u4f20 \u6b63\u5b97 Plus (bootleg of Knights of Valour Plus, set 3)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovsgqyzbRomInfo, kovsgqyzbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovsgqyzInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 4)

static struct BurnRomInfo kovsgqyzcRomDesc[] = {
	{ "pgm_sgqyzc.rom",				0x0400000, 0x09774a3e, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "qyza0603.rom",  				0x0800000, 0xc8b92220, 3 | BRF_GRA },			//  5

	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "qyzb0601.rom",  				0x0800000, 0x64f55882, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kovsgqyzb_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovsgqyzc, kovsgqyzc, pgm)
STD_ROM_FN(kovsgqyzc)

struct BurnDriver BurnDrvKovsgqyzc = {
	"kovsgqyzc", "kovplus", "pgm", NULL, "1999",
	"Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 4)\0", NULL, "ANX", "PolyGameMaster",
	L"\u4e09\u570b\u7fa4\u82f1\u4f20 \u6b63\u5b97 Plus (bootleg of Knights of Valour Plus, set 4)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovsgqyzcRomInfo, kovsgqyzcRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovsgqyzInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Qunying Zhuan Zhengzong Plus (bootleg of Knights of Valour Plus, set 5)
// Dumped by GC8TECH.COM 2019-04-21

static struct BurnRomInfo kovsgqyzdRomDesc[] = {
	// Only program ROM on chip ST M27c322 dumped directly in an eprom programmer.
	{ "pgm_sgqyzd.rom",				0x0400000, 0xa019e6d2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data
	
	{ "pgm_a0600.u2",     			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",     			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u6",     			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "qyza0603.rom",  				0x0800000, 0xc8b92220, 3 | BRF_GRA },			//  5

	{ "pgm_b0600.u5",     			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "qyzb0601.rom",  				0x0800000, 0x64f55882, 4 | BRF_GRA },			//  7
	
	{ "pgm_m0600.u3",     			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			//  8 Samples
	
	{ "kovsgqyzb_igs027a.bin",		0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },  //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovsgqyzd, kovsgqyzd, pgm)
STD_ROM_FN(kovsgqyzd)

struct BurnDriver BurnDrvKovsgqyzd = {
	"kovsgqyzd", "kovplus", "pgm", NULL, "1999",
	"Knights of Valour: SanGuo QunYingZhuan / Sangoku Senki: SanGuo QunYingZhuan (V119, set 5)\0", NULL, "ANX", "PolyGameMaster",
	L"\u4e09\u570b\u7fa4\u82f1\u4f20 \u6b63\u5b97 Plus (bootleg of Knights of Valour Plus, set 5)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovsgqyzdRomInfo, kovsgqyzdRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovsgqyzInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Sangoku Senki Super Heroes (bootleg, V104, China)

static struct BurnRomInfo kovshbRomDesc[] = {
	{ "u0706.rom",	   				0x0400000, 0x57c75b34, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },			//  3
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },			//  4
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },			//  5
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },			//  6

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  8
	{ "pgm_b0602_v200.u10",			0x0100000, 0x9df77934, 4 | BRF_GRA },			//  9

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china.asic", 		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovshb, kovshb, pgm)
STD_ROM_FN(kovshb)

static INT32 kovshbInit()
{
	pPgmProtCallback = install_protection_asic27a_kovsh;

	nPgmAsicRegionHackAddress = 0x3f0d;

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x00000260);

	return nRet;
}

struct BurnDriver BurnDrvKovshb = {
	"kovshb", "kovsh", "pgm", NULL, "1999",
	"Knights of Valour Superheroes / Sangoku Senki Superheroes (bootleg, V104, China)\0", NULL, "IGS", "PolyGameMaster",
	L"Knights of Valour Superheroes\0\u4E09\u56FD\u6218\u7EAA\0\u98CE\u4E91\u518D\u8D77 (bootleg, V104, China)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshbRomInfo, kovshbRomName, NULL, NULL, NULL, NULL, pgmInputInfo, kovassgDIPInfo,
	kovshbInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07 Black Ver., bootleg Knights of Valour Super Heroes conversion)
// 68k: SANGO EX V100 12/06/99 13:36:04, ARM: China internal ROM

static struct BurnRomInfo ddpdojblkblRomDesc[] = {
	// bootleg on a converted KOVSH cart
	{ "ddp_doj_u1.bin",				0x0400000, 0xeb4ab06a, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "t04401w064.u19",				0x0800000, 0x3a95f19c, 2 | BRF_GRA },			//  1 Tile data

	{ "a04401w064.u7",				0x0800000, 0xed229794, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a04402w064.u8",				0x0800000, 0x752167b0, 3 | BRF_GRA },			//  3

	{ "b04401w064_corrupt.u1",		0x0800000, 0x8cbff066, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "m04401b032.u17",				0x0400000, 0x5a0dbd76, 5 | BRF_SND },			//  5 Samples

	{ "kovsh_v100_china.asic",		0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS }, //  6 Internal ARM7 Rom
};

STDROMPICKEXT(ddpdojblkbl, ddpdojblkbl, pgm) // custom bios
STD_ROM_FN(ddpdojblkbl)

struct BurnDriver BurnDrvDdpdojblkbl = {
	"ddpdojblkbl", "ddp3", NULL, NULL, "2012",
	"DoDonPachi Dai-Ou-Jou Black Label (Japan, 2002.10.07 Black Ver., bootleg Knights of Valour Super Heroes conversion)\0", NULL, "bootleg", "PolyGameMaster",
	L"DoDonPachi Dai-Ou-Jou Black Label\0\u6012\u9996\u9818\u8702 \u5927\u5F80\u751F Black Label (Japan, 2002.10.07 Black Ver., bootleg Knights of Valour Super Heroes conversion)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddpdojblkblRomInfo, ddpdojblkblRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver., bootleg cartridge conversion)

static struct BurnRomInfo ketblRomDesc[] = {
	// this assumes a Dodonpachi 2 Bee Storm cart was used
	{ "ketsui_u1.bin",		   		0x0400000, 0x391767b4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	
	{ "t04701w064.u19", 			0x0800000, 0x2665b041, 2 | BRF_GRA },			//  1 Tile data

	{ "a04701w064.u7", 				0x0800000, 0x5ef1b94b, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a04702w064.u8", 				0x0800000, 0x26d6da7f, 3 | BRF_GRA },			//  3

	{ "b04701w064.u1",				0x0800000, 0x1bec008d, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "m04701b032.u17",				0x0400000, 0xb46e22d1, 5 | BRF_SND },			//  5 Samples

	{ "ddp2_igs027a_japan.bin",		0x0004000, 0x742d34d2, 7 | BRF_PRG | BRF_ESS },	//  6 Internal ARM7 Rom
	
	{ "v100.u23", 	   				0x0020000, 0x06c3dd29, 8 | BRF_PRG | BRF_ESS }, //  7 External ARM7 Rom
};

STDROMPICKEXT(ketbl, ketbl, pgm) // custom bios
STD_ROM_FN(ketbl)

static void ketblCallback()
{
	memcpy(PGM68KROM, PGM68KROM + 0x0200000, 0x200000);
}

static INT32 ketblInit()
{
	pPgmInitCallback = pgm_decrypt_ddp2;
	pPgmProtCallback = install_protection_asic27a_martmast;
	pPgmInitCallback = ketblCallback;

//	nPgmAsicRegionHackAddress = 0x2882; // 2883?

	INT32 nRet = pgmInit();

	Arm7SetIdleLoopAddress(0x8010998);

	return nRet;
}

struct BurnDriver BurnDrvKetbl = {
	"ketbl", "ket", NULL, NULL, "2002",
	"Ketsui: Kizuna Jigoku Tachi (2003/01/01. Master Ver., bootleg cartridge conversion)\0", NULL, "bootleg", "PolyGameMaster based",
	L"Ketsui: Kizuna Jigoku Tachi\0\u30b1\u30c4\u30a4: \u7d46\u5730\u7344\u305f\u3061 (2003/01/01. Master Ver., bootleg cartridge conversion)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, 0,
	NULL, ketblRomInfo, ketblRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ketblInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};


// Espgaluda (2003/10/15 Master Ver, bootleg cartridge conversion)

static struct BurnRomInfo espgalblRomDesc[] = {
	// this assumes a Dodonpachi 2 Bee Storm cart was used
	{ "espgaluda_u8.bin",			0x0400000, 0x6a92dd52, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t04801w064.u19",	   			0x0800000, 0x6021c79e, 2 | BRF_GRA },			//  1 Tile data

	{ "a04801w064.u7",				0x0800000, 0x26dd4932, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a04802w064.u8",	   			0x0800000, 0x0e6bf7a9, 3 | BRF_GRA },			//  3

	{ "b04801w064.u1",	 			0x0800000, 0x98dce13a, 4 | BRF_GRA },			//  4 Sprite Masks & Color Indexes

	{ "w04801b032.u17",				0x0400000, 0x60298536, 5 | BRF_SND },			//  5 Samples

	{ "ddp2_igs027a_japan.bin",		0x0004000, 0x742d34d2, 7 | BRF_PRG | BRF_ESS },	//  6 Internal ARM7 Rom
	
	{ "v100.u23", 	   				0x0020000, 0x06c3dd29, 8 | BRF_PRG | BRF_ESS }, //  7 External ARM7 Rom
};

STDROMPICKEXT(espgalbl, espgalbl, pgm) // custom bios
STD_ROM_FN(espgalbl)

struct BurnDriver BurnDrvEspgalbl = {
	"espgalbl", "espgal", NULL, NULL, "2003",
	"Espgaluda (2003/10/15 Master Ver, bootleg cartridge conversion)\0", NULL, "bootleg", "PolyGameMaster based",
	L"Espgaluda\0\u30A8\u30B9\u30D7\u30AC\u30EB\u30FC\u30C0 (2003/10/15 Master Ver, bootleg cartridge conversion)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_JAMMAPCB | HARDWARE_IGS_USE_ARM_CPU, GBF_VERSHOOT, 0,
	NULL, espgalblRomInfo, espgalblRomName, NULL, NULL, NULL, NULL, pgmInputInfo, jammaDIPInfo,
	ddp2Init, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	224, 448, 3, 4
};

// 西游释厄传 新群魔乱舞(修改版)
// Modified by: 萧鱼儿, 一只狼, wzc123
// More info at: https://www.ppxclub.com/forum.php?mod=viewthread&tid=718724

static struct BurnRomInfo oldsplusnrRomDesc[] = {
	/* v211 - 20240128 */
	{ "v-211cn.u10",			0x0400000, 0x89c0afdf, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "igs_t05301naw064.u2",	0x0800000, 0xbf3db9be, 2 | BRF_GRA },                        //  1 Tile data

	{ "igs_a05301naw064.u3",	0x0800000, 0x0f456049, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "igs_a05302nw064.u4",		0x0800000, 0x4ed9028c, 3 | BRF_GRA },                        //  3
	{ "igs_a05303naw064.u6",	0x0800000, 0x5f75eca2, 3 | BRF_GRA },                        //  4
	{ "igs_a05304naw064.u8",	0x0800000, 0xbce3d08b, 3 | BRF_GRA },                        //  5

	{ "igs_b05301naw064.u9",	0x0800000, 0x1cc7c2b1, 4 | BRF_GRA },                        //  6 Sprite Masks & Color Indexes
	{ "igs_b05302naw064.u11",	0x0800000, 0x25b7a816, 4 | BRF_GRA },                        //  7

	{ "igs_w05301nab032.u5",	0x0400000, 0xb740b810, 5 | BRF_SND },                        //  8 Samples

	{ "oldsplus_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  9 Internal ARM7 Rom

	/* v208 - 20200705 */
	{ "v-208cn.dif",			0x0400000, 0x56ef7c1b, 0 | BRF_PRG | BRF_ESS },              

	{ "igs_t05301w064.dif",		0x0800000, 0x27b8be4b, 0 | BRF_GRA },                        // 11 Tile data

	{ "igs_a05301nw064.dif",	0x0800000, 0x28a6f9f3, 0 | BRF_GRA },                        // 12 Sprite Color Data
	{ "igs_a05303w064.dif",		0x0800000, 0x56e00d62, 0 | BRF_GRA },
	{ "igs_a05304w064.dif",		0x0800000, 0x560f9b68, 0 | BRF_GRA },

	{ "igs_b05301nw064.dif",	0x0800000, 0x1565554b, 0 | BRF_GRA },                        // 15 Sprite Masks & Color Indexes
	{ "igs_b05302nw064.dif",	0x0800000, 0x86924573, 0 | BRF_GRA },

	{ "igs_w05301b032.dif",		0x0400000, 0x20eb7bc6, 0 | BRF_SND },                        // 17 Samples
};

STDROMPICKEXT(oldsplusnr, oldsplusnr, pgm)
STD_ROM_FN(oldsplusnr)

static void oldsplusnrLoadSPRColCallback(UINT8* gfx, INT32)
{
	// gfx is a pointer to the temporary memory where the Sprite Color Data roms are loaded
	// v208
	if (nBurnDrvSubActive) {
		RomDiffPatch(gfx + 0x00000000, 12, 0x800000); // Sprite Color Data
		RomDiffPatch(gfx + 0x01000000, 13, 0x800000);
		RomDiffPatch(gfx + 0x01800000, 14, 0x800000);
	}
}

static void oldsplusnrLoadTileCallback(UINT8* gfx, INT32)
{
	// gfx is a pointer to the (PGMTileROM + 0x180000)
	// v208
	if (nBurnDrvSubActive) RomDiffPatch(gfx, 11, 0x800000); // Tile data
}

static void oldsplusnrLoadRomsCallback()
{
	// v208
	if (nBurnDrvSubActive) {
		RomDiffPatch(PGM68KROM     + 0x000000, 10, 0x400000); // 68K Code
		RomDiffPatch(PGMSPRMaskROM + 0x000000, 15, 0x800000); // Sprite Masks & Color Indexes
		RomDiffPatch(PGMSPRMaskROM + 0x800000, 16, 0x800000);
		RomDiffPatch(ICSSNDROM     + 0x400000, 17, 0x400000); // Samples
	}

	// Decrypt 68k rom
	pgm_decrypt_oldsplus();
}

static INT32 oldsplusnrInit()
{
	nBurnDrvSubActive = (VerSwitcher & 0x01);

	switch (nBurnDrvSubActive) {
		case 0x00:
			pszCustomNameA = "Xiyou Shi E Zhuan Xin Qunmo Luanwu (v211, Hack)\0";
			break;

		case 0x01:
			pszCustomNameA = "Xiyou Shi E Zhuan Xin Qunmo Luanwu (v208, Hack)\0";
			break;
	}

	pPgmColorDataDecryptcallback	= oldsplusnrLoadSPRColCallback;
	pPgmTileDecryptCallback			= oldsplusnrLoadTileCallback;
	pPgmInitCallback				= oldsplusnrLoadRomsCallback;
	pPgmProtCallback				= install_protection_asic27a_oldsplus; // simulation

	return pgmInit();
}

struct BurnDriver BurnDrvoldsplusnr = {
	"oldsplusnr", "oldsplus", "pgm", NULL, "2020-2024",
	"Xiyou Shi E Zhuan Xin Qunmo Luanwu (Hack)\0", "Switchable v211 / v208", "hack", "PolyGameMaster",
	L"\u897f\u6e38\u91ca\u5384\u4f20 \u65b0\u7fa4\u9b54\u4e71\u821e (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, oldsplusnrRomInfo, oldsplusnrRomName, NULL, NULL, NULL, NULL, oldsplusnrInputInfo, oldsplusnrDIPInfo,
	oldsplusnrInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes Plus (The Road to Survival True King Version 2018-02-23, FEITIANXUE)
// 2018-02-23
// This set is known as kovsho89 in HBMAME

static struct BurnRomInfo kovshpqszlRomDesc[] = {
	{ "p0600h_101_po89.rom",		0x0400000, 0x086d100b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600_pw10.rom",    			0x0800000, 0xa919ec0f, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600_po73.rom",	  	 		0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601_po73.rom",	  	 		0x0800000, 0x9c1d1bea, 3 | BRF_GRA }, 	        //  3 
	{ "a0602_po73.rom",	   			0x0800000, 0x7b437e0f, 3 | BRF_GRA }, 	        //  4
	{ "a0540_po89.rom",	  	 		0x0800000, 0xb038e568, 3 | BRF_GRA }, 	        //  5
	{ "a0541_po73.rom",	   			0x0800000, 0x1fa3c921, 3 | BRF_GRA }, 	        //  6

	{ "b0600_po73.rom",	   			0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "b0540_po89.rom",	   			0x0800000, 0x03a1641e, 4 | BRF_GRA },			//  8
	
	{ "m0600_po69.rom",	 	  		0x0400000, 0xec3f335d, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china_po89.asic", 0x0004000, 0x4627f36f, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovshpqszl, kovshpqszl, pgm)
STD_ROM_FN(kovshpqszl)

struct BurnDriver BurnDrvkovshpqszl = {
	"kovshpqszl", "kovshp", "pgm", NULL, "2018",
	"Knights of Valour Super Heroes Plus (The Road to Survival True King, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus (The Road to Survival True King, ver. 500)\0\u4e82\u4e16\u82f1\u96c4 - \u6c42\u751f\u4e4b\u8def (\u7248\u672c 500, \u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshpqszlRomInfo, kovshpqszlRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes Plus (The Road to Survival True King Version 2020-01-03, FEITIANXUE)
// 2020-01-03
// This set is originally known as kovsho119 in HBMAME

static struct BurnRomInfo kovshpqszltwRomDesc[] = {
	{ "p0600h_101_po119.rom",		0x04006F8, 0x4c2be2a2, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "t0600_pw10.rom",    			0x0800000, 0xa919ec0f, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600_po73.rom",	  	 		0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601_po73.rom",	  	 		0x0800000, 0x9c1d1bea, 3 | BRF_GRA }, 	        //  3 
	{ "a0602_po73.rom",	   			0x0800000, 0x7b437e0f, 3 | BRF_GRA }, 	        //  4
	{ "a0540_po89.rom",	  	 		0x0800000, 0xb038e568, 3 | BRF_GRA }, 	        //  5
	{ "a0541_po73.rom",	   			0x0800000, 0x1fa3c921, 3 | BRF_GRA }, 	        //  6

	{ "b0600_po73.rom",	   			0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "b0540_po89.rom",	   			0x0800000, 0x03a1641e, 4 | BRF_GRA },			//  8
	{ "b0601_po77.rom",	  	 		0x0800000, 0x5a9defbb, 4 | BRF_GRA },			//  9

	{ "m0600_po69.rom",	 	  		0x0400000, 0xec3f335d, 5 | BRF_SND },			// 10 Samples

	{ "kovsh_v100_china_po89.asic", 0x0004000, 0x4627f36f, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovshpqszltw, kovshpqszltw, pgm)
STD_ROM_FN(kovshpqszltw)

struct BurnDriver BurnDrvkovshpqszltw = {
	"kovshpqszltw", "kovshp", "pgm", NULL, "2020-01-03",
	"Knights of Valour Super Heroes Plus (The Road to Survival True King Tian Wang, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus (The Road to Survival True King Tian Wang, ver. 500)\0\u4e82\u4e16\u82f1\u96c4 - \u6c42\u751f\u4e4b\u8def\u5929\u738b\u7248 (\u7248\u672c 500, \u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshpqszltwRomInfo, kovshpqszltwRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes Plus (The Best Firepower In 2020, 2020-02-06)
// 2020-02-06
// This set is known as kovsho175 in HBMAME

static struct BurnRomInfo kovshpzqhlRomDesc[] = {
	{ "p0600h_101_po175.rom",		 0x0200000,  0x396b0f14, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p0600h_101_po123.rom",		 0x0200000,  0xb9ad393f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "t0600_pw111.rom",    		 0x0800000,  0xde7fdc0c, 2 | BRF_GRA },			  //  2 Tile data

	{ "a0600_po123.rom",	  	 	 0x2000000, 0x8cee9b01, 3 | BRF_GRA },			  //  3 Sprite Color Data
	
	{ "b0600_po123.rom",	   		 0x1000000, 0x82a43111, 4 | BRF_GRA },			  //  7 Sprite Masks & Color Indexes
		
	{ "pgm_m0600.u5",	   			 0x0400000,  0x3ada4fd6, 5 | BRF_SND },			  //  8 Samples
 
 #if !defined (ROM_VERIFY)
	{ "kovsh_v100_china_po89.asic",  0x0004000,  0x4627f36f, 7 | BRF_PRG | BRF_ESS },  //  9 Internal ARM7 Rom
#else	
	{ "kovsh_v100_china_po123.asic", 0x0004000,  0x39abc15f, 7 | BRF_PRG | BRF_ESS },  //  9 Internal ARM7 Rom
#endif
};

STDROMPICKEXT(kovshpzqhl, kovshpzqhl, pgm)
STD_ROM_FN(kovshpzqhl)

struct BurnDriver BurnDrvkovshpzqhl = {
	"kovshpzqhl", "kovshp", "pgm", NULL, "2020",
	"Knights of Valour Super Heroes Plus (The Best Firepower In 2020, 2020-02-06)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Knights of Valour Super Heroes Plus (The Best Firepower In 2020, 2020-02-06)\0\u4E09\u56FD\u6218\u7EAA - \u4E71\u4E16\u67AD\u96C4 (\u6700\u5F3A\u706B\u529B 2020, 2020-02-06)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshpzqhlRomInfo, kovshpzqhlRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// San Guo Zhan Ji - Luan Shi Ying Xiong
// Modified by Fei Tian Xue
// GOTVG 20141209

static struct BurnRomInfo kovlsyxRomDesc[] = {
	{ "lsyx_p0600h_101.rom",	0x0400000, 0xd24da1f8, 1 | BRF_PRG | BRF_ESS },  //  0 68K Code

	{ "lsyx_t0600.u2",			0x0800000, 0xa919ec0f, 2 | BRF_GRA },            //  1 Tile data

	{ "lsyx_a0600.u3",			0x0800000, 0x47794602, 3 | BRF_GRA },            //  2 Sprite Color Data
	{ "lsyx_a0601.u4",			0x0800000, 0xd94abe4b, 3 | BRF_GRA },            //  3 
	{ "pgm_a0602.u6",			0x0800000, 0xe7a32959, 3 | BRF_GRA },            //  4
	{ "lsyx_a05401w064.u8",		0x0800000, 0x6afd8e35, 3 | BRF_GRA },            //  5
	{ "lsyx_a05411w064.u8",		0x0800000, 0x164de1a8, 3 | BRF_GRA },            //  6

	{ "lsyx_b0600.u9",			0x0800000, 0xf48a3b3d, 4 | BRF_GRA },            //  7 Sprite Masks & Color Indexes
	{ "lsyx_b05401w064.u11",	0x0800000, 0xc3eefa33, 4 | BRF_GRA },            //  8
	
	{ "lsyx_m0600.u5",			0x0400000, 0x459ad15f, 5 | BRF_SND },            //  9 Samples

	{ "lsyx_v100_china.asic",	0x0004000, 0x8598d603, 7 | BRF_PRG | BRF_ESS },  // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovlsyx, kovlsyx, pgm)
STD_ROM_FN(kovlsyx)

static INT32 kovshpmaphackInit()
{
	nPGMMapperHack = 1;

	return  kovshpInit();
}

struct BurnDriver BurnDrvkovlsyx = {
	"kovlsyx", "kovshp", "pgm", NULL, "2014",
	"San Guo Zhan Ji - Luan Shi Ying Xiong\0", "Imperfect Protection Emulation", "hack", "PolyGameMaster",
	L"San Guo Zhan Ji - Luan Shi Ying Xiong\0\u4e09\u56fd\u6218\u7eaa - \u4e71\u4e16\u82f1\u96c4\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovlsyxRomInfo, kovlsyxRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpmaphackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luan Shi Ying Xiong - Qiu Sheng Zhi Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)
// This set is known as kovsho100 in HBMAME

static struct BurnRomInfo kovlsyxqszlRomDesc[] = {
	{ "p0600h_101_po100.rom",		0x0400000, 0xe1bf582e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600_pw10.rom",    			0x0800000, 0xa919ec0f, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600_po73.rom",	  	 	 	0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601_po73.rom",	  	 		0x0800000, 0x9c1d1bea, 3 | BRF_GRA }, 	        //  3 
	{ "a0602_po73.rom",	   			0x0800000, 0x7b437e0f, 3 | BRF_GRA }, 	        //  4
	{ "a0540_po89.rom",	  	 		0x0800000, 0xb038e568, 3 | BRF_GRA }, 	        //  5
	{ "a0541_po73.rom",	   			0x0800000, 0x1fa3c921, 3 | BRF_GRA }, 	        //  6

	{ "b0600_po73.rom",	   			0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "b0540_po89.rom",	   			0x0800000, 0x03a1641e, 4 | BRF_GRA },			//  8
	
	{ "m0600_po69.rom",	 	  		0x0400000, 0xec3f335d, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china_po89.asic", 0x0004000, 0x4627f36f, 7 | BRF_PRG | BRF_ESS },  // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovlsyxqszl, kovlsyxqszl, pgm)
STD_ROM_FN(kovlsyxqszl)

struct BurnDriver BurnDrvkovlsyxqszl = {
	"kovlsyxqszl", "kovshp", "pgm", NULL, "2018",
	"Luan Shi Ying Xiong - Qiu Sheng Zhi Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Luan Shi Ying Xiong - Qiu Sheng Zhi Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)\0\u4E71\u4E16\u82F1\u96C4 - \u6C42\u751F\u4E4B\u8DEF (\u4E09\u56FD\u6218\u7EAA - \u4E71\u4E16\u67AD\u96C4 \u4FEE\u6539\u7248, \u7248\u672C 500)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovlsyxqszlRomInfo, kovlsyxqszlRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovassgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luan Shi Ying Xiong - Qun Xiong Zhu Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)
// This set is known as kovsho101 in HBMAME

static struct BurnRomInfo kovlsyxqxzlRomDesc[] = {
	{ "p0600h_101_po101.rom",		0x0400000, 0x4c434662, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600_pw10.rom",    			0x0800000, 0xa919ec0f, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600_po73.rom",	  	 	 	0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601_po73.rom",	  	 		0x0800000, 0x9c1d1bea, 3 | BRF_GRA }, 	        //  3 
	{ "a0602_po73.rom",	   			0x0800000, 0x7b437e0f, 3 | BRF_GRA }, 	        //  4
	{ "a0540_po75.rom",	  	 		0x0800000, 0x612306ed, 3 | BRF_GRA }, 	        //  5
	{ "a0541_po113.rom",	   		0x0800000, 0xfac5160e, 3 | BRF_GRA }, 	        //  6

	{ "b0600_po73.rom",	   			0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "b0540_po113.rom",	   		0x0800000, 0xb7032905, 4 | BRF_GRA },			//  8
	
	{ "m0600_po69.rom",	 	  		0x0400000, 0xec3f335d, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china_pw113.asic", 	0x0004000, 0xd9a43086, 7 | BRF_PRG | BRF_ESS },  // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovlsyxqxzl, kovlsyxqxzl, pgm)
STD_ROM_FN(kovlsyxqxzl)

struct BurnDriver BurnDrvkovlsyxqxzl = {
	"kovlsyxqxzl", "kovshp", "pgm", NULL, "2018",
	"Luan Shi Ying Xiong - Qun Xiong Zhu Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Luan Shi Ying Xiong - Qiu Sheng Zhu Lu (hack of Knights of Valour Super Heroes Plus, ver. 500)\0\u4E71\u4E16\u82F1\u96C4 - \u7FA4\u96C4\u9010\u9E7F (\u4E09\u56FD\u6218\u7EAA - \u4E71\u4E16\u67AD\u96C4 \u4FEE\u6539\u7248, \u7248\u672C 500)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovlsyxqxzlRomInfo, kovlsyxqxzlRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovassgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luan Shi Ying Xiong - Qun Xiong Zhu Lu Plus (hack of Knights of Valour Super Heroes Plus, ver. 500)
// This set is known as kovsho102 in HBMAME

static struct BurnRomInfo kovlsyxqxzlpRomDesc[] = {
	{ "p0600h_101_po102.rom",		0x0400000, 0x9f1274ff, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "t0600_pw10.rom",    			0x0800000, 0xa919ec0f, 2 | BRF_GRA },			//  1 Tile data

	{ "a0600_po73.rom",	  	 	 	0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "a0601_po73.rom",	  	 		0x0800000, 0x9c1d1bea, 3 | BRF_GRA }, 	        //  3 
	{ "a0602_po73.rom",	   			0x0800000, 0x7b437e0f, 3 | BRF_GRA }, 	        //  4
	{ "a0540_po75.rom",	  	 		0x0800000, 0x612306ed, 3 | BRF_GRA }, 	        //  5
	{ "a0541_po113.rom",	   		0x0800000, 0xfac5160e, 3 | BRF_GRA }, 	        //  6

	{ "b0600_po73.rom",	   			0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "b0540_po113.rom",	   		0x0800000, 0xb7032905, 4 | BRF_GRA },			//  8
	
	{ "m0600_po69.rom",	 	  		0x0400000, 0xec3f335d, 5 | BRF_SND },			//  9 Samples

	{ "kovsh_v100_china_pw113.asic", 	0x0004000, 0xd9a43086, 7 | BRF_PRG | BRF_ESS },  // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovlsyxqxzlp, kovlsyxqxzlp, pgm)
STD_ROM_FN(kovlsyxqxzlp)

struct BurnDriver BurnDrvkovlsyxqxzlp = {
	"kovlsyxqxzlp", "kovshp", "pgm", NULL, "2018",
	"Luan Shi Ying Xiong - Qun Xiong Zhu Lu Plus (hack of Knights of Valour Super Heroes Plus, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Luan Shi Ying Xiong - Qiu Sheng Zhu Lu Plus (hack of Knights of Valour Super Heroes Plus, ver. 500)\0\u4E71\u4E16\u82F1\u96C4 - \u7FA4\u96C4\u9010\u9E7F Plus (\u4E09\u56FD\u6218\u7EAA - \u4E71\u4E16\u67AD\u96C4 \u4FEE\u6539\u7248, \u7248\u672C 500)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovlsyxqxzlpRomInfo, kovlsyxqxzlpRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovassgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Yi Tong Zhong Yuan Qing Ban (2020-A, hack)
// Hack by FEITIANXUE
// YZKOF 2020-07-21

static struct BurnRomInfo kovytzyceRomDesc[] = {
	{ "kovytzyce_p0600.rom",		0x0400000, 0xb2e375cb, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "pgm_t0600.u2",				0x0800000, 0x4acc1ad6, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "kovytzyce_a0602.rom",		0x0800000, 0xf25b6930, 3 | BRF_GRA }, 	        //  4
	{ "kovytzyce_a0540.rom",		0x0800000, 0x0acdd7f2, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "kovytzyce_b0540.rom",		0x0800000, 0x767bf47a, 4 | BRF_GRA },			//  7

	{ "kovytzyce_m0600.rom",	   	0x0400000, 0x1f981f2c, 5 | BRF_SND },			//  8 Samples

	{ "kovytzyce_v100_china.asic",	0x0004000, 0x36a4ecc8, 7 | BRF_PRG | BRF_ESS },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovytzyce, kovytzyce, pgm)
STD_ROM_FN(kovytzyce)

struct BurnDriver BurnDrvkovytzyce = {
	"kovytzyce", "kovshp", "pgm", NULL, "2020-07-21",
	"Knights of Valour Super Heroes / Yi Tong Zhong Yuan Qing Ban (2020-A, hack)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Knights of Valour Super Heroes / Yi Tong Zhong Yuan Qing Ban (2020-A, hack)\0\u4E09\u56FD\u6218\u7EAA - \u4E00\u7EDF\u4E2D\u539F \u9752\u7248 (2020-A, \u4FEE\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovytzyceRomInfo, kovytzyceRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Super Heroes / Yi Tong Zhong Yuan Wu Shuang Ban (2019-0, hack)
// This set is known as kovsho184 in HBMAME

static struct BurnRomInfo kovytzywsRomDesc[] = {
	{ "ytzyws_v201cn.rom",			0x0400000, 0x4f01a87d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "ytzyws_t0600.u2",			0x0800000, 0xde7fdc0c, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",				0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",				0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "ytzyws_a0602.u6",			0x0800000, 0xf25b6930, 3 | BRF_GRA }, 	        //  4
	{ "ytzyws_a05401w064.u8",		0x0800000, 0xdf811b9c, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",				0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "ytzyws_b05401w064.u11",		0x0800000, 0xaa821799, 4 | BRF_GRA },			//  7

	{ "ytzyws_m0600.u5",			0x0400000, 0x53532587, 5 | BRF_SND },			//  8 Samples

	{ "ytzyws_v100_china.asic",		0x0004000, 0x5aa832c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovytzyws, kovytzyws, pgm)
STD_ROM_FN(kovytzyws)

struct BurnDriver BurnDrvkovytzyws = {
	"kovytzyws", "kovshp", "pgm", NULL, "2018",
	"Knights of Valour Super Heroes / Yi Tong Zhong Yuan Wu Shuang Ban (2019-0, hack)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	L"Knights of Valour Super Heroes / Yi Tong Zhong Yuan Wu Shuang Ban (2019-0, hack)\0\u4E09\u56FD\u6218\u7EAA - \u4E00\u7EDF\u4E2D\u539F \u65E0\u53CC\u7248 (2019-0, \u4FEE\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovytzywsRomInfo, kovytzywsRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Yi Tong Zhong Yuan - Warriors (20211024)
// Modified by Fei Tian Xue

static struct BurnRomInfo kovytzywscwRomDesc[] = {
	{ "ytzywscw_v201cn.rom",		0x0600000, 0xf5b62abd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "ytzywscw_t0600.u2",			0x0800000, 0x29e43d4f, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a0600.u3",				0x0800000, 0xd8167834, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a0601.u4",				0x0800000, 0xff7a4373, 3 | BRF_GRA }, 	        //  3 
	{ "ytzywscw_a0602.u6",			0x0800000, 0xbb211e0f, 3 | BRF_GRA }, 	        //  4
	{ "ytzywscw_a05401w064.u8",		0x1000000, 0xc58c518f, 3 | BRF_GRA }, 	        //  5

	{ "pgm_b0600.u9",				0x0800000, 0x7d3cd059, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "ytzywscw_b05401w064.u11",	0x1000000, 0x3ac97a58, 4 | BRF_GRA },			//  7

	{ "ytzywscw_m0600.u5",			0x0800000, 0x574353db, 5 | BRF_SND },			//  8 Samples

	{ "ytzyws_v100_china.asic",		0x0004000, 0x5aa832c1, 7 | BRF_PRG | BRF_ESS }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovytzywscw, kovytzywscw, pgm)
STD_ROM_FN(kovytzywscw)

static INT32 kovshpsprhackInit()
{
	nPGMSpriteBufferHack = 1;

	return kovshpInit();
}

struct BurnDriver BurnDrvkovytzywscw = {
	"kovytzywscw", "kovshp", "pgm", NULL, "2021",
	"Yi Tong Zhong Yuan - Warriors\0", "Imperfect Protection Emulation", "hack", "PolyGameMaster",
	L"Yi Tong Zhong Yuan - Warriors\0\u4e00\u7edf\u4e2d\u539f - \u65e0\u53cc\u7248\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovytzywscwRomInfo, kovytzywscwRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpsprhackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Zhan Ji 2 Fengwu Longyin (ver. 205S, , Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)
// Modified by XIAOFAN
// GOTVG 20210510

static struct BurnRomInfo kov2pfwllRomDesc[] = {
	{ "kov2pfwll_32m.u8",			0x0400000, 0x92de1852, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "kov2pfwll_t1200.u21",		0x0800000, 0x279a2f9e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",				0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",				0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",				0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",				0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",				0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",				0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",				0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",				0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2p_igs027a_china.bin",	0x0004000, 0x19a0bd95, 7 | BRF_PRG | BRF_ESS },	// 10 Internal ARM7 Rom

	{ "kov2pfwll_16m.u23",			0x0200000, 0x672f2d48, 8 | BRF_PRG | BRF_ESS },	// 11 External ARM7 Rom
};

STDROMPICKEXT(kov2pfwll, kov2pfwll, pgm)
STD_ROM_FN(kov2pfwll)

static INT32 kov2pfwllInit()
{
	nPGMMapperHack = 1;

	INT32 nRet = kov2pInit();
	
	if (nRet == 0)
	{
		if (PGMARMROM[0x26f] == 0x0a)	// crash patch
			PGMARMROM[0x26f] = 0xea;
	}

	return nRet;
}

struct BurnDriver BurnDrvKov2pfwll = {
	"kov2pfwll", "kov2p", "pgm", NULL, "2021",
	"Sanguo Zhan Ji 2 Fengwu Longyin (ver. 205S, , Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)\0", NULL, "Hack", "PolyGameMaster",
	L"\u4e09\u570b\u6230\u7d00 2 - \u51e4\u821e\u9f99\u541f (ver. 205S, , Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2pfwllRomInfo, kov2pfwllRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kov2pfwllInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sanguo Zhan Ji 2 Fengwu Longyin (ver. S0709, Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)

static struct BurnRomInfo kov2pfwlysRomDesc[] = {
	{ "fwlys_32m.u8",					0x0400000, 0x05bb05bd, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "fwlys_t1200.u21",				0x0800000, 0x279a2f9e, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",					0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",					0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",					0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",					0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",					0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",					0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",					0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",					0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2pfwlys_igs027a_china.bin",	0x0004000, 0x06214503, 7 | BRF_PRG | BRF_ESS },	// 10 Internal ARM7 Rom

	{ "fwlys_16m.u23",					0x0200000, 0x5a090a75, 8 | BRF_PRG | BRF_ESS },	// 11 External ARM7 Rom
};

STDROMPICKEXT(kov2pfwlys, kov2pfwlys, pgm)
STD_ROM_FN(kov2pfwlys)

struct BurnDriver BurnDrvkov2pfwlys = {
	"kov2pfwlys", "kov2p", "pgm", NULL, "2021",
	"Sanguo Zhan Ji 2 Fengwu Longyin (ver. S0709, Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)\0", NULL, "Hack", "PolyGameMaster",
	L"\u4e09\u570b\u6230\u7d00 2 - \u51e4\u821e\u9f99\u541f (ver. S0709, Hack of Sanguo Zhan Ji 2 Qunxiong Zhengba)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2pfwlysRomInfo, kov2pfwlysRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Xie Feng Tian Chi (Hack)
// Hacked by FEITIANXUE

static struct BurnRomInfo kov2pshpdRomDesc[] = {
	{ "kov2pshpd_32m.u8",			0x0600000, 0x39352749, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u27",	  			0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "kov2pshpd_a1200.u1",			0x0800000, 0xf93967d6, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",				0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "kov2pshpd_a1202.u6",			0x0800000, 0x49ac20bb, 3 | BRF_GRA },			//  4
	{ "kov2pshpd_a1203.u8",			0x0800000, 0x56a3f258, 3 | BRF_GRA },			//  5
	{ "kov2pshpd_a1204.u10",		0x0200000, 0x7bbc9f1e, 3 | BRF_GRA },			//  6

	{ "kov2pshpd_b1200.u5",			0x0800000, 0xd9cdb53e, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "kov2pshpd_b1201.u7",			0x0800000, 0xb693dad1, 4 | BRF_GRA },			//  8
	{ "kov2pshpd_b120x.bin",		0x0800000, 0xf134dc7e, 4 | BRF_GRA },			//  9

	{ "kov2pshpd_m1200.u3",			0x0800000, 0x30eb3c86, 5 | BRF_SND },			// 10 Samples

	{ "kov2pshpd_igs027a_china.bin",	0x0004000, 0x40a75c7e, 7 | BRF_PRG | BRF_ESS }, // 11 Internal ARM7 Rom

	{ "kov2pshpd_16m.u23",			0x0200000, 0xc4c1ddba, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(kov2pshpd, kov2pshpd, pgm)
STD_ROM_FN(kov2pshpd)

struct BurnDriver BurnDrvkov2pshpd = {
	"kov2pshpd", "kov2p", "pgm", NULL, "2019-12-16",
	"Knights of Valour 2 Plus - Xie Feng Tian Chi (Hack)\0", NULL, "Hack", "PolyGameMaster",
	L"Knights of Valour 2 Plus - Xie Feng Tian Chi (Hack)\0\u4e82\u4e16\u82f1\u96c4 2 - \u90aa\u9cf3\u5929\u71be (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2pshpdRomInfo, kov2pshpdRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend Special / Da Sheng Gui Lai (Hack)
// Hacked by XIAOYUER
// GOTVG 2021/01/11

static struct BurnRomInfo oldsdsglRomDesc[] = {
	{ "dsgl_p0500_v100.u24",	0x0400000, 0xfd55142c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "dsgl_t0500.u18",			0x0400000, 0x434ddae8, 2 | BRF_GRA },			//  1 Tile data
	{ "pgm_t0501.u19",			0x0200000, 0xd2106864, 2 | BRF_GRA },			//  2
	

	{ "pgm_a0500.u5",			0x0400000, 0x80a59197, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "pgm_a0501.u6",			0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  4
	{ "pgm_a0502.u7",			0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  5
	{ "pgm_a0503.u8",			0x0400000, 0x066dffec, 3 | BRF_GRA },			//  6
	{ "pgm_a0504.u11",			0x0400000, 0x45337583, 3 | BRF_GRA },			//  7
	{ "pgm_a0505.u12",			0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			//  8
	{ "dsgl_a0506.u13",			0x0400000, 0x1c174e4a, 3 | BRF_GRA },			//  9

	{ "pgm_b0500.u9",			0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",			0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 11
	{ "pgm_b0502.u15",			0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 12
	{ "dsgl_b0503.u16",			0x0400000, 0x4922301f, 4 | BRF_GRA },			// 13

	{ "pgm_m0500.u1",			0x0200000, 0x37928cdd, 5 | BRF_SND },			// 14 Samples

#if !defined (ROM_VERIFY)
	{ "sp_data.u6",				0x010000, 0xe7613dda, 9 | BRF_PRG | BRF_ESS },	// 15 Protection Rom
#else
	{ "ram_dump",				0x040000, 0x280cfb4e, 0 | BRF_OPT },			// 16 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(oldsdsgl, oldsdsgl, pgm)
STD_ROM_FN(oldsdsgl)

struct BurnDriver BurnDrvoldsdsgl = {
	"oldsdsgl", "olds", "pgm", NULL, "2020",
	"Oriental Legend Special - Da Sheng Gui Lai (Hack)\0", NULL, "Hack", "PolyGameMaster",
	L"Oriental Legend Super - Da Sheng Gui Lai (Hack)\0\u897f\u884c\u5e73\u5996\u8a18 - \u5927\u8056\u6b78\u4f86 (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, oldsdsglRomInfo, oldsdsglRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, oldschsDIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend ZERO (Hack)
// Hacked by FEITIANXUE
// GOTVG 2019/06/06

static struct BurnRomInfo zerofxzRomDesc[] = {
	{ "zerofxz_p0500.u24",			0x0400000, 0x98c35ddd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t0500.u18",				0x0400000, 0xd881726c, 2 | BRF_GRA },			//  1 Tile data
	{ "pgm_t0501.u19",				0x0200000, 0xd2106864, 2 | BRF_GRA },			//  2

	{ "pgm_a0500.u5",				0x0400000, 0x80a59197, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "pgm_a0501.u6",				0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  4
	{ "pgm_a0502.u7",				0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  5
	{ "pgm_a0503.u8",				0x0400000, 0x066dffec, 3 | BRF_GRA },			//  6
	{ "pgm_a0504.u11",				0x0400000, 0x45337583, 3 | BRF_GRA },			//  7
	{ "pgm_a0505.u12",				0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			//  8
	{ "zerofxz_a0506.u13",			0x0400000, 0xf5e5cce3, 3 | BRF_GRA },			//  9

	{ "pgm_b0500.u9",				0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",				0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 11
	{ "pgm_b0502.u15",				0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 12
	{ "zerofxz_b0503.u16",			0x0400000, 0x9679f77d, 4 | BRF_GRA },			// 13

	{ "pgm_m0500.u1",				0x0200000, 0x37928cdd, 5 | BRF_SND },			// 14 Samples
	
#if !defined (ROM_VERIFY)
	{ "sp_data.u6",					0x010000, 0xe7613dda, 9 | BRF_PRG | BRF_ESS },	// 15 Protection Rom
#else
	{ "ram_dump",					0x040000, 0x280cfb4e, 0 | BRF_OPT },			// 16 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(zerofxz, zerofxz, pgm)
STD_ROM_FN(zerofxz)

struct BurnDriver BurnDrvzerofxz = {
	"zerofxz", "olds", "pgm", NULL, "2019",
	"Oriental Legend ZERO (Hack)\0", NULL, "Hack", "PolyGameMaster",
	L"Oriental Legend ZERO (Hack)\0\u897f\u904a\u91cb\u5384\u50b3 ZERO (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, zerofxzRomInfo, zerofxzRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, oldschsDIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend Special / Meng Xun Ling Shan (Hack)
// Modified by XIAOYUER
// GOTVG 20240905

static struct BurnRomInfo oldsmxRomDesc[] = {
	{ "oldsmx_p0500.u24",			0x0400000, 0xd271e4fb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "oldsmx_t0500.u18",			0x0400000, 0x434ddae8, 2 | BRF_GRA },			//  1 Tile data
	{ "pgm_t0501.u19",				0x0200000, 0xd2106864, 2 | BRF_GRA },			//  2

	{ "pgm_a0500.u5",				0x0400000, 0x80a59197, 3 | BRF_GRA },			//  3 Sprite Color Data
	{ "pgm_a0501.u6",				0x0400000, 0x98c931b1, 3 | BRF_GRA },			//  4
	{ "pgm_a0502.u7",				0x0400000, 0xc3fcdf1d, 3 | BRF_GRA },			//  5
	{ "pgm_a0503.u8",				0x0400000, 0x066dffec, 3 | BRF_GRA },			//  6
	{ "pgm_a0504.u11",				0x0400000, 0x45337583, 3 | BRF_GRA },			//  7
	{ "pgm_a0505.u12",				0x0400000, 0x5b8cf3a5, 3 | BRF_GRA },			//  8
	{ "oldsmx_a0506.u13",			0x0400000, 0xb88991f9, 3 | BRF_GRA },			//  9

	{ "pgm_b0500.u9",				0x0400000, 0xcde07f74, 4 | BRF_GRA },			// 10 Sprite Masks & Color Indexes
	{ "pgm_b0501.u10",				0x0400000, 0x1546c2e9, 4 | BRF_GRA },			// 11
	{ "pgm_b0502.u15",				0x0400000, 0xe97b31c3, 4 | BRF_GRA },			// 12
	{ "oldsmx_b0503.u16",			0x0400000, 0xd1d330e8, 4 | BRF_GRA },			// 13

	{ "pgm_m0500.u1",				0x0200000, 0x37928cdd, 5 | BRF_SND },			// 14 Samples
	
#if !defined (ROM_VERIFY)
	{ "sp_data.u6",	   				0x010000, 0xe7613dda, 9 | BRF_PRG | BRF_ESS },	// 15 Protection Rom
#else
	{ "ram_dump",	   				0x040000, 0x280cfb4e, 0 | BRF_OPT },			// 16 Dump of RAM shared with protection device
#endif
};

STDROMPICKEXT(oldsmx, oldsmx, pgm)
STD_ROM_FN(oldsmx)

struct BurnDriver BurnDrvoldsmx = {
	"oldsmx", "olds", "pgm", NULL, "2020",
	"Xiyou Shi E Zhuan Super - Meng Xun Ling Shan (Hack)\0", NULL, "Hack", "PolyGameMaster",
	L"Xiyou Shi E Zhuan Super - Meng Xun Ling Shan (Hack)\0\u897f\u884c\u5e73\u5996\u8bb0 - \u68a6\u5bfb\u7075\u5c71\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, oldsmxRomInfo, oldsmxRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, oldschsDIPInfo,
	oldsInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus - Qun Xiong Luan Wu 2020 / 三国战纪 - 群雄乱舞 2020
// GOTVG 20230304

static struct BurnRomInfo kovplus20txRomDesc[] = {
	{ "20tx_p0603_119.u1",	0x0400000, 0x8ede5eea, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "20tx_t0600.u11",		0x0800000, 0x2cf53a60, 2 | BRF_GRA },                        //  1 Tile data
	
	{ "20tx_a0600.u2",		0x0800000, 0x783df23c, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "20tx_a0601.u4",		0x0800000, 0x551e3563, 3 | BRF_GRA },                        //  3
	{ "20tx_a0602.u6",		0x0800000, 0x58615616, 3 | BRF_GRA },                        //  4
	{ "20tx_a0603.u9",		0x0400000, 0x4d755da5, 3 | BRF_GRA },                        //  5
	
	{ "20tx_b0600.u5",		0x0800000, 0xe3bf000f, 4 | BRF_GRA },                        //  6 Sprite Masks & Color Indexes
	{ "20tx_b0601.u7",		0x0400000, 0xc91abed0, 4 | BRF_GRA },                        //  7
	
	{ "20tx_m0600.u3",		0x0400000, 0x68fa1b32, 5 | BRF_SND },                        //  8 Samples
	
	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovplus20tx, kovplus20tx, pgm)
STD_ROM_FN(kovplus20tx)

struct BurnDriver BurnDrvkovplus20tx = {
	"kovplus20tx", "kovplus", "pgm", NULL, "2023",
	"Knights of Valour Plus - Qun Xiong Luan Wu 2020\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Plus - Qun Xiong Luan Wu 2020\0\u4e09\u56fd\u6218\u7eaa - \u7fa4\u96c4\u4e71\u821e 2020\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovplus20txRomInfo, kovplus20txRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
    kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour 2 Plus - Extend Magic Plus (Hack)
// Hacked by Ruisayg 2020/09/13
// https://www.ppxclub.com/forum.php?mod=viewthread&tid=700897

static struct BurnRomInfo kov2pempRomDesc[] = {
	{ "kov2pemp_32m.u8",				0x0400000, 0xde8bf78a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1200.u21",					0x0800000, 0xd7e26609, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1200.u1",					0x0800000, 0xceeb81d8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1201.u4",					0x0800000, 0x21063ca7, 3 | BRF_GRA },			//  3
	{ "pgm_a1202.u6",					0x0800000, 0x4bb92fae, 3 | BRF_GRA },			//  4
	{ "pgm_a1203.u8",					0x0800000, 0xe73cb627, 3 | BRF_GRA },			//  5
	{ "pgm_a1204.u10",					0x0200000, 0x14b4b5bb, 3 | BRF_GRA },			//  6

	{ "pgm_b1200.u5",					0x0800000, 0xbed7d994, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",					0x0800000, 0xf251eb57, 4 | BRF_GRA },			//  8

	{ "pgm_m1200.u3",					0x0800000, 0xb0d88720, 5 | BRF_SND },			//  9 Samples

	{ "kov2pemp_igs027a_china.bin",		0x0004000, 0x06214503, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "kov2pemp_16m.u23",				0x0200000, 0x03ffac76, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2pemp, kov2pemp, pgm)
STD_ROM_FN(kov2pemp)

struct BurnDriver BurnDrvKov2pemp = {
	"kov2pemp", "kov2p", "pgm", NULL, "2020",
	"Knights of Valour 2 Plus - Extend Magic Plus (Hack)\0", NULL, "Hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2pempRomInfo, kov2pempRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kov2pInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters - Community Patch (Hack)
// Hack by GwyrgynBlood
// Enhance game balance

static struct BurnRomInfo marmatcpRomDesc[] = {
	{ "v104_32m_cp.u9",   			0x0400000, 0x29d89916, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10

	{ "martial_masters_v102_usa.asic",	0x0004000, 0xa6c0828c, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "v102_16m.u10",  				0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(marmatcp, marmatcp, pgm)
STD_ROM_FN(marmatcp)

struct BurnDriver BurnDrvMarmatcp = {
	"marmatcp", "martmast", "pgm", NULL, "20??",
	"Martial Masters - Community Patch (Hack)\0", NULL, "Hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, marmatcpRomInfo, marmatcpRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmastDIPInfo,
	martmastInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Martial Masters - Secret Characters (Hack)
// Hack by kuze (?)
// patch available from https://www.arcade-projects.com/forums/index.php?thread/3034

static struct BurnRomInfo marmatscRomDesc[] = {
	{ "v104_32m_sc.u9",   			0x0400000, 0xda10c8d1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "pgm_t1000.u3",	 			0x0800000, 0xbbf879b5, 2 | BRF_GRA },			//  1 Tile data

	{ "pgm_a1000.u3",    			0x0800000, 0x43577ac8, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "pgm_a1001.u4",    			0x0800000, 0xfe7a476f, 3 | BRF_GRA },			//  3
	{ "pgm_a1002.u6",    			0x0800000, 0x62e33d38, 3 | BRF_GRA },			//  4
	{ "pgm_a1003.u8",    			0x0800000, 0xb2c4945a, 3 | BRF_GRA },			//  5
	{ "pgm_a1004.u10",   			0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },			//  6

	{ "pgm_b1000.u9",	  			0x0800000, 0xc5961f6f, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "pgm_b1001.u11",	  			0x0800000, 0x0b7e1c06, 4 | BRF_GRA },			//  8

	{ "pgm_m1000.u5",     			0x0800000, 0xed407ae8, 5 | BRF_SND },			//  9 Samples
	{ "pgm_m1001.u7",      			0x0400000, 0x662d2d48, 5 | BRF_SND },			// 10

	{ "martial_masters_v102_usa.asic",	0x0004000, 0xa6c0828c, 7 | BRF_PRG | BRF_ESS },  // 11 Internal ARM7 Rom

	{ "v102_16m.u10",  				0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS }, // 12 External ARM7 Rom
};

STDROMPICKEXT(marmatsc, marmatsc, pgm)
STD_ROM_FN(marmatsc)

struct BurnDriver BurnDrvMarmatsc = {
	"marmatsc", "martmast", "pgm", NULL, "2017",
	"Martial Masters - Secret Characters (Hack)\0", NULL, "Hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_VSFIGHT, 0,
	NULL, marmatscRomInfo, marmatscRomName, NULL, NULL, NULL, NULL, pgmInputInfo, martmastDIPInfo,
	martmastInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Oriental Legend 2 (One Key Edition, Hack)
// Hacked by 哆啦A梦 2021/10/28
// https://www.ppxclub.com/forum.php?mod=viewthread&tid=702576&extra=page%3D1

static struct BurnRomInfo oldsplusokRomDesc[] = {
	{ "ok_v-205cn.u10",			0x0400000, 0x80eb8fda, 1 | BRF_PRG | BRF_ESS },	//  0 68K Code

	{ "igs_t05301w064.u2",		0x0800000, 0x8257bbb0, 2 | BRF_GRA },			//  1 Tile data

	{ "igs_a05301w064.u3",		0x0800000, 0x57946fd2, 3 | BRF_GRA },	 		//  2 Sprite Color Data
	{ "igs_a05302w064.u4",		0x0800000, 0x3459a0b8, 3 | BRF_GRA },			//  3
	{ "igs_a05303w064.u6",		0x0800000, 0x13475d85, 3 | BRF_GRA },			//  4
	{ "igs_a05304w064.u8",		0x0800000, 0xf03ef7a6, 3 | BRF_GRA },			//  5

	{ "igs_b05301w064.u9",		0x0800000, 0xfd98f503, 4 | BRF_GRA },			//  6 Sprite Masks & Color Indexes
	{ "igs_b05302w064.u11",		0x0800000, 0x9f6094a8, 4 | BRF_GRA },			//  7

	{ "igs_w05301b032.u5",		0x0400000, 0x86ec83bc, 5 | BRF_SND },			//  8 Samples

	{ "oldsplus_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },	//  9 Internal ARM7 Rom
};

STDROMPICKEXT(oldsplusok, oldsplusok, pgm)
STD_ROM_FN(oldsplusok)

struct BurnDriver BurnDrvoldsplusok = {
	"oldsplusok", "oldsplus", "pgm", NULL, "2021",
	"Oriental Legend 2 (One Key Edition, Hack)\0", "Incomplete Dump", "Hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, oldsplusokRomInfo, oldsplusokRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, oldsplusnrDIPInfo,
	oldsplusInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Luan Shi Ying Xiong - Qun Xiong Zhu Lu Wu Shuang Edition (Hack, ver. 500)
// GOTVG 20201218

static struct BurnRomInfo kovshpd3dwRomDesc[] = {
	{ "kovshpd3dw_p0600.rom",			0x0400000, 0xf88f4b0a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "kovshpd3dw_t0600.rom",			0x0800000, 0x1f78d522, 2 | BRF_GRA },			//  1 Tile data

	{ "kovshpd3dw_a0600.rom",			0x0800000, 0x72643288, 3 | BRF_GRA },			//  2 Sprite Color Data
	{ "kovshpd3dw_a0601.rom",			0x0800000, 0x94a0c63d, 3 | BRF_GRA }, 	        //  3 
	{ "kovshpd3dw_a0602.rom",			0x0800000, 0xaf21456c, 3 | BRF_GRA }, 	        //  4
	{ "kovshpd3dw_a0540.rom",			0x0800000, 0xa3204d56, 3 | BRF_GRA }, 	        //  5
	{ "kovshpd3dw_a0541.rom",			0x0800000, 0xfac5160e, 3 | BRF_GRA }, 	        //  6

	{ "kovshpd3dw_b0600.rom",	   		0x0800000, 0x0646c5ef, 4 | BRF_GRA },			//  7 Sprite Masks & Color Indexes
	{ "kovshpd3dw_b0540.rom",	   		0x0800000, 0x8697ec72, 4 | BRF_GRA },			//  8
	{ "kovshpd3dw_b0601.rom",	   		0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },			//  9
	
	{ "kovshpd3dw_m0600.rom",			0x0400000, 0xec3f335d, 5 | BRF_SND },			// 10 Samples

	{ "kovshpd3dw_v100_china.asic", 	0x0004000, 0xd9a43086, 7 | BRF_PRG | BRF_ESS },	// 11 Internal ARM7 Rom
};

STDROMPICKEXT(kovshpd3dw, kovshpd3dw, pgm)
STD_ROM_FN(kovshpd3dw)

struct BurnDriver BurnDrvkovshpd3dw = {
	"kovshpd3dw", "kovshp", "pgm", NULL, "2020-12-18",
	"Luan Shi Ying Xiong - Qun Xiong Zhu Lu Wu Shuang Edition (Hack, ver. 500)\0", "Imperfect Protection Emulation", "Hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshpd3dwRomInfo, kovshpd3dwRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovassgInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// 三國戰紀 2 - 鬥轉星旋 (Hack, ver.110)
// Modified by BOLAO
// Contributor: Creamymami (FBAs)

static struct BurnRomInfo kov2dzxxRomDesc[] = {
	{ "dzxx_v110_u18.u18",			0x0400000, 0x4bb58528, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "dzxx_t1200.u27",	   			0x0800000, 0xe27c3fab, 2 | BRF_GRA },           //  1 Tile data

	{ "pgm_a1200.u1",	   			0x0800000, 0xceeb81d8, 3 | BRF_GRA },           //  2 Sprite Color Data
	{ "pgm_a1201.u4",   			0x0800000, 0x21063ca7, 3 | BRF_GRA },           //  3
	{ "pgm_a1202.u6",	   			0x0800000, 0x4bb92fae, 3 | BRF_GRA },           //  4
	{ "pgm_a1203.u8",	   			0x0800000, 0xe73cb627, 3 | BRF_GRA },           //  5
	{ "pgm_a1204.u10",   			0x0200000, 0x14b4b5bb, 3 | BRF_GRA },           //  6

	{ "pgm_b1200.u5",	   			0x0800000, 0xbed7d994, 4 | BRF_GRA },           //  7 Sprite Masks & Color Indexes
	{ "pgm_b1201.u7",	   			0x0800000, 0xf251eb57, 4 | BRF_GRA },           //  8

	{ "dzxx_m1200.u3",	   			0x0a00000, 0xc13c47fd, 5 | BRF_SND },           //  9 Samples

	{ "kov2dzxx_v110_china.asic", 	0x0004000, 0xed3198b4, 7 | BRF_PRG | BRF_ESS }, // 10 Internal ARM7 Rom

	{ "dzxx_v110_u19.u19",			0x0300000, 0xce3fb9b3, 8 | BRF_PRG | BRF_ESS }, // 11 External ARM7 Rom
};

STDROMPICKEXT(kov2dzxx, kov2dzxx, kov2dzxxBios)
STD_ROM_FN(kov2dzxx)

static INT32 kov2maphackInit()
{
	nPGMMapperHack = 1;

	return kov2Init();
}

struct BurnDriver BurnDrvkov2dzxx = {
	"kov2dzxx", "kov2", "pgm", NULL, "2021",
	"Knights of Valour 2 - Dou Zhuan Xing Xuan (Hack, ver.110)\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour 2 - Dou Zhuan Xing Xuan\0\u4e09\u570b\u6230\u7d00 2 - \u9b25\u8f49\u661f\u65cb (Hack, ver.110)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kov2dzxxRomInfo, kov2dzxxRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kov2dzxxDIPInfo,
	kov2maphackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus 2012 - Warriors
// GOTVG 20230120

static struct BurnRomInfo kovp12dwRomDesc[] = {
	{ "12dw_p0603_119.u1",	0x0400000, 0x5c0e150b, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "12dw_t0600.u11",		0x0800000, 0x364aec7e, 2 | BRF_GRA },                        //  1 Tile data

	{ "12dw_a0600.u2",		0x0800000, 0x5bd7ea9f, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "pgm_a0601.u4",		0x0800000, 0xff7a4373, 3 | BRF_GRA },                        //  3
	{ "12dw_a0602.u6",		0x0800000, 0xf68a56b7, 3 | BRF_GRA },                        //  4
	{ "pgm_a0603.u9",		0x0400000, 0xec31abda, 3 | BRF_GRA },                        //  5

	{ "12dw_b0600.u5",		0x0800000, 0x07d91ea3, 4 | BRF_GRA },                        //  6 Sprite Masks & Color Indexes
	{ "12dw_b0601.u7",		0x0400000, 0xa8fc03a0, 4 | BRF_GRA },                        //  7

	{ "12dw_m0600.u3",		0x0400000, 0xacadac0d, 5 | BRF_SND },                        //  8 Samples

	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovp12dw, kovp12dw, kovp12dwBios)
STD_ROM_FN(kovp12dw)

struct BurnDriver BurnDrvkovp12dw = {
	"kovp12dw", "kovplus", "pgm", NULL, "2023",
	"Knights of Valour Plus 2012 - Warriors\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Plus 2012 - Warriors\0\u4e09\u570b\u6230\u7d00 \u6b63\u5b97 2012 - \u7121\u96d9 (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovp12dwRomInfo, kovp12dwRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus 2012 - Nightmare
// 20220723

static struct BurnRomInfo kovp12emRomDesc[] = {
	{ "12em_p0603_119.u1",	0x0400000, 0x145f39f9, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "12em_t0600.u11",		0x0800000, 0x597ee034, 2 | BRF_GRA },                        //  1 Tile data

	{ "12dw_a0600.u2",		0x0800000, 0x5bd7ea9f, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "pgm_a0601.u4",		0x0800000, 0xff7a4373, 3 | BRF_GRA },                        //  3
	{ "pgm_a0602.u6",     	0x0800000, 0xe7a32959, 3 | BRF_GRA },                        //  4
	{ "pgm_a0603.u9",		0x0400000, 0xec31abda, 3 | BRF_GRA },                        //  5

	{ "12dw_b0600.u5",		0x0800000, 0x07d91ea3, 4 | BRF_GRA },                        //  6 Sprite Masks & Color Indexes
	{ "pgm_b0601.u7",     	0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },                        //  7

	{ "12em_m0600.u3",		0x0400000, 0x7a50501b, 5 | BRF_SND },                        //  8 Samples

	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovp12em, kovp12em, kovp12emBios)
STD_ROM_FN(kovp12em)

static INT32 kovmaphackInit()
{
	nPGMMapperHack = 1;

	return kovInit();
}

struct BurnDriver BurnDrvkovp12em = {
	"kovp12em", "kovplus", "pgm", NULL, "2022",
	"Knights of Valour Plus 2012 - Nightmare\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Plus 2012 - Nightmare\0\u4e09\u570b\u6230\u7d00 \u6b63\u5b97 2012 - \u5669\u5922 (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovp12emRomInfo, kovp12emRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovmaphackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Plus - New Biography of Heroes

static struct BurnRomInfo kovplusqRomDesc[] = {
	{ "qyxz_p0603_119.u1",	0x0600000, 0x9e4c21bb, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "qyxz_t0600.u11",		0x0800000, 0x1f2a486c, 2 | BRF_GRA },                        //  1 Tile data

	{ "qyxz_a0600.u2",		0x0800000, 0x82616975, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "qyxz_a0601.u4",		0x0800000, 0xd756a6f2, 3 | BRF_GRA },                        //  3
	{ "qyxz_a0602.u6",		0x0800000, 0x5092a3c3, 3 | BRF_GRA },                        //  4
	{ "qyxz_a0603.u9",		0x0800000, 0x7baf3f06, 3 | BRF_GRA },                        //  5

	{ "qyxz_b0600.u5",		0x0800000, 0xe39a14b6, 4 | BRF_GRA },                        //  6 Sprite Masks & Color Indexes
	{ "qyxz_b0601.u7",		0x0800000, 0xf778a641, 4 | BRF_GRA },                        //  7

	{ "pgm_m0600.u3",		0x0400000, 0x3ada4fd6, 5 | BRF_SND },                        //  8 Samples

	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  9 Internal ARM7 Rom
};

STDROMPICKEXT(kovplusq, kovplusq, pgm)
STD_ROM_FN(kovplusq)

struct BurnDriver BurnDrvkovplusq = {
	"kovplusq", "kovplus", "pgm", NULL, "2021",
	"Knights of Valour Plus - New Biography of Heroes (V120)\0", NULL, "Hack", "PolyGameMaster",
	L"Knights of Valour Plus - New Biography of Heroes(v120)\0\u4e09\u570b\u6230\u7d00 - \u7fa4\u82f1\u65b0\u50b3 (v120, \u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovplusqRomInfo, kovplusqRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};

// San Guo Zhan Ji - Qun Xiong Zheng Ba Feng Yun Zai Qi (Hack, ver. 2022-07-31)
// Modified by Fei Tian Xue
// 2022-07-31

static struct BurnRomInfo kovqxzbwsRomDesc[] = {
	{ "qxzb_100.rom",			0x600000, 0xa14c3b2d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "qxzb_t0600.u2",			0x800000, 0x9a361916, 2 | BRF_GRA },           //  1 Tile data

	{ "qxzb_a0600.u3",			0x800000, 0x72643288, 3 | BRF_GRA },           //  2 Sprite Color Data
	{ "qxzb_a0601.u4",			0x800000, 0x85c26c90, 3 | BRF_GRA },           //  3
	{ "qxzb_a0602.u6",			0x800000, 0xee5a308e, 3 | BRF_GRA },           //  4
	{ "qxzb_a05401w064.u8",		0x800000, 0x306adeeb, 3 | BRF_GRA },           //  5
	{ "qxzb_a05411w064.u8",		0x800000, 0x4ed6ed80, 3 | BRF_GRA },           //  6
	{ "qxzb_a05421w064.u8",		0x800000, 0x14499cf4, 3 | BRF_GRA },           //  7
	
	{ "qxzb_b0600.u9",			0x800000, 0x0646c5ef, 4 | BRF_GRA },           //  8 Sprite Masks & Color Indexes
	{ "qxzb_b05401w064.u11",	0x800000, 0x35728e9a, 4 | BRF_GRA },           //  9
	{ "qxzb_b05411w064.u11",	0x800000, 0xfbdae21a, 4 | BRF_GRA },           // 10
	
	{ "qxzb_m0600.u5",			0x800000, 0x468a0d43, 5 | BRF_SND },           // 11 Samples

	{ "qxzb_v100_china.asic",	0x008000, 0xcf5c6119, 7 | BRF_PRG | BRF_ESS }, // 12 Internal ARM7 Rom
};

STDROMPICKEXT(kovqxzbws, kovqxzbws, pgm)
STD_ROM_FN(kovqxzbws)

struct BurnDriver BurnDrvkovqxzbws = {
	"kovqxzbws", "kovshp", "pgm", NULL, "2022",
	"San Guo Zhan Ji - Qun Xiong Zheng Ba Feng Yun Zai Qi (Hack, ver. 2022-07-31)\0", "hack", "IGS", "PolyGameMaster",
	L"San Guo Zhan Ji - Qun Xiong Zheng Ba Feng Yun Zai Qi (Hack, ver. 2022-07-31)\0\u4e09\u570b\u6230\u7d00 - \u7fa4\u96c4\u722d\u9738 \u98a8\u96f2\u518d\u8d77 (\u4fee\u6539\u7248)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovqxzbwsRomInfo, kovqxzbwsRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshpsprhackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// San Guo Zhan Ji - Heng Sao Qian Jun (Hack)

static struct BurnRomInfo kovphsqjRomDesc[] = {
	{ "hsqj_p0603_119.u1",	0x0600000, 0x476f2ae3, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "hsqj_t0600.u11",		0x0800000, 0xec2357a0, 2 | BRF_GRA },                        //  1 Tile data

	{ "hsqj_a0600.u2",		0x0800000, 0x75a6632a, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "hsqj_a0601.u4",		0x0800000, 0x7a737a6c, 3 | BRF_GRA },                        //  3
	{ "hsqj_a0602.u6",		0x0800000, 0xdb5d6514, 3 | BRF_GRA },                        //  4
	{ "hsqj_a0603.u9",		0x0800000, 0xc4d01ddc, 3 | BRF_GRA },                        //  5
	{ "hsqj_a0604.bin",		0x0800000, 0xe361ff69, 3 | BRF_GRA },                        //  6
	{ "hsqj_a0605.bin",		0x0400000, 0x6351ddcf, 3 | BRF_GRA },                        //  7

	{ "hsqj_b0600.u5",		0x0800000, 0xf96d5bf7, 4 | BRF_GRA },                        //  8 Sprite Masks & Color Indexes
	{ "hsqj_b0601.u7",		0x0800000, 0x33b87f2d, 4 | BRF_GRA },                        //  9
	{ "hsqj_b0602.bin",		0x0800000, 0x12a3e8d2, 4 | BRF_GRA },                        // 10

	{ "hsqj_m0600.u3",		0x0800000, 0xc1744a4c, 5 | BRF_SND },                        // 11 Samples

	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 12 Internal ARM7 Rom
};

STDROMPICKEXT(kovphsqj, kovphsqj, pgm)
STD_ROM_FN(kovphsqj)

static INT32 kovsprhackInit()
{
	nPGMSpriteBufferHack = 1;

	return kovInit();
}

struct BurnDriver BurnDrvKovphsqj = {
	"kovphsqj", "kovplus", "pgm", NULL, "2023",
	"San Guo Zhan Ji - Heng Sao Qian Jun (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"San Guo Zhan Ji - Heng Sao Qian Jun\0\u4e09\u570b\u6230\u7d00 - \u6a6b\u6383\u5343\u8ecd (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovphsqjRomInfo, kovphsqjRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovsprhackInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// San Guo Zhan Ji - Zhen Tun Shi Tian Di (Hack)

static struct BurnRomInfo kovptstdRomDesc[] = {
	{ "tstd_p0603_119.u1",	0x0600000, 0xc83ca4de, 1 | BRF_PRG | BRF_ESS },              //  0 68K Code

	{ "tstd_t0600.u11",		0x0800000, 0xc13c78c3, 2 | BRF_GRA },                        //  1 Tile data

	{ "hsqj_a0600.u2",		0x0800000, 0x75a6632a, 3 | BRF_GRA },                        //  2 Sprite Color Data
	{ "tstd_a0601.u4",		0x0800000, 0x689d14df, 3 | BRF_GRA },                        //  3
	{ "tstd_a0602.u6",		0x0800000, 0x4f33e027, 3 | BRF_GRA },                        //  4
	{ "hsqj_a0603.u9",		0x0800000, 0xc4d01ddc, 3 | BRF_GRA },                        //  5
	{ "tstd_a0604.bin",		0x0400000, 0x3145341c, 3 | BRF_GRA },                        //  6

	{ "hsqj_b0600.u5",		0x0800000, 0xf96d5bf7, 4 | BRF_GRA },                        //  7 Sprite Masks & Color Indexes
	{ "tstd_b0601.u7",		0x0800000, 0x7240067d, 4 | BRF_GRA },                        //  8

	{ "tstd_m0600.u3",		0x0800000, 0x7a0db06a, 5 | BRF_SND },                        //  9 Samples

	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP }, // 10 Internal ARM7 Rom
};

STDROMPICKEXT(kovptstd, kovptstd, pgm)
STD_ROM_FN(kovptstd)

struct BurnDriver BurnDrvKovptstd = {
	"kovptstd", "kovplus", "pgm", NULL, "2023",
	"San Guo Zhan Ji - Zhen Tun Shi Tian Di (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"San Guo Zhan Ji - Zhen Tun Shi Tian Di\0\u4e09\u570b\u6230\u7d00 - \u771f\u00b7\u541e\u98df\u5929\u5730 (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_SCRFIGHT, 0,
	NULL, kovptstdRomInfo, kovptstdRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


#define KOVPLUS_COMPONENTS											\
	{ "pgm_t0600.u11",		0x0800000, 0x4acc1ad6, 2 | BRF_GRA },	\
	{ "pgm_a0600.u2",		0x0800000, 0xd8167834, 3 | BRF_GRA },	\
	{ "pgm_a0601.u4",		0x0800000, 0xff7a4373, 3 | BRF_GRA },	\
	{ "pgm_a0602.u6",		0x0800000, 0xe7a32959, 3 | BRF_GRA },	\
	{ "pgm_a0603.u9",		0x0400000, 0xec31abda, 3 | BRF_GRA },	\
	{ "pgm_b0600.u5",		0x0800000, 0x7d3cd059, 4 | BRF_GRA },	\
	{ "pgm_b0601.u7",		0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },	\
	{ "pgm_m0600.u3",		0x0400000, 0x3ada4fd6, 5 | BRF_SND },	\
	{ "kov_igs027a.bin",	0x0004000, 0x00000000, 7 | BRF_PRG | BRF_ESS | BRF_NODUMP },

// San Guo Zhan Ji - Xiao Bing Zheng Ba (Ver.A, Hack)

static struct BurnRomInfo kovpxbaRomDesc[] = {
	{ "xba_p0603_v119.u1",	0x0400000, 0xf8cd3baa, 1 | BRF_PRG | BRF_ESS },

	KOVPLUS_COMPONENTS
};

STDROMPICKEXT(kovpxba, kovpxba, pgm)
STD_ROM_FN(kovpxba)

struct BurnDriver BurnDrvKovpxba = {
	"kovpxba", "kovplus", "pgm", NULL, "2018",
	"San Guo Zhan Ji - Xiao Bing Zheng Ba (Ver.A, Hack)\0", NULL, "hack", "PolyGameMaster",
	L"San Guo Zhan Ji - Xiao Bing Zheng Ba\0\u4e09\u570b\u6230\u7d00 - \u5c0f\u5175\u722d\u9738 (Ver.A, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovpxbaRomInfo, kovpxbaRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// San Guo Zhan Ji - Xiao Bing Zheng Ba (Ver.B, Hack)

static struct BurnRomInfo kovpxbbRomDesc[] = {
	{ "xbb_p0603_v119.u1",	0x0400000, 0xc94d20b9, 1 | BRF_PRG | BRF_ESS },

	KOVPLUS_COMPONENTS
};

STDROMPICKEXT(kovpxbb, kovpxbb, pgm)
STD_ROM_FN(kovpxbb)

struct BurnDriver BurnDrvKovpxbb = {
	"kovpxbb", "kovplus", "pgm", NULL, "2018",
	"San Guo Zhan Ji - Xiao Bing Zheng Ba (Ver.B, Hack)\0", NULL, "hack", "PolyGameMaster",
	L"San Guo Zhan Ji - Xiao Bing Zheng Ba\0\u4e09\u570b\u6230\u7d00 - \u5c0f\u5175\u722d\u9738 (Ver.B, Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM/* | HARDWARE_IGS_USE_ARM_CPU*/, GBF_SCRFIGHT, 0,
	NULL, kovpxbbRomInfo, kovpxbbRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};

#undef KOVPLUS_COMPONENTS


// Knights of Valour Superheroes / Sangoku Senki Superheroes - Dungeons & Warriors (Hack)
// Modified by IGHT
// 20091227

static struct BurnRomInfo kovshdfRomDesc[] = {
	{ "df_p0604_v103.u1",			0x0400000, 0x589ea23f, 1 | BRF_PRG | BRF_ESS },

	{ "pgm_t0600.u11",     			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },
	{ "df_a0604_v200.u9",			0x0800000, 0x6445078b, 3 | BRF_GRA },

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },
	{ "df_b0602_v200.u10",			0x0400000, 0xb0cf55f2, 4 | BRF_GRA },

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },

	{ "kovshdf_v100_china.asic",	0x0004000, 0x70e33f5e, 7 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(kovshdf, kovshdf, pgm)
STD_ROM_FN(kovshdf)

struct BurnDriver BurnDrvKovshdf = {
	"kovshdf", "kovsh", "pgm", NULL, "2009",
	"Knights of Valour Superheroes / Sangoku Senki Superheroes - Dungeons & Warriors (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Superheroes\0\u4e09\u570b\u6230\u7d00\0\u98a8\u96f2\u518d\u8d77 - \u5730\u4e0b\u57ce\u8207\u52c7\u58eb (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshdfRomInfo, kovshdfRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Superheroes / Sangoku Senki Superheroes - The Road Beyond the Gods (Hack)
// 20210312

static struct BurnRomInfo kovshszlRomDesc[] = {
	{ "szl_p0604_v103.u1",		0x0400000, 0x5d6b0ebd, 1 | BRF_PRG | BRF_ESS },

	{ "pgm_t0600.u11",			0x0800000, 0x4acc1ad6, 2 | BRF_GRA },

	{ "pgm_a0600.u1",			0x0800000, 0xd8167834, 3 | BRF_GRA },
	{ "pgm_a0601.u3",			0x0800000, 0xff7a4373, 3 | BRF_GRA },
	{ "pgm_a0602.u5",			0x0800000, 0xe7a32959, 3 | BRF_GRA },
	{ "pgm_a0613.u7",			0x0400000, 0xec31abda, 3 | BRF_GRA },
	{ "szl_a0604_v200.u9",		0x0400000, 0x410f3a5e, 3 | BRF_GRA },

	{ "pgm_b0600.u6",			0x0800000, 0x7d3cd059, 4 | BRF_GRA },
	{ "pgm_b0601.u8",			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },
	{ "szl_b0602_v200.u10",		0x0400000, 0x62f67ca3, 4 | BRF_GRA },

	{ "pgm_m0600.u4",			0x0400000, 0x3ada4fd6, 5 | BRF_SND },

	{ "kovsh_v100_china.asic",	0x0004000, 0x0f09a5c1, 7 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(kovshszl, kovshszl, pgm)
STD_ROM_FN(kovshszl)

struct BurnDriver BurnDrvKovshszl = {
	"kovshszl", "kovsh", "pgm", NULL, "2021",
	"Knights of Valour Superheroes / Sangoku Senki Superheroes - The Road Beyond the Gods (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Superheroes\0\u4e09\u570b\u6230\u7d00\0\u98a8\u96f2\u518d\u8d77 - \u8d85\u795e\u4e4b\u8def (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshszlRomInfo, kovshszlRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Sangoku Senki Super Heroes - Evil & Darkness Plus (Hack)

static struct BurnRomInfo kovshxshRomDesc[] = {
	{ "xs_p0604_v103.u1",		0x0400000, 0x4a20baa5, 1 | BRF_PRG | BRF_ESS },

	{ "xs_t0600.u11",     		0x0800000, 0x36442749, 2 | BRF_GRA },

	{ "xs_a0600.u1",	   		0x0800000, 0xee091a4f, 3 | BRF_GRA },
	{ "pgm_a0601.u3",	   		0x0800000, 0xff7a4373, 3 | BRF_GRA },
	{ "xs_a0602.u5",	   		0x0800000, 0xefb30f14, 3 | BRF_GRA },
	{ "xs_a0613.u7",	   		0x0200000, 0x5ce4b5fa, 3 | BRF_GRA },
	{ "xs_a0604_v200.u9",		0x0800000, 0xcea605ec, 3 | BRF_GRA },

	{ "xs_b0600.u6",	   		0x0800000, 0xfbb494e5, 4 | BRF_GRA },
	{ "xs_b0601.u8",	   		0x0400000, 0xf43e4276, 4 | BRF_GRA },
	{ "xs_b0602_v200.u10",		0x0400000, 0x16844847, 4 | BRF_GRA },

	{ "xs_m0600.u4",	   		0x0400000, 0x968a15b9, 5 | BRF_SND },

	{ "kovxs_v100_china.asic",	0x0004000, 0xf6b6b4e8, 7 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(kovshxsh, kovshxsh, pgm)
STD_ROM_FN(kovshxsh)

struct BurnDriver BurnDrvKovshxsh = {
	"kovshxsh", "kovsh", "pgm", NULL, "2021",
	"Sangoku Senki Super Heroes - Evil & Darkness Plus (Hack)\0", "Sound incomplete", "hack", "PolyGameMaster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshxshRomInfo, kovshxshRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// Knights of Valour Superheroes / Sangoku Senki Superheroes - Muggle Legend (Hack)
// 20200624

static struct BurnRomInfo kovshmgRomDesc[] = {
	{ "mg_p0604_v103.u1",			0x0400000, 0xd0bbd6c9, 1 | BRF_PRG | BRF_ESS },

	{ "mg_t0600.u11",     			0x0800000, 0xa919ec0f, 2 | BRF_GRA },

	{ "pgm_a0600.u1",	   			0x0800000, 0xd8167834, 3 | BRF_GRA },
	{ "pgm_a0601.u3",	   			0x0800000, 0xff7a4373, 3 | BRF_GRA },
	{ "pgm_a0602.u5",	   			0x0800000, 0xe7a32959, 3 | BRF_GRA },
	{ "pgm_a0613.u7",	   			0x0400000, 0xec31abda, 3 | BRF_GRA },
	{ "pgm_a0604_v200.u9",			0x0400000, 0x26b59fd3, 3 | BRF_GRA },

	{ "pgm_b0600.u6",	   			0x0800000, 0x7d3cd059, 4 | BRF_GRA },
	{ "pgm_b0601.u8",	   			0x0400000, 0xa0bb1c2f, 4 | BRF_GRA },
	{ "mg_b0602_v200.u10",			0x0100000, 0x834afabe, 4 | BRF_GRA },

	{ "pgm_m0600.u4",	   			0x0400000, 0x3ada4fd6, 5 | BRF_SND },

	{ "kovshmg_v100_china.asic",	0x0004000, 0x12470e2a, 7 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(kovshmg, kovshmg, pgm)
STD_ROM_FN(kovshmg)

struct BurnDriver BurnDrvKovshmg = {
	"kovshmg", "kovsh", "pgm", NULL, "2020",
	"Knights of Valour Superheroes / Sangoku Senki Superheroes - Muggle Legend (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"Knights of Valour Superheroes\0\u4e09\u570b\u6230\u7d00\0\u98a8\u96f2\u518d\u8d77 - \u9ebb\u74dc\u50b3\u8aaa (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU, GBF_SCRFIGHT, 0,
	NULL, kovshmgRomInfo, kovshmgRomName, NULL, NULL, NULL, NULL, pgmhInputInfo, kovchsDIPInfo,
	kovshInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


#define KILLBLD_COMPONENTS										\
	{ "pgm_t0300.u14",	0x0400000, 0x0922f7d9, 2 | BRF_GRA },	\
	{ "pgm_a0300.u9",	0x0400000, 0x3f9455d3, 3 | BRF_GRA },	\
	{ "pgm_a0301.u10",	0x0400000, 0x92776889, 3 | BRF_GRA },	\
	{ "pgm_a0303.u11",	0x0400000, 0x33f5cc69, 3 | BRF_GRA },	\
	{ "pgm_a0306.u12",	0x0400000, 0xcc018a8e, 3 | BRF_GRA },	\
	{ "pgm_a0307.u2",	0x0400000, 0xbc772e39, 3 | BRF_GRA },	\
	{ "pgm_b0300.u13",	0x0400000, 0x7f876981, 4 | BRF_GRA },	\
	{ "pgm_b0302.u14",	0x0400000, 0xeea9c502, 4 | BRF_GRA },	\
	{ "pgm_b0303.u15",	0x0200000, 0x77a9652e, 4 | BRF_GRA },	\
	{ "pgm_m0300.u1",	0x0400000, 0x93159695, 5 | BRF_SND },	\
	{ "kb_u2_v109.u2",	0x0010000, 0xde3eae63, 9 | BRF_PRG | BRF_ESS },

// The Killing Blade - Heroes (Hack)
// GOTVG 20180622

static struct BurnRomInfo killbldqyRomDesc[] = {
	{ "kbqy_v109.u9",	0x0200000, 0x98c85d0e, 1 | BRF_PRG | BRF_ESS },

	KILLBLD_COMPONENTS
};

STDROMPICKEXT(killbldqy, killbldqy, pgm)
STD_ROM_FN(killbldqy)

struct BurnDriver BurnDrvKillbldqy = {
	"killbldqy", "killbld", "pgm", NULL, "2018",
	"The Killing Blade - Heroes (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"The Killing Blade - Heroes\0\u50b2\u528d\u72c2\u5200 - \u7fa4\u82f1 (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbldqyRomInfo, killbldqyRomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbldDIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};


// The Killing Blade - Infinite Energy (Hack)
// GOTVG 20230527

static struct BurnRomInfo killbldjqRomDesc[] = {
	{ "kbjq_v109.u9",	0x0200000, 0x612ce0f1, 1 | BRF_PRG | BRF_ESS },

	KILLBLD_COMPONENTS
};

STDROMPICKEXT(killbldjq, killbldjq, pgm)
STD_ROM_FN(killbldjq)

struct BurnDriver BurnDrvKillbldjq = {
	"killbldjq", "killbld", "pgm", NULL, "2023",
	"The Killing Blade - Infinite Energy (Hack)\0", NULL, "hack", "PolyGameMaster",
	L"The Killing Blade - Infinite Energy\0\u50b2\u528d\u72c2\u5200 - \u7121\u9650\u6c23 (Hack)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 4, HARDWARE_IGS_PGM, GBF_VSFIGHT, 0,
	NULL, killbldjqRomInfo, killbldjqRomName, NULL, NULL, NULL, NULL, pgmInputInfo, killbldDIPInfo,
	killbldInit, pgmExit, pgmFrame, pgmDraw, pgmScan, &nPgmPalRecalc, 0x900,
	448, 224, 4, 3
};

#undef KILLBLD_COMPONENTS
