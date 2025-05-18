#include "burnint.h"
#include "pce.h"

static struct BurnInputInfo pceInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	PCEJoy1 + 3,	"p1 start"	}, // 0
	{"P1 Select",		BIT_DIGITAL,	PCEJoy1 + 2,	"p1 select"	},
	{"P1 Up",			BIT_DIGITAL,	PCEJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	PCEJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	PCEJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	PCEJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	PCEJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	PCEJoy1 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	PCEJoy1 + 8,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	PCEJoy1 + 9,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	PCEJoy1 + 10,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	PCEJoy1 + 11,	"p1 fire 6"	},

	{"P2 Start",		BIT_DIGITAL,	PCEJoy2 + 3,	"p2 start"	}, // 12
	{"P2 Select",		BIT_DIGITAL,	PCEJoy2 + 2,	"p2 select"	},
	{"P2 Up",			BIT_DIGITAL,	PCEJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	PCEJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	PCEJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	PCEJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	PCEJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	PCEJoy2 + 0,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	PCEJoy2 + 8,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	PCEJoy2 + 9,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	PCEJoy2 + 10,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	PCEJoy2 + 11,	"p2 fire 6"	},

	{"P3 Start",		BIT_DIGITAL,	PCEJoy3 + 3,	"p3 start"	}, // 24
	{"P3 Select",		BIT_DIGITAL,	PCEJoy3 + 2,	"p3 select"	},
	{"P3 Up",			BIT_DIGITAL,	PCEJoy3 + 4,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	PCEJoy3 + 6,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	PCEJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	PCEJoy3 + 5,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	PCEJoy3 + 1,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	PCEJoy3 + 0,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	PCEJoy3 + 8,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	PCEJoy3 + 9,	"p3 fire 4"	},
	{"P3 Button 5",		BIT_DIGITAL,	PCEJoy3 + 10,	"p3 fire 5"	},
	{"P3 Button 6",		BIT_DIGITAL,	PCEJoy3 + 11,	"p3 fire 6"	},

	{"P4 Start",		BIT_DIGITAL,	PCEJoy4 + 3,	"p4 start"	}, // 36
	{"P4 Select",		BIT_DIGITAL,	PCEJoy4 + 2,	"p4 select"	},
	{"P4 Up",			BIT_DIGITAL,	PCEJoy4 + 4,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	PCEJoy4 + 6,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	PCEJoy4 + 7,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	PCEJoy4 + 5,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	PCEJoy4 + 1,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	PCEJoy4 + 0,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	PCEJoy4 + 8,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	PCEJoy4 + 9,	"p4 fire 4"	},
	{"P4 Button 5",		BIT_DIGITAL,	PCEJoy4 + 10,	"p4 fire 5"	},
	{"P4 Button 6",		BIT_DIGITAL,	PCEJoy4 + 11,	"p4 fire 6"	},

	{"P5 Start",		BIT_DIGITAL,	PCEJoy5 + 3,	"p5 start"	}, // 48
	{"P5 Select",		BIT_DIGITAL,	PCEJoy5 + 2,	"p5 select"	},
	{"P5 Up",			BIT_DIGITAL,	PCEJoy5 + 4,	"p5 up"		},
	{"P5 Down",			BIT_DIGITAL,	PCEJoy5 + 6,	"p5 down"	},
	{"P5 Left",			BIT_DIGITAL,	PCEJoy5 + 7,	"p5 left"	},
	{"P5 Right",		BIT_DIGITAL,	PCEJoy5 + 5,	"p5 right"	},
	{"P5 Button 1",		BIT_DIGITAL,	PCEJoy5 + 1,	"p5 fire 1"	},
	{"P5 Button 2",		BIT_DIGITAL,	PCEJoy5 + 0,	"p5 fire 2"	},
	{"P5 Button 3",		BIT_DIGITAL,	PCEJoy5 + 8,	"p5 fire 3"	},
	{"P5 Button 4",		BIT_DIGITAL,	PCEJoy5 + 9,	"p5 fire 4"	},
	{"P5 Button 5",		BIT_DIGITAL,	PCEJoy5 + 10,	"p5 fire 5"	},
	{"P5 Button 6",		BIT_DIGITAL,	PCEJoy5 + 11,	"p5 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&PCEReset,		"reset"		}, // 60
	{"Dip A",			BIT_DIPSWITCH,	PCEDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	PCEDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	PCEDips + 2,	"dip"		},
};

STDINPUTINFO(pce)

static struct BurnDIPInfo pceDIPList[] =
{
	DIP_OFFSET(0x3d)

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 1"	},
	{0x00, 0x01, 0x03, 0x00, "2-buttons"				},
	{0x00, 0x01, 0x03, 0x02, "6-buttons"				},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 2"	},
	{0x00, 0x01, 0x0c, 0x00, "2-buttons"				},
	{0x00, 0x01, 0x0c, 0x08, "6-buttons"				},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 3"	},
	{0x00, 0x01, 0x30, 0x00, "2-buttons"				},
	{0x00, 0x01, 0x30, 0x20, "6-buttons"				},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 4"	},
	{0x00, 0x01, 0xc0, 0x00, "2-buttons"				},
	{0x00, 0x01, 0xc0, 0x80, "6-buttons"				},

	{0   , 0xfe, 0   ,    2, "Joystick Type Player 5"	},
	{0x01, 0x01, 0x03, 0x00, "2-buttons"				},
	{0x01, 0x01, 0x03, 0x02, "6-buttons"				},
#if 0
	// not supported yet..
	{0   , 0xfe, 0   ,    2, "Arcade Card"				},
	{0x02, 0x01, 0x01, 0x00, "Off"						},
	{0x02, 0x01, 0x01, 0x01, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "Sprite Limit"				},
	{0x02, 0x01, 0x10, 0x10, "Disabled (hack)"			},
	{0x02, 0x01, 0x10, 0x00, "Enabled"					},

	{0   , 0xfe, 0   ,    2, "Alt Palette"				},
	{0x02, 0x01, 0x20, 0x00, "Disabled"					},
	{0x02, 0x01, 0x20, 0x20, "Enabled"					},

	{0   , 0xfe, 0   ,    2, "Sound Synthesis"			},
	{0x02, 0x01, 0x80, 0x00, "LQ (Low CPU Usage)"		},
	{0x02, 0x01, 0x80, 0x80, "HQ (High CPU Usage)"		},
};

static struct BurnDIPInfo pcedefaultsDIPList[] =
{
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},
	{0x02, 0xff, 0xff, 0x01, NULL						},
};

static struct BurnDIPInfo pceHQSoundDIPList[] =
{
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},
	{0x02, 0xff, 0xff, 0x81, NULL						},
};

STDDIPINFOEXT(pce, pcedefaults, pce)
STDDIPINFOEXT(pce_hq_sound, pceHQSound, pce)


// -----------------------
// CD-Rom System Firmwares
// -----------------------


// CD-Rom System Card (v1.0)

