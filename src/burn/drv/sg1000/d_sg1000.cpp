// FB Neo Sega SG-1000-based arcade driver module
// Based on MAME driver by Tomasz Slanina
// Code by iq_132, fixups & bring up-to-date by dink Aug 18, 2014

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "tms9928a.h"
#include "8255ppi.h"
#include "sn76496.h"

static UINT8 *AllMem	= NULL;
static UINT8 *MemEnd	= NULL;
static UINT8 *AllRam	= NULL;
static UINT8 *RamEnd	= NULL;
static UINT8 *DrvZ80ROM	= NULL;
static UINT8 *DrvZ80Dec = NULL;
static UINT8 *DrvZ80RAM	= NULL;

static UINT8 DrvInputs[2];
static UINT8 DrvDips[1];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvReset;
static UINT8 DrvNMI;
static UINT8 ramexp = 0; // ram expansion mode

static UINT8 dip_changed;

static struct BurnInputInfo Sg1000InputList[] = {
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Console Pause",	BIT_DIGITAL,	&DrvNMI,		"consolepause"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"DIPS",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sg1000)

static struct BurnDIPInfo Sg1000DIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0,       2, "Sprite Limit"			},
	{0x0e, 0x01, 0x20, 0x00, "Enabled"				},
	{0x0e, 0x01, 0x20, 0x20, "Disabled (hack)"		},
};

STDDIPINFO(Sg1000)

static void __fastcall sg1000_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;
	switch (port & ~0x3f)
	{
		case 0x40:
			SN76496Write(0, data);
		return;
	}

	switch (port & ~0x3e)
	{
		case 0x80:
			TMS9928AWriteVRAM(data);
		return;

		case 0x81:
			TMS9928AWriteRegs(data);
		return;
	}
}

static UINT8 __fastcall sg1000_read_port(UINT16 port)
{
	port &= 0xff;

	switch (port & ~0x3e)
	{
		case 0x80:
			return TMS9928AReadVRAM();

		case 0x81:
			return TMS9928AReadRegs();
	}

	switch (port)
	{
		case 0xdc:
			return DrvInputs[0];

		case 0xdd:
			return DrvInputs[1];

		case 0xde:
			return 0x80;

	}

	return 0;
}

static void vdp_interrupt(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	TMS9928AReset();
	ZetClose();

	SN76496Reset();

	dip_changed = DrvDips[0];

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvZ80Dec		= Next; Next += 0x010000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x010400;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void __fastcall sg1000_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc000 && address <= 0xffff) {
		DrvZ80RAM[address & 0x3ff] = data;
		return;
	}
	//bprintf(0, _T("a[%X] d[%X],"), address, data);
}

static UINT8 __fastcall sg1000_read(UINT16 address)
{
	if (address >= 0xc000 && address <= 0xffff) {
		return DrvZ80RAM[address & 0x3ff];
	}
	//bprintf(0, _T("a[%X],"), address);
	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 len0 = 0;
	UINT8 *Load0 = DrvZ80ROM;

	memset(DrvZ80ROM, 0xff, 0x10000); // unmapped romspace = 0xff, fixes safari hunting - thanks Eke! :)

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) == BRF_PRG) {
			if (BurnLoadRom(Load0, i, 1)) return 1;
			Load0 += ri.nLen;
			len0 += ri.nLen;
			bprintf(0, _T("SG-1000 - Loaded PRG #%X to 0x%X.\n"), i, len0);

			continue;
		}
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0xbfff, MAP_ROM);

	if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_SG1000_RAMEXP_A) == HARDWARE_SEGA_SG1000_RAMEXP_A) {
		bprintf(0, _T("SG-1000 - RAM Expansion mode A.\n"));
		ramexp = 1;
		ZetMapMemory(DrvZ80ROM + 0x0000, 0x0000, 0x1fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM + 0x0400, 0x2000, 0x3fff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM + 0x4000, 0x4000, 0xbfff, MAP_ROM);
	} else
	if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_SG1000_RAMEXP_B) == HARDWARE_SEGA_SG1000_RAMEXP_B) {
		bprintf(0, _T("SG-1000 - RAM Expansion mode B.\n"));
		ramexp = 1;
		ZetMapMemory(DrvZ80RAM + 0x400, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM + 0x400, 0xe000, 0xffff, MAP_RAM); // mirror
	} else
	if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_SG1000_RAMEXP_2K) == HARDWARE_SEGA_SG1000_RAMEXP_2K) {
		bprintf(0, _T("SG-1000 - RAM Expansion mode 2K (Othello).\n"));
		ramexp = 1;
		for (INT32 i = 0x8000; i < 0xc000; i += 0x800) {
			ZetMapMemory(DrvZ80RAM + 0x400, i, i + 0x7ff, MAP_RAM);
		}
	} else
	if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_SG1000_RAMEXP_8K) == HARDWARE_SEGA_SG1000_RAMEXP_8K) {
		bprintf(0, _T("SG-1000 - RAM Expansion mode 8K (The Castle).\n"));
		ramexp = 1;
		for (INT32 i = 0x8000; i < 0xc000; i += 0x2000) {
			bprintf(0, _T("mirror %x - %x \n"), i, i + 0x1fff);
			ZetMapMemory(DrvZ80RAM + 0x400, i, i + 0x1fff, MAP_RAM);
		}
	} else

		ramexp = 0;

	bprintf(0, _T("ramexp mode %x\n"), ramexp);

	ZetSetOutHandler(sg1000_write_port);
	ZetSetInHandler(sg1000_read_port);
	ZetSetWriteHandler(sg1000_write);
	ZetSetReadHandler(sg1000_read);
	ZetClose();

	SN76489AInit(0, 3579545, 0);
	SN76496SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 3579545);

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, vdp_interrupt);
	TMS9928ASetSpriteslimit((DrvDips[0] & 0x20) ? 0 : 1);
	bprintf(0, _T("Sprite Limit: %S\n"), (DrvDips[0] & 0x20) ? "Disabled" : "Enabled");

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	TMS9928AExit();
	ZetExit();
	SN76496Exit();

	BurnFreeMemIndex();

	ramexp = 0;

	return 0;
}

static INT32 DrvFrame()
{
	static UINT8 lastnmi = 0;

	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{ // Compile Inputs
		memset (DrvInputs, 0xff, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			if (i==6 || i==7)
				DrvInputs[1] ^= (DrvJoy1[i] & 1) << i;
			else
				DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if ((dip_changed ^ DrvDips[0]) & 0x20) {
			TMS9928ASetSpriteslimit((DrvDips[0] & 0x20) ? 0 : 1);
			bprintf(0, _T("Sprite Limit: %S\n"), (DrvDips[0] & 0x20) ? "Disabled" : "Enabled");
			dip_changed = DrvDips[0];
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3579545 / 60 };
	INT32 nCyclesDone[1] = { 0 };

    ZetOpen(0);

	if (DrvNMI && !lastnmi) {
		bprintf(0, _T("nmi %X.\n"), DrvNMI);
		ZetNmi();
		lastnmi = DrvNMI;
	} else lastnmi = DrvNMI;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);

		TMS9928AScanline(i);
	}

	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		TMS9928ADraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllMem;
		ba.nLen	  = MemEnd - AllMem; // some games use romspace as ram.
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);
		TMS9928AScan(nAction, pnMin);
	}

	return 0;
}

INT32 SG1KGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "SG1K_"
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 5); j++) {
		szFilename[j] = pszGameName[j + 5];
	}

	*pszName = szFilename;

	return 0;
}

// End of driver, the following driver info. has been synthesized from hash/sg1000.xml of MESS


