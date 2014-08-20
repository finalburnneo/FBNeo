// FB Alpha Sega SG-1000-based arcade driver module
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
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo Sg1000InputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
};

STDINPUTINFO(Sg1000)

static struct BurnDIPInfo Sg1000DIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x40, 0x40, "Off"			},
	{0x11, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x11, 0x01, 0x80, 0x00, "Japanese"		},
	{0x11, 0x01, 0x80, 0x80, "English"		},
};

STDDIPINFO(Sg1000)

static void __fastcall sg1000_write_port(unsigned short port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x7f:
			SN76496Write(0, data);
		return;

		case 0xbe:
			TMS9928AWriteVRAM(data);
		return;

		case 0xbf:
			TMS9928AWriteRegs(data);
		return;

		case 0xdc:
		case 0xdd:
		case 0xde:
		case 0xdf:
			ppi8255_w(0, port & 3, data);
		return;
	}
}

static UINT8 __fastcall sg1000_read_port(unsigned short port)
{
	switch (port & 0xff)
	{
		case 0xbe:
			return TMS9928AReadVRAM();

		case 0xbf:
			return TMS9928AReadRegs();

/*		case 0xdc:
		case 0xdd:
		case 0xde:
		case 0xdf:
                return ppi8255_r(0, port & 3); screws up sg-1000 inputs! */
		case 0xdc:
			return DrvInputs[0];

		case 0xdd:
			return DrvInputs[1];

		case 0xde:
			return 0x80;

	}

	return 0;
}

static UINT8 sg1000_ppi8255_portA_read() { return DrvInputs[0]; }
static UINT8 sg1000_ppi8255_portB_read() { return DrvInputs[1]; }
static UINT8 sg1000_ppi8255_portC_read() { return DrvDips[0]; }

static void sg1000_ppi8255_portC_write(UINT8 data)
{
	data &= 0x01; // coin counter
}

static void vdp_interrupt(int state)
{
	ZetSetIRQLine(0, state ? ZET_IRQSTATUS_ACK : ZET_IRQSTATUS_NONE);
}

static int DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	return 0;
}

static int MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvZ80Dec		= Next; Next += 0x010000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static int DrvInit()
{
	AllMem = NULL;
	MemIndex();
	int nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x8000, 2, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xc3ff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc3ff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc3ff, 2, DrvZ80RAM);
	ZetSetOutHandler(sg1000_write_port);
	ZetSetInHandler(sg1000_read_port);
	ZetClose();

	SN76489Init(0, 3579545, 0);

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, vdp_interrupt);

	ppi8255_init(1);
	PPI0PortReadA	= sg1000_ppi8255_portA_read;
	PPI0PortReadB	= sg1000_ppi8255_portB_read;
	PPI0PortReadC	= sg1000_ppi8255_portC_read;
	PPI0PortWriteC	= sg1000_ppi8255_portC_write;

	DrvDoReset();

	return 0;
}

static int DrvExit()
{
	TMS9928AExit();
	ZetExit();
	SN76496Exit();
	ppi8255_exit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static int DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2);
		for (int i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
                        if (i==6 || i==7)
                            DrvInputs[1] ^= (DrvJoy1[i] & 1) << i;
                        else
                            DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetOpen(0);
	ZetRun(3579545 / 60);
	TMS9928AInterrupt();
	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		TMS9928ADraw();
	}

	return 0;
}

static int DrvScan(int nAction,int *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
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

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	// remove the "SG1K_"
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 5];
	}

	*pszName = szFilename;

	return 0;
}

// End of driver, the following driver info. has been synthesized from hash/sg1000.xml of MESS

// San-nin Mahjong (Jpn, OMV)