static struct BurnRomInfo pce_cdsysbRomDesc[] = {
	{ "CD-Rom System Card (Japan, v1.0)(1988).pce", 0x040000, 0x3f9f95a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cdsysb)
STD_ROM_FN(pce_cdsysb)

struct BurnDriverD BurnDrvpce_cdsysb = {
	"pce_cdsysb", NULL, NULL, NULL, "1988",
	"CD-Rom System Card (v1.0)\0", NULL, "NEC - Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BIOS, 0,
	PceGetZipName, pce_cdsysbRomInfo, pce_cdsysbRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// CD-Rom System Card (v2.0)

static struct BurnRomInfo pce_cdsysaRomDesc[] = {
	{ "CD-Rom System Card (Japan, v2.0)(1989).pce", 0x040000, 0x52520bc6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cdsysa)
STD_ROM_FN(pce_cdsysa)

struct BurnDriverD BurnDrvpce_cdsysa = {
	"pce_cdsysa", "pce_cdsys", NULL, NULL, "1989",
	"CD-Rom System Card (v2.0)\0", NULL, "NEC - Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BIOS, 0,
	PceGetZipName, pce_cdsysaRomInfo, pce_cdsysaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// CD-Rom System Card (v2.1)

static struct BurnRomInfo pce_cdsysRomDesc[] = {
	{ "CD-Rom System Card (Japan, v2.1)(1989).pce", 0x040000, 0x283b74e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cdsys)
STD_ROM_FN(pce_cdsys)

struct BurnDriverD BurnDrvpce_cdsys = {
	"pce_cdsys", NULL, NULL, NULL, "1989",
	"CD-Rom System Card (v2.1)\0", NULL, "NEC - Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BIOS, 0,
	PceGetZipName, pce_cdsysRomInfo, pce_cdsysRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Games Express CD Card

static struct BurnRomInfo pce_gecdRomDesc[] = {
	{ "Games Express CD Card (Japan)(1993).pce", 0x008000, 0x51a12d90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gecd)
STD_ROM_FN(pce_gecd)

struct BurnDriverD BurnDrvpce_gecd = {
	"pce_gecd", NULL, NULL, NULL, "1993",
	"Games Express CD Card\0", NULL, "Games Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BIOS, 0,
	PceGetZipName, pce_gecdRomInfo, pce_gecdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Super CD-Rom System Card (v3.0)

static struct BurnRomInfo pce_scdsysRomDesc[] = {
	{ "Super CD-Rom System Card (Japan, v3.0)(1991).pce", 0x040000, 0x6d9a73ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_scdsys)
STD_ROM_FN(pce_scdsys)

struct BurnDriverD BurnDrvpce_scdsys = {
	"pce_scdsys", NULL, NULL, NULL, "1991",
	"Super CD-Rom System Card (v3.0)\0", NULL, "NEC - Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BIOS, 0,
	PceGetZipName, pce_scdsysRomInfo, pce_scdsysRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// TurboGrafx CD Super System Card (v3.0)

static struct BurnRomInfo tg_scdsysRomDesc[] = {
	{ "TurboGrafx CD System Card (USA, v3.0)(1992).pce", 0x040000, 0x2b5b75fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_scdsys)
STD_ROM_FN(tg_scdsys)

struct BurnDriverD BurnDrvtg_scdsys = {
	"tg_scdsys", NULL, NULL, NULL, "1992",
	"TurboGrafx CD Super System Card (v3.0)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_TG16, GBF_BIOS, 0,
	TgGetZipName, tg_scdsysRomInfo, tg_scdsysRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// TurboGrafx CD System Card (v2.0)

static struct BurnRomInfo tg_cdsysRomDesc[] = {
	{ "TurboGrafx CD System Card (USA, v2.0)(1989).pce", 0x040000, 0xff2a5ec3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_cdsys)
STD_ROM_FN(tg_cdsys)

struct BurnDriverD BurnDrvtg_cdsys = {
	"tg_cdsys", NULL, NULL, NULL, "1989",
	"TurboGrafx CD System Card (v2.0)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	0, 1, HARDWARE_PCENGINE_TG16, GBF_BIOS, 0,
	TgGetZipName, tg_cdsysRomInfo, tg_cdsysRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// ---------------
// PC-Engine Games
// ---------------


// 1943 Kai (Japan)

static struct BurnRomInfo pce_1943kaiRomDesc[] = {
	{ "1943 kai (japan).pce", 0x080000, 0xfde08d6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_1943kai)
STD_ROM_FN(pce_1943kai)

struct BurnDriver BurnDrvpce_1943kai = {
	"pce_1943kai", NULL, NULL, NULL, "1991",
	"1943 Kai (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_1943kaiRomInfo, pce_1943kaiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// 21 Emon - Mezase Hotel ou!! (Japan)

static struct BurnRomInfo pce_21emonRomDesc[] = {
	{ "21 emon - mezase hotel ou!! (japan).pce", 0x080000, 0x73614660, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_21emon)
STD_ROM_FN(pce_21emon)

struct BurnDriver BurnDrvpce_21emon = {
	"pce_21emon", NULL, NULL, NULL, "1994",
	"21 Emon - Mezase Hotel ou!! (Japan)\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pce_21emonRomInfo, pce_21emonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Adventure Island (Japan)

static struct BurnRomInfo pce_advislndRomDesc[] = {
	{ "adventure island (japan).pce", 0x040000, 0x8e71d4f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_advislnd)
STD_ROM_FN(pce_advislnd)

struct BurnDriver BurnDrvpce_advislnd = {
	"pce_advislnd", NULL, NULL, NULL, "1991",
	"Adventure Island (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_ADV, 0,
	PceGetZipName, pce_advislndRomInfo, pce_advislndRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Aero Blasters (Japan)

static struct BurnRomInfo pce_aeroblstRomDesc[] = {
	{ "aero blasters (japan).pce", 0x080000, 0x25be2b81, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_aeroblst)
STD_ROM_FN(pce_aeroblst)

struct BurnDriver BurnDrvpce_aeroblst = {
	"pce_aeroblst", NULL, NULL, NULL, "1990",
	"Aero Blasters (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_aeroblstRomInfo, pce_aeroblstRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// After Burner II (Japan)

static struct BurnRomInfo pce_aburner2RomDesc[] = {
	{ "after burner ii (japan).pce", 0x080000, 0xca72a828, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_aburner2)
STD_ROM_FN(pce_aburner2)

struct BurnDriver BurnDrvpce_aburner2 = {
	"pce_aburner2", NULL, NULL, NULL, "1990",
	"After Burner II (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_aburner2RomInfo, pce_aburner2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 232, 4, 3
};


// Alien Crush (Japan)

static struct BurnRomInfo pce_acrushRomDesc[] = {
	{ "Alien Crush (Japan)(1988)(Naxat Soft).pce", 0x040000, 0x60edf4e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_acrush)
STD_ROM_FN(pce_acrush)

struct BurnDriver BurnDrvpce_acrush = {
	"pce_acrush", NULL, NULL, NULL, "1988",
	"Alien Crush (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PINBALL, 0,
	PceGetZipName, pce_acrushRomInfo, pce_acrushRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ankoku Densetsu (Japan)

static struct BurnRomInfo pce_ankokuRomDesc[] = {
	{ "Ankoku Densetsu (Japan)(1990)(Victor).pce", 0x040000, 0xcacc06fb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ankoku)
STD_ROM_FN(pce_ankoku)

struct BurnDriver BurnDrvpce_ankoku = {
	"pce_ankoku", NULL, NULL, NULL, "1990",
	"Ankoku Densetsu (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_ankokuRomInfo, pce_ankokuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Aoi Blink (Japan)

static struct BurnRomInfo pce_aoiblinkRomDesc[] = {
	{ "Aoi Blink (Japan)(1990)(Hudson Soft).pce", 0x060000, 0x08a09b9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_aoiblink)
STD_ROM_FN(pce_aoiblink)

struct BurnDriver BurnDrvpce_aoiblink = {
	"pce_aoiblink", NULL, NULL, NULL, "1990",
	"Aoi Blink (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_aoiblinkRomInfo, pce_aoiblinkRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Blue Blink (Hack, English v0.99b)
// https://www.romhacking.net/translations/504/
static struct BurnRomInfo pce_aoiblinkeRomDesc[] = {
	{ "Blue Blink T-Eng v0.99b (2022)(Gaijin Productions).pce", 0x060000, 0x25d34a20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_aoiblinke)
STD_ROM_FN(pce_aoiblinke)

struct BurnDriver BurnDrvpce_aoiblinke = {
	"pce_aoiblinke", "pce_aoiblink", NULL, NULL, "2022",
	"Blue Blink (Hack, English v0.99b)\0", NULL, "Gaijin Productions", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_aoiblinkeRomInfo, pce_aoiblinkeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Appare! Gateball (Japan)

static struct BurnRomInfo pce_appgatebRomDesc[] = {
	{ "Appare! Gateball (Japan)(1988)(Hudson Soft).pce", 0x040000, 0x2b54cba2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_appgateb)
STD_ROM_FN(pce_appgateb)

struct BurnDriver BurnDrvpce_appgateb = {
	"pce_appgateb", NULL, NULL, NULL, "1988",
	"Appare! Gateball (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_appgatebRomInfo, pce_appgatebRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Armed Formation F (Japan)

static struct BurnRomInfo pce_armedfRomDesc[] = {
	{ "Armed Formation F (Japan)(1990)(Big Don).pce", 0x040000, 0x20ef87fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_armedf)
STD_ROM_FN(pce_armedf)

struct BurnDriver BurnDrvpce_armedf = {
	"pce_armedf", NULL, NULL, NULL, "1990",
	"Armed Formation F (Japan)\0", NULL, "Big Don", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_armedfRomInfo, pce_armedfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Artist Tool (Japan)

static struct BurnRomInfo pce_arttoolRomDesc[] = {
	{ "Artist Tool (Japan)(1989)(NEC Home Electronics).pce", 0x040000, 0x5e4fa713, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_arttool)
STD_ROM_FN(pce_arttool)

struct BurnDriver BurnDrvpce_arttool = {
	"pce_arttool", NULL, NULL, NULL, "1989",
	"Artist Tool (Japan)\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pce_arttoolRomInfo, pce_arttoolRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Atomic Robo-kid Special (Japan)

static struct BurnRomInfo pce_atomroboRomDesc[] = {
	{ "Atomic Robo-kid Special (Japan)(1989)(UPL).pce", 0x080000, 0xdd175efd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_atomrobo)
STD_ROM_FN(pce_atomrobo)

struct BurnDriver BurnDrvpce_atomrobo = {
	"pce_atomrobo", NULL, NULL, NULL, "1989",
	"Atomic Robo-kid Special (Japan)\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_atomroboRomInfo, pce_atomroboRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// AV Poker (Japan)

static struct BurnRomInfo pce_avpokerRomDesc[] = {
	{ "AV Poker (Japan)(1992)(Game Express).pce", 0x080000, 0xb866d282, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_avpoker)
STD_ROM_FN(pce_avpoker)

struct BurnDriver BurnDrvpce_avpoker = {
	"pce_avpoker", NULL, NULL, NULL, "1992",
	"AV Poker (Japan)\0", NULL, "Games Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_CASINO | GBF_CARD, 0,
	PceGetZipName, pce_avpokerRomInfo, pce_avpokerRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ballistix (Japan)

static struct BurnRomInfo pce_ballistxRomDesc[] = {
	{ "Ballistix (Japan)(1991)(Coconuts Japan).pce", 0x040000, 0x8acfc8aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ballistx)
STD_ROM_FN(pce_ballistx)

struct BurnDriver BurnDrvpce_ballistx = {
	"pce_ballistx", NULL, NULL, NULL, "1991",
	"Ballistix (Japan)\0", NULL, "Coconuts Japan - Psygnosis", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_ballistxRomInfo, pce_ballistxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bari Bari Densetsu (Japan)

static struct BurnRomInfo pce_baribariRomDesc[] = {
	{ "Bari Bari Densetsu (Japan)(1989)(Taito).pce", 0x060000, 0xc267e25d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_baribari)
STD_ROM_FN(pce_baribari)

struct BurnDriver BurnDrvpce_baribari = {
	"pce_baribari", NULL, NULL, NULL, "1989",
	"Bari Bari Densetsu (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_baribariRomInfo, pce_baribariRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Barunba (Japan)

static struct BurnRomInfo pce_barunbaRomDesc[] = {
	{ "Barunba (Japan)(1989)(Namcot).pce", 0x080000, 0x4a3df3ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_barunba)
STD_ROM_FN(pce_barunba)

struct BurnDriver BurnDrvpce_barunba = {
	"pce_barunba", NULL, NULL, NULL, "1989",
	"Barunba (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_barunbaRomInfo, pce_barunbaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Batman (Japan)

static struct BurnRomInfo pce_batmanRomDesc[] = {
	{ "Batman (Japan)(1990)(Sunsoft).pce", 0x060000, 0x106bb7b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_batman)
STD_ROM_FN(pce_batman)

struct BurnDriver BurnDrvpce_batman = {
	"pce_batman", NULL, NULL, NULL, "1990",
	"Batman (Japan)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_batmanRomInfo, pce_batmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Battle Lode Runner (Japan)

static struct BurnRomInfo pce_batloderRomDesc[] = {
	{ "Battle Lode Runner (Japan)(1993)(Hudson Soft).pce", 0x040000, 0x59e44f45, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_batloder)
STD_ROM_FN(pce_batloder)

struct BurnDriver BurnDrvpce_batloder = {
	"pce_batloder", NULL, NULL, NULL, "1993",
	"Battle Lode Runner (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_batloderRomInfo, pce_batloderRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Be Ball (Japan)

static struct BurnRomInfo pce_beballRomDesc[] = {
	{ "Be Ball (Japan)(1990)(Hudson Soft).pce", 0x040000, 0x261f1013, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_beball)
STD_ROM_FN(pce_beball)

struct BurnDriver BurnDrvpce_beball = {
	"pce_beball", NULL, NULL, NULL, "1990",
	"Be Ball (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_MAZE | GBF_ACTION, 0,
	PceGetZipName, pce_beballRomInfo, pce_beballRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Benkei Gaiden (Japan)

static struct BurnRomInfo pce_benkeiRomDesc[] = {
	{ "Benkei Gaiden (Japan)(1989)(Sunsoft).pce", 0x060000, 0xe1a73797, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_benkei)
STD_ROM_FN(pce_benkei)

struct BurnDriver BurnDrvpce_benkei = {
	"pce_benkei", NULL, NULL, NULL, "1989",
	"Benkei Gaiden (Japan)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_benkeiRomInfo, pce_benkeiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Benkei Gaiden (Japan, Alt)

static struct BurnRomInfo pce_benkei1RomDesc[] = {
	{ "Benkei Gaiden (Japan, Alt)(1989)(Sunsoft).pce", 0x060000, 0xc9626a43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_benkei1)
STD_ROM_FN(pce_benkei1)

struct BurnDriver BurnDrvpce_benkei1 = {
	"pce_benkei1", "pce_benkei", NULL, NULL, "1989",
	"Benkei Gaiden (Japan, Alt)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_benkei1RomInfo, pce_benkei1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bikkuriman World (Japan)

static struct BurnRomInfo pce_bikkuriRomDesc[] = {
	{ "Bikkuriman World (Japan)(1987)(Hudson Soft).pce", 0x040000, 0x2841fd1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bikkuri)
STD_ROM_FN(pce_bikkuri)

struct BurnDriver BurnDrvpce_bikkuri = {
	"pce_bikkuri", NULL, NULL, NULL, "1987",
	"Bikkuriman World (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_bikkuriRomInfo, pce_bikkuriRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bikkuriman World (Japan, Alt)

static struct BurnRomInfo pce_bikkuri1RomDesc[] = {
	{ "Bikkuriman World (Japan, Alt)(1987)(Hudson Soft).pce", 0x040000, 0x34893891, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bikkuri1)
STD_ROM_FN(pce_bikkuri1)

struct BurnDriver BurnDrvpce_bikkuri1 = {
	"pce_bikkuri1", "pce_bikkuri", NULL, NULL, "1987",
	"Bikkuriman World (Japan, Alt)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_bikkuri1RomInfo, pce_bikkuri1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Blodia (Japan)

static struct BurnRomInfo pce_blodiaRomDesc[] = {
	{ "Blodia (Japan)(1990)(Hudson Soft).pce", 0x020000, 0x958bcd09, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_blodia)
STD_ROM_FN(pce_blodia)

struct BurnDriver BurnDrvpce_blodia = {
	"pce_blodia", NULL, NULL, NULL, "1990",
	"Blodia (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_blodiaRomInfo, pce_blodiaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Body Conquest II (Japan)

static struct BurnRomInfo pce_bodycon2RomDesc[] = {
	{ "Body Conquest II (Japan)(1993)(Game Express).pce", 0x080000, 0xffd92458, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bodycon2)
STD_ROM_FN(pce_bodycon2)

struct BurnDriver BurnDrvpce_bodycon2 = {
	"pce_bodycon2", NULL, NULL, NULL, "1993",
	"Body Conquest II (Japan)\0", NULL, "Games Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_bodycon2RomInfo, pce_bodycon2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman '93 (Japan)

static struct BurnRomInfo pce_bombmn93RomDesc[] = {
	{ "Bomberman '93 (Japan)(1992)(Hudson Soft).pce", 0x080000, 0xb300c5d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bombmn93)
STD_ROM_FN(pce_bombmn93)

struct BurnDriver BurnDrvpce_bombmn93 = {
	"pce_bombmn93", NULL, NULL, NULL, "1992",
	"Bomberman '93 (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_bombmn93RomInfo, pce_bombmn93RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman '93 - Special Version (Japan)

static struct BurnRomInfo pce_bombmn93sRomDesc[] = {
	{ "Bomberman '93 - Special Version (Japan)(1992)(Hudson Soft).pce", 0x080000, 0x02309aa0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bombmn93s)
STD_ROM_FN(pce_bombmn93s)

struct BurnDriver BurnDrvpce_bombmn93s = {
	"pce_bombmn93s", "pce_bombmn93", NULL, NULL, "1992",
	"Bomberman '93 - Special Version (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_bombmn93sRomInfo, pce_bombmn93sRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman '94 (Japan)

static struct BurnRomInfo pce_bombmn94RomDesc[] = {
	{ "Bomberman '94 (Japan)(1993)(Hudson Soft).pce", 0x100000, 0x05362516, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bombmn94)
STD_ROM_FN(pce_bombmn94)

struct BurnDriver BurnDrvpce_bombmn94 = {
	"pce_bombmn94", NULL, NULL, NULL, "1993",
	"Bomberman '94 (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_bombmn94RomInfo, pce_bombmn94RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman (Japan)

static struct BurnRomInfo pce_bombmanRomDesc[] = {
	{ "Bomberman (Japan)(1990)(Hudson Soft).pce", 0x040000, 0x9abb4d1f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bombman)
STD_ROM_FN(pce_bombman)

struct BurnDriver BurnDrvpce_bombman = {
	"pce_bombman", NULL, NULL, NULL, "1990",
	"Bomberman (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_bombmanRomInfo, pce_bombmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman - Users Battle (Japan)

static struct BurnRomInfo pce_bombmnubRomDesc[] = {
	{ "Bomberman - Users Battle (Japan)(1990)(Hudson Soft).pce", 0x040000, 0x1489fa51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bombmnub)
STD_ROM_FN(pce_bombmnub)

struct BurnDriver BurnDrvpce_bombmnub = {
	"pce_bombmnub", NULL, NULL, NULL, "1990",
	"Bomberman - Users Battle (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_bombmnubRomInfo, pce_bombmnubRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bouken Danshaku Don - The Lost Sunheart (Japan)

static struct BurnRomInfo pce_lostsunhRomDesc[] = {
	{ "Bouken Danshaku Don - The Lost Sunheart (Japan)(1992)(I'Max).pce", 0x080000, 0x8f4d9f94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_lostsunh)
STD_ROM_FN(pce_lostsunh)

struct BurnDriver BurnDrvpce_lostsunh = {
	"pce_lostsunh", NULL, NULL, NULL, "1992",
	"Bouken Danshaku Don - The Lost Sunheart (Japan)\0", NULL, "I'Max", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_lostsunhRomInfo, pce_lostsunhRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Break In (Japan)

static struct BurnRomInfo pce_breakinRomDesc[] = {
	{ "Break In (Japan)(1989)(Naxat Soft).pce", 0x040000, 0xc9d7426a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_breakin)
STD_ROM_FN(pce_breakin)

struct BurnDriver BurnDrvpce_breakin = {
	"pce_breakin", NULL, NULL, NULL, "1989",
	"Break In (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_breakinRomInfo, pce_breakinRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bubblegum Crash! - Knight Sabers 2034 (Japan)

static struct BurnRomInfo pce_bubblegmRomDesc[] = {
	{ "Bubblegum Crash! - Knight Sabers 2034 (Japan)(1991)(Naxat Soft).pce", 0x080000, 0x0d766139, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bubblegm)
STD_ROM_FN(pce_bubblegm)

struct BurnDriver BurnDrvpce_bubblegm = {
	"pce_bubblegm", NULL, NULL, NULL, "1991",
	"Bubblegum Crash! - Knight Sabers 2034 (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_bubblegmRomInfo, pce_bubblegmRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bubblegum Crash! - Knight Sabers 2034 (Hack, English)
// https://www.romhacking.net/translations/1234/
static struct BurnRomInfo pce_bubblegmeRomDesc[] = {
	{ "Bubblegum Crash! - Knight Sabers 2034 T-Eng (2022)(Dave Shadoff).pce", 0x0c0000, 0x704b6916, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bubblegme)
STD_ROM_FN(pce_bubblegme)

struct BurnDriver BurnDrvpce_bubblegme = {
	"pce_bubblegme", "pce_bubblegm", NULL, NULL, "2022",
	"Bubblegum Crash! - Knight Sabers 2034 (Hack, English)\0", NULL, "Dave Shadoff, filler", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_bubblegmeRomInfo, pce_bubblegmeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bull Fight: Ring no Haja (Japan)

static struct BurnRomInfo pce_bullfghtRomDesc[] = {
	{ "Bull Fight - Ring no Haja (Japan)(1989)(Cream).pce", 0x060000, 0x5c4d1991, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bullfght)
STD_ROM_FN(pce_bullfght)

struct BurnDriver BurnDrvpce_bullfght = {
	"pce_bullfght", NULL, NULL, NULL, "1989",
	"Bull Fight: Ring no Haja (Japan)\0", NULL, "Cream Co.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_bullfghtRomInfo, pce_bullfghtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Burning Angels (Japan)

static struct BurnRomInfo pce_burnanglRomDesc[] = {
	{ "Burning Angels (Japan)(1990)(Naxat Soft).pce", 0x060000, 0xd233c05a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_burnangl)
STD_ROM_FN(pce_burnangl)

struct BurnDriver BurnDrvpce_burnangl = {
	"pce_burnangl", NULL, NULL, NULL, "1990",
	"Burning Angels (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_burnanglRomInfo, pce_burnanglRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Busou Keiji: Cyber Cross (Japan)

static struct BurnRomInfo pce_cyberxRomDesc[] = {
	{ "Busou Keiji - Cyber Cross (Japan)(1989)(Face).pce", 0x060000, 0xd0c250ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cyberx)
STD_ROM_FN(pce_cyberx)

struct BurnDriver BurnDrvpce_cyberx = {
	"pce_cyberx", NULL, NULL, NULL, "1989",
	"Busou Keiji: Cyber Cross (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	PceGetZipName, pce_cyberxRomInfo, pce_cyberxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cadash (Japan)

static struct BurnRomInfo pce_cadashRomDesc[] = {
	{ "Cadash (Japan)(1990)(Taito).pce", 0x060000, 0x8dc0d85f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cadash)
STD_ROM_FN(pce_cadash)

struct BurnDriver BurnDrvpce_cadash = {
	"pce_cadash", NULL, NULL, NULL, "1990",
	"Cadash (Japan)\0", "Bad graphics", "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_RPG, 0,
	PceGetZipName, pce_cadashRomInfo, pce_cadashRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Champion Wrestler (Japan)

static struct BurnRomInfo pce_champwrsRomDesc[] = {
	{ "Champion Wrestler (Japan)(1990)(Taito).pce", 0x060000, 0x9edc0aea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_champwrs)
STD_ROM_FN(pce_champwrs)

struct BurnDriver BurnDrvpce_champwrs = {
	"pce_champwrs", NULL, NULL, NULL, "1990",
	"Champion Wrestler (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_champwrsRomInfo, pce_champwrsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chibi Maruko Chan - Quiz de Piihyara (Japan)

static struct BurnRomInfo pce_chibimRomDesc[] = {
	{ "Chibi Maruko Chan - Quiz de Piihyara (Japan)(1991)(Namcot).pce", 0x080000, 0x951ed380, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_chibim)
STD_ROM_FN(pce_chibim)

struct BurnDriver BurnDrvpce_chibim = {
	"pce_chibim", NULL, NULL, NULL, "1991",
	"Chibi Maruko Chan - Quiz de Piihyara (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_QUIZ, 0,
	PceGetZipName, pce_chibimRomInfo, pce_chibimRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chikudenya Toubei - Kubikiri Yakata Yori (Japan)

static struct BurnRomInfo pce_chikudenRomDesc[] = {
	{ "Chikudenya Toubei - Kubikiri Yakata Yori (Japan)(1990)(Naxat Soft).pce", 0x080000, 0xcab21b2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_chikuden)
STD_ROM_FN(pce_chikuden)

struct BurnDriver BurnDrvpce_chikuden = {
	"pce_chikuden", NULL, NULL, NULL, "1990",
	"Chikudenya Toubei - Kubikiri Yakata Yori (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_chikudenRomInfo, pce_chikudenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chikudenya Toubei - Kubikiri Yakata Yori (Japan, Alt)

static struct BurnRomInfo pce_chikuden1RomDesc[] = {
	{ "Chikudenya Toubei - Kubikiri Yakata Yori (Japan, Alt)(1990)(Naxat Soft).pce", 0x080000, 0x84098884, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_chikuden1)
STD_ROM_FN(pce_chikuden1)

struct BurnDriver BurnDrvpce_chikuden1 = {
	"pce_chikuden1", "pce_chikuden", NULL, NULL, "1990",
	"Chikudenya Toubei - Kubikiri Yakata Yori (Japan, Alt)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_chikuden1RomInfo, pce_chikuden1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chouzetsu Rinjin - Bravoman (Japan)

static struct BurnRomInfo pce_bravomanRomDesc[] = {
	{ "Chouzetsu Rinjin - Bravoman (Japan)(1990)(Namcot).pce", 0x080000, 0x0df57c90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bravoman)
STD_ROM_FN(pce_bravoman)

struct BurnDriver BurnDrvpce_bravoman = {
	"pce_bravoman", NULL, NULL, NULL, "1990",
	"Chouzetsu Rinjin - Bravoman (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_bravomanRomInfo, pce_bravomanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chouzetsu Rinjin - Bravoman (Hack, English)
// https://www.romhacking.net/translations/7192/
static struct BurnRomInfo pce_bravomaneRomDesc[] = {
	{ "Chouzetsu Rinjin - Bravoman - T-Eng (2024)(Pennywise).pce", 1048576, 0x7413aebd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bravomane)
STD_ROM_FN(pce_bravomane)

struct BurnDriver BurnDrvpce_bravomane = {
	"pce_bravomane", "pce_bravoman", NULL, NULL, "2024",
	"Chouzetsu Rinjin - Bravoman (Hack, English)\0", NULL, "Pennywise", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_bravomaneRomInfo, pce_bravomaneRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Circus Lido (Japan)

static struct BurnRomInfo pce_circusldRomDesc[] = {
	{ "Circus Lido (Japan)(1991)(Unipost).pce", 0x040000, 0xc3212c24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_circusld)
STD_ROM_FN(pce_circusld)

struct BurnDriver BurnDrvpce_circusld = {
	"pce_circusld", NULL, NULL, NULL, "1991",
	"Circus Lido (Japan)\0", NULL, "Unipost", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_circusldRomInfo, pce_circusldRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// City Hunter (Japan)

static struct BurnRomInfo pce_cityhuntRomDesc[] = {
	{ "City Hunter (Japan)(1989)(Sunsoft).pce", 0x060000, 0xf91b055f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cityhunt)
STD_ROM_FN(pce_cityhunt)

struct BurnDriver BurnDrvpce_cityhunt = {
	"pce_cityhunt", NULL, NULL, NULL, "1989",
	"City Hunter (Japan)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_cityhuntRomInfo, pce_cityhuntRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// City Hunter (Hack, English v1.1)
// https://www.romhacking.net/translations/4517/
static struct BurnRomInfo pce_cityhunteRomDesc[] = {
	{ "City Hunter T-Eng v1.1 (2022)(filler, cabbage).pce", 0x060000, 0x23f3df33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cityhunte)
STD_ROM_FN(pce_cityhunte)

struct BurnDriver BurnDrvpce_cityhunte = {
	"pce_cityhunte", "pce_cityhunt", NULL, NULL, "2022",
	"City Hunter (Hack, English v1.1)\0", NULL, "filler, cabbage", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_cityhunteRomInfo, pce_cityhunteRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Columns (Japan)

static struct BurnRomInfo pce_columnsRomDesc[] = {
	{ "columns (japan).pce", 0x020000, 0x99f7a572, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_columns)
STD_ROM_FN(pce_columns)

struct BurnDriver BurnDrvpce_columns = {
	"pce_columns", NULL, NULL, NULL, "1991",
	"Columns (Japan)\0", NULL, "Nihon Telenet", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_columnsRomInfo, pce_columnsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Coryoon: Child of Dragon (Japan)

static struct BurnRomInfo pce_coryoonRomDesc[] = {
	{ "Coryoon - Child of Dragon (Japan)(1991)(Naxat Soft).pce", 0x080000, 0xb4d29e3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_coryoon)
STD_ROM_FN(pce_coryoon)

struct BurnDriver BurnDrvpce_coryoon = {
	"pce_coryoon", NULL, NULL, NULL, "1991",
	"Coryoon: Child of Dragon (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_coryoonRomInfo, pce_coryoonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Coryoon: Child of Dragon (Japan, Alt)

static struct BurnRomInfo pce_coryoon1RomDesc[] = {
	{ "Coryoon - Child of Dragon (Japan, Alt)(1991)(Naxat Soft).pce", 0x080000, 0xd5389889, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_coryoon1)
STD_ROM_FN(pce_coryoon1)

struct BurnDriver BurnDrvpce_coryoon1 = {
	"pce_coryoon1", "pce_coryoon", NULL, NULL, "1991",
	"Coryoon: Child of Dragon (Japan, Alt)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_coryoon1RomInfo, pce_coryoon1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cross Wiber: Cyber Combat Police (Japan)

static struct BurnRomInfo pce_xwiberRomDesc[] = {
	{ "Cross Wiber - Cyber Combat Police (Japan)(1990)(Face).pce", 0x080000, 0x2df97bd0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_xwiber)
STD_ROM_FN(pce_xwiber)

struct BurnDriver BurnDrvpce_xwiber = {
	"pce_xwiber", NULL, NULL, NULL, "1990",
	"Cross Wiber: Cyber Combat Police (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_xwiberRomInfo, pce_xwiberRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cyber Core (Japan)

static struct BurnRomInfo pce_cybrcoreRomDesc[] = {
	{ "Cyber Core (Japan)(1990)(IGS).pce", 0x060000, 0xa98d276a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cybrcore)
STD_ROM_FN(pce_cybrcore)

struct BurnDriver BurnDrvpce_cybrcore = {
	"pce_cybrcore", NULL, NULL, NULL, "1990",
	"Cyber Core (Japan)\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_cybrcoreRomInfo, pce_cybrcoreRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cyber Dodge (Japan)

static struct BurnRomInfo pce_cyberdodRomDesc[] = {
	{ "cyber dodge (japan).pce", 0x080000, 0xb5326b16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cyberdod)
STD_ROM_FN(pce_cyberdod)

struct BurnDriver BurnDrvpce_cyberdod = {
	"pce_cyberdod", NULL, NULL, NULL, "1992",
	"Cyber Dodge (Japan)\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_cyberdodRomInfo, pce_cyberdodRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cyber Knight (Japan)

static struct BurnRomInfo pce_cyknightRomDesc[] = {
	{ "cyber knight (japan).pce", 0x080000, 0xa594fac0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cyknight)
STD_ROM_FN(pce_cyknight)

struct BurnDriver BurnDrvpce_cyknight = {
	"pce_cyknight", NULL, NULL, NULL, "1990",
	"Cyber Knight (Japan)\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_cyknightRomInfo, pce_cyknightRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dai Senpu (Japan)

static struct BurnRomInfo pce_daisenpuRomDesc[] = {
	{ "dai senpu (japan).pce", 0x080000, 0x9107bcc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_daisenpu)
STD_ROM_FN(pce_daisenpu)

struct BurnDriver BurnDrvpce_daisenpu = {
	"pce_daisenpu", NULL, NULL, NULL, "1990",
	"Dai Senpu (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_daisenpuRomInfo, pce_daisenpuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Daichi Kun Crisis - Do Natural (Japan)

static struct BurnRomInfo pce_donaturlRomDesc[] = {
	{ "daichi kun crisis - do natural (japan).pce", 0x060000, 0x61a2935f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_donaturl)
STD_ROM_FN(pce_donaturl)

struct BurnDriver BurnDrvpce_donaturl = {
	"pce_donaturl", NULL, NULL, NULL, "1989",
	"Daichi Kun Crisis - Do Natural (Japan)\0", NULL, "Salio", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_donaturlRomInfo, pce_donaturlRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Darius Alpha (Japan)

static struct BurnRomInfo pce_dariusaRomDesc[] = {
	{ "darius alpha (japan).pce", 0x060000, 0xb0ba689f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dariusa)
STD_ROM_FN(pce_dariusa)

struct BurnDriver BurnDrvpce_dariusa = {
	"pce_dariusa", NULL, NULL, NULL, "1990",
	"Darius Alpha (Japan)\0", NULL, "NEC Avenue", "SuperGrafx Enhanced",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_dariusaRomInfo, pce_dariusaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Darius Plus (Japan)

static struct BurnRomInfo pce_dariuspRomDesc[] = {
	{ "darius plus (japan).pce", 0x0c0000, 0xbebfe042, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dariusp)
STD_ROM_FN(pce_dariusp)

struct BurnDriver BurnDrvpce_dariusp = {
	"pce_dariusp", NULL, NULL, NULL, "1990",
	"Darius Plus (Japan)\0", NULL, "NEC Avenue", "SuperGrafx Enhanced",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_dariuspRomInfo, pce_dariuspRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dead Moon - Tsuki Sekai no Akumu (Japan)

static struct BurnRomInfo pce_deadmoonRomDesc[] = {
	{ "Dead Moon - Tsuki Sekai no Akumu (Japan)(1991)(Natsume).pce", 0x080000, 0x56739bc7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_deadmoon)
STD_ROM_FN(pce_deadmoon)

struct BurnDriver BurnDrvpce_deadmoon = {
	"pce_deadmoon", NULL, NULL, NULL, "1991",
	"Dead Moon - Tsuki Sekai no Akumu (Japan)\0", NULL, "Natsume", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_deadmoonRomInfo, pce_deadmoonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Deep Blue - Kaitei Shinwa (Japan)

static struct BurnRomInfo pce_deepblueRomDesc[] = {
	{ "Deep Blue - Kaitei Shinwa (Japan)(1989)(Pack-In-Video).pce", 0x040000, 0x053a0f83, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_deepblue)
STD_ROM_FN(pce_deepblue)

struct BurnDriver BurnDrvpce_deepblue = {
	"pce_deepblue", NULL, NULL, NULL, "1989",
	"Deep Blue - Kaitei Shinwa (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_deepblueRomInfo, pce_deepblueRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Detana!! Twinbee (Japan)

static struct BurnRomInfo pce_twinbeeRomDesc[] = {
	{ "Detana!! Twinbee (Japan)(1992)(Konami).pce", 0x080000, 0x5cf59d80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_twinbee)
STD_ROM_FN(pce_twinbee)

struct BurnDriver BurnDrvpce_twinbee = {
	"pce_twinbee", NULL, NULL, NULL, "1992",
	"Detana!! Twinbee (Japan)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_twinbeeRomInfo, pce_twinbeeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Devil Crash - Naxat Pinball (Japan)

static struct BurnRomInfo pce_devlcrshRomDesc[] = {
	{ "Devil Crash - Naxat Pinball (Japan)(1990)(Naxat Soft).pce", 0x060000, 0x4ec81a80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_devlcrsh)
STD_ROM_FN(pce_devlcrsh)

struct BurnDriver BurnDrvpce_devlcrsh = {
	"pce_devlcrsh", NULL, NULL, NULL, "1990",
	"Devil Crash - Naxat Pinball (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PINBALL, 0,
	PceGetZipName, pce_devlcrshRomInfo, pce_devlcrshRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Die Hard (Japan)

static struct BurnRomInfo pce_diehardRomDesc[] = {
	{ "Die Hard (Japan)(1990)(Pack-In-Video).pce", 0x080000, 0x1b5b1cb1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_diehard)
STD_ROM_FN(pce_diehard)

struct BurnDriver BurnDrvpce_diehard = {
	"pce_diehard", NULL, NULL, NULL, "1990",
	"Die Hard (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_diehardRomInfo, pce_diehardRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Digital Champ (Japan)

static struct BurnRomInfo pce_digichmpRomDesc[] = {
	{ "Digital Champ (Japan)(1989)(Naxat Soft).pce", 0x040000, 0x17ba3032, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_digichmp)
STD_ROM_FN(pce_digichmp)

struct BurnDriver BurnDrvpce_digichmp = {
	"pce_digichmp", NULL, NULL, NULL, "1989",
	"Digital Champ (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_digichmpRomInfo, pce_digichmpRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Don Doko Don! (Japan)

static struct BurnRomInfo pce_dondokoRomDesc[] = {
	{ "Don Doko Don! (Japan)(1990)(Taito).pce", 0x060000, 0xf42aa73e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dondoko)
STD_ROM_FN(pce_dondoko)

struct BurnDriver BurnDrvpce_dondoko = {
	"pce_dondoko", NULL, NULL, NULL, "1990",
	"Don Doko Don! (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_dondokoRomInfo, pce_dondokoRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Doraemon: Meikyuu Dai Sakusen (Japan)

static struct BurnRomInfo pce_doramsRomDesc[] = {
	{ "Doraemon - Meikyuu Dai Sakusen (Japan)(1989)(Hudson Soft).pce", 0x040000, 0xdc760a07, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dorams)
STD_ROM_FN(pce_dorams)

struct BurnDriver BurnDrvpce_dorams = {
	"pce_dorams", NULL, NULL, NULL, "1989",
	"Doraemon: Meikyuu Dai Sakusen (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_MAZE, 0,
	PceGetZipName, pce_doramsRomInfo, pce_doramsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Doraemon: Nobita no Dorabian Night (Japan)

static struct BurnRomInfo pce_dorandnRomDesc[] = {
	{ "Doraemon - Nobita no Dorabian Night (Japan)(1991)(Hudson Soft).pce", 0x080000, 0x013a747f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dorandn)
STD_ROM_FN(pce_dorandn)

struct BurnDriver BurnDrvpce_dorandn = {
	"pce_dorandn", NULL, NULL, NULL, "1991",
	"Doraemon: Nobita no Dorabian Night (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_dorandnRomInfo, pce_dorandnRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Double Dungeons - W (Japan)

static struct BurnRomInfo pce_ddungwRomDesc[] = {
	{ "Double Dungeons - W (Japan)(1989)(NCS - Masaya).pce", 0x040000, 0x86087b39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ddungw)
STD_ROM_FN(pce_ddungw)

struct BurnDriver BurnDrvpce_ddungw = {
	"pce_ddungw", NULL, NULL, NULL, "1989",
	"Double Dungeons - W (Japan)\0", NULL, "NCS - Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RPG | GBF_MAZE, 0,
	PceGetZipName, pce_ddungwRomInfo, pce_ddungwRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Download (Japan)

static struct BurnRomInfo pce_downloadRomDesc[] = {
	{ "Download (Japan)(1990)(NEC Avenue).pce", 0x080000, 0x85101c20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_download)
STD_ROM_FN(pce_download)

struct BurnDriver BurnDrvpce_download = {
	"pce_download", NULL, NULL, NULL, "1990",
	"Download (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_downloadRomInfo, pce_downloadRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Download (Japan, Alt)

static struct BurnRomInfo pce_download1RomDesc[] = {
	{ "Download (Japan, Alt)(1990)(NEC Avenue).pce", 0x080000, 0x4e0de488, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_download1)
STD_ROM_FN(pce_download1)

struct BurnDriver BurnDrvpce_download1 = {
	"pce_download1", "pce_download", NULL, NULL, "1990",
	"Download (Japan, Alt)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_download1RomInfo, pce_download1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Egg! (Japan)

static struct BurnRomInfo pce_dragneggRomDesc[] = {
	{ "Dragon Egg! (Japan)(1991)(NCS, Masaya).pce", 0x080000, 0x442405d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dragnegg)
STD_ROM_FN(pce_dragnegg)

struct BurnDriver BurnDrvpce_dragnegg = {
	"pce_dragnegg", NULL, NULL, NULL, "1991",
	"Dragon Egg! (Japan)\0", NULL, "NCS, Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_dragneggRomInfo, pce_dragneggRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Egg! (Hack, English)
// https://www.romhacking.net/translations/6405/
static struct BurnRomInfo pce_dragneggeRomDesc[] = {
	{ "Dragon Egg! T-Eng (2022)(onionzoo).pce", 0x080000, 0xbde8ea74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dragnegge)
STD_ROM_FN(pce_dragnegge)

struct BurnDriver BurnDrvpce_dragnegge = {
	"pce_dragnegge", "pce_dragnegg", NULL, NULL, "2022",
	"Dragon Egg! (Hack, English)\0", NULL, "onionzoo", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_dragneggeRomInfo, pce_dragneggeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Saber: After Story of Dragon Spirit (Japan)

static struct BurnRomInfo pce_dsaberRomDesc[] = {
	{ "Dragon Saber - After Story of Dragon Spirit (Japan)(1991)(Namcot).pce", 0x080000, 0x3219849c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dsaber)
STD_ROM_FN(pce_dsaber)

struct BurnDriver BurnDrvpce_dsaber = {
	"pce_dsaber", NULL, NULL, NULL, "1991",
	"Dragon Saber: After Story of Dragon Spirit (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_dsaberRomInfo, pce_dsaberRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Saber: After Story of Dragon Spirit (Japan, Alt)

static struct BurnRomInfo pce_dsaber1RomDesc[] = {
	{ "Dragon Saber - After Story of Dragon Spirit (Japan, Alt)(1991)(Namcot).pce", 0x080000, 0xc89ce75a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dsaber1)
STD_ROM_FN(pce_dsaber1)

struct BurnDriver BurnDrvpce_dsaber1 = {
	"pce_dsaber1", "pce_dsaber", NULL, NULL, "1991",
	"Dragon Saber: After Story of Dragon Spirit (Japan, Alt)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_dsaber1RomInfo, pce_dsaber1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Spirit (Japan)

static struct BurnRomInfo pce_dspiritRomDesc[] = {
	{ "Dragon Spirit (Japan)(1988)(Namcot).pce", 0x040000, 0x01a76935, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dspirit)
STD_ROM_FN(pce_dspirit)

struct BurnDriver BurnDrvpce_dspirit = {
	"pce_dspirit", NULL, NULL, NULL, "1988",
	"Dragon Spirit (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_dspiritRomInfo, pce_dspiritRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Drop Rock Hora Hora (Japan)

static struct BurnRomInfo pce_droprockRomDesc[] = {
	{ "Drop Rock Hora Hora (Japan)(1990)(Data East).pce", 0x040000, 0x67ec5ec4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_droprock)
STD_ROM_FN(pce_droprock)

struct BurnDriver BurnDrvpce_droprock = {
	"pce_droprock", NULL, NULL, NULL, "1990",
	"Drop Rock Hora Hora (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BREAKOUT, 0,
	PceGetZipName, pce_droprockRomInfo, pce_droprockRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Drop Rock Hora Hora (Japan, Alt)

static struct BurnRomInfo pce_droprock1RomDesc[] = {
	{ "Drop Rock Hora Hora (Japan, Alt)(1990)(Data East).pce", 0x040000, 0x8e81fcac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_droprock1)
STD_ROM_FN(pce_droprock1)

struct BurnDriver BurnDrvpce_droprock1 = {
	"pce_droprock1", "pce_droprock", NULL, NULL, "1990",
	"Drop Rock Hora Hora (Japan, Alt)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BREAKOUT, 0,
	PceGetZipName, pce_droprock1RomInfo, pce_droprock1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dungeon Explorer (Japan)

static struct BurnRomInfo pce_dungexplRomDesc[] = {
	{ "Dungeon Explorer (Japan)(1989)(Hudson Soft).pce", 0x060000, 0x1b1a80a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dungexpl)
STD_ROM_FN(pce_dungexpl)

struct BurnDriver BurnDrvpce_dungexpl = {
	"pce_dungexpl", NULL, NULL, NULL, "1989",
	"Dungeon Explorer (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAZE | GBF_RUNGUN, 0,
	PceGetZipName, pce_dungexplRomInfo, pce_dungexplRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Energy (Japan)

static struct BurnRomInfo pce_energyRomDesc[] = {
	{ "Energy (Japan)(1989)(NCS).pce", 0x040000, 0xca68ff21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_energy)
STD_ROM_FN(pce_energy)

struct BurnDriver BurnDrvpce_energy = {
	"pce_energy", NULL, NULL, NULL, "1989",
	"Energy (Japan)\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN | GBF_PLATFORM, 0,
	PceGetZipName, pce_energyRomInfo, pce_energyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F-1 Dream (Japan)

static struct BurnRomInfo pce_f1dreamRomDesc[] = {
	{ "F-1 Dream (Japan)(1989)(NEC).pce", 0x040000, 0xd50ff730, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1dream)
STD_ROM_FN(pce_f1dream)

struct BurnDriver BurnDrvpce_f1dream = {
	"pce_f1dream", NULL, NULL, NULL, "1989",
	"F-1 Dream (Japan)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1dreamRomInfo, pce_f1dreamRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F-1 Pilot: You're King of Kings (Japan)

static struct BurnRomInfo pce_f1pilotRomDesc[] = {
	{ "F-1 Pilot - You're King of Kings (Japan)(1989)(Pack-In-Video).pce", 0x060000, 0x09048174, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1pilot)
STD_ROM_FN(pce_f1pilot)

struct BurnDriver BurnDrvpce_f1pilot = {
	"pce_f1pilot", NULL, NULL, NULL, "1989",
	"F-1 Pilot: You're King of Kings (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1pilotRomInfo, pce_f1pilotRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F1 Circus '91: World Championship (Japan)

static struct BurnRomInfo pce_f1circ91RomDesc[] = {
	{ "F1 Circus '91 - World Championship (Japan)(1991)(Nihon Bussan).pce", 0x080000, 0xd7cfd70f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1circ91)
STD_ROM_FN(pce_f1circ91)

struct BurnDriver BurnDrvpce_f1circ91 = {
	"pce_f1circ91", NULL, NULL, NULL, "1991",
	"F1 Circus '91: World Championship (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1circ91RomInfo, pce_f1circ91RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F1 Circus '92: The Speed of Sound (Japan)

static struct BurnRomInfo pce_f1circ92RomDesc[] = {
	{ "F1 Circus '92 - The Speed of Sound (Japan)(1992)(Nihon Bussan).pce", 0x0c0000, 0xb268f2a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1circ92)
STD_ROM_FN(pce_f1circ92)

struct BurnDriver BurnDrvpce_f1circ92 = {
	"pce_f1circ92", NULL, NULL, NULL, "1992",
	"F1 Circus '92: The Speed of Sound (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1circ92RomInfo, pce_f1circ92RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F1 Circus (Japan)

static struct BurnRomInfo pce_f1circusRomDesc[] = {
	{ "F1 Circus (Japan)(1990)(Nihon Bussan).pce", 0x080000, 0xe14dee08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1circus)
STD_ROM_FN(pce_f1circus)

struct BurnDriver BurnDrvpce_f1circus = {
	"pce_f1circus", NULL, NULL, NULL, "1990",
	"F1 Circus (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1circusRomInfo, pce_f1circusRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F1 Circus (Japan, Alt)

static struct BurnRomInfo pce_f1circus1RomDesc[] = {
	{ "F1 Circus (Japan, Alt)(1990)(Nihon Bussan).pce", 0x080000, 0x79705779, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1circus1)
STD_ROM_FN(pce_f1circus1)

struct BurnDriver BurnDrvpce_f1circus1 = {
	"pce_f1circus1", "pce_f1circus", NULL, NULL, "1990",
	"F1 Circus (Japan, Alt)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1circus1RomInfo, pce_f1circus1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// F1 Triple Battle (Japan)

static struct BurnRomInfo pce_f1tbRomDesc[] = {
	{ "F1 Triple Battle (Japan)(1989)(Human).pce", 0x060000, 0x13bf0409, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_f1tb)
STD_ROM_FN(pce_f1tb)

struct BurnDriver BurnDrvpce_f1tb = {
	"pce_f1tb", NULL, NULL, NULL, "1989",
	"F1 Triple Battle (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_f1tbRomInfo, pce_f1tbRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fantasy Zone (Japan)

static struct BurnRomInfo pce_fantzoneRomDesc[] = {
	{ "Fantasy Zone (Japan)(1988)(NEC).pce", 0x040000, 0x72cb0f9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fantzone)
STD_ROM_FN(pce_fantzone)

struct BurnDriver BurnDrvpce_fantzone = {
	"pce_fantzone", NULL, NULL, NULL, "1988",
	"Fantasy Zone (Japan)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_fantzoneRomInfo, pce_fantzoneRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fantasy Zone (PC-Engine Mini Ed.)

static struct BurnRomInfo pce_fantzoneminiRomDesc[] = {
	{ "Fantasy Zone (PC-Engine Mini Ed.)(2020)(NEC).pce", 0x080000, 0x4d3f879a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fantzonemini)
STD_ROM_FN(pce_fantzonemini)

struct BurnDriver BurnDrvpce_fantzonemini = {
	"pce_fantzonemini", "pce_fantzone", NULL, NULL, "2020",
	"Fantasy Zone (PC-Engine Mini Ed.)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_fantzoneminiRomInfo, pce_fantzoneminiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fighting Run (Japan)

static struct BurnRomInfo pce_fightrunRomDesc[] = {
	{ "Fighting Run (Japan)(1991)(Nihon Bussan).pce", 0x080000, 0x1828d2e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fightrun)
STD_ROM_FN(pce_fightrun)

struct BurnDriver BurnDrvpce_fightrun = {
	"pce_fightrun", NULL, NULL, NULL, "1991",
	"Fighting Run (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_fightrunRomInfo, pce_fightrunRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Blaster (Japan)

static struct BurnRomInfo pce_finlblstRomDesc[] = {
	{ "Final Blaster (Japan)(1990)(Namcot).pce", 0x060000, 0xc90971ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finlblst)
STD_ROM_FN(pce_finlblst)

struct BurnDriver BurnDrvpce_finlblst = {
	"pce_finlblst", NULL, NULL, NULL, "1990",
	"Final Blaster (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_finlblstRomInfo, pce_finlblstRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Lap Twin (Japan)

static struct BurnRomInfo pce_finallapRomDesc[] = {
	{ "Final Lap Twin (Japan)(1989)(Namcot).pce", 0x060000, 0xc8c084e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finallap)
STD_ROM_FN(pce_finallap)

struct BurnDriver BurnDrvpce_finallap = {
	"pce_finallap", NULL, NULL, NULL, "1989",
	"Final Lap Twin (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_finallapRomInfo, pce_finallapRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Match Tennis (Japan)

static struct BurnRomInfo pce_finalmtRomDesc[] = {
	{ "Final Match Tennis (Japan)(1991)(Human).pce", 0x040000, 0x560d2305, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finalmt)
STD_ROM_FN(pce_finalmt)

struct BurnDriver BurnDrvpce_finalmt = {
	"pce_finalmt", NULL, NULL, NULL, "1991",
	"Final Match Tennis (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_finalmtRomInfo, pce_finalmtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Soldier (Japan)

static struct BurnRomInfo pce_finalsolRomDesc[] = {
	{ "Final Soldier (Japan)(1991)(Hudson Soft).pce", 0x080000, 0xaf2dd2af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finalsol)
STD_ROM_FN(pce_finalsol)

struct BurnDriver BurnDrvpce_finalsol = {
	"pce_finalsol", NULL, NULL, NULL, "1991",
	"Final Soldier (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_finalsolRomInfo, pce_finalsolRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Soldier - Special Version (Japan)

static struct BurnRomInfo pce_finalsolsRomDesc[] = {
	{ "Final Soldier - Special Version (Japan)(1991)(Hudson Soft).pce", 0x080000, 0x02a578c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finalsols)
STD_ROM_FN(pce_finalsols)

struct BurnDriver BurnDrvpce_finalsols = {
	"pce_finalsols", "pce_finalsol", NULL, NULL, "1991",
	"Final Soldier - Special Version (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_finalsolsRomInfo, pce_finalsolsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fire Pro Wrestling - Combination Tag (Japan)

static struct BurnRomInfo pce_fireprowRomDesc[] = {
	{ "Fire Pro Wrestling - Combination Tag (Japan)(1989)(Human).pce", 0x060000, 0x90ed6575, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fireprow)
STD_ROM_FN(pce_fireprow)

struct BurnDriver BurnDrvpce_fireprow = {
	"pce_fireprow", NULL, NULL, NULL, "1989",
	"Fire Pro Wrestling - Combination Tag (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_fireprowRomInfo, pce_fireprowRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fire Pro Wrestling 2 - 2nd Bout (Japan)

static struct BurnRomInfo pce_fireprw2RomDesc[] = {
	{ "Fire Pro Wrestling 2 - 2nd Bout (Japan)(1991)(Human).pce", 0x080000, 0xe88987bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fireprw2)
STD_ROM_FN(pce_fireprw2)

struct BurnDriver BurnDrvpce_fireprw2 = {
	"pce_fireprw2", NULL, NULL, NULL, "1991",
	"Fire Pro Wrestling 2 - 2nd Bout (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_fireprw2RomInfo, pce_fireprw2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fire Pro Wrestling 3 - Legend Bout (Japan)

static struct BurnRomInfo pce_fireprw3RomDesc[] = {
	{ "Fire Pro Wrestling 3 - Legend Bout (Japan)(1992)(Human).pce", 0x100000, 0x534e8808, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fireprw3)
STD_ROM_FN(pce_fireprw3)

struct BurnDriver BurnDrvpce_fireprw3 = {
	"pce_fireprw3", NULL, NULL, NULL, "1992",
	"Fire Pro Wrestling 3 - Legend Bout (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_fireprw3RomInfo, pce_fireprw3RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Formation Soccer - Human Cup '90 (Japan)

static struct BurnRomInfo pce_fsoccr90RomDesc[] = {
	{ "formation soccer - human cup '90 (japan).pce", 0x040000, 0x85a1e7b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fsoccr90)
STD_ROM_FN(pce_fsoccr90)

struct BurnDriver BurnDrvpce_fsoccr90 = {
	"pce_fsoccr90", NULL, NULL, NULL, "1990",
	"Formation Soccer - Human Cup '90 (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSFOOTBALL, 0,
	PceGetZipName, pce_fsoccr90RomInfo, pce_fsoccr90RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Formation Soccer - On J. League (Japan)

static struct BurnRomInfo pce_fsoccerRomDesc[] = {
	{ "formation soccer - on j. league (japan).pce", 0x080000, 0x7146027c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_fsoccer)
STD_ROM_FN(pce_fsoccer)

struct BurnDriver BurnDrvpce_fsoccer = {
	"pce_fsoccer", NULL, NULL, NULL, "1994",
	"Formation Soccer - On J. League (Japan)\0", NULL, "Human", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSFOOTBALL, 0,
	PceGetZipName, pce_fsoccerRomInfo, pce_fsoccerRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fushigi no Yume no Alice (Japan)

static struct BurnRomInfo pce_aliceRomDesc[] = {
	{ "Fushigi no Yume no Alice (Japan)(1990)(Face).pce", 0x060000, 0x12c4e6fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_alice)
STD_ROM_FN(pce_alice)

struct BurnDriver BurnDrvpce_alice = {
	"pce_alice", NULL, NULL, NULL, "1990",
	"Fushigi no Yume no Alice (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_aliceRomInfo, pce_aliceRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gai Flame (Japan)

static struct BurnRomInfo pce_gaiflameRomDesc[] = {
	{ "Gai Flame (Japan)(1990)(NCS).pce", 0x060000, 0x95f90dec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gaiflame)
STD_ROM_FN(pce_gaiflame)

struct BurnDriver BurnDrvpce_gaiflame = {
	"pce_gaiflame", NULL, NULL, NULL, "1990",
	"Gai Flame (Japan)\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG | GBF_STRATEGY, 0,
	PceGetZipName, pce_gaiflameRomInfo, pce_gaiflameRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Gai Flame (Hack, English v0.98)
// https://www.romhacking.net/translations/6770/
static struct BurnRomInfo pce_gaiflameeRomDesc[] = {
	{ "Gai Flame T-Eng v0.98 (2022)(Nebulous Translations).pce", 0x060000, 0x581164ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gaiflamee)
STD_ROM_FN(pce_gaiflamee)

struct BurnDriver BurnDrvpce_gaiflamee = {
	"pce_gaiflamee", "pce_gaiflame", NULL, NULL, "2022",
	"Gai Flame (Hack, English v0.98)\0", NULL, "Nebulous Translations", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG | GBF_STRATEGY, 0,
	PceGetZipName, pce_gaiflameeRomInfo, pce_gaiflameeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gaia no Monshou (Japan)

static struct BurnRomInfo pce_gaiamonsRomDesc[] = {
	{ "gaia no monshou (japan).pce", 0x040000, 0x6fd6827c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gaiamons)
STD_ROM_FN(pce_gaiamons)

struct BurnDriver BurnDrvpce_gaiamons = {
	"pce_gaiamons", NULL, NULL, NULL, "1988",
	"Gaia no Monshou (Japan)\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_gaiamonsRomInfo, pce_gaiamonsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Galaga '88 (Japan)

static struct BurnRomInfo pce_galaga88RomDesc[] = {
	{ "Galaga '88 (Japan)(1988)(Namcot).pce", 0x040000, 0x1a8393c6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_galaga88)
STD_ROM_FN(pce_galaga88)

struct BurnDriver BurnDrvpce_galaga88 = {
	"pce_galaga88", NULL, NULL, NULL, "1988",
	"Galaga '88 (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_galaga88RomInfo, pce_galaga88RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ganbare! Golf Boys (Japan)

static struct BurnRomInfo pce_ganbgolfRomDesc[] = {
	{ "Ganbare! Golf Boys (Japan)(1989)(NCS).pce", 0x040000, 0x27a4d11a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ganbgolf)
STD_ROM_FN(pce_ganbgolf)

struct BurnDriver BurnDrvpce_ganbgolf = {
	"pce_ganbgolf", NULL, NULL, NULL, "1989",
	"Ganbare! Golf Boys (Japan)\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_ganbgolfRomInfo, pce_ganbgolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gekisha Boy (Japan)

static struct BurnRomInfo pce_gekisboyRomDesc[] = {
	{ "Gekisha Boy (Japan)(1992)(Irem).pce", 0x080000, 0xe8702d51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gekisboy)
STD_ROM_FN(pce_gekisboy)

struct BurnDriver BurnDrvpce_gekisboy = {
	"pce_gekisboy", NULL, NULL, NULL, "1992",
	"Gekisha Boy (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_gekisboyRomInfo, pce_gekisboyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pce_hq_soundDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Photograph Boy (Hack, English)
// https://www.romhacking.net/translations/6969/
static struct BurnRomInfo pce_photoboyRomDesc[] = {
	{ "Photograph Boy T-Eng (2023)(LaytonLoztew).pce", 0x080000, 0x61febf33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_photoboy)
STD_ROM_FN(pce_photoboy)

struct BurnDriver BurnDrvpce_photoboy = {
	"pce_photoboy", "pce_gekisboy", NULL, NULL, "2023",
	"Photograph Boy (Hack, English)\0", NULL, "LaytonLoztew", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_photoboyRomInfo, pce_photoboyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pce_hq_soundDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Genji Tsuushin Agedama (Japan)

static struct BurnRomInfo pce_genjitsuRomDesc[] = {
	{ "Genji Tsuushin Agedama (Japan)(1991)(NEC Home Electronics).pce", 0x080000, 0xad450dfc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_genjitsu)
STD_ROM_FN(pce_genjitsu)

struct BurnDriver BurnDrvpce_genjitsu = {
	"pce_genjitsu", NULL, NULL, NULL, "1991",
	"Genji Tsuushin Agedama (Japan)\0", NULL, "NEC Home Electronics", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_HORSHOOT, 0,
	PceGetZipName, pce_genjitsuRomInfo, pce_genjitsuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Genpei Toumaden (Japan)

static struct BurnRomInfo pce_genpeiRomDesc[] = {
	{ "Genpei Toumaden (Japan)(1990)(Namcot).pce", 0x080000, 0xb926c682, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_genpei)
STD_ROM_FN(pce_genpei)

struct BurnDriver BurnDrvpce_genpei = {
	"pce_genpei", NULL, NULL, NULL, "1990",
	"Genpei Toumaden (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_genpeiRomInfo, pce_genpeiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Genpei Toumaden ni no Maki (Japan)

static struct BurnRomInfo pce_genpemakRomDesc[] = {
	{ "Genpei Toumaden ni no Maki (Japan)(1992)(Namcot).pce", 0x080000, 0x8793758c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_genpemak)
STD_ROM_FN(pce_genpemak)

struct BurnDriver BurnDrvpce_genpemak = {
	"pce_genpemak", NULL, NULL, NULL, "1992",
	"Genpei Toumaden ni no Maki (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_genpemakRomInfo, pce_genpemakRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gokuraku! Chuka Taisen (Japan)

static struct BurnRomInfo pce_chukataiRomDesc[] = {
	{ "Gokuraku! Chuka Taisen (Japan)(1992)(Taito).pce", 0x060000, 0xe749a22c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_chukatai)
STD_ROM_FN(pce_chukatai)

struct BurnDriver BurnDrvpce_chukatai = {
	"pce_chukatai", NULL, NULL, NULL, "1992",
	"Gokuraku! Chuka Taisen (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_chukataiRomInfo, pce_chukataiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gomola Speed (Japan)

static struct BurnRomInfo pce_gomolaRomDesc[] = {
	{ "Gomola Speed (Japan)(1990)(UPL).pce", 0x060000, 0x4bd38f17, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gomola)
STD_ROM_FN(pce_gomola)

struct BurnDriver BurnDrvpce_gomola = {
	"pce_gomola", NULL, NULL, NULL, "1990",
	"Gomola Speed (Japan)\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_gomolaRomInfo, pce_gomolaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gradius (Japan)

static struct BurnRomInfo pce_gradiusRomDesc[] = {
	{ "Gradius (Japan)(1991)(Konami).pce", 0x040000, 0x0517da65, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gradius)
STD_ROM_FN(pce_gradius)

struct BurnDriver BurnDrvpce_gradius = {
	"pce_gradius", NULL, NULL, NULL, "1991",
	"Gradius (Japan)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_gradiusRomInfo, pce_gradiusRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gradius (PC-Engine Mini Ed.)

static struct BurnRomInfo pce_gradiusminiRomDesc[] = {
	{ "Gradius (PC-Engine Mini Ed.)(2020)(Konami).pce", 0x080000, 0x4eabdc5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gradiusmini)
STD_ROM_FN(pce_gradiusmini)

struct BurnDriver BurnDrvpce_gradiusmini = {
	"pce_gradiusmini", "pce_gradius", NULL, NULL, "2020",
	"Gradius (PC-Engine Mini Ed.)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_gradiusminiRomInfo, pce_gradiusminiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// GunHed (Japan)

static struct BurnRomInfo pce_gunhedRomDesc[] = {
	{ "GunHed (Japan)(1989)(Hudson Soft).pce", 0x060000, 0xa17d4d7e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gunhed)
STD_ROM_FN(pce_gunhed)

struct BurnDriver BurnDrvpce_gunhed = {
	"pce_gunhed", NULL, NULL, NULL, "1989",
	"GunHed (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_gunhedRomInfo, pce_gunhedRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// GunHed: Hudson GunHed Taikai (Japan)

static struct BurnRomInfo pce_gunhedhtRomDesc[] = {
	{ "GunHed - Hudson GunHed Taikai (Japan)(1989)(Hudson Soft).pce", 0x040000, 0x57f183ae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gunhedht)
STD_ROM_FN(pce_gunhedht)

struct BurnDriver BurnDrvpce_gunhedht = {
	"pce_gunhedht", NULL, NULL, NULL, "1989",
	"GunHed: Hudson GunHed Taikai (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_gunhedhtRomInfo, pce_gunhedhtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hana Taaka Daka!? (Japan)

static struct BurnRomInfo pce_hanatakaRomDesc[] = {
	{ "Hana Taaka Daka! (Japan)(1991)(Taito).pce", 0x080000, 0xba4d0dd4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_hanataka)
STD_ROM_FN(pce_hanataka)

struct BurnDriver BurnDrvpce_hanataka = {
	"pce_hanataka", NULL, NULL, NULL, "1991",
	"Hana Taaka Daka!? (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_hanatakaRomInfo, pce_hanatakaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hanii in the Sky (Japan)

static struct BurnRomInfo pce_haniiskyRomDesc[] = {
	{ "Hanii in the Sky (Japan)(1989)(Face).pce", 0x040000, 0xbf3e2cc7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_haniisky)
STD_ROM_FN(pce_haniisky)

struct BurnDriver BurnDrvpce_haniisky = {
	"pce_haniisky", NULL, NULL, NULL, "1989",
	"Hanii in the Sky (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_haniiskyRomInfo, pce_haniiskyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Honey in the Sky (Hack, English + 3-button controller)
// https://www.romhacking.net/translations/6531/
static struct BurnRomInfo pce_haniiskyeRomDesc[] = {
	{ "Honey in the Sky T-Eng (2022)(filler).pce", 0x040000, 0xd1dc2f66, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_haniiskye)
STD_ROM_FN(pce_haniiskye)

struct BurnDriver BurnDrvpce_haniiskye = {
	"pce_haniiskye", "pce_haniisky", NULL, NULL, "2022",
	"Honey in the Sky (Hack, English + 3-button controller)\0", NULL, "filler, cabbage", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_haniiskyeRomInfo, pce_haniiskyeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hanii on the Road (Japan)

static struct BurnRomInfo pce_haniirodRomDesc[] = {
	{ "Hanii on the Road (Japan)(1990)(Face).pce", 0x060000, 0x9897fa86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_haniirod)
STD_ROM_FN(pce_haniirod)

struct BurnDriver BurnDrvpce_haniirod = {
	"pce_haniirod", NULL, NULL, NULL, "1990",
	"Hanii on the Road (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_haniirodRomInfo, pce_haniirodRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hatris (Japan)

static struct BurnRomInfo pce_hatrisRomDesc[] = {
	{ "Hatris (Japan)(1991)(Tengen).pce", 0x020000, 0x44e7df53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_hatris)
STD_ROM_FN(pce_hatris)

struct BurnDriver BurnDrvpce_hatris = {
	"pce_hatris", NULL, NULL, NULL, "1991",
	"Hatris (Japan)\0", NULL, "Tengen", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_hatrisRomInfo, pce_hatrisRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Heavy Unit (Japan)

static struct BurnRomInfo pce_hvyunitRomDesc[] = {
	{ "Heavy Unit (Japan)(1989)(Taito).pce", 0x060000, 0xeb923de5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_hvyunit)
STD_ROM_FN(pce_hvyunit)

struct BurnDriver BurnDrvpce_hvyunit = {
	"pce_hvyunit", NULL, NULL, NULL, "1989",
	"Heavy Unit (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_hvyunitRomInfo, pce_hvyunitRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hisou Kihei - Xserd (Japan)

static struct BurnRomInfo pce_xserdRomDesc[] = {
	{ "Hisou Kihei - Xserd (Japan)(1990)(Masaya).pce", 0x060000, 0x1cab1ee6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_xserd)
STD_ROM_FN(pce_xserd)

struct BurnDriver BurnDrvpce_xserd = {
	"pce_xserd", NULL, NULL, NULL, "1990",
	"Hisou Kihei - Xserd (Japan)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_xserdRomInfo, pce_xserdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hit the Ice - VHL The Official Video Hockey League (Japan)

static struct BurnRomInfo pce_hiticeRomDesc[] = {
	{ "Hit the Ice - VHL The Official Video Hockey League (Japan)(1991)(Taito).pce", 0x060000, 0x7acb60c8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_hitice)
STD_ROM_FN(pce_hitice)

struct BurnDriver BurnDrvpce_hitice = {
	"pce_hitice", NULL, NULL, NULL, "1991",
	"Hit the Ice - VHL The Official Video Hockey League (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_hiticeRomInfo, pce_hiticeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Honoo no Toukyuuji - Dodge Danpei (Japan)

static struct BurnRomInfo pce_ddanpeiRomDesc[] = {
	{ "Honoo no Toukyuuji - Dodge Danpei (Japan)(1992)(Hudson Soft).pce", 0x080000, 0xb01ee703, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ddanpei)
STD_ROM_FN(pce_ddanpei)

struct BurnDriver BurnDrvpce_ddanpei = {
	"pce_ddanpei", NULL, NULL, NULL, "1992",
	"Honoo no Toukyuuji - Dodge Danpei (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC | GBF_STRATEGY, 0,
	PceGetZipName, pce_ddanpeiRomInfo, pce_ddanpeiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Idol Hanafuda Fan Club (Japan)

static struct BurnRomInfo pce_idolhanaRomDesc[] = {
	{ "Idol Hanafuda Fan Club (Japan)(1991)(Game Express).pce", 0x080000, 0x9ec6fc6c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_idolhana)
STD_ROM_FN(pce_idolhana)

struct BurnDriver BurnDrvpce_idolhana = {
	"pce_idolhana", NULL, NULL, NULL, "1991",
	"Idol Hanafuda Fan Club (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_idolhanaRomInfo, pce_idolhanaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Image Fight (Japan)

static struct BurnRomInfo pce_imagefgtRomDesc[] = {
	{ "Image Fight (Japan)(1990)(Irem).pce", 0x080000, 0xa80c565f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_imagefgt)
STD_ROM_FN(pce_imagefgt)

struct BurnDriver BurnDrvpce_imagefgt = {
	"pce_imagefgt", NULL, NULL, NULL, "1990",
	"Image Fight (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_imagefgtRomInfo, pce_imagefgtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// J. League Greatest Eleven (Japan)

static struct BurnRomInfo pce_jleag11RomDesc[] = {
	{ "j. league greatest eleven (japan).pce", 0x080000, 0x0ad97b04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_jleag11)
STD_ROM_FN(pce_jleag11)

struct BurnDriver BurnDrvpce_jleag11 = {
	"pce_jleag11", NULL, NULL, NULL, "1993",
	"J. League Greatest Eleven (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSFOOTBALL, 0,
	PceGetZipName, pce_jleag11RomInfo, pce_jleag11RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jack Nicklaus' Greatest 18 Holes of Major Championship Golf (Japan)

static struct BurnRomInfo pce_nicklausRomDesc[] = {
	{ "Jack Nicklaus' Greatest 18 Holes of Major Championship Golf (Japan)(1989)(Victor).pce", 0x040000, 0xea751e82, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nicklaus)
STD_ROM_FN(pce_nicklaus)

struct BurnDriver BurnDrvpce_nicklaus = {
	"pce_nicklaus", NULL, NULL, NULL, "1989",
	"Jack Nicklaus' Greatest 18 Holes of Major Championship Golf (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_nicklausRomInfo, pce_nicklausRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jackie Chan (Japan)

static struct BurnRomInfo pce_jchanRomDesc[] = {
	{ "Jackie Chan (Japan)(1991)(Hudson Soft).pce", 0x080000, 0xc6fa6373, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_jchan)
STD_ROM_FN(pce_jchan)

struct BurnDriver BurnDrvpce_jchan = {
	"pce_jchan", NULL, NULL, NULL, "1991",
	"Jackie Chan (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_jchanRomInfo, pce_jchanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jigoku Meguri (Japan)

static struct BurnRomInfo pce_jigomeguRomDesc[] = {
	{ "Jigoku Meguri (Japan)(1990)(Taito).pce", 0x060000, 0xcc7d3eeb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_jigomegu)
STD_ROM_FN(pce_jigomegu)

struct BurnDriver BurnDrvpce_jigomegu = {
	"pce_jigomegu", NULL, NULL, NULL, "1990",
	"Jigoku Meguri (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_jigomeguRomInfo, pce_jigomeguRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jinmu Denshou (Japan)

static struct BurnRomInfo pce_jinmuRomDesc[] = {
	{ "Jinmu Denshou (Japan)(1989)(Big Club).pce", 0x080000, 0xc150637a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_jinmu)
STD_ROM_FN(pce_jinmu)

struct BurnDriver BurnDrvpce_jinmu = {
	"pce_jinmu", NULL, NULL, NULL, "1989",
	"Jinmu Denshou (Japan)\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_jinmuRomInfo, pce_jinmuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jinmu Denshou (Japan, Alt)

static struct BurnRomInfo pce_jinmu1RomDesc[] = {
	{ "Jinmu Denshou (Japan, Alt)(1989)(Big Club).pce", 0x080000, 0x84240ef9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_jinmu1)
STD_ROM_FN(pce_jinmu1)

struct BurnDriver BurnDrvpce_jinmu1 = {
	"pce_jinmu1", "pce_jinmu", NULL, NULL, "1989",
	"Jinmu Denshou (Japan, Alt)\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_jinmu1RomInfo, pce_jinmu1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Juuouki (Japan)

static struct BurnRomInfo pce_juuoukiRomDesc[] = {
	{ "Juuouki (Japan)(1989)(NEC Avenue).pce", 0x080000, 0xc8c7d63e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_juuouki)
STD_ROM_FN(pce_juuouki)

struct BurnDriver BurnDrvpce_juuouki = {
	"pce_juuouki", NULL, NULL, NULL, "1989",
	"Juuouki (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_juuoukiRomInfo, pce_juuoukiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Juuouki (Japan, Alt)

static struct BurnRomInfo pce_juuouki1RomDesc[] = {
	{ "Juuouki (Japan, Alt)(1989)(NEC Avenue).pce", 0x080000, 0x6a628982, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_juuouki1)
STD_ROM_FN(pce_juuouki1)

struct BurnDriver BurnDrvpce_juuouki1 = {
	"pce_juuouki1", "pce_juuouki", NULL, NULL, "1989",
	"Juuouki (Japan, Alt)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_juuouki1RomInfo, pce_juuouki1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kaizou Choujin Shubibinman (Japan)

static struct BurnRomInfo pce_shubibiRomDesc[] = {
	{ "Kaizou Choujin Shubibinman (Japan)(1989)(NCS - Masaya).pce", 0x040000, 0xa9084d6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shubibi)
STD_ROM_FN(pce_shubibi)

struct BurnDriver BurnDrvpce_shubibi = {
	"pce_shubibi", NULL, NULL, NULL, "1989",
	"Kaizou Choujin Shubibinman (Japan)\0", NULL, "NCS - Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_shubibiRomInfo, pce_shubibiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kaizou Choujin Shubibinman 2: Aratanaru Teki (Japan)

static struct BurnRomInfo pce_shubibi2RomDesc[] = {
	{ "Kaizou Choujin Shubibinman 2 - Aratanaru Teki (Japan)(1991)(Masaya).pce", 0x080000, 0x109ba474, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shubibi2)
STD_ROM_FN(pce_shubibi2)

struct BurnDriver BurnDrvpce_shubibi2 = {
	"pce_shubibi2", NULL, NULL, NULL, "1991",
	"Kaizou Choujin Shubibinman 2: Aratanaru Teki (Japan)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN | GBF_PLATFORM, 0,
	PceGetZipName, pce_shubibi2RomInfo, pce_shubibi2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kato Chan & Ken Chan (Japan)

static struct BurnRomInfo pce_katochanRomDesc[] = {
	{ "Kato Chan & Ken Chan (Japan)(1987)(Hudson Soft).pce", 0x040000, 0x6069c5e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_katochan)
STD_ROM_FN(pce_katochan)

struct BurnDriver BurnDrvpce_katochan = {
	"pce_katochan", NULL, NULL, NULL, "1987",
	"Kato Chan & Ken Chan (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_katochanRomInfo, pce_katochanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kattobi! Takuhai Kun (Japan)

static struct BurnRomInfo pce_kattobiRomDesc[] = {
	{ "Kattobi! Takuhai Kun (Japan)(1990)(Tonkin House).pce", 0x060000, 0x4f2844b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kattobi)
STD_ROM_FN(pce_kattobi)

struct BurnDriver BurnDrvpce_kattobi = {
	"pce_kattobi", NULL, NULL, NULL, "1990",
	"Kattobi! Takuhai Kun (Japan)\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_kattobiRomInfo, pce_kattobiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kickball (Japan)

static struct BurnRomInfo pce_kickballRomDesc[] = {
	{ "Kickball (Japan)(1990)(Masaya).pce", 0x060000, 0x7e3c367b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kickball)
STD_ROM_FN(pce_kickball)

struct BurnDriver BurnDrvpce_kickball = {
	"pce_kickball", NULL, NULL, NULL, "1990",
	"Kickball (Japan)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_kickballRomInfo, pce_kickballRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kiki KaiKai (Japan)

static struct BurnRomInfo pce_kikikaiRomDesc[] = {
	{ "Kiki KaiKai (Japan)(1990)(Taito).pce", 0x060000, 0xc0cb5add, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kikikai)
STD_ROM_FN(pce_kikikai)

struct BurnDriver BurnDrvpce_kikikai = {
	"pce_kikikai", NULL, NULL, NULL, "1990",
	"Kiki KaiKai (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_kikikaiRomInfo, pce_kikikaiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// King of Casino (Japan)

static struct BurnRomInfo pce_kingcasnRomDesc[] = {
	{ "King of Casino (Japan)(1990)(Victor).pce", 0x040000, 0xbf52788e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kingcasn)
STD_ROM_FN(pce_kingcasn)

struct BurnDriver BurnDrvpce_kingcasn = {
	"pce_kingcasn", NULL, NULL, NULL, "1990",
	"King of Casino (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_CASINO, 0,
	PceGetZipName, pce_kingcasnRomInfo, pce_kingcasnRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Klax (Japan)

static struct BurnRomInfo pce_klaxRomDesc[] = {
	{ "Klax (Japan)(1990)(Tengen).pce", 0x040000, 0xc74ffbc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_klax)
STD_ROM_FN(pce_klax)

struct BurnDriver BurnDrvpce_klax = {
	"pce_klax", NULL, NULL, NULL, "1990",
	"Klax (Japan)\0", NULL, "Tengen", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_klaxRomInfo, pce_klaxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Knight Rider Special (Japan)

static struct BurnRomInfo pce_knightrsRomDesc[] = {
	{ "Knight Rider Special (Japan)(1989)(Pack-In-Video).pce", 0x040000, 0xc614116c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_knightrs)
STD_ROM_FN(pce_knightrs)

struct BurnDriver BurnDrvpce_knightrs = {
	"pce_knightrs", NULL, NULL, NULL, "1989",
	"Knight Rider Special (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING | GBF_SHOOT, 0,
	PceGetZipName, pce_knightrsRomInfo, pce_knightrsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Knight Rider Special (Hack, English v2.0)
// https://www.romhacking.net/translations/2156/
static struct BurnRomInfo pce_knightrseRomDesc[] = {
	{ "Knight Rider Special T-Eng v2.0 (2017)(Psyklax).pce", 0x040000, 0x1109d8e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_knightrse)
STD_ROM_FN(pce_knightrse)

struct BurnDriver BurnDrvpce_knightrse = {
	"pce_knightrse", "pce_knightrs", NULL, NULL, "2017",
	"Knight Rider Special (Hack, English v2.0)\0", NULL, "Psyklax", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING | GBF_SHOOT, 0,
	PceGetZipName, pce_knightrseRomInfo, pce_knightrseRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Kore Ga Pro Yakyuu '89 (Japan)

static struct BurnRomInfo pce_proyak89RomDesc[] = {
	{ "Kore Ga Pro Yakyuu '89 (Japan)(1989)(Intec).pce", 0x040000, 0x44f60137, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_proyak89)
STD_ROM_FN(pce_proyak89)

struct BurnDriver BurnDrvpce_proyak89 = {
	"pce_proyak89", NULL, NULL, NULL, "1989",
	"Kore Ga Pro Yakyuu '89 (Japan)\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_proyak89RomInfo, pce_proyak89RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kore Ga Pro Yakyuu '90 (Japan)

static struct BurnRomInfo pce_proyak90RomDesc[] = {
	{ "Kore Ga Pro Yakyuu '90 (Japan)(1990)(Intec).pce", 0x040000, 0x1772b229, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_proyak90)
STD_ROM_FN(pce_proyak90)

struct BurnDriver BurnDrvpce_proyak90 = {
	"pce_proyak90", NULL, NULL, NULL, "1990",
	"Kore Ga Pro Yakyuu '90 (Japan)\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_proyak90RomInfo, pce_proyak90RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kung Fu, The (Japan)

static struct BurnRomInfo pce_kungfuRomDesc[] = {
	{ "Kung Fu, The (Japan)(1987)(Hudson Soft).pce", 0x040000, 0xb552c906, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kungfu)
STD_ROM_FN(pce_kungfu)

struct BurnDriver BurnDrvpce_kungfu = {
	"pce_kungfu", NULL, NULL, NULL, "1987",
	"Kung Fu, The (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_kungfuRomInfo, pce_kungfuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kyuukyoku Mahjong - Idol Graphics (Japan)

static struct BurnRomInfo pce_kyukyomjRomDesc[] = {
	{ "Kyuukyoku Mahjong - Idol Graphics (Japan)(1992)(Game Express).pce", 0x080000, 0x02dde03e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kyukyomj)
STD_ROM_FN(pce_kyukyomj)

struct BurnDriver BurnDrvpce_kyukyomj = {
	"pce_kyukyomj", NULL, NULL, NULL, "1992",
	"Kyuukyoku Mahjong - Idol Graphics (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_kyukyomjRomInfo, pce_kyukyomjRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kyuukyoku Mahjong II (Japan)

static struct BurnRomInfo pce_kyukyom2RomDesc[] = {
	{ "Kyuukyoku Mahjong II (Japan)(1993)(Game Express).pce", 0x080000, 0xe5b6b3e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_kyukyom2)
STD_ROM_FN(pce_kyukyom2)

struct BurnDriver BurnDrvpce_kyukyom2 = {
	"pce_kyukyom2", NULL, NULL, NULL, "1993",
	"Kyuukyoku Mahjong II (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_kyukyom2RomInfo, pce_kyukyom2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Kyuukyoku Tiger (Japan)

static struct BurnRomInfo pce_ktigerRomDesc[] = {
	{ "Kyuukyoku Tiger (Japan)(1989)(Taito).pce", 0x040000, 0x09509315, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ktiger)
STD_ROM_FN(pce_ktiger)

struct BurnDriver BurnDrvpce_ktiger = {
	"pce_ktiger", NULL, NULL, NULL, "1989",
	"Kyuukyoku Tiger (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_ktigerRomInfo, pce_ktigerRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Lady Sword (Japan)

static struct BurnRomInfo pce_ladyswrdRomDesc[] = {
	{ "Lady Sword (Japan)(1992)(Game Express).pce", 0x100000, 0xc6f764ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ladyswrd)
STD_ROM_FN(pce_ladyswrd)

struct BurnDriver BurnDrvpce_ladyswrd = {
	"pce_ladyswrd", NULL, NULL, NULL, "1992",
	"Lady Sword (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG | GBF_MAZE, 0,
	PceGetZipName, pce_ladyswrdRomInfo, pce_ladyswrdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Lady Sword (Japan, Alt)

static struct BurnRomInfo pce_ladyswrd1RomDesc[] = {
	{ "Lady Sword (Japan, Alt)(1992)(Game Express).pce", 0x100000, 0xeb833d15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ladyswrd1)
STD_ROM_FN(pce_ladyswrd1)

struct BurnDriver BurnDrvpce_ladyswrd1 = {
	"pce_ladyswrd1", "pce_ladyswrd", NULL, NULL, "1992",
	"Lady Sword (Japan, Alt)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG | GBF_MAZE, 0,
	PceGetZipName, pce_ladyswrd1RomInfo, pce_ladyswrd1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Lady Sword (Hack, English)
// https://www.romhacking.net/translations/1574/
static struct BurnRomInfo pce_ladyswrdengRomDesc[] = {
	{ "Lady Sword T-Eng (2010)(EsperKnight, filler, Grant Laughlin, Tomaitheous).pce", 0x100200, 0xc556aa43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ladyswrdeng)
STD_ROM_FN(pce_ladyswrdeng)

struct BurnDriver BurnDrvpce_ladyswrdeng = {
	"pce_ladyswrdeng", "pce_ladyswrd", NULL, NULL, "2010",
	"Lady Sword (Hack, English)\0", NULL, "EsperKnight, filler, Grant Laughlin, Tomaitheous", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_ladyswrdengRomInfo, pce_ladyswrdengRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Legend of Hero Tonma (Japan)

static struct BurnRomInfo pce_lohtRomDesc[] = {
	{ "Legend of Hero Tonma (Japan)(1991)(Irem).pce", 0x080000, 0xc28b0d8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_loht)
STD_ROM_FN(pce_loht)

struct BurnDriver BurnDrvpce_loht = {
	"pce_loht", NULL, NULL, NULL, "1991",
	"Legend of Hero Tonma (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_lohtRomInfo, pce_lohtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Lode Runner - Lost Labyrinth (Japan)

static struct BurnRomInfo pce_ldrunRomDesc[] = {
	{ "Lode Runner - Lost Labyrinth (Japan)(1990)(Pack-In-Video).pce", 0x040000, 0xe6ee1468, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ldrun)
STD_ROM_FN(pce_ldrun)

struct BurnDriver BurnDrvpce_ldrun = {
	"pce_ldrun", NULL, NULL, NULL, "1990",
	"Lode Runner - Lost Labyrinth (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_PLATFORM, 0,
	PceGetZipName, pce_ldrunRomInfo, pce_ldrunRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Magical Chase (Japan)

static struct BurnRomInfo pce_magchaseRomDesc[] = {
	{ "Magical Chase (Japan)(1991)(Palsoft - Quest).pce", 0x080000, 0xdd0ebf8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_magchase)
STD_ROM_FN(pce_magchase)

struct BurnDriver BurnDrvpce_magchase = {
	"pce_magchase", NULL, NULL, NULL, "1991",
	"Magical Chase (Japan)\0", NULL, "Palsoft - Quest", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_magchaseRomInfo, pce_magchaseRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Gakuen: Touma Soushirou Toujou (Japan)

static struct BurnRomInfo pce_mjgakuenRomDesc[] = {
	{ "Mahjong Gakuen - Touma Soushirou Toujou (Japan)(1990)(Face).pce", 0x080000, 0xf5b90d55, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjgakuen)
STD_ROM_FN(pce_mjgakuen)

struct BurnDriver BurnDrvpce_mjgakuen = {
	"pce_mjgakuen", NULL, NULL, NULL, "1990",
	"Mahjong Gakuen: Touma Soushirou Toujou (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_mjgakuenRomInfo, pce_mjgakuenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Gakuen Mild: Touma Soushirou Toujou (Japan)

static struct BurnRomInfo pce_mjgakmldRomDesc[] = {
	{ "Mahjong Gakuen Mild - Touma Soushirou Toujou (Japan)(1990)(Face).pce", 0x080000, 0xf4148600, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjgakmld)
STD_ROM_FN(pce_mjgakmld)

struct BurnDriver BurnDrvpce_mjgakmld = {
	"pce_mjgakmld", NULL, NULL, NULL, "1990",
	"Mahjong Gakuen Mild: Touma Soushirou Toujou (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_mjgakmldRomInfo, pce_mjgakmldRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Gakuen Mild: Touma Soushirou Toujou (Japan, Alt)

static struct BurnRomInfo pce_mjgakmld1RomDesc[] = {
	{ "Mahjong Gakuen Mild - Touma Soushirou Toujou (Japan, Alt)(1990)(Face).pce", 0x080000, 0x3e4d432a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjgakmld1)
STD_ROM_FN(pce_mjgakmld1)

struct BurnDriver BurnDrvpce_mjgakmld1 = {
	"pce_mjgakmld1", "pce_mjgakmld", NULL, NULL, "1990",
	"Mahjong Gakuen Mild: Touma Soushirou Toujou (Japan, Alt)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_mjgakmld1RomInfo, pce_mjgakmld1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Gokuu Special (Japan)

static struct BurnRomInfo pce_mjgokuspRomDesc[] = {
	{ "Mahjong Gokuu Special (Japan)(1990)(Sunsoft).pce", 0x060000, 0xf8861456, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjgokusp)
STD_ROM_FN(pce_mjgokusp)

struct BurnDriver BurnDrvpce_mjgokusp = {
	"pce_mjgokusp", NULL, NULL, NULL, "1990",
	"Mahjong Gokuu Special (Japan)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_mjgokuspRomInfo, pce_mjgokuspRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Haou Den: Kaiser's Quest (Japan)

static struct BurnRomInfo pce_mjkaiserRomDesc[] = {
	{ "Mahjong Haou Den - Kaiser's Quest (Japan)(1992)(UPL).pce", 0x080000, 0xdf10c895, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjkaiser)
STD_ROM_FN(pce_mjkaiser)

struct BurnDriver BurnDrvpce_mjkaiser = {
	"pce_mjkaiser", NULL, NULL, NULL, "1992",
	"Mahjong Haou Den: Kaiser's Quest (Japan)\0", NULL, "UPL", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_mjkaiserRomInfo, pce_mjkaiserRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mahjong Shikyaku Retsuden: Mahjong Wars (Japan)

static struct BurnRomInfo pce_mjwarsRomDesc[] = {
	{ "Mahjong Shikyaku Retsuden - Mahjong Wars (Japan)(1990)(Nihon Bussan).pce", 0x040000, 0x6c34aaea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mjwars)
STD_ROM_FN(pce_mjwars)

struct BurnDriver BurnDrvpce_mjwars = {
	"pce_mjwars", NULL, NULL, NULL, "1990",
	"Mahjong Shikyaku Retsuden: Mahjong Wars (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG | GBF_RPG, 0,
	PceGetZipName, pce_mjwarsRomInfo, pce_mjwarsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Maison Ikkoku (Japan)

static struct BurnRomInfo pce_mikkokuRomDesc[] = {
	{ "Maison Ikkoku (Japan)(1989)(Micro Cabin).pce", 0x040000, 0x5c78fee1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mikkoku)
STD_ROM_FN(pce_mikkoku)

struct BurnDriver BurnDrvpce_mikkoku = {
	"pce_mikkoku", NULL, NULL, NULL, "1989",
	"Maison Ikkoku (Japan)\0", NULL, "Micro Cabin", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_mikkokuRomInfo, pce_mikkokuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Maison Ikkoku (Hack, English)
// https://www.romhacking.net/translations/724/
static struct BurnRomInfo pce_mikkokueRomDesc[] = {
	{ "Maison Ikkoku (T-Eng)(2008)(Dave Shadoff, filler).pce", 0x080200, 0xb5ed56d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mikkokue)
STD_ROM_FN(pce_mikkokue)

struct BurnDriver BurnDrvpce_mikkokue = {
	"pce_mikkokue", "pce_mikkoku", NULL, NULL, "2008",
	"Maison Ikkoku (Hack, English)\0", NULL, "Dave Shadoff, filler", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_mikkokueRomInfo, pce_mikkokueRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Majin Eiyuu Den Wataru (Japan)

static struct BurnRomInfo pce_wataruRomDesc[] = {
	{ "Majin Eiyuu Den Wataru (Japan)(1988)(Hudson Soft).pce", 0x040000, 0x2f8935aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wataru)
STD_ROM_FN(pce_wataru)

struct BurnDriver BurnDrvpce_wataru = {
	"pce_wataru", NULL, NULL, NULL, "1988",
	"Majin Eiyuu Den Wataru (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_wataruRomInfo, pce_wataruRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Makai Hakken Den Shada (Japan)

static struct BurnRomInfo pce_makaihakRomDesc[] = {
	{ "Makai Hakken Den Shada (Japan)(1989)(Data East).pce", 0x040000, 0xbe62eef5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_makaihak)
STD_ROM_FN(pce_makaihak)

struct BurnDriver BurnDrvpce_makaihak = {
	"pce_makaihak", NULL, NULL, NULL, "1989",
	"Makai Hakken Den Shada (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_makaihakRomInfo, pce_makaihakRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Makai Hakken Den Shada (Hack, English)
// https://www.romhacking.net/translations/2636/
static struct BurnRomInfo pce_makaihakeRomDesc[] = {
	{ "Makai Hakkenden Shada T-Eng (2015)(Shubibiman).pce", 0x040000, 0x017de16d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_makaihake)
STD_ROM_FN(pce_makaihake)

struct BurnDriver BurnDrvpce_makaihake = {
	"pce_makaihake", "pce_makaihak", NULL, NULL, "2015",
	"Makai Hakken Den Shada (Hack, English)\0", NULL, "Shubibiman", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_makaihakeRomInfo, pce_makaihakeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Makai Prince Dorabocchan (Japan)

static struct BurnRomInfo pce_makaipriRomDesc[] = {
	{ "Makai Prince Dorabocchan (Japan)(1990)(Naxat Soft).pce", 0x060000, 0xb101b333, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_makaipri)
STD_ROM_FN(pce_makaipri)

struct BurnDriver BurnDrvpce_makaipri = {
	"pce_makaipri", NULL, NULL, NULL, "1990",
	"Makai Prince Dorabocchan (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_makaipriRomInfo, pce_makaipriRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Makyou Densetsu (Japan)

static struct BurnRomInfo pce_makyodenRomDesc[] = {
	{ "Makyou Densetsu (Japan)(1988)(Victor).pce", 0x040000, 0xd4c5af46, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_makyoden)
STD_ROM_FN(pce_makyoden)

struct BurnDriver BurnDrvpce_makyoden = {
	"pce_makyoden", NULL, NULL, NULL, "1988",
	"Makyou Densetsu (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_makyodenRomInfo, pce_makyodenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Maniac Pro Wres - Asu e no Tatakai (Japan)

static struct BurnRomInfo pce_maniacpwRomDesc[] = {
	{ "maniac puroresu - asu e no tatakai (japan).pce", 0x080000, 0x99f2865c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_maniacpw)
STD_ROM_FN(pce_maniacpw)

struct BurnDriver BurnDrvpce_maniacpw = {
	"pce_maniacpw", NULL, NULL, NULL, "1990",
	"Maniac Pro Wres - Asu e no Tatakai (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT | GBF_RPG, 0,
	PceGetZipName, pce_maniacpwRomInfo, pce_maniacpwRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Marchen Maze (Japan)

static struct BurnRomInfo pce_marchenRomDesc[] = {
	{ "maerchen maze (japan).pce", 0x040000, 0xa15a1f37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_marchen)
STD_ROM_FN(pce_marchen)

struct BurnDriver BurnDrvpce_marchen = {
	"pce_marchen", NULL, NULL, NULL, "1990",
	"Marchen Maze (Japan)\0", NULL, "Namcot", "PC Engine",
	L"M\u00E4rchen Maze (Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_marchenRomInfo, pce_marchenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mashin Eiyuuden Wataru (Japan)

static struct BurnRomInfo pce_mashwataruRomDesc[] = {
	{ "Mashin Eiyuuden Wataru (Japan)(1988)(Hudson Soft).pce", 0x040000, 0x2f8935aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mashwataru)
STD_ROM_FN(pce_mashwataru)

struct BurnDriver BurnDrvpce_mashwataru = {
	"pce_mashwataru", NULL, NULL, NULL, "1988",
	"Mashin Eiyuuden Wataru (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_ADV, 0,
	PceGetZipName, pce_mashwataruRomInfo, pce_mashwataruRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mashin Eiyuuden Wataru (Hack, English)
// https://mail.romhacking.net/translations/6762/
static struct BurnRomInfo pce_mashwatarueRomDesc[] = {
	{ "Mashin Eiyuuden Wataru T-Eng (2022)(Stardust Crusaders).pce", 0x080000, 0x53e14cb5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mashwatarue)
STD_ROM_FN(pce_mashwatarue)

struct BurnDriver BurnDrvpce_mashwatarue = {
	"pce_mashwatarue", "pce_mashwataru", NULL, NULL, "2022",
	"Mashin Eiyuuden Wataru (Hack, English)\0", NULL, "Stardust Crusaders", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_ADV, 0,
	PceGetZipName, pce_mashwatarueRomInfo, pce_mashwatarueRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mesopotamia (Japan)

static struct BurnRomInfo pce_mesopotRomDesc[] = {
	{ "Mesopotamia (Japan)(1991)(Atlus).pce", 0x080000, 0xe87190f1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mesopot)
STD_ROM_FN(pce_mesopot)

struct BurnDriver BurnDrvpce_mesopot = {
	"pce_mesopot", NULL, NULL, NULL, "1991",
	"Mesopotamia (Japan)\0", NULL, "Atlus Co.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_mesopotRomInfo, pce_mesopotRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Metal Stoker (Japan)

static struct BurnRomInfo pce_metlstokRomDesc[] = {
	{ "Metal Stoker (Japan)(1991)(Face).pce", 0x080000, 0x25a02bee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_metlstok)
STD_ROM_FN(pce_metlstok)

struct BurnDriver BurnDrvpce_metlstok = {
	"pce_metlstok", NULL, NULL, NULL, "1991",
	"Metal Stoker (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_metlstokRomInfo, pce_metlstokRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mizubaku Dai Bouken ~ Liquid Kids (Japan)

static struct BurnRomInfo pce_mizubakuRomDesc[] = {
	{ "Mizubaku Dai Bouken (Japan)(1992)(Taito).pce", 0x080000, 0xb2ef558d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mizubaku)
STD_ROM_FN(pce_mizubaku)

struct BurnDriver BurnDrvpce_mizubaku = {
	"pce_mizubaku", NULL, NULL, NULL, "1992",
	"Mizubaku Dai Bouken ~ Liquid Kids (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_mizubakuRomInfo, pce_mizubakuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Momotarou Densetsu Gaiden Dai 1 Shuu (Japan)

static struct BurnRomInfo pce_momogdnRomDesc[] = {
	{ "momotarou densetsu gaiden dai 1 shuu (japan).pce", 0x080000, 0xf860455c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_momogdn)
STD_ROM_FN(pce_momogdn)

struct BurnDriver BurnDrvpce_momogdn = {
	"pce_momogdn", NULL, NULL, NULL, "1992",
	"Momotarou Densetsu Gaiden Dai 1 Shuu (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_momogdnRomInfo, pce_momogdnRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Momotarou Densetsu II (Japan)

static struct BurnRomInfo pce_momo2RomDesc[] = {
	{ "momotarou densetsu ii (japan).pce", 0x0c0000, 0xd9e1549a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_momo2)
STD_ROM_FN(pce_momo2)

struct BurnDriver BurnDrvpce_momo2 = {
	"pce_momo2", NULL, NULL, NULL, "1990",
	"Momotarou Densetsu II (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_momo2RomInfo, pce_momo2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Momotarou Densetsu Turbo (Japan)

static struct BurnRomInfo pce_momotrboRomDesc[] = {
	{ "momotarou densetsu turbo (japan).pce", 0x060000, 0x625221a6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_momotrbo)
STD_ROM_FN(pce_momotrbo)

struct BurnDriver BurnDrvpce_momotrbo = {
	"pce_momotrbo", NULL, NULL, NULL, "1990",
	"Momotarou Densetsu Turbo (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_momotrboRomInfo, pce_momotrboRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Momotarou Katsugeki (Japan)

static struct BurnRomInfo pce_momoktsgRomDesc[] = {
	{ "momotarou katsugeki (japan).pce", 0x080000, 0x345f43e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_momoktsg)
STD_ROM_FN(pce_momoktsg)

struct BurnDriver BurnDrvpce_momoktsg = {
	"pce_momoktsg", NULL, NULL, NULL, "1990",
	"Momotarou Katsugeki (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_momoktsgRomInfo, pce_momoktsgRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Monster Pro Wres (Japan)

static struct BurnRomInfo pce_mnstprowRomDesc[] = {
	{ "monster puroresu (japan).pce", 0x080000, 0xf2e46d25, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mnstprow)
STD_ROM_FN(pce_mnstprow)

struct BurnDriver BurnDrvpce_mnstprow = {
	"pce_mnstprow", NULL, NULL, NULL, "1991",
	"Monster Pro Wres (Japan)\0", NULL, "ASK", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_mnstprowRomInfo, pce_mnstprowRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Morita Shougi PC (Japan)

static struct BurnRomInfo pce_moritashRomDesc[] = {
	{ "morita shougi pc (japan).pce", 0x080000, 0x2546efe0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_moritash)
STD_ROM_FN(pce_moritash)

struct BurnDriver BurnDrvpce_moritash = {
	"pce_moritash", NULL, NULL, NULL, "1991",
	"Morita Shougi PC (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_moritashRomInfo, pce_moritashRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Moto Roader (Japan)

static struct BurnRomInfo pce_motoroadRomDesc[] = {
	{ "moto roader (japan).pce", 0x040000, 0x428f36cd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_motoroad)
STD_ROM_FN(pce_motoroad)

struct BurnDriver BurnDrvpce_motoroad = {
	"pce_motoroad", NULL, NULL, NULL, "1989",
	"Moto Roader (Japan)\0", NULL, "NCS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_motoroadRomInfo, pce_motoroadRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Moto Roader II (Japan)

static struct BurnRomInfo pce_motorod2RomDesc[] = {
	{ "Moto roader II (Japan)(1991)(Masaya).pce", 0x060000, 0x0b7f6e5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_motorod2)
STD_ROM_FN(pce_motorod2)

struct BurnDriver BurnDrvpce_motorod2 = {
	"pce_motorod2", NULL, NULL, NULL, "1991",
	"Moto Roader II (Japan)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_motorod2RomInfo, pce_motorod2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Moto Roader II (Japan, Alt)

static struct BurnRomInfo pce_motorod2aRomDesc[] = {
	{ "Moto roader II (Japan, Alt)(1991)(Masaya).pce", 0x060000, 0x4ba525ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_motorod2a)
STD_ROM_FN(pce_motorod2a)

struct BurnDriver BurnDrvpce_motorod2a = {
	"pce_motorod2a", "pce_motorod2", NULL, NULL, "1991",
	"Moto Roader II (Japan, Alt)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 5, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_motorod2aRomInfo, pce_motorod2aRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mr. Heli no Daibouken (Japan)

static struct BurnRomInfo pce_mrheliRomDesc[] = {
	{ "Mr. Heli no Daibouken (Japan)(1989)(Irem).pce", 0x080000, 0x2cb92290, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mrheli)
STD_ROM_FN(pce_mrheli)

struct BurnDriver BurnDrvpce_mrheli = {
	"pce_mrheli", NULL, NULL, NULL, "1989",
	"Mr. Heli no Daibouken (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_mrheliRomInfo, pce_mrheliRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Mr. Heli no Daibouken (Japan, Alt)

static struct BurnRomInfo pce_mrheli1RomDesc[] = {
	{ "Mr. Heli no Daibouken (Japan, Alt)(1989)(Irem).pce", 0x080000, 0xac0cd796, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mrheli1)
STD_ROM_FN(pce_mrheli1)

struct BurnDriver BurnDrvpce_mrheli1 = {
	"pce_mrheli1", "pce_mrheli", NULL, NULL, "1989",
	"Mr. Heli no Daibouken (Japan, Alt)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_mrheli1RomInfo, pce_mrheli1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Narazumono Sentai Butai - Bloody Wolf (Japan)

static struct BurnRomInfo pce_blodwolfRomDesc[] = {
	{ "Narazumono Sentai Butai - Bloody Wolf (Japan)(1989)(Data East).pce", 0x080000, 0xb01f70c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_blodwolf)
STD_ROM_FN(pce_blodwolf)

struct BurnDriver BurnDrvpce_blodwolf = {
	"pce_blodwolf", NULL, NULL, NULL, "1989",
	"Narazumono Sentai Butai - Bloody Wolf (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_blodwolfRomInfo, pce_blodwolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Naxat Open (Japan)

static struct BurnRomInfo pce_naxopenRomDesc[] = {
	{ "Naxat Open (Japan)(1989)(Naxat Soft).pce", 0x060000, 0x60ecae22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_naxopen)
STD_ROM_FN(pce_naxopen)

struct BurnDriver BurnDrvpce_naxopen = {
	"pce_naxopen", NULL, NULL, NULL, "1989",
	"Naxat Open (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_naxopenRomInfo, pce_naxopenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Naxat Stadium (Japan)

static struct BurnRomInfo pce_naxstadRomDesc[] = {
	{ "Naxat Stadium (Japan)(1990)(Naxat Soft).pce", 0x080000, 0x20a7d128, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_naxstad)
STD_ROM_FN(pce_naxstad)

struct BurnDriver BurnDrvpce_naxstad = {
	"pce_naxstad", NULL, NULL, NULL, "1990",
	"Naxat Stadium (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_naxstadRomInfo, pce_naxstadRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Nazo no Masquerade (Japan)

static struct BurnRomInfo pce_nazomasqRomDesc[] = {
	{ "Nazo no Masquerade (Japan)(1990)(Masaya).pce", 0x060000, 0x0441d85a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nazomasq)
STD_ROM_FN(pce_nazomasq)

struct BurnDriver BurnDrvpce_nazomasq = {
	"pce_nazomasq", NULL, NULL, NULL, "1990",
	"Nazo no Masquerade (Japan)\0", NULL, "Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_nazomasqRomInfo, pce_nazomasqRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Necromancer (Japan)

static struct BurnRomInfo pce_necromcrRomDesc[] = {
	{ "Necromancer (Japan)(1988)(Hudson Soft).pce", 0x040000, 0x53109ae6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_necromcr)
STD_ROM_FN(pce_necromcr)

struct BurnDriver BurnDrvpce_necromcr = {
	"pce_necromcr", NULL, NULL, NULL, "1988",
	"Necromancer (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_necromcrRomInfo, pce_necromcrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Necros no Yousai (Japan)

static struct BurnRomInfo pce_necrosRomDesc[] = {
	{ "Necros no Yousai (Japan)(1990)(ASK).pce", 0x080000, 0xfb0fdcfe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_necros)
STD_ROM_FN(pce_necros)

struct BurnDriver BurnDrvpce_necros = {
	"pce_necros", NULL, NULL, NULL, "1990",
	"Necros no Yousai (Japan)\0", NULL, "ASK", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_necrosRomInfo, pce_necrosRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Nectaris (Japan)

static struct BurnRomInfo pce_nectarisRomDesc[] = {
	{ "Nectaris (Japan)(1990)(Hudson Soft).pce", 0x060000, 0x0243453b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nectaris)
STD_ROM_FN(pce_nectaris)

struct BurnDriver BurnDrvpce_nectaris = {
	"pce_nectaris", NULL, NULL, NULL, "1990",
	"Nectaris (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_nectarisRomInfo, pce_nectarisRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu - PC Bangai Hen (Japan)

static struct BurnRomInfo pce_nekdodgeRomDesc[] = {
	{ "Nekketsu Koukou Dodgeball Bu - PC Bangai Hen (Japan)(1990)(Naxat Soft).pce", 0x040000, 0x65fdb863, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nekdodge)
STD_ROM_FN(pce_nekdodge)

struct BurnDriver BurnDrvpce_nekdodge = {
	"pce_nekdodge", NULL, NULL, NULL, "1990",
	"Nekketsu Koukou Dodgeball Bu - PC Bangai Hen (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_nekdodgeRomInfo, pce_nekdodgeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu - Soccer PC Hen (Japan)

static struct BurnRomInfo pce_neksoccrRomDesc[] = {
	{ "Nekketsu Koukou Dodgeball Bu - Soccer PC Hen (Japan)(1992)(Naxat Soft).pce", 0x080000, 0xf2285c6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_neksoccr)
STD_ROM_FN(pce_neksoccr)

struct BurnDriver BurnDrvpce_neksoccr = {
	"pce_neksoccr", NULL, NULL, NULL, "1992",
	"Nekketsu Koukou Dodgeball Bu - Soccer PC Hen (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSFOOTBALL | GBF_STRATEGY, 0,
	PceGetZipName, pce_neksoccrRomInfo, pce_neksoccrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Neutopia (Japan)

static struct BurnRomInfo pce_neutopiaRomDesc[] = {
	{ "Neutopia (Japan)(1989)(Hudson Soft).pce", 0x060000, 0x9c49ef11, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_neutopia)
STD_ROM_FN(pce_neutopia)

struct BurnDriver BurnDrvpce_neutopia = {
	"pce_neutopia", NULL, NULL, NULL, "1989",
	"Neutopia (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_neutopiaRomInfo, pce_neutopiaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Neutopia II (Japan)

static struct BurnRomInfo pce_neutopi2RomDesc[] = {
	{ "Neutopia II (Japan)(1991)(Hudson Soft).pce", 0x0c0000, 0x2b94aedc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_neutopi2)
STD_ROM_FN(pce_neutopi2)

struct BurnDriver BurnDrvpce_neutopi2 = {
	"pce_neutopi2", NULL, NULL, NULL, "1991",
	"Neutopia II (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_neutopi2RomInfo, pce_neutopi2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// New Zealand Story, The (Japan)

static struct BurnRomInfo pce_tnzsRomDesc[] = {
	{ "New Zealand Story, The (Japan)(1990)(Taito).pce", 0x060000, 0x8e4d75a8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tnzs)
STD_ROM_FN(pce_tnzs)

struct BurnDriver BurnDrvpce_tnzs = {
	"pce_tnzs", NULL, NULL, NULL, "1990",
	"New Zealand Story, The (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_tnzsRomInfo, pce_tnzsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// NHK Taiga Drama - Taiheiki (Japan)

static struct BurnRomInfo pce_nhktaidrRomDesc[] = {
	{ "nhk taiga drama - taiheiki (japan).pce", 0x080000, 0xa32430d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nhktaidr)
STD_ROM_FN(pce_nhktaidr)

struct BurnDriver BurnDrvpce_nhktaidr = {
	"pce_nhktaidr", NULL, NULL, NULL, "1992",
	"NHK Taiga Drama - Taiheiki (Japan)\0", NULL, "NHK Enterprise", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_nhktaidrRomInfo, pce_nhktaidrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Niko Niko, Pun (Japan)

static struct BurnRomInfo pce_nikopunRomDesc[] = {
	{ "niko niko pun (japan).pce", 0x080000, 0x82def9ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nikopun)
STD_ROM_FN(pce_nikopun)

struct BurnDriver BurnDrvpce_nikopun = {
	"pce_nikopun", NULL, NULL, NULL, "1991",
	"Niko Niko, Pun (Japan)\0", NULL, "NHK Enterprise", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_nikopunRomInfo, pce_nikopunRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ninja Ryukenden (Japan)

static struct BurnRomInfo pce_nryukendRomDesc[] = {
	{ "ninja ryuuken den (japan).pce", 0x080000, 0x67573bac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nryukend)
STD_ROM_FN(pce_nryukend)

struct BurnDriver BurnDrvpce_nryukend = {
	"pce_nryukend", NULL, NULL, NULL, "1992",
	"Ninja Ryukenden (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_nryukendRomInfo, pce_nryukendRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ninja Warriors, The (Japan)

static struct BurnRomInfo pce_ninjawarRomDesc[] = {
	{ "Ninja Warriors, The (Japan)(1989)(Taito).pce", 0x060000, 0x96e0cd9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ninjawar)
STD_ROM_FN(pce_ninjawar)

struct BurnDriver BurnDrvpce_ninjawar = {
	"pce_ninjawar", NULL, NULL, NULL, "1989",
	"Ninja Warriors, The (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_ninjawarRomInfo, pce_ninjawarRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Obocchama-kun (Japan)

static struct BurnRomInfo pce_obocchaRomDesc[] = {
	{ "Obocchama-kun (Japan)(1991)(Namcot).pce", 0x080000, 0x4d3b0bc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_oboccha)
STD_ROM_FN(pce_oboccha)

struct BurnDriver BurnDrvpce_oboccha = {
	"pce_oboccha", NULL, NULL, NULL, "1991",
	"Obocchama-kun (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_obocchaRomInfo, pce_obocchaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Off the Wall (Prototype)

static struct BurnRomInfo pce_offthewallRomDesc[] = {
	{ "Off the Wall (Proto)(1992)(Tengen - Atari).pce", 0x080200, 0x40f04a7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_offthewall)
STD_ROM_FN(pce_offthewall)

struct BurnDriver BurnDrvpce_offthewall = {
	"pce_offthewall", NULL, NULL, NULL, "1992",
	"Off the Wall (Prototype)\0", NULL, "Tengen - Atari", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BREAKOUT, 0,
	PceGetZipName, pce_offthewallRomInfo, pce_offthewallRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Operation Wolf (Japan)

static struct BurnRomInfo pce_opwolfRomDesc[] = {
	{ "Operation Wolf (Japan)(1990)(NEC Avenue).pce", 0x080000, 0xff898f87, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_opwolf)
STD_ROM_FN(pce_opwolf)

struct BurnDriver BurnDrvpce_opwolf = {
	"pce_opwolf", NULL, NULL, NULL, "1990",
	"Operation Wolf (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_opwolfRomInfo, pce_opwolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ordyne (Japan)

static struct BurnRomInfo pce_ordyneRomDesc[] = {
	{ "Ordyne (Japan)(1989)(Namcot).pce", 0x080000, 0x8c565cb6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ordyne)
STD_ROM_FN(pce_ordyne)

struct BurnDriver BurnDrvpce_ordyne = {
	"pce_ordyne", NULL, NULL, NULL, "1989",
	"Ordyne (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_ordyneRomInfo, pce_ordyneRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Out Live (Japan)

static struct BurnRomInfo pce_outliveRomDesc[] = {
	{ "Out Live (Japan)(1989)(Sunsoft).pce", 0x040000, 0x5cdb3f5b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_outlive)
STD_ROM_FN(pce_outlive)

struct BurnDriver BurnDrvpce_outlive = {
	"pce_outlive", NULL, NULL, NULL, "1989",
	"Out Live (Japan)\0", NULL, "Sunsoft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_outliveRomInfo, pce_outliveRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Out Live (Hack, English v1.1)
// https://www.romhacking.net/translations/2814/
static struct BurnRomInfo pce_outliveeRomDesc[] = {
	{ "Out Live T-Eng v1.1 (2016)(Nebulous Translations).pce", 0x04a000, 0x2f1cee20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_outlivee)
STD_ROM_FN(pce_outlivee)

struct BurnDriver BurnDrvpce_outlivee = {
	"pce_outlivee", "pce_outlive", NULL, NULL, "2016",
	"Out Live (Hack, English v1.1)\0", NULL, "Nebulous Translations", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_outliveeRomInfo, pce_outliveeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Out Run (Japan)

static struct BurnRomInfo pce_outrunRomDesc[] = {
	{ "Out Run (Japan)(1990)(NEC Avenue).pce", 0x080000, 0xe203f223, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_outrun)
STD_ROM_FN(pce_outrun)

struct BurnDriver BurnDrvpce_outrun = {
	"pce_outrun", NULL, NULL, NULL, "1990",
	"Out Run (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_outrunRomInfo, pce_outrunRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Override (Japan)

static struct BurnRomInfo pce_overrideRomDesc[] = {
	{ "Override (Japan)(1991)(Data East).pce", 0x040000, 0xb74ec562, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_override)
STD_ROM_FN(pce_override)

struct BurnDriver BurnDrvpce_override = {
	"pce_override", NULL, NULL, NULL, "1991",
	"Override (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_overrideRomInfo, pce_overrideRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// P-47: The Freedom Fighter (Japan)

static struct BurnRomInfo pce_p47RomDesc[] = {
	{ "P-47 - The Freedom Fighter (Japan)(1989)(Aicom).pce", 0x040000, 0x7632db90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_p47)
STD_ROM_FN(pce_p47)

struct BurnDriver BurnDrvpce_p47 = {
	"pce_p47", NULL, NULL, NULL, "1989",
	"P-47: The Freedom Fighter (Japan)\0", NULL, "Aicom Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_p47RomInfo, pce_p47RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pac-Land (Japan)

static struct BurnRomInfo pce_paclandRomDesc[] = {
	{ "Pac-Land (Japan)(1989)(Namcot).pce", 0x040000, 0x14fad3ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pacland)
STD_ROM_FN(pce_pacland)

struct BurnDriver BurnDrvpce_pacland = {
	"pce_pacland", NULL, NULL, NULL, "1989",
	"Pac-Land (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_paclandRomInfo, pce_paclandRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pachio Kun - Juuban Shoubu (Japan)

static struct BurnRomInfo pce_pachikunRomDesc[] = {
	{ "pachio kun - juuban shoubu (japan).pce", 0x080000, 0x4148fd7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pachikun)
STD_ROM_FN(pce_pachikun)

struct BurnDriver BurnDrvpce_pachikun = {
	"pce_pachikun", NULL, NULL, NULL, "1992",
	"Pachio Kun - Juuban Shoubu (Japan)\0", NULL, "Coconuts Japan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV | GBF_CASINO, 0,
	PceGetZipName, pce_pachikunRomInfo, pce_pachikunRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Paranoia (Japan)

static struct BurnRomInfo pce_paranoiaRomDesc[] = {
	{ "paranoia (japan).pce", 0x040000, 0x9893e0e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_paranoia)
STD_ROM_FN(pce_paranoia)

struct BurnDriver BurnDrvpce_paranoia = {
	"pce_paranoia", NULL, NULL, NULL, "1990",
	"Paranoia (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_paranoiaRomInfo, pce_paranoiaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Parasol Stars - The Story of Bubble Bobble III (Japan)

static struct BurnRomInfo pce_parasolRomDesc[] = {
	{ "Parasol Stars - The Story of Bubble Bobble III (Japan)(1991)(Taito).pce", 0x060000, 0x51e86451, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_parasol)
STD_ROM_FN(pce_parasol)

struct BurnDriver BurnDrvpce_parasol = {
	"pce_parasol", NULL, NULL, NULL, "1991",
	"Parasol Stars - The Story of Bubble Bobble III (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_parasolRomInfo, pce_parasolRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Parodius da! - Shinwa Kara Owarai He (Japan)

static struct BurnRomInfo pce_parodiusRomDesc[] = {
	{ "Parodius da! - Shinwa Kara Owarai He (Japan)(1992)(Konami).pce", 0x100000, 0x647718f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_parodius)
STD_ROM_FN(pce_parodius)

struct BurnDriver BurnDrvpce_parodius = {
	"pce_parodius", NULL, NULL, NULL, "1992",
	"Parodius da! - Shinwa Kara Owarai He (Japan)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_parodiusRomInfo, pce_parodiusRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Denjin: Punkic Cyborgs (Japan)

static struct BurnRomInfo pce_pcdenjRomDesc[] = {
	{ "PC Denjin - Punkic Cyborgs (Japan)(1992)(Hudson Soft).pce", 0x080000, 0x740491c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcdenj)
STD_ROM_FN(pce_pcdenj)

struct BurnDriver BurnDrvpce_pcdenj = {
	"pce_pcdenj", NULL, NULL, NULL, "1992",
	"PC Denjin - Punkic Cyborgs (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_pcdenjRomInfo, pce_pcdenjRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Denjin: Punkic Cyborgs (Japan, Alt)

static struct BurnRomInfo pce_pcdenjaRomDesc[] = {
	{ "PC Denjin - Punkic Cyborgs (Japan, Alt)(1992)(Hudson Soft).pce", 0x080000, 0x8fb4f228, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcdenja)
STD_ROM_FN(pce_pcdenja)

struct BurnDriver BurnDrvpce_pcdenja = {
	"pce_pcdenja", "pce_pcdenj", NULL, NULL, "1992",
	"PC Denjin: Punkic Cyborgs (Japan, Alt)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_pcdenjaRomInfo, pce_pcdenjaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Genjin: Pithecanthropus Computerurus (Japan)

static struct BurnRomInfo pce_pcgenjRomDesc[] = {
	{ "PC Genjin - Pithecanthropus Computerurus (Japan)(1989)(Hudson Soft).pce", 0x060000, 0x2cb5cd55, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcgenj)
STD_ROM_FN(pce_pcgenj)

struct BurnDriver BurnDrvpce_pcgenj = {
	"pce_pcgenj", NULL, NULL, NULL, "1989",
	"PC Genjin: Pithecanthropus Computerurus (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_pcgenjRomInfo, pce_pcgenjRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Genjin: Pithecanthropus Computerurus (Japan, Alt)

static struct BurnRomInfo pce_pcgenjaRomDesc[] = {
	{ "PC Genjin - Pithecanthropus Computerurus (Japan, Alt)(1989)(Hudson Soft).pce", 0x060000, 0x67b35e6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcgenja)
STD_ROM_FN(pce_pcgenja)

struct BurnDriver BurnDrvpce_pcgenja = {
	"pce_pcgenja", "pce_pcgenj", NULL, NULL, "1989",
	"PC Genjin: Pithecanthropus Computerurus (Japan, Alt)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_pcgenjaRomInfo, pce_pcgenjaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Genjin 2: Pithecanthropus Computerurus (Japan)

static struct BurnRomInfo pce_pcgenj2RomDesc[] = {
	{ "PC Genjin 2 - Pithecanthropus Computerurus (Japan)(1991)(Hudson Soft).pce", 0x080000, 0x3028f7ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcgenj2)
STD_ROM_FN(pce_pcgenj2)

struct BurnDriver BurnDrvpce_pcgenj2 = {
	"pce_pcgenj2", NULL, NULL, NULL, "1991",
	"PC Genjin 2: Pithecanthropus Computerurus (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_pcgenj2RomInfo, pce_pcgenj2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Genjin 3: Pithecanthropus Computerurus (Japan)

static struct BurnRomInfo pce_pcgenj3RomDesc[] = {
	{ "PC Genjin 3 - Pithecanthropus Computerurus (Japan)(1993)(Hudson Soft).pce", 0x100000, 0xa170b60e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcgenj3)
STD_ROM_FN(pce_pcgenj3)

struct BurnDriver BurnDrvpce_pcgenj3 = {
	"pce_pcgenj3", NULL, NULL, NULL, "1993",
	"PC Genjin 3: Pithecanthropus Computerurus (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_pcgenj3RomInfo, pce_pcgenj3RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Genjin 3: Pithecanthropus Computerurus (Taikenban) (Japan)

static struct BurnRomInfo pce_pcgenj3tRomDesc[] = {
	{ "PC Genjin 3 - Pithecanthropus Computerurus (Taikenban) (Japan)(1993)(Hudson Soft).pce", 0x0c0000, 0x6f6ed301, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcgenj3t)
STD_ROM_FN(pce_pcgenj3t)

struct BurnDriver BurnDrvpce_pcgenj3t = {
	"pce_pcgenj3t", NULL, NULL, NULL, "1993",
	"PC Genjin 3: Pithecanthropus Computerurus (Taikenban) (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_pcgenj3tRomInfo, pce_pcgenj3tRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// PC Pachi-slot (Japan)

static struct BurnRomInfo pce_pcpachiRomDesc[] = {
	{ "pc pachi-slot (japan).pce", 0x080000, 0x0aa88f33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pcpachi)
STD_ROM_FN(pce_pcpachi)

struct BurnDriver BurnDrvpce_pcpachi = {
	"pce_pcpachi", NULL, NULL, NULL, "1992",
	"PC Pachi-slot (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_CASINO, 0,
	PceGetZipName, pce_pcpachiRomInfo, pce_pcpachiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Populous (Japan)

static struct BurnRomInfo pce_populousRomDesc[] = {
	{ "populous (japan).pce", 0x080000, 0x083c956a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_populous)
STD_ROM_FN(pce_populous)

struct BurnDriver BurnDrvpce_populous = {
	"pce_populous", NULL, NULL, NULL, "1991",
	"Populous (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_populousRomInfo, pce_populousRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	populousInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Populous (Japan) (Alt)

static struct BurnRomInfo pce_populous1RomDesc[] = {
	{ "populous (japan) [a].pce", 0x080000, 0x0a9ade99, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_populous1)
STD_ROM_FN(pce_populous1)

struct BurnDriver BurnDrvpce_populous1 = {
	"pce_populous1", "pce_populous", NULL, NULL, "1991",
	"Populous (Japan) (Alt)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_populous1RomInfo, pce_populous1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	populousInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Drift (Japan)

static struct BurnRomInfo pce_pdriftRomDesc[] = {
	{ "power drift (japan).pce", 0x080000, 0x25e0f6e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pdrift)
STD_ROM_FN(pce_pdrift)

struct BurnDriver BurnDrvpce_pdrift = {
	"pce_pdrift", NULL, NULL, NULL, "1990",
	"Power Drift (Japan)\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_pdriftRomInfo, pce_pdriftRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Drift (Japan) (Alt)

static struct BurnRomInfo pce_pdrift1RomDesc[] = {
	{ "power drift (japan) [a].pce", 0x080000, 0x99e6d988, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pdrift1)
STD_ROM_FN(pce_pdrift1)

struct BurnDriver BurnDrvpce_pdrift1 = {
	"pce_pdrift1", "pce_pdrift", NULL, NULL, "1990",
	"Power Drift (Japan) (Alt)\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_pdrift1RomInfo, pce_pdrift1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Eleven (Japan)

static struct BurnRomInfo pce_power11RomDesc[] = {
	{ "power eleven (japan).pce", 0x060000, 0x3e647d8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_power11)
STD_ROM_FN(pce_power11)

struct BurnDriver BurnDrvpce_power11 = {
	"pce_power11", NULL, NULL, NULL, "1991",
	"Power Eleven (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSFOOTBALL, 0,
	PceGetZipName, pce_power11RomInfo, pce_power11RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Gate (Japan)

static struct BurnRomInfo pce_powergatRomDesc[] = {
	{ "power gate (japan).pce", 0x040000, 0xbe8b6e3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_powergat)
STD_ROM_FN(pce_powergat)

struct BurnDriver BurnDrvpce_powergat = {
	"pce_powergat", NULL, NULL, NULL, "1991",
	"Power Gate (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_powergatRomInfo, pce_powergatRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Golf (Japan)

static struct BurnRomInfo pce_pgolfRomDesc[] = {
	{ "power golf (japan).pce", 0x060000, 0xea324f07, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pgolf)
STD_ROM_FN(pce_pgolf)

struct BurnDriver BurnDrvpce_pgolf = {
	"pce_pgolf", NULL, NULL, NULL, "1989",
	"Power Golf (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pgolfRomInfo, pce_pgolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League '93 (Japan)

static struct BurnRomInfo pce_pleag93RomDesc[] = {
	{ "power league '93 (japan).pce", 0x0c0000, 0x7d3e6f33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleag93)
STD_ROM_FN(pce_pleag93)

struct BurnDriver BurnDrvpce_pleag93 = {
	"pce_pleag93", NULL, NULL, NULL, "1993",
	"Power League '93 (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleag93RomInfo, pce_pleag93RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League - All Star Version (Japan)

static struct BurnRomInfo pce_pleagasRomDesc[] = {
	{ "power league (all star version) (japan).pce", 0x040000, 0x04a85769, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleagas)
STD_ROM_FN(pce_pleagas)

struct BurnDriver BurnDrvpce_pleagas = {
	"pce_pleagas", NULL, NULL, NULL, "1988",
	"Power League - All Star Version (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleagasRomInfo, pce_pleagasRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League (Japan)

static struct BurnRomInfo pce_pleagueRomDesc[] = {
	{ "power league (japan).pce", 0x040000, 0x69180984, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleague)
STD_ROM_FN(pce_pleague)

struct BurnDriver BurnDrvpce_pleague = {
	"pce_pleague", NULL, NULL, NULL, "1988",
	"Power League (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleagueRomInfo, pce_pleagueRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League II (Japan)

static struct BurnRomInfo pce_pleag2RomDesc[] = {
	{ "power league ii (japan).pce", 0x060000, 0xc5fdfa89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleag2)
STD_ROM_FN(pce_pleag2)

struct BurnDriver BurnDrvpce_pleag2 = {
	"pce_pleag2", NULL, NULL, NULL, "1989",
	"Power League II (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleag2RomInfo, pce_pleag2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League III (Japan)

static struct BurnRomInfo pce_pleag3RomDesc[] = {
	{ "power league iii (japan).pce", 0x060000, 0x8aa4b220, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleag3)
STD_ROM_FN(pce_pleag3)

struct BurnDriver BurnDrvpce_pleag3 = {
	"pce_pleag3", NULL, NULL, NULL, "1990",
	"Power League III (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleag3RomInfo, pce_pleag3RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League IV (Japan)

static struct BurnRomInfo pce_pleag4RomDesc[] = {
	{ "power league iv (japan).pce", 0x080000, 0x30cc3563, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleag4)
STD_ROM_FN(pce_pleag4)

struct BurnDriver BurnDrvpce_pleag4 = {
	"pce_pleag4", NULL, NULL, NULL, "1991",
	"Power League IV (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleag4RomInfo, pce_pleag4RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power League V (Japan)

static struct BurnRomInfo pce_pleag5RomDesc[] = {
	{ "power league v (japan).pce", 0x0c0000, 0x8b61e029, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_pleag5)
STD_ROM_FN(pce_pleag5)

struct BurnDriver BurnDrvpce_pleag5 = {
	"pce_pleag5", NULL, NULL, NULL, "1992",
	"Power League V (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_pleag5RomInfo, pce_pleag5RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Sports (Japan)

static struct BurnRomInfo pce_psportsRomDesc[] = {
	{ "Power Sports (Japan)(1992)(Hudson Soft).pce", 0x080000, 0x29eec024, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_psports)
STD_ROM_FN(pce_psports)

struct BurnDriver BurnDrvpce_psports = {
	"pce_psports", NULL, NULL, NULL, "1992",
	"Power Sports (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_psportsRomInfo, pce_psportsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Tennis (Japan)

static struct BurnRomInfo pce_ptennisRomDesc[] = {
	{ "Power Tennis (Japan)(1993)(Hudson Soft).pce", 0x080000, 0x8def5aa1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ptennis)
STD_ROM_FN(pce_ptennis)

struct BurnDriver BurnDrvpce_ptennis = {
	"pce_ptennis", NULL, NULL, NULL, "1993",
	"Power Tennis (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_ptennisRomInfo, pce_ptennisRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pro Tennis World Court (Japan)

static struct BurnRomInfo pce_ptennwcRomDesc[] = {
	{ "Pro Tennis World Court (Japan)(1988)(Namcot).pce", 0x040000, 0x11a36745, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ptennwc)
STD_ROM_FN(pce_ptennwc)

struct BurnDriver BurnDrvpce_ptennwc = {
	"pce_ptennwc", NULL, NULL, NULL, "1988",
	"Pro Tennis World Court (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC | GBF_ADV, 0,
	PceGetZipName, pce_ptennwcRomInfo, pce_ptennwcRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pro Yakyuu World Stadium '91 (Japan)

static struct BurnRomInfo pce_proyak91RomDesc[] = {
	{ "Pro Yakyuu World Stadium '91 (Japan)(1991)(Namcot).pce", 0x040000, 0x66b167a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_proyak91)
STD_ROM_FN(pce_proyak91)

struct BurnDriver BurnDrvpce_proyak91 = {
	"pce_proyak91", NULL, NULL, NULL, "1991",
	"Pro Yakyuu World Stadium '91 (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_proyak91RomInfo, pce_proyak91RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pro Yakyuu World Stadium (Japan)

static struct BurnRomInfo pce_proyakRomDesc[] = {
	{ "Pro Yakyuu World Stadium (Japan)(1988)(Namcot).pce", 0x040000, 0x34e089a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_proyak)
STD_ROM_FN(pce_proyak)

struct BurnDriver BurnDrvpce_proyak = {
	"pce_proyak", NULL, NULL, NULL, "1988",
	"Pro Yakyuu World Stadium (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_proyakRomInfo, pce_proyakRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Psycho Chaser (Japan)

static struct BurnRomInfo pce_psychasRomDesc[] = {
	{ "Psycho Chaser (Japan)(1990)(Naxat Soft).pce", 0x040000, 0x03883ee8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_psychas)
STD_ROM_FN(pce_psychas)

struct BurnDriver BurnDrvpce_psychas = {
	"pce_psychas", NULL, NULL, NULL, "1990",
	"Psycho Chaser (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_psychasRomInfo, pce_psychasRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Puzzle Boy (Japan)

static struct BurnRomInfo pce_puzzlboyRomDesc[] = {
	{ "Puzzle Boy (Japan)(1991)(Nihon Telenet).pce", 0x040000, 0xfaa6e187, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_puzzlboy)
STD_ROM_FN(pce_puzzlboy)

struct BurnDriver BurnDrvpce_puzzlboy = {
	"pce_puzzlboy", NULL, NULL, NULL, "1991",
	"Puzzle Boy (Japan)\0", NULL, "Nihon Telenet", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_puzzlboyRomInfo, pce_puzzlboyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Puzznic (Japan)

static struct BurnRomInfo pce_puzznicRomDesc[] = {
	{ "Puzznic (Japan)(1990)(Taito).pce", 0x040000, 0x965c95b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_puzznic)
STD_ROM_FN(pce_puzznic)

struct BurnDriver BurnDrvpce_puzznic = {
	"pce_puzznic", NULL, NULL, NULL, "1990",
	"Puzznic (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_puzznicRomInfo, pce_puzznicRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Quiz Toukou Shashin (Japan)

static struct BurnRomInfo pce_quiztsRomDesc[] = {
	{ "Quiz Toukou Shashin (Japan)(1994)(Games Express).pce", 0x100000, 0xf2e6856d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_quizts)
STD_ROM_FN(pce_quizts)

struct BurnDriver BurnDrvpce_quizts = {
	"pce_quizts", NULL, NULL, NULL, "1994",
	"Quiz Toukou Shashin (Japan)\0", NULL, "Games Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_QUIZ, 0,
	PceGetZipName, pce_quiztsRomInfo, pce_quiztsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// R-Type - Part-1 (Japan)

static struct BurnRomInfo pce_rtypep1RomDesc[] = {
	{ "R-Type Part-1 (Japan)(1988)(Hudson Soft).pce", 0x040000, 0xcec3d28a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rtypep1)
STD_ROM_FN(pce_rtypep1)

struct BurnDriver BurnDrvpce_rtypep1 = {
	"pce_rtypep1", NULL, NULL, NULL, "1988",
	"R-Type - Part-1 (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_rtypep1RomInfo, pce_rtypep1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// R-Type - Part-2 (Japan)

static struct BurnRomInfo pce_rtypep2RomDesc[] = {
	{ "R-Type Part-2 (Japan)(1988)(Hudson Soft)", 0x040000, 0xf207ecae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rtypep2)
STD_ROM_FN(pce_rtypep2)

struct BurnDriver BurnDrvpce_rtypep2 = {
	"pce_rtypep2", NULL, NULL, NULL, "1988",
	"R-Type - Part-2 (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_rtypep2RomInfo, pce_rtypep2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// R-Type - Part-2 (Japan, v1.1)

static struct BurnRomInfo pce_rtypep2v11RomDesc[] = {
	{ "R-Type Part-2 v1.1 (Japan)(1988)(Hudson Soft)", 0x040000, 0x417b961d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rtypep2v11)
STD_ROM_FN(pce_rtypep2v11)

struct BurnDriver BurnDrvpce_rtypep2v11 = {
	"pce_rtypep2v11", "pce_rtypep2", NULL, NULL, "1988",
	"R-Type - Part-2 (Japan, v1.1)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_rtypep2v11RomInfo, pce_rtypep2v11RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Rabio Lepus Special (Japan)

static struct BurnRomInfo pce_rabiolepRomDesc[] = {
	{ "Rabio Lepus Special (Japan)(1990)(Video System).pce", 0x060000, 0xd8373de6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rabiolep)
STD_ROM_FN(pce_rabiolep)

struct BurnDriver BurnDrvpce_rabiolep = {
	"pce_rabiolep", NULL, NULL, NULL, "1990",
	"Rabio Lepus Special (Japan)\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_rabiolepRomInfo, pce_rabiolepRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Racing Damashii (Japan)

static struct BurnRomInfo pce_racindamRomDesc[] = {
	{ "racing damashii (japan).pce", 0x080000, 0x3e79734c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_racindam)
STD_ROM_FN(pce_racindam)

struct BurnDriver BurnDrvpce_racindam = {
	"pce_racindam", NULL, NULL, NULL, "1991",
	"Racing Damashii (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_racindamRomInfo, pce_racindamRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Raiden (Japan)

static struct BurnRomInfo pce_raidenRomDesc[] = {
	{ "Raiden (Japan)(1991)(Hudson Soft).pce", 0x0c0000, 0x850829f2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_raiden)
STD_ROM_FN(pce_raiden)

struct BurnDriver BurnDrvpce_raiden = {
	"pce_raiden", NULL, NULL, NULL, "1991",
	"Raiden (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_raidenRomInfo, pce_raidenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Rastan Saga II (Japan)

static struct BurnRomInfo pce_rastan2RomDesc[] = {
	{ "Rastan Saga II (Japan)(1990)(Taito).pce", 0x060000, 0x00c38e69, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rastan2)
STD_ROM_FN(pce_rastan2)

struct BurnDriver BurnDrvpce_rastan2 = {
	"pce_rastan2", NULL, NULL, NULL, "1990",
	"Rastan Saga II (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_rastan2RomInfo, pce_rastan2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Rock-on (Japan)

static struct BurnRomInfo pce_rockonRomDesc[] = {
	{ "Rock-on (Japan)(1989)(Big Club).pce", 0x060000, 0x2fd65312, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_rockon)
STD_ROM_FN(pce_rockon)

struct BurnDriver BurnDrvpce_rockon = {
	"pce_rockon", NULL, NULL, NULL, "1989",
	"Rock-on (Japan)\0", NULL, "Big Club", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_rockonRomInfo, pce_rockonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Ryuukyuu (Japan)

static struct BurnRomInfo pce_ryukyuRomDesc[] = {
	{ "Ryukyu (Japan)(1990)(Face).pce", 0x040000, 0x91e6896f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_ryukyu)
STD_ROM_FN(pce_ryukyu)

struct BurnDriver BurnDrvpce_ryukyu = {
	"pce_ryukyu", NULL, NULL, NULL, "1990",
	"Ryuukyuu (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_ryukyuRomInfo, pce_ryukyuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Sadakichi 7 Series - Hideyoshi no Ougon (Japan)

static struct BurnRomInfo pce_sadaki7RomDesc[] = {
	{ "Sadakichi 7 Series - Hideyoshi no Ougon (Japan)(1988)(Hudson Soft).pce", 0x040000, 0xf999356f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sadaki7)
STD_ROM_FN(pce_sadaki7)

struct BurnDriver BurnDrvpce_sadaki7 = {
	"pce_sadaki7", NULL, NULL, NULL, "1988",
	"Sadakichi 7 Series - Hideyoshi no Ougon (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ADV, 0,
	PceGetZipName, pce_sadaki7RomInfo, pce_sadaki7RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Saigo no Nindou - Ninja Spirit (Japan)

static struct BurnRomInfo pce_saigoninRomDesc[] = {
	{ "Saigo no Nindou - Ninja Spirit (Japan)(1990)(Irem).pce", 0x080000, 0x0590a156, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_saigonin)
STD_ROM_FN(pce_saigonin)

struct BurnDriver BurnDrvpce_saigonin = {
	"pce_saigonin", NULL, NULL, NULL, "1990",
	"Saigo no Nindou - Ninja Spirit (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_saigoninRomInfo, pce_saigoninRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Salamander (Japan)

static struct BurnRomInfo pce_salamandRomDesc[] = {
	{ "Salamander (Japan)(1991)(Konami).pce", 0x040000, 0xfaecce20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_salamand)
STD_ROM_FN(pce_salamand)

struct BurnDriver BurnDrvpce_salamand = {
	"pce_salamand", NULL, NULL, NULL, "1991",
	"Salamander (Japan)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT | GBF_VERSHOOT, 0,
	PceGetZipName, pce_salamandRomInfo, pce_salamandRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Salamander (PC-Engine Mini Ed.)

static struct BurnRomInfo pce_salamandminiRomDesc[] = {
	{ "Salamander (PC-Engine Mini Ed.)(2020)(Konami).pce", 0x100000, 0x83b0c3a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_salamandmini)
STD_ROM_FN(pce_salamandmini)

struct BurnDriver BurnDrvpce_salamandmini = {
	"pce_salamandmini", "pce_salamand", NULL, NULL, "2020",
	"Salamander (PC-Engine Mini Ed.)\0", NULL, "Konami", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT | GBF_VERSHOOT, 0,
	PceGetZipName, pce_salamandminiRomInfo, pce_salamandminiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Sekigahara (Japan)

static struct BurnRomInfo pce_sekigahaRomDesc[] = {
	{ "Sekigahara (Japan)(1990)(Tonkin House).pce", 0x080000, 0x2e955051, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sekigaha)
STD_ROM_FN(pce_sekigaha)

struct BurnDriver BurnDrvpce_sekigaha = {
	"pce_sekigaha", NULL, NULL, NULL, "1990",
	"Sekigahara (Japan)\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_sekigahaRomInfo, pce_sekigahaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Sengoku Mahjong (Japan)

static struct BurnRomInfo pce_sengokmjRomDesc[] = {
	{ "Sengoku Mahjong (Japan)(1988)(Hudson Soft).pce", 0x040000, 0x90e6bf49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sengokmj)
STD_ROM_FN(pce_sengokmj)

struct BurnDriver BurnDrvpce_sengokmj = {
	"pce_sengokmj", NULL, NULL, NULL, "1988",
	"Sengoku Mahjong (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_sengokmjRomInfo, pce_sengokmjRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Shanghai (Japan)

static struct BurnRomInfo pce_shanghaiRomDesc[] = {
	{ "Shanghai (Japan)(1987)(Hudson Soft).pce", 0x020000, 0x6923d736, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shanghai)
STD_ROM_FN(pce_shanghai)

struct BurnDriver BurnDrvpce_shanghai = {
	"pce_shanghai", NULL, NULL, NULL, "1987",
	"Shanghai (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_shanghaiRomInfo, pce_shanghaiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Shinobi (Japan)

static struct BurnRomInfo pce_shinobiRomDesc[] = {
	{ "Shinobi (Japan)(1989)(Asmik).pce", 0x060000, 0xbc655cf3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shinobi)
STD_ROM_FN(pce_shinobi)

struct BurnDriver BurnDrvpce_shinobi = {
	"pce_shinobi", NULL, NULL, NULL, "1989",
	"Shinobi (Japan)\0", NULL, "Asmik", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_shinobiRomInfo, pce_shinobiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Shiryou Sensen (Japan)

static struct BurnRomInfo pce_shiryoRomDesc[] = {
	{ "Shiryou Sensen (Japan)(1989)(Victor).pce", 0x040000, 0x469a0fdf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shiryo)
STD_ROM_FN(pce_shiryo)

struct BurnDriver BurnDrvpce_shiryo = {
	"pce_shiryo", NULL, NULL, NULL, "1989",
	"Shiryou Sensen (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_shiryoRomInfo, pce_shiryoRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Shougi Shodan Icchokusen (Japan)

static struct BurnRomInfo pce_shogisiRomDesc[] = {
	{ "Shougi Shodan Icchokusen (Japan)(1990)(Home Data).pce", 0x040000, 0x23ec8970, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shogisi)
STD_ROM_FN(pce_shogisi)

struct BurnDriver BurnDrvpce_shogisi = {
	"pce_shogisi", NULL, NULL, NULL, "1990",
	"Shougi Shodan Icchokusen (Japan)\0", NULL, "Home Data", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_shogisiRomInfo, pce_shogisiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Shougi Shoshinsha Muyou (Japan)

static struct BurnRomInfo pce_shogismRomDesc[] = {
	{ "Shougi Shoshinsha Muyou (Japan)(1991)(Home Data).pce", 0x040000, 0x457f2bc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shogism)
STD_ROM_FN(pce_shogism)

struct BurnDriver BurnDrvpce_shogism = {
	"pce_shogism", NULL, NULL, NULL, "1991",
	"Shougi Shoshinsha Muyou (Japan)\0", NULL, "Home Data", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_shogismRomInfo, pce_shogismRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Side Arms - Hyper Dyne (Japan)

static struct BurnRomInfo pce_sidearmsRomDesc[] = {
	{ "Side Arms - Hyper Dyne (Japan)(1989)(NEC Avenue).pce", 0x040000, 0xe5e7b8b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sidearms)
STD_ROM_FN(pce_sidearms)

struct BurnDriver BurnDrvpce_sidearms = {
	"pce_sidearms", NULL, NULL, NULL, "1989",
	"Side Arms - Hyper Dyne (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_sidearmsRomInfo, pce_sidearmsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 352, 242, 4, 3
};


// Silent Debuggers (Japan)

static struct BurnRomInfo pce_silentdRomDesc[] = {
	{ "Silent Debuggers (Japan)(1991)(Data East).pce", 0x080000, 0x616ea179, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_silentd)
STD_ROM_FN(pce_silentd)

struct BurnDriver BurnDrvpce_silentd = {
	"pce_silentd", NULL, NULL, NULL, "1991",
	"Silent Debuggers (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT | GBF_ADV, 0,
	PceGetZipName, pce_silentdRomInfo, pce_silentdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Sindibad Chitei no Dai Makyuu (Japan)

static struct BurnRomInfo pce_sindibadRomDesc[] = {
	{ "Sindibad Chitei no Dai Makyuu (Japan)(1990)(IGS).pce", 0x060000, 0xb5c4eebd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sindibad)
STD_ROM_FN(pce_sindibad)

struct BurnDriver BurnDrvpce_sindibad = {
	"pce_sindibad", NULL, NULL, NULL, "1990",
	"Sindibad Chitei no Dai Makyuu (Japan)\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_sindibadRomInfo, pce_sindibadRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Skweek (Japan)

static struct BurnRomInfo pce_skweekRomDesc[] = {
	{ "Skweek (Japan)(1991)(Victor).pce", 0x040000, 0x4d539c9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_skweek)
STD_ROM_FN(pce_skweek)

struct BurnDriver BurnDrvpce_skweek = {
	"pce_skweek", NULL, NULL, NULL, "1991",
	"Skweek (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_skweekRomInfo, pce_skweekRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Sokoban World (Japan)

static struct BurnRomInfo pce_sokobanRomDesc[] = {
	{ "soukoban world (japan).pce", 0x020000, 0xfb37ddc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sokoban)
STD_ROM_FN(pce_sokoban)

struct BurnDriver BurnDrvpce_sokoban = {
	"pce_sokoban", NULL, NULL, NULL, "1990",
	"Sokoban World (Japan)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_sokobanRomInfo, pce_sokobanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Soldier Blade (Japan)

static struct BurnRomInfo pce_soldbladRomDesc[] = {
	{ "soldier blade (japan).pce", 0x080000, 0x8420b12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_soldblad)
STD_ROM_FN(pce_soldblad)

struct BurnDriver BurnDrvpce_soldblad = {
	"pce_soldblad", NULL, NULL, NULL, "1992",
	"Soldier Blade (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_soldbladRomInfo, pce_soldbladRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Soldier Blade Special - Caravan Stage (Japan)

static struct BurnRomInfo pce_soldblasRomDesc[] = {
	{ "soldier blade special - caravan stage (japan).pce", 0x080000, 0xf39f38ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_soldblas)
STD_ROM_FN(pce_soldblas)

struct BurnDriver BurnDrvpce_soldblas = {
	"pce_soldblas", NULL, NULL, NULL, "1992",
	"Soldier Blade Special - Caravan Stage (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_soldblasRomInfo, pce_soldblasRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Son Son II (Japan)

static struct BurnRomInfo pce_sonson2RomDesc[] = {
	{ "son son ii (japan).pce", 0x040000, 0xd7921df2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sonson2)
STD_ROM_FN(pce_sonson2)

struct BurnDriver BurnDrvpce_sonson2 = {
	"pce_sonson2", NULL, NULL, NULL, "1989",
	"Son Son II (Japan)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_sonson2RomInfo, pce_sonson2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Space Harrier (Japan)

static struct BurnRomInfo pce_sharrierRomDesc[] = {
	{ "space harrier (japan).pce", 0x080000, 0x64580427, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sharrier)
STD_ROM_FN(pce_sharrier)

struct BurnDriver BurnDrvpce_sharrier = {
	"pce_sharrier", NULL, NULL, NULL, "1988",
	"Space Harrier (Japan)\0", NULL, "NEC", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_sharrierRomInfo, pce_sharrierRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Space Invaders - Fukkatsu no Hi (Japan)

static struct BurnRomInfo pce_spaceinvRomDesc[] = {
	{ "Space Invaders - Fukkatsu no Hi (Japan)(1990)(Taito).pce", 0x040000, 0x99496db3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_spaceinv)
STD_ROM_FN(pce_spaceinv)

struct BurnDriver BurnDrvpce_spaceinv = {
	"pce_spaceinv", NULL, NULL, NULL, "1990",
	"Space Invaders - Fukkatsu no Hi (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_spaceinvRomInfo, pce_spaceinvRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Special Criminal Investigation (Japan)

static struct BurnRomInfo pce_sciRomDesc[] = {
	{ "special criminal investigation (japan).pce", 0x080000, 0x09a0bfcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sci)
STD_ROM_FN(pce_sci)

struct BurnDriver BurnDrvpce_sci = {
	"pce_sci", NULL, NULL, NULL, "1991",
	"Special Criminal Investigation (Japan)\0", NULL, "Taito", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT | GBF_RACING, 0,
	PceGetZipName, pce_sciRomInfo, pce_sciRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Spin Pair (Japan)

static struct BurnRomInfo pce_spinpairRomDesc[] = {
	{ "spin pair (japan).pce", 0x040000, 0x1c6ff459, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_spinpair)
STD_ROM_FN(pce_spinpair)

struct BurnDriver BurnDrvpce_spinpair = {
	"pce_spinpair", NULL, NULL, NULL, "1990",
	"Spin Pair (Japan)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_spinpairRomInfo, pce_spinpairRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Spiral Wave (Japan)

static struct BurnRomInfo pce_spirwaveRomDesc[] = {
	{ "spiral wave (japan).pce", 0x080000, 0xa5290dd0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_spirwave)
STD_ROM_FN(pce_spirwave)

struct BurnDriver BurnDrvpce_spirwave = {
	"pce_spirwave", NULL, NULL, NULL, "1991",
	"Spiral Wave (Japan)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT | GBF_ADV, 0,
	PceGetZipName, pce_spirwaveRomInfo, pce_spirwaveRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Splatterhouse (Japan)

static struct BurnRomInfo pce_splatthRomDesc[] = {
	{ "Splatterhouse (Japan)(1990)(Namcot).pce", 0x080000, 0x6b319457, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_splatth)
STD_ROM_FN(pce_splatth)

struct BurnDriver BurnDrvpce_splatth = {
	"pce_splatth", NULL, NULL, NULL, "1990",
	"Splatterhouse (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_splatthRomInfo, pce_splatthRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Stratego (Japan)

static struct BurnRomInfo pce_strategoRomDesc[] = {
	{ "Stratego (Japan)(1992)(Victor).pce", 0x040000, 0x727f4656, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_stratego)
STD_ROM_FN(pce_stratego)

struct BurnDriver BurnDrvpce_stratego = {
	"pce_stratego", NULL, NULL, NULL, "1992",
	"Stratego (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_strategoRomInfo, pce_strategoRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Street Fighter II' - Champion Edition (Japan)

static struct BurnRomInfo pce_sf2ceRomDesc[] = {
	{ "street fighter ii' - champion edition (japan).pce", 0x280000, 0xd15cb6bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sf2ce)
STD_ROM_FN(pce_sf2ce)

struct BurnDriver BurnDrvpce_sf2ce = {
	"pce_sf2ce", NULL, NULL, NULL, "1993",
	"Street Fighter II' - Champion Edition (Japan)\0", NULL, "NEC - Capcom", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_sf2ceRomInfo, pce_sf2ceRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Strip Fighter II (Japan)

static struct BurnRomInfo pce_stripf2RomDesc[] = {
	{ "strip fighter ii (japan).pce", 0x100000, 0xd6fc51ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_stripf2)
STD_ROM_FN(pce_stripf2)

struct BurnDriver BurnDrvpce_stripf2 = {
	"pce_stripf2", NULL, NULL, NULL, "19??",
	"Strip Fighter II (Japan)\0", NULL, "Game Express", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_stripf2RomInfo, pce_stripf2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Metal Crusher (Japan)

static struct BurnRomInfo pce_smcrushRomDesc[] = {
	{ "super metal crusher (japan).pce", 0x040000, 0x56488b36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_smcrush)
STD_ROM_FN(pce_smcrush)

struct BurnDriver BurnDrvpce_smcrush = {
	"pce_smcrush", NULL, NULL, NULL, "1991",
	"Super Metal Crusher (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_smcrushRomInfo, pce_smcrushRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Momotarou Dentetsu (Japan)

static struct BurnRomInfo pce_smomoRomDesc[] = {
	{ "super momotarou dentetsu (japan).pce", 0x060000, 0x3eb5304a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_smomo)
STD_ROM_FN(pce_smomo)

struct BurnDriver BurnDrvpce_smomo = {
	"pce_smomo", NULL, NULL, NULL, "1989",
	"Super Momotarou Dentetsu (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pce_smomoRomInfo, pce_smomoRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Momotarou Dentetsu II (Japan)

static struct BurnRomInfo pce_smomo2RomDesc[] = {
	{ "super momotarou dentetsu ii (japan).pce", 0x0c0000, 0x2bc023fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_smomo2)
STD_ROM_FN(pce_smomo2)

struct BurnDriver BurnDrvpce_smomo2 = {
	"pce_smomo2", NULL, NULL, NULL, "1991",
	"Super Momotarou Dentetsu II (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pce_smomo2RomInfo, pce_smomo2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Star Soldier (Japan)

static struct BurnRomInfo pce_sssoldrRomDesc[] = {
	{ "super star soldier (japan).pce", 0x080000, 0x5d0e3105, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sssoldr)
STD_ROM_FN(pce_sssoldr)

struct BurnDriver BurnDrvpce_sssoldr = {
	"pce_sssoldr", NULL, NULL, NULL, "1990",
	"Super Star Soldier (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_sssoldrRomInfo, pce_sssoldrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Volleyball (Japan)

static struct BurnRomInfo pce_svolleyRomDesc[] = {
	{ "super volleyball (japan).pce", 0x040000, 0xce2e4f9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_svolley)
STD_ROM_FN(pce_svolley)

struct BurnDriver BurnDrvpce_svolley = {
	"pce_svolley", NULL, NULL, NULL, "1990",
	"Super Volleyball (Japan)\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_svolleyRomInfo, pce_svolleyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Susanoo Densetsu (Japan)

static struct BurnRomInfo pce_susanoRomDesc[] = {
	{ "susanoo densetsu (japan).pce", 0x080000, 0xcf73d8fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_susano)
STD_ROM_FN(pce_susano)

struct BurnDriver BurnDrvpce_susano = {
	"pce_susano", NULL, NULL, NULL, "1989",
	"Susanoo Densetsu (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RPG, 0,
	PceGetZipName, pce_susanoRomInfo, pce_susanoRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Taito Chase H.Q. (Japan)

static struct BurnRomInfo pce_chasehqRomDesc[] = {
	{ "Taito Chase H.Q. (Japan)(1990)(Taito).pce", 0x060000, 0x6f4fd790, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_chasehq)
STD_ROM_FN(pce_chasehq)

struct BurnDriver BurnDrvpce_chasehq = {
	"pce_chasehq", NULL, NULL, NULL, "1990",
	"Taito Chase H.Q. (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_chasehqRomInfo, pce_chasehqRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Takahashi Meijin no Shin Boukenjima (Japan)

static struct BurnRomInfo pce_takameibRomDesc[] = {
	{ "Takahashi Meijin no Shin Boukenjima (Japan)(1992)(Hudson Soft).pce", 0x080000, 0xe415ea19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_takameib)
STD_ROM_FN(pce_takameib)

struct BurnDriver BurnDrvpce_takameib = {
	"pce_takameib", NULL, NULL, NULL, "1992",
	"Takahashi Meijin no Shin Boukenjima (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_takameibRomInfo, pce_takameibRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Takeda Shingen (Japan)

static struct BurnRomInfo pce_shingenRomDesc[] = {
	{ "Takeda Shingen (Japan)(1989)(Aicom).pce", 0x040000, 0xf022be13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shingen)
STD_ROM_FN(pce_shingen)

struct BurnDriver BurnDrvpce_shingen = {
	"pce_shingen", NULL, NULL, NULL, "1989",
	"Takeda Shingen (Japan)\0", NULL, "Aicom Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_shingenRomInfo, pce_shingenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Takeda Shingen (Japan, Alt)

static struct BurnRomInfo pce_shingen1RomDesc[] = {
	{ "Takeda Shingen (Japan, Alt)(1989)(Aicom).pce", 0x040000, 0xdf7af71c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_shingen1)
STD_ROM_FN(pce_shingen1)

struct BurnDriver BurnDrvpce_shingen1 = {
	"pce_shingen1", "pce_shingen", NULL, NULL, "1989",
	"Takeda Shingen (Japan, Alt)\0", NULL, "Aicom Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_shingen1RomInfo, pce_shingen1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tatsujin (Japan)

static struct BurnRomInfo pce_tatsujinRomDesc[] = {
	{ "Tatsujin (Japan)(1992)(Taito).pce", 0x080000, 0xa6088275, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tatsujin)
STD_ROM_FN(pce_tatsujin)

struct BurnDriver BurnDrvpce_tatsujin = {
	"pce_tatsujin", NULL, NULL, NULL, "1992",
	"Tatsujin (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_tatsujinRomInfo, pce_tatsujinRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tatsujin (Japan, Prototype)

static struct BurnRomInfo pce_tatsujinpRomDesc[] = {
	{ "Tatsujin (Japan, Proto)(1992)(Taito).pce", 0x080000, 0xc1b26659, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tatsujinp)
STD_ROM_FN(pce_tatsujinp)

struct BurnDriver BurnDrvpce_tatsujinp = {
	"pce_tatsujinp", "pce_tatsujin", NULL, NULL, "1992",
	"Tatsujin (Japan, Prototype)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_tatsujinpRomInfo, pce_tatsujinpRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tatsu no Ko Fighter (Japan)

static struct BurnRomInfo pce_tatsunokRomDesc[] = {
	{ "Tatsu no Ko Fighter (Japan)(1989)(Tonkin House).pce", 0x040000, 0xeeb6dd43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tatsunok)
STD_ROM_FN(pce_tatsunok)

struct BurnDriver BurnDrvpce_tatsunok = {
	"pce_tatsunok", NULL, NULL, NULL, "1989",
	"Tatsu no Ko Fighter (Japan)\0", NULL, "Tonkin House", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM | GBF_ADV, 0,
	PceGetZipName, pce_tatsunokRomInfo, pce_tatsunokRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tenseiryuu: Saint Dragon (Japan)

static struct BurnRomInfo pce_sdragonRomDesc[] = {
	{ "Tenseiryuu - Saint Dragon (Japan)(1990)(Aicom).pce", 0x060000, 0x2e278ccb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_sdragon)
STD_ROM_FN(pce_sdragon)

struct BurnDriver BurnDrvpce_sdragon = {
	"pce_sdragon", NULL, NULL, NULL, "1990",
	"Tenseiryuu: Saint Dragon (Japan)\0", NULL, "Aicom Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_sdragonRomInfo, pce_sdragonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Terra Cresta II: Mandoraa no Gyakushuu (Japan)

static struct BurnRomInfo pce_terracr2RomDesc[] = {
	{ "Terra Cresta II - Mandoraa no Gyakushuu (Japan)(1992)(Nihon Bussan).pce", 0x080000, 0x1b2d0077, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_terracr2)
STD_ROM_FN(pce_terracr2)

struct BurnDriver BurnDrvpce_terracr2 = {
	"pce_terracr2", NULL, NULL, NULL, "1992",
	"Terra Cresta II: Mandoraa no Gyakushuu (Japan)\0", NULL, "Nihon Bussan", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_terracr2RomInfo, pce_terracr2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Thunder Blade (Japan)

static struct BurnRomInfo pce_tbladeRomDesc[] = {
	{ "Thunder Blade (Japan)(1990)(NEC Avenue).pce", 0x080000, 0xddc3e809, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tblade)
STD_ROM_FN(pce_tblade)

struct BurnDriver BurnDrvpce_tblade = {
	"pce_tblade", NULL, NULL, NULL, "1990",
	"Thunder Blade (Japan)\0", NULL, "NEC Avenue", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT | GBF_SHOOT, 0,
	PceGetZipName, pce_tbladeRomInfo, pce_tbladeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Time Cruise II (Japan)

static struct BurnRomInfo pce_timcrus2RomDesc[] = {
	{ "Time Cruise II (Japan)(1991)(Face).pce", 0x080000, 0xcfec1d6a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_timcrus2)
STD_ROM_FN(pce_timcrus2)

struct BurnDriver BurnDrvpce_timcrus2 = {
	"pce_timcrus2", NULL, NULL, NULL, "1991",
	"Time Cruise II (Japan)\0", NULL, "Face Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PINBALL, 0,
	PceGetZipName, pce_timcrus2RomInfo, pce_timcrus2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Titan (Japan)

static struct BurnRomInfo pce_titanRomDesc[] = {
	{ "Titan (Japan)(1991)(Naxat Soft).pce", 0x040000, 0xd20f382f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_titan)
STD_ROM_FN(pce_titan)

struct BurnDriver BurnDrvpce_titan = {
	"pce_titan", NULL, NULL, NULL, "1991",
	"Titan (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_BREAKOUT, 0,
	PceGetZipName, pce_titanRomInfo, pce_titanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Toilet Kids (Japan)

static struct BurnRomInfo pce_toiletkRomDesc[] = {
	{ "Toilet Kids (Japan)(1992)(Media Rings Corporation).pce", 0x080000, 0x53b7784b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_toiletk)
STD_ROM_FN(pce_toiletk)

struct BurnDriver BurnDrvpce_toiletk = {
	"pce_toiletk", NULL, NULL, NULL, "1992",
	"Toilet Kids (Japan)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_toiletkRomInfo, pce_toiletkRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tora e no Michi (Japan)

static struct BurnRomInfo pce_toramichRomDesc[] = {
	{ "Tora e no Michi (Japan)(1990)(Victor).pce", 0x060000, 0x82ae3b16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_toramich)
STD_ROM_FN(pce_toramich)

struct BurnDriver BurnDrvpce_toramich = {
	"pce_toramich", NULL, NULL, NULL, "1990",
	"Tora e no Michi (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	PceGetZipName, pce_toramichRomInfo, pce_toramichRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Toshi Tensou Keikaku - Eternal City (Japan)

static struct BurnRomInfo pce_etercityRomDesc[] = {
	{ "Toshi Tensou Keikaku - Eternal City (Japan)(1991)(Naxat Soft).pce", 0x060000, 0xb18d102d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_etercity)
STD_ROM_FN(pce_etercity)

struct BurnDriver BurnDrvpce_etercity = {
	"pce_etercity", NULL, NULL, NULL, "1991",
	"Toshi Tensou Keikaku - Eternal City (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN | GBF_PLATFORM, 0,
	PceGetZipName, pce_etercityRomInfo, pce_etercityRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tower of Druaga, The (Japan)

static struct BurnRomInfo pce_druagaRomDesc[] = {
	{ "Tower of Druaga, The (Japan)(1992)(Namcot).pce", 0x080000, 0x72e00bc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_druaga)
STD_ROM_FN(pce_druaga)

struct BurnDriver BurnDrvpce_druaga = {
	"pce_druaga", NULL, NULL, NULL, "1992",
	"Tower of Druaga, The (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_druagaRomInfo, pce_druagaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tower of Druaga, The (Hack, English v1.01)
// https://www.romhacking.net/translations/7190/
static struct BurnRomInfo pce_druagaeRomDesc[] = {
	{ "Tower of Druaga, The T-Eng v1.01 (2024)(Pennywise).pce", 1048576, 0x1914ad86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_druagae)
STD_ROM_FN(pce_druagae)

struct BurnDriver BurnDrvpce_druagae = {
	"pce_druagae", "pce_druaga", NULL, NULL, "2024",
	"Tower of Druaga, The (Hack, English v1.01)\0", NULL, "Pennywise", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_ADV, 0,
	PceGetZipName, pce_druagaeRomInfo, pce_druagaeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Toy Shop Boys (Japan)

static struct BurnRomInfo pce_toyshopbRomDesc[] = {
	{ "Toy Shop Boys (Japan)(1990)(Victor).pce", 0x040000, 0x97c5ee9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_toyshopb)
STD_ROM_FN(pce_toyshopb)

struct BurnDriver BurnDrvpce_toyshopb = {
	"pce_toyshopb", NULL, NULL, NULL, "1990",
	"Toy Shop Boys (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_toyshopbRomInfo, pce_toyshopbRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tricky (Japan)

static struct BurnRomInfo pce_trickyRomDesc[] = {
	{ "Tricky (Japan)(1991)(IGS).pce", 0x040000, 0x3aea2f8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tricky)
STD_ROM_FN(pce_tricky)

struct BurnDriver BurnDrvpce_tricky = {
	"pce_tricky", NULL, NULL, NULL, "1991",
	"Tricky (Japan)\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_trickyRomInfo, pce_trickyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tsuppari Oozumou - Heisei Ban (Japan)

static struct BurnRomInfo pce_tsuppariRomDesc[] = {
	{ "Tsuppari Oozumou - Heisei Ban (Japan)(1993)(Naxat Soft).pce", 0x040000, 0x61a6e210, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tsuppari)
STD_ROM_FN(pce_tsuppari)

struct BurnDriver BurnDrvpce_tsuppari = {
	"pce_tsuppari", NULL, NULL, NULL, "1993",
	"Tsuppari Oozumou - Heisei Ban (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_VSFIGHT, 0,
	PceGetZipName, pce_tsuppariRomInfo, pce_tsuppariRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Tsuru Teruhito no Jissen Kabushiki Bai Bai Game (Japan)

static struct BurnRomInfo pce_baibaiRomDesc[] = {
	{ "Tsuru Teruhito no Jissen Kabushiki Bai Bai Game (Japan)(1989)(Intec).pce", 0x040000, 0xf70112e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_baibai)
STD_ROM_FN(pce_baibai)

struct BurnDriver BurnDrvpce_baibai = {
	"pce_baibai", NULL, NULL, NULL, "1989",
	"Tsuru Teruhito no Jissen Kabushiki Bai Bai Game (Japan)\0", NULL, "Intec", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_QUIZ, 0,
	PceGetZipName, pce_baibaiRomInfo, pce_baibaiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// TV Sports Basketball (Japan)

static struct BurnRomInfo pce_tvbasketRomDesc[] = {
	{ "TV Sports Basketball (Japan)(1993)(Victor).pce", 0x080000, 0x10b60601, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tvbasket)
STD_ROM_FN(pce_tvbasket)

struct BurnDriver BurnDrvpce_tvbasket = {
	"pce_tvbasket", NULL, NULL, NULL, "1993",
	"TV Sports Basketball (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_tvbasketRomInfo, pce_tvbasketRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// TV Sports Football (Japan)

static struct BurnRomInfo pce_tvfootblRomDesc[] = {
	{ "TV Sports Football (Japan)(1991)(Victor).pce", 0x060000, 0x968d908a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tvfootbl)
STD_ROM_FN(pce_tvfootbl)

struct BurnDriver BurnDrvpce_tvfootbl = {
	"pce_tvfootbl", NULL, NULL, NULL, "1991",
	"TV Sports Football (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_tvfootblRomInfo, pce_tvfootblRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// TV Sports Hockey (Japan)

static struct BurnRomInfo pce_tvhockeyRomDesc[] = {
	{ "TV Sports Hockey (Japan)(1993)(Victor).pce", 0x060000, 0xe7529890, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tvhockey)
STD_ROM_FN(pce_tvhockey)

struct BurnDriver BurnDrvpce_tvhockey = {
	"pce_tvhockey", NULL, NULL, NULL, "1993",
	"TV Sports Hockey (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_tvhockeyRomInfo, pce_tvhockeyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// USA Pro Basketball (Japan)

static struct BurnRomInfo pce_usaprobsRomDesc[] = {
	{ "USA Pro Basketball (Japan)(1989)(Aicom).pce", 0x040000, 0x1cad4b7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_usaprobs)
STD_ROM_FN(pce_usaprobs)

struct BurnDriver BurnDrvpce_usaprobs = {
	"pce_usaprobs", NULL, NULL, NULL, "1989",
	"USA Pro Basketball (Japan)\0", NULL, "Aicom Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_usaprobsRomInfo, pce_usaprobsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Veigues - Tactical Gladiator (Japan)

static struct BurnRomInfo pce_veiguesRomDesc[] = {
	{ "Veigues - Tactical Gladiator (Japan)(1990)(Victor).pce", 0x060000, 0x04188c5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_veigues)
STD_ROM_FN(pce_veigues)

struct BurnDriver BurnDrvpce_veigues = {
	"pce_veigues", NULL, NULL, NULL, "1990",
	"Veigues - Tactical Gladiator (Japan)\0", NULL, "Victor Interactive Software", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_veiguesRomInfo, pce_veiguesRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Victory Run (Japan)

static struct BurnRomInfo pce_victoryrRomDesc[] = {
	{ "Victory Run (Japan)(1987)(Hudson Soft).pce", 0x040000, 0x03e28cff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_victoryr)
STD_ROM_FN(pce_victoryr)

struct BurnDriver BurnDrvpce_victoryr = {
	"pce_victoryr", NULL, NULL, NULL, "1987",
	"Victory Run (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_victoryrRomInfo, pce_victoryrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Vigilante (Japan)

static struct BurnRomInfo pce_vigilantRomDesc[] = {
	{ "Vigilante (Japan)(1989)(Irem).pce", 0x060000, 0xe4124fe0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_vigilant)
STD_ROM_FN(pce_vigilant)

struct BurnDriver BurnDrvpce_vigilant = {
	"pce_vigilant", NULL, NULL, NULL, "1989",
	"Vigilante (Japan)\0", NULL, "Irem Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_vigilantRomInfo, pce_vigilantRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Violent Soldier (Japan)

static struct BurnRomInfo pce_violentsRomDesc[] = {
	{ "Violent Soldier (Japan)(1990)(IGS).pce", 0x060000, 0x1bc36b36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_violents)
STD_ROM_FN(pce_violents)

struct BurnDriver BurnDrvpce_violents = {
	"pce_violents", NULL, NULL, NULL, "1990",
	"Violent Soldier (Japan)\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_violentsRomInfo, pce_violentsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Volfied (Japan)

static struct BurnRomInfo pce_volfiedRomDesc[] = {
	{ "Volfied (Japan)(1989)(Taito).pce", 0x060000, 0xad226f30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_volfied)
STD_ROM_FN(pce_volfied)

struct BurnDriver BurnDrvpce_volfied = {
	"pce_volfied", NULL, NULL, NULL, "1989",
	"Volfied (Japan)\0", NULL, "Taito Corp.", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION | GBF_PUZZLE, 0,
	PceGetZipName, pce_volfiedRomInfo, pce_volfiedRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// W-ring - The Double Rings (Japan)

static struct BurnRomInfo pce_wringRomDesc[] = {
	{ "W-Ring - The Double Rings (Japan)(1990)(Naxat Soft).pce", 0x060000, 0xbe990010, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wring)
STD_ROM_FN(pce_wring)

struct BurnDriver BurnDrvpce_wring = {
	"pce_wring", NULL, NULL, NULL, "1990",
	"W-ring - The Double Rings (Japan)\0", NULL, "Naxat Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_wringRomInfo, pce_wringRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Wai Wai Mahjong - Yukaina Janyuu Tachi (Japan)

static struct BurnRomInfo pce_waiwaimjRomDesc[] = {
	{ "Wai Wai Mahjong - Yukaina Janyuu Tachi (Japan)(1989)(Video System).pce", 0x040000, 0xa2a0776e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_waiwaimj)
STD_ROM_FN(pce_waiwaimj)

struct BurnDriver BurnDrvpce_waiwaimj = {
	"pce_waiwaimj", NULL, NULL, NULL, "1989",
	"Wai Wai Mahjong - Yukaina Janyuu Tachi (Japan)\0", NULL, "Video System", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_MAHJONG, 0,
	PceGetZipName, pce_waiwaimjRomInfo, pce_waiwaimjRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Walkuere no Densetsu (Japan)

static struct BurnRomInfo pce_valkyrieRomDesc[] = {
	{ "Walkuere no Densetsu (Japan)(1990)(Namcot).pce", 0x080000, 0xa3303978, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_valkyrie)
STD_ROM_FN(pce_valkyrie)

struct BurnDriver BurnDrvpce_valkyrie = {
	"pce_valkyrie", NULL, NULL, NULL, "1990",
	"Valkyrie no Densetsu (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_valkyrieRomInfo, pce_valkyrieRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Valkyrie no Densetsu (Hack, English)
// https://www.romhacking.net/translations/2637/
static struct BurnRomInfo pce_valkyrieeRomDesc[] = {
	{ "Valkyrie no Densetsu T-Eng (2015)(cabbage).pce", 0x080000, 0x97a62bd1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_valkyriee)
STD_ROM_FN(pce_valkyriee)

struct BurnDriver BurnDrvpce_valkyriee = {
	"pce_valkyriee", "pce_valkyrie", NULL, NULL, "2015",
	"Valkyrie no Densetsu (Hack, English)\0", NULL, "cabbage", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RUNGUN, 0,
	PceGetZipName, pce_valkyrieeRomInfo, pce_valkyrieeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Wallaby!! - Usagi no Kuni no Kangaroo Race (Japan)

static struct BurnRomInfo pce_wallabyRomDesc[] = {
	{ "Wallaby!! - Usagi no Kuni no Kangaroo Race (Japan)(1990)(NCS - Masaya).pce", 0x060000, 0x0112d0c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wallaby)
STD_ROM_FN(pce_wallaby)

struct BurnDriver BurnDrvpce_wallaby = {
	"pce_wallaby", NULL, NULL, NULL, "1990",
	"Wallaby!! - Usagi no Kuni no Kangaroo Race (Japan)\0", NULL, "NCS - Masaya", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_STRATEGY, 0,
	PceGetZipName, pce_wallabyRomInfo, pce_wallabyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Winning Shot (Japan)

static struct BurnRomInfo pce_winshotRomDesc[] = {
	{ "Winning Shot (Japan)(1989)(Data East).pce", 0x040000, 0x9b5ebc58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_winshot)
STD_ROM_FN(pce_winshot)

struct BurnDriver BurnDrvpce_winshot = {
	"pce_winshot", NULL, NULL, NULL, "1989",
	"Winning Shot (Japan)\0", NULL, "Data East", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_winshotRomInfo, pce_winshotRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Wonder Momo (Japan)

static struct BurnRomInfo pce_wondermRomDesc[] = {
	{ "Wonder Momo (Japan)(1989)(Namcot).pce", 0x040000, 0x59d07314, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wonderm)
STD_ROM_FN(pce_wonderm)

struct BurnDriver BurnDrvpce_wonderm = {
	"pce_wonderm", NULL, NULL, NULL, "1989",
	"Wonder Momo (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_wondermRomInfo, pce_wondermRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	wondermomoInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// World Beach Volley (Japan)

static struct BurnRomInfo pce_wbeachRomDesc[] = {
	{ "World Beach Volley (Japan)(1990)(IGS).pce", 0x040000, 0xbe850530, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wbeach)
STD_ROM_FN(pce_wbeach)

struct BurnDriver BurnDrvpce_wbeach = {
	"pce_wbeach", NULL, NULL, NULL, "1990",
	"World Beach Volley (Japan)\0", NULL, "IGS", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_wbeachRomInfo, pce_wbeachRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// World Circuit (Japan)

static struct BurnRomInfo pce_wcircuitRomDesc[] = {
	{ "World Circuit (Japan)(1991)(Namcot).pce", 0x040000, 0xb3eeea2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wcircuit)
STD_ROM_FN(pce_wcircuit)

struct BurnDriver BurnDrvpce_wcircuit = {
	"pce_wcircuit", NULL, NULL, NULL, "1991",
	"World Circuit (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_wcircuitRomInfo, pce_wcircuitRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// World Jockey (Japan)

static struct BurnRomInfo pce_wjockeyRomDesc[] = {
	{ "World Jockey (Japan)(1991)(Namcot).pce", 0x040000, 0xa9ab2954, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_wjockey)
STD_ROM_FN(pce_wjockey)

struct BurnDriver BurnDrvpce_wjockey = {
	"pce_wjockey", NULL, NULL, NULL, "1991",
	"World Jockey (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_wjockeyRomInfo, pce_wjockeyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Xevious - Fardraut Densetsu (Japan)

static struct BurnRomInfo pce_xeviousRomDesc[] = {
	{ "Xevious - Fardraut Densetsu (Japan)(1990)(Namcot).pce", 0x040000, 0xf8f85eec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_xevious)
STD_ROM_FN(pce_xevious)

struct BurnDriver BurnDrvpce_xevious = {
	"pce_xevious", NULL, NULL, NULL, "1990",
	"Xevious - Fardraut Densetsu (Japan)\0", NULL, "Namcot", "PC Engine",
	L"Xevious - Fardraut Densetsu (Japan)\0\u30bc\u30d3\u30a6\u30b9 \u30d5\u30a1\u30fc\u30c9\u30e9\u30a6\u30c8\u4f1d\u8aac\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_xeviousRomInfo, pce_xeviousRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Xevious - Fardraut Densetsu (Hack, Simplified Chinese v1.0)
// https://www.1dzj.top/index.php/post/186.html
// 20250330
static struct BurnRomInfo pce_xeviousscRomDesc[] = {
	{ "Xevious - Fardraut Densetsu T-Chs v1.0 (2025)(Feiyue Wujin Ankong).pce", 0x040000, 0xdcd579cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_xevioussc)
STD_ROM_FN(pce_xevioussc)

struct BurnDriver BurnDrvpce_xevioussc = {
	"pce_xevioussc", "pce_xevious", NULL, NULL, "2025",
	"Xevious - Fardraut Densetsu (Hack, Simplified Chinese v1.0)\0", NULL, "Feiyue Wujin Ankong", "PC Engine",
	NULL, NULL, L"\u98de\u8dc3\u65e0\u5c3d\u6697\u7a7a", NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_xeviousscRomInfo, pce_xeviousscRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Youkai Douchuuki (Japan)

static struct BurnRomInfo pce_youkaidRomDesc[] = {
	{ "Youkai Douchuuki (Japan)(1988)(Namcot).pce", 0x040000, 0x80c3f824, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_youkaid)
STD_ROM_FN(pce_youkaid)

struct BurnDriver BurnDrvpce_youkaid = {
	"pce_youkaid", NULL, NULL, NULL, "1988",
	"Youkai Douchuuki (Japan)\0", NULL, "Namcot", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_youkaidRomInfo, pce_youkaidRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Yuu Yuu Jinsei - Victory Life (Japan)

static struct BurnRomInfo pce_yuyuRomDesc[] = {
	{ "Yuu Yuu Jinsei - Victory Life (Japan)(1988)(Hudson Soft).pce", 0x040000, 0xc0905ca9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_yuyu)
STD_ROM_FN(pce_yuyu)

struct BurnDriver BurnDrvpce_yuyu = {
	"pce_yuyu", NULL, NULL, NULL, "1988",
	"Yuu Yuu Jinsei - Victory Life (Japan)\0", NULL, "Hudson Soft", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_PCENGINE, GBF_BOARD, 0,
	PceGetZipName, pce_yuyuRomInfo, pce_yuyuRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Zero4 Champ (Japan)

static struct BurnRomInfo pce_zero4caRomDesc[] = {
	{ "Zero4 Champ (Japan)(1991)(Media Rings Corporation).pce", 0x080000, 0xee156721, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_zero4ca)
STD_ROM_FN(pce_zero4ca)

struct BurnDriver BurnDrvpce_zero4ca = {
	"pce_zero4ca", "pce_zero4c", NULL, NULL, "1991",
	"Zero4 Champ (Japan)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RACING | GBF_ADV, 0,
	PceGetZipName, pce_zero4caRomInfo, pce_zero4caRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Zero4 Champ (Japan, v1.5)

static struct BurnRomInfo pce_zero4cRomDesc[] = {
	{ "Zero4 Champ v1.5 (Japan)(1991)(Media Rings Corporation).pce", 0x080000, 0xb77f2e2f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_zero4c)
STD_ROM_FN(pce_zero4c)

struct BurnDriver BurnDrvpce_zero4c = {
	"pce_zero4c", NULL, NULL, NULL, "1991",
	"Zero4 Champ (Japan, v1.5)\0", NULL, "Media Rings Corporation", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RACING | GBF_ADV, 0,
	PceGetZipName, pce_zero4cRomInfo, pce_zero4cRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Zero4 Champ (Hack, English)
// https://www.romhacking.net/translations/7399/
static struct BurnRomInfo pce_zero4ceRomDesc[] = {
	{ "Zero4 Champ T-Eng (2025)(Supper).pce", 688128, 0xde8da942, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_zero4ce)
STD_ROM_FN(pce_zero4ce)

struct BurnDriver BurnDrvpce_zero4ce = {
	"pce_zero4ce", "pce_zero4c", NULL, NULL, "2025",
	"Zero4 Champ (Hack, English)\0", NULL, "Supper", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PCENGINE_PCENGINE, GBF_RACING | GBF_ADV, 0,
	PceGetZipName, pce_zero4ceRomInfo, pce_zero4ceRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Zipang (Japan)

static struct BurnRomInfo pce_zipangRomDesc[] = {
	{ "Zipang (Japan)(1990)(Pack-In-Video).pce", 0x040000, 0x67aab7a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_zipang)
STD_ROM_FN(pce_zipang)

struct BurnDriver BurnDrvpce_zipang = {
	"pce_zipang", NULL, NULL, NULL, "1990",
	"Zipang (Japan)\0", NULL, "Pack-In-Video", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_zipangRomInfo, pce_zipangRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// -------------------
// TurboGrafx-16 Games
// -------------------


// Aero Blasters (USA)

static struct BurnRomInfo tg_aeroblstRomDesc[] = {
	{ "Aero Blasters (USA)(1990)(NEC - Hudson Soft).pce", 0x080000, 0xb03e0b32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_aeroblst)
STD_ROM_FN(tg_aeroblst)

struct BurnDriver BurnDrvtg_aeroblst = {
	"tg_aeroblst", NULL, NULL, NULL, "1990",
	"Aero Blasters (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_aeroblstRomInfo, tg_aeroblstRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Air Zonk (USA)

static struct BurnRomInfo tg_airzonkRomDesc[] = {
	{ "Air Zonk (USA)(1992)(Hudson Soft).pce", 0x080000, 0x933d5bcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_airzonk)
STD_ROM_FN(tg_airzonk)

struct BurnDriver BurnDrvtg_airzonk = {
	"tg_airzonk", NULL, NULL, NULL, "1992",
	"Air Zonk (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_airzonkRomInfo, tg_airzonkRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Alien Crush (USA)

static struct BurnRomInfo tg_acrushRomDesc[] = {
	{ "Alien Crush (USA)(1989)(NEC, Hudson Soft).pce", 0x040000, 0xea488494, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_acrush)
STD_ROM_FN(tg_acrush)

struct BurnDriver BurnDrvtg_acrush = {
	"tg_acrush", NULL, NULL, NULL, "1989",
	"Alien Crush (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PINBALL, 0,
	TgGetZipName, tg_acrushRomInfo, tg_acrushRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ballistix (USA)

static struct BurnRomInfo tg_ballistxRomDesc[] = {
	{ "Ballistix (USA)(1991)(NEC - Psygnosis).pce", 0x040000, 0x420fa189, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_ballistx)
STD_ROM_FN(tg_ballistx)

struct BurnDriver BurnDrvtg_ballistx = {
	"tg_ballistx", NULL, NULL, NULL, "1991",
	"Ballistix (USA)\0", NULL, "NEC - Psygnosis", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_ACTION, 0,
	TgGetZipName, tg_ballistxRomInfo, tg_ballistxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Battle Royale (USA)

static struct BurnRomInfo tg_batlroylRomDesc[] = {
	{ "Battle Royale (USA)(1990)(NEC).pce", 0x080000, 0xe70b01af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_batlroyl)
STD_ROM_FN(tg_batlroyl)

struct BurnDriver BurnDrvtg_batlroyl = {
	"tg_batlroyl", NULL, NULL, NULL, "1990",
	"Battle Royale (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_VSFIGHT, 0,
	TgGetZipName, tg_batlroylRomInfo, tg_batlroylRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Blazing Lazers (USA)

static struct BurnRomInfo tg_blazlazrRomDesc[] = {
	{ "Blazing Lazers (USA)(1989)(NEC - Hudson Soft).pce", 0x060000, 0xb4a1b0f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_blazlazr)
STD_ROM_FN(tg_blazlazr)

struct BurnDriver BurnDrvtg_blazlazr = {
	"tg_blazlazr", NULL, NULL, NULL, "1989",
	"Blazing Lazers (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_blazlazrRomInfo, tg_blazlazrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bloody Wolf (USA)

static struct BurnRomInfo tg_blodwolfRomDesc[] = {
	{ "Bloody wolf (USA)(1990)(NEC - Data East).pce", 0x080000, 0x37baf6bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_blodwolf)
STD_ROM_FN(tg_blodwolf)

struct BurnDriver BurnDrvtg_blodwolf = {
	"tg_blodwolf", NULL, NULL, NULL, "1990",
	"Bloody Wolf (USA)\0", NULL, "NEC - Data East", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_RUNGUN, 0,
	TgGetZipName, tg_blodwolfRomInfo, tg_blodwolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman '93 (USA)

static struct BurnRomInfo tg_bombmn93RomDesc[] = {
	{ "Bomberman '93 (USA)(1990)(Hudson Soft).pce", 0x080000, 0x56171c1c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bombmn93)
STD_ROM_FN(tg_bombmn93)

struct BurnDriver BurnDrvtg_bombmn93 = {
	"tg_bombmn93", NULL, NULL, NULL, "1990",
	"Bomberman '93 (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_ACTION | GBF_MAZE, 0,
	TgGetZipName, tg_bombmn93RomInfo, tg_bombmn93RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bomberman (USA)

static struct BurnRomInfo tg_bombmanRomDesc[] = {
	{ "Bomberman (USA)(1991)(NEC - Hudson Soft).pce", 0x040000, 0x5f6f3c2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bombman)
STD_ROM_FN(tg_bombman)

struct BurnDriver BurnDrvtg_bombman = {
	"tg_bombman", NULL, NULL, NULL, "1991",
	"Bomberman (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_ACTION | GBF_MAZE, 0,
	TgGetZipName, tg_bombmanRomInfo, tg_bombmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bonk III - Bonk's Big Adventure (USA)

static struct BurnRomInfo tg_bonk3RomDesc[] = {
	{ "Bonk III - Bonk's Big Adventure (USA)(1993)(Hudson Soft).pce", 0x100000, 0x5a3f76d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bonk3)
STD_ROM_FN(tg_bonk3)

struct BurnDriver BurnDrvtg_bonk3 = {
	"tg_bonk3", NULL, NULL, NULL, "1993",
	"Bonk III - Bonk's Big Adventure (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_bonk3RomInfo, tg_bonk3RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bonk's Adventure (USA)

static struct BurnRomInfo tg_bonkRomDesc[] = {
	{ "Bonk's Adventure (USA)(1990)(NEC - Hudson Soft).pce", 0x060000, 0x599ead9b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bonk)
STD_ROM_FN(tg_bonk)

struct BurnDriver BurnDrvtg_bonk = {
	"tg_bonk", NULL, NULL, NULL, "1990",
	"Bonk's Adventure (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_bonkRomInfo, tg_bonkRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bonk's Revenge (USA)

static struct BurnRomInfo tg_bonk2RomDesc[] = {
	{ "Bonk's Revenge (USA)(1991)(NEC - Hudson Soft).pce", 0x080000, 0x14250f9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bonk2)
STD_ROM_FN(tg_bonk2)

struct BurnDriver BurnDrvtg_bonk2 = {
	"tg_bonk2", NULL, NULL, NULL, "1991",
	"Bonk's Revenge (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_bonk2RomInfo, tg_bonk2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Boxyboy (USA)

static struct BurnRomInfo tg_boxyboyRomDesc[] = {
	{ "Boxyboy (USA)(1990)(NEC).pce", 0x020000, 0x605be213, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_boxyboy)
STD_ROM_FN(tg_boxyboy)

struct BurnDriver BurnDrvtg_boxyboy = {
	"tg_boxyboy", NULL, NULL, NULL, "1990",
	"Boxyboy (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PUZZLE, 0,
	TgGetZipName, tg_boxyboyRomInfo, tg_boxyboyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Bravoman (USA)

static struct BurnRomInfo tg_bravomanRomDesc[] = {
	{ "Bravoman (USA)(1990)(NEC).pce", 0x080000, 0xcca08b02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_bravoman)
STD_ROM_FN(tg_bravoman)

struct BurnDriver BurnDrvtg_bravoman = {
	"tg_bravoman", NULL, NULL, NULL, "1990",
	"Bravoman (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	TgGetZipName, tg_bravomanRomInfo, tg_bravomanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cadash (USA)

static struct BurnRomInfo tg_cadashRomDesc[] = {
	{ "Cadash (USA)(1991)(Working Designs).pce", 0x080000, 0xbb0b3aef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_cadash)
STD_ROM_FN(tg_cadash)

struct BurnDriver BurnDrvtg_cadash = {
	"tg_cadash", NULL, NULL, NULL, "1991",
	"Cadash (USA)\0", "Bad graphics", "Working Designs", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_RPG, 0,
	TgGetZipName, tg_cadashRomInfo, tg_cadashRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Champions Forever Boxing (USA)

static struct BurnRomInfo tg_forevboxRomDesc[] = {
	{ "Champions Forever Boxing (USA)(1991)(NEC).pce", 0x080000, 0x15ee889a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_forevbox)
STD_ROM_FN(tg_forevbox)

struct BurnDriver BurnDrvtg_forevbox = {
	"tg_forevbox", NULL, NULL, NULL, "1991",
	"Champions Forever Boxing (USA)\0", "Bad sound", "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_VSFIGHT, 0,
	TgGetZipName, tg_forevboxRomInfo, tg_forevboxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Chew-Man-Fu (USA)

static struct BurnRomInfo tg_chewmanRomDesc[] = {
	{ "Chew-Man-Fu (USA)(1990)(NEC).pce", 0x040000, 0x8cd13e9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_chewman)
STD_ROM_FN(tg_chewman)

struct BurnDriver BurnDrvtg_chewman = {
	"tg_chewman", NULL, NULL, NULL, "1990",
	"Chew-Man-Fu (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_MAZE | GBF_ACTION, 0,
	TgGetZipName, tg_chewmanRomInfo, tg_chewmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// China Warrior (USA)

static struct BurnRomInfo tg_chinawarRomDesc[] = {
	{ "China Warrior (USA)(1989)(NEC).pce", 0x040000, 0xa2ee361d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_chinawar)
STD_ROM_FN(tg_chinawar)

struct BurnDriver BurnDrvtg_chinawar = {
	"tg_chinawar", NULL, NULL, NULL, "1989",
	"China Warrior (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT, 0,
	TgGetZipName, tg_chinawarRomInfo, tg_chinawarRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cratermaze (USA)

static struct BurnRomInfo tg_cratermzRomDesc[] = {
	{ "Cratermaze (USA)(1990)(NEC).pce", 0x040000, 0x9033e83a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_cratermz)
STD_ROM_FN(tg_cratermz)

struct BurnDriver BurnDrvtg_cratermz = {
	"tg_cratermz", NULL, NULL, NULL, "1990",
	"Cratermaze (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_MAZE | GBF_ACTION, 0,
	TgGetZipName, tg_cratermzRomInfo, tg_cratermzRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Cyber Core (USA)

static struct BurnRomInfo tg_cybrcoreRomDesc[] = {
	{ "Cyber Core (USA)(1990)(NEC).pce", 0x060000, 0x4cfb6e3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_cybrcore)
STD_ROM_FN(tg_cybrcore)

struct BurnDriver BurnDrvtg_cybrcore = {
	"tg_cybrcore", NULL, NULL, NULL, "1990",
	"Cyber Core (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_cybrcoreRomInfo, tg_cybrcoreRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Darkwing Duck (USA)

static struct BurnRomInfo tg_darkwingRomDesc[] = {
	{ "Darkwing Duck (USA)(1992)(TTI).pce", 0x080000, 0x4ac97606, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_darkwing)
STD_ROM_FN(tg_darkwing)

struct BurnDriver BurnDrvtg_darkwing = {
	"tg_darkwing", NULL, NULL, NULL, "1992",
	"Darkwing Duck (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_darkwingRomInfo, tg_darkwingRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Davis Cup Tennis (USA)

static struct BurnRomInfo tg_daviscupRomDesc[] = {
	{ "Davis Cup Tennis (USA)(1991)(NEC).pce", 0x080000, 0x9edab596, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_daviscup)
STD_ROM_FN(tg_daviscup)

struct BurnDriver BurnDrvtg_daviscup = {
	"tg_daviscup", NULL, NULL, NULL, "1991",
	"Davis Cup Tennis (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_daviscupRomInfo, tg_daviscupRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dead Moon (USA)

static struct BurnRomInfo tg_deadmoonRomDesc[] = {
	{ "Dead Moon (USA)(1992)(TTI).pce", 0x080000, 0xf5d98b0b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_deadmoon)
STD_ROM_FN(tg_deadmoon)

struct BurnDriver BurnDrvtg_deadmoon = {
	"tg_deadmoon", NULL, NULL, NULL, "1992",
	"Dead Moon (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_deadmoonRomInfo, tg_deadmoonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Deep Blue (USA)

static struct BurnRomInfo tg_deepblueRomDesc[] = {
	{ "Deep Blue (USA)(1989)(NEC).pce", 0x040000, 0x16b40b44, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_deepblue)
STD_ROM_FN(tg_deepblue)

struct BurnDriver BurnDrvtg_deepblue = {
	"tg_deepblue", NULL, NULL, NULL, "1989",
	"Deep Blue (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_deepblueRomInfo, tg_deepblueRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Devil's Crush - Naxat Pinball (USA)

static struct BurnRomInfo tg_devlcrshRomDesc[] = {
	{ "Devil's Crush - Naxat Pinball (USA)(1990)(NEC).pce", 0x060000, 0x157b4492, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_devlcrsh)
STD_ROM_FN(tg_devlcrsh)

struct BurnDriver BurnDrvtg_devlcrsh = {
	"tg_devlcrsh", NULL, NULL, NULL, "1990",
	"Devil's Crush - Naxat Pinball (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_PINBALL, 0,
	TgGetZipName, tg_devlcrshRomInfo, tg_devlcrshRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Double Dungeons - W (USA)

static struct BurnRomInfo tg_ddungwRomDesc[] = {
	{ "Double Dungeons - W (USA)(1990)(NEC - NCS).pce", 0x040000, 0x4a1a8c60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_ddungw)
STD_ROM_FN(tg_ddungw)

struct BurnDriver BurnDrvtg_ddungw = {
	"tg_ddungw", NULL, NULL, NULL, "1990",
	"Double Dungeons - W (USA)\0", NULL, "NEC - NCS", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_RPG | GBF_MAZE, 0,
	TgGetZipName, tg_ddungwRomInfo, tg_ddungwRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon Spirit (USA)

static struct BurnRomInfo tg_dspiritRomDesc[] = {
	{ "Dragon Spirit (USA)(1989)(NEC).pce", 0x040000, 0x086f148c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_dspirit)
STD_ROM_FN(tg_dspirit)

struct BurnDriver BurnDrvtg_dspirit = {
	"tg_dspirit", NULL, NULL, NULL, "1989",
	"Dragon Spirit (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_dspiritRomInfo, tg_dspiritRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dragon's Curse (USA)

static struct BurnRomInfo tg_dragcrseRomDesc[] = {
	{ "Dragon's Curse (USA)(1990)(NEC).pce", 0x040000, 0x7d2c4b09, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_dragcrse)
STD_ROM_FN(tg_dragcrse)

struct BurnDriver BurnDrvtg_dragcrse = {
	"tg_dragcrse", NULL, NULL, NULL, "1990",
	"Dragon's Curse (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM | GBF_ADV, 0,
	TgGetZipName, tg_dragcrseRomInfo, tg_dragcrseRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Drop Off (USA)

static struct BurnRomInfo tg_dropoffRomDesc[] = {
	{ "Drop Off (USA)(1990)(NEC).pce", 0x040000, 0xfea27b32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_dropoff)
STD_ROM_FN(tg_dropoff)

struct BurnDriver BurnDrvtg_dropoff = {
	"tg_dropoff", NULL, NULL, NULL, "1990",
	"Drop Off (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_BREAKOUT, 0,
	TgGetZipName, tg_dropoffRomInfo, tg_dropoffRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Dungeon Explorer (USA)

static struct BurnRomInfo tg_dungexplRomDesc[] = {
	{ "Dungeon Explorer (USA)(1989)(NEC).pce", 0x060000, 0x4ff01515, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_dungexpl)
STD_ROM_FN(tg_dungexpl)

struct BurnDriver BurnDrvtg_dungexpl = {
	"tg_dungexpl", NULL, NULL, NULL, "1989",
	"Dungeon Explorer (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_MAZE | GBF_RUNGUN, 0,
	TgGetZipName, tg_dungexplRomInfo, tg_dungexplRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Falcon (USA)

static struct BurnRomInfo tg_falconRomDesc[] = {
	{ "Falcon (USA)(1992)(TTI).pce", 0x080000, 0x0bc0a12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_falcon)
STD_ROM_FN(tg_falcon)

struct BurnDriver BurnDrvtg_falcon = {
	"tg_falcon", NULL, NULL, NULL, "1992",
	"Falcon (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SIM, 0,
	TgGetZipName, tg_falconRomInfo, tg_falconRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Fantasy Zone (USA)

static struct BurnRomInfo tg_fantzoneRomDesc[] = {
	{ "Fantasy Zone (USA)(1989)(NEC).pce", 0x040000, 0xe8c3573d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_fantzone)
STD_ROM_FN(tg_fantzone)

struct BurnDriver BurnDrvtg_fantzone = {
	"tg_fantzone", NULL, NULL, NULL, "1989",
	"Fantasy Zone (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_fantzoneRomInfo, tg_fantzoneRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Final Lap Twin (USA)

static struct BurnRomInfo tg_finallapRomDesc[] = {
	{ "Final Lap Twin (USA)(1990)(NEC).pce", 0x060000, 0x26408ea3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_finallap)
STD_ROM_FN(tg_finallap)

struct BurnDriver BurnDrvtg_finallap = {
	"tg_finallap", NULL, NULL, NULL, "1990",
	"Final Lap Twin (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_RACING, 0,
	TgGetZipName, tg_finallapRomInfo, tg_finallapRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Galaga '90 (USA)

static struct BurnRomInfo tg_galaga90RomDesc[] = {
	{ "Galaga '90 (USA)(1989)(NEC).pce", 0x040000, 0x2909dec6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_galaga90)
STD_ROM_FN(tg_galaga90)

struct BurnDriver BurnDrvtg_galaga90 = {
	"tg_galaga90", NULL, NULL, NULL, "1989",
	"Galaga '90 (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SHOOT, 0,
	TgGetZipName, tg_galaga90RomInfo, tg_galaga90RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ghost Manor (USA)

static struct BurnRomInfo tg_ghostmanRomDesc[] = {
	{ "Ghost Manor (USA)(1992)(TTI).pce", 0x080000, 0x2db4c1fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_ghostman)
STD_ROM_FN(tg_ghostman)

struct BurnDriver BurnDrvtg_ghostman = {
	"tg_ghostman", NULL, NULL, NULL, "1992",
	"Ghost Manor (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_ghostmanRomInfo, tg_ghostmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Gunboat (USA)

static struct BurnRomInfo tg_gunboatRomDesc[] = {
	{ "Gunboat (USA)(1992)(NEC).pce", 0x080000, 0xf370b58e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_gunboat)
STD_ROM_FN(tg_gunboat)

struct BurnDriver BurnDrvtg_gunboat = {
	"tg_gunboat", NULL, NULL, NULL, "1992",
	"Gunboat (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SIM, 0,
	TgGetZipName, tg_gunboatRomInfo, tg_gunboatRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Hit the Ice - VHL The Official Video Hockey League (USA)

static struct BurnRomInfo tg_hiticeRomDesc[] = {
	{ "Hit the Ice - VHL The Official Video Hockey League (USA)(1992)(TTI).pce", 0x060000, 0x8b29c3aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_hitice)
STD_ROM_FN(tg_hitice)

struct BurnDriver BurnDrvtg_hitice = {
	"tg_hitice", NULL, NULL, NULL, "1992",
	"Hit the Ice - VHL The Official Video Hockey League (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_hiticeRomInfo, tg_hiticeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Impossamole (USA)

static struct BurnRomInfo tg_impossamRomDesc[] = {
	{ "Impossamole (USA)(1991)(NEC - Gremlin Graphics).pce", 0x080000, 0xe2470f5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_impossam)
STD_ROM_FN(tg_impossam)

struct BurnDriver BurnDrvtg_impossam = {
	"tg_impossam", NULL, NULL, NULL, "1991",
	"Impossamole (USA)\0", NULL, "NEC - Gremlin Graphics", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_impossamRomInfo, tg_impossamRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// J.J. & Jeff (USA)

static struct BurnRomInfo tg_jjnjeffRomDesc[] = {
	{ "J.J. & Jeff (USA)(1990)(NEC - Hudson Soft).pce", 0x040000, 0xe01c5127, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_jjnjeff)
STD_ROM_FN(tg_jjnjeff)

struct BurnDriver BurnDrvtg_jjnjeff = {
	"tg_jjnjeff", NULL, NULL, NULL, "1990",
	"J.J. & Jeff (USA)\0", NULL, "NEC - Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_jjnjeffRomInfo, tg_jjnjeffRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jack Nicklaus' Turbo Golf (USA)

static struct BurnRomInfo tg_nicklausRomDesc[] = {
	{ "Jack Nicklaus' Turbo Golf (USA)(1990)(Accolade).pce", 0x040000, 0x83384572, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_nicklaus)
STD_ROM_FN(tg_nicklaus)

struct BurnDriver BurnDrvtg_nicklaus = {
	"tg_nicklaus", NULL, NULL, NULL, "1990",
	"Jack Nicklaus' Turbo Golf (USA)\0", NULL, "Accolade", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_nicklausRomInfo, tg_nicklausRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Jackie Chan's Action Kung Fu (USA)

static struct BurnRomInfo tg_jchanRomDesc[] = {
	{ "Jackie Chan's Action Kung Fu (USA)(1992)(Hudson Soft).pce", 0x080000, 0x9d2f6193, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_jchan)
STD_ROM_FN(tg_jchan)

struct BurnDriver BurnDrvtg_jchan = {
	"tg_jchan", NULL, NULL, NULL, "1992",
	"Jackie Chan's Action Kung Fu (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_jchanRomInfo, tg_jchanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Keith Courage in Alpha Zones (USA)

static struct BurnRomInfo tg_keithcorRomDesc[] = {
	{ "Keith Courage in Alpha Zones (USA)(1989)(NEC).pce", 0x040000, 0x474d7a72, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_keithcor)
STD_ROM_FN(tg_keithcor)

struct BurnDriver BurnDrvtg_keithcor = {
	"tg_keithcor", NULL, NULL, NULL, "1989",
	"Keith Courage in Alpha Zones (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_keithcorRomInfo, tg_keithcorRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// King of Casino (USA)

static struct BurnRomInfo tg_kingcasnRomDesc[] = {
	{ "King of Casino (USA)(1990)(NEC).pce", 0x040000, 0x2f2e2240, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_kingcasn)
STD_ROM_FN(tg_kingcasn)

struct BurnDriver BurnDrvtg_kingcasn = {
	"tg_kingcasn", NULL, NULL, NULL, "1990",
	"King of Casino (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_CASINO, 0,
	TgGetZipName, tg_kingcasnRomInfo, tg_kingcasnRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Klax (USA)

static struct BurnRomInfo tg_klaxRomDesc[] = {
	{ "Klax (USA)(1990)(Tengen).pce", 0x040000, 0x0f1b59b4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_klax)
STD_ROM_FN(tg_klax)

struct BurnDriver BurnDrvtg_klax = {
	"tg_klax", NULL, NULL, NULL, "1990",
	"Klax (USA)\0", NULL, "Tengen", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PUZZLE, 0,
	TgGetZipName, tg_klaxRomInfo, tg_klaxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Legend of Hero Tonma (USA)

static struct BurnRomInfo tg_lohtRomDesc[] = {
	{ "Legend of Hero Tonma (USA)(1991)(TTI).pce", 0x080000, 0x3c131486, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_loht)
STD_ROM_FN(tg_loht)

struct BurnDriver BurnDrvtg_loht = {
	"tg_loht", NULL, NULL, NULL, "1991",
	"Legend of Hero Tonma (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_lohtRomInfo, tg_lohtRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Legendary Axe II, The (USA)

static struct BurnRomInfo tg_legaxe2RomDesc[] = {
	{ "Legendary Axe II, The (USA)(1990)(NEC).pce", 0x040000, 0x220ebf91, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_legaxe2)
STD_ROM_FN(tg_legaxe2)

struct BurnDriver BurnDrvtg_legaxe2 = {
	"tg_legaxe2", NULL, NULL, NULL, "1990",
	"Legendary Axe II, The (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_legaxe2RomInfo, tg_legaxe2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Legendary Axe, The (USA)

static struct BurnRomInfo tg_legaxeRomDesc[] = {
	{ "Legendary Axe, The (USA)(1989)(NEC).pce", 0x040000, 0x2d211007, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_legaxe)
STD_ROM_FN(tg_legaxe)

struct BurnDriver BurnDrvtg_legaxe = {
	"tg_legaxe", NULL, NULL, NULL, "1989",
	"Legendary Axe, The (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_legaxeRomInfo, tg_legaxeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Magical Chase (USA)

static struct BurnRomInfo tg_magchaseRomDesc[] = {
	{ "Magical Chase (USA)(1993)(Quest).pce", 0x080000, 0x95cd2979, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_magchase)
STD_ROM_FN(tg_magchase)

struct BurnDriver BurnDrvtg_magchase = {
	"tg_magchase", NULL, NULL, NULL, "1993",
	"Magical Chase (USA)\0", NULL, "Quest", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_magchaseRomInfo, tg_magchaseRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Military Madness (USA)

static struct BurnRomInfo tg_miltrymdRomDesc[] = {
	{ "Military Madness (USA)(1989)(NEC).pce", 0x060000, 0x93f316f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_miltrymd)
STD_ROM_FN(tg_miltrymd)

struct BurnDriver BurnDrvtg_miltrymd = {
	"tg_miltrymd", NULL, NULL, NULL, "1989",
	"Military Madness (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_STRATEGY, 0,
	TgGetZipName, tg_miltrymdRomInfo, tg_miltrymdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Moto Roader (USA)

static struct BurnRomInfo tg_motoroadRomDesc[] = {
	{ "Moto Roader (USA)(1989)(NEC).pce", 0x040000, 0xe2b0d544, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_motoroad)
STD_ROM_FN(tg_motoroad)

struct BurnDriver BurnDrvtg_motoroad = {
	"tg_motoroad", NULL, NULL, NULL, "1989",
	"Moto Roader (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_RACING, 0,
	TgGetZipName, tg_motoroadRomInfo, tg_motoroadRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Neutopia (USA)

static struct BurnRomInfo tg_neutopiaRomDesc[] = {
	{ "Neutopia (USA)(1990)(NEC).pce", 0x060000, 0xa9a94e1b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_neutopia)
STD_ROM_FN(tg_neutopia)

struct BurnDriver BurnDrvtg_neutopia = {
	"tg_neutopia", NULL, NULL, NULL, "1990",
	"Neutopia (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_ACTION | GBF_ADV, 0,
	TgGetZipName, tg_neutopiaRomInfo, tg_neutopiaRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Neutopia II (USA)

static struct BurnRomInfo tg_neutopi2RomDesc[] = {
	{ "Neutopia II (USA)(1992)(NEC).pce", 0x0c0000, 0xc4ed4307, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_neutopi2)
STD_ROM_FN(tg_neutopi2)

struct BurnDriver BurnDrvtg_neutopi2 = {
	"tg_neutopi2", NULL, NULL, NULL, "1992",
	"Neutopia II (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_ACTION | GBF_ADV, 0,
	TgGetZipName, tg_neutopi2RomInfo, tg_neutopi2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// New Adventure Island (USA)

static struct BurnRomInfo tg_advislndRomDesc[] = {
	{ "New Adventure Island (USA)(1992)(Hudson Soft).pce", 0x080000, 0x756a1802, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_advislnd)
STD_ROM_FN(tg_advislnd)

struct BurnDriver BurnDrvtg_advislnd = {
	"tg_advislnd", NULL, NULL, NULL, "1992",
	"New Adventure Island (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_advislndRomInfo, tg_advislndRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Night Creatures (USA)

static struct BurnRomInfo tg_nightcrRomDesc[] = {
	{ "Night Creatures (USA)(1992)(NEC).pce", 0x080000, 0xc159761b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_nightcr)
STD_ROM_FN(tg_nightcr)

struct BurnDriver BurnDrvtg_nightcr = {
	"tg_nightcr", NULL, NULL, NULL, "1992",
	"Night Creatures (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_nightcrRomInfo, tg_nightcrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ninja Spirit (USA)

static struct BurnRomInfo tg_nspiritRomDesc[] = {
	{ "Ninja Spirit (USA)(1990)(NEC - Irem).pce", 0x080000, 0xde8af1c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_nspirit)
STD_ROM_FN(tg_nspirit)

struct BurnDriver BurnDrvtg_nspirit = {
	"tg_nspirit", NULL, NULL, NULL, "1990",
	"Ninja Spirit (USA)\0", NULL, "NEC - Irem", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_nspiritRomInfo, tg_nspiritRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Order of the Griffon (USA)

static struct BurnRomInfo tg_griffonRomDesc[] = {
	{ "Order of the Griffon (USA)(1992)(TTI).pce", 0x080000, 0xfae0fc60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_griffon)
STD_ROM_FN(tg_griffon)

struct BurnDriver BurnDrvtg_griffon = {
	"tg_griffon", NULL, NULL, NULL, "1992",
	"Order of the Griffon (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_RPG, 0,
	TgGetZipName, tg_griffonRomInfo, tg_griffonRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Ordyne (USA)

static struct BurnRomInfo tg_ordyneRomDesc[] = {
	{ "Ordyne (USA)(1989)(NEC).pce", 0x080000, 0xe7bf2a74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_ordyne)
STD_ROM_FN(tg_ordyne)

struct BurnDriver BurnDrvtg_ordyne = {
	"tg_ordyne", NULL, NULL, NULL, "1989",
	"Ordyne (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_ordyneRomInfo, tg_ordyneRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Pac-Land (USA)

static struct BurnRomInfo tg_paclandRomDesc[] = {
	{ "Pac-Land (USA)(1990)(NEC).pce", 0x040000, 0xd6e30ccd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_pacland)
STD_ROM_FN(tg_pacland)

struct BurnDriver BurnDrvtg_pacland = {
	"tg_pacland", NULL, NULL, NULL, "1990",
	"Pac-Land (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_paclandRomInfo, tg_paclandRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Panza Kick Boxing (USA)

static struct BurnRomInfo tg_panzakbRomDesc[] = {
	{ "Panza Kick Boxing (USA)(1991)(NEC).pce", 0x080000, 0xa980e0e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_panzakb)
STD_ROM_FN(tg_panzakb)

struct BurnDriver BurnDrvtg_panzakb = {
	"tg_panzakb", NULL, NULL, NULL, "1991",
	"Panza Kick Boxing (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_VSFIGHT, 0,
	TgGetZipName, tg_panzakbRomInfo, tg_panzakbRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Parasol Stars: The Story of Bubble Bobble III (USA)

static struct BurnRomInfo tg_parasolRomDesc[] = {
	{ "Parasol Stars - The Story of Bubble Bobble III (USA)(1991)(Working Designs).pce", 0x060000, 0xe6458212, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_parasol)
STD_ROM_FN(tg_parasol)

struct BurnDriver BurnDrvtg_parasol = {
	"tg_parasol", NULL, NULL, NULL, "1991",
	"Parasol Stars: The Story of Bubble Bobble III (USA)\0", NULL, "Working Designs", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_parasolRomInfo, tg_parasolRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Power Golf (USA)

static struct BurnRomInfo tg_pgolfRomDesc[] = {
	{ "Power Golf (USA)(1989)(NEC).pce", 0x060000, 0xed1d3843, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_pgolf)
STD_ROM_FN(tg_pgolf)

struct BurnDriver BurnDrvtg_pgolf = {
	"tg_pgolf", NULL, NULL, NULL, "1989",
	"Power Golf (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_pgolfRomInfo, tg_pgolfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Psychosis (USA)

static struct BurnRomInfo tg_psychosRomDesc[] = {
	{ "Psychosis (USA)(1990)(NEC).pce", 0x040000, 0x6cc10824, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_psychos)
STD_ROM_FN(tg_psychos)

struct BurnDriver BurnDrvtg_psychos = {
	"tg_psychos", NULL, NULL, NULL, "1990",
	"Psychosis (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_psychosRomInfo, tg_psychosRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// R-Type (USA)

static struct BurnRomInfo tg_rtypeRomDesc[] = {
	{ "R-Type (USA)(1989)(NEC).pce", 0x080000, 0x91ce5156, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_rtype)
STD_ROM_FN(tg_rtype)

struct BurnDriver BurnDrvtg_rtype = {
	"tg_rtype", NULL, NULL, NULL, "1989",
	"R-Type (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_rtypeRomInfo, tg_rtypeRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Raiden (USA)

static struct BurnRomInfo tg_raidenRomDesc[] = {
	{ "Raiden (USA)(1991)(Hudson Soft).pce", 0x0c0000, 0xbc59c31e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_raiden)
STD_ROM_FN(tg_raiden)

struct BurnDriver BurnDrvtg_raiden = {
	"tg_raiden", NULL, NULL, NULL, "1991",
	"Raiden (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_raidenRomInfo, tg_raidenRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Samurai-Ghost (USA)

static struct BurnRomInfo tg_samuraigRomDesc[] = {
	{ "Samurai-Ghost (USA)(1992)(TTI).pce", 0x080000, 0x77a924b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_samuraig)
STD_ROM_FN(tg_samuraig)

struct BurnDriver BurnDrvtg_samuraig = {
	"tg_samuraig", NULL, NULL, NULL, "1992",
	"Samurai-Ghost (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT, 0,
	TgGetZipName, tg_samuraigRomInfo, tg_samuraigRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 264, 240, 4, 3
};


// Shockman (USA)

static struct BurnRomInfo tg_shockmanRomDesc[] = {
	{ "Shockman (USA)(1992)(NEC).pce", 0x080000, 0x2774462c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_shockman)
STD_ROM_FN(tg_shockman)

struct BurnDriver BurnDrvtg_shockman = {
	"tg_shockman", NULL, NULL, NULL, "1992",
	"Shockman (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_RUNGUN | GBF_PLATFORM, 0,
	TgGetZipName, tg_shockmanRomInfo, tg_shockmanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Side Arms - Hyper Dyne (USA)

static struct BurnRomInfo tg_sidearmsRomDesc[] = {
	{ "Side Arms - Hyper Dyne (USA)(1989)(Radiance Software).pce", 0x040000, 0xd1993c9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_sidearms)
STD_ROM_FN(tg_sidearms)

struct BurnDriver BurnDrvtg_sidearms = {
	"tg_sidearms", NULL, NULL, NULL, "1989",
	"Side Arms - Hyper Dyne (USA)\0", NULL, "Radiance Software", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_sidearmsRomInfo, tg_sidearmsRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 352, 242, 4, 3
};


// Silent Debuggers (USA)

static struct BurnRomInfo tg_silentdRomDesc[] = {
	{ "Silent Debuggers (USA)(1991)(Hudson Soft).pce", 0x080000, 0xfa7e5d66, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_silentd)
STD_ROM_FN(tg_silentd)

struct BurnDriver BurnDrvtg_silentd = {
	"tg_silentd", NULL, NULL, NULL, "1991",
	"Silent Debuggers (USA)\0", NULL, "Hudson Soft", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SHOOT | GBF_ADV, 0,
	TgGetZipName, tg_silentdRomInfo, tg_silentdRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Sinistron (USA)

static struct BurnRomInfo tg_sinistrnRomDesc[] = {
	{ "Sinistron (USA)(1991)(NEC).pce", 0x060000, 0x4f6e2dbd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_sinistrn)
STD_ROM_FN(tg_sinistrn)

struct BurnDriver BurnDrvtg_sinistrn = {
	"tg_sinistrn", NULL, NULL, NULL, "1991",
	"Sinistron (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_sinistrnRomInfo, tg_sinistrnRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Soldier Blade (USA)

static struct BurnRomInfo tg_soldbladRomDesc[] = {
	{ "Soldier Blade (USA)(1992)(TTI).pce", 0x080000, 0x4bb68b13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_soldblad)
STD_ROM_FN(tg_soldblad)

struct BurnDriver BurnDrvtg_soldblad = {
	"tg_soldblad", NULL, NULL, NULL, "1992",
	"Soldier Blade (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_soldbladRomInfo, tg_soldbladRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Somer Assault (USA)

static struct BurnRomInfo tg_somerassRomDesc[] = {
	{ "Somer Assault (USA)(1992)(Atlus).pce", 0x080000, 0x8fcaf2e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_somerass)
STD_ROM_FN(tg_somerass)

struct BurnDriver BurnDrvtg_somerass = {
	"tg_somerass", NULL, NULL, NULL, "1992",
	"Somer Assault (USA)\0", NULL, "Atlus Co.", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_somerassRomInfo, tg_somerassRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Sonic Spike: World Championship Beach Volleyball (USA)

static struct BurnRomInfo tg_wbeachRomDesc[] = {
	{ "Sonic Spike - World Championship Beach Volleyball (USA)(1990)(NEC).pce", 0x040000, 0xf74e5eb3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_wbeach)
STD_ROM_FN(tg_wbeach)

struct BurnDriver BurnDrvtg_wbeach = {
	"tg_wbeach", NULL, NULL, NULL, "1990",
	"Sonic Spike: World Championship Beach Volleyball (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_wbeachRomInfo, tg_wbeachRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Space Harrier (USA)

static struct BurnRomInfo tg_sharrierRomDesc[] = {
	{ "Space Harrier (USA)(1989)(NEC).pce", 0x080000, 0x43b05eb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_sharrier)
STD_ROM_FN(tg_sharrier)

struct BurnDriver BurnDrvtg_sharrier = {
	"tg_sharrier", NULL, NULL, NULL, "1989",
	"Space Harrier (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SHOOT, 0,
	TgGetZipName, tg_sharrierRomInfo, tg_sharrierRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Splatterhouse (USA)

static struct BurnRomInfo tg_splatthRomDesc[] = {
	{ "Splatterhouse (USA)(1990)(NEC - Namco).pce", 0x080000, 0xd00ca74f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_splatth)
STD_ROM_FN(tg_splatth)

struct BurnDriver BurnDrvtg_splatth = {
	"tg_splatth", NULL, NULL, NULL, "1990",
	"Splatterhouse (USA)\0", NULL, "NEC - Namco", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT, 0,
	TgGetZipName, tg_splatthRomInfo, tg_splatthRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Star Soldier (USA)

static struct BurnRomInfo tg_sssoldrRomDesc[] = {
	{ "Super Star Soldier (USA)(1990)(NEC).pce", 0x080000, 0xdb29486f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_sssoldr)
STD_ROM_FN(tg_sssoldr)

struct BurnDriver BurnDrvtg_sssoldr = {
	"tg_sssoldr", NULL, NULL, NULL, "1990",
	"Super Star Soldier (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_VERSHOOT, 0,
	TgGetZipName, tg_sssoldrRomInfo, tg_sssoldrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Super Volleyball (USA)

static struct BurnRomInfo tg_svolleyRomDesc[] = {
	{ "Super Volleyball (USA)(1990)(NEC).pce", 0x040000, 0x245040b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_svolley)
STD_ROM_FN(tg_svolley)

struct BurnDriver BurnDrvtg_svolley = {
	"tg_svolley", NULL, NULL, NULL, "1990",
	"Super Volleyball (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_svolleyRomInfo, tg_svolleyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Taito Chase H.Q. (USA)

static struct BurnRomInfo tg_chasehqRomDesc[] = {
	{ "Taito Chase H.Q. (USA)(1992)(TTI).pce", 0x060000, 0x9298254c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_chasehq)
STD_ROM_FN(tg_chasehq)

struct BurnDriver BurnDrvtg_chasehq = {
	"tg_chasehq", NULL, NULL, NULL, "1992",
	"Taito Chase H.Q. (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_RACING, 0,
	TgGetZipName, tg_chasehqRomInfo, tg_chasehqRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Takin' It to the Hoop (USA)

static struct BurnRomInfo tg_taknhoopRomDesc[] = {
	{ "Takin' It to the Hoop (USA)(1989)(NEC - Aicom).pce", 0x040000, 0xe9d51797, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_taknhoop)
STD_ROM_FN(tg_taknhoop)

struct BurnDriver BurnDrvtg_taknhoop = {
	"tg_taknhoop", NULL, NULL, NULL, "1989",
	"Takin' It to the Hoop (USA)\0", NULL, "NEC - Aicom", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_taknhoopRomInfo, tg_taknhoopRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// TaleSpin (USA)

static struct BurnRomInfo tg_talespinRomDesc[] = {
	{ "TaleSpin (USA)(1991)(NEC).pce", 0x080000, 0xbae9cecc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_talespin)
STD_ROM_FN(tg_talespin)

struct BurnDriver BurnDrvtg_talespin = {
	"tg_talespin", NULL, NULL, NULL, "1991",
	"TaleSpin (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PLATFORM, 0,
	TgGetZipName, tg_talespinRomInfo, tg_talespinRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Tiger Road (USA)

static struct BurnRomInfo tg_tigerrodRomDesc[] = {
	{ "Tiger Road (USA)(1990)(NEC).pce", 0x060000, 0x985d492d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_tigerrod)
STD_ROM_FN(tg_tigerrod)

struct BurnDriver BurnDrvtg_tigerrod = {
	"tg_tigerrod", NULL, NULL, NULL, "1990",
	"Tiger Road (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	TgGetZipName, tg_tigerrodRomInfo, tg_tigerrodRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Time Cruise (USA)

static struct BurnRomInfo tg_timcrusRomDesc[] = {
	{ "Time Cruise (USA)(1992)(TTI).pce", 0x080000, 0x02c39660, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_timcrus)
STD_ROM_FN(tg_timcrus)

struct BurnDriver BurnDrvtg_timcrus = {
	"tg_timcrus", NULL, NULL, NULL, "1992",
	"Time Cruise (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PINBALL, 0,
	TgGetZipName, tg_timcrusRomInfo, tg_timcrusRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Timeball (USA)

static struct BurnRomInfo tg_timeballRomDesc[] = {
	{ "Timeball (USA)(1990)(NEC).pce", 0x020000, 0x5d395019, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_timeball)
STD_ROM_FN(tg_timeball)

struct BurnDriver BurnDrvtg_timeball = {
	"tg_timeball", NULL, NULL, NULL, "1990",
	"Timeball (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PUZZLE, 0,
	TgGetZipName, tg_timeballRomInfo, tg_timeballRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Tricky Kick (USA)

static struct BurnRomInfo tg_trickyRomDesc[] = {
	{ "Tricky Kick (USA)(1990)(NEC).pce", 0x040000, 0x48e6fd34, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_tricky)
STD_ROM_FN(tg_tricky)

struct BurnDriver BurnDrvtg_tricky = {
	"tg_tricky", NULL, NULL, NULL, "1990",
	"Tricky Kick (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_PUZZLE, 0,
	TgGetZipName, tg_trickyRomInfo, tg_trickyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Turrican (USA)

static struct BurnRomInfo tg_turricanRomDesc[] = {
	{ "Turrican (USA)(1991)(Ballistic).pce", 0x040000, 0xeb045edf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_turrican)
STD_ROM_FN(tg_turrican)

struct BurnDriver BurnDrvtg_turrican = {
	"tg_turrican", NULL, NULL, NULL, "1991",
	"Turrican (USA)\0", NULL, "Ballistic", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_RUNGUN | GBF_PLATFORM, 0,
	TgGetZipName, tg_turricanRomInfo, tg_turricanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 224, 4, 3
};

// TV Sports Basketball (USA)

static struct BurnRomInfo tg_tvbasketRomDesc[] = {
	{ "TV Sports Basketball (USA)(1991)(NEC - Cinemaware).pce", 0x080000, 0xea54d653, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_tvbasket)
STD_ROM_FN(tg_tvbasket)

struct BurnDriver BurnDrvtg_tvbasket = {
	"tg_tvbasket", NULL, NULL, NULL, "1991",
	"TV Sports Basketball (USA)\0", NULL, "NEC - Cinemaware", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_tvbasketRomInfo, tg_tvbasketRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// TV Sports Football (USA)

static struct BurnRomInfo tg_tvfootblRomDesc[] = {
	{ "TV Sports Football (USA)(1990)(NEC - Cinemaware).pce", 0x060000, 0x5e25b557, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_tvfootbl)
STD_ROM_FN(tg_tvfootbl)

struct BurnDriver BurnDrvtg_tvfootbl = {
	"tg_tvfootbl", NULL, NULL, NULL, "1990",
	"TV Sports Football (USA)\0", NULL, "NEC - Cinemaware", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_tvfootblRomInfo, tg_tvfootblRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// TV Sports Hockey (USA)

static struct BurnRomInfo tg_tvhockeyRomDesc[] = {
	{ "TV Sports Hockey (USA)(1991)(NEC).pce", 0x060000, 0x97fe5bcf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_tvhockey)
STD_ROM_FN(tg_tvhockey)

struct BurnDriver BurnDrvtg_tvhockey = {
	"tg_tvhockey", NULL, NULL, NULL, "1991",
	"TV Sports Hockey (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_tvhockeyRomInfo, tg_tvhockeyRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Veigues - Tactical Gladiator (USA)

static struct BurnRomInfo tg_veiguesRomDesc[] = {
	{ "Veigues - Tactical Gladiator (USA)(1990)(NEC).pce", 0x060000, 0x99d14fb7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_veigues)
STD_ROM_FN(tg_veigues)

struct BurnDriver BurnDrvtg_veigues = {
	"tg_veigues", NULL, NULL, NULL, "1990",
	"Veigues - Tactical Gladiator (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_veiguesRomInfo, tg_veiguesRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Victory Run (USA)

static struct BurnRomInfo tg_victoryrRomDesc[] = {
	{ "Victory Run (USA)(1989)(NEC).pce", 0x040000, 0x85cbd045, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_victoryr)
STD_ROM_FN(tg_victoryr)

struct BurnDriver BurnDrvtg_victoryr = {
	"tg_victoryr", NULL, NULL, NULL, "1989",
	"Victory Run (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_RACING, 0,
	TgGetZipName, tg_victoryrRomInfo, tg_victoryrRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Vigilante (USA)

static struct BurnRomInfo tg_vigilantRomDesc[] = {
	{ "Vigilante (USA)(1989)(NEC).pce", 0x060000, 0x79d49a0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_vigilant)
STD_ROM_FN(tg_vigilant)

struct BurnDriver BurnDrvtg_vigilant = {
	"tg_vigilant", NULL, NULL, NULL, "1989",
	"Vigilante (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_TG16, GBF_SCRFIGHT, 0,
	TgGetZipName, tg_vigilantRomInfo, tg_vigilantRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// World Class Baseball (USA)

static struct BurnRomInfo tg_wcbaseblRomDesc[] = {
	{ "World Class Baseball (USA)(1989)(NEC).pce", 0x040000, 0x4186d0c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_wcbasebl)
STD_ROM_FN(tg_wcbasebl)

struct BurnDriver BurnDrvtg_wcbasebl = {
	"tg_wcbasebl", NULL, NULL, NULL, "1989",
	"World Class Baseball (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_wcbaseblRomInfo, tg_wcbaseblRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// World Court Tennis (USA)

static struct BurnRomInfo tg_wctennisRomDesc[] = {
	{ "World Court Tennis (USA)(1989)(NEC).pce", 0x040000, 0xa4457df0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_wctennis)
STD_ROM_FN(tg_wctennis)

struct BurnDriver BurnDrvtg_wctennis = {
	"tg_wctennis", NULL, NULL, NULL, "1989",
	"World Court Tennis (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_wctennisRomInfo, tg_wctennisRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// World Sports Competition (USA)

static struct BurnRomInfo tg_wscompRomDesc[] = {
	{ "World Sports Competition (USA)(1992)(TTI).pce", 0x080000, 0x4b93f0ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_wscomp)
STD_ROM_FN(tg_wscomp)

struct BurnDriver BurnDrvtg_wscomp = {
	"tg_wscomp", NULL, NULL, NULL, "1992",
	"World Sports Competition (USA)\0", NULL, "TTI", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_PCENGINE_TG16, GBF_SPORTSMISC, 0,
	TgGetZipName, tg_wscompRomInfo, tg_wscompRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Yo, Bro (USA)

static struct BurnRomInfo tg_yobroRomDesc[] = {
	{ "Yo, Bro (USA)(1991)(NEC).pce", 0x080000, 0x3ca7db48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_yobro)
STD_ROM_FN(tg_yobro)

struct BurnDriver BurnDrvtg_yobro = {
	"tg_yobro", NULL, NULL, NULL, "1991",
	"Yo, Bro (USA)\0", NULL, "NEC", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_TG16, GBF_ACTION, 0,
	TgGetZipName, tg_yobroRomInfo, tg_yobroRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 352, 208, 4, 3
};


// --------------------------
// PC-Engine SuperGrafx Games
// --------------------------


// 1941 - Counter Attack (Japan) (SGX)

static struct BurnRomInfo sgx_1941RomDesc[] = {
	{ "1941 - Counter Attack (Japan)(1991)(Hudson Soft).pce", 0x100000, 0x8c4588e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_1941)
STD_ROM_FN(sgx_1941)

struct BurnDriver BurnDrvsgx_1941 = {
	"sgx_1941", NULL, NULL, NULL, "1991",
	"1941 - Counter Attack (Japan) (SGX)\0", NULL, "Hudson Soft", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_SGX, GBF_VERSHOOT, 0,
	SgxGetZipName, sgx_1941RomInfo, sgx_1941RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 224, 240, 4, 3
};


// Aldynes (Japan) (SGX)

static struct BurnRomInfo sgx_aldynesRomDesc[] = {
	{ "Aldynes (Japan)(1991)(Hudson Soft).pce", 0x100000, 0x4c2126b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_aldynes)
STD_ROM_FN(sgx_aldynes)

struct BurnDriver BurnDrvsgx_aldynes = {
	"sgx_aldynes", NULL, NULL, NULL, "1991",
	"Aldynes (Japan) (SGX)\0", NULL, "Hudson Soft", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_SGX, GBF_HORSHOOT, 0,
	SgxGetZipName, sgx_aldynesRomInfo, sgx_aldynesRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Battle Ace (Japan) (SGX)

static struct BurnRomInfo sgx_battlaceRomDesc[] = {
	{ "Battle Ace (Japan)(1989)(Hudson Soft).pce", 0x080000, 0x3b13af61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_battlace)
STD_ROM_FN(sgx_battlace)

struct BurnDriver BurnDrvsgx_battlace = {
	"sgx_battlace", NULL, NULL, NULL, "1989",
	"Battle Ace (Japan) (SGX)\0", NULL, "Hudson Soft", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_SGX, GBF_SHOOT, 0,
	SgxGetZipName, sgx_battlaceRomInfo, sgx_battlaceRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// Daimakaimura (Japan) (SGX)

static struct BurnRomInfo sgx_daimakaiRomDesc[] = {
	{ "Daimakaimura (Japan)(1990)(NEC Avenue).pce", 0x100000, 0xb486a8ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_daimakai)
STD_ROM_FN(sgx_daimakai)

struct BurnDriver BurnDrvsgx_daimakai = {
	"sgx_daimakai", NULL, NULL, NULL, "1990",
	"Daimakaimura (Japan) (SGX)\0", NULL, "NEC Avenue", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_SGX, GBF_RUNGUN | GBF_PLATFORM, 0,
	SgxGetZipName, sgx_daimakaiRomInfo, sgx_daimakaiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 320, 240, 4, 3
};


// Daimakaimura (Japan, Alt) (SGX)

static struct BurnRomInfo sgx_daimakai1RomDesc[] = {
	{ "Daimakaimura (Japan, Alt)(1990)(NEC Avenue).pce", 0x100200, 0x8e961f63, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_daimakai1)
STD_ROM_FN(sgx_daimakai1)

struct BurnDriver BurnDrvsgx_daimakai1 = {
	"sgx_daimakai1", "sgx_daimakai", NULL, NULL, "1990",
	"Daimakaimura (Japan, Alt) (SGX)\0", NULL, "NEC Avenue", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_PCENGINE_SGX, GBF_RUNGUN | GBF_PLATFORM, 0,
	SgxGetZipName, sgx_daimakai1RomInfo, sgx_daimakai1RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 320, 240, 4, 3
};


// Daimakaimura - Debug Menu (SGX) (Hack)

static struct BurnRomInfo sgx_daimakaidRomDesc[] = {
	{ "Daimakaimura - Debug Menu (2007)(Chris Covell).pce", 0x100000, 0xd6722c04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_daimakaid)
STD_ROM_FN(sgx_daimakaid)

struct BurnDriver BurnDrvsgx_daimakaid = {
	"sgx_daimakaid", "sgx_daimakai", NULL, NULL, "2007",
	"Daimakaimura - Debug Menu (SGX) (Hack)\0", NULL, "Chris Covell", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_SGX, GBF_RUNGUN | GBF_PLATFORM, 0,
	SgxGetZipName, sgx_daimakaidRomInfo, sgx_daimakaidRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 320, 240, 4, 3
};


// Madou Ou Granzort (Japan) (SGX)

static struct BurnRomInfo sgx_granzortRomDesc[] = {
	{ "Madou Ou Granzort (Japan)(1990)(Hudson Soft).pce", 0x080000, 0x1f041166, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_granzort)
STD_ROM_FN(sgx_granzort)

struct BurnDriver BurnDrvsgx_granzort = {
	"sgx_granzort", NULL, NULL, NULL, "1990",
	"Madou Ou Granzort (Japan) (SGX)\0", NULL, "Hudson Soft", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PCENGINE_SGX, GBF_RUNGUN | GBF_PLATFORM, 0,
	SgxGetZipName, sgx_granzortRomInfo, sgx_granzortRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};


// ------------------------------------
// Aftermarket/Imp. Hack/Homebrew Games
// ------------------------------------


// Atlantean (HB)

static struct BurnRomInfo pce_atlanteanRomDesc[] = {
	{ "Atlantean (2014)(Aetherbyte).pce", 0x080200, 0xef596649, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_atlantean)
STD_ROM_FN(pce_atlantean)

struct BurnDriver BurnDrvpce_atlantean = {
	"pce_atlantean", NULL, NULL, NULL, "2014",
	"Atlantean (HB)\0", NULL, "Aetherbyte", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_atlanteanRomInfo, pce_atlanteanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Barbarian (HB)

static struct BurnRomInfo pce_barbarianRomDesc[] = {
	{ "Barbarian (2017)(F.L).pce", 0x0b8200, 0x42d3c3f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_barbarian)
STD_ROM_FN(pce_barbarian)

struct BurnDriver BurnDrvpce_barbarian = {
	"pce_barbarian", NULL, NULL, NULL, "2017",
	"Barbarian (HB)\0", NULL, "F.L", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_barbarianRomInfo, pce_barbarianRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Bug Hunt (HB)

static struct BurnRomInfo pce_bughuntRomDesc[] = {
	{ "Bug Hunt (2013)(Cabbage).pce", 0x030000, 0xc15a2b45, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_bughunt)
STD_ROM_FN(pce_bughunt)

struct BurnDriver BurnDrvpce_bughunt = {
	"pce_bughunt", NULL, NULL, NULL, "2013",
	"Bug Hunt (HB)\0", NULL, "cabbage", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_ACTION, 0,
	PceGetZipName, pce_bughuntRomInfo, pce_bughuntRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Daimakaimura - Improvement Hack (Hack, SGX)
// https://romhackplaza.org/romhacks/daimakaimura-improvement-hack-pc-engine-supergrafx/
static struct BurnRomInfo sgx_daimakaiiRomDesc[] = {
	{ "Daimakaimura Improvement Hack (2024)(Upsilandre).pce", 0x100000, 0x460d8892, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_daimakaii)
STD_ROM_FN(sgx_daimakaii)

struct BurnDriver BurnDrvsgx_daimakaii = {
	"sgx_daimakaii", "sgx_daimakai", NULL, NULL, "2024",
	"Daimakaimura - Improvement Hack (Hack, SGX)\0", NULL, "Upsilandre", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_SGX, GBF_RUNGUN | GBF_PLATFORM, 0,
	SgxGetZipName, sgx_daimakaiiRomInfo, sgx_daimakaiiRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 352, 240, 4, 3
};

// Dinoforce (Japan)

static struct BurnRomInfo pce_dinoforceRomDesc[] = {
	{ "Dinoforce (Japan)(2022)(Tokuhisa Tajima).pce", 0x080000, 0x334300b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_dinoforce)
STD_ROM_FN(pce_dinoforce)

struct BurnDriver BurnDrvpce_dinoforce = {
	"pce_dinoforce", NULL, NULL, NULL, "2022",
	"Dinoforce (Japan)\0", NULL, "Tokuhisa Tajima", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_dinoforceRomInfo, pce_dinoforceRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Final Match Tennis - Ladies

static struct BurnRomInfo pce_finalmtladiesRomDesc[] = {
	{ "Final Match Tennis - Ladies (2002-2016)(Human-MooZ).pce", 0x040000, 0xe92d1290, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_finalmtladies)
STD_ROM_FN(pce_finalmtladies)

struct BurnDriver BurnDrvpce_finalmtladies = {
	"pce_finalmtladies", NULL, NULL, NULL, "2002-2016",
	"Final Match Tennis - Ladies (Super CD-Rom Hack)\0", NULL, "Human - Mooz", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HACK, 4, HARDWARE_PCENGINE_PCENGINE, GBF_SPORTSMISC, 0,
	PceGetZipName, pce_finalmtladiesRomInfo, pce_finalmtladiesRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// GunHed FX1 (Hack)

static struct BurnRomInfo pce_gunhedfxRomDesc[] = {
	{ "Gunhed FX1 (Hack, 2016)(Phase).pce", 0x060000, 0x9710c85c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_gunhedfx)
STD_ROM_FN(pce_gunhedfx)

struct BurnDriver BurnDrvpce_gunhedfx = {
	"pce_gunhedfx", "pce_gunhed", NULL, NULL, "2016",
	"GunHed FX1 (Hack)\0", NULL, "Phase", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_VERSHOOT, 0,
	PceGetZipName, pce_gunhedfxRomInfo, pce_gunhedfxRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// HuZERO - Caravan Edition (HB)

static struct BurnRomInfo pce_huzeroRomDesc[] = {
	{ "HuZERO - Caravan Edition (2014)(Chris Covell).pce", 0x040000, 0x2871c4c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_huzero)
STD_ROM_FN(pce_huzero)

struct BurnDriver BurnDrvpce_huzero = {
	"pce_huzero", NULL, NULL, NULL, "2014",
	"HuZERO - Caravan Edition (HB)\0", NULL, "Chris Covell", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_RACING, 0,
	PceGetZipName, pce_huzeroRomInfo, pce_huzeroRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Mai Nurse (HB)

static struct BurnRomInfo pce_mainurseRomDesc[] = {
	{ "Mai Nurse (2024)(lunoka).pce", 262144, 0xc94e2bc6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_mainurse)
STD_ROM_FN(pce_mainurse)

struct BurnDriver BurnDrvpce_mainurse = {
	"pce_mainurse", NULL, NULL, NULL, "2024",
	"Mai Nurse (HB)\0", NULL, "lunoka", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_mainurseRomInfo, pce_mainurseRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 224, 4, 3
};

// Nantettatte Engine (HB)

static struct BurnRomInfo pce_nantetRomDesc[] = {
	{ "Nantettatte Engine (2017)(Atherbyte).pce", 0x040000, 0x3974fb41, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_nantet)
STD_ROM_FN(pce_nantet)

struct BurnDriver BurnDrvpce_nantet = {
	"pce_nantet", NULL, NULL, NULL, "2017",
	"Nantettatte Engine (HB)\0", NULL, "Atherbyte", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SHOOT, 0,
	PceGetZipName, pce_nantetRomInfo, pce_nantetRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// PC-Engine CPU Timing Test by Chris Covell
// http://www.chrismcovell.com/CPUTest/
static struct BurnRomInfo pce_cputestRomDesc[] = {
	{ "CPU Timing Test (2019)(Chris Covell).pce", 0x8000, 0x46D79BBE, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_cputest)
STD_ROM_FN(pce_cputest)

struct BurnDriverD BurnDrvpce_cputest = {
	"pce_cputest", NULL, NULL, NULL, "2019",
	"PC-Engine CPU Timing Test (HB)\0", NULL, "Chris Covell", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_MISC, 0,
	PceGetZipName, pce_cputestRomInfo, pce_cputestRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Reflectron (HB)

static struct BurnRomInfo pce_reflectronRomDesc[] = {
	{ "Reflectron (2013)(Atherbyte).pce", 0x040000, 0x6a3727e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_reflectron)
STD_ROM_FN(pce_reflectron)

struct BurnDriver BurnDrvpce_reflectron = {
	"pce_reflectron", NULL, NULL, NULL, "2013",
	"Reflectron (HB)\0", NULL, "Aetherbyte", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_reflectronRomInfo, pce_reflectronRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// RTA2 - Homebrew conversion of RTA2, bonus game present on Revivel Chase CD (HB, SGX)
// https://www.gamopat-forum.com/t86439-rt2a-sur-supergrafx
static struct BurnRomInfo sgx_rta2RomDesc[] = {
	{ "RTA2 (2016)(Revival Games).sgx", 0x028200, 0x4a4db3da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_rta2)
STD_ROM_FN(sgx_rta2)

struct BurnDriver BurnDrvsgx_rta2 = {
	"sgx_rta2", NULL, NULL, NULL, "2016",
	"RTA2 (HB, SGX)\0", "Homebrew conversion of RTA2", "Revival Games", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_SGX, GBF_HORSHOOT, 0,
	SgxGetZipName, sgx_rta2RomInfo, sgx_rta2RomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 336, 240, 4, 3
};

// R-Type (SGX) (Hack, Unfinished)
// https://www.chrismcovell.com/creations.html#courrtype
static struct BurnRomInfo sgx_rtypeuhRomDesc[] = {
	{ "R-Type SGX (Unfinished Hack)(2019)(Chris Covell).sgx", 0x080000, 0xcdc9bb74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sgx_rtypeuh)
STD_ROM_FN(sgx_rtypeuh)

struct BurnDriver BurnDrvsgx_rtypeuh = {
	"sgx_rtypeuh", NULL, NULL, NULL, "2019",
	"R-Type (SGX) (Hack, Unfinished)\0", NULL, "Chris Covell", "SuperGrafx",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HACK, 1, HARDWARE_PCENGINE_SGX, GBF_HORSHOOT, 0,
	SgxGetZipName, sgx_rtypeuhRomInfo, sgx_rtypeuhRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	SGXInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 352, 240, 4, 3
};

// R-Type Plus (Hack)
// https://www.romhacking.net/hacks/8349/
static struct BurnRomInfo tg_rtypeplusRomDesc[] = {
	{ "R-Type Plus (2024)(Justin Gibbins).pce", 524288, 0xdc118e24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_rtypeplus)
STD_ROM_FN(tg_rtypeplus)

struct BurnDriver BurnDrvtg_rtypeplus = {
	"tg_rtypeplus", "tg_rtype", NULL, NULL, "2024",
	"R-Type Plus (Hack)\0", NULL, "Justin Gibbins", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_TG16, GBF_HORSHOOT, 0,
	TgGetZipName, tg_rtypeplusRomInfo, tg_rtypeplusRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Santatlantean (HB)

static struct BurnRomInfo pce_santatlanteanRomDesc[] = {
	{ "Santatlantean (2014)(Atherbyte).pce", 0x080200, 0xe6b38af1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_santatlantean)
STD_ROM_FN(pce_santatlantean)

struct BurnDriver BurnDrvpce_santatlantean = {
	"pce_santatlantean", NULL, NULL, NULL, "2014",
	"Santatlantean (HB)\0", NULL, "Aetherbyte", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_HORSHOOT, 0,
	PceGetZipName, pce_santatlanteanRomInfo, pce_santatlanteanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Splatterhouse Chrome (Hack)
// https://www.romhacking.net/hacks/520/
static struct BurnRomInfo pce_splatthcRomDesc[] = {
	{ "Splatterhouse Chrome (Hack, 2009)(guemmai).pce", 0x080200, 0xf6ab818b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_splatthc)
STD_ROM_FN(pce_splatthc)

struct BurnDriver BurnDrvpce_splatthc = {
	"pce_splatthc", "pce_splatth", NULL, NULL, "2009",
	"Splatterhouse Chrome (Hack)\0", NULL, "guemmai", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_PCENGINE, GBF_SCRFIGHT, 0,
	PceGetZipName, pce_splatthcRomInfo, pce_splatthcRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// SplitRes (HB)

static struct BurnRomInfo pce_splitresRomDesc[] = {
	{ "Split Res (2008)(Chris Covell).pce", 0x020000, 0x3758E3B3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_splitres)
STD_ROM_FN(pce_splitres)

struct BurnDriver BurnDrvpce_splitres = {
	"pce_splitres", NULL, NULL, NULL, "2008",
	"Split Res (HB)\0", NULL, "Chris Covell", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_splitresRomInfo, pce_splitresRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Taito Chase H.Q. - Noise Fix (Hack)
// https://www.romhacking.net/hacks/7242/
static struct BurnRomInfo tg_chasehqnfRomDesc[] = {
	{ "Taito Chase H.Q. - Noise Fix (Hack)(2022)(Justin Gibbins).pce", 0x060000, 0x947740bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(tg_chasehqnf)
STD_ROM_FN(tg_chasehqnf)

struct BurnDriver BurnDrvtg_chasehqnf = {
	"tg_chasehqnf", "tg_chasehq", NULL, NULL, "2022",
	"Taito Chase H.Q. - Noise Fix (Hack)\0", NULL, "Justin Gibbins", "TurboGrafx 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_PCENGINE_TG16, GBF_RACING, 0,
	TgGetZipName, tg_chasehqnfRomInfo, tg_chasehqnfRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	TG16Init, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Tongueman's Logic (HB)

static struct BurnRomInfo pce_tonguemanRomDesc[] = {
	{ "Tongueman's Logic (2007)(Chris Covell).pce", 0x080000, 0xfe451c22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_tongueman)
STD_ROM_FN(pce_tongueman)

struct BurnDriver BurnDrvpce_tongueman = {
	"pce_tongueman", NULL, NULL, NULL, "2007",
	"Tongueman's Logic (HB)\0", NULL, "Chris Covell", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PUZZLE, 0,
	PceGetZipName, pce_tonguemanRomInfo, pce_tonguemanRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};

// Uwol Quest for Money (HB, v1.1)
// https://www.portabledev.com/jeux/pc-engine/uwol-quest-for-money/
static struct BurnRomInfo pce_uwolRomDesc[] = {
	{ "Uwol Quest for Money v1.1 (2018)(Alekmaul).pce", 0x040000, 0x59421adc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(pce_uwol)
STD_ROM_FN(pce_uwol)

struct BurnDriver BurnDrvpce_uwol = {
	"pce_uwol", NULL, NULL, NULL, "2018",
	"Uwol Quest for Money (HB, v1.1)\0", NULL, "Alekmaul", "PC Engine",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_PCENGINE_PCENGINE, GBF_PLATFORM, 0,
	PceGetZipName, pce_uwolRomInfo, pce_uwolRomName, NULL, NULL, NULL, NULL, pceInputInfo, pceDIPInfo,
	PCEInit, PCEExit, PCEFrame, PCEDraw, PCEScan,
	&PCEPaletteRecalc, 0x400, 1024, 240, 4, 3
};