// Bank Panic (Japan)
static struct BurnRomInfo sg1k_bankpRomDesc[] = {
	{ "Bank Panic (Japan)(1985)(Sega).sg",	0x08000, 0xd8a87095, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bankp)
STD_ROM_FN(sg1k_bankp)

struct BurnDriver BurnDrvsg1k_bankp = {
	"sg1k_bankp", NULL, NULL, NULL, "1985",
	"Bank Panic (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_bankpRomInfo, sg1k_bankpRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bi Li Da Dao (Taiwan)
static struct BurnRomInfo sg1k_bilidadaRomDesc[] = {
	{ "Bi Li Da Dao (Taiwan)(198x)(Aaronix).sg",	0x08000, 0xbd43fde4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bilidada)
STD_ROM_FN(sg1k_bilidada)

struct BurnDriver BurnDrvsg1k_bilidada = {
	"sg1k_bilidada", "sg1k_bankp", NULL, NULL, "198?",
	"Bi Li Da Dao (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_bilidadaRomInfo, sg1k_bilidadaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Black Onyx, The (Japan)
static struct BurnRomInfo sg1k_blckonyxRomDesc[] = {
	{ "Black Onyx, The (Japan)(1987)(Sega).sg",	0x08000, 0x26ecd094, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_blckonyx)
STD_ROM_FN(sg1k_blckonyx)

struct BurnDriver BurnDrvsg1k_blckonyx = {
	"sg1k_blckonyx", NULL, NULL, NULL, "1987",
	"Black Onyx, The (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RPG, 0,
	SG1KGetZipName, sg1k_blckonyxRomInfo, sg1k_blckonyxRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Black Onyx, The (Hack, T-Eng)
static struct BurnRomInfo sg1k_blckonyxeRomDesc[] = {
	{ "Black Onyx, the T-Eng (2010)(SSTranslations).sg",	65535, 0x626e30cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_blckonyxe)
STD_ROM_FN(sg1k_blckonyxe)

struct BurnDriver BurnDrvsg1k_blckonyxe = {
	"sg1k_blckonyxe", "sg1k_blckonyx", NULL, NULL, "2010",
	"Black Onyx, The (Hack, T-Eng)\0", NULL, "SSTranslations", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_SEGA_SG1000, GBF_RPG, 0,
	SG1KGetZipName, sg1k_blckonyxeRomInfo, sg1k_blckonyxeRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Black Onyx Enhanced, The (Hack, T-Eng)
static struct BurnRomInfo sg1k_blckonyxeeRomDesc[] = {
	{ "Black Onyx Enhanced, the T-Eng (2010)(SSTranslations).sg",	65535, 0x3003d9bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_blckonyxee)
STD_ROM_FN(sg1k_blckonyxee)

struct BurnDriver BurnDrvsg1k_blckonyxee = {
	"sg1k_blckonyxee", "sg1k_blckonyx", NULL, NULL, "2010",
	"Black Onyx Enhanced, The (Hack, T-Eng)\0", NULL, "SSTranslations", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_SEGA_SG1000, GBF_RPG, 0,
	SG1KGetZipName, sg1k_blckonyxeeRomInfo, sg1k_blckonyxeeRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bomb Jack (Japan)
static struct BurnRomInfo sg1k_bombjackRomDesc[] = {
	{ "Bomb Jack (Japan)(1985)(Sega).sg",	0x08000, 0xea0f2691, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjack)
STD_ROM_FN(sg1k_bombjack)

struct BurnDriver BurnDrvsg1k_bombjack = {
	"sg1k_bombjack", NULL, NULL, NULL, "1985",
	"Bomb Jack (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_bombjackRomInfo, sg1k_bombjackRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bomb Jack (Korea)
static struct BurnRomInfo sg1k_bombjackk1RomDesc[] = {
	{ "Bomb Jack (Korea)(198x).sg",	0x08000, 0x0c69d837, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackk1)
STD_ROM_FN(sg1k_bombjackk1)

struct BurnDriver BurnDrvsg1k_bombjackk1 = {
	"sg1k_bombjackk1", "sg1k_bombjack", NULL, NULL, "198?",
	"Bomb Jack (Korea)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_bombjackk1RomInfo, sg1k_bombjackk1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bomb Jack (Korea, Alt)
static struct BurnRomInfo sg1k_bombjackk2RomDesc[] = {
	{ "Bomb Jack (Korea, Alt)(198x).sg",	0x08000, 0xb0c7b310, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackk2)
STD_ROM_FN(sg1k_bombjackk2)

struct BurnDriver BurnDrvsg1k_bombjackk2 = {
	"sg1k_bombjackk2", "sg1k_bombjack", NULL, NULL, "198?",
	"Bomb Jack (Korea, Alt)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_bombjackk2RomInfo, sg1k_bombjackk2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bomb Jack (Taiwan)
static struct BurnRomInfo sg1k_bombjackt1RomDesc[] = {
	{ "Bomb Jack (Taiwan)(198x).sg",	0x08000, 0xcda3a335, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackt1)
STD_ROM_FN(sg1k_bombjackt1)

struct BurnDriver BurnDrvsg1k_bombjackt1 = {
	"sg1k_bombjackt1", "sg1k_bombjack", NULL, NULL, "198?",
	"Bomb Jack (Taiwan)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_bombjackt1RomInfo, sg1k_bombjackt1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bomber Man Special (Taiwan)
static struct BurnRomInfo sg1k_bombmnspRomDesc[] = {
	{ "Bomber Man Special (Taiwan)(198x)(DahJee).sg",	0x0c000, 0x69fc1494, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombmnsp)
STD_ROM_FN(sg1k_bombmnsp)

struct BurnDriver BurnDrvsg1k_bombmnsp = {
	"sg1k_bombmnsp", NULL, NULL, NULL, "198?",
	"Bomber Man Special (Taiwan)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_MAZE, 0,
	SG1KGetZipName, sg1k_bombmnspRomInfo, sg1k_bombmnspRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Borderline (Euro, Japan)
static struct BurnRomInfo sg1k_bordrlinRomDesc[] = {
	{ "Borderline (Euro, Japan)(1983)(Sega).sg",	0x04000, 0x0b4bca74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bordrlin)
STD_ROM_FN(sg1k_bordrlin)

struct BurnDriver BurnDrvsg1k_bordrlin = {
	"sg1k_bordrlin", NULL, NULL, NULL, "1983",
	"Borderline (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_bordrlinRomInfo, sg1k_bordrlinRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Chaoren (Taiwan)
static struct BurnRomInfo sg1k_bombjackt2RomDesc[] = {
	{ "Chaoren (Taiwan)(198x).sg",	0x08000, 0xdeb213fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackt2)
STD_ROM_FN(sg1k_bombjackt2)

struct BurnDriver BurnDrvsg1k_bombjackt2 = {
	"sg1k_bombjackt2", "sg1k_bombjack", NULL, NULL, "198?",
	"Chaoren (Taiwan)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_bombjackt2RomInfo, sg1k_bombjackt2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cabbage Patch Kids (Taiwan)
static struct BurnRomInfo sg1k_cabkidsRomDesc[] = {
	{ "Cabbage Patch Kids (Taiwan)(198x)(Aaronix).sg",	0x08000, 0x9d91ab78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cabkids)
STD_ROM_FN(sg1k_cabkids)

struct BurnDriver BurnDrvsg1k_cabkids = {
	"sg1k_cabkids", NULL, NULL, NULL, "198?",
	"Cabbage Patch Kids (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_cabkidsRomInfo, sg1k_cabkidsRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Castle, The (Japan)
static struct BurnRomInfo sg1k_castleRomDesc[] = {
	{ "Castle, The (Japan)(1986)(Sega).sg",	0x08000, 0x092f29d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_castle)
STD_ROM_FN(sg1k_castle)

struct BurnDriver BurnDrvsg1k_castle = {
	"sg1k_castle", NULL, NULL, NULL, "1986",
	"Castle, The (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_8K, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_castleRomInfo, sg1k_castleRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mowang migong ~ Castle, The (Taiwan)
static struct BurnRomInfo sg1k_castletRomDesc[] = {
	{ "Mowang migong - Castle, The (Taiwan)(198x)(DahJee).sg",	0x0c000, 0x2e366ccf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_castlet)
STD_ROM_FN(sg1k_castlet)

struct BurnDriver BurnDrvsg1k_castlet = {
	"sg1k_castlet", "sg1k_castle", NULL, NULL, "198?",
	"Mowang migong ~ Castle, The (Taiwan)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_castletRomInfo, sg1k_castletRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Chack'n Pop (Japan)
static struct BurnRomInfo sg1k_chacknRomDesc[] = {
	{ "Chack'n Pop (Japan)(1985)(Sega).sg",	0x08000, 0xd37bda49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chackn)
STD_ROM_FN(sg1k_chackn)

struct BurnDriver BurnDrvsg1k_chackn = {
	"sg1k_chackn", NULL, NULL, NULL, "1985",
	"Chack'n Pop (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_chacknRomInfo, sg1k_chacknRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Chack'n Pop (Hack, English)
// https://www.romhacking.net/translations/1550/
static struct BurnRomInfo sg1k_chackneRomDesc[] = {
	{ "Chack'n Pop T-Eng (2010)(goldenband).bin",	32768, 0xc106c923, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chackne)
STD_ROM_FN(sg1k_chackne)

struct BurnDriver BurnDrvsg1k_chackne = {
	"sg1k_chackne", "sg1k_chackn", NULL, NULL, "2010",
	"Chack'n Pop (Hack, English)\0", NULL, "goldenband", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_chackneRomInfo, sg1k_chackneRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Da Yu ~ Chack'n Pop (Taiwan)
static struct BurnRomInfo sg1k_dayuRomDesc[] = {
	{ "Da Yu - Chack'n Pop (Taiwan)(198x).sg",	0x08000, 0xd81a72ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dayu)
STD_ROM_FN(sg1k_dayu)

struct BurnDriver BurnDrvsg1k_dayu = {
	"sg1k_dayu", "sg1k_chackn", NULL, NULL, "198?",
	"Da Yu ~ Chack'n Pop\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_dayuRomInfo, sg1k_dayuRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Challenge Derby (Japan, OMV)

static struct BurnRomInfo sg1k_chaldrbyRomDesc[] = {
	{ "challenge derby [16k].bin",	0x04000, 0xc91551da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrby)
STD_ROM_FN(sg1k_chaldrby)

struct BurnDriver BurnDrvsg1k_chaldrby = {
	"sg1k_chaldrby", NULL, NULL, NULL, "1984",
	"Challenge Derby (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 8, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbyRomInfo, sg1k_chaldrbyRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Challenge Derby (Japan, OMV, Alt)

static struct BurnRomInfo sg1k_chaldrbyaRomDesc[] = {
	{ "challenge derby [40k].bin",	0x0a000, 0x02bc891f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrbya)
STD_ROM_FN(sg1k_chaldrbya)

struct BurnDriver BurnDrvsg1k_chaldrbya = {
	"sg1k_chaldrbya", "sg1k_chaldrby", NULL, NULL, "1984",
	"Challenge Derby (Japan, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 8, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbyaRomInfo, sg1k_chaldrbyaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Challenge Derby (Japan, OMV, Hacked)

static struct BurnRomInfo sg1k_chaldrbybRomDesc[] = {
	{ "challenge derby [ver.b] [40k].bin",	0x0a000, 0xc5f014dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrbyb)
STD_ROM_FN(sg1k_chaldrbyb)

struct BurnDriver BurnDrvsg1k_chaldrbyb = {
	"sg1k_chaldrbyb", "sg1k_chaldrby", NULL, NULL, "1984",
	"Challenge Derby (Japan, OMV, Hacked)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 8, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbybRomInfo, sg1k_chaldrbybRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Baseball (Japan)

static struct BurnRomInfo sg1k_champbasRomDesc[] = {
	{ "mpr-6381.ic1",	0x04000, 0x5970a12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbas)
STD_ROM_FN(sg1k_champbas)

struct BurnDriver BurnDrvsg1k_champbas = {
	"sg1k_champbas", NULL, NULL, NULL, "1983",
	"Champion Baseball (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champbasRomInfo, sg1k_champbasRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Baseball (Japan, Older)

static struct BurnRomInfo sg1k_champbasaRomDesc[] = {
	{ "champion baseball [40k].bin",	0x0a000, 0xc0c16fa7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbasa)
STD_ROM_FN(sg1k_champbasa)

struct BurnDriver BurnDrvsg1k_champbasa = {
	"sg1k_champbasa", "sg1k_champbas", NULL, NULL, "1983",
	"Champion Baseball (Japan, Older)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champbasaRomInfo, sg1k_champbasaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Baseball (Taiwan)

static struct BurnRomInfo sg1k_champbastRomDesc[] = {
	{ "champion baseball [16k] (tw).bin",	0x04000, 0x677e6878, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbast)
STD_ROM_FN(sg1k_champbast)

struct BurnDriver BurnDrvsg1k_champbast = {
	"sg1k_champbast", "sg1k_champbas", NULL, NULL, "1985?",
	"Champion Baseball (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champbastRomInfo, sg1k_champbastRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Billiards (Japan)

static struct BurnRomInfo sg1k_champbilRomDesc[] = {
	{ "champion billiards (japan).bin",	0x08000, 0x62b21e31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbil)
STD_ROM_FN(sg1k_champbil)

struct BurnDriver BurnDrvsg1k_champbil = {
	"sg1k_champbil", NULL, NULL, NULL, "1986",
	"Champion Billiards (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champbilRomInfo, sg1k_champbilRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Boxing (Japan)

static struct BurnRomInfo sg1k_champboxRomDesc[] = {
	{ "mpr-6100.ic1",	0x08000, 0x26f947d1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbox)
STD_ROM_FN(sg1k_champbox)

struct BurnDriver BurnDrvsg1k_champbox = {
	"sg1k_champbox", NULL, NULL, NULL, "1984",
	"Champion Boxing (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champboxRomInfo, sg1k_champboxRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Boxing (Taiwan)

static struct BurnRomInfo sg1k_champboxtRomDesc[] = {
	{ "champion boxing (tw).bin",	0x08000, 0x15d2ce33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champboxt)
STD_ROM_FN(sg1k_champboxt)

struct BurnDriver BurnDrvsg1k_champboxt = {
	"sg1k_champboxt", "sg1k_champbox", NULL, NULL, "1984?",
	"Champion Boxing (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champboxtRomInfo, sg1k_champboxtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Boxing (Japan, MyCard)

static struct BurnRomInfo sg1k_champbox1RomDesc[] = {
	{ "champion boxing (japan) (mycard).bin",	0x08000, 0xf8b2ac1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbox1)
STD_ROM_FN(sg1k_champbox1)

struct BurnDriver BurnDrvsg1k_champbox1 = {
	"sg1k_champbox1", "sg1k_champbox", NULL, NULL, "1984",
	"Champion Boxing (Japan, MyCard)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champbox1RomInfo, sg1k_champbox1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Golf (Japan)

static struct BurnRomInfo sg1k_champglfRomDesc[] = {
	{ "mpr-5435.ic1",	0x08000, 0x868419b5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglf)
STD_ROM_FN(sg1k_champglf)

struct BurnDriver BurnDrvsg1k_champglf = {
	"sg1k_champglf", NULL, NULL, NULL, "1983",
	"Champion Golf (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champglfRomInfo, sg1k_champglfRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Golf (Korea)

static struct BurnRomInfo sg1k_champglfkRomDesc[] = {
	{ "champion golf (kr).bin",	0x08000, 0x35af42ad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglfk)
STD_ROM_FN(sg1k_champglfk)

struct BurnDriver BurnDrvsg1k_champglfk = {
	"sg1k_champglfk", "sg1k_champglf", NULL, NULL, "198?",
	"Champion Golf (Korea)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champglfkRomInfo, sg1k_champglfkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Golf (Japan, MyCard)

static struct BurnRomInfo sg1k_champglf1RomDesc[] = {
	{ "champion golf [card].bin",	0x08000, 0x5a904122, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglf1)
STD_ROM_FN(sg1k_champglf1)

struct BurnDriver BurnDrvsg1k_champglf1 = {
	"sg1k_champglf1", "sg1k_champglf", NULL, NULL, "1983",
	"Champion Golf (Japan, MyCard)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champglf1RomInfo, sg1k_champglf1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Ice Hockey (Japan)

static struct BurnRomInfo sg1k_champiceRomDesc[] = {
	{ "champion ice hockey (japan).bin",	0x08000, 0xbdc05652, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champice)
STD_ROM_FN(sg1k_champice)

struct BurnDriver BurnDrvsg1k_champice = {
	"sg1k_champice", NULL, NULL, NULL, "1985",
	"Champion Ice Hockey (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champiceRomInfo, sg1k_champiceRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Ice Hockey (Korea)

static struct BurnRomInfo sg1k_champicekRomDesc[] = {
	{ "champion ice hockey (kr).bin",	0x08000, 0xec0b862c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champicek)
STD_ROM_FN(sg1k_champicek)

struct BurnDriver BurnDrvsg1k_champicek = {
	"sg1k_champicek", "sg1k_champice", NULL, NULL, "198?",
	"Champion Ice Hockey (Korea)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champicekRomInfo, sg1k_champicekRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Qugunqiu (Taiwan)

static struct BurnRomInfo sg1k_champicetRomDesc[] = {
	{ "champion ice hockey (tw).bin",	0x08000, 0xc6e5192f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champicet)
STD_ROM_FN(sg1k_champicet)

struct BurnDriver BurnDrvsg1k_champicet = {
	"sg1k_champicet", "sg1k_champice", NULL, NULL, "1985?",
	"Qugunqiu (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champicetRomInfo, sg1k_champicetRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Kendou (Japan)

static struct BurnRomInfo sg1k_champkenRomDesc[] = {
	{ "champion kendou (japan).bin",	0x08000, 0x10cdebce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champken)
STD_ROM_FN(sg1k_champken)

struct BurnDriver BurnDrvsg1k_champken = {
	"sg1k_champken", NULL, NULL, NULL, "1986",
	"Champion Kendou (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champkenRomInfo, sg1k_champkenRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jiandao (Taiwan)

static struct BurnRomInfo sg1k_champkentRomDesc[] = {
	{ "champion kendou (tw).bin",	0x08000, 0xa5f61363, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champkent)
STD_ROM_FN(sg1k_champkent)

struct BurnDriver BurnDrvsg1k_champkent = {
	"sg1k_champkent", "sg1k_champken", NULL, NULL, "1986?",
	"Jiandao (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champkentRomInfo, sg1k_champkentRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Pro Wrestling (Japan)

static struct BurnRomInfo sg1k_champpwrRomDesc[] = {
	{ "champion pro wrestling (japan).bin",	0x08000, 0x372fe6bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champpwr)
STD_ROM_FN(sg1k_champpwr)

struct BurnDriver BurnDrvsg1k_champpwr = {
	"sg1k_champpwr", NULL, NULL, NULL, "1985",
	"Champion Pro Wrestling (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_champpwrRomInfo, sg1k_champpwrRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Soccer (Japan)

static struct BurnRomInfo sg1k_champscrRomDesc[] = {
	{ "champion soccer (japan).bin",	0x04000, 0x6f39719e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champscr)
STD_ROM_FN(sg1k_champscr)

struct BurnDriver BurnDrvsg1k_champscr = {
	"sg1k_champscr", NULL, NULL, NULL, "1984",
	"Champion Soccer (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSFOOTBALL, 0,
	SG1KGetZipName, sg1k_champscrRomInfo, sg1k_champscrRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Soccer (Taiwan)

static struct BurnRomInfo sg1k_champscrtRomDesc[] = {
	{ "champion soccer (tw).bin",	0x04000, 0x269c1aee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champscrt)
STD_ROM_FN(sg1k_champscrt)

struct BurnDriver BurnDrvsg1k_champscrt = {
	"sg1k_champscrt", "sg1k_champscr", NULL, NULL, "1984?",
	"Champion Soccer (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSFOOTBALL, 0,
	SG1KGetZipName, sg1k_champscrtRomInfo, sg1k_champscrtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Champion Tennis (Japan)

static struct BurnRomInfo sg1k_champtnsRomDesc[] = {
	{ "mpr-5439.ic1",	0x02000, 0x7c663316, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champtns)
STD_ROM_FN(sg1k_champtns)

struct BurnDriver BurnDrvsg1k_champtns = {
	"sg1k_champtns", NULL, NULL, NULL, "1983",
	"Champion Tennis (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_champtnsRomInfo, sg1k_champtnsRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Choplifter (Japan)

static struct BurnRomInfo sg1k_chopliftRomDesc[] = {
	{ "choplifter (japan).bin",	0x08000, 0x732b7180, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_choplift)
STD_ROM_FN(sg1k_choplift)

struct BurnDriver BurnDrvsg1k_choplift = {
	"sg1k_choplift", NULL, NULL, NULL, "1985",
	"Choplifter (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_chopliftRomInfo, sg1k_chopliftRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Choplifter (Japan, Prototype)

static struct BurnRomInfo sg1k_chopliftpRomDesc[] = {
	{ "choplifter (japan) (proto).bin",	0x08000, 0xff435469, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftp)
STD_ROM_FN(sg1k_chopliftp)

struct BurnDriver BurnDrvsg1k_chopliftp = {
	"sg1k_chopliftp", "sg1k_choplift", NULL, NULL, "1985",
	"Choplifter (Japan, Prototype)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_chopliftpRomInfo, sg1k_chopliftpRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Choplifter (Korea)

static struct BurnRomInfo sg1k_chopliftkRomDesc[] = {
	{ "choplifter (kr).bin",	0x08000, 0x954d8f26, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftk)
STD_ROM_FN(sg1k_chopliftk)

struct BurnDriver BurnDrvsg1k_chopliftk = {
	"sg1k_chopliftk", "sg1k_choplift", NULL, NULL, "1985?",
	"Choplifter (Korea)\0", NULL, "Sega?", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_chopliftkRomInfo, sg1k_chopliftkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fei Lang (Taiwan)

static struct BurnRomInfo sg1k_feilangRomDesc[] = {
	{ "choplifter [chinese logo] (tw).bin",	0x08000, 0x746a8d86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_feilang)
STD_ROM_FN(sg1k_feilang)

struct BurnDriver BurnDrvsg1k_feilang = {
	"sg1k_feilang", "sg1k_choplift", NULL, NULL, "1985?",
	"Fei Lang (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_feilangRomInfo, sg1k_feilangRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fei Lang ~ Choplifter (Taiwan, Alt)

static struct BurnRomInfo sg1k_feilangaRomDesc[] = {
	{ "choplifter [english logo] (tw).bin",	0x08000, 0x7c603987, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_feilanga)
STD_ROM_FN(sg1k_feilanga)

struct BurnDriver BurnDrvsg1k_feilanga = {
	"sg1k_feilanga", "sg1k_choplift", NULL, NULL, "1985?",
	"Fei Lang ~ Choplifter (Taiwan, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_feilangaRomInfo, sg1k_feilangaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Choplifter (Taiwan)

static struct BurnRomInfo sg1k_choplifttRomDesc[] = {
	{ "choplifter [no logo] (tw).bin",	0x08000, 0x825e1aae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftt)
STD_ROM_FN(sg1k_chopliftt)

struct BurnDriver BurnDrvsg1k_chopliftt = {
	"sg1k_chopliftt", "sg1k_choplift", NULL, NULL, "1985?",
	"Choplifter (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_choplifttRomInfo, sg1k_choplifttRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Championship Lode Runner (Japan)

static struct BurnRomInfo sg1k_cloderunRomDesc[] = {
	{ "championship lode runner (japan).bin",	0x08000, 0x11db4b1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cloderun)
STD_ROM_FN(sg1k_cloderun)

struct BurnDriver BurnDrvsg1k_cloderun = {
	"sg1k_cloderun", NULL, NULL, NULL, "1985",
	"Championship Lode Runner (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_cloderunRomInfo, sg1k_cloderunRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Xun Bao Er Dai (Taiwan)

static struct BurnRomInfo sg1k_cloderuntRomDesc[] = {
	{ "championship lode runner (tw).bin",	0x08000, 0xec95ebcb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cloderunt)
STD_ROM_FN(sg1k_cloderunt)

struct BurnDriver BurnDrvsg1k_cloderunt = {
	"sg1k_cloderunt", "sg1k_cloderun", NULL, NULL, "1985?",
	"Xun Bao Er Dai (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_cloderuntRomInfo, sg1k_cloderuntRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Maxi Tuan ~ Circus Charlie (Taiwan)

static struct BurnRomInfo sg1k_circuscRomDesc[] = {
	{ "circus charlie (tw).bin",	0x08000, 0x3404fce4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_circusc)
STD_ROM_FN(sg1k_circusc)

struct BurnDriver BurnDrvsg1k_circusc = {
	"sg1k_circusc", NULL, NULL, NULL, "198?",
	"Circus Charlie (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_circuscRomInfo, sg1k_circuscRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Circus Charlie (Korea)

static struct BurnRomInfo sg1k_circusckRomDesc[] = {
	{ "circus charlie (kr).bin",	0x08000, 0x7f7f009d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_circusck)
STD_ROM_FN(sg1k_circusck)

struct BurnDriver BurnDrvsg1k_circusck = {
	"sg1k_circusck", "sg1k_circusc", NULL, NULL, "198?",
	"Circus Charlie (Korea)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_circusckRomInfo, sg1k_circusckRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Congo Bongo (Euro, Japan, v1)

static struct BurnRomInfo sg1k_congoRomDesc[] = {
	{ "congo bongo [v1].bin",	0x06000, 0x5a24c7cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congo)
STD_ROM_FN(sg1k_congo)

struct BurnDriver BurnDrvsg1k_congo = {
	"sg1k_congo", NULL, NULL, NULL, "1983",
	"Congo Bongo (Euro, Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_congoRomInfo, sg1k_congoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Congo Bongo (Euro, Japan, v1 Alt)

static struct BurnRomInfo sg1k_congoaRomDesc[] = {
	{ "congo bongo [v1] [40k map].bin",	0x0a000, 0xf1506565, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congoa)
STD_ROM_FN(sg1k_congoa)

struct BurnDriver BurnDrvsg1k_congoa = {
	"sg1k_congoa", "sg1k_congo", NULL, NULL, "1983",
	"Congo Bongo (Euro, Japan, v1 Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_congoaRomInfo, sg1k_congoaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Congo Bongo (Japan, v0)

static struct BurnRomInfo sg1k_congobRomDesc[] = {
	{ "congo bongo [v0].bin",	0x06000, 0x5eb48a20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congob)
STD_ROM_FN(sg1k_congob)

struct BurnDriver BurnDrvsg1k_congob = {
	"sg1k_congob", "sg1k_congo", NULL, NULL, "1983",
	"Congo Bongo (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_congobRomInfo, sg1k_congobRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Congo Bongo (Taiwan)

static struct BurnRomInfo sg1k_congotRomDesc[] = {
	{ "congo bongo (tw).bin",	0x06000, 0xf7eb94c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congot)
STD_ROM_FN(sg1k_congot)

struct BurnDriver BurnDrvsg1k_congot = {
	"sg1k_congot", "sg1k_congo", NULL, NULL, "19??",
	"Congo Bongo (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_congotRomInfo, sg1k_congotRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// C_So! (Japan)

static struct BurnRomInfo sg1k_csoRomDesc[] = {
	{ "c_so! (japan).bin",	0x08000, 0xbe7ed0eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cso)
STD_ROM_FN(sg1k_cso)

struct BurnDriver BurnDrvsg1k_cso = {
	"sg1k_cso", NULL, NULL, NULL, "1985",
	"C_So! (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_csoRomInfo, sg1k_csoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Wanpi Gui (Taiwan)

static struct BurnRomInfo sg1k_csotRomDesc[] = {
	{ "c_so! (tw).bin",	0x08000, 0x82c4e3e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_csot)
STD_ROM_FN(sg1k_csot)

struct BurnDriver BurnDrvsg1k_csot = {
	"sg1k_csot", "sg1k_cso", NULL, NULL, "1985?",
	"Wanpi Gui (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_csotRomInfo, sg1k_csotRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Da Cike (Taiwan)

static struct BurnRomInfo sg1k_dacikeRomDesc[] = {
	{ "yie ar kung-fu ii [dahjee] (tw).bin",	0x0c000, 0xfc87463c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dacike)
STD_ROM_FN(sg1k_dacike)

struct BurnDriver BurnDrvsg1k_dacike = {
	"sg1k_dacike", NULL, NULL, NULL, "1986?",
	"Yie Ar Kung-Fu II, Da Cike (Taiwan)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_A, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_dacikeRomInfo, sg1k_dacikeRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dokidoki Penguin Land (Japan)

static struct BurnRomInfo sg1k_dokidokiRomDesc[] = {
	{ "dokidoki penguin land (japan).bin",	0x08000, 0x346556b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dokidoki)
STD_ROM_FN(sg1k_dokidoki)

struct BurnDriver BurnDrvsg1k_dokidoki = {
	"sg1k_dokidoki", NULL, NULL, NULL, "1985",
	"Dokidoki Penguin Land (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM | GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_dokidokiRomInfo, sg1k_dokidokiRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Qi E (Taiwan)

static struct BurnRomInfo sg1k_qieRomDesc[] = {
	{ "doki doki penguin land (19xx)(-)(tw).bin",	0x08000, 0xfdc095bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_qie)
STD_ROM_FN(sg1k_qie)

struct BurnDriver BurnDrvsg1k_qie = {
	"sg1k_qie", "sg1k_dokidoki", NULL, NULL, "1985?",
	"Qi E (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM | GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_qieRomInfo, sg1k_qieRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dragon Wang (Japan, v1)

static struct BurnRomInfo sg1k_dragwangRomDesc[] = {
	{ "dragon wang (japan).bin",	0x08000, 0x99c3de21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dragwang)
STD_ROM_FN(sg1k_dragwang)

struct BurnDriver BurnDrvsg1k_dragwang = {
	"sg1k_dragwang", NULL, NULL, NULL, "1985",
	"Dragon Wang (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SCRFIGHT, 0,
	SG1KGetZipName, sg1k_dragwangRomInfo, sg1k_dragwangRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jing Wu Men (Taiwan)

static struct BurnRomInfo sg1k_jingwumnRomDesc[] = {
	{ "dragon wang (tw).bin",	0x08000, 0x7c7d4397, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jingwumn)
STD_ROM_FN(sg1k_jingwumn)

struct BurnDriver BurnDrvsg1k_jingwumn = {
	"sg1k_jingwumn", "sg1k_dragwang", NULL, NULL, "1985?",
	"Jing Wu Men (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SCRFIGHT, 0,
	SG1KGetZipName, sg1k_jingwumnRomInfo, sg1k_jingwumnRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jing Wu Men (Taiwan, Alt)

static struct BurnRomInfo sg1k_jingwumnaRomDesc[] = {
	{ "dragon wang [v0] [english logo] (tw).bin",	0x08000, 0x6f94e5c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jingwumna)
STD_ROM_FN(sg1k_jingwumna)

struct BurnDriver BurnDrvsg1k_jingwumna = {
	"sg1k_jingwumna", "sg1k_dragwang", NULL, NULL, "1985?",
	"Jing Wu Men (Taiwan, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SCRFIGHT, 0,
	SG1KGetZipName, sg1k_jingwumnaRomInfo, sg1k_jingwumnaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dragon Wang (Japan, v0)

static struct BurnRomInfo sg1k_dragwang1RomDesc[] = {
	{ "dragon wang (japan) (alt).bin",	0x08000, 0x60f30138, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dragwang1)
STD_ROM_FN(sg1k_dragwang1)

struct BurnDriver BurnDrvsg1k_dragwang1 = {
	"sg1k_dragwang1", "sg1k_dragwang", NULL, NULL, "1985",
	"Dragon Wang (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SCRFIGHT, 0,
	SG1KGetZipName, sg1k_dragwang1RomInfo, sg1k_dragwang1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Drol (Japan)

static struct BurnRomInfo sg1k_drolRomDesc[] = {
	{ "drol (japan).bin",	0x08000, 0x288940cb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drol)
STD_ROM_FN(sg1k_drol)

struct BurnDriver BurnDrvsg1k_drol = {
	"sg1k_drol", NULL, NULL, NULL, "1985",
	"Drol (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN | GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_drolRomInfo, sg1k_drolRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Drol (Taiwan)

static struct BurnRomInfo sg1k_droltRomDesc[] = {
	{ "drol (tw).bin",	0x08000, 0xb7fc033d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drolt)
STD_ROM_FN(sg1k_drolt)

struct BurnDriver BurnDrvsg1k_drolt = {
	"sg1k_drolt", "sg1k_drol", NULL, NULL, "1985?",
	"Drol (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN | GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_droltRomInfo, sg1k_droltRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Drol (Korea)

static struct BurnRomInfo sg1k_drolkRomDesc[] = {
	{ "drol (kr).bin",	0x08000, 0x1d7c53bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drolk)
STD_ROM_FN(sg1k_drolk)

struct BurnDriver BurnDrvsg1k_drolk = {
	"sg1k_drolk", "sg1k_drol", NULL, NULL, "198?",
	"Drol (Korea)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN | GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_drolkRomInfo, sg1k_drolkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Elevator Action (Japan)

static struct BurnRomInfo sg1k_elevatorRomDesc[] = {
	{ "elevator action (japan).bin",	0x08000, 0x5af8f69d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_elevator)
STD_ROM_FN(sg1k_elevator)

struct BurnDriver BurnDrvsg1k_elevator = {
	"sg1k_elevator", NULL, NULL, NULL, "1985",
	"Elevator Action (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_elevatorRomInfo, sg1k_elevatorRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Die Dui Die (Taiwan)

static struct BurnRomInfo sg1k_elevatortRomDesc[] = {
	{ "elevator action (tw).bin",	0x08000, 0x6846e36d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_elevatort)
STD_ROM_FN(sg1k_elevatort)

struct BurnDriver BurnDrvsg1k_elevatort = {
	"sg1k_elevatort", "sg1k_elevator", NULL, NULL, "1985?",
	"Die Dui Die (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_elevatortRomInfo, sg1k_elevatortRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Exerion (Euro, Japan)

static struct BurnRomInfo sg1k_exerionRomDesc[] = {
	{ "mpr-5692.ic1",	0x04000, 0xa2c45b61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exerion)
STD_ROM_FN(sg1k_exerion)

struct BurnDriver BurnDrvsg1k_exerion = {
	"sg1k_exerion", NULL, NULL, NULL, "1983",
	"Exerion (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_exerionRomInfo, sg1k_exerionRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Exerion (Taiwan)

static struct BurnRomInfo sg1k_exeriontRomDesc[] = {
	{ "exerion (tw).bin",	0x04000, 0xb5c84a32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exeriont)
STD_ROM_FN(sg1k_exeriont)

struct BurnDriver BurnDrvsg1k_exeriont = {
	"sg1k_exeriont", "sg1k_exerion", NULL, NULL, "1983?",
	"Exerion (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_exeriontRomInfo, sg1k_exeriontRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Exerion (Korea)

static struct BurnRomInfo sg1k_exerionkRomDesc[] = {
	{ "exerion (kr).bin",	0x08000, 0xfc628d0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exerionk)
STD_ROM_FN(sg1k_exerionk)

struct BurnDriver BurnDrvsg1k_exerionk = {
	"sg1k_exerionk", "sg1k_exerion", NULL, NULL, "198?",
	"Exerion (Korea)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_exerionkRomInfo, sg1k_exerionkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Flicky (Japan, v1)

static struct BurnRomInfo sg1k_flickyRomDesc[] = {
	{ "flicky [v1].bin",	0x08000, 0x26d2862c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flicky)
STD_ROM_FN(sg1k_flicky)

struct BurnDriver BurnDrvsg1k_flicky = {
	"sg1k_flicky", NULL, NULL, NULL, "1984",
	"Flicky (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_flickyRomInfo, sg1k_flickyRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Flicky (Japan, v0)

static struct BurnRomInfo sg1k_flickyaRomDesc[] = {
	{ "flicky (japan).bin",	0x08000, 0xbd24d27b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flickya)
STD_ROM_FN(sg1k_flickya)

struct BurnDriver BurnDrvsg1k_flickya = {
	"sg1k_flickya", "sg1k_flicky", NULL, NULL, "1984",
	"Flicky (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_flickyaRomInfo, sg1k_flickyaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Flicky (Taiwan)

static struct BurnRomInfo sg1k_flickytRomDesc[] = {
	{ "flicky (tw).bin",	0x08000, 0xcd0666a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flickyt)
STD_ROM_FN(sg1k_flickyt)

struct BurnDriver BurnDrvsg1k_flickyt = {
	"sg1k_flickyt", "sg1k_flicky", NULL, NULL, "1984?",
	"Flicky (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_flickytRomInfo, sg1k_flickytRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Girl's Garden (Japan)

static struct BurnRomInfo sg1k_girlgardRomDesc[] = {
	{ "girl's garden (japan).bin",	0x08000, 0x1898f274, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_girlgard)
STD_ROM_FN(sg1k_girlgard)

struct BurnDriver BurnDrvsg1k_girlgard = {
	"sg1k_girlgard", NULL, NULL, NULL, "1984",
	"Girl's Garden (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_girlgardRomInfo, sg1k_girlgardRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Girl's Garden (Taiwan)

static struct BurnRomInfo sg1k_girlgardtRomDesc[] = {
	{ "girl's garden (tw).bin",	0x08000, 0xb9635ac4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_girlgardt)
STD_ROM_FN(sg1k_girlgardt)

struct BurnDriver BurnDrvsg1k_girlgardt = {
	"sg1k_girlgardt", "sg1k_girlgard", NULL, NULL, "1984?",
	"Girl's Garden (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_girlgardtRomInfo, sg1k_girlgardtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Golgo 13 (Japan)

static struct BurnRomInfo sg1k_golgo13RomDesc[] = {
	{ "golgo 13 (japan).bin",	0x08000, 0x0d159ed0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_golgo13)
STD_ROM_FN(sg1k_golgo13)

struct BurnDriver BurnDrvsg1k_golgo13 = {
	"sg1k_golgo13", NULL, NULL, NULL, "1984",
	"Golgo 13 (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_golgo13RomInfo, sg1k_golgo13RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// GP World (Japan, v1)

static struct BurnRomInfo sg1k_gpworldRomDesc[] = {
	{ "gp world (japan).bin",	0x08000, 0x191ffe0a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworld)
STD_ROM_FN(sg1k_gpworld)

struct BurnDriver BurnDrvsg1k_gpworld = {
	"sg1k_gpworld", NULL, NULL, NULL, "1985",
	"GP World (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_gpworldRomInfo, sg1k_gpworldRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// GP World (Japan, v0)

static struct BurnRomInfo sg1k_gpworldaRomDesc[] = {
	{ "mpr-6485.ic1",	0x08000, 0x942adf84, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworlda)
STD_ROM_FN(sg1k_gpworlda)

struct BurnDriver BurnDrvsg1k_gpworlda = {
	"sg1k_gpworlda", "sg1k_gpworld", NULL, NULL, "1985",
	"GP World (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_gpworldaRomInfo, sg1k_gpworldaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// GP World (Taiwan)

static struct BurnRomInfo sg1k_gpworldtRomDesc[] = {
	{ "gp world (tw).bin",	0x08000, 0xf19f7548, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworldt)
STD_ROM_FN(sg1k_gpworldt)

struct BurnDriver BurnDrvsg1k_gpworldt = {
	"sg1k_gpworldt", "sg1k_gpworld", NULL, NULL, "1985?",
	"GP World (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_gpworldtRomInfo, sg1k_gpworldtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gulkave (Japan)

static struct BurnRomInfo sg1k_gulkaveRomDesc[] = {
	{ "gulkave (japan).bin",	0x08000, 0x15a754a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gulkave)
STD_ROM_FN(sg1k_gulkave)

struct BurnDriver BurnDrvsg1k_gulkave = {
	"sg1k_gulkave", NULL, NULL, NULL, "1986",
	"Gulkave (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_gulkaveRomInfo, sg1k_gulkaveRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gulkave (Korea)

static struct BurnRomInfo sg1k_gulkavekRomDesc[] = {
	{ "gulkave (kr).bin",	0x08000, 0xddcb708b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gulkavek)
STD_ROM_FN(sg1k_gulkavek)

struct BurnDriver BurnDrvsg1k_gulkavek = {
	"sg1k_gulkavek", "sg1k_gulkave", NULL, NULL, "198?",
	"Gulkave (Korea)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_gulkavekRomInfo, sg1k_gulkavekRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Guzzler (Japan, OMV)

static struct BurnRomInfo sg1k_guzzlerRomDesc[] = {
	{ "guzzler (japan) (om).bin",	0x02000, 0x61fa9ea0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_guzzler)
STD_ROM_FN(sg1k_guzzler)

struct BurnDriver BurnDrvsg1k_guzzler = {
	"sg1k_guzzler", NULL, NULL, NULL, "1983",
	"Guzzler (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_MAZE, 0,
	SG1KGetZipName, sg1k_guzzlerRomInfo, sg1k_guzzlerRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Guzzler (Taiwan)

static struct BurnRomInfo sg1k_guzzlertRomDesc[] = {
	{ "guzzler (tw).bin",	0x02000, 0xeb808158, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_guzzlert)
STD_ROM_FN(sg1k_guzzlert)

struct BurnDriver BurnDrvsg1k_guzzlert = {
	"sg1k_guzzlert", "sg1k_guzzler", NULL, NULL, "1983?",
	"Guzzler (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_MAZE, 0,
	SG1KGetZipName, sg1k_guzzlertRomInfo, sg1k_guzzlertRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hang-On II (Japan)

static struct BurnRomInfo sg1k_hangon2RomDesc[] = {
	{ "hang-on ii (japan).bin",	0x08000, 0x9be3c6bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2)
STD_ROM_FN(sg1k_hangon2)

struct BurnDriver BurnDrvsg1k_hangon2 = {
	"sg1k_hangon2", NULL, NULL, NULL, "1985",
	"Hang-On II (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_hangon2RomInfo, sg1k_hangon2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hang-On II (Taiwan)

static struct BurnRomInfo sg1k_hangon2t1RomDesc[] = {
	{ "hang-on ii [english logo] (tw).bin",	0x08000, 0xcabd451b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2t1)
STD_ROM_FN(sg1k_hangon2t1)

struct BurnDriver BurnDrvsg1k_hangon2t1 = {
	"sg1k_hangon2t1", "sg1k_hangon2", NULL, NULL, "1985?",
	"Hang-On II (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_hangon2t1RomInfo, sg1k_hangon2t1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Motuo Feiche (Taiwan)

static struct BurnRomInfo sg1k_hangon2t2RomDesc[] = {
	{ "hang-on ii [chinese logo] (tw).bin",	0x08000, 0xe98a111e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2t2)
STD_ROM_FN(sg1k_hangon2t2)

struct BurnDriver BurnDrvsg1k_hangon2t2 = {
	"sg1k_hangon2t2", "sg1k_hangon2", NULL, NULL, "1985?",
	"Motuo Feiche (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_hangon2t2RomInfo, sg1k_hangon2t2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// H.E.R.O. (Japan)

static struct BurnRomInfo sg1k_heroRomDesc[] = {
	{ "h.e.r.o. (japan).bin",	0x08000, 0x4587de6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hero)
STD_ROM_FN(sg1k_hero)

struct BurnDriver BurnDrvsg1k_hero = {
	"sg1k_hero", NULL, NULL, NULL, "1985",
	"H.E.R.O. (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_heroRomInfo, sg1k_heroRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Qing Feng Xia (Taiwan)

static struct BurnRomInfo sg1k_herotRomDesc[] = {
	{ "h.e.r.o. (tw).bin",	0x08000, 0x83958998, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_herot)
STD_ROM_FN(sg1k_herot)

struct BurnDriver BurnDrvsg1k_herot = {
	"sg1k_herot", "sg1k_hero", NULL, NULL, "1985?",
	"Qing Feng Xia (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_herotRomInfo, sg1k_herotRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Home Mahjong (Japan, v1)

static struct BurnRomInfo sg1k_homemjRomDesc[] = {
	{ "home mahjong (japan) (v1.1).bin",	0x0c000, 0xe7e0f0e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemj)
STD_ROM_FN(sg1k_homemj)

struct BurnDriver BurnDrvsg1k_homemj = {
	"sg1k_homemj", NULL, NULL, NULL, "1984",
	"Home Mahjong (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_homemjRomInfo, sg1k_homemjRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Home Mahjong (Japan, v0)

static struct BurnRomInfo sg1k_homemj1RomDesc[] = {
	{ "home mahjong (japan) (v1.0).bin",	0x0c000, 0xc9d1ae7d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemj1)
STD_ROM_FN(sg1k_homemj1)

struct BurnDriver BurnDrvsg1k_homemj1 = {
	"sg1k_homemj1", "sg1k_homemj", NULL, NULL, "1984",
	"Home Mahjong (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_homemj1RomInfo, sg1k_homemj1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Yon-nin Mahjong (Taiwan)

static struct BurnRomInfo sg1k_homemjtRomDesc[] = {
	{ "home mahjong (tw).bin",	0x0c000, 0x0583a9fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemjt)
STD_ROM_FN(sg1k_homemjt)

struct BurnDriver BurnDrvsg1k_homemjt = {
	"sg1k_homemjt", "sg1k_homemj", NULL, NULL, "1984?",
	"Yon-nin Mahjong (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_homemjtRomInfo, sg1k_homemjtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hustle Chumy (Japan)

static struct BurnRomInfo sg1k_hustleRomDesc[] = {
	{ "hustle chumy (japan).bin",	0x04000, 0xa627d440, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hustle)
STD_ROM_FN(sg1k_hustle)

struct BurnDriver BurnDrvsg1k_hustle = {
	"sg1k_hustle", NULL, NULL, NULL, "1984",
	"Hustle Chumy (Japan)\0", NULL, "Compile", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_hustleRomInfo, sg1k_hustleRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hyper Sports (Japan)

static struct BurnRomInfo sg1k_hypersptRomDesc[] = {
	{ "mpr-6487.ic1",	0x08000, 0xba09a0fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hyperspt)
STD_ROM_FN(sg1k_hyperspt)

struct BurnDriver BurnDrvsg1k_hyperspt = {
	"sg1k_hyperspt", NULL, NULL, NULL, "1985",
	"Hyper Sports (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_hypersptRomInfo, sg1k_hypersptRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hyper Sports (Taiwan)

static struct BurnRomInfo sg1k_hyperspttRomDesc[] = {
	{ "hyper sports (tw).bin",	0x08000, 0x87619ac2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersptt)
STD_ROM_FN(sg1k_hypersptt)

struct BurnDriver BurnDrvsg1k_hypersptt = {
	"sg1k_hypersptt", "sg1k_hyperspt", NULL, NULL, "1985?",
	"Hyper Sports (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_hyperspttRomInfo, sg1k_hyperspttRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hyper Sports (Korea)

static struct BurnRomInfo sg1k_hypersptkRomDesc[] = {
	{ "hyper sports (kr).bin",	0x08000, 0x3157ef9c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersptk)
STD_ROM_FN(sg1k_hypersptk)

struct BurnDriver BurnDrvsg1k_hypersptk = {
	"sg1k_hypersptk", "sg1k_hyperspt", NULL, NULL, "198?",
	"Hyper Sports (Korea)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_hypersptkRomInfo, sg1k_hypersptkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hyper Sports 2 (Taiwan)

static struct BurnRomInfo sg1k_hypersp2RomDesc[] = {
	{ "hyper sports 2 (tw).bin",	0x08000, 0xb0234e12, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersp2)
STD_ROM_FN(sg1k_hypersp2)

struct BurnDriver BurnDrvsg1k_hypersp2 = {
	"sg1k_hypersp2", NULL, NULL, NULL, "198?",
	"Hyper Sports 2 (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_hypersp2RomInfo, sg1k_hypersp2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 007 James Bond (Japan, v2.7, OMV)

static struct BurnRomInfo sg1k_jb007RomDesc[] = {
	{ "james bond 007 (japan) (v2.7) (om).bin",	0x04000, 0x90160849, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007)
STD_ROM_FN(sg1k_jb007)

struct BurnDriver BurnDrvsg1k_jb007 = {
	"sg1k_jb007", NULL, NULL, NULL, "1984",
	"007 James Bond (Japan, v2.7, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_jb007RomInfo, sg1k_jb007RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 007 James Bond (Japan, v2.6, OMV)

static struct BurnRomInfo sg1k_jb007aRomDesc[] = {
	{ "007 james bond [v2.6] [40k map].bin",	0x0a000, 0xa8b5b57f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007a)
STD_ROM_FN(sg1k_jb007a)

struct BurnDriver BurnDrvsg1k_jb007a = {
	"sg1k_jb007a", "sg1k_jb007", NULL, NULL, "1984",
	"007 James Bond (Japan, v2.6, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_jb007aRomInfo, sg1k_jb007aRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 007 James Bond (Taiwan)

static struct BurnRomInfo sg1k_jb007tRomDesc[] = {
	{ "james bond 007 (tw).bin",	0x04000, 0x76d6c64d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007t)
STD_ROM_FN(sg1k_jb007t)

struct BurnDriver BurnDrvsg1k_jb007t = {
	"sg1k_jb007t", "sg1k_jb007", NULL, NULL, "1984?",
	"007 James Bond (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_jb007tRomInfo, sg1k_jb007tRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jinzita (Taiwan)

static struct BurnRomInfo sg1k_jinzitaRomDesc[] = {
	{ "king's valley (tw).bin",	0x08000, 0x223397a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jinzita)
STD_ROM_FN(sg1k_jinzita)

struct BurnDriver BurnDrvsg1k_jinzita = {
	"sg1k_jinzita", NULL, NULL, NULL, "198?",
	"King's Valley, Jinzita (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_A, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_jinzitaRomInfo, sg1k_jinzitaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Lode Runner (Euro, Japan)

static struct BurnRomInfo sg1k_ldrunRomDesc[] = {
	{ "mpr-5998.ic1",	0x08000, 0x00ed3970, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrun)
STD_ROM_FN(sg1k_ldrun)

struct BurnDriver BurnDrvsg1k_ldrun = {
	"sg1k_ldrun", NULL, NULL, NULL, "1984",
	"Lode Runner (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_ldrunRomInfo, sg1k_ldrunRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Lode Runner (Korea)

static struct BurnRomInfo sg1k_ldrunkRomDesc[] = {
	{ "lode runner (kr).bin",	0x08000, 0xe68bc7d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrunk)
STD_ROM_FN(sg1k_ldrunk)

struct BurnDriver BurnDrvsg1k_ldrunk = {
	"sg1k_ldrunk", "sg1k_ldrun", NULL, NULL, "1984?",
	"Lode Runner (Korea)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_ldrunkRomInfo, sg1k_ldrunkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Lode Runner (Taiwan)

static struct BurnRomInfo sg1k_ldruntRomDesc[] = {
	{ "lode runner (tw).bin",	0x08000, 0xd953bdb7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrunt)
STD_ROM_FN(sg1k_ldrunt)

struct BurnDriver BurnDrvsg1k_ldrunt = {
	"sg1k_ldrunt", "sg1k_ldrun", NULL, NULL, "1984?",
	"Lode Runner (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_ldruntRomInfo, sg1k_ldruntRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// SG-1000 M2 Check Program

static struct BurnRomInfo sg1k_m2cpRomDesc[] = {
	{ "sg-1000 m2 check program.bin",	0x02000, 0x207e7e99, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_m2cp)
STD_ROM_FN(sg1k_m2cp)

struct BurnDriver BurnDrvsg1k_m2cp = {
	"sg1k_m2cp", NULL, NULL, NULL, "1984",
	"SG-1000 M2 Check Program\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_m2cpRomInfo, sg1k_m2cpRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Magical Tree (Taiwan)

static struct BurnRomInfo sg1k_magtreeRomDesc[] = {
	{ "magical tree (tw).bin",	0x08000, 0xb3a8291a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_magtree)
STD_ROM_FN(sg1k_magtree)

struct BurnDriver BurnDrvsg1k_magtree = {
	"sg1k_magtree", NULL, NULL, NULL, "198?",
	"Magical Tree (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_magtreeRomInfo, sg1k_magtreeRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Magical Kid Wiz (Taiwan)

static struct BurnRomInfo sg1k_mkidwizRomDesc[] = {
	{ "magical kid wiz (tw).bin",	0x0c000, 0xffc4ee3f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mkidwiz)
STD_ROM_FN(sg1k_mkidwiz)

struct BurnDriver BurnDrvsg1k_mkidwiz = {
	"sg1k_mkidwiz", NULL, NULL, NULL, "198?",
	"Magical Kid Wiz (Taiwan)\0", "Stuck at boot logo?", "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_mkidwizRomInfo, sg1k_mkidwizRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mahjong (Japan)

static struct BurnRomInfo sg1k_mahjongRomDesc[] = {
	{ "mahjong (japan).bin",	0x06000, 0x6d909857, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjong)
STD_ROM_FN(sg1k_mahjong)

struct BurnDriver BurnDrvsg1k_mahjong = {
	"sg1k_mahjong", NULL, NULL, NULL, "1983",
	"Mahjong (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_mahjongRomInfo, sg1k_mahjongRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mahjong (Japan, Alt)

static struct BurnRomInfo sg1k_mahjongaRomDesc[] = {
	{ "mahjong [big 1983].bin",	0x06000, 0x1c137cab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjonga)
STD_ROM_FN(sg1k_mahjonga)

struct BurnDriver BurnDrvsg1k_mahjonga = {
	"sg1k_mahjonga", "sg1k_mahjong", NULL, NULL, "1983",
	"Mahjong (Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_mahjongaRomInfo, sg1k_mahjongaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mahjong (Taiwan)

static struct BurnRomInfo sg1k_mahjongtRomDesc[] = {
	{ "mahjong (tw).bin",	0x06000, 0xbc823a89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjongt)
STD_ROM_FN(sg1k_mahjongt)

struct BurnDriver BurnDrvsg1k_mahjongt = {
	"sg1k_mahjongt", "sg1k_mahjong", NULL, NULL, "1983?",
	"Mahjong (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_mahjongtRomInfo, sg1k_mahjongtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Monaco GP (Japan, v2)

static struct BurnRomInfo sg1k_monacogpRomDesc[] = {
	{ "monaco gp [32k] [v2].bin",	0x08000, 0x02e5d66a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogp)
STD_ROM_FN(sg1k_monacogp)

struct BurnDriver BurnDrvsg1k_monacogp = {
	"sg1k_monacogp", NULL, NULL, NULL, "1983",
	"Monaco GP (Japan, v2)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_monacogpRomInfo, sg1k_monacogpRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Monaco GP (Japan, v1)

static struct BurnRomInfo sg1k_monacogpaRomDesc[] = {
	{ "monaco gp [32k].bin",	0x08000, 0xda2d57f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpa)
STD_ROM_FN(sg1k_monacogpa)

struct BurnDriver BurnDrvsg1k_monacogpa = {
	"sg1k_monacogpa", "sg1k_monacogp", NULL, NULL, "1983",
	"Monaco GP (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_monacogpaRomInfo, sg1k_monacogpaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Monaco GP (Japan, v0)

static struct BurnRomInfo sg1k_monacogpbRomDesc[] = {
	{ "monaco gp [24k].bin",	0x0a000, 0x8572d73a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpb)
STD_ROM_FN(sg1k_monacogpb)

struct BurnDriver BurnDrvsg1k_monacogpb = {
	"sg1k_monacogpb", "sg1k_monacogp", NULL, NULL, "1983",
	"Monaco GP (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_monacogpbRomInfo, sg1k_monacogpbRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Monaco GP (Taiwan)

static struct BurnRomInfo sg1k_monacogptRomDesc[] = {
	{ "monaco gp (tw).bin",	0x08000, 0x01cda679, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpt)
STD_ROM_FN(sg1k_monacogpt)

struct BurnDriver BurnDrvsg1k_monacogpt = {
	"sg1k_monacogpt", "sg1k_monacogp", NULL, NULL, "1983?",
	"Monaco GP (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_monacogptRomInfo, sg1k_monacogptRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Knightmare, Moyu Chuanqi (Taiwan)

static struct BurnRomInfo sg1k_moyuchuaRomDesc[] = {
	{ "knightmare [jumbo] (tw).bin",	0x0c000, 0x281d2888, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_moyuchua)
STD_ROM_FN(sg1k_moyuchua)

struct BurnDriver BurnDrvsg1k_moyuchua = {
	"sg1k_moyuchua", NULL, NULL, NULL, "1986?",
	"Knightmare, Moyu Chuanqi (Taiwan)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_A, GBF_RUNGUN | GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_moyuchuaRomInfo, sg1k_moyuchuaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Ninja Princess (Japan)

static struct BurnRomInfo sg1k_ninjapriRomDesc[] = {
	{ "ninja princess (japan).bin",	0x08000, 0x3b912408, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ninjapri)
STD_ROM_FN(sg1k_ninjapri)

struct BurnDriver BurnDrvsg1k_ninjapri = {
	"sg1k_ninjapri", NULL, NULL, NULL, "1986",
	"Ninja Princess (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN, 0,
	SG1KGetZipName, sg1k_ninjapriRomInfo, sg1k_ninjapriRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Ninja Princess (Hack, T-Eng)

static struct BurnRomInfo sg1k_ninjaprieRomDesc[] = {
	{ "Ninja Princess (T-Eng)(2010)(goldenband).bin",	32768, 0xbdb6cf0e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ninjaprie)
STD_ROM_FN(sg1k_ninjaprie)

struct BurnDriver BurnDrvsg1k_ninjaprie = {
	"sg1k_ninjaprie", "sg1k_ninjapri", NULL, NULL, "2010",
	"Ninja Princess (Hack, T-Eng)\0", NULL, "goldenband", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN, 0,
	SG1KGetZipName, sg1k_ninjaprieRomInfo, sg1k_ninjaprieRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Renzhe Gongzhu (Taiwan)

static struct BurnRomInfo sg1k_ninjapritRomDesc[] = {
	{ "ninja princess (tw).bin",	0x08000, 0x464d144b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ninjaprit)
STD_ROM_FN(sg1k_ninjaprit)

struct BurnDriver BurnDrvsg1k_ninjaprit = {
	"sg1k_ninjaprit", "sg1k_ninjapri", NULL, NULL, "1986?",
	"Renzhe Gongzhu (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RUNGUN, 0,
	SG1KGetZipName, sg1k_ninjapritRomInfo, sg1k_ninjapritRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// N-Sub (Euro?)

static struct BurnRomInfo sg1k_nsubRomDesc[] = {
	{ "n-sub (europe) (1988).bin",	0x04000, 0x09196fc5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsub)
STD_ROM_FN(sg1k_nsub)

struct BurnDriver BurnDrvsg1k_nsub = {
	"sg1k_nsub", NULL, NULL, NULL, "1988",
	"N-Sub (Euro?)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_nsubRomInfo, sg1k_nsubRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// N-Sub (Japan)

static struct BurnRomInfo sg1k_nsubaRomDesc[] = {
	{ "n-sub [16k].bin",	0x04000, 0x652bbd1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsuba)
STD_ROM_FN(sg1k_nsuba)

struct BurnDriver BurnDrvsg1k_nsuba = {
	"sg1k_nsuba", "sg1k_nsub", NULL, NULL, "1983",
	"N-Sub (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_nsubaRomInfo, sg1k_nsubaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// N-Sub (Japan, Alt)

static struct BurnRomInfo sg1k_nsubbRomDesc[] = {
	{ "n-sub [40k].bin",	0x0a000, 0xb377d6e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsubb)
STD_ROM_FN(sg1k_nsubb)

struct BurnDriver BurnDrvsg1k_nsubb = {
	"sg1k_nsubb", "sg1k_nsub", NULL, NULL, "1983",
	"N-Sub (Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_nsubbRomInfo, sg1k_nsubbRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// N-Sub (Taiwan)

static struct BurnRomInfo sg1k_nsubtRomDesc[] = {
	{ "n-sub (tw).bin",	0x04000, 0x3e371769, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsubt)
STD_ROM_FN(sg1k_nsubt)

struct BurnDriver BurnDrvsg1k_nsubt = {
	"sg1k_nsubt", "sg1k_nsub", NULL, NULL, "1983?",
	"N-Sub (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_nsubtRomInfo, sg1k_nsubtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Okamoto Ayako no Match Play Golf (Japan, OMV)

static struct BurnRomInfo sg1k_matchpgRomDesc[] = {
	{ "okamoto ayako no match play golf [b] (jp).bin",	0x08000, 0x49d3db2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpg)
STD_ROM_FN(sg1k_matchpg)

struct BurnDriver BurnDrvsg1k_matchpg = {
	"sg1k_matchpg", NULL, NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_matchpgRomInfo, sg1k_matchpgRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Okamoto Ayako no Match Play Golf (Japan, OMV, Alt)

static struct BurnRomInfo sg1k_matchpgaRomDesc[] = {
	{ "okamoto ayako no match play golf [a] (jp).bin",	0x08000, 0x547dd7fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpga)
STD_ROM_FN(sg1k_matchpga)

struct BurnDriver BurnDrvsg1k_matchpga = {
	"sg1k_matchpga", "sg1k_matchpg", NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Japan, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_matchpgaRomInfo, sg1k_matchpgaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Okamoto Ayako no Match Play Golf (Taiwan)

static struct BurnRomInfo sg1k_matchpgtRomDesc[] = {
	{ "okamoto ayako no match play golf (tw).bin",	0x08000, 0xb60492d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpgt)
STD_ROM_FN(sg1k_matchpgt)

struct BurnDriver BurnDrvsg1k_matchpgt = {
	"sg1k_matchpgt", "sg1k_matchpg", NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SPORTSMISC, 0,
	SG1KGetZipName, sg1k_matchpgtRomInfo, sg1k_matchpgtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Orguss (Euro, Japan)

static struct BurnRomInfo sg1k_orgussRomDesc[] = {
	{ "mpr-5743.ic1",	0x08000, 0xf4f78b76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_orguss)
STD_ROM_FN(sg1k_orguss)

struct BurnDriver BurnDrvsg1k_orguss = {
	"sg1k_orguss", NULL, NULL, NULL, "1984",
	"Orguss (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_orgussRomInfo, sg1k_orgussRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Othello (Japan)

static struct BurnRomInfo sg1k_othelloRomDesc[] = {
	{ "othello (japan).bin",	0x08000, 0xaf4f14bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_othello)
STD_ROM_FN(sg1k_othello)

struct BurnDriver BurnDrvsg1k_othello = {
	"sg1k_othello", NULL, NULL, NULL, "1985",
	"Othello (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_2K, GBF_BOARD, 0,
	SG1KGetZipName, sg1k_othelloRomInfo, sg1k_othelloRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Hei Bai Qi (Taiwan)

static struct BurnRomInfo sg1k_heibaiqiRomDesc[] = {
	{ "othello (tw).bin",	0x08000, 0x1d1a0ca3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_heibaiqi)
STD_ROM_FN(sg1k_heibaiqi)

struct BurnDriver BurnDrvsg1k_heibaiqi = {
	"sg1k_heibaiqi", "sg1k_othello", NULL, NULL, "1985?",
	"Hei Bai Qi (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_2K, GBF_BOARD, 0,
	SG1KGetZipName, sg1k_heibaiqiRomInfo, sg1k_heibaiqiRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pacar (Euro, Japan)

static struct BurnRomInfo sg1k_pacarRomDesc[] = {
	{ "mpr-5556.ic1",	0x04000, 0x30c52e5e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacar)
STD_ROM_FN(sg1k_pacar)

struct BurnDriver BurnDrvsg1k_pacar = {
	"sg1k_pacar", NULL, NULL, NULL, "1983",
	"Pacar (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_pacarRomInfo, sg1k_pacarRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pacar (Japan, Alt)

static struct BurnRomInfo sg1k_pacaraRomDesc[] = {
	{ "pacar [40k map].bin",	0x0a000, 0x19949375, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacara)
STD_ROM_FN(sg1k_pacara)

struct BurnDriver BurnDrvsg1k_pacara = {
	"sg1k_pacara", "sg1k_pacar", NULL, NULL, "1983",
	"Pacar (Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_pacaraRomInfo, sg1k_pacaraRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pacar (Taiwan)

static struct BurnRomInfo sg1k_pacartRomDesc[] = {
	{ "pacar (tw).bin",	0x04000, 0xdd6817a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacart)
STD_ROM_FN(sg1k_pacart)

struct BurnDriver BurnDrvsg1k_pacart = {
	"sg1k_pacart", "sg1k_pacar", NULL, NULL, "1983?",
	"Pacar (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_pacartRomInfo, sg1k_pacartRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pachinko II (Japan)

static struct BurnRomInfo sg1k_pachink2RomDesc[] = {
	{ "pachinko ii (japan).bin",	0x04000, 0xfd7cb50a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachink2)
STD_ROM_FN(sg1k_pachink2)

struct BurnDriver BurnDrvsg1k_pachink2 = {
	"sg1k_pachink2", NULL, NULL, NULL, "1984",
	"Pachinko II (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachink2RomInfo, sg1k_pachink2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pachinko II (Taiwan)

static struct BurnRomInfo sg1k_pachink2tRomDesc[] = {
	{ "pachinko ii (tw).bin",	0x04000, 0x6ebe81bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachink2t)
STD_ROM_FN(sg1k_pachink2t)

struct BurnDriver BurnDrvsg1k_pachink2t = {
	"sg1k_pachink2t", "sg1k_pachink2", NULL, NULL, "1984?",
	"Pachinko II (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachink2tRomInfo, sg1k_pachink2tRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pachinko (Japan)

static struct BurnRomInfo sg1k_pachinkoRomDesc[] = {
	{ "pachinko (japan).bin",	0x0a000, 0x326587e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachinko)
STD_ROM_FN(sg1k_pachinko)

struct BurnDriver BurnDrvsg1k_pachinko = {
	"sg1k_pachinko", NULL, NULL, NULL, "1983",
	"Pachinko (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachinkoRomInfo, sg1k_pachinkoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Ping Pong (Taiwan)

static struct BurnRomInfo sg1k_pingpongRomDesc[] = {
	{ "ping pong (konami's) (tw).bin",	0x08000, 0x4d972a9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pingpong)
STD_ROM_FN(sg1k_pingpong)

struct BurnDriver BurnDrvsg1k_pingpong = {
	"sg1k_pingpong", NULL, NULL, NULL, "1985?",
	"Ping Pong (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_BALLPADDLE, 0,
	SG1KGetZipName, sg1k_pingpongRomInfo, sg1k_pingpongRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitfall II - The Lost Caverns (Japan, v0)

static struct BurnRomInfo sg1k_pitfall2aRomDesc[] = {
	{ "pitfall ii - the lost caverns (japan).bin",	0x08000, 0x37fca2eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pitfall2a)
STD_ROM_FN(sg1k_pitfall2a)

struct BurnDriver BurnDrvsg1k_pitfall2a = {
	"sg1k_pitfall2a", "sg1k_pitfall2", NULL, NULL, "1985",
	"Pitfall II - The Lost Caverns (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_pitfall2aRomInfo, sg1k_pitfall2aRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitfall II - The Lost Caverns (Japan, v1)

static struct BurnRomInfo sg1k_pitfall2RomDesc[] = {
	{ "pitfall ii - the lost caverns (japan) (alt).bin",	0x08000, 0x3db74761, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pitfall2)
STD_ROM_FN(sg1k_pitfall2)

struct BurnDriver BurnDrvsg1k_pitfall2 = {
	"sg1k_pitfall2", NULL, NULL, NULL, "1985",
	"Pitfall II - The Lost Caverns (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_pitfall2RomInfo, sg1k_pitfall2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fa Gui Qibing (Taiwan)

static struct BurnRomInfo sg1k_faguiqibRomDesc[] = {
	{ "pitfall ii [v0] [chinese logo] (tw).bin",	0x08000, 0x476a079b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_faguiqib)
STD_ROM_FN(sg1k_faguiqib)

struct BurnDriver BurnDrvsg1k_faguiqib = {
	"sg1k_faguiqib", "sg1k_pitfall2", NULL, NULL, "1985?",
	"Fa Gui Qibing (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_faguiqibRomInfo, sg1k_faguiqibRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fa Gui Qibing (Taiwan, Alt)

static struct BurnRomInfo sg1k_faguiqibaRomDesc[] = {
	{ "pitfall ii [v0] [english logo] (tw).bin",	0x08000, 0x4e93bc8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_faguiqiba)
STD_ROM_FN(sg1k_faguiqiba)

struct BurnDriver BurnDrvsg1k_faguiqiba = {
	"sg1k_faguiqiba", "sg1k_pitfall2", NULL, NULL, "1985?",
	"Fa Gui Qibing (Taiwan, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_faguiqibaRomInfo, sg1k_faguiqibaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pop Flamer (Euro, Japan)

static struct BurnRomInfo sg1k_popflameRomDesc[] = {
	{ "pop flamer (japan, europe).bin",	0x04000, 0xdb6404ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_popflame)
STD_ROM_FN(sg1k_popflame)

struct BurnDriver BurnDrvsg1k_popflame = {
	"sg1k_popflame", NULL, NULL, NULL, "1983",
	"Pop Flamer (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_popflameRomInfo, sg1k_popflameRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pop Flamer (Taiwan)

static struct BurnRomInfo sg1k_popflametRomDesc[] = {
	{ "pop flamer (tw).bin",	0x04000, 0xab1da8a6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_popflamet)
STD_ROM_FN(sg1k_popflamet)

struct BurnDriver BurnDrvsg1k_popflamet = {
	"sg1k_popflamet", "sg1k_popflame", NULL, NULL, "1983?",
	"Pop Flamer (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_popflametRomInfo, sg1k_popflametRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Q*bert (Japan, OMV)

static struct BurnRomInfo sg1k_qbertRomDesc[] = {
	{ "q-bert (japan) (om).bin",	0x02000, 0x77db4704, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_qbert)
STD_ROM_FN(sg1k_qbert)

struct BurnDriver BurnDrvsg1k_qbert = {
	"sg1k_qbert", NULL, NULL, NULL, "1983",
	"Q*bert (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_qbertRomInfo, sg1k_qbertRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rally-X (Taiwan)

static struct BurnRomInfo sg1k_rallyxRomDesc[] = {
	{ "rally-x (tw).bin",	0x08000, 0xaaac12cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_rallyx)
STD_ROM_FN(sg1k_rallyx)

struct BurnDriver BurnDrvsg1k_rallyx = {
	"sg1k_rallyx", NULL, NULL, NULL, "1986?",
	"Rally-X (Taiwan)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_rallyxRomInfo, sg1k_rallyxRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Road Fighter (Taiwan)

static struct BurnRomInfo sg1k_roadfghtRomDesc[] = {
	{ "road fighter (tw).bin",	0x08000, 0xd2edd329, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_roadfght)
STD_ROM_FN(sg1k_roadfght)

struct BurnDriver BurnDrvsg1k_roadfght = {
	"sg1k_roadfght", NULL, NULL, NULL, "1986?",
	"Road Fighter (Taiwan)\0", NULL, "Jumbo?", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_RACING, 0,
	SG1KGetZipName, sg1k_roadfghtRomInfo, sg1k_roadfghtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Huojian Che (Taiwan)

static struct BurnRomInfo sg1k_huojicheRomDesc[] = {
	{ "road fighter [jumbo] (tw).bin",	0x08000, 0x29e047cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_huojiche)
STD_ROM_FN(sg1k_huojiche)

struct BurnDriver BurnDrvsg1k_huojiche = {
	"sg1k_huojiche", "sg1k_roadfght", NULL, NULL, "1986?",
	"Huojian Che (Taiwan)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_B, GBF_RACING, 0,
	SG1KGetZipName, sg1k_huojicheRomInfo, sg1k_huojicheRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rock n' Bolt (Japan)

static struct BurnRomInfo sg1k_rocknbolRomDesc[] = {
	{ "rock n' bolt (japan).bin",	0x08000, 0x0ffdd03d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_rocknbol)
STD_ROM_FN(sg1k_rocknbol)

struct BurnDriver BurnDrvsg1k_rocknbol = {
	"sg1k_rocknbol", NULL, NULL, NULL, "1985",
	"Rock n' Bolt (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_rocknbolRomInfo, sg1k_rocknbolRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mo Tian Da Lou (Taiwan)

static struct BurnRomInfo sg1k_motiandaRomDesc[] = {
	{ "rock n' bolt (tw).bin",	0x08000, 0x4eacb981, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_motianda)
STD_ROM_FN(sg1k_motianda)

struct BurnDriver BurnDrvsg1k_motianda = {
	"sg1k_motianda", "sg1k_rocknbol", NULL, NULL, "1985?",
	"Mo Tian Da Lou (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_motiandaRomInfo, sg1k_motiandaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Safari Hunting (Japan)
static struct BurnRomInfo sg1k_safarihuRomDesc[] = {
	{ "Safari Hunting (Japan)(1983)(Sega).sg",	0x04000, 0x49e9718b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarihu)
STD_ROM_FN(sg1k_safarihu)

struct BurnDriver BurnDrvsg1k_safarihu = {
	"sg1k_safarihu", NULL, NULL, NULL, "1983",
	"Safari Hunting (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_safarihuRomInfo, sg1k_safarihuRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Safari Hunting (Taiwan)
static struct BurnRomInfo sg1k_safarihutRomDesc[] = {
	{ "Safari Hunting (Taiwan)(198x)(Aaronix).sg",	0x04000, 0x6dc51c01, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarihut)
STD_ROM_FN(sg1k_safarihut)

struct BurnDriver BurnDrvsg1k_safarihut = {
	"sg1k_safarihut", "sg1k_safarihu", NULL, NULL, "198?",
	"Safari Hunting (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_safarihutRomInfo, sg1k_safarihutRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Safari Race (Euro)
static struct BurnRomInfo sg1k_safarircRomDesc[] = {
	{ "Safari Race (Euro)(1988)(Sega).sg",	0x08000, 0x619dd066, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarirc)
STD_ROM_FN(sg1k_safarirc)

struct BurnDriver BurnDrvsg1k_safarirc = {
	"sg1k_safarirc", NULL, NULL, NULL, "1988",
	"Safari Race (Euro)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_safarircRomInfo, sg1k_safarircRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Safari Race (Japan)
static struct BurnRomInfo sg1k_safarircjRomDesc[] = {
	{ "Safari Race (Japan)(1984)(Sega).sg",	0x08000, 0x08707fe3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarircj)
STD_ROM_FN(sg1k_safarircj)

struct BurnDriver BurnDrvsg1k_safarircj = {
	"sg1k_safarircj", "sg1k_safarirc", NULL, NULL, "1984",
	"Safari Race (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_safarircjRomInfo, sg1k_safarircjRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Safari Race (Taiwan)
static struct BurnRomInfo sg1k_safarirctRomDesc[] = {
	{ "Safari Race (Taiwan)(198x)(Aaronix).sg",	0x08000, 0xb2724428, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarirct)
STD_ROM_FN(sg1k_safarirct)

struct BurnDriver BurnDrvsg1k_safarirct = {
	"sg1k_safarirct", "sg1k_safarirc", NULL, NULL, "198?",
	"Safari Race (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_safarirctRomInfo, sg1k_safarirctRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// San-nin Mahjong (Japan, OMV)
static struct BurnRomInfo sg1k_3ninmjRomDesc[] = {
	{ "San-nin Mahjong (Japan, OMV)(1984)(Tsukuda Original).sg",	0x04000, 0x885fa64d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_3ninmj)
STD_ROM_FN(sg1k_3ninmj)

struct BurnDriver BurnDrvsg1k_3ninmj = {
	"sg1k_3ninmj", NULL, NULL, NULL, "1984",
	"San-nin Mahjong (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_3ninmjRomInfo, sg1k_3ninmjRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// San-nin Mahjong (Taiwan)
static struct BurnRomInfo sg1k_3ninmjtRomDesc[] = {
	{ "San-nin Mahjong (Taiwan)(198x)(Aaronix).sg",	0x04000, 0x6fd17655, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_3ninmjt)
STD_ROM_FN(sg1k_3ninmjt)

struct BurnDriver BurnDrvsg1k_3ninmjt = {
	"sg1k_3ninmjt", "sg1k_3ninmj", NULL, NULL, "198?",
	"San-nin Mahjong (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_MAHJONG, 0,
	SG1KGetZipName, sg1k_3ninmjtRomInfo, sg1k_3ninmjtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sega Flipper (Euro, Japan)
static struct BurnRomInfo sg1k_segaflipRomDesc[] = {
	{ "Sega Flipper (Euro, Japan)(1983)(Sega).sg",	0x04000, 0x8efc77bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflip)
STD_ROM_FN(sg1k_segaflip)

struct BurnDriver BurnDrvsg1k_segaflip = {
	"sg1k_segaflip", NULL, NULL, NULL, "1983",
	"Sega Flipper (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PINBALL, 0,
	SG1KGetZipName, sg1k_segaflipRomInfo, sg1k_segaflipRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sega Flipper (Japan, Alt)
static struct BurnRomInfo sg1k_segaflipaRomDesc[] = {
	{ "Sega Flipper (Japan, Alt)(1983)(Sega).sg",	0x0a000, 0xfd76ad99, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflipa)
STD_ROM_FN(sg1k_segaflipa)

struct BurnDriver BurnDrvsg1k_segaflipa = {
	"sg1k_segaflipa", "sg1k_segaflip", NULL, NULL, "1983",
	"Sega Flipper (Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PINBALL, 0,
	SG1KGetZipName, sg1k_segaflipaRomInfo, sg1k_segaflipaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Flipper (Taiwan)
static struct BurnRomInfo sg1k_segafliptRomDesc[] = {
	{ "Flipper (Taiwan)(198x)(Aaronix).sg",	0x04000, 0x042c36ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflipt)
STD_ROM_FN(sg1k_segaflipt)

struct BurnDriver BurnDrvsg1k_segaflipt = {
	"sg1k_segaflipt", "sg1k_segaflip", NULL, NULL, "1984?",
	"Flipper (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PINBALL, 0,
	SG1KGetZipName, sg1k_segafliptRomInfo, sg1k_segafliptRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sega-Galaga (Japan)
static struct BurnRomInfo sg1k_segagalaRomDesc[] = {
	{ "Sega-Galaga (Japan)(1983)(Sega).sg",	0x04000, 0x981e36c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segagala)
STD_ROM_FN(sg1k_segagala)

struct BurnDriver BurnDrvsg1k_segagala = {
	"sg1k_segagala", NULL, NULL, NULL, "1983",
	"Sega-Galaga (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_segagalaRomInfo, sg1k_segagalaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sega-Galaga (Japan, Alt)
static struct BurnRomInfo sg1k_segagala1RomDesc[] = {
	{ "Sega-Galaga (Japan, Alt)(1983)(Sega).sg",	0x0a000, 0x31283003, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segagala1)
STD_ROM_FN(sg1k_segagala1)

struct BurnDriver BurnDrvsg1k_segagala1 = {
	"sg1k_segagala1", "sg1k_segagala", NULL, NULL, "1983",
	"Sega-Galaga (Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_segagala1RomInfo, sg1k_segagala1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Galaga (Taiwan)
static struct BurnRomInfo sg1k_galagaRomDesc[] = {
	{ "Galaga (Taiwan)(198x)(Aaronix).sg",	0x04000, 0x845bbb22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_galaga)
STD_ROM_FN(sg1k_galaga)

struct BurnDriver BurnDrvsg1k_galaga = {
	"sg1k_galaga", "sg1k_segagala", NULL, NULL, "198?",
	"Galaga (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_galagaRomInfo, sg1k_galagaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Serizawa Hachidan no Tsumeshogi (Japan)

static struct BurnRomInfo sg1k_serizawaRomDesc[] = {
	{ "serizawa hachidan no tsumeshogi (japan).bin",	0x04000, 0x545fc9bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_serizawa)
STD_ROM_FN(sg1k_serizawa)

struct BurnDriver BurnDrvsg1k_serizawa = {
	"sg1k_serizawa", NULL, NULL, NULL, "1983",
	"Serizawa Hachidan no Tsumeshogi (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_BOARD, 0,
	SG1KGetZipName, sg1k_serizawaRomInfo, sg1k_serizawaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Shinnyushain Tooru Kun (Japan)

static struct BurnRomInfo sg1k_shinnyusRomDesc[] = {
	{ "shinnyushain tooru kun (japan).bin",	0x08000, 0x5a917e06, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_shinnyus)
STD_ROM_FN(sg1k_shinnyus)

struct BurnDriver BurnDrvsg1k_shinnyus = {
	"sg1k_shinnyus", NULL, NULL, NULL, "1985",
	"Shinnyushain Tooru Kun (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_shinnyusRomInfo, sg1k_shinnyusRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Shinnyushain Tooru Kun (Taiwan)

static struct BurnRomInfo sg1k_shinnyustRomDesc[] = {
	{ "shinnyushain tooru kun (tw).bin",	0x08000, 0x33fc5cf6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_shinnyust)
STD_ROM_FN(sg1k_shinnyust)

struct BurnDriver BurnDrvsg1k_shinnyust = {
	"sg1k_shinnyust", "sg1k_shinnyus", NULL, NULL, "19??",
	"Shinnyushain Tooru Kun (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_shinnyustRomInfo, sg1k_shinnyustRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Shuangxiang Pao (Taiwan)

static struct BurnRomInfo sg1k_sxpaoRomDesc[] = {
	{ "twinbee [jumbo] (tw).bin",	0x0c000, 0xc550b4f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sxpao)
STD_ROM_FN(sg1k_sxpao)

struct BurnDriver BurnDrvsg1k_sxpao = {
	"sg1k_sxpao", NULL, NULL, NULL, "1986?",
	"Twinbee (Taiwan)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000 | HARDWARE_SEGA_SG1000_RAMEXP_A, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_sxpaoRomInfo, sg1k_sxpaoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sindbad Mystery (Euro, Japan)

static struct BurnRomInfo sg1k_sindbadmRomDesc[] = {
	{ "mpr-5546.ic1",	0x08000, 0x01932df9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sindbadm)
STD_ROM_FN(sg1k_sindbadm)

struct BurnDriver BurnDrvsg1k_sindbadm = {
	"sg1k_sindbadm", NULL, NULL, NULL, "1983",
	"Sindbad Mystery (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_sindbadmRomInfo, sg1k_sindbadmRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sindbad Mystery (Taiwan)

static struct BurnRomInfo sg1k_sindbadmtRomDesc[] = {
	{ "sindbad mystery (tw).bin",	0x08000, 0x945f7847, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sindbadmt)
STD_ROM_FN(sg1k_sindbadmt)

struct BurnDriver BurnDrvsg1k_sindbadmt = {
	"sg1k_sindbadmt", "sg1k_sindbadm", NULL, NULL, "1983?",
	"Sindbad Mystery (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_sindbadmtRomInfo, sg1k_sindbadmtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Soukoban (Japan)

static struct BurnRomInfo sg1k_sokobanRomDesc[] = {
	{ "soukoban (japan).bin",	0x08000, 0x922c5468, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokoban)
STD_ROM_FN(sg1k_sokoban)

struct BurnDriver BurnDrvsg1k_sokoban = {
	"sg1k_sokoban", NULL, NULL, NULL, "1985",
	"Soukoban (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_sokobanRomInfo, sg1k_sokobanRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Soukoban (Taiwan)

static struct BurnRomInfo sg1k_sokobantRomDesc[] = {
	{ "soukoban (tw).bin",	0x08000, 0xa2c3fc97, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokobant)
STD_ROM_FN(sg1k_sokobant)

struct BurnDriver BurnDrvsg1k_sokobant = {
	"sg1k_sokobant", "sg1k_sokoban", NULL, NULL, "1985?",
	"Soukoban (Taiwan)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_sokobantRomInfo, sg1k_sokobantRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Soukoban (Korea)

static struct BurnRomInfo sg1k_sokobankRomDesc[] = {
	{ "soukoban (kr).bin",	0x08000, 0x148ea752, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokobank)
STD_ROM_FN(sg1k_sokobank)

struct BurnDriver BurnDrvsg1k_sokobank = {
	"sg1k_sokobank", "sg1k_sokoban", NULL, NULL, "1985?",
	"Soukoban (Korea)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_sokobankRomInfo, sg1k_sokobankRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// I.Q. (Korea)

static struct BurnRomInfo sg1k_iqRomDesc[] = {
	{ "soukoban [iq logo] (kr).bin",	0x08000, 0x89bafec5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_iq)
STD_ROM_FN(sg1k_iq)

struct BurnDriver BurnDrvsg1k_iq = {
	"sg1k_iq", "sg1k_sokoban", NULL, NULL, "1985?",
	"I.Q. (Korea)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_iqRomInfo, sg1k_iqRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Armor (Japan, v20, OMV)

static struct BurnRomInfo sg1k_spacearmRomDesc[] = {
	{ "space armor [v2].bin",	0x04000, 0xac4f0a5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearm)
STD_ROM_FN(sg1k_spacearm)

struct BurnDriver BurnDrvsg1k_spacearm = {
	"sg1k_spacearm", NULL, NULL, NULL, "1984",
	"Space Armor (Japan, v20, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_spacearmRomInfo, sg1k_spacearmRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Armor (Japan, v20, OMV, Alt)

static struct BurnRomInfo sg1k_spacearmaRomDesc[] = {
	{ "space armor [v1].bin",	0x04000, 0xd5fdb4a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearma)
STD_ROM_FN(sg1k_spacearma)

struct BurnDriver BurnDrvsg1k_spacearma = {
	"sg1k_spacearma", "sg1k_spacearm", NULL, NULL, "1984",
	"Space Armor (Japan, v20, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_spacearmaRomInfo, sg1k_spacearmaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Armor (Japan, v10, OMV)

static struct BurnRomInfo sg1k_spacearmbRomDesc[] = {
	{ "space armor [v0] [40k map].bin",	0x0a000, 0xd23b0e3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearmb)
STD_ROM_FN(sg1k_spacearmb)

struct BurnDriver BurnDrvsg1k_spacearmb = {
	"sg1k_spacearmb", "sg1k_spacearm", NULL, NULL, "1984",
	"Space Armor (Japan, v10, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_spacearmbRomInfo, sg1k_spacearmbRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Invaders (Japan)

static struct BurnRomInfo sg1k_spaceinvRomDesc[] = {
	{ "space invaders (japan).bin",	0x04000, 0x6ad5cb3d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spaceinv)
STD_ROM_FN(sg1k_spaceinv)

struct BurnDriver BurnDrvsg1k_spaceinv = {
	"sg1k_spaceinv", NULL, NULL, NULL, "1985",
	"Space Invaders (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_spaceinvRomInfo, sg1k_spaceinvRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Invaders (Taiwan)

static struct BurnRomInfo sg1k_spaceinvtRomDesc[] = {
	{ "space invaders (tw).bin",	0x04000, 0x0760ea93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spaceinvt)
STD_ROM_FN(sg1k_spaceinvt)

struct BurnDriver BurnDrvsg1k_spaceinvt = {
	"sg1k_spaceinvt", "sg1k_spaceinv", NULL, NULL, "1985?",
	"Space Invaders (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_spaceinvtRomInfo, sg1k_spaceinvtRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Mountain (Japan, OMV)

static struct BurnRomInfo sg1k_spacemntRomDesc[] = {
	{ "space mountain (japan) (om).bin",	0x02000, 0xbbd87d8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacemnt)
STD_ROM_FN(sg1k_spacemnt)

struct BurnDriver BurnDrvsg1k_spacemnt = {
	"sg1k_spacemnt", NULL, NULL, NULL, "1983",
	"Space Mountain (Japan, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_spacemntRomInfo, sg1k_spacemntRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Slalom (Japan)

static struct BurnRomInfo sg1k_spaceslaRomDesc[] = {
	{ "space slalom (japan).bin",	0x02000, 0xb8b58b30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacesla)
STD_ROM_FN(sg1k_spacesla)

struct BurnDriver BurnDrvsg1k_spacesla = {
	"sg1k_spacesla", NULL, NULL, NULL, "1983",
	"Space Slalom (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_spaceslaRomInfo, sg1k_spaceslaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Force (Japan)

static struct BurnRomInfo sg1k_starfrceRomDesc[] = {
	{ "star force (japan).bin",	0x08000, 0xb846b52a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrce)
STD_ROM_FN(sg1k_starfrce)

struct BurnDriver BurnDrvsg1k_starfrce = {
	"sg1k_starfrce", NULL, NULL, NULL, "1985",
	"Star Force (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starfrceRomInfo, sg1k_starfrceRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Force (Taiwan)

static struct BurnRomInfo sg1k_starfrcetRomDesc[] = {
	{ "star force (tw).bin",	0x08000, 0x1f736931, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrcet)
STD_ROM_FN(sg1k_starfrcet)

struct BurnDriver BurnDrvsg1k_starfrcet = {
	"sg1k_starfrcet", "sg1k_starfrce", NULL, NULL, "1985?",
	"Star Force (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starfrcetRomInfo, sg1k_starfrcetRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Force (Taiwan, Alt)

static struct BurnRomInfo sg1k_starfrcetaRomDesc[] = {
	{ "star force [b] (tw).bin",	0x08000, 0x6f9b1ccd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrceta)
STD_ROM_FN(sg1k_starfrceta)

struct BurnDriver BurnDrvsg1k_starfrceta = {
	"sg1k_starfrceta", "sg1k_starfrce", NULL, NULL, "1985?",
	"Star Force (Taiwan, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starfrcetaRomInfo, sg1k_starfrcetaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Jacker (Euro, Japan, v1.2)
static struct BurnRomInfo sg1k_starjackRomDesc[] = {
	{ "Star Jacker (Euro, Japan, v1.2)(1983)(Sega).sg",	0x08000, 0x3fe59505, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack)
STD_ROM_FN(sg1k_starjack)

struct BurnDriver BurnDrvsg1k_starjack = {
	"sg1k_starjack", NULL, NULL, NULL, "1983",
	"Star Jacker (Euro, Japan, v1.2)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starjackRomInfo, sg1k_starjackRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Jacker (Euro, Japan, v1.1)
static struct BurnRomInfo sg1k_starjack1RomDesc[] = {
	{ "Star Jacker (Euro, Japan, v1.1)(1983)(Sega).sg",	0x08000, 0x7f25deca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack1)
STD_ROM_FN(sg1k_starjack1)

struct BurnDriver BurnDrvsg1k_starjack1 = {
	"sg1k_starjack1", "sg1k_starjack", NULL, NULL, "1983",
	"Star Jacker (Euro, Japan, v1.1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starjack1RomInfo, sg1k_starjack1RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Jacker (Japan, v1.0)
static struct BurnRomInfo sg1k_starjack2RomDesc[] = {
	{ "Star Jacker (Japan, v1.0)(1983)(Sega).sg",	0x08000, 0x1ae94122, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack2)
STD_ROM_FN(sg1k_starjack2)

struct BurnDriver BurnDrvsg1k_starjack2 = {
	"sg1k_starjack2", "sg1k_starjack", NULL, NULL, "1983",
	"Star Jacker (Japan, v1.0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starjack2RomInfo, sg1k_starjack2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Jacker (Taiwan)
static struct BurnRomInfo sg1k_starjacktRomDesc[] = {
	{ "Star Jacker (Taiwan)(198x)(Aaronix.sg",	0x08000, 0xdf162201, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjackt)
STD_ROM_FN(sg1k_starjackt)

struct BurnDriver BurnDrvsg1k_starjackt = {
	"sg1k_starjackt", "sg1k_starjack", NULL, NULL, "198?",
	"Star Jacker (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_starjacktRomInfo, sg1k_starjacktRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Tank (Japan)
static struct BurnRomInfo sg1k_supertnkRomDesc[] = {
	{ "Super Tank (Japan)(1986)(Sega).sg",	0x08000, 0x084cc13e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnk)
STD_ROM_FN(sg1k_supertnk)

struct BurnDriver BurnDrvsg1k_supertnk = {
	"sg1k_supertnk", NULL, NULL, NULL, "1986",
	"Super Tank (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_supertnkRomInfo, sg1k_supertnkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Tank (Korea)
static struct BurnRomInfo sg1k_supertnkkRomDesc[] = {
	{ "Super Tank (Korea)(198x).sg",	0x08000, 0x4c48b7ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnkk)
STD_ROM_FN(sg1k_supertnkk)

struct BurnDriver BurnDrvsg1k_supertnkk = {
	"sg1k_supertnkk", "sg1k_supertnk", NULL, NULL, "1986?",
	"Super Tank (Korea)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_supertnkkRomInfo, sg1k_supertnkkRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Chaoji Tanke (Taiwan)
static struct BurnRomInfo sg1k_supertnktRomDesc[] = {
	{ "Chaoji Tanke (Taiwan)(198x).sg",	0x08000, 0xd0c3df3f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnkt)
STD_ROM_FN(sg1k_supertnkt)

struct BurnDriver BurnDrvsg1k_supertnkt = {
	"sg1k_supertnkt", "sg1k_supertnk", NULL, NULL, "198?",
	"Chaoji Tanke (Taiwan)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_VERSHOOT, 0,
	SG1KGetZipName, sg1k_supertnktRomInfo, sg1k_supertnktRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Wonder Boy (Japan, v1)
static struct BurnRomInfo sg1k_wboyRomDesc[] = {
	{ "Wonder Boy (Japan, v1)(1986)(Sega).sg",	0x08000, 0xe8f0344d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboy)
STD_ROM_FN(sg1k_wboy)

struct BurnDriver BurnDrvsg1k_wboy = {
	"sg1k_wboy", NULL, NULL, NULL, "1986",
	"Wonder Boy (Japan, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_wboyRomInfo, sg1k_wboyRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Wonder Boy (Japan, v0)
static struct BurnRomInfo sg1k_wboyaRomDesc[] = {
	{ "Wonder Boy (Japan, v0)(1986)(Sega).sg",	0x08000, 0x160535c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboya)
STD_ROM_FN(sg1k_wboya)

struct BurnDriver BurnDrvsg1k_wboya = {
	"sg1k_wboya", "sg1k_wboy", NULL, NULL, "1986",
	"Wonder Boy (Japan, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_wboyaRomInfo, sg1k_wboyaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Taotailang (Taiwan)
static struct BurnRomInfo sg1k_wboytRomDesc[] = {
	{ "Taotailang (Taiwan)(198x).sg",	0x08000, 0x953fc2b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboyt)
STD_ROM_FN(sg1k_wboyt)

struct BurnDriver BurnDrvsg1k_wboyt = {
	"sg1k_wboyt", "sg1k_wboy", NULL, NULL, "198?",
	"Taotailang (Taiwan)\0", NULL, "<unknown>", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_wboytRomInfo, sg1k_wboytRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yamato (Euro, Japan)
static struct BurnRomInfo sg1k_yamatoRomDesc[] = {
	{ "Yamato (Euro, Japan)(1983)(Sega).sg",	0x04000, 0xe2fd5201, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamato)
STD_ROM_FN(sg1k_yamato)

struct BurnDriver BurnDrvsg1k_yamato = {
	"sg1k_yamato", NULL, NULL, NULL, "1983",
	"Yamato (Euro, Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_yamatoRomInfo, sg1k_yamatoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yamato (Euro, Japan, Alt)
static struct BurnRomInfo sg1k_yamatoaRomDesc[] = {
	{ "Yamato (Euro, Japan, Alt)(1983)(Sega).sg",	0x0a000, 0x9c7497ff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamatoa)
STD_ROM_FN(sg1k_yamatoa)

struct BurnDriver BurnDrvsg1k_yamatoa = {
	"sg1k_yamatoa", "sg1k_yamato", NULL, NULL, "1983",
	"Yamato (Euro, Japan, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_yamatoaRomInfo, sg1k_yamatoaRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yamato (Taiwan)
static struct BurnRomInfo sg1k_yamatotRomDesc[] = {
	{ "Yamato (Taiwan)(198x)(aaronix).sg",	0x04000, 0xb65a093f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamatot)
STD_ROM_FN(sg1k_yamatot)

struct BurnDriver BurnDrvsg1k_yamatot = {
	"sg1k_yamatot", "sg1k_yamato", NULL, NULL, "198?",
	"Yamato (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_yamatotRomInfo, sg1k_yamatotRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yie Ar Kung-Fu (Taiwan)
static struct BurnRomInfo sg1k_yiearRomDesc[] = {
	{ "Yie Ar Kung-Fu (Taiwan)(198x)(Aaronix).sg",	0x08000, 0xbb0f1930, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yiear)
STD_ROM_FN(sg1k_yiear)

struct BurnDriver BurnDrvsg1k_yiear = {
	"sg1k_yiear", NULL, NULL, NULL, "198?",
	"Yie Ar Kung-Fu (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_VSFIGHT, 0,
	SG1KGetZipName, sg1k_yiearRomInfo, sg1k_yiearRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zaxxon (Japan)
static struct BurnRomInfo sg1k_zaxxonRomDesc[] = {
	{ "Zaxxon (Japan)(1985)(Sega).sg",	0x08000, 0x905467e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zaxxon)
STD_ROM_FN(sg1k_zaxxon)

struct BurnDriver BurnDrvsg1k_zaxxon = {
	"sg1k_zaxxon", NULL, NULL, NULL, "1985",
	"Zaxxon (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_zaxxonRomInfo, sg1k_zaxxonRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zaxxon (Taiwan)
static struct BurnRomInfo sg1k_zaxxontRomDesc[] = {
	{ "Zaxxon (Taiwan)(198x)(Aaronix).sg",	0x08000, 0x49cae925, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zaxxont)
STD_ROM_FN(sg1k_zaxxont)

struct BurnDriver BurnDrvsg1k_zaxxont = {
	"sg1k_zaxxont", "sg1k_zaxxon", NULL, NULL, "198?",
	"Zaxxon (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_zaxxontRomInfo, sg1k_zaxxontRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zippy Race (Japan)
static struct BurnRomInfo sg1k_zippyracRomDesc[] = {
	{ "Zippy Race (Japan)(1983)(Sega).sg",	0x08000, 0xbc5d20df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zippyrac)
STD_ROM_FN(sg1k_zippyrac)

struct BurnDriver BurnDrvsg1k_zippyrac = {
	"sg1k_zippyrac", NULL, NULL, NULL, "1983",
	"Zippy Race (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_zippyracRomInfo, sg1k_zippyracRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zippy Race (Taiwan)
static struct BurnRomInfo sg1k_zippyractRomDesc[] = {
	{ "Zippy Race (Taiwan)(198x)(Aaronix).sg",	0x08000, 0xbcf441a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zippyract)
STD_ROM_FN(sg1k_zippyract)

struct BurnDriver BurnDrvsg1k_zippyract = {
	"sg1k_zippyract", "sg1k_zippyrac", NULL, NULL, "198?",
	"Zippy Race (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_SG1000, GBF_RACING, 0,
	SG1KGetZipName, sg1k_zippyractRomInfo, sg1k_zippyractRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zoom 909 (Japan)
static struct BurnRomInfo sg1k_zoom909RomDesc[] = {
	{ "Zoom 909 (Japan)(1985)(Sega).sg",	0x08000, 0x093830d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zoom909)
STD_ROM_FN(sg1k_zoom909)

struct BurnDriver BurnDrvsg1k_zoom909 = {
	"sg1k_zoom909", NULL, NULL, NULL, "1985",
	"Zoom 909 (Japan)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_zoom909RomInfo, sg1k_zoom909RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Huo Hu Li ~ Zoom 909 (Taiwan)
static struct BurnRomInfo sg1k_huohuliRomDesc[] = {
	{ "Huo Hu Li (Taiwan)(198x)(Aaronix).sg",	0x08000, 0x9943fc2b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_huohuli)
STD_ROM_FN(sg1k_huohuli)

struct BurnDriver BurnDrvsg1k_huohuli = {
	"sg1k_huohuli", "sg1k_zoom909", NULL, NULL, "1985?",
	"Huo Hu Li ~ Zoom 909 (Taiwan)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_huohuliRomInfo, sg1k_huohuliRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// --------------------------
// Homebrew/Aftermarket Games
// --------------------------


// Aerial (HB)
static struct BurnRomInfo sg1k_aerialRomDesc[] = {
	{ "Aerial (2022)(Inufuto).sg",	10750, 0x87e4f535, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_aerial)
STD_ROM_FN(sg1k_aerial)

struct BurnDriver BurnDrvsg1k_aerial = {
	"sg1k_aerial", NULL, NULL, NULL, "2022",
	"Aerial (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_HORSHOOT, 0,
	SG1KGetZipName, sg1k_aerialRomInfo, sg1k_aerialRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// AntiAir (HB)
static struct BurnRomInfo sg1k_antiairRomDesc[] = {
	{ "AntiAir (2024)(Inufuto).sg",	8107, 0x948599bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_antiair)
STD_ROM_FN(sg1k_antiair)

struct BurnDriver BurnDrvsg1k_antiair = {
	"sg1k_antiair", NULL, NULL, NULL, "2024",
	"AntiAir (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_antiairRomInfo, sg1k_antiairRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Arno Dash (HB)
static struct BurnRomInfo sg1k_arnodashRomDesc[] = {
	{ "arno dash (2020)(under4mhz).sg",	0x08000, 0xea921f7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_arnodash)
STD_ROM_FN(sg1k_arnodash)

struct BurnDriver BurnDrvsg1k_arnodash = {
	"sg1k_arnodash", NULL, NULL, NULL, "2020",
	"Arno Dash (HB)\0", NULL, "Under4MHz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_arnodashRomInfo, sg1k_arnodashRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Arno Dash 2 (HB, v1.03)
static struct BurnRomInfo sg1k_arnodash2RomDesc[] = {
	{ "Arno Dash 2 v1.03 (2020)(Under4Mhz).sg",	32768, 0xe0478a53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_arnodash2)
STD_ROM_FN(sg1k_arnodash2)

struct BurnDriver BurnDrvsg1k_arnodash2 = {
	"sg1k_arnodash2", NULL, NULL, NULL, "2020",
	"Arno Dash 2 (HB, v1.03)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_arnodash2RomInfo, sg1k_arnodash2RomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bloktris (HB)
static struct BurnRomInfo sg1k_bloktrisRomDesc[] = {
	{ "Bloktris (2023)(Under4Mhz).sg",	49152, 0x7febcd40, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bloktris)
STD_ROM_FN(sg1k_bloktris)

struct BurnDriver BurnDrvsg1k_bloktris = {
	"sg1k_bloktris", NULL, NULL, NULL, "2023",
	"Bloktris (HB)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_bloktrisRomInfo, sg1k_bloktrisRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cacorm (HB)
static struct BurnRomInfo sg1k_cacormRomDesc[] = {
	{ "Cacorm (2022)(Inufuto).sg",	8550, 0x26d37da3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cacorm)
STD_ROM_FN(sg1k_cacorm)

struct BurnDriver BurnDrvsg1k_cacorm = {
	"sg1k_cacorm", NULL, NULL, NULL, "2022",
	"Cacorm (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_cacormRomInfo, sg1k_cacormRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Che-Man (HB)
static struct BurnRomInfo sg1k_chemanRomDesc[] = {
	{ "Che-Man (2018)(The Mojon Twins).bin",	49152, 0x477a5d75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cheman)
STD_ROM_FN(sg1k_cheman)

struct BurnDriver BurnDrvsg1k_cheman = {
	"sg1k_cheman", NULL, NULL, NULL, "2018",
	"Che-Man (HB)\0", NULL, "The Mojon Twins", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_chemanRomInfo, sg1k_chemanRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cheril Perils Classic (HB)
static struct BurnRomInfo sg1k_cherilpRomDesc[] = {
	{ "Cheril Perils Classic (2018)(The Mojon Twins).bin",	49152, 0xf87d6ef5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cherilp)
STD_ROM_FN(sg1k_cherilp)

struct BurnDriver BurnDrvsg1k_cherilp = {
	"sg1k_cherilp", NULL, NULL, NULL, "2018",
	"Cheril Perils Classic (HB)\0", NULL, "The Mojon Twins", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_cherilpRomInfo, sg1k_cherilpRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cracky (HB)
static struct BurnRomInfo sg1k_crackyRomDesc[] = {
	{ "Cracky (2023)(Inufuto).sg",	7390, 0x34517b4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cracky)
STD_ROM_FN(sg1k_cracky)

struct BurnDriver BurnDrvsg1k_cracky = {
	"sg1k_cracky", NULL, NULL, NULL, "2023",
	"Cracky (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_crackyRomInfo, sg1k_crackyRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cross Bomber (HB)
static struct BurnRomInfo sg1k_crossbomberRomDesc[] = {
	{ "Cross Bomber (2023)(Fabrizio Caruso).sg",	32768, 0xc44b95f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_crossbomber)
STD_ROM_FN(sg1k_crossbomber)

struct BurnDriver BurnDrvsg1k_crossbomber = {
	"sg1k_crossbomber", NULL, NULL, NULL, "2023",
	"Cross Bomber (HB)\0", NULL, "Fabrizio Caruso", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_crossbomberRomInfo, sg1k_crossbomberRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cross TREX (HB)
static struct BurnRomInfo sg1k_crosstrexRomDesc[] = {
	{ "Cross TREX (2024)(Fabrizio Caruso).sg",	32768, 0x6211e354, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_crosstrex)
STD_ROM_FN(sg1k_crosstrex)

struct BurnDriver BurnDrvsg1k_crosstrex = {
	"sg1k_crosstrex", NULL, NULL, NULL, "2024",
	"Cross TREX (HB)\0", NULL, "Fabrizio Caruso", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_crosstrexRomInfo, sg1k_crosstrexRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Foryster (HB, v00.9)
static struct BurnRomInfo sg1k_forysterRomDesc[] = {
	{ "Foryster v00.9 (2022)(siudym).bin",	32768, 0xf8d7fa2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_foryster)
STD_ROM_FN(sg1k_foryster)

struct BurnDriver BurnDrvsg1k_foryster = {
	"sg1k_foryster", NULL, NULL, NULL, "2022",
	"Foryster (HB, v00.9)\0", NULL, "siudym", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_forysterRomInfo, sg1k_forysterRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Guntus (HB)
static struct BurnRomInfo sg1k_guntusRomDesc[] = {
	{ "Guntus (2022)(Inufuto).sg",	11885, 0xc23f2f57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_guntus)
STD_ROM_FN(sg1k_guntus)

struct BurnDriver BurnDrvsg1k_guntus = {
	"sg1k_guntus", NULL, NULL, NULL, "2022",
	"Guntus (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_SHOOT, 0,
	SG1KGetZipName, sg1k_guntusRomInfo, sg1k_guntusRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Hopman (HB)
static struct BurnRomInfo sg1k_hopmanRomDesc[] = {
	{ "Hopman (2023)(Inufuto).sg",	9958, 0x8bc01dd6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hopman)
STD_ROM_FN(sg1k_hopman)

struct BurnDriver BurnDrvsg1k_hopman = {
	"sg1k_hopman", NULL, NULL, NULL, "2023",
	"Hopman (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_hopmanRomInfo, sg1k_hopmanRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Jet Paco and Jet Puri (HB)
static struct BurnRomInfo sg1k_jetpacoRomDesc[] = {
	{ "Jet Paco and Jet Puri (2018)(The Mojon Twins).sg",	0x0c000, 0xd31c6b3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jetpaco)
STD_ROM_FN(sg1k_jetpaco)

struct BurnDriver BurnDrvsg1k_jetpaco = {
	"sg1k_jetpaco", NULL, NULL, NULL, "2018",
	"Jet Paco and Jet Puri (HB)\0", NULL, "The Mojon Twins", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_jetpacoRomInfo, sg1k_jetpacoRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Klondike Solitaire (HB, v1.04)
static struct BurnRomInfo sg1k_ksolitaireRomDesc[] = {
	{ "Klondike Solitaire v1.04 (2020)(Under4Mhz).sg",	32768, 0x5f9f8b15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ksolitaire)
STD_ROM_FN(sg1k_ksolitaire)

struct BurnDriver BurnDrvsg1k_ksolitaire = {
	"sg1k_ksolitaire", NULL, NULL, NULL, "2020",
	"Klondike Solitaire (HB, v1.04)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_CARD, 0,
	SG1KGetZipName, sg1k_ksolitaireRomInfo, sg1k_ksolitaireRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mahjong Solitaire (HB, v1.07)
static struct BurnRomInfo sg1k_msolitaireRomDesc[] = {
	{ "Mahjong Solitaire v1.07 (2020)(Under4Mhz).sg",	32768, 0x1b14b1c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_msolitaire)
STD_ROM_FN(sg1k_msolitaire)

struct BurnDriver BurnDrvsg1k_msolitaire = {
	"sg1k_msolitaire", NULL, NULL, NULL, "2020",
	"Mahjong Solitaire (HB, v1.07)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_msolitaireRomInfo, sg1k_msolitaireRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mieyen (HB)
static struct BurnRomInfo sg1k_mieyenRomDesc[] = {
	{ "Mieyen (2025)(Inufuto).sg",	16384, 0x3c65bb5a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mieyen)
STD_ROM_FN(sg1k_mieyen)

struct BurnDriver BurnDrvsg1k_mieyen = {
	"sg1k_mieyen", NULL, NULL, NULL, "2025",
	"Mieyen (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_MAZE, 0,
	SG1KGetZipName, sg1k_mieyenRomInfo, sg1k_mieyenRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Palikat (HB)
// https://www.smspower.org/forums/18128-PalikatForSegaSC3000
static struct BurnRomInfo sg1k_palikatRomDesc[] = {
	{ "Palikat (2020)(helmha).sg",	0x08000, 0xec169914, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_palikat)
STD_ROM_FN(sg1k_palikat)

struct BurnDriver BurnDrvsg1k_palikat = {
	"sg1k_palikat", NULL, NULL, NULL, "2020",
	"Palikat (HB)\0", NULL, "helmha", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_palikatRomInfo, sg1k_palikatRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pegged (HB, v1.02)
static struct BurnRomInfo sg1k_peggedRomDesc[] = {
	{ "Pegged v1.02 (2021)(Under4Mhz).sg",	32768, 0x21e891f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pegged)
STD_ROM_FN(sg1k_pegged)

struct BurnDriver BurnDrvsg1k_pegged = {
	"sg1k_pegged", NULL, NULL, NULL, "2021",
	"Pegged (HB, v1.02)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_peggedRomInfo, sg1k_peggedRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitman (HB, v1.03)
static struct BurnRomInfo sg1k_pitmanRomDesc[] = {
	{ "Pitman v1.03 (2021)(Under4Mhz).sg",	32768, 0xe325f022, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pitman)
STD_ROM_FN(sg1k_pitman)

struct BurnDriver BurnDrvsg1k_pitman = {
	"sg1k_pitman", NULL, NULL, NULL, "2021",
	"Pitman (HB, v1.03)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_pitmanRomInfo, sg1k_pitmanRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Snake (HB, v1.04)
static struct BurnRomInfo sg1k_snakeRomDesc[] = {
	{ "Snake v1.04 (2021)(Under4Mhz).sg",	32768, 0xb9ab9d0c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_snake)
STD_ROM_FN(sg1k_snake)

struct BurnDriver BurnDrvsg1k_snake = {
	"sg1k_snake", NULL, NULL, NULL, "2021",
	"Snake (HB, v1.04)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_snakeRomInfo, sg1k_snakeRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sgt. Helmet - Training Day (HB)
static struct BurnRomInfo sg1k_sgthelmetRomDesc[] = {
	{ "Sgt. Helmet - Training Day (2018)(The Mojon Twins).bin",	49152, 0x27c065a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sgthelmet)
STD_ROM_FN(sg1k_sgthelmet)

struct BurnDriver BurnDrvsg1k_sgthelmet = {
	"sg1k_sgthelmet", NULL, NULL, NULL, "2018",
	"Sgt. Helmet - Training Day (HB)\0", NULL, "The Mojon Twins", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_MAZE | GBF_RUNGUN, 0,
	SG1KGetZipName, sg1k_sgthelmetRomInfo, sg1k_sgthelmetRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super UWOL! (HB)
static struct BurnRomInfo sg1k_superuwolRomDesc[] = {
	{ "Super UWOL (2016)(The Mojon Twins).sg",	0x0c000, 0xaa8ea6eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_superuwol)
STD_ROM_FN(sg1k_superuwol)

struct BurnDriver BurnDrvsg1k_superuwol = {
	"sg1k_superuwol", NULL, NULL, NULL, "2016",
	"Super UWOL! (HB)\0", NULL, "The Mojon Twins", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PLATFORM, 0,
	SG1KGetZipName, sg1k_superuwolRomInfo, sg1k_superuwolRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ultimate Dungeon Escape (HB)
static struct BurnRomInfo sg1k_ultdungescRomDesc[] = {
	{ "Ultimate Dungeon Escape (2025)(tibone, Cyrus, Revo).sg",	32768, 0x73515934, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ultdungesc)
STD_ROM_FN(sg1k_ultdungesc)

struct BurnDriver BurnDrvsg1k_ultdungesc = {
	"sg1k_ultdungesc", NULL, NULL, NULL, "2025",
	"Ultimate Dungeon Escape (HB)\0", NULL, "tibone, Cyrus, Revo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_MAZE | GBF_RPG, 0,
	SG1KGetZipName, sg1k_ultdungescRomInfo, sg1k_ultdungescRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Vexed (HB, v1.08)
static struct BurnRomInfo sg1k_vexedRomDesc[] = {
	{ "Vexed v1.08 (2023)(Under4Mhz).sg",	32768, 0x124b2a43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_vexed)
STD_ROM_FN(sg1k_vexed)

struct BurnDriver BurnDrvsg1k_vexed = {
	"sg1k_vexed", NULL, NULL, NULL, "2020-23",
	"Vexed (HB, v1.08)\0", NULL, "Under4Mhz", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_vexedRomInfo, sg1k_vexedRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Whack 'Em Smack 'Em Byrons (HB)
static struct BurnRomInfo sg1k_wsbyronsRomDesc[] = {
	{ "Whack 'Em Smack 'Em Byrons (2024)(Jess Creations).sg",	32706, 0xa444660d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wsbyrons)
STD_ROM_FN(sg1k_wsbyrons)

struct BurnDriver BurnDrvsg1k_wsbyrons = {
	"sg1k_wsbyrons", NULL, NULL, NULL, "2024",
	"Whack 'Em Smack 'Em Byrons (HB)\0", NULL, "Jess Creations", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION, 0,
	SG1KGetZipName, sg1k_wsbyronsRomInfo, sg1k_wsbyronsRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yawi (HB, v0.9.3)
static struct BurnRomInfo sg1k_yawiRomDesc[] = {
	{ "Yawi v0.9.3 (2025)(dark).sg",	49152, 0xa1cc62dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yawi)
STD_ROM_FN(sg1k_yawi)

struct BurnDriver BurnDrvsg1k_yawi = {
	"sg1k_yawi", NULL, NULL, NULL, "2025",
	"Yawi (HB, v0.9.3)\0", NULL, "dark", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_ACTION | GBF_PUZZLE, 0,
	SG1KGetZipName, sg1k_yawiRomInfo, sg1k_yawiRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yewdow (HB)
static struct BurnRomInfo sg1k_yewdowRomDesc[] = {
	{ "Yewdow (2023)(Inufuto).sg",	9141, 0x5917970d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yewdow)
STD_ROM_FN(sg1k_yewdow)

struct BurnDriver BurnDrvsg1k_yewdow = {
	"sg1k_yewdow", NULL, NULL, NULL, "2023",
	"Yewdow (HB)\0", NULL, "Inufuto", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SEGA_SG1000, GBF_MAZE | GBF_ACTION, 0,
	SG1KGetZipName, sg1k_yewdowRomInfo, sg1k_yewdowRomName, NULL, NULL, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

