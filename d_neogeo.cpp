// Super Dodge Ball / Kunio no Nekketsu Toukyuu Densetsu (Secret Character Hack)
/* MVS ONLY RELEASE */

static struct BurnRomInfo sdodgebhRomDesc[] = {
	{ "208-p1.p1",    0x200000, 0xE6E58566, 1 | BRF_ESS | BRF_PRG }, //  0 68K code			/ TC5316200

	{ "208-s1.s1",    0x020000, 0x64abd6b3, 2 | BRF_GRA },           //  1 Text layer tiles / TC531000

	{ "208-c1.c1",    0x400000, 0x93d8619b, 3 | BRF_GRA },           //  2 Sprite data		/ TC5332205
	{ "208-c2.c2",    0x400000, 0x1c737bb6, 3 | BRF_GRA },           //  3 					/ TC5332205
	{ "208-c3.c3",    0x200000, 0x14cb1703, 3 | BRF_GRA },           //  4 					/ TC5332205
	{ "208-c4.c4",    0x200000, 0xc7165f19, 3 | BRF_GRA },           //  5 					/ TC5332205

	{ "208-m1.m1",    0x020000, 0x0a5f3325, 4 | BRF_ESS | BRF_PRG }, //  6 Z80 code			/ TC531001

	{ "208-v1.v1",    0x400000, 0xe7899a24, 5 | BRF_SND },           //  7 Sound data		/ TC5332204
};

STDROMPICKEXT(sdodgebh, sdodgebh, neogeo)
STD_ROM_FN(sdodgebh)

struct BurnDriver BurnDrvsdodgebh = {
	"sdodgebh", NULL, "neogeo", NULL, "1996",
	"Super Dodge Ball / Kunio no Nekketsu Toukyuu Densetsu (Secret Character Hack)\0", NULL, "Technos", "Neo Geo MVS",
	L"Super Dodge Ball\0\u304F\u306B\u304A\u306E\u71B1\u8840\u95D8\u7403\u4F1D\u8AAC\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO | HARDWARE_SNK_SWAPP, GBF_SPORTSMISC, 0,
	NULL, sdodgebhRomInfo, sdodgebhRomName, NULL, NULL, NULL, NULL, neogeoInputInfo, neogeoDIPInfo,
	NeoInit, NeoExit, NeoFrame, NeoRender, NeoScan, &NeoRecalcPalette,
	0x1000,	304, 224, 4, 3
};

// Magical Drop III (Secret Character Hack)

static struct BurnRomInfo magdrop3bhRomDesc[] = {
	{ "233_boss-p1.p1",    0x100000, 0x80BFE2A9, 1 | BRF_ESS | BRF_PRG }, //  0 68K code

	{ "233-s1.s1",    0x020000, 0x7399e68a, 2 | BRF_GRA },           //  1 Text layer tiles

	{ "233-c1.c1",    0x400000, 0x65e3f4c4, 3 | BRF_GRA },           //  2 Sprite data
	{ "233-c2.c2",    0x400000, 0x35dea6c9, 3 | BRF_GRA },           //  3 
	{ "233-c3.c3",    0x400000, 0x0ba2c502, 3 | BRF_GRA },           //  4 
	{ "233-c4.c4",    0x400000, 0x70dbbd6d, 3 | BRF_GRA },           //  5 

	{ "233-m1.m1",    0x020000, 0x5beaf34e, 4 | BRF_ESS | BRF_PRG }, //  6 Z80 code

	{ "233-v1.v1",    0x400000, 0x58839298, 5 | BRF_SND },           //  7 Sound data
	{ "233-v2.v2",    0x080000, 0xd5e30df4, 5 | BRF_SND },           //  8 
};

STDROMPICKEXT(magdrop3bh, magdrop3bh, neogeo)
STD_ROM_FN(magdrop3bh)

