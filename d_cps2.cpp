STD_ROM_PICK(Spf2tcbh)
STD_ROM_FN(Spf2tcbh)

static struct BurnRomInfo Spf2tcbhRomDesc[] = {
	{ "pzfu_boss.03a", 0x080000, 0xca1f3e42, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "pzf.04",        0x080000, 0x01bf5ff5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "pzf.14m",       0x100000, 0x2d4881cb, CPS2_GFX | BRF_GRA },
	{ "pzf.16m",       0x100000, 0x4b0fd1be, CPS2_GFX | BRF_GRA },
	{ "pzf.18m",       0x100000, 0xe43aac33, CPS2_GFX | BRF_GRA },
	{ "pzf.20m",       0x100000, 0x7f536ff1, CPS2_GFX | BRF_GRA },

	{ "pzf.01",        0x020000, 0x600fb2a3, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "pzf.02",        0x020000, 0x496076e0, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "pzf.11m",       0x200000, 0x78442743, CPS2_QSND | BRF_SND },
	{ "pzf.12m",       0x200000, 0x399d2c7b, CPS2_QSND | BRF_SND },

	{ "spf2tcbh.key",     0x000014, 0x4c4dc7e3, CPS2_ENCRYPTION_KEY },
};

struct BurnDriver BurnDrvCpsSpf2tcbh = {
	"spf2tcbh", "spf2t", NULL, NULL, "1996",
	"Super Puzzle Fighter II Turbo (Boss\Color Blind Hack)\0", NULL, "Capcom", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAPCOM_CPS2, GBF_PUZZLE, FBF_SF,
	NULL, Spf2tcbhRomInfo, Spf2tcbhRomName, NULL, NULL, NULL, NULL, Spf2tInputInfo, NULL,
	Cps2Init, DrvExit, Cps2Frame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};