static struct BurnRomInfo sg1k_3ninmjRomDesc[] = {
	{ "san-nin mahjong.bin",	0x04000, 0x885fa64d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_3ninmj)
STD_ROM_FN(sg1k_3ninmj)

struct BurnDriver BurnDrvsg1k_3ninmj = {
	"sg1k_3ninmj", NULL, NULL, NULL, "1984",
	"San-nin Mahjong (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_3ninmjRomInfo, sg1k_3ninmjRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// San-nin Mahjong (Tw)

static struct BurnRomInfo sg1k_3ninmjtRomDesc[] = {
	{ "san-nin mahjong (tw).bin",	0x04000, 0x6fd17655, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_3ninmjt)
STD_ROM_FN(sg1k_3ninmjt)

struct BurnDriver BurnDrvsg1k_3ninmjt = {
	"sg1k_3ninmjt", NULL, NULL, NULL, "1984?",
	"San-nin Mahjong (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_3ninmjtRomInfo, sg1k_3ninmjtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bank Panic (Jpn)

static struct BurnRomInfo sg1k_bankpRomDesc[] = {
	{ "bank panic (japan).bin",	0x08000, 0xd8a87095, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bankp)
STD_ROM_FN(sg1k_bankp)

struct BurnDriver BurnDrvsg1k_bankp = {
	"sg1k_bankp", NULL, NULL, NULL, "1985",
	"Bank Panic (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bankpRomInfo, sg1k_bankpRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bi Li Da Dao (Tw)

static struct BurnRomInfo sg1k_bilidadaRomDesc[] = {
	{ "bank panic (tw).bin",	0x08000, 0xbd43fde4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bilidada)
STD_ROM_FN(sg1k_bilidada)

struct BurnDriver BurnDrvsg1k_bilidada = {
	"sg1k_bilidada", NULL, NULL, NULL, "1985?",
	"Bi Li Da Dao (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bilidadaRomInfo, sg1k_bilidadaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// The Black Onyx (Jpn)

static struct BurnRomInfo sg1k_blckonyxRomDesc[] = {
	{ "black onyx, the (japan).bin",	0x08000, 0x26ecd094, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_blckonyx)
STD_ROM_FN(sg1k_blckonyx)

struct BurnDriver BurnDrvsg1k_blckonyx = {
	"sg1k_blckonyx", NULL, NULL, NULL, "1987",
	"The Black Onyx (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_blckonyxRomInfo, sg1k_blckonyxRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bomb Jack (Jpn)

static struct BurnRomInfo sg1k_bombjackRomDesc[] = {
	{ "bomb jack (japan).bin",	0x08000, 0xea0f2691, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjack)
STD_ROM_FN(sg1k_bombjack)

struct BurnDriver BurnDrvsg1k_bombjack = {
	"sg1k_bombjack", NULL, NULL, NULL, "1985",
	"Bomb Jack (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombjackRomInfo, sg1k_bombjackRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bomb Jack (Kor)

static struct BurnRomInfo sg1k_bombjackk1RomDesc[] = {
	{ "bomb jack [english logo] (kr).bin",	0x08000, 0x0c69d837, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackk1)
STD_ROM_FN(sg1k_bombjackk1)

struct BurnDriver BurnDrvsg1k_bombjackk1 = {
	"sg1k_bombjackk1", NULL, NULL, NULL, "1985?",
	"Bomb Jack (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombjackk1RomInfo, sg1k_bombjackk1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Beom Jjaek (Kor)

static struct BurnRomInfo sg1k_bombjackk2RomDesc[] = {
	{ "bomb jack [korean logo] (kr).bin",	0x08000, 0xb0c7b310, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackk2)
STD_ROM_FN(sg1k_bombjackk2)

struct BurnDriver BurnDrvsg1k_bombjackk2 = {
	"sg1k_bombjackk2", NULL, NULL, NULL, "1985?",
	"Beom Jjaek (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombjackk2RomInfo, sg1k_bombjackk2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bomb Jack (Tw)

static struct BurnRomInfo sg1k_bombjackt1RomDesc[] = {
	{ "bomb jack [english logo] (tw).bin",	0x08000, 0xcda3a335, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackt1)
STD_ROM_FN(sg1k_bombjackt1)

struct BurnDriver BurnDrvsg1k_bombjackt1 = {
	"sg1k_bombjackt1", NULL, NULL, NULL, "1985?",
	"Bomb Jack (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombjackt1RomInfo, sg1k_bombjackt1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Bomber Man Special (Tw)

static struct BurnRomInfo sg1k_bombmnspRomDesc[] = {
	{ "bomberman special (tw).bin",	0x0c000, 0x69fc1494, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombmnsp)
STD_ROM_FN(sg1k_bombmnsp)

struct BurnDriver BurnDrvsg1k_bombmnsp = {
	"sg1k_bombmnsp", NULL, NULL, NULL, "1986?",
	"Bomber Man Special (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombmnspRomInfo, sg1k_bombmnspRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hongzha Dui (Tw)

static struct BurnRomInfo sg1k_hongduiRomDesc[] = {
	{ "bomberman special [dahjee] (tw).bin",	0x0c000, 0xce5648c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hongdui)
STD_ROM_FN(sg1k_hongdui)

struct BurnDriver BurnDrvsg1k_hongdui = {
	"sg1k_hongdui", NULL, NULL, NULL, "1986?",
	"Hongzha Dui (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hongduiRomInfo, sg1k_hongduiRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chaoren (Tw)

static struct BurnRomInfo sg1k_bombjackt2RomDesc[] = {
	{ "bomb jack [chinese logo] (tw).bin",	0x08000, 0xdeb213fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bombjackt2)
STD_ROM_FN(sg1k_bombjackt2)

struct BurnDriver BurnDrvsg1k_bombjackt2 = {
	"sg1k_bombjackt2", NULL, NULL, NULL, "1985?",
	"Chaoren (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bombjackt2RomInfo, sg1k_bombjackt2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Borderline (Euro, Jpn)

static struct BurnRomInfo sg1k_bordrlinRomDesc[] = {
	{ "borderline (japan, europe).bin",	0x04000, 0x0b4bca74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_bordrlin)
STD_ROM_FN(sg1k_bordrlin)

struct BurnDriver BurnDrvsg1k_bordrlin = {
	"sg1k_bordrlin", NULL, NULL, NULL, "1983",
	"Borderline (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_bordrlinRomInfo, sg1k_bordrlinRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Cabbage Patch Kids (Tw)

static struct BurnRomInfo sg1k_cabkidsRomDesc[] = {
	{ "cabbage patch kids (tw).bin",	0x08000, 0x9d91ab78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cabkids)
STD_ROM_FN(sg1k_cabkids)

struct BurnDriver BurnDrvsg1k_cabkids = {
	"sg1k_cabkids", NULL, NULL, NULL, "198?",
	"Cabbage Patch Kids (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_cabkidsRomInfo, sg1k_cabkidsRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// The Castle (Jpn)

static struct BurnRomInfo sg1k_castleRomDesc[] = {
	{ "mpr-10159.ic1",	0x08000, 0x092f29d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_castle)
STD_ROM_FN(sg1k_castle)

struct BurnDriver BurnDrvsg1k_castle = {
	"sg1k_castle", NULL, NULL, NULL, "1986",
	"The Castle (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_castleRomInfo, sg1k_castleRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Mowang migong ~ The Castle (Tw)

static struct BurnRomInfo sg1k_castletRomDesc[] = {
	{ "castle, the [msx] (tw).bin",	0x0c000, 0x2e366ccf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_castlet)
STD_ROM_FN(sg1k_castlet)

struct BurnDriver BurnDrvsg1k_castlet = {
	"sg1k_castlet", NULL, NULL, NULL, "1986?",
	"Mowang migong ~ The Castle (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_castletRomInfo, sg1k_castletRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chack'n Pop (Jpn)

static struct BurnRomInfo sg1k_chacknRomDesc[] = {
	{ "chack'n pop (japan).bin",	0x08000, 0xd37bda49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chackn)
STD_ROM_FN(sg1k_chackn)

struct BurnDriver BurnDrvsg1k_chackn = {
	"sg1k_chackn", NULL, NULL, NULL, "1985",
	"Chack'n Pop (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chacknRomInfo, sg1k_chacknRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Da Yu (Tw)

static struct BurnRomInfo sg1k_dayuRomDesc[] = {
	{ "chack'n pop (tw).bin",	0x08000, 0xd81a72ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dayu)
STD_ROM_FN(sg1k_dayu)

struct BurnDriver BurnDrvsg1k_dayu = {
	"sg1k_dayu", NULL, NULL, NULL, "1985?",
	"Da Yu (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_dayuRomInfo, sg1k_dayuRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Challenge Derby (Jpn, OMV)

static struct BurnRomInfo sg1k_chaldrbyRomDesc[] = {
	{ "challenge derby [16k].bin",	0x04000, 0xc91551da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrby)
STD_ROM_FN(sg1k_chaldrby)

struct BurnDriver BurnDrvsg1k_chaldrby = {
	"sg1k_chaldrby", NULL, NULL, NULL, "1984",
	"Challenge Derby (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbyRomInfo, sg1k_chaldrbyRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Challenge Derby (Jpn, OMV, Alt)

static struct BurnRomInfo sg1k_chaldrbyaRomDesc[] = {
	{ "challenge derby [40k].bin",	0x0a000, 0x02bc891f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrbya)
STD_ROM_FN(sg1k_chaldrbya)

struct BurnDriver BurnDrvsg1k_chaldrbya = {
	"sg1k_chaldrbya", NULL, NULL, NULL, "1984",
	"Challenge Derby (Jpn, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbyaRomInfo, sg1k_chaldrbyaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Challenge Derby (Jpn, OMV, Hacked)

static struct BurnRomInfo sg1k_chaldrbybRomDesc[] = {
	{ "challenge derby [ver.b] [40k].bin",	0x0a000, 0xc5f014dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chaldrbyb)
STD_ROM_FN(sg1k_chaldrbyb)

struct BurnDriver BurnDrvsg1k_chaldrbyb = {
	"sg1k_chaldrbyb", NULL, NULL, NULL, "1984",
	"Challenge Derby (Jpn, OMV, Hacked)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chaldrbybRomInfo, sg1k_chaldrbybRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Baseball (Jpn)

static struct BurnRomInfo sg1k_champbasRomDesc[] = {
	{ "mpr-6381.ic1",	0x04000, 0x5970a12b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbas)
STD_ROM_FN(sg1k_champbas)

struct BurnDriver BurnDrvsg1k_champbas = {
	"sg1k_champbas", NULL, NULL, NULL, "1983",
	"Champion Baseball (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbasRomInfo, sg1k_champbasRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Baseball (Jpn, Older)

static struct BurnRomInfo sg1k_champbasaRomDesc[] = {
	{ "champion baseball [40k].bin",	0x0a000, 0xc0c16fa7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbasa)
STD_ROM_FN(sg1k_champbasa)

struct BurnDriver BurnDrvsg1k_champbasa = {
	"sg1k_champbasa", NULL, NULL, NULL, "1983",
	"Champion Baseball (Jpn, Older)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbasaRomInfo, sg1k_champbasaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Baseball (Tw)

static struct BurnRomInfo sg1k_champbastRomDesc[] = {
	{ "champion baseball [16k] (tw).bin",	0x04000, 0x677e6878, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbast)
STD_ROM_FN(sg1k_champbast)

struct BurnDriver BurnDrvsg1k_champbast = {
	"sg1k_champbast", NULL, NULL, NULL, "1985?",
	"Champion Baseball (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbastRomInfo, sg1k_champbastRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Billiards (Jpn)

static struct BurnRomInfo sg1k_champbilRomDesc[] = {
	{ "champion billiards (japan).bin",	0x08000, 0x62b21e31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbil)
STD_ROM_FN(sg1k_champbil)

struct BurnDriver BurnDrvsg1k_champbil = {
	"sg1k_champbil", NULL, NULL, NULL, "1986",
	"Champion Billiards (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbilRomInfo, sg1k_champbilRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hua Shi Zhuangqiu (Tw)

static struct BurnRomInfo sg1k_champbiltRomDesc[] = {
	{ "champion billiards (tw).bin",	0x08000, 0x56d85bd4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbilt)
STD_ROM_FN(sg1k_champbilt)

struct BurnDriver BurnDrvsg1k_champbilt = {
	"sg1k_champbilt", NULL, NULL, NULL, "1986?",
	"Hua Shi Zhuangqiu (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbiltRomInfo, sg1k_champbiltRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Boxing (Jpn)

static struct BurnRomInfo sg1k_champboxRomDesc[] = {
	{ "mpr-6100.ic1",	0x08000, 0x26f947d1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbox)
STD_ROM_FN(sg1k_champbox)

struct BurnDriver BurnDrvsg1k_champbox = {
	"sg1k_champbox", NULL, NULL, NULL, "1984",
	"Champion Boxing (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champboxRomInfo, sg1k_champboxRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Boxing (Tw)

static struct BurnRomInfo sg1k_champboxtRomDesc[] = {
	{ "champion boxing (tw).bin",	0x08000, 0x15d2ce33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champboxt)
STD_ROM_FN(sg1k_champboxt)

struct BurnDriver BurnDrvsg1k_champboxt = {
	"sg1k_champboxt", NULL, NULL, NULL, "1984?",
	"Champion Boxing (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champboxtRomInfo, sg1k_champboxtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Golf (Jpn)

static struct BurnRomInfo sg1k_champglfRomDesc[] = {
	{ "mpr-5435.ic1",	0x08000, 0x868419b5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglf)
STD_ROM_FN(sg1k_champglf)

struct BurnDriver BurnDrvsg1k_champglf = {
	"sg1k_champglf", NULL, NULL, NULL, "1983",
	"Champion Golf (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champglfRomInfo, sg1k_champglfRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Golf (Kor)

static struct BurnRomInfo sg1k_champglfkRomDesc[] = {
	{ "champion golf (kr).bin",	0x08000, 0x35af42ad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglfk)
STD_ROM_FN(sg1k_champglfk)

struct BurnDriver BurnDrvsg1k_champglfk = {
	"sg1k_champglfk", NULL, NULL, NULL, "198?",
	"Champion Golf (Kor)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champglfkRomInfo, sg1k_champglfkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Ice Hockey (Jpn)

static struct BurnRomInfo sg1k_champiceRomDesc[] = {
	{ "champion ice hockey (japan).bin",	0x08000, 0xbdc05652, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champice)
STD_ROM_FN(sg1k_champice)

struct BurnDriver BurnDrvsg1k_champice = {
	"sg1k_champice", NULL, NULL, NULL, "1985",
	"Champion Ice Hockey (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champiceRomInfo, sg1k_champiceRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Ice Hockey (Kor)

static struct BurnRomInfo sg1k_champicekRomDesc[] = {
	{ "champion ice hockey (kr).bin",	0x08000, 0xec0b862c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champicek)
STD_ROM_FN(sg1k_champicek)

struct BurnDriver BurnDrvsg1k_champicek = {
	"sg1k_champicek", NULL, NULL, NULL, "198?",
	"Champion Ice Hockey (Kor)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champicekRomInfo, sg1k_champicekRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Qugunqiu (Tw)

static struct BurnRomInfo sg1k_champicetRomDesc[] = {
	{ "champion ice hockey (tw).bin",	0x08000, 0xc6e5192f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champicet)
STD_ROM_FN(sg1k_champicet)

struct BurnDriver BurnDrvsg1k_champicet = {
	"sg1k_champicet", NULL, NULL, NULL, "1985?",
	"Qugunqiu (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champicetRomInfo, sg1k_champicetRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Kendou (Jpn)

static struct BurnRomInfo sg1k_champkenRomDesc[] = {
	{ "champion kendou (japan).bin",	0x08000, 0x10cdebce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champken)
STD_ROM_FN(sg1k_champken)

struct BurnDriver BurnDrvsg1k_champken = {
	"sg1k_champken", NULL, NULL, NULL, "1986",
	"Champion Kendou (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champkenRomInfo, sg1k_champkenRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Jiandao (Tw)

static struct BurnRomInfo sg1k_champkentRomDesc[] = {
	{ "champion kendou (tw).bin",	0x08000, 0xa5f61363, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champkent)
STD_ROM_FN(sg1k_champkent)

struct BurnDriver BurnDrvsg1k_champkent = {
	"sg1k_champkent", NULL, NULL, NULL, "1986?",
	"Jiandao (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champkentRomInfo, sg1k_champkentRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Pro Wrestling (Jpn)

static struct BurnRomInfo sg1k_champpwrRomDesc[] = {
	{ "champion pro wrestling (japan).bin",	0x08000, 0x372fe6bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champpwr)
STD_ROM_FN(sg1k_champpwr)

struct BurnDriver BurnDrvsg1k_champpwr = {
	"sg1k_champpwr", NULL, NULL, NULL, "1985",
	"Champion Pro Wrestling (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champpwrRomInfo, sg1k_champpwrRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Soccer (Jpn)

static struct BurnRomInfo sg1k_champscrRomDesc[] = {
	{ "champion soccer (japan).bin",	0x04000, 0x6f39719e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champscr)
STD_ROM_FN(sg1k_champscr)

struct BurnDriver BurnDrvsg1k_champscr = {
	"sg1k_champscr", NULL, NULL, NULL, "1984",
	"Champion Soccer (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champscrRomInfo, sg1k_champscrRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Soccer (Tw)

static struct BurnRomInfo sg1k_champscrtRomDesc[] = {
	{ "champion soccer (tw).bin",	0x04000, 0x269c1aee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champscrt)
STD_ROM_FN(sg1k_champscrt)

struct BurnDriver BurnDrvsg1k_champscrt = {
	"sg1k_champscrt", NULL, NULL, NULL, "1984?",
	"Champion Soccer (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champscrtRomInfo, sg1k_champscrtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Tennis (Jpn)

static struct BurnRomInfo sg1k_champtnsRomDesc[] = {
	{ "mpr-5439.ic1",	0x02000, 0x7c663316, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champtns)
STD_ROM_FN(sg1k_champtns)

struct BurnDriver BurnDrvsg1k_champtns = {
	"sg1k_champtns", NULL, NULL, NULL, "1983",
	"Champion Tennis (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champtnsRomInfo, sg1k_champtnsRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Choplifter (Jpn)

static struct BurnRomInfo sg1k_chopliftRomDesc[] = {
	{ "choplifter (japan).bin",	0x08000, 0x732b7180, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_choplift)
STD_ROM_FN(sg1k_choplift)

struct BurnDriver BurnDrvsg1k_choplift = {
	"sg1k_choplift", NULL, NULL, NULL, "1985",
	"Choplifter (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chopliftRomInfo, sg1k_chopliftRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Choplifter (Jpn, Prototype)

static struct BurnRomInfo sg1k_chopliftpRomDesc[] = {
	{ "choplifter (japan) (proto).bin",	0x08000, 0xff435469, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftp)
STD_ROM_FN(sg1k_chopliftp)

struct BurnDriver BurnDrvsg1k_chopliftp = {
	"sg1k_chopliftp", NULL, NULL, NULL, "1985",
	"Choplifter (Jpn, Prototype)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chopliftpRomInfo, sg1k_chopliftpRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Choplifter (Kor)

static struct BurnRomInfo sg1k_chopliftkRomDesc[] = {
	{ "choplifter (kr).bin",	0x08000, 0x954d8f26, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftk)
STD_ROM_FN(sg1k_chopliftk)

struct BurnDriver BurnDrvsg1k_chopliftk = {
	"sg1k_chopliftk", NULL, NULL, NULL, "1985?",
	"Choplifter (Kor)\0", NULL, "Sega?", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chopliftkRomInfo, sg1k_chopliftkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Fei Lang (Tw)

static struct BurnRomInfo sg1k_feilangRomDesc[] = {
	{ "choplifter [chinese logo] (tw).bin",	0x08000, 0x746a8d86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_feilang)
STD_ROM_FN(sg1k_feilang)

struct BurnDriver BurnDrvsg1k_feilang = {
	"sg1k_feilang", NULL, NULL, NULL, "1985?",
	"Fei Lang (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_feilangRomInfo, sg1k_feilangRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Fei Lang ~ Choplifter (Tw, Alt)

static struct BurnRomInfo sg1k_feilangaRomDesc[] = {
	{ "choplifter [english logo] (tw).bin",	0x08000, 0x7c603987, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_feilanga)
STD_ROM_FN(sg1k_feilanga)

struct BurnDriver BurnDrvsg1k_feilanga = {
	"sg1k_feilanga", NULL, NULL, NULL, "1985?",
	"Fei Lang ~ Choplifter (Tw, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_feilangaRomInfo, sg1k_feilangaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Choplifter (Tw)

static struct BurnRomInfo sg1k_choplifttRomDesc[] = {
	{ "choplifter [no logo] (tw).bin",	0x08000, 0x825e1aae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chopliftt)
STD_ROM_FN(sg1k_chopliftt)

struct BurnDriver BurnDrvsg1k_chopliftt = {
	"sg1k_chopliftt", NULL, NULL, NULL, "1985?",
	"Choplifter (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_choplifttRomInfo, sg1k_choplifttRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Championship Lode Runner (Jpn)

static struct BurnRomInfo sg1k_cloderunRomDesc[] = {
	{ "championship lode runner (japan).bin",	0x08000, 0x11db4b1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cloderun)
STD_ROM_FN(sg1k_cloderun)

struct BurnDriver BurnDrvsg1k_cloderun = {
	"sg1k_cloderun", NULL, NULL, NULL, "1985",
	"Championship Lode Runner (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_cloderunRomInfo, sg1k_cloderunRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Xun Bao Er Dai (Tw)

static struct BurnRomInfo sg1k_cloderuntRomDesc[] = {
	{ "championship lode runner (tw).bin",	0x08000, 0xec95ebcb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cloderunt)
STD_ROM_FN(sg1k_cloderunt)

struct BurnDriver BurnDrvsg1k_cloderunt = {
	"sg1k_cloderunt", NULL, NULL, NULL, "1985?",
	"Xun Bao Er Dai (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_cloderuntRomInfo, sg1k_cloderuntRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Maxi Tuan ~ Circus Charlie (Tw)

static struct BurnRomInfo sg1k_circuscRomDesc[] = {
	{ "circus charlie (tw).bin",	0x08000, 0x3404fce4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_circusc)
STD_ROM_FN(sg1k_circusc)

struct BurnDriver BurnDrvsg1k_circusc = {
	"sg1k_circusc", NULL, NULL, NULL, "198?",
	"Maxi Tuan ~ Circus Charlie (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_circuscRomInfo, sg1k_circuscRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Circus Charlie (Kor)

static struct BurnRomInfo sg1k_circusckRomDesc[] = {
	{ "circus charlie (kr).bin",	0x08000, 0x7f7f009d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_circusck)
STD_ROM_FN(sg1k_circusck)

struct BurnDriver BurnDrvsg1k_circusck = {
	"sg1k_circusck", NULL, NULL, NULL, "198?",
	"Circus Charlie (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_circusckRomInfo, sg1k_circusckRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Congo Bongo (Euro, Jpn, v1)

static struct BurnRomInfo sg1k_congoRomDesc[] = {
	{ "congo bongo [v1].bin",	0x06000, 0x5a24c7cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congo)
STD_ROM_FN(sg1k_congo)

struct BurnDriver BurnDrvsg1k_congo = {
	"sg1k_congo", NULL, NULL, NULL, "1983",
	"Congo Bongo (Euro, Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_congoRomInfo, sg1k_congoRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Congo Bongo (Euro, Jpn, v1 Alt)

static struct BurnRomInfo sg1k_congoaRomDesc[] = {
	{ "congo bongo [v1] [40k map].bin",	0x0a000, 0xf1506565, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congoa)
STD_ROM_FN(sg1k_congoa)

struct BurnDriver BurnDrvsg1k_congoa = {
	"sg1k_congoa", NULL, NULL, NULL, "1983",
	"Congo Bongo (Euro, Jpn, v1 Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_congoaRomInfo, sg1k_congoaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Congo Bongo (Jpn, v0)

static struct BurnRomInfo sg1k_congobRomDesc[] = {
	{ "congo bongo [v0].bin",	0x06000, 0x5eb48a20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congob)
STD_ROM_FN(sg1k_congob)

struct BurnDriver BurnDrvsg1k_congob = {
	"sg1k_congob", NULL, NULL, NULL, "1983",
	"Congo Bongo (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_congobRomInfo, sg1k_congobRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Congo Bongo (Tw)

static struct BurnRomInfo sg1k_congotRomDesc[] = {
	{ "congo bongo (tw).bin",	0x06000, 0xf7eb94c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_congot)
STD_ROM_FN(sg1k_congot)

struct BurnDriver BurnDrvsg1k_congot = {
	"sg1k_congot", NULL, NULL, NULL, "19??",
	"Congo Bongo (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_congotRomInfo, sg1k_congotRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// C_So! (Jpn)

static struct BurnRomInfo sg1k_csoRomDesc[] = {
	{ "c_so! (japan).bin",	0x08000, 0xbe7ed0eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cso)
STD_ROM_FN(sg1k_cso)

struct BurnDriver BurnDrvsg1k_cso = {
	"sg1k_cso", NULL, NULL, NULL, "1985",
	"C_So! (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_csoRomInfo, sg1k_csoRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Wanpi Gui (Tw)

static struct BurnRomInfo sg1k_csotRomDesc[] = {
	{ "c_so! (tw).bin",	0x08000, 0x82c4e3e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_csot)
STD_ROM_FN(sg1k_csot)

struct BurnDriver BurnDrvsg1k_csot = {
	"sg1k_csot", NULL, NULL, NULL, "1985?",
	"Wanpi Gui (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_csotRomInfo, sg1k_csotRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Da Cike (Tw)

static struct BurnRomInfo sg1k_dacikeRomDesc[] = {
	{ "yie ar kung-fu ii [dahjee] (tw).bin",	0x0c000, 0xfc87463c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dacike)
STD_ROM_FN(sg1k_dacike)

struct BurnDriver BurnDrvsg1k_dacike = {
	"sg1k_dacike", NULL, NULL, NULL, "1986?",
	"Da Cike (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_dacikeRomInfo, sg1k_dacikeRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Dokidoki Penguin Land (Jpn)

static struct BurnRomInfo sg1k_dokidokiRomDesc[] = {
	{ "dokidoki penguin land (japan).bin",	0x08000, 0x346556b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dokidoki)
STD_ROM_FN(sg1k_dokidoki)

struct BurnDriver BurnDrvsg1k_dokidoki = {
	"sg1k_dokidoki", NULL, NULL, NULL, "1985",
	"Dokidoki Penguin Land (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_dokidokiRomInfo, sg1k_dokidokiRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Qi E (Tw)

static struct BurnRomInfo sg1k_qieRomDesc[] = {
	{ "doki doki penguin land (19xx)(-)(tw).bin",	0x08000, 0xfdc095bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_qie)
STD_ROM_FN(sg1k_qie)

struct BurnDriver BurnDrvsg1k_qie = {
	"sg1k_qie", NULL, NULL, NULL, "1985?",
	"Qi E (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_qieRomInfo, sg1k_qieRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Dragon Wang (Jpn, v1)

static struct BurnRomInfo sg1k_dragwangRomDesc[] = {
	{ "dragon wang (japan).bin",	0x08000, 0x99c3de21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dragwang)
STD_ROM_FN(sg1k_dragwang)

struct BurnDriver BurnDrvsg1k_dragwang = {
	"sg1k_dragwang", NULL, NULL, NULL, "1985",
	"Dragon Wang (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_dragwangRomInfo, sg1k_dragwangRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Jing Wu Men (Tw)

static struct BurnRomInfo sg1k_jingwumnRomDesc[] = {
	{ "dragon wang (tw).bin",	0x08000, 0x7c7d4397, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jingwumn)
STD_ROM_FN(sg1k_jingwumn)

struct BurnDriver BurnDrvsg1k_jingwumn = {
	"sg1k_jingwumn", NULL, NULL, NULL, "1985?",
	"Jing Wu Men (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jingwumnRomInfo, sg1k_jingwumnRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Jing Wu Men (Tw, Alt)

static struct BurnRomInfo sg1k_jingwumnaRomDesc[] = {
	{ "dragon wang [v0] [english logo] (tw).bin",	0x08000, 0x6f94e5c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jingwumna)
STD_ROM_FN(sg1k_jingwumna)

struct BurnDriver BurnDrvsg1k_jingwumna = {
	"sg1k_jingwumna", NULL, NULL, NULL, "1985?",
	"Jing Wu Men (Tw, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jingwumnaRomInfo, sg1k_jingwumnaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Dragon Wang (Jpn, v0)

static struct BurnRomInfo sg1k_dragwang1RomDesc[] = {
	{ "dragon wang (japan) (alt).bin",	0x08000, 0x60f30138, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_dragwang1)
STD_ROM_FN(sg1k_dragwang1)

struct BurnDriver BurnDrvsg1k_dragwang1 = {
	"sg1k_dragwang1", NULL, NULL, NULL, "1985",
	"Dragon Wang (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_dragwang1RomInfo, sg1k_dragwang1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Drol (Jpn)

static struct BurnRomInfo sg1k_drolRomDesc[] = {
	{ "drol (japan).bin",	0x08000, 0x288940cb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drol)
STD_ROM_FN(sg1k_drol)

struct BurnDriver BurnDrvsg1k_drol = {
	"sg1k_drol", NULL, NULL, NULL, "1985",
	"Drol (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_drolRomInfo, sg1k_drolRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Drol (Tw)

static struct BurnRomInfo sg1k_droltRomDesc[] = {
	{ "drol (tw).bin",	0x08000, 0xb7fc033d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drolt)
STD_ROM_FN(sg1k_drolt)

struct BurnDriver BurnDrvsg1k_drolt = {
	"sg1k_drolt", NULL, NULL, NULL, "1985?",
	"Drol (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_droltRomInfo, sg1k_droltRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Drol (Kor)

static struct BurnRomInfo sg1k_drolkRomDesc[] = {
	{ "drol (kr).bin",	0x08000, 0x1d7c53bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_drolk)
STD_ROM_FN(sg1k_drolk)

struct BurnDriver BurnDrvsg1k_drolk = {
	"sg1k_drolk", NULL, NULL, NULL, "198?",
	"Drol (Kor)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_drolkRomInfo, sg1k_drolkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Elevator Action (Jpn)

static struct BurnRomInfo sg1k_elevatorRomDesc[] = {
	{ "elevator action (japan).bin",	0x08000, 0x5af8f69d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_elevator)
STD_ROM_FN(sg1k_elevator)

struct BurnDriver BurnDrvsg1k_elevator = {
	"sg1k_elevator", NULL, NULL, NULL, "1985",
	"Elevator Action (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_elevatorRomInfo, sg1k_elevatorRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Die Dui Die (Tw)

static struct BurnRomInfo sg1k_elevatortRomDesc[] = {
	{ "elevator action (tw).bin",	0x08000, 0x6846e36d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_elevatort)
STD_ROM_FN(sg1k_elevatort)

struct BurnDriver BurnDrvsg1k_elevatort = {
	"sg1k_elevatort", NULL, NULL, NULL, "1985?",
	"Die Dui Die (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_elevatortRomInfo, sg1k_elevatortRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Exerion (Euro, Jpn)

static struct BurnRomInfo sg1k_exerionRomDesc[] = {
	{ "mpr-5692.ic1",	0x04000, 0xa2c45b61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exerion)
STD_ROM_FN(sg1k_exerion)

struct BurnDriver BurnDrvsg1k_exerion = {
	"sg1k_exerion", NULL, NULL, NULL, "1983",
	"Exerion (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_exerionRomInfo, sg1k_exerionRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Exerion (Tw)

static struct BurnRomInfo sg1k_exeriontRomDesc[] = {
	{ "exerion (tw).bin",	0x04000, 0xb5c84a32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exeriont)
STD_ROM_FN(sg1k_exeriont)

struct BurnDriver BurnDrvsg1k_exeriont = {
	"sg1k_exeriont", NULL, NULL, NULL, "1983?",
	"Exerion (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_exeriontRomInfo, sg1k_exeriontRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Exerion (Kor)

static struct BurnRomInfo sg1k_exerionkRomDesc[] = {
	{ "exerion (kr).bin",	0x08000, 0xfc628d0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_exerionk)
STD_ROM_FN(sg1k_exerionk)

struct BurnDriver BurnDrvsg1k_exerionk = {
	"sg1k_exerionk", NULL, NULL, NULL, "198?",
	"Exerion (Kor)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_exerionkRomInfo, sg1k_exerionkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Flicky (Jpn, v1)

static struct BurnRomInfo sg1k_flickyRomDesc[] = {
	{ "flicky [v1].bin",	0x08000, 0x26d2862c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flicky)
STD_ROM_FN(sg1k_flicky)

struct BurnDriver BurnDrvsg1k_flicky = {
	"sg1k_flicky", NULL, NULL, NULL, "1984",
	"Flicky (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_flickyRomInfo, sg1k_flickyRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Flicky (Jpn, v0)

static struct BurnRomInfo sg1k_flickyaRomDesc[] = {
	{ "flicky (japan).bin",	0x08000, 0xbd24d27b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flickya)
STD_ROM_FN(sg1k_flickya)

struct BurnDriver BurnDrvsg1k_flickya = {
	"sg1k_flickya", NULL, NULL, NULL, "1984",
	"Flicky (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_flickyaRomInfo, sg1k_flickyaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Flicky (Tw)

static struct BurnRomInfo sg1k_flickytRomDesc[] = {
	{ "flicky (tw).bin",	0x08000, 0xcd0666a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_flickyt)
STD_ROM_FN(sg1k_flickyt)

struct BurnDriver BurnDrvsg1k_flickyt = {
	"sg1k_flickyt", NULL, NULL, NULL, "1984?",
	"Flicky (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_flickytRomInfo, sg1k_flickytRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Girl's Garden (Jpn)

static struct BurnRomInfo sg1k_girlgardRomDesc[] = {
	{ "girl's garden (japan).bin",	0x08000, 0x1898f274, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_girlgard)
STD_ROM_FN(sg1k_girlgard)

struct BurnDriver BurnDrvsg1k_girlgard = {
	"sg1k_girlgard", NULL, NULL, NULL, "1984",
	"Girl's Garden (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_girlgardRomInfo, sg1k_girlgardRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Girl's Garden (Tw)

static struct BurnRomInfo sg1k_girlgardtRomDesc[] = {
	{ "girl's garden (tw).bin",	0x08000, 0xb9635ac4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_girlgardt)
STD_ROM_FN(sg1k_girlgardt)

struct BurnDriver BurnDrvsg1k_girlgardt = {
	"sg1k_girlgardt", NULL, NULL, NULL, "1984?",
	"Girl's Garden (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_girlgardtRomInfo, sg1k_girlgardtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Golgo 13 (Jpn)

static struct BurnRomInfo sg1k_golgo13RomDesc[] = {
	{ "golgo 13 (japan).bin",	0x08000, 0x0d159ed0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_golgo13)
STD_ROM_FN(sg1k_golgo13)

struct BurnDriver BurnDrvsg1k_golgo13 = {
	"sg1k_golgo13", NULL, NULL, NULL, "1984",
	"Golgo 13 (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_golgo13RomInfo, sg1k_golgo13RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// GP World (Jpn, v1)

static struct BurnRomInfo sg1k_gpworldRomDesc[] = {
	{ "gp world (japan).bin",	0x08000, 0x191ffe0a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworld)
STD_ROM_FN(sg1k_gpworld)

struct BurnDriver BurnDrvsg1k_gpworld = {
	"sg1k_gpworld", NULL, NULL, NULL, "1985",
	"GP World (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_gpworldRomInfo, sg1k_gpworldRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// GP World (Jpn, v0)

static struct BurnRomInfo sg1k_gpworldaRomDesc[] = {
	{ "mpr-6485.ic1",	0x08000, 0x942adf84, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworlda)
STD_ROM_FN(sg1k_gpworlda)

struct BurnDriver BurnDrvsg1k_gpworlda = {
	"sg1k_gpworlda", NULL, NULL, NULL, "1985",
	"GP World (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_gpworldaRomInfo, sg1k_gpworldaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// GP World (Tw)

static struct BurnRomInfo sg1k_gpworldtRomDesc[] = {
	{ "gp world (tw).bin",	0x08000, 0xf19f7548, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gpworldt)
STD_ROM_FN(sg1k_gpworldt)

struct BurnDriver BurnDrvsg1k_gpworldt = {
	"sg1k_gpworldt", NULL, NULL, NULL, "1985?",
	"GP World (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_gpworldtRomInfo, sg1k_gpworldtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Gulkave (Jpn)

static struct BurnRomInfo sg1k_gulkaveRomDesc[] = {
	{ "gulkave (japan).bin",	0x08000, 0x15a754a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gulkave)
STD_ROM_FN(sg1k_gulkave)

struct BurnDriver BurnDrvsg1k_gulkave = {
	"sg1k_gulkave", NULL, NULL, NULL, "1986",
	"Gulkave (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_gulkaveRomInfo, sg1k_gulkaveRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Gulkave (Kor)

static struct BurnRomInfo sg1k_gulkavekRomDesc[] = {
	{ "gulkave (kr).bin",	0x08000, 0xddcb708b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_gulkavek)
STD_ROM_FN(sg1k_gulkavek)

struct BurnDriver BurnDrvsg1k_gulkavek = {
	"sg1k_gulkavek", NULL, NULL, NULL, "198?",
	"Gulkave (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_gulkavekRomInfo, sg1k_gulkavekRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Guzzler (Jpn, OMV)

static struct BurnRomInfo sg1k_guzzlerRomDesc[] = {
	{ "guzzler (japan) (om).bin",	0x02000, 0x61fa9ea0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_guzzler)
STD_ROM_FN(sg1k_guzzler)

struct BurnDriver BurnDrvsg1k_guzzler = {
	"sg1k_guzzler", NULL, NULL, NULL, "1983",
	"Guzzler (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_guzzlerRomInfo, sg1k_guzzlerRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Guzzler (Tw)

static struct BurnRomInfo sg1k_guzzlertRomDesc[] = {
	{ "guzzler (tw).bin",	0x02000, 0xeb808158, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_guzzlert)
STD_ROM_FN(sg1k_guzzlert)

struct BurnDriver BurnDrvsg1k_guzzlert = {
	"sg1k_guzzlert", NULL, NULL, NULL, "1983?",
	"Guzzler (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_guzzlertRomInfo, sg1k_guzzlertRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hang-On II (Jpn)

static struct BurnRomInfo sg1k_hangon2RomDesc[] = {
	{ "hang-on ii (japan).bin",	0x08000, 0x9be3c6bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2)
STD_ROM_FN(sg1k_hangon2)

struct BurnDriver BurnDrvsg1k_hangon2 = {
	"sg1k_hangon2", NULL, NULL, NULL, "1985",
	"Hang-On II (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hangon2RomInfo, sg1k_hangon2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hang-On II (Tw)

static struct BurnRomInfo sg1k_hangon2t1RomDesc[] = {
	{ "hang-on ii [english logo] (tw).bin",	0x08000, 0xcabd451b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2t1)
STD_ROM_FN(sg1k_hangon2t1)

struct BurnDriver BurnDrvsg1k_hangon2t1 = {
	"sg1k_hangon2t1", NULL, NULL, NULL, "1985?",
	"Hang-On II (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hangon2t1RomInfo, sg1k_hangon2t1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Motuo Feiche (Tw)

static struct BurnRomInfo sg1k_hangon2t2RomDesc[] = {
	{ "hang-on ii [chinese logo] (tw).bin",	0x08000, 0xe98a111e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hangon2t2)
STD_ROM_FN(sg1k_hangon2t2)

struct BurnDriver BurnDrvsg1k_hangon2t2 = {
	"sg1k_hangon2t2", NULL, NULL, NULL, "1985?",
	"Motuo Feiche (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hangon2t2RomInfo, sg1k_hangon2t2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// H.E.R.O. (Jpn)

static struct BurnRomInfo sg1k_heroRomDesc[] = {
	{ "h.e.r.o. (japan).bin",	0x08000, 0x4587de6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hero)
STD_ROM_FN(sg1k_hero)

struct BurnDriver BurnDrvsg1k_hero = {
	"sg1k_hero", NULL, NULL, NULL, "1985",
	"H.E.R.O. (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_heroRomInfo, sg1k_heroRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Qing Feng Xia (Tw)

static struct BurnRomInfo sg1k_herotRomDesc[] = {
	{ "h.e.r.o. (tw).bin",	0x08000, 0x83958998, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_herot)
STD_ROM_FN(sg1k_herot)

struct BurnDriver BurnDrvsg1k_herot = {
	"sg1k_herot", NULL, NULL, NULL, "1985?",
	"Qing Feng Xia (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_herotRomInfo, sg1k_herotRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Home Mahjong (Jpn, v1)

static struct BurnRomInfo sg1k_homemjRomDesc[] = {
	{ "home mahjong (japan) (v1.1).bin",	0x0c000, 0xe7e0f0e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemj)
STD_ROM_FN(sg1k_homemj)

struct BurnDriver BurnDrvsg1k_homemj = {
	"sg1k_homemj", NULL, NULL, NULL, "1984",
	"Home Mahjong (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_homemjRomInfo, sg1k_homemjRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Home Mahjong (Jpn, v0)

static struct BurnRomInfo sg1k_homemj1RomDesc[] = {
	{ "home mahjong (japan) (v1.0).bin",	0x0c000, 0xc9d1ae7d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemj1)
STD_ROM_FN(sg1k_homemj1)

struct BurnDriver BurnDrvsg1k_homemj1 = {
	"sg1k_homemj1", NULL, NULL, NULL, "1984",
	"Home Mahjong (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_homemj1RomInfo, sg1k_homemj1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yon-nin Mahjong (Tw)

static struct BurnRomInfo sg1k_homemjtRomDesc[] = {
	{ "home mahjong (tw).bin",	0x0c000, 0x0583a9fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homemjt)
STD_ROM_FN(sg1k_homemjt)

struct BurnDriver BurnDrvsg1k_homemjt = {
	"sg1k_homemjt", NULL, NULL, NULL, "1984?",
	"Yon-nin Mahjong (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_homemjtRomInfo, sg1k_homemjtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hustle Chumy (Jpn)

static struct BurnRomInfo sg1k_hustleRomDesc[] = {
	{ "hustle chumy (japan).bin",	0x04000, 0xa627d440, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hustle)
STD_ROM_FN(sg1k_hustle)

struct BurnDriver BurnDrvsg1k_hustle = {
	"sg1k_hustle", NULL, NULL, NULL, "1984",
	"Hustle Chumy (Jpn)\0", NULL, "Compile", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hustleRomInfo, sg1k_hustleRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hyper Sports (Jpn)

static struct BurnRomInfo sg1k_hypersptRomDesc[] = {
	{ "mpr-6487.ic1",	0x08000, 0xba09a0fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hyperspt)
STD_ROM_FN(sg1k_hyperspt)

struct BurnDriver BurnDrvsg1k_hyperspt = {
	"sg1k_hyperspt", NULL, NULL, NULL, "1985",
	"Hyper Sports (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hypersptRomInfo, sg1k_hypersptRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hyper Sports (Tw)

static struct BurnRomInfo sg1k_hyperspttRomDesc[] = {
	{ "hyper sports (tw).bin",	0x08000, 0x87619ac2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersptt)
STD_ROM_FN(sg1k_hypersptt)

struct BurnDriver BurnDrvsg1k_hypersptt = {
	"sg1k_hypersptt", NULL, NULL, NULL, "1985?",
	"Hyper Sports (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hyperspttRomInfo, sg1k_hyperspttRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hyper Sports (Kor)

static struct BurnRomInfo sg1k_hypersptkRomDesc[] = {
	{ "hyper sports (kr).bin",	0x08000, 0x3157ef9c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersptk)
STD_ROM_FN(sg1k_hypersptk)

struct BurnDriver BurnDrvsg1k_hypersptk = {
	"sg1k_hypersptk", NULL, NULL, NULL, "198?",
	"Hyper Sports (Kor)\0", NULL, "Samsung", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hypersptkRomInfo, sg1k_hypersptkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hyper Sports 2 (Tw)

static struct BurnRomInfo sg1k_hypersp2RomDesc[] = {
	{ "hyper sports 2 (tw).bin",	0x08000, 0xb0234e12, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_hypersp2)
STD_ROM_FN(sg1k_hypersp2)

struct BurnDriver BurnDrvsg1k_hypersp2 = {
	"sg1k_hypersp2", NULL, NULL, NULL, "198?",
	"Hyper Sports 2 (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_hypersp2RomInfo, sg1k_hypersp2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// 007 James Bond (Jpn, v2.7, OMV)

static struct BurnRomInfo sg1k_jb007RomDesc[] = {
	{ "james bond 007 (japan) (v2.7) (om).bin",	0x04000, 0x90160849, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007)
STD_ROM_FN(sg1k_jb007)

struct BurnDriver BurnDrvsg1k_jb007 = {
	"sg1k_jb007", NULL, NULL, NULL, "1984",
	"007 James Bond (Jpn, v2.7, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jb007RomInfo, sg1k_jb007RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// 007 James Bond (Jpn, v2.6, OMV)

static struct BurnRomInfo sg1k_jb007aRomDesc[] = {
	{ "007 james bond [v2.6] [40k map].bin",	0x0a000, 0xa8b5b57f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007a)
STD_ROM_FN(sg1k_jb007a)

struct BurnDriver BurnDrvsg1k_jb007a = {
	"sg1k_jb007a", NULL, NULL, NULL, "1984",
	"007 James Bond (Jpn, v2.6, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jb007aRomInfo, sg1k_jb007aRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// 007 James Bond (Tw)

static struct BurnRomInfo sg1k_jb007tRomDesc[] = {
	{ "james bond 007 (tw).bin",	0x04000, 0x76d6c64d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jb007t)
STD_ROM_FN(sg1k_jb007t)

struct BurnDriver BurnDrvsg1k_jb007t = {
	"sg1k_jb007t", NULL, NULL, NULL, "1984?",
	"007 James Bond (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jb007tRomInfo, sg1k_jb007tRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Jinzita (Tw)

static struct BurnRomInfo sg1k_jinzitaRomDesc[] = {
	{ "king's valley (tw).bin",	0x08000, 0x223397a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_jinzita)
STD_ROM_FN(sg1k_jinzita)

struct BurnDriver BurnDrvsg1k_jinzita = {
	"sg1k_jinzita", NULL, NULL, NULL, "198?",
	"Jinzita (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_jinzitaRomInfo, sg1k_jinzitaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Lode Runner (Euro, Jpn)

static struct BurnRomInfo sg1k_ldrunRomDesc[] = {
	{ "mpr-5998.ic1",	0x08000, 0x00ed3970, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrun)
STD_ROM_FN(sg1k_ldrun)

struct BurnDriver BurnDrvsg1k_ldrun = {
	"sg1k_ldrun", NULL, NULL, NULL, "1984",
	"Lode Runner (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_ldrunRomInfo, sg1k_ldrunRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Lode Runner (Kor)

static struct BurnRomInfo sg1k_ldrunkRomDesc[] = {
	{ "lode runner (kr).bin",	0x08000, 0xe68bc7d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrunk)
STD_ROM_FN(sg1k_ldrunk)

struct BurnDriver BurnDrvsg1k_ldrunk = {
	"sg1k_ldrunk", NULL, NULL, NULL, "1984?",
	"Lode Runner (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_ldrunkRomInfo, sg1k_ldrunkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Lode Runner (Tw)

static struct BurnRomInfo sg1k_ldruntRomDesc[] = {
	{ "lode runner (tw).bin",	0x08000, 0xd953bdb7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ldrunt)
STD_ROM_FN(sg1k_ldrunt)

struct BurnDriver BurnDrvsg1k_ldrunt = {
	"sg1k_ldrunt", NULL, NULL, NULL, "1984?",
	"Lode Runner (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_ldruntRomInfo, sg1k_ldruntRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
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
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_m2cpRomInfo, sg1k_m2cpRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Magical Tree (Tw)

static struct BurnRomInfo sg1k_magtreeRomDesc[] = {
	{ "magical tree (tw).bin",	0x08000, 0xb3a8291a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_magtree)
STD_ROM_FN(sg1k_magtree)

struct BurnDriver BurnDrvsg1k_magtree = {
	"sg1k_magtree", NULL, NULL, NULL, "198?",
	"Magical Tree (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_magtreeRomInfo, sg1k_magtreeRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Magical Kid Wiz (Tw)

static struct BurnRomInfo sg1k_mkidwizRomDesc[] = {
	{ "magical kid wiz (tw).bin",	0x0c000, 0xffc4ee3f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mkidwiz)
STD_ROM_FN(sg1k_mkidwiz)

struct BurnDriver BurnDrvsg1k_mkidwiz = {
	"sg1k_mkidwiz", NULL, NULL, NULL, "198?",
	"Magical Kid Wiz (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_mkidwizRomInfo, sg1k_mkidwizRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Mahjong (Jpn)

static struct BurnRomInfo sg1k_mahjongRomDesc[] = {
	{ "mahjong (japan).bin",	0x06000, 0x6d909857, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjong)
STD_ROM_FN(sg1k_mahjong)

struct BurnDriver BurnDrvsg1k_mahjong = {
	"sg1k_mahjong", NULL, NULL, NULL, "1983",
	"Mahjong (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_mahjongRomInfo, sg1k_mahjongRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Mahjong (Jpn, Alt)

static struct BurnRomInfo sg1k_mahjongaRomDesc[] = {
	{ "mahjong [big 1983].bin",	0x06000, 0x1c137cab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjonga)
STD_ROM_FN(sg1k_mahjonga)

struct BurnDriver BurnDrvsg1k_mahjonga = {
	"sg1k_mahjonga", NULL, NULL, NULL, "1983",
	"Mahjong (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_mahjongaRomInfo, sg1k_mahjongaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Mahjong (Tw)

static struct BurnRomInfo sg1k_mahjongtRomDesc[] = {
	{ "mahjong (tw).bin",	0x06000, 0xbc823a89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_mahjongt)
STD_ROM_FN(sg1k_mahjongt)

struct BurnDriver BurnDrvsg1k_mahjongt = {
	"sg1k_mahjongt", NULL, NULL, NULL, "1983?",
	"Mahjong (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_mahjongtRomInfo, sg1k_mahjongtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Monaco GP (Jpn, v2)

static struct BurnRomInfo sg1k_monacogpRomDesc[] = {
	{ "monaco gp [32k] [v2].bin",	0x08000, 0x02e5d66a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogp)
STD_ROM_FN(sg1k_monacogp)

struct BurnDriver BurnDrvsg1k_monacogp = {
	"sg1k_monacogp", NULL, NULL, NULL, "1983",
	"Monaco GP (Jpn, v2)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_monacogpRomInfo, sg1k_monacogpRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Monaco GP (Jpn, v1)

static struct BurnRomInfo sg1k_monacogpaRomDesc[] = {
	{ "monaco gp [32k].bin",	0x08000, 0xda2d57f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpa)
STD_ROM_FN(sg1k_monacogpa)

struct BurnDriver BurnDrvsg1k_monacogpa = {
	"sg1k_monacogpa", NULL, NULL, NULL, "1983",
	"Monaco GP (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_monacogpaRomInfo, sg1k_monacogpaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Monaco GP (Jpn, v0)

static struct BurnRomInfo sg1k_monacogpbRomDesc[] = {
	{ "monaco gp [24k].bin",	0x0a000, 0x8572d73a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpb)
STD_ROM_FN(sg1k_monacogpb)

struct BurnDriver BurnDrvsg1k_monacogpb = {
	"sg1k_monacogpb", NULL, NULL, NULL, "1983",
	"Monaco GP (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_monacogpbRomInfo, sg1k_monacogpbRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Monaco GP (Tw)

static struct BurnRomInfo sg1k_monacogptRomDesc[] = {
	{ "monaco gp (tw).bin",	0x08000, 0x01cda679, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_monacogpt)
STD_ROM_FN(sg1k_monacogpt)

struct BurnDriver BurnDrvsg1k_monacogpt = {
	"sg1k_monacogpt", NULL, NULL, NULL, "1983?",
	"Monaco GP (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_monacogptRomInfo, sg1k_monacogptRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Moyu Chuanqi (Tw)

static struct BurnRomInfo sg1k_moyuchuaRomDesc[] = {
	{ "knightmare [jumbo] (tw).bin",	0x0c000, 0x281d2888, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_moyuchua)
STD_ROM_FN(sg1k_moyuchua)

struct BurnDriver BurnDrvsg1k_moyuchua = {
	"sg1k_moyuchua", NULL, NULL, NULL, "1986?",
	"Moyu Chuanqi (Tw)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_moyuchuaRomInfo, sg1k_moyuchuaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Ninja Princess (Jpn)

static struct BurnRomInfo sg1k_ninjapriRomDesc[] = {
	{ "ninja princess (japan).bin",	0x08000, 0x3b912408, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ninjapri)
STD_ROM_FN(sg1k_ninjapri)

struct BurnDriver BurnDrvsg1k_ninjapri = {
	"sg1k_ninjapri", NULL, NULL, NULL, "1986",
	"Ninja Princess (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_ninjapriRomInfo, sg1k_ninjapriRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Renzhe Gongzhu (Tw)

static struct BurnRomInfo sg1k_ninjapritRomDesc[] = {
	{ "ninja princess (tw).bin",	0x08000, 0x464d144b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_ninjaprit)
STD_ROM_FN(sg1k_ninjaprit)

struct BurnDriver BurnDrvsg1k_ninjaprit = {
	"sg1k_ninjaprit", NULL, NULL, NULL, "1986?",
	"Renzhe Gongzhu (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_ninjapritRomInfo, sg1k_ninjapritRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
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
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_nsubRomInfo, sg1k_nsubRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// N-Sub (Jpn)

static struct BurnRomInfo sg1k_nsubaRomDesc[] = {
	{ "n-sub [16k].bin",	0x04000, 0x652bbd1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsuba)
STD_ROM_FN(sg1k_nsuba)

struct BurnDriver BurnDrvsg1k_nsuba = {
	"sg1k_nsuba", NULL, NULL, NULL, "1983",
	"N-Sub (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_nsubaRomInfo, sg1k_nsubaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// N-Sub (Jpn, Alt)

static struct BurnRomInfo sg1k_nsubbRomDesc[] = {
	{ "n-sub [40k].bin",	0x0a000, 0xb377d6e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsubb)
STD_ROM_FN(sg1k_nsubb)

struct BurnDriver BurnDrvsg1k_nsubb = {
	"sg1k_nsubb", NULL, NULL, NULL, "1983",
	"N-Sub (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_nsubbRomInfo, sg1k_nsubbRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// N-Sub (Tw)

static struct BurnRomInfo sg1k_nsubtRomDesc[] = {
	{ "n-sub (tw).bin",	0x04000, 0x3e371769, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nsubt)
STD_ROM_FN(sg1k_nsubt)

struct BurnDriver BurnDrvsg1k_nsubt = {
	"sg1k_nsubt", NULL, NULL, NULL, "1983?",
	"N-Sub (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_nsubtRomInfo, sg1k_nsubtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Okamoto Ayako no Match Play Golf (Jpn, OMV)

static struct BurnRomInfo sg1k_matchpgRomDesc[] = {
	{ "okamoto ayako no match play golf [b] (jp).bin",	0x08000, 0x49d3db2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpg)
STD_ROM_FN(sg1k_matchpg)

struct BurnDriver BurnDrvsg1k_matchpg = {
	"sg1k_matchpg", NULL, NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_matchpgRomInfo, sg1k_matchpgRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Okamoto Ayako no Match Play Golf (Jpn, OMV, Alt)

static struct BurnRomInfo sg1k_matchpgaRomDesc[] = {
	{ "okamoto ayako no match play golf [a] (jp).bin",	0x08000, 0x547dd7fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpga)
STD_ROM_FN(sg1k_matchpga)

struct BurnDriver BurnDrvsg1k_matchpga = {
	"sg1k_matchpga", NULL, NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Jpn, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_matchpgaRomInfo, sg1k_matchpgaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Okamoto Ayako no Match Play Golf (Tw)

static struct BurnRomInfo sg1k_matchpgtRomDesc[] = {
	{ "okamoto ayako no match play golf (tw).bin",	0x08000, 0xb60492d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_matchpgt)
STD_ROM_FN(sg1k_matchpgt)

struct BurnDriver BurnDrvsg1k_matchpgt = {
	"sg1k_matchpgt", NULL, NULL, NULL, "1984?",
	"Okamoto Ayako no Match Play Golf (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_matchpgtRomInfo, sg1k_matchpgtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Orguss (Euro, Jpn)

static struct BurnRomInfo sg1k_orgussRomDesc[] = {
	{ "mpr-5743.ic1",	0x08000, 0xf4f78b76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_orguss)
STD_ROM_FN(sg1k_orguss)

struct BurnDriver BurnDrvsg1k_orguss = {
	"sg1k_orguss", NULL, NULL, NULL, "1984",
	"Orguss (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_orgussRomInfo, sg1k_orgussRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Othello (Jpn)

static struct BurnRomInfo sg1k_othelloRomDesc[] = {
	{ "othello (japan).bin",	0x08000, 0xaf4f14bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_othello)
STD_ROM_FN(sg1k_othello)

struct BurnDriver BurnDrvsg1k_othello = {
	"sg1k_othello", NULL, NULL, NULL, "1985",
	"Othello (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_othelloRomInfo, sg1k_othelloRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Hei Bai Qi (Tw)

static struct BurnRomInfo sg1k_heibaiqiRomDesc[] = {
	{ "othello (tw).bin",	0x08000, 0x1d1a0ca3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_heibaiqi)
STD_ROM_FN(sg1k_heibaiqi)

struct BurnDriver BurnDrvsg1k_heibaiqi = {
	"sg1k_heibaiqi", NULL, NULL, NULL, "1985?",
	"Hei Bai Qi (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_heibaiqiRomInfo, sg1k_heibaiqiRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pacar (Euro, Jpn)

static struct BurnRomInfo sg1k_pacarRomDesc[] = {
	{ "mpr-5556.ic1",	0x04000, 0x30c52e5e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacar)
STD_ROM_FN(sg1k_pacar)

struct BurnDriver BurnDrvsg1k_pacar = {
	"sg1k_pacar", NULL, NULL, NULL, "1983",
	"Pacar (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pacarRomInfo, sg1k_pacarRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pacar (Jpn, Alt)

static struct BurnRomInfo sg1k_pacaraRomDesc[] = {
	{ "pacar [40k map].bin",	0x0a000, 0x19949375, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacara)
STD_ROM_FN(sg1k_pacara)

struct BurnDriver BurnDrvsg1k_pacara = {
	"sg1k_pacara", NULL, NULL, NULL, "1983",
	"Pacar (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pacaraRomInfo, sg1k_pacaraRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pacar (Tw)

static struct BurnRomInfo sg1k_pacartRomDesc[] = {
	{ "pacar (tw).bin",	0x04000, 0xdd6817a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pacart)
STD_ROM_FN(sg1k_pacart)

struct BurnDriver BurnDrvsg1k_pacart = {
	"sg1k_pacart", NULL, NULL, NULL, "1983?",
	"Pacar (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pacartRomInfo, sg1k_pacartRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pachinko II (Jpn)

static struct BurnRomInfo sg1k_pachink2RomDesc[] = {
	{ "pachinko ii (japan).bin",	0x04000, 0xfd7cb50a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachink2)
STD_ROM_FN(sg1k_pachink2)

struct BurnDriver BurnDrvsg1k_pachink2 = {
	"sg1k_pachink2", NULL, NULL, NULL, "1984",
	"Pachinko II (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachink2RomInfo, sg1k_pachink2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pachinko II (Tw)

static struct BurnRomInfo sg1k_pachink2tRomDesc[] = {
	{ "pachinko ii (tw).bin",	0x04000, 0x6ebe81bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachink2t)
STD_ROM_FN(sg1k_pachink2t)

struct BurnDriver BurnDrvsg1k_pachink2t = {
	"sg1k_pachink2t", NULL, NULL, NULL, "1984?",
	"Pachinko II (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachink2tRomInfo, sg1k_pachink2tRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pachinko (Jpn)

static struct BurnRomInfo sg1k_pachinkoRomDesc[] = {
	{ "pachinko (japan).bin",	0x0a000, 0x326587e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pachinko)
STD_ROM_FN(sg1k_pachinko)

struct BurnDriver BurnDrvsg1k_pachinko = {
	"sg1k_pachinko", NULL, NULL, NULL, "1983",
	"Pachinko (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pachinkoRomInfo, sg1k_pachinkoRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Ping Pong (Tw)

static struct BurnRomInfo sg1k_pingpongRomDesc[] = {
	{ "ping pong (konami's) (tw).bin",	0x08000, 0x4d972a9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pingpong)
STD_ROM_FN(sg1k_pingpong)

struct BurnDriver BurnDrvsg1k_pingpong = {
	"sg1k_pingpong", NULL, NULL, NULL, "1985?",
	"Ping Pong (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pingpongRomInfo, sg1k_pingpongRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pitfall II - The Lost Caverns (Jpn, v0)

static struct BurnRomInfo sg1k_pitfall2aRomDesc[] = {
	{ "pitfall ii - the lost caverns (japan).bin",	0x08000, 0x37fca2eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pitfall2a)
STD_ROM_FN(sg1k_pitfall2a)

struct BurnDriver BurnDrvsg1k_pitfall2a = {
	"sg1k_pitfall2a", NULL, NULL, NULL, "1985",
	"Pitfall II - The Lost Caverns (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pitfall2aRomInfo, sg1k_pitfall2aRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pitfall II - The Lost Caverns (Jpn, v1)

static struct BurnRomInfo sg1k_pitfall2RomDesc[] = {
	{ "pitfall ii - the lost caverns (japan) (alt).bin",	0x08000, 0x3db74761, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_pitfall2)
STD_ROM_FN(sg1k_pitfall2)

struct BurnDriver BurnDrvsg1k_pitfall2 = {
	"sg1k_pitfall2", NULL, NULL, NULL, "1985",
	"Pitfall II - The Lost Caverns (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_pitfall2RomInfo, sg1k_pitfall2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Fa Gui Qibing (Tw)

static struct BurnRomInfo sg1k_faguiqibRomDesc[] = {
	{ "pitfall ii [v0] [chinese logo] (tw).bin",	0x08000, 0x476a079b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_faguiqib)
STD_ROM_FN(sg1k_faguiqib)

struct BurnDriver BurnDrvsg1k_faguiqib = {
	"sg1k_faguiqib", NULL, NULL, NULL, "1985?",
	"Fa Gui Qibing (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_faguiqibRomInfo, sg1k_faguiqibRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Fa Gui Qibing (Tw, Alt)

static struct BurnRomInfo sg1k_faguiqibaRomDesc[] = {
	{ "pitfall ii [v0] [english logo] (tw).bin",	0x08000, 0x4e93bc8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_faguiqiba)
STD_ROM_FN(sg1k_faguiqiba)

struct BurnDriver BurnDrvsg1k_faguiqiba = {
	"sg1k_faguiqiba", NULL, NULL, NULL, "1985?",
	"Fa Gui Qibing (Tw, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_faguiqibaRomInfo, sg1k_faguiqibaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pop Flamer (Euro, Jpn)

static struct BurnRomInfo sg1k_popflameRomDesc[] = {
	{ "pop flamer (japan, europe).bin",	0x04000, 0xdb6404ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_popflame)
STD_ROM_FN(sg1k_popflame)

struct BurnDriver BurnDrvsg1k_popflame = {
	"sg1k_popflame", NULL, NULL, NULL, "1983",
	"Pop Flamer (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_popflameRomInfo, sg1k_popflameRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Pop Flamer (Tw)

static struct BurnRomInfo sg1k_popflametRomDesc[] = {
	{ "pop flamer (tw).bin",	0x04000, 0xab1da8a6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_popflamet)
STD_ROM_FN(sg1k_popflamet)

struct BurnDriver BurnDrvsg1k_popflamet = {
	"sg1k_popflamet", NULL, NULL, NULL, "1983?",
	"Pop Flamer (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_popflametRomInfo, sg1k_popflametRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Q*bert (Jpn, OMV)

static struct BurnRomInfo sg1k_qbertRomDesc[] = {
	{ "q-bert (japan) (om).bin",	0x02000, 0x77db4704, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_qbert)
STD_ROM_FN(sg1k_qbert)

struct BurnDriver BurnDrvsg1k_qbert = {
	"sg1k_qbert", NULL, NULL, NULL, "1983",
	"Q*bert (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_qbertRomInfo, sg1k_qbertRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Rally-X (Tw)

static struct BurnRomInfo sg1k_rallyxRomDesc[] = {
	{ "rally-x (tw).bin",	0x08000, 0xaaac12cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_rallyx)
STD_ROM_FN(sg1k_rallyx)

struct BurnDriver BurnDrvsg1k_rallyx = {
	"sg1k_rallyx", NULL, NULL, NULL, "1986?",
	"Rally-X (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_rallyxRomInfo, sg1k_rallyxRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Du Du Che (Tw)

static struct BurnRomInfo sg1k_duducheRomDesc[] = {
	{ "rally-x [dahjee] (tw).bin",	0x08000, 0x306d5f78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_duduche)
STD_ROM_FN(sg1k_duduche)

struct BurnDriver BurnDrvsg1k_duduche = {
	"sg1k_duduche", NULL, NULL, NULL, "1986?",
	"Du Du Che (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_duducheRomInfo, sg1k_duducheRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Road Fighter (Tw)

static struct BurnRomInfo sg1k_roadfghtRomDesc[] = {
	{ "road fighter (tw).bin",	0x08000, 0xd2edd329, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_roadfght)
STD_ROM_FN(sg1k_roadfght)

struct BurnDriver BurnDrvsg1k_roadfght = {
	"sg1k_roadfght", NULL, NULL, NULL, "1986?",
	"Road Fighter (Tw)\0", NULL, "Jumbo?", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_roadfghtRomInfo, sg1k_roadfghtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Huojian Che (Tw)

static struct BurnRomInfo sg1k_huojicheRomDesc[] = {
	{ "road fighter [jumbo] (tw).bin",	0x08000, 0x29e047cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_huojiche)
STD_ROM_FN(sg1k_huojiche)

struct BurnDriver BurnDrvsg1k_huojiche = {
	"sg1k_huojiche", NULL, NULL, NULL, "1986?",
	"Huojian Che (Tw)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_huojicheRomInfo, sg1k_huojicheRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Rock n' Bolt (Jpn)

static struct BurnRomInfo sg1k_rocknbolRomDesc[] = {
	{ "rock n' bolt (japan).bin",	0x08000, 0x0ffdd03d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_rocknbol)
STD_ROM_FN(sg1k_rocknbol)

struct BurnDriver BurnDrvsg1k_rocknbol = {
	"sg1k_rocknbol", NULL, NULL, NULL, "1985",
	"Rock n' Bolt (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_rocknbolRomInfo, sg1k_rocknbolRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Mo Tian Da Lou (Tw)

static struct BurnRomInfo sg1k_motiandaRomDesc[] = {
	{ "rock n' bolt (tw).bin",	0x08000, 0x4eacb981, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_motianda)
STD_ROM_FN(sg1k_motianda)

struct BurnDriver BurnDrvsg1k_motianda = {
	"sg1k_motianda", NULL, NULL, NULL, "1985?",
	"Mo Tian Da Lou (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_motiandaRomInfo, sg1k_motiandaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Safari Hunting (Jpn)

static struct BurnRomInfo sg1k_safarihuRomDesc[] = {
	{ "safari hunting (japan).bin",	0x04000, 0x49e9718b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarihu)
STD_ROM_FN(sg1k_safarihu)

struct BurnDriver BurnDrvsg1k_safarihu = {
	"sg1k_safarihu", NULL, NULL, NULL, "1983",
	"Safari Hunting (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_safarihuRomInfo, sg1k_safarihuRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Safari Hunting (Tw)

static struct BurnRomInfo sg1k_safarihutRomDesc[] = {
	{ "safari hunting (tw).bin",	0x04000, 0x6dc51c01, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarihut)
STD_ROM_FN(sg1k_safarihut)

struct BurnDriver BurnDrvsg1k_safarihut = {
	"sg1k_safarihut", NULL, NULL, NULL, "1983?",
	"Safari Hunting (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_safarihutRomInfo, sg1k_safarihutRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Safari Race (Euro?)

static struct BurnRomInfo sg1k_safarircRomDesc[] = {
	{ "safari race [1988].bin",	0x08000, 0x619dd066, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarirc)
STD_ROM_FN(sg1k_safarirc)

struct BurnDriver BurnDrvsg1k_safarirc = {
	"sg1k_safarirc", NULL, NULL, NULL, "1988",
	"Safari Race (Euro?)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_safarircRomInfo, sg1k_safarircRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Safari Race (Jpn)

static struct BurnRomInfo sg1k_safarircjRomDesc[] = {
	{ "mpr-5977.ic1",	0x08000, 0x08707fe3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarircj)
STD_ROM_FN(sg1k_safarircj)

struct BurnDriver BurnDrvsg1k_safarircj = {
	"sg1k_safarircj", NULL, NULL, NULL, "1984",
	"Safari Race (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_safarircjRomInfo, sg1k_safarircjRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Safari Race (Tw)

static struct BurnRomInfo sg1k_safarirctRomDesc[] = {
	{ "safari race (tw).bin",	0x08000, 0xb2724428, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_safarirct)
STD_ROM_FN(sg1k_safarirct)

struct BurnDriver BurnDrvsg1k_safarirct = {
	"sg1k_safarirct", NULL, NULL, NULL, "1984?",
	"Safari Race (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_safarirctRomInfo, sg1k_safarirctRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega Flipper (Euro, Jpn)

static struct BurnRomInfo sg1k_segaflipRomDesc[] = {
	{ "sega flipper (japan, europe).bin",	0x04000, 0x8efc77bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflip)
STD_ROM_FN(sg1k_segaflip)

struct BurnDriver BurnDrvsg1k_segaflip = {
	"sg1k_segaflip", NULL, NULL, NULL, "1983",
	"Sega Flipper (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_segaflipRomInfo, sg1k_segaflipRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega Flipper (Jpn, Alt)

static struct BurnRomInfo sg1k_segaflipaRomDesc[] = {
	{ "sega flipper [40k map].bin",	0x0a000, 0xfd76ad99, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflipa)
STD_ROM_FN(sg1k_segaflipa)

struct BurnDriver BurnDrvsg1k_segaflipa = {
	"sg1k_segaflipa", NULL, NULL, NULL, "1983",
	"Sega Flipper (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_segaflipaRomInfo, sg1k_segaflipaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Flipper (Tw)

static struct BurnRomInfo sg1k_segafliptRomDesc[] = {
	{ "sega flipper (tw).bin",	0x04000, 0x042c36ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segaflipt)
STD_ROM_FN(sg1k_segaflipt)

struct BurnDriver BurnDrvsg1k_segaflipt = {
	"sg1k_segaflipt", NULL, NULL, NULL, "1984?",
	"Flipper (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_segafliptRomInfo, sg1k_segafliptRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega-Galaga (Jpn)

static struct BurnRomInfo sg1k_segagalaRomDesc[] = {
	{ "sega-galaga (japan).bin",	0x04000, 0x981e36c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segagala)
STD_ROM_FN(sg1k_segagala)

struct BurnDriver BurnDrvsg1k_segagala = {
	"sg1k_segagala", NULL, NULL, NULL, "1983",
	"Sega-Galaga (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_segagalaRomInfo, sg1k_segagalaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega-Galaga (Jpn, Alt)

static struct BurnRomInfo sg1k_segagala1RomDesc[] = {
	{ "sega-galaga [40k map].bin",	0x0a000, 0x31283003, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_segagala1)
STD_ROM_FN(sg1k_segagala1)

struct BurnDriver BurnDrvsg1k_segagala1 = {
	"sg1k_segagala1", NULL, NULL, NULL, "1983",
	"Sega-Galaga (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_segagala1RomInfo, sg1k_segagala1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Galaga (Tw)

static struct BurnRomInfo sg1k_galagaRomDesc[] = {
	{ "galaga (tw).bin",	0x04000, 0x845bbb22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_galaga)
STD_ROM_FN(sg1k_galaga)

struct BurnDriver BurnDrvsg1k_galaga = {
	"sg1k_galaga", NULL, NULL, NULL, "1983?",
	"Galaga (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_galagaRomInfo, sg1k_galagaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Serizawa Hachidan no Tsumeshogi (Jpn)

static struct BurnRomInfo sg1k_serizawaRomDesc[] = {
	{ "serizawa hachidan no tsumeshogi (japan).bin",	0x04000, 0x545fc9bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_serizawa)
STD_ROM_FN(sg1k_serizawa)

struct BurnDriver BurnDrvsg1k_serizawa = {
	"sg1k_serizawa", NULL, NULL, NULL, "1983",
	"Serizawa Hachidan no Tsumeshogi (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_serizawaRomInfo, sg1k_serizawaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Shinnyushain Tooru Kun (Jpn)

static struct BurnRomInfo sg1k_shinnyusRomDesc[] = {
	{ "shinnyushain tooru kun (japan).bin",	0x08000, 0x5a917e06, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_shinnyus)
STD_ROM_FN(sg1k_shinnyus)

struct BurnDriver BurnDrvsg1k_shinnyus = {
	"sg1k_shinnyus", NULL, NULL, NULL, "1985",
	"Shinnyushain Tooru Kun (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_shinnyusRomInfo, sg1k_shinnyusRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Shinnyushain Tooru Kun (Tw)

static struct BurnRomInfo sg1k_shinnyustRomDesc[] = {
	{ "shinnyushain tooru kun (tw).bin",	0x08000, 0x33fc5cf6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_shinnyust)
STD_ROM_FN(sg1k_shinnyust)

struct BurnDriver BurnDrvsg1k_shinnyust = {
	"sg1k_shinnyust", NULL, NULL, NULL, "19??",
	"Shinnyushain Tooru Kun (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_shinnyustRomInfo, sg1k_shinnyustRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Shuangxiang Pao (Tw)

static struct BurnRomInfo sg1k_sxpaoRomDesc[] = {
	{ "twinbee [jumbo] (tw).bin",	0x0c000, 0xc550b4f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sxpao)
STD_ROM_FN(sg1k_sxpao)

struct BurnDriver BurnDrvsg1k_sxpao = {
	"sg1k_sxpao", NULL, NULL, NULL, "1986?",
	"Shuangxiang Pao (Tw)\0", NULL, "Jumbo", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sxpaoRomInfo, sg1k_sxpaoRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sindbad Mystery (Euro, Jpn)

static struct BurnRomInfo sg1k_sindbadmRomDesc[] = {
	{ "mpr-5546.ic1",	0x08000, 0x01932df9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sindbadm)
STD_ROM_FN(sg1k_sindbadm)

struct BurnDriver BurnDrvsg1k_sindbadm = {
	"sg1k_sindbadm", NULL, NULL, NULL, "1983",
	"Sindbad Mystery (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sindbadmRomInfo, sg1k_sindbadmRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sindbad Mystery (Tw)

static struct BurnRomInfo sg1k_sindbadmtRomDesc[] = {
	{ "sindbad mystery (tw).bin",	0x08000, 0x945f7847, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sindbadmt)
STD_ROM_FN(sg1k_sindbadmt)

struct BurnDriver BurnDrvsg1k_sindbadmt = {
	"sg1k_sindbadmt", NULL, NULL, NULL, "1983?",
	"Sindbad Mystery (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sindbadmtRomInfo, sg1k_sindbadmtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Soukoban (Jpn)

static struct BurnRomInfo sg1k_sokobanRomDesc[] = {
	{ "soukoban (japan).bin",	0x08000, 0x922c5468, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokoban)
STD_ROM_FN(sg1k_sokoban)

struct BurnDriver BurnDrvsg1k_sokoban = {
	"sg1k_sokoban", NULL, NULL, NULL, "1985",
	"Soukoban (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sokobanRomInfo, sg1k_sokobanRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Soukoban (Tw)

static struct BurnRomInfo sg1k_sokobantRomDesc[] = {
	{ "soukoban (tw).bin",	0x08000, 0xa2c3fc97, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokobant)
STD_ROM_FN(sg1k_sokobant)

struct BurnDriver BurnDrvsg1k_sokobant = {
	"sg1k_sokobant", NULL, NULL, NULL, "1985?",
	"Soukoban (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sokobantRomInfo, sg1k_sokobantRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Soukoban (Kor)

static struct BurnRomInfo sg1k_sokobankRomDesc[] = {
	{ "soukoban (kr).bin",	0x08000, 0x148ea752, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sokobank)
STD_ROM_FN(sg1k_sokobank)

struct BurnDriver BurnDrvsg1k_sokobank = {
	"sg1k_sokobank", NULL, NULL, NULL, "1985?",
	"Soukoban (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sokobankRomInfo, sg1k_sokobankRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// I.Q. (Kor)

static struct BurnRomInfo sg1k_iqRomDesc[] = {
	{ "soukoban [iq logo] (kr).bin",	0x08000, 0x89bafec5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_iq)
STD_ROM_FN(sg1k_iq)

struct BurnDriver BurnDrvsg1k_iq = {
	"sg1k_iq", NULL, NULL, NULL, "1985?",
	"I.Q. (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_iqRomInfo, sg1k_iqRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Armor (Jpn, v20, OMV)

static struct BurnRomInfo sg1k_spacearmRomDesc[] = {
	{ "space armor [v2].bin",	0x04000, 0xac4f0a5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearm)
STD_ROM_FN(sg1k_spacearm)

struct BurnDriver BurnDrvsg1k_spacearm = {
	"sg1k_spacearm", NULL, NULL, NULL, "1984",
	"Space Armor (Jpn, v20, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spacearmRomInfo, sg1k_spacearmRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Armor (Jpn, v20, OMV, Alt)

static struct BurnRomInfo sg1k_spacearmaRomDesc[] = {
	{ "space armor [v1].bin",	0x04000, 0xd5fdb4a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearma)
STD_ROM_FN(sg1k_spacearma)

struct BurnDriver BurnDrvsg1k_spacearma = {
	"sg1k_spacearma", NULL, NULL, NULL, "1984",
	"Space Armor (Jpn, v20, OMV, Alt)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spacearmaRomInfo, sg1k_spacearmaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Armor (Jpn, v10, OMV)

static struct BurnRomInfo sg1k_spacearmbRomDesc[] = {
	{ "space armor [v0] [40k map].bin",	0x0a000, 0xd23b0e3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacearmb)
STD_ROM_FN(sg1k_spacearmb)

struct BurnDriver BurnDrvsg1k_spacearmb = {
	"sg1k_spacearmb", NULL, NULL, NULL, "1984",
	"Space Armor (Jpn, v10, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spacearmbRomInfo, sg1k_spacearmbRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Invaders (Jpn)

static struct BurnRomInfo sg1k_spaceinvRomDesc[] = {
	{ "space invaders (japan).bin",	0x04000, 0x6ad5cb3d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spaceinv)
STD_ROM_FN(sg1k_spaceinv)

struct BurnDriver BurnDrvsg1k_spaceinv = {
	"sg1k_spaceinv", NULL, NULL, NULL, "1985",
	"Space Invaders (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spaceinvRomInfo, sg1k_spaceinvRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Invaders (Tw)

static struct BurnRomInfo sg1k_spaceinvtRomDesc[] = {
	{ "space invaders (tw).bin",	0x04000, 0x0760ea93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spaceinvt)
STD_ROM_FN(sg1k_spaceinvt)

struct BurnDriver BurnDrvsg1k_spaceinvt = {
	"sg1k_spaceinvt", NULL, NULL, NULL, "1985?",
	"Space Invaders (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spaceinvtRomInfo, sg1k_spaceinvtRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Mountain (Jpn, OMV)

static struct BurnRomInfo sg1k_spacemntRomDesc[] = {
	{ "space mountain (japan) (om).bin",	0x02000, 0xbbd87d8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacemnt)
STD_ROM_FN(sg1k_spacemnt)

struct BurnDriver BurnDrvsg1k_spacemnt = {
	"sg1k_spacemnt", NULL, NULL, NULL, "1983",
	"Space Mountain (Jpn, OMV)\0", NULL, "Tsukuda Original", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spacemntRomInfo, sg1k_spacemntRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Space Slalom (Jpn)

static struct BurnRomInfo sg1k_spaceslaRomDesc[] = {
	{ "space slalom (japan).bin",	0x02000, 0xb8b58b30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_spacesla)
STD_ROM_FN(sg1k_spacesla)

struct BurnDriver BurnDrvsg1k_spacesla = {
	"sg1k_spacesla", NULL, NULL, NULL, "1983",
	"Space Slalom (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_spaceslaRomInfo, sg1k_spaceslaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Force (Jpn)

static struct BurnRomInfo sg1k_starfrceRomDesc[] = {
	{ "star force (japan).bin",	0x08000, 0xb846b52a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrce)
STD_ROM_FN(sg1k_starfrce)

struct BurnDriver BurnDrvsg1k_starfrce = {
	"sg1k_starfrce", NULL, NULL, NULL, "1985",
	"Star Force (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starfrceRomInfo, sg1k_starfrceRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Force (Tw)

static struct BurnRomInfo sg1k_starfrcetRomDesc[] = {
	{ "star force (tw).bin",	0x08000, 0x1f736931, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrcet)
STD_ROM_FN(sg1k_starfrcet)

struct BurnDriver BurnDrvsg1k_starfrcet = {
	"sg1k_starfrcet", NULL, NULL, NULL, "1985?",
	"Star Force (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starfrcetRomInfo, sg1k_starfrcetRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Force (Tw, Alt)

static struct BurnRomInfo sg1k_starfrcetaRomDesc[] = {
	{ "star force [b] (tw).bin",	0x08000, 0x6f9b1ccd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starfrceta)
STD_ROM_FN(sg1k_starfrceta)

struct BurnDriver BurnDrvsg1k_starfrceta = {
	"sg1k_starfrceta", NULL, NULL, NULL, "1985?",
	"Star Force (Tw, Alt)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starfrcetaRomInfo, sg1k_starfrcetaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Jacker (Euro, Jpn, v1.2)

static struct BurnRomInfo sg1k_starjackRomDesc[] = {
	{ "star jacker (japan, europe) (v1.2).bin",	0x08000, 0x3fe59505, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack)
STD_ROM_FN(sg1k_starjack)

struct BurnDriver BurnDrvsg1k_starjack = {
	"sg1k_starjack", NULL, NULL, NULL, "1983",
	"Star Jacker (Euro, Jpn, v1.2)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starjackRomInfo, sg1k_starjackRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Jacker (Euro, Jpn, v1.1)

static struct BurnRomInfo sg1k_starjack1RomDesc[] = {
	{ "star jacker (japan, europe) (v1.1).bin",	0x08000, 0x7f25deca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack1)
STD_ROM_FN(sg1k_starjack1)

struct BurnDriver BurnDrvsg1k_starjack1 = {
	"sg1k_starjack1", NULL, NULL, NULL, "1983",
	"Star Jacker (Euro, Jpn, v1.1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starjack1RomInfo, sg1k_starjack1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Jacker (Jpn, v1.0)

static struct BurnRomInfo sg1k_starjack2RomDesc[] = {
	{ "star jacker (japan) (v1.0).bin",	0x08000, 0x1ae94122, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjack2)
STD_ROM_FN(sg1k_starjack2)

struct BurnDriver BurnDrvsg1k_starjack2 = {
	"sg1k_starjack2", NULL, NULL, NULL, "1983",
	"Star Jacker (Jpn, v1.0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starjack2RomInfo, sg1k_starjack2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Star Jacker (Tw)

static struct BurnRomInfo sg1k_starjacktRomDesc[] = {
	{ "star jacker [v1] (tw).bin",	0x08000, 0xdf162201, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_starjackt)
STD_ROM_FN(sg1k_starjackt)

struct BurnDriver BurnDrvsg1k_starjackt = {
	"sg1k_starjackt", NULL, NULL, NULL, "1983?",
	"Star Jacker (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_starjacktRomInfo, sg1k_starjacktRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Super Tank (Jpn)

static struct BurnRomInfo sg1k_supertnkRomDesc[] = {
	{ "super tank (japan).bin",	0x08000, 0x084cc13e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnk)
STD_ROM_FN(sg1k_supertnk)

struct BurnDriver BurnDrvsg1k_supertnk = {
	"sg1k_supertnk", NULL, NULL, NULL, "1986",
	"Super Tank (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_supertnkRomInfo, sg1k_supertnkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Super Tank (Kor)

static struct BurnRomInfo sg1k_supertnkkRomDesc[] = {
	{ "super tank (kr).bin",	0x08000, 0x4c48b7ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnkk)
STD_ROM_FN(sg1k_supertnkk)

struct BurnDriver BurnDrvsg1k_supertnkk = {
	"sg1k_supertnkk", NULL, NULL, NULL, "1986?",
	"Super Tank (Kor)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_supertnkkRomInfo, sg1k_supertnkkRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chaoji Tanke (Tw)

static struct BurnRomInfo sg1k_supertnktRomDesc[] = {
	{ "super tank (tw).bin",	0x08000, 0xd0c3df3f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_supertnkt)
STD_ROM_FN(sg1k_supertnkt)

struct BurnDriver BurnDrvsg1k_supertnkt = {
	"sg1k_supertnkt", NULL, NULL, NULL, "1986?",
	"Chaoji Tanke (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_supertnktRomInfo, sg1k_supertnktRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanke Dazhan (Tw)

static struct BurnRomInfo sg1k_tankdazhRomDesc[] = {
	{ "tank battalion [dahjee] (tw).bin",	0x08000, 0x5cbd1163, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tankdazh)
STD_ROM_FN(sg1k_tankdazh)

struct BurnDriver BurnDrvsg1k_tankdazh = {
	"sg1k_tankdazh", NULL, NULL, NULL, "1986?",
	"Tanke Dazhan (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tankdazhRomInfo, sg1k_tankdazhRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Terebi Oekaki (Jpn)

static struct BurnRomInfo sg1k_terebioeRomDesc[] = {
	{ "terebi oekaki (japan).bin",	0x02000, 0xdd4a661b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_terebioe)
STD_ROM_FN(sg1k_terebioe)

struct BurnDriver BurnDrvsg1k_terebioe = {
	"sg1k_terebioe", NULL, NULL, NULL, "1985",
	"Terebi Oekaki (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_terebioeRomInfo, sg1k_terebioeRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Wonder Boy (Jpn, v1)

static struct BurnRomInfo sg1k_wboyRomDesc[] = {
	{ "wonder boy [v1].bin",	0x08000, 0xe8f0344d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboy)
STD_ROM_FN(sg1k_wboy)

struct BurnDriver BurnDrvsg1k_wboy = {
	"sg1k_wboy", NULL, NULL, NULL, "1986",
	"Wonder Boy (Jpn, v1)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_wboyRomInfo, sg1k_wboyRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Wonder Boy (Jpn, v0)

static struct BurnRomInfo sg1k_wboyaRomDesc[] = {
	{ "wonder boy (japan).bin",	0x08000, 0x160535c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboya)
STD_ROM_FN(sg1k_wboya)

struct BurnDriver BurnDrvsg1k_wboya = {
	"sg1k_wboya", NULL, NULL, NULL, "1986",
	"Wonder Boy (Jpn, v0)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_wboyaRomInfo, sg1k_wboyaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Taotailang (Tw)

static struct BurnRomInfo sg1k_wboytRomDesc[] = {
	{ "wonder boy [v1] (tw).bin",	0x08000, 0x953fc2b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_wboyt)
STD_ROM_FN(sg1k_wboyt)

struct BurnDriver BurnDrvsg1k_wboyt = {
	"sg1k_wboyt", NULL, NULL, NULL, "1986?",
	"Taotailang (Tw)\0", NULL, "Unknown", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_wboytRomInfo, sg1k_wboytRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yamato (Euro, Jpn)

static struct BurnRomInfo sg1k_yamatoRomDesc[] = {
	{ "yamato (japan, europe).bin",	0x04000, 0xe2fd5201, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamato)
STD_ROM_FN(sg1k_yamato)

struct BurnDriver BurnDrvsg1k_yamato = {
	"sg1k_yamato", NULL, NULL, NULL, "1983",
	"Yamato (Euro, Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_yamatoRomInfo, sg1k_yamatoRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yamato (Euro, Jpn, Alt)

static struct BurnRomInfo sg1k_yamatoaRomDesc[] = {
	{ "yamato [40k map].bin",	0x0a000, 0x9c7497ff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamatoa)
STD_ROM_FN(sg1k_yamatoa)

struct BurnDriver BurnDrvsg1k_yamatoa = {
	"sg1k_yamatoa", NULL, NULL, NULL, "1983",
	"Yamato (Euro, Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_yamatoaRomInfo, sg1k_yamatoaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yamato (Tw)

static struct BurnRomInfo sg1k_yamatotRomDesc[] = {
	{ "yamato (tw).bin",	0x04000, 0xb65a093f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yamatot)
STD_ROM_FN(sg1k_yamatot)

struct BurnDriver BurnDrvsg1k_yamatot = {
	"sg1k_yamatot", NULL, NULL, NULL, "1983?",
	"Yamato (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_yamatotRomInfo, sg1k_yamatotRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yie Ar Kung-Fu (Tw)

static struct BurnRomInfo sg1k_yiearRomDesc[] = {
	{ "yie ar kung-fu (tw).bin",	0x08000, 0xbb0f1930, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yiear)
STD_ROM_FN(sg1k_yiear)

struct BurnDriver BurnDrvsg1k_yiear = {
	"sg1k_yiear", NULL, NULL, NULL, "1985?",
	"Yie Ar Kung-Fu (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_yiearRomInfo, sg1k_yiearRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Yingzi Chuanshuo (Tw)

static struct BurnRomInfo sg1k_yingchuaRomDesc[] = {
	{ "legend of kage, the [dahjee] (tw).bin",	0x0c000, 0x2e7166d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_yingchua)
STD_ROM_FN(sg1k_yingchua)

struct BurnDriver BurnDrvsg1k_yingchua = {
	"sg1k_yingchua", NULL, NULL, NULL, "1986?",
	"Yingzi Chuanshuo (Tw)\0", NULL, "DahJee", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_yingchuaRomInfo, sg1k_yingchuaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Zaxxon (Jpn)

static struct BurnRomInfo sg1k_zaxxonRomDesc[] = {
	{ "zaxxon (japan).bin",	0x08000, 0x905467e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zaxxon)
STD_ROM_FN(sg1k_zaxxon)

struct BurnDriver BurnDrvsg1k_zaxxon = {
	"sg1k_zaxxon", NULL, NULL, NULL, "1985",
	"Zaxxon (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_zaxxonRomInfo, sg1k_zaxxonRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Zaxxon (Tw)

static struct BurnRomInfo sg1k_zaxxontRomDesc[] = {
	{ "zaxxon (tw).bin",	0x08000, 0x49cae925, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zaxxont)
STD_ROM_FN(sg1k_zaxxont)

struct BurnDriver BurnDrvsg1k_zaxxont = {
	"sg1k_zaxxont", NULL, NULL, NULL, "1985?",
	"Zaxxon (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_zaxxontRomInfo, sg1k_zaxxontRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Zippy Race (Jpn)

static struct BurnRomInfo sg1k_zippyracRomDesc[] = {
	{ "zippy race (japan).bin",	0x08000, 0xbc5d20df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zippyrac)
STD_ROM_FN(sg1k_zippyrac)

struct BurnDriver BurnDrvsg1k_zippyrac = {
	"sg1k_zippyrac", NULL, NULL, NULL, "1983",
	"Zippy Race (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_zippyracRomInfo, sg1k_zippyracRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Zippy Race (Tw)

static struct BurnRomInfo sg1k_zippyractRomDesc[] = {
	{ "zippy race (tw).bin",	0x08000, 0xbcf441a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zippyract)
STD_ROM_FN(sg1k_zippyract)

struct BurnDriver BurnDrvsg1k_zippyract = {
	"sg1k_zippyract", NULL, NULL, NULL, "1983?",
	"Zippy Race (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_zippyractRomInfo, sg1k_zippyractRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Zoom 909 (Jpn)

static struct BurnRomInfo sg1k_zoom909RomDesc[] = {
	{ "zoom 909 (japan).bin",	0x08000, 0x093830d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_zoom909)
STD_ROM_FN(sg1k_zoom909)

struct BurnDriver BurnDrvsg1k_zoom909 = {
	"sg1k_zoom909", NULL, NULL, NULL, "1985",
	"Zoom 909 (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_zoom909RomInfo, sg1k_zoom909RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Huo Hu Li (Tw)

static struct BurnRomInfo sg1k_huohuliRomDesc[] = {
	{ "zoom 909 (tw).bin",	0x08000, 0x9943fc2b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_huohuli)
STD_ROM_FN(sg1k_huohuli)

struct BurnDriver BurnDrvsg1k_huohuli = {
	"sg1k_huohuli", NULL, NULL, NULL, "1985?",
	"Huo Hu Li (Tw)\0", NULL, "Aaronix", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	SG1KGetZipName, sg1k_huohuliRomInfo, sg1k_huohuliRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Uranai Angel Cutie (Jpn)

static struct BurnRomInfo sg1k_uranaiacRomDesc[] = {
	{ "uranai angel cutie (jp).bin",	0x08000, 0xae4f92cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_uranaiac)
STD_ROM_FN(sg1k_uranaiac)

struct BurnDriver BurnDrvsg1k_uranaiac = {
	"sg1k_uranaiac", NULL, NULL, NULL, "198?",
	"Uranai Angel Cutie (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_uranaiacRomInfo, sg1k_uranaiacRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Link World (Prototype?)

static struct BurnRomInfo sg1k_linkwrldRomDesc[] = {
	{ "link world.bin",	0x08000, 0xd5e919c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_linkwrld)
STD_ROM_FN(sg1k_linkwrld)

struct BurnDriver BurnDrvsg1k_linkwrld = {
	"sg1k_linkwrld", NULL, NULL, NULL, "1987",
	"Link World (Prototype?)\0", NULL, "Abstract Software", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_linkwrldRomInfo, sg1k_linkwrldRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Home BASIC (Jpn)

static struct BurnRomInfo sg1k_homebasRomDesc[] = {
	{ "home basic (jp).bin",	0x08000, 0x78a37cbc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_homebas)
STD_ROM_FN(sg1k_homebas)

struct BurnDriver BurnDrvsg1k_homebas = {
	"sg1k_homebas", NULL, NULL, NULL, "1985",
	"Home BASIC (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_homebasRomInfo, sg1k_homebasRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// BASIC Level III (Jpn)

static struct BurnRomInfo sg1k_basic3RomDesc[] = {
	{ "basic3.bin",	0x08000, 0xa46e3c73, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_basic3)
STD_ROM_FN(sg1k_basic3)

struct BurnDriver BurnDrvsg1k_basic3 = {
	"sg1k_basic3", NULL, NULL, NULL, "1985",
	"BASIC Level III (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_basic3RomInfo, sg1k_basic3RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// BASIC Level III (Export)

static struct BurnRomInfo sg1k_basic3eRomDesc[] = {
	{ "basic3e.bin",	0x08000, 0x5d9f11ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_basic3e)
STD_ROM_FN(sg1k_basic3e)

struct BurnDriver BurnDrvsg1k_basic3e = {
	"sg1k_basic3e", NULL, NULL, NULL, "1985",
	"BASIC Level III (Export)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_basic3eRomInfo, sg1k_basic3eRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// BASIC Level 2 (Jpn)

static struct BurnRomInfo sg1k_basic2RomDesc[] = {
	{ "basic2.bin",	0x08000, 0xf691f9c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_basic2)
STD_ROM_FN(sg1k_basic2)

struct BurnDriver BurnDrvsg1k_basic2 = {
	"sg1k_basic2", NULL, NULL, NULL, "1985",
	"BASIC Level 2 (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_basic2RomInfo, sg1k_basic2RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega Music Editor (Euro?)

static struct BurnRomInfo sg1k_musicRomDesc[] = {
	{ "music.bin",	0x08000, 0x622010e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_music)
STD_ROM_FN(sg1k_music)

struct BurnDriver BurnDrvsg1k_music = {
	"sg1k_music", NULL, NULL, NULL, "198?",
	"Sega Music Editor (Euro?)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_musicRomInfo, sg1k_musicRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Music (Jpn)

static struct BurnRomInfo sg1k_musicjRomDesc[] = {
	{ "music (jp).bin",	0x08000, 0x2ec28526, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_musicj)
STD_ROM_FN(sg1k_musicj)

struct BurnDriver BurnDrvsg1k_musicj = {
	"sg1k_musicj", NULL, NULL, NULL, "1983",
	"Music (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_musicjRomInfo, sg1k_musicjRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eitango ~ Chuugaku 1-Nen (Jpn)

static struct BurnRomInfo sg1k_chueit1nRomDesc[] = {
	{ "chuugaku hisshuu eitango (chuugaku 1-nen) (jp).bin",	0x04000, 0x6ccb297a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueit1n)
STD_ROM_FN(sg1k_chueit1n)

struct BurnDriver BurnDrvsg1k_chueit1n = {
	"sg1k_chueit1n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eitango ~ Chuugaku 1-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueit1nRomInfo, sg1k_chueit1nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eisakubun ~ Chuugaku 1-Nen (Jpn)

static struct BurnRomInfo sg1k_chueis1nRomDesc[] = {
	{ "chuugaku hisshuu eisakubun (chuugaku 1-nen) (jp).bin",	0x04000, 0x9bafd9e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueis1n)
STD_ROM_FN(sg1k_chueis1n)

struct BurnDriver BurnDrvsg1k_chueis1n = {
	"sg1k_chueis1n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eisakubun ~ Chuugaku 1-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueis1nRomInfo, sg1k_chueis1nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eisakubun ~ Chuugaku 1-Nen (Jpn, Alt)

static struct BurnRomInfo sg1k_chueis1naRomDesc[] = {
	{ "chuugaku hisshuu eisakubun (chuugaku 1-nen) (jp) [40k map].bin",	0x0a000, 0x975106b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueis1na)
STD_ROM_FN(sg1k_chueis1na)

struct BurnDriver BurnDrvsg1k_chueis1na = {
	"sg1k_chueis1na", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eisakubun ~ Chuugaku 1-Nen (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueis1naRomInfo, sg1k_chueis1naRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eibunpou ~ Chuugaku 1-Nen (Jpn)

static struct BurnRomInfo sg1k_chueib1nRomDesc[] = {
	{ "chuugaku hisshuu eibunpou (chuugaku 1-nen) (jp).bin",	0x02000, 0x345c8bc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueib1n)
STD_ROM_FN(sg1k_chueib1n)

struct BurnDriver BurnDrvsg1k_chueib1n = {
	"sg1k_chueib1n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eibunpou ~ Chuugaku 1-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueib1nRomInfo, sg1k_chueib1nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansu ~ Shougaku 4-Nen Jou (Jpn)

static struct BurnRomInfo sg1k_tansan4jRomDesc[] = {
	{ "tanoshii sansuu (shougaku 4-nen jou) (jp).bin",	0x04000, 0x573e04dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan4j)
STD_ROM_FN(sg1k_tansan4j)

struct BurnDriver BurnDrvsg1k_tansan4j = {
	"sg1k_tansan4j", NULL, NULL, NULL, "1983",
	"Tanoshii Sansu ~ Shougaku 4-Nen Jou (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan4jRomInfo, sg1k_tansan4jRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansu ~ Shougaku 4-Nen Jou (Jpn, Alt)

static struct BurnRomInfo sg1k_tansan4jaRomDesc[] = {
	{ "tanoshii sansuu (shougaku 4-nen jou) [40k map] (jp).bin",	0x0a000, 0x7c400e3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan4ja)
STD_ROM_FN(sg1k_tansan4ja)

struct BurnDriver BurnDrvsg1k_tansan4ja = {
	"sg1k_tansan4ja", NULL, NULL, NULL, "1983",
	"Tanoshii Sansu ~ Shougaku 4-Nen Jou (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan4jaRomInfo, sg1k_tansan4jaRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Kagaku ~ Gensokigou Master (Jpn)

static struct BurnRomInfo sg1k_kagakuRomDesc[] = {
	{ "kagaku (gensokigou master) (jp).bin",	0x02000, 0xf9c81fe1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_kagaku)
STD_ROM_FN(sg1k_kagaku)

struct BurnDriver BurnDrvsg1k_kagaku = {
	"sg1k_kagaku", NULL, NULL, NULL, "1983",
	"Kagaku ~ Gensokigou Master (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_kagakuRomInfo, sg1k_kagakuRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Nihonshi Nenpyou (Jpn)

static struct BurnRomInfo sg1k_nihonshiRomDesc[] = {
	{ "nihonshi nenpyou (jp).bin",	0x04000, 0x129d6359, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_nihonshi)
STD_ROM_FN(sg1k_nihonshi)

struct BurnDriver BurnDrvsg1k_nihonshi = {
	"sg1k_nihonshi", NULL, NULL, NULL, "1983",
	"Nihonshi Nenpyou (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_nihonshiRomInfo, sg1k_nihonshiRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sekaishi Nenpyou ~ Monbushou Shidouyouryou Junkyo (Jpn)

static struct BurnRomInfo sg1k_sekaishiRomDesc[] = {
	{ "sekaishi nenpyou (jp).bin",	0x04000, 0x3274ee48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_sekaishi)
STD_ROM_FN(sg1k_sekaishi)

struct BurnDriver BurnDrvsg1k_sekaishi = {
	"sg1k_sekaishi", NULL, NULL, NULL, "1983",
	"Sekaishi Nenpyou ~ Monbushou Shidouyouryou Junkyo (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_sekaishiRomInfo, sg1k_sekaishiRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eitango ~ Chuugaku 2-Nen (Jpn)

static struct BurnRomInfo sg1k_chueit2nRomDesc[] = {
	{ "chuugaku hisshuu eitango (chuugaku 2-nen) (jp).bin",	0x04000, 0x8507216b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueit2n)
STD_ROM_FN(sg1k_chueit2n)

struct BurnDriver BurnDrvsg1k_chueit2n = {
	"sg1k_chueit2n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eitango ~ Chuugaku 2-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueit2nRomInfo, sg1k_chueit2nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eitango ~ Chuugaku 2-Nen (Jpn, Alt)

static struct BurnRomInfo sg1k_chueit2naRomDesc[] = {
	{ "chuugaku hisshuu eitango (chuugaku 2-nen) [40k map] (jp).bin",	0x0a000, 0x4f599483, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueit2na)
STD_ROM_FN(sg1k_chueit2na)

struct BurnDriver BurnDrvsg1k_chueit2na = {
	"sg1k_chueit2na", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eitango ~ Chuugaku 2-Nen (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueit2naRomInfo, sg1k_chueit2naRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eisakubun ~ Chuugaku 2-Nen (Jpn)

static struct BurnRomInfo sg1k_chueis2nRomDesc[] = {
	{ "chuugaku hisshuu eisakubun (chuugaku 2-nen) (jp).bin",	0x04000, 0x581dce41, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueis2n)
STD_ROM_FN(sg1k_chueis2n)

struct BurnDriver BurnDrvsg1k_chueis2n = {
	"sg1k_chueis2n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eisakubun ~ Chuugaku 2-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueis2nRomInfo, sg1k_chueis2nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eisakubun ~ Chuugaku 2-Nen (Jpn, Alt)

static struct BurnRomInfo sg1k_chueis2naRomDesc[] = {
	{ "chuugaku hisshuu eisakubun (chuugaku 2-nen) [40k map] (jp).bin",	0x0a000, 0x3657b285, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueis2na)
STD_ROM_FN(sg1k_chueis2na)

struct BurnDriver BurnDrvsg1k_chueis2na = {
	"sg1k_chueis2na", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eisakubun ~ Chuugaku 2-Nen (Jpn, Alt)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueis2naRomInfo, sg1k_chueis2naRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Chuugaku Hisshuu Eibunpou ~ Chuugaku 2-Nen (Jpn)

static struct BurnRomInfo sg1k_chueib2nRomDesc[] = {
	{ "chuugaku hisshuu eibunpou (chuugaku 2-nen) (jp).bin",	0x02000, 0xd4c473b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_chueib2n)
STD_ROM_FN(sg1k_chueib2n)

struct BurnDriver BurnDrvsg1k_chueib2n = {
	"sg1k_chueib2n", NULL, NULL, NULL, "1983",
	"Chuugaku Hisshuu Eibunpou ~ Chuugaku 2-Nen (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_chueib2nRomInfo, sg1k_chueib2nRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansu ~ Shougaku 4-Nen Ge (Jpn)

static struct BurnRomInfo sg1k_tansan4gRomDesc[] = {
	{ "tanoshii sansuu (shougaku 4-nen ge) (jp).bin",	0x04000, 0x5fde25bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan4g)
STD_ROM_FN(sg1k_tansan4g)

struct BurnDriver BurnDrvsg1k_tansan4g = {
	"sg1k_tansan4g", NULL, NULL, NULL, "1983",
	"Tanoshii Sansu ~ Shougaku 4-Nen Ge (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan4gRomInfo, sg1k_tansan4gRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansu ~ Shougaku 5-Nen Ge (Jpn)

static struct BurnRomInfo sg1k_tansan5gRomDesc[] = {
	{ "tanoshii sansuu (shougaku 5-nen ge) (jp).bin",	0x04000, 0x6a96978d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan5g)
STD_ROM_FN(sg1k_tansan5g)

struct BurnDriver BurnDrvsg1k_tansan5g = {
	"sg1k_tansan5g", NULL, NULL, NULL, "1983",
	"Tanoshii Sansu ~ Shougaku 5-Nen Ge (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan5gRomInfo, sg1k_tansan5gRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansu ~ Shougaku 6-Nen Ge (Jpn)

static struct BurnRomInfo sg1k_tansan6gRomDesc[] = {
	{ "tanoshii sansuu (shougaku 6-nen ge) (jp).bin",	0x08000, 0xd0d18e70, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan6g)
STD_ROM_FN(sg1k_tansan6g)

struct BurnDriver BurnDrvsg1k_tansan6g = {
	"sg1k_tansan6g", NULL, NULL, NULL, "1983",
	"Tanoshii Sansu ~ Shougaku 6-Nen Ge (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan6gRomInfo, sg1k_tansan6gRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansuu ~ Shougaku 5-Nen Jou (Jpn)

static struct BurnRomInfo sg1k_tansan5jRomDesc[] = {
	{ "tanoshii sansuu (shougaku 5-nen jou) (jp).bin",	0x08000, 0x419d75d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan5j)
STD_ROM_FN(sg1k_tansan5j)

struct BurnDriver BurnDrvsg1k_tansan5j = {
	"sg1k_tansan5j", NULL, NULL, NULL, "1983",
	"Tanoshii Sansuu ~ Shougaku 5-Nen Jou (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan5jRomInfo, sg1k_tansan5jRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Tanoshii Sansuu ~ Shougaku 6-Nen Jou (Jpn)

static struct BurnRomInfo sg1k_tansan6jRomDesc[] = {
	{ "tanoshii sansuu (shougaku 6-nen jou) (jp).bin",	0x08000, 0x8b5e6e1b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_tansan6j)
STD_ROM_FN(sg1k_tansan6j)

struct BurnDriver BurnDrvsg1k_tansan6j = {
	"sg1k_tansan6j", NULL, NULL, NULL, "1983",
	"Tanoshii Sansuu ~ Shougaku 6-Nen Jou (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_tansan6jRomInfo, sg1k_tansan6jRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Sega Card Catcher (Jpn)

static struct BurnRomInfo sg1k_cardctchRomDesc[] = {
	{ "tanoshii sansuu (shougaku 6-nen jou) (jp).bin",	0x08000, 0x8b5e6e1b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_cardctch)
STD_ROM_FN(sg1k_cardctch)

struct BurnDriver BurnDrvsg1k_cardctch = {
	"sg1k_cardctch", NULL, NULL, NULL, "198?",
	"Sega Card Catcher (Jpn)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_cardctchRomInfo, sg1k_cardctchRomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Boxing (Jpn, MyCard)

static struct BurnRomInfo sg1k_champbox1RomDesc[] = {
	{ "champion boxing (japan) (mycard).bin",	0x08000, 0xf8b2ac1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champbox1)
STD_ROM_FN(sg1k_champbox1)

struct BurnDriver BurnDrvsg1k_champbox1 = {
	"sg1k_champbox1", NULL, NULL, NULL, "1984",
	"Champion Boxing (Jpn, MyCard)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champbox1RomInfo, sg1k_champbox1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};


// Champion Golf (Jpn, MyCard)

static struct BurnRomInfo sg1k_champglf1RomDesc[] = {
	{ "champion golf [card].bin",	0x08000, 0x5a904122, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sg1k_champglf1)
STD_ROM_FN(sg1k_champglf1)

struct BurnDriver BurnDrvsg1k_champglf1 = {
	"sg1k_champglf1", NULL, NULL, NULL, "1983",
	"Champion Golf (Jpn, MyCard)\0", NULL, "Sega", "Sega SG-1000",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_SG1000, GBF_MISC, 0,
	SG1KGetZipName, sg1k_champglf1RomInfo, sg1k_champglf1RomName, NULL, NULL, Sg1000InputInfo, Sg1000DIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	285, 243, 4, 3
};