struct BurnDriver BurnDrvmagdrop3bh = {
	"magdrop3bh", NULL, "neogeo", NULL, "1997",
	"Magical Drop III (Secret Character Hack)\0", NULL, "Data East Corporation", "Neo Geo MVS",
	L"Magical Drop III\0\u30DE\u30B8\u30AB\u30EB\u30C9\u30ED\u30C3\u30D7III\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO, GBF_PUZZLE, 0,
	NULL, magdrop3bhRomInfo, magdrop3bhRomName, NULL, NULL, NULL, NULL, neogeoInputInfo, neogeoDIPInfo,
	NeoInit, NeoExit, NeoFrame, NeoRender, NeoScan, &NeoRecalcPalette,
	0x1000,	304, 224, 4, 3
};

// Waku Waku 7 (Boss Hack)

static struct BurnRomInfo wakuwak7bhRomDesc[] = {
	{ "225-p1bh.p1",    0x100000, 0x0B7A3776, 1 | BRF_ESS | BRF_PRG }, //  0 68K code
	{ "225-p2.sp2",   0x200000, 0xfe190665, 1 | BRF_ESS | BRF_PRG }, //  1 

	{ "225-s1.s1",    0x020000, 0x71c4b4b5, 2 | BRF_GRA },           //  2 Text layer tiles

	{ "225-c1.c1",    0x400000, 0xee4fea54, 3 | BRF_GRA },           //  3 Sprite data
	{ "225-c2.c2",    0x400000, 0x0c549e2d, 3 | BRF_GRA },           //  4 
	{ "225-c3.c3",    0x400000, 0xaf0897c0, 3 | BRF_GRA },           //  5 
	{ "225-c4.c4",    0x400000, 0x4c66527a, 3 | BRF_GRA },           //  6 
	{ "225-c5.c5",    0x400000, 0x8ecea2b5, 3 | BRF_GRA },           //  7 
	{ "225-c6.c6",    0x400000, 0x0eb11a6d, 3 | BRF_GRA },           //  8 

	{ "225-m1.m1",    0x020000, 0x0634bba6, 4 | BRF_ESS | BRF_PRG }, //  9 Z80 code

	{ "225-v1.v1",    0x400000, 0x6195c6b4, 5 | BRF_SND },           // 10 Sound data
	{ "225-v2.v2",    0x400000, 0x6159c5fe, 5 | BRF_SND },           // 11 
};

STDROMPICKEXT(wakuwak7bh, wakuwak7bh, neogeo)
STD_ROM_FN(wakuwak7bh)

new: struct BurnDriver BurnDrvwakuwak7bh = {
	"wakuwak7bh", NULL, "neogeo", NULL, "1996",
	"Waku Waku 7 (Boss Hack)\0", NULL, "Sunsoft", "Neo Geo MVS",
	L"Waku Waku 7\0\u308F\u304F\u308F\u304F\uFF17\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO, GBF_VSFIGHT, 0,
	NULL, wakuwak7bhRomInfo, wakuwak7bhRomName, NULL, NULL, NULL, NULL, neogeoInputInfo, neogeoDIPInfo,
	NeoInit, NeoExit, NeoFrame, NeoRender, NeoScan, &NeoRecalcPalette,
	0x1000,	304, 224, 4, 3
};

// Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - the newcomers (Secret Character Hack) (NGM-2400)

static struct BurnRomInfo rbff2bhRomDesc[] = {
	{ "240-p1fc.p1",    0x100000, 0xD01854FA, 1 | BRF_ESS | BRF_PRG }, //  0 68K code
	{ "240-p2fc.sp2",   0x400000, 0xC063193D, 1 | BRF_ESS | BRF_PRG }, //  1 

	{ "240-s1bh.s1",    0x020000, 0x141A8492, 2 | BRF_GRA },           //  2 Text layer tiles

	{ "240-c1.c1",    0x800000, 0xeffac504, 3 | BRF_GRA },           //  3 Sprite data
	{ "240-c2.c2",    0x800000, 0xed182d44, 3 | BRF_GRA },           //  4 
	{ "240-c3.c3",    0x800000, 0x22e0330a, 3 | BRF_GRA },           //  5 
	{ "240-c4.c4",    0x800000, 0xc19a07eb, 3 | BRF_GRA },           //  6 
	{ "240-c5.c5",    0x800000, 0x244dff5a, 3 | BRF_GRA },           //  7 
	{ "240-c6.c6",    0x800000, 0x4609e507, 3 | BRF_GRA },           //  8 

