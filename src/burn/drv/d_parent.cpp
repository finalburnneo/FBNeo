// A module for keeping parents who have no driver in FBA

#include "burnint.h"

static UINT8 ParentReset         = 0;

static struct BurnInputInfo ParentInputList[] = {
	{"Reset"             , BIT_DIGITAL  , &ParentReset        , "reset"     },
};

STDINPUTINFO(Parent)

static INT32 ParentInit()
{
	return 1;
}

static INT32 ParentExit()
{
	return 0;
}

static struct BurnRomInfo EightballactRomDesc[] = {
	{ "8b-dk.5e",           0x01000, 0x166c1c9b, BRF_ESS | BRF_PRG },
	{ "8b-dk.5c",           0x01000, 0x9ec87baa, BRF_ESS | BRF_PRG },
	{ "8b-dk.5b",           0x01000, 0xf836a962, BRF_ESS | BRF_PRG },
	{ "8b-dk.5a",           0x01000, 0xd45866d4, BRF_ESS | BRF_PRG },
	
	{ "8b-dk.3h",           0x00800, 0xa8752c60, BRF_ESS | BRF_PRG },

	{ "8b-dk.3n",           0x00800, 0x44830867, BRF_GRA },
	{ "8b-dk.3p",           0x00800, 0x6148c6f2, BRF_GRA },
	
	{ "8b-dk.7c",           0x00800, 0xe34409f5, BRF_GRA },
	{ "8b-dk.7d",           0x00800, 0xb4dc37ca, BRF_GRA },
	{ "8b-dk.7e",           0x00800, 0x655af8a8, BRF_GRA },
	{ "8b-dk.7f",           0x00800, 0xa29b2763, BRF_GRA },
	
	{ "8b.2e",              0x00100, 0xc7379a12, BRF_GRA },
	{ "8b.2f",              0x00100, 0x116612b4, BRF_GRA },
	{ "8b.2n",              0x00100, 0x30586988, BRF_GRA },
	
	{ "82s147.prm",         0x00200, 0x46e5bc92, BRF_GRA },
	