	{ "240-m1.m1",    0x040000, 0xed482791, 4 | BRF_ESS | BRF_PRG }, //  9 Z80 code

	{ "240-v1.v1",    0x400000, 0xf796265a, 5 | BRF_SND },           // 10 Sound data
	{ "240-v2.v2",    0x400000, 0x2cb3f3bb, 5 | BRF_SND },           // 11 
	{ "240-v3.v3",    0x400000, 0x8fe1367a, 5 | BRF_SND },           // 12 
	{ "240-v4.v4",    0x200000, 0x996704d8, 5 | BRF_SND },           // 13 
};

STDROMPICKEXT(rbff2bh, rbff2bh, neogeo)
STD_ROM_FN(rbff2bh)

struct BurnDriver BurnDrvrbff2bh = {
	"rbff2bh", NULL, "neogeo", NULL, "1998",
	"Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - the newcomers (Secret Character Hack) (NGM-2400)\0", NULL, "SNK", "Neo Geo MVS",
	L"Real Bout Fatal Fury 2 - The Newcomers\0Real Bout \u9913\u72FC\u4F1D\u8AAC\uFF12 (NGM-2400)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO, GBF_VSFIGHT, FBF_FATFURY,
	NULL, rbff2bhRomInfo, rbff2bhRomName, NULL, NULL, NULL, NULL, neogeoInputInfo, neogeoDIPInfo,
	NeoInit, NeoExit, NeoFrame, NeoRender, NeoScan, &NeoRecalcPalette,
	0x1000, 320, 224, 4, 3
};

// Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special (Boss Hack)

static struct BurnRomInfo rbffspbhRomDesc[] = {
	{ "223-p1bs.p1",    0x100000, 0xABF2A6E7, 1 | BRF_ESS | BRF_PRG }, //  0 68K code
	{ "223-p2.sp2",   0x400000, 0xaddd8f08, 1 | BRF_ESS | BRF_PRG }, //  1 

	{ "223-s1.s1",    0x020000, 0x7ecd6e8c, 2 | BRF_GRA },           //  2 Text layer tiles

	{ "223-c1.c1",    0x400000, 0xebab05e2, 3 | BRF_GRA },           //  3 Sprite data
	{ "223-c2.c2",    0x400000, 0x641868c3, 3 | BRF_GRA },           //  4 
	{ "223-c3.c3",    0x400000, 0xca00191f, 3 | BRF_GRA },           //  5 
	{ "223-c4.c4",    0x400000, 0x1f23d860, 3 | BRF_GRA },           //  6 
	{ "223-c5.c5",    0x400000, 0x321e362c, 3 | BRF_GRA },           //  7 
	{ "223-c6.c6",    0x400000, 0xd8fcef90, 3 | BRF_GRA },           //  8 
	{ "223-c7.c7",    0x400000, 0xbc80dd2d, 3 | BRF_GRA },           //  9 
	{ "223-c8.c8",    0x400000, 0x5ad62102, 3 | BRF_GRA },           // 10 

	{ "223-m1.m1",    0x020000, 0x3fee46bf, 4 | BRF_ESS | BRF_PRG }, // 11 Z80 code

	{ "223-v1.v1",    0x400000, 0x76673869, 5 | BRF_SND },           // 12 Sound data
	{ "223-v2.v2",    0x400000, 0x7a275acd, 5 | BRF_SND },           // 13 
	{ "223-v3.v3",    0x400000, 0x5a797fd2, 5 | BRF_SND },           // 14 
};

STDROMPICKEXT(rbffspbh, rbffspbh, neogeo)
STD_ROM_FN(rbffspbh)

struct BurnDriver BurnDrvrbffspbh = {
	"rbffspec", NULL, "neogeo", NULL, "1996",
	"Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special (Boss Hack)\0", NULL, "SNK", "Neo Geo MVS",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO, GBF_VSFIGHT, FBF_FATFURY,
	NULL, rbffspbhRomInfo, rbffspbhRomName, NULL, NULL, NULL, NULL, neogeoInputInfo, neogeoDIPInfo,
	NeoInit, NeoExit, NeoFrame, NeoRender, NeoScan, &NeoRecalcPalette,
	0x1000, 320, 224, 4, 3
};