	{ "pls153h.bin",        0x000eb, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(Eightballact)
STD_ROM_FN(Eightballact)

struct BurnDriver BurnDrvEightballact = {
	"8ballact", NULL, NULL, NULL, "1984",
	"Eight Ball Action (DK conversion)\0", "Parent set for working drivers", "Seatongrove Ltd (Magic Eletronics USA licence)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, EightballactRomInfo, EightballactRomName, NULL, NULL, NULL, NULL, ParentInputInfo, NULL,
	ParentInit, ParentExit, NULL, NULL, NULL,
	NULL, 0, 256, 224, 4, 3
};

static struct BurnRomInfo HeroRomDesc[] = {
	{ "hr-gp1.bin",         0x01000, 0x82f39788, BRF_ESS | BRF_PRG },
	{ "hr-gp2.bin",         0x01000, 0x79607812, BRF_ESS | BRF_PRG },
	{ "hr-gp3.bin",         0x01000, 0x2902715c, BRF_ESS | BRF_PRG },
	{ "hr-gp4.bin",         0x01000, 0x696d2f8e, BRF_ESS | BRF_PRG },
	{ "hr-gp5.bin",         0x01000, 0x936a4ba6, BRF_ESS | BRF_PRG },
	
	{ "hr-sdp1.bin",        0x00800, 0xc34ecf79, BRF_ESS | BRF_PRG },
	
	{ "hr-sp1.bin",         0x00800, 0xa5c33cb1, BRF_SND },

	{ "hr-cp1.bin",         0x00800, 0x2d201496, BRF_GRA },
	{ "hr-cp2.bin",         0x00800, 0x21b61fe3, BRF_GRA },
	{ "hr-cp3.bin",         0x00800, 0x9c8e3f9e, BRF_GRA },
	
	{ "5b.bin",             0x00800, 0xf055a624, BRF_SND },
	
	{ "82s185.10h",         0x00800, 0xc205bca6, BRF_GRA },
	{ "82s123.10k",         0x00020, 0xb5221cec, BRF_GRA },
};

STD_ROM_PICK(Hero)
STD_ROM_FN(Hero)

struct BurnDriver BurnDrvHero = {
	"hero", NULL, NULL, NULL, "1984",
	"Hero\0", "Parent set for working drivers", "Century Electronics / Seatongrove Ltd", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, HeroRomInfo, HeroRomName, NULL, NULL, NULL, NULL, ParentInputInfo, NULL,
	ParentInit, ParentExit, NULL, NULL, NULL,
	NULL, 0, 224, 256, 3, 4
};

static struct BurnRomInfo HunchbakRomDesc[] = {
	{ "hb-gp1.bin",         0x01000, 0xaf801d54, BRF_ESS | BRF_PRG },
	{ "hb-gp2.bin",         0x01000, 0xb448cc8e, BRF_ESS | BRF_PRG },
	{ "hb-gp3.bin",         0x01000, 0x57c6ea7b, BRF_ESS | BRF_PRG },
	{ "hb-gp4.bin",         0x01000, 0x7f91287b, BRF_ESS | BRF_PRG },
	{ "hb-gp5.bin",         0x01000, 0x1dd5755c, BRF_ESS | BRF_PRG },
	
	{ "6c.sdp1",            0x01000, 0xf9ba2854, BRF_ESS | BRF_PRG },
	
	{ "8a.sp1",             0x00800, 0xed1cd201, BRF_SND },

	{ "11a.cp1",            0x00800, 0xf256b047, BRF_GRA },
	{ "10a.cp2",            0x00800, 0xb870c64f, BRF_GRA },
	{ "9a.cp3",             0x00800, 0x9a7dab88, BRF_GRA },
	
	{ "5b.bin",             0x00800, 0xf055a624, BRF_SND },
	
	{ "82s185.10h",         0x00800, 0xc205bca6, BRF_GRA },
	{ "82s123.10k",         0x00020, 0xb5221cec, BRF_GRA },
};

STD_ROM_PICK(Hunchbak)
STD_ROM_FN(Hunchbak)

struct BurnDriver BurnDrvHunchbak = {
	"hunchbak", NULL, NULL, NULL, "1983",
	"Hunchback (set 1)\0", "Parent set for working drivers", "Century Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, HunchbakRomInfo, HunchbakRomName, NULL, NULL, NULL, NULL, ParentInputInfo, NULL,
	ParentInit, ParentExit, NULL, NULL, NULL,
	NULL, 0, 224, 256, 3, 4
};

static struct BurnRomInfo HuncholyRomDesc[] = {
	{ "ho-gp1.bin",         0x01000, 0x4f17cda7, BRF_ESS | BRF_PRG },
	{ "ho-gp2.bin",         0x01000, 0x70fa52c7, BRF_ESS | BRF_PRG },
	{ "ho-gp3.bin",         0x01000, 0x931934b1, BRF_ESS | BRF_PRG },
	{ "ho-gp4.bin",         0x01000, 0xaf5cd501, BRF_ESS | BRF_PRG },
	{ "ho-gp5.bin",         0x01000, 0x658e8974, BRF_ESS | BRF_PRG },
	
	{ "ho-sdp1.bin",        0x01000, 0x3efb3ffd, BRF_ESS | BRF_PRG },
	
	{ "ho-sp1.bin",         0x01000, 0x3fd39b1e, BRF_SND },

	{ "ho-cp1.bin",         0x00800, 0xc6c73d46, BRF_GRA },
	{ "ho-cp2.bin",         0x00800, 0xe596371c, BRF_GRA },
	{ "ho-cp3.bin",         0x00800, 0x11fae1cf, BRF_GRA },
	
	{ "5b.bin",             0x00800, 0xf055a624, BRF_SND },
	
	{ "82s185.10h",         0x00800, 0xc205bca6, BRF_GRA },
	{ "82s123.10k",         0x00020, 0xb5221cec, BRF_GRA },
};

STD_ROM_PICK(Huncholy)
STD_ROM_FN(Huncholy)

struct BurnDriver BurnDrvHuncholy = {
	"huncholy", NULL, NULL, NULL, "1984",
	"Hunchback Olympic\0", "Parent set for working drivers", "Seatongrove Ltd", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, HuncholyRomInfo, HuncholyRomName, NULL, NULL, NULL, NULL, ParentInputInfo, NULL,
	ParentInit, ParentExit, NULL, NULL, NULL,
	NULL, 0, 224, 256, 3, 4
};
