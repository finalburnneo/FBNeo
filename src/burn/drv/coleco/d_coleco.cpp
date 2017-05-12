// FB Alpha ColecoVision console driver module
// Code by iq_132, fixups & bring up-to-date by dink Aug 19, 2014

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "sn76496.h"
#include "tms9928a.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80BIOS;
static UINT8 *DrvCartROM;
static UINT8 *DrvZ80RAM;

static INT32 joy_mode;
static INT32 joy_status[2];
static INT32 last_state;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[2] = { 0, 0 };
static UINT16 DrvInputs[4];
static UINT8 DrvReset;
static UINT32 MegaCart;
static UINT32 MegaCartBank;

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnInputInfo ColecoInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Fire 1",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},
	{"P1 Fire 2",	BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	{"P1 0",		BIT_DIGITAL,	DrvJoy1 +  0,	"p1 0"		},
	{"P1 1",		BIT_DIGITAL,	DrvJoy1 +  1,	"p1 1"		},
	{"P1 2",		BIT_DIGITAL,	DrvJoy1 +  2,	"p1 2"		},
	{"P1 3",		BIT_DIGITAL,	DrvJoy1 +  3,	"p1 3"		},
	{"P1 4",		BIT_DIGITAL,	DrvJoy1 +  4,	"p1 4"		},
	{"P1 5",		BIT_DIGITAL,	DrvJoy1 +  5,	"p1 5"		},
	{"P1 6",		BIT_DIGITAL,	DrvJoy1 +  6,	"p1 6"		},
	{"P1 7",		BIT_DIGITAL,	DrvJoy1 +  7,	"p1 7"		},
	{"P1 8",		BIT_DIGITAL,	DrvJoy1 +  8,	"p1 8"		},
	{"P1 9",		BIT_DIGITAL,	DrvJoy1 +  9,	"p1 9"		},
	{"P1 #",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 #"		},
	{"P1 *",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 *"		},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Fire 1",	BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Fire 2",	BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 2"	},
	{"P2 0",		BIT_DIGITAL,	DrvJoy3 +  0,	"p2 0"		},
	{"P2 1",		BIT_DIGITAL,	DrvJoy3 +  1,	"p2 1"		},
	{"P2 2",		BIT_DIGITAL,	DrvJoy3 +  2,	"p2 2"		},
	{"P2 3",		BIT_DIGITAL,	DrvJoy3 +  3,	"p2 3"		},
	{"P2 4",		BIT_DIGITAL,	DrvJoy3 +  4,	"p2 4"		},
	{"P2 5",		BIT_DIGITAL,	DrvJoy3 +  5,	"p2 5"		},
	{"P2 6",		BIT_DIGITAL,	DrvJoy3 +  6,	"p2 6"		},
	{"P2 7",		BIT_DIGITAL,	DrvJoy3 +  7,	"p2 7"		},
	{"P2 8",		BIT_DIGITAL,	DrvJoy3 +  8,	"p2 8"		},
	{"P2 9",		BIT_DIGITAL,	DrvJoy3 +  9,	"p2 9"		},
	{"P2 #",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 #"		},
	{"P2 *",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 *"		},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Controller Select",	BIT_DIPSWITCH,	DrvDips + 0,	"dip" },
	{"Bios Select",	BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Coleco)

static struct BurnDIPInfo ColecoDIPList[]=
{
	{0x25, 0xff, 0xff, 0x00, NULL				},
	{0x26, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    3, "Port 1 Controller"		},
	{0x25, 0x01, 0x07, 0x00, "ColecoVision Controller"	},
	{0x25, 0x01, 0x07, 0x01, "Super Action Controller"	},
	{0x25, 0x01, 0x07, 0x02, "Driving Controller"		},

	{0   , 0xfe, 0   ,    2, "Port 2 Controller"		},
	{0x25, 0x01, 0x70, 0x00, "ColecoVision Controller"	},
	{0x25, 0x01, 0x70, 0x10, "Super Action Controller"	},

	{0   , 0xfe, 0   ,    2, "Extra Controller"		},
	{0x25, 0x01, 0x80, 0x00, "None"				},
	{0x25, 0x01, 0x80, 0x80, "Roller Controller"		},

	{0   , 0xfe, 0   ,    4, "Bios Select"			},
	{0x26, 0x01, 0x03, 0x00, "Standard"			},
	{0x26, 0x01, 0x03, 0x01, "Thick Characters"		},
	{0x26, 0x01, 0x03, 0x02, "SVI-603 Coleco Game Adapter"	},
	{0x26, 0x01, 0x03, 0x03, "Chuang Zao Zhe 50"            },

	{0   , 0xfe, 0,       2, "Bypass bios intro (hack)"	},
	{0x26, 0x01, 0x10, 0x00, "Off"				},
	{0x26, 0x01, 0x10, 0x10, "On"				},
};

STDDIPINFO(Coleco)

static UINT8 paddle_r(INT32 paddle)
{
	INT32 ctrl_select = (DrvDips[0] >> (paddle ? 4 : 0)) & 0x07;

	UINT8 data = 0x0f;

	if (ctrl_select != 0x01)
	{
		if (joy_mode == 0)
		{
			UINT16 input = 0x0000;

			if (ctrl_select == 0x00) // colecovision controller
			{
				input = DrvInputs[(2 * paddle) + 0]; // keypad
			}
			else if (ctrl_select == 0x02)		// super action controller controller
			{
				input = 0xffff; //input_port_read(space->machine, "SAC_KPD2");
			}

			if (ctrl_select != 0x03)
			{
				if (!(input & 0x0001)) data &= 0x0a; /* 0 */
				if (!(input & 0x0002)) data &= 0x0d; /* 1 */
				if (!(input & 0x0004)) data &= 0x07; /* 2 */
				if (!(input & 0x0008)) data &= 0x0c; /* 3 */
				if (!(input & 0x0010)) data &= 0x02; /* 4 */
				if (!(input & 0x0020)) data &= 0x03; /* 5 */
				if (!(input & 0x0040)) data &= 0x0e; /* 6 */
				if (!(input & 0x0080)) data &= 0x05; /* 7 */
				if (!(input & 0x0100)) data &= 0x01; /* 8 */
				if (!(input & 0x0200)) data &= 0x0b; /* 9 */
				if (!(input & 0x0400)) data &= 0x06; /* # */
				if (!(input & 0x0800)) data &= 0x09; /* . */
				if (!(input & 0x1000)) data &= 0x04; /* Blue Action Button */
				if (!(input & 0x2000)) data &= 0x08; /* Purple Action Button */
			}

			return ((input & 0x4000) >> 8) | 0x30 | (data);
		}
		else
		{
			if (ctrl_select == 0x00)			// colecovision controller
			{
				data = DrvInputs[(2 * paddle) + 1] & 0xcf; // Joy
			}
			else if (ctrl_select == 0x02)		// super action controller controller
			{
				data = 0xff & 0xcf; //input_port_read(space->machine, "SAC_JOY2") & 0xcf;
			}

			if ((DrvDips[0] & 0x80) || (ctrl_select == 0x02) || (ctrl_select == 0x03))
			{
				if (joy_status[paddle] == 0) data |= 0x30;
				else if (joy_status[paddle] == 1) data |= 0x20;
			}

			return data | 0x80;
		}
	}

	return data;
}

static void paddle_callback()
{
	UINT8 analog1 = 0x00;
	UINT8 analog2 = 0x00;
	UINT8 ctrl_sel = DrvDips[0];

	if ((ctrl_sel & 0x07) == 0x03)	// Driving controller
		analog1 = 0xff; //input_port_read_safe(machine, "DRIV", 0);
	else
	{
		if ((ctrl_sel & 0x07) == 0x02)	// Super Action Controller P1
			analog1 = 0xff; //input_port_read_safe(machine, "SAC_SLIDE1", 0);

		if ((ctrl_sel & 0x70) == 0x20)	// Super Action Controller P2
			analog2 = 0xff; //input_port_read_safe(machine, "SAC_SLIDE2", 0);

		if (ctrl_sel & 0x80)				// Roller controller
		{
			analog1 = 0xff; //input_port_read_safe(machine, "ROLLER_X", 0);
			analog2 = 0xff; // input_port_read_safe(machine, "ROLLER_Y", 0);
		}
	}

	if (analog1 == 0)
		joy_status[0] = 0;
	else if (analog1 & 0x08)
		joy_status[0] = -1;
	else
		joy_status[0] = 1;

	if (analog2 == 0)
		joy_status[1] = 0;
	else if (analog2 & 0x08)
		joy_status[1] = -1;
	else
		joy_status[1] = 1;

	if (joy_status[0] || joy_status[1]) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO); // ACK? -- lower when input read?
	}
}

void __fastcall coleco_write_port(UINT16 port, UINT8 data)
{
	switch (port & ~0xff1e)
	{
		case 0x80:
		case 0x81:
			joy_mode = 0;
		return;

		case 0xa0:
			TMS9928AWriteVRAM(data);
		return;

		case 0xa1:
			TMS9928AWriteRegs(data);
		return;

		case 0xc0:
		case 0xc1:
			joy_mode = 1;
		return;

		case 0xe0:
		case 0xe1:
			SN76496Write(0, data);
		return;
	}
}

UINT8 __fastcall coleco_read_port(UINT16 port)
{
	switch (port & ~0xff1e)
	{
		case 0xa0:
			return TMS9928AReadVRAM();

		case 0xa1:
			return TMS9928AReadRegs();

		case 0xe0:
		case 0xe1:
			return paddle_r(0);

		case 0xe2:
		case 0xe3:
			return paddle_r(1);
	}

	return 0;
}

static void coleco_vdp_interrupt(INT32 state)
{
	if (state && !last_state)
		ZetNmi();

	last_state = state;
}

static void CVFastLoadHack() {
    if (DrvDips[1] & 0x10) {
        DrvZ80BIOS[0x13f1] = 0x00;
        DrvZ80BIOS[0x13f2] = 0x00;
        DrvZ80BIOS[0x13f3] = 0x00;
    }
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	BurnLoadRom(DrvZ80BIOS, 0x80 + (DrvDips[1] & 3), 1);
	CVFastLoadHack();

	ZetOpen(0);
	ZetReset();
	ZetSetVector(0xff);
	ZetClose();

	TMS9928AReset();

	memset (DrvZ80RAM, 0xff, 0x400); // ram initialized to 0xff

	last_state = 0; // irq state...
	MegaCartBank = 0;

	return 0;
}

#if 0
static void __fastcall main_write(UINT16 address, UINT8 data)
{
	// maybe we should support bankswitching on writes too?
	//bprintf(0, _T("mw %X,"), address);
}
#endif

static UINT8 __fastcall main_read(UINT16 address)
{
	if (address >= 0xffc0/* && address <= 0xffff*/) {
		UINT32 MegaCartBanks = MegaCart / 0x4000;

		MegaCartBank = (0xffff - address) & (MegaCartBanks - 1);

		MegaCartBank = (MegaCartBanks - MegaCartBank) - 1;

		return 0;
	}

	if (address >= 0xc000 && address <= 0xffbf)
		return DrvCartROM[(MegaCartBank * 0x4000) + (address - 0xc000)];

	//bprintf(0, _T("mr %X,"), address);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80BIOS		= Next; Next += 0x004000;
	DrvCartROM		= Next; Next += 0x100000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	// refresh rate 59.92hz

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	MegaCart = 0;

	{
		char* pRomName;
		struct BurnRomInfo ri;

		if (BurnLoadRom(DrvZ80BIOS, 0x80, 1)) return 1;

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
			BurnDrvGetRomInfo(&ri, i);

			if ((ri.nType & BRF_PRG) && (ri.nLen == 0x2000 || ri.nLen == 0x1000) && (i<10)) {
				BurnLoadRom(DrvCartROM+(i * 0x2000), i, 1);
				bprintf(0, _T("ColecoVision romload #%d\n"), i);
			} else if ((ri.nType & BRF_PRG) && (i<10)) { // Load rom thats not in 0x2000 (8k) chunks
				bprintf(0, _T("ColecoVision romload (unsegmented) #%d size: %X\n"), i, ri.nLen);
				BurnLoadRom(DrvCartROM, i, 1);
				if (ri.nLen >= 0x20000) MegaCart = ri.nLen;
			}
		}
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x1fff, 0, DrvZ80BIOS);
	ZetMapArea(0x0000, 0x1fff, 2, DrvZ80BIOS);

	for (INT32 i = 0x6000; i < 0x8000; i+=0x0400) {
		ZetMapArea(i + 0x0000, i + 0x03ff, 0, DrvZ80RAM);
		ZetMapArea(i + 0x0000, i + 0x03ff, 1, DrvZ80RAM);
		ZetMapArea(i + 0x0000, i + 0x03ff, 2, DrvZ80RAM);
	}

	if (MegaCart) {
		// MegaCart
		UINT32 MegaCartBanks = MegaCart / 0x4000;
		UINT32 lastbank = (MegaCartBanks - 1) * 0x4000;
		bprintf(0, _T("ColecoVision MegaCart: mapping cartrom[%X] to 0x8000 - 0xbfff.\n"), lastbank);
		ZetMapArea(0x8000, 0xbfff, 0, DrvCartROM + lastbank);
		ZetMapArea(0x8000, 0xbfff, 2, DrvCartROM + lastbank);
		ZetSetReadHandler(main_read);
		//ZetSetWriteHandler(main_write);
	} else {
		// Regular CV Cart
		ZetMapArea(0x8000, 0xffff, 0, DrvCartROM);
		ZetMapArea(0x8000, 0xffff, 2, DrvCartROM);
	}


	ZetSetOutHandler(coleco_write_port);
	ZetSetInHandler(coleco_read_port);
	ZetClose();

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, coleco_vdp_interrupt);

	SN76489AInit(0, 3579545, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	TMS9928AExit();
	ZetExit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 3579545 / 60;
	INT32 nCyclesDone  = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		TMS9928AScanline(i);

		if ((i%5)==4) paddle_callback(); // 50x / frame (3000x / sec)
	}

	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
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

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		TMS9928AScan(nAction, pnMin);

		SCAN_VAR(joy_mode);
		SCAN_VAR(joy_status);
		SCAN_VAR(last_state);
	}

	return 0;
}

INT32 CVGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	// remove the "CV_"
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 3];
	}

	*pszName = szFilename;

	return 0;
}

// ColecoVision

static struct BurnRomInfo cv_colecoRomDesc[] = {
    { "coleco.rom",     0x2000, 0x3aa93ef3, BRF_PRG | BRF_BIOS }, // 0x80 - Normal (Coleco, 1982)
    { "colecoa.rom",	0x2000, 0x39bb16fc, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x81 - Thick Characters (Coleco, 1982)
    { "svi603.rom", 	0x2000, 0x19e91b82, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x82 - SVI-603 Coleco Game Adapter (Spectravideo, 1983)
    { "czz50.rom",		0x4000, 0x4999abc6, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x83 - Chuang Zao Zhe 50 (Bit Corporation, 1986)
};

STD_ROM_PICK(cv_coleco)
STD_ROM_FN(cv_coleco)

struct BurnDriver BurnDrvcv_Coleco = {
	"cv_coleco", NULL, NULL, NULL, "1982",
	"ColecoVision System BIOS\0", "BIOS only", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_COLECO, GBF_BIOS, 0,
	CVGetZipName, cv_colecoRomInfo, cv_colecoRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Homebrew games

static struct BurnRomInfo cv_diggerRomDesc[] = {
	{ "digger_cv.rom",	0x06000, 0x77088cab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_digger, cv_digger, cv_coleco)
STD_ROM_FN(cv_digger)

struct BurnDriver BurnDrvcv_digger = {
	"cv_digger", NULL, "cv_coleco", NULL, "2012",
	"Digger\0", NULL, "Coleco*Windmill Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_diggerRomInfo, cv_diggerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

static struct BurnRomInfo cv_questgcRomDesc[] = {
	{ "quest_golden_chalice_colecovision.rom",	0x08000, 0x6da37da8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_questgc, cv_questgc, cv_coleco)
STD_ROM_FN(cv_questgc)

struct BurnDriver BurnDrvcv_questgc = {
	"cv_questgc", NULL, "cv_coleco", NULL, "2012",
	"Quest for the Golden Chalice\0", NULL, "Coleco*Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_questgcRomInfo, cv_questgcRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

static struct BurnRomInfo cv_princessquestRomDesc[] = {
	{ "princess_quest_colecovision.rom",	0x40000, 0xa59eaa2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_princessquest, cv_princessquest, cv_coleco)
STD_ROM_FN(cv_princessquest)

struct BurnDriver BurnDrvcv_princessquest = {
	"cv_pquest", NULL, "cv_coleco", NULL, "2012",
	"Princess Quest\0", NULL, "Coleco*Nanochess", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_princessquestRomInfo, cv_princessquestRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

static struct BurnRomInfo cv_danslitherRomDesc[] = {
	{ "danslither.rom",	0x0402a, 0x92624cff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_danslither, cv_danslither, cv_coleco)
STD_ROM_FN(cv_danslither)

struct BurnDriver BurnDrvcv_danslither = {
	"cv_danslither", "cv_slither", "cv_coleco", NULL, "1983",
	"Slither (Joystick Version)\0", NULL, "Coleco*Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_danslitherRomInfo, cv_danslitherRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// End of driver, the following driver info. has been synthesized from hash/coleco.xml of MESS
// Note: when re-converting coleco.xml, make sure to change "&amp;" to "and"!
// Note2: don't forget to add notice to choplifter: "Corrupted sprites. Use (Alt) version!"
// Castelo

static struct BurnRomInfo cv_casteloRomDesc[] = {
	{ "castelo.bin",	0x02000, 0x07a3d3db, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_castelo, cv_castelo, cv_coleco)
STD_ROM_FN(cv_castelo)

struct BurnDriver BurnDrvcv_castelo = {
	"cv_castelo", NULL, "cv_coleco", NULL, "1985",
	"Castelo\0", NULL, "Splice Vision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_casteloRomInfo, cv_casteloRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Q*bert

static struct BurnRomInfo cv_qbertRomDesc[] = {
	{ "qbert.bin",	0x02000, 0x532f61ba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qbert, cv_qbert, cv_coleco)
STD_ROM_FN(cv_qbert)

struct BurnDriver BurnDrvcv_qbert = {
	"cv_qbert", NULL, "cv_coleco", NULL, "1983",
	"Q*bert\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_qbertRomInfo, cv_qbertRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Q*bert (Alt)

static struct BurnRomInfo cv_qbertaRomDesc[] = {
	{ "qberta.bin",	0x02000, 0x13f06adc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qberta, cv_qberta, cv_coleco)
STD_ROM_FN(cv_qberta)

struct BurnDriver BurnDrvcv_qberta = {
	"cv_qberta", "cv_qbert", "cv_coleco", NULL, "1983",
	"Q*bert (Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_qbertaRomInfo, cv_qbertaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cobra

static struct BurnRomInfo cv_scobraRomDesc[] = {
	{ "scobra.bin",	0x02000, 0x6cb5cb8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_scobra, cv_scobra, cv_coleco)
STD_ROM_FN(cv_scobra)

struct BurnDriver BurnDrvcv_scobra = {
	"cv_scobra", NULL, "cv_coleco", NULL, "1983",
	"Super Cobra\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_scobraRomInfo, cv_scobraRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cobra (Alt)

static struct BurnRomInfo cv_scobraaRomDesc[] = {
	{ "scobraa.bin",	0x02000, 0xf84622d2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_scobraa, cv_scobraa, cv_coleco)
STD_ROM_FN(cv_scobraa)

struct BurnDriver BurnDrvcv_scobraa = {
	"cv_scobraa", "cv_scobra", "cv_coleco", NULL, "1983",
	"Super Cobra (Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_scobraaRomInfo, cv_scobraaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Sketch

static struct BurnRomInfo cv_ssketchRomDesc[] = {
	{ "ssketch.bin",	0x02000, 0x8627300a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ssketch, cv_ssketch, cv_coleco)
STD_ROM_FN(cv_ssketch)

struct BurnDriver BurnDrvcv_ssketch = {
	"cv_ssketch", NULL, "cv_coleco", NULL, "1984",
	"Super Sketch\0", NULL, "Personal Peripherals", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ssketchRomInfo, cv_ssketchRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Antarctic Adventure

static struct BurnRomInfo cv_antarctRomDesc[] = {
	{ "antaradv.1",	0x02000, 0xfbeb5fad, BRF_PRG | BRF_ESS },
	{ "antaradv.2",	0x02000, 0x0f4d40dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_antarct, cv_antarct, cv_coleco)
STD_ROM_FN(cv_antarct)

struct BurnDriver BurnDrvcv_antarct = {
	"cv_antarct", NULL, "cv_coleco", NULL, "1984",
	"Antarctic Adventure\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_antarctRomInfo, cv_antarctRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Alphabet Zoo

static struct BurnRomInfo cv_alphazooRomDesc[] = {
	{ "alphazoo.1",	0x02000, 0xae42a206, BRF_PRG | BRF_ESS },
	{ "alphazoo.2",	0x02000, 0x953ad47c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_alphazoo, cv_alphazoo, cv_coleco)
STD_ROM_FN(cv_alphazoo)

struct BurnDriver BurnDrvcv_alphazoo = {
	"cv_alphazoo", NULL, "cv_coleco", NULL, "1984",
	"Alphabet Zoo\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_alphazooRomInfo, cv_alphazooRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Amazing Bumpman

static struct BurnRomInfo cv_amazingRomDesc[] = {
	{ "amazing.1",	0x02000, 0xd3cae98d, BRF_PRG | BRF_ESS },
	{ "amazing.2",	0x02000, 0x36d0e09e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_amazing, cv_amazing, cv_coleco)
STD_ROM_FN(cv_amazing)

struct BurnDriver BurnDrvcv_amazing = {
	"cv_amazing", NULL, "cv_coleco", NULL, "1986",
	"Amazing Bumpman\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_amazingRomInfo, cv_amazingRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Number Bumper

static struct BurnRomInfo cv_numbumpRomDesc[] = {
	{ "numbump.1",	0x02000, 0x4a2cb66a, BRF_PRG | BRF_ESS },
	{ "numbump.2",	0x02000, 0xaf68a52d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_numbump, cv_numbump, cv_coleco)
STD_ROM_FN(cv_numbump)

struct BurnDriver BurnDrvcv_numbump = {
	"cv_numbump", "cv_amazing", "cv_coleco", NULL, "1984",
	"Number Bumper\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_numbumpRomInfo, cv_numbumpRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Aquattack

static struct BurnRomInfo cv_aquatackRomDesc[] = {
	{ "aquatack.1",	0x02000, 0x96c60fa2, BRF_PRG | BRF_ESS },
	{ "aquatack.2",	0x02000, 0x628dd0cb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_aquatack, cv_aquatack, cv_coleco)
STD_ROM_FN(cv_aquatack)

struct BurnDriver BurnDrvcv_aquatack = {
	"cv_aquatack", NULL, "cv_coleco", NULL, "1984",
	"Aquattack\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_aquatackRomInfo, cv_aquatackRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Aquattack (Alt)

static struct BurnRomInfo cv_aquatackaRomDesc[] = {
	{ "aquatacka.1",	0x02000, 0x7f33f73a, BRF_PRG | BRF_ESS },
	{ "aquatacka.2",	0x02000, 0x8f3553eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_aquatacka, cv_aquatacka, cv_coleco)
STD_ROM_FN(cv_aquatacka)

struct BurnDriver BurnDrvcv_aquatacka = {
	"cv_aquatacka", "cv_aquatack", "cv_coleco", NULL, "1984",
	"Aquattack (Alt)\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_aquatackaRomInfo, cv_aquatackaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Artillery Duel

static struct BurnRomInfo cv_artduelRomDesc[] = {
	{ "artduel.1",	0x02000, 0x4e18f196, BRF_PRG | BRF_ESS },
	{ "artduel.2",	0x02000, 0x778c5a52, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_artduel, cv_artduel, cv_coleco)
STD_ROM_FN(cv_artduel)

struct BurnDriver BurnDrvcv_artduel = {
	"cv_artduel", NULL, "cv_coleco", NULL, "1983",
	"Artillery Duel\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_artduelRomInfo, cv_artduelRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// BC's Quest for Tires

static struct BurnRomInfo cv_bcquestRomDesc[] = {
	{ "bcquest.1",	0x02000, 0x1b866fb5, BRF_PRG | BRF_ESS },
	{ "bcquest.2",	0x02000, 0xcf56f6fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest, cv_bcquest, cv_coleco)
STD_ROM_FN(cv_bcquest)

struct BurnDriver BurnDrvcv_bcquest = {
	"cv_bcquest", NULL, "cv_coleco", NULL, "1983",
	"BC's Quest for Tires\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bcquestRomInfo, cv_bcquestRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Beamrider

static struct BurnRomInfo cv_beamridrRomDesc[] = {
	{ "beamridr.1",	0x02000, 0x7f08b2f4, BRF_PRG | BRF_ESS },
	{ "beamridr.2",	0x02000, 0x5cef708a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_beamridr, cv_beamridr, cv_coleco)
STD_ROM_FN(cv_beamridr)

struct BurnDriver BurnDrvcv_beamridr = {
	"cv_beamridr", NULL, "cv_coleco", NULL, "1983",
	"Beamrider\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_beamridrRomInfo, cv_beamridrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Blockade Runner

static struct BurnRomInfo cv_blockrunRomDesc[] = {
	{ "blockrun.1",	0x02000, 0x2f153090, BRF_PRG | BRF_ESS },
	{ "blockrun.2",	0x02000, 0x74ebff13, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_blockrun, cv_blockrun, cv_coleco)
STD_ROM_FN(cv_blockrun)

struct BurnDriver BurnDrvcv_blockrun = {
	"cv_blockrun", NULL, "cv_coleco", NULL, "1984",
	"Blockade Runner\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_blockrunRomInfo, cv_blockrunRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Boulder Dash

static struct BurnRomInfo cv_bdashRomDesc[] = {
	{ "bdash.1",	0x02000, 0xaee6e532, BRF_PRG | BRF_ESS },
	{ "bdash.2",	0x02000, 0x1092afb5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bdash, cv_bdash, cv_coleco)
STD_ROM_FN(cv_bdash)

struct BurnDriver BurnDrvcv_bdash = {
	"cv_bdash", NULL, "cv_coleco", NULL, "1984",
	"Boulder Dash\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bdashRomInfo, cv_bdashRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Brain Strainers

static struct BurnRomInfo cv_brainstrRomDesc[] = {
	{ "brainstr.1",	0x02000, 0x5de3b863, BRF_PRG | BRF_ESS },
	{ "brainstr.2",	0x02000, 0x690d25eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_brainstr, cv_brainstr, cv_coleco)
STD_ROM_FN(cv_brainstr)

struct BurnDriver BurnDrvcv_brainstr = {
	"cv_brainstr", NULL, "cv_coleco", NULL, "1984",
	"Brain Strainers\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_brainstrRomInfo, cv_brainstrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Burgertime

static struct BurnRomInfo cv_btimeRomDesc[] = {
	{ "btime.1",	0x02000, 0x0440c21e, BRF_PRG | BRF_ESS },
	{ "btime.2",	0x02000, 0x55522e34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_btime, cv_btime, cv_coleco)
STD_ROM_FN(cv_btime)

struct BurnDriver BurnDrvcv_btime = {
	"cv_btime", NULL, "cv_coleco", NULL, "1984",
	"Burgertime\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_btimeRomInfo, cv_btimeRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cabbage Patch Kids: Adventure in the Park

static struct BurnRomInfo cv_cabbageRomDesc[] = {
	{ "cabbage.1",	0x02000, 0x6a8fa43b, BRF_PRG | BRF_ESS },
	{ "cabbage.2",	0x02000, 0x49b92492, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbage, cv_cabbage, cv_coleco)
STD_ROM_FN(cv_cabbage)

struct BurnDriver BurnDrvcv_cabbage = {
	"cv_cabbage", NULL, "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cabbageRomInfo, cv_cabbageRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Campaign '84

static struct BurnRomInfo cv_campaignRomDesc[] = {
	{ "campaign.1",	0x02000, 0xd657ab6b, BRF_PRG | BRF_ESS },
	{ "campaign.2",	0x02000, 0x844aefcf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_campaign, cv_campaign, cv_coleco)
STD_ROM_FN(cv_campaign)

struct BurnDriver BurnDrvcv_campaign = {
	"cv_campaign", NULL, "cv_coleco", NULL, "1983",
	"Campaign '84\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_campaignRomInfo, cv_campaignRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Carnival

static struct BurnRomInfo cv_carnivalRomDesc[] = {
	{ "carnival.1",	0x02000, 0x3cab8c1f, BRF_PRG | BRF_ESS },
	{ "carnival.2",	0x02000, 0x4cf856a9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_carnival, cv_carnival, cv_coleco)
STD_ROM_FN(cv_carnival)

struct BurnDriver BurnDrvcv_carnival = {
	"cv_carnival", NULL, "cv_coleco", NULL, "1982",
	"Carnival\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_carnivalRomInfo, cv_carnivalRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cosmic Avenger

static struct BurnRomInfo cv_cavengerRomDesc[] = {
	{ "cavenger.1",	0x02000, 0xc852bee7, BRF_PRG | BRF_ESS },
	{ "cavenger.2",	0x02000, 0x75da80eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cavenger, cv_cavenger, cv_coleco)
STD_ROM_FN(cv_cavenger)

struct BurnDriver BurnDrvcv_cavenger = {
	"cv_cavenger", NULL, "cv_coleco", NULL, "1982",
	"Cosmic Avenger\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cavengerRomInfo, cv_cavengerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cosmic Avenger (Alt)

static struct BurnRomInfo cv_cavengeraRomDesc[] = {
	{ "cavenger.1",	0x02000, 0xc852bee7, BRF_PRG | BRF_ESS },
	{ "cavengera.2",	0x02000, 0x58d86f66, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cavengera, cv_cavengera, cv_coleco)
STD_ROM_FN(cv_cavengera)

struct BurnDriver BurnDrvcv_cavengera = {
	"cv_cavengera", "cv_cavenger", "cv_coleco", NULL, "1982",
	"Cosmic Avenger (Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cavengeraRomInfo, cv_cavengeraRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cosmic Crisis

static struct BurnRomInfo cv_ccrisisRomDesc[] = {
	{ "ccrisis.1",	0x02000, 0xf8084e5a, BRF_PRG | BRF_ESS },
	{ "ccrisis.2",	0x02000, 0x8c841d6a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ccrisis, cv_ccrisis, cv_coleco)
STD_ROM_FN(cv_ccrisis)

struct BurnDriver BurnDrvcv_ccrisis = {
	"cv_ccrisis", NULL, "cv_coleco", NULL, "1983",
	"Cosmic Crisis\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ccrisisRomInfo, cv_ccrisisRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Centipede

static struct BurnRomInfo cv_centipedRomDesc[] = {
	{ "centiped.1",	0x02000, 0x4afc1fea, BRF_PRG | BRF_ESS },
	{ "centiped.2",	0x02000, 0x9ca2a63d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_centiped, cv_centiped, cv_coleco)
STD_ROM_FN(cv_centiped)

struct BurnDriver BurnDrvcv_centiped = {
	"cv_centiped", NULL, "cv_coleco", NULL, "1983",
	"Centipede\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_centipedRomInfo, cv_centipedRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Choplifter!

static struct BurnRomInfo cv_chopliftRomDesc[] = {
	{ "choplift.1",	0x02000, 0x78564c16, BRF_PRG | BRF_ESS },
	{ "choplift.2",	0x02000, 0xb1abf125, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_choplift, cv_choplift, cv_coleco)
STD_ROM_FN(cv_choplift)

struct BurnDriver BurnDrvcv_choplift = {
	"cv_choplift", NULL, "cv_coleco", NULL, "1984",
	"Choplifter!\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_chopliftRomInfo, cv_chopliftRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Chuck Norris Superkicks

static struct BurnRomInfo cv_chucknorRomDesc[] = {
	{ "chucknor.1",	0x02000, 0x3c04540f, BRF_PRG | BRF_ESS },
	{ "chucknor.2",	0x02000, 0xa5c58202, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_chucknor, cv_chucknor, cv_coleco)
STD_ROM_FN(cv_chucknor)

struct BurnDriver BurnDrvcv_chucknor = {
	"cv_chucknor", NULL, "cv_coleco", NULL, "1983",
	"Chuck Norris Superkicks\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_chucknorRomInfo, cv_chucknorRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Decathlon

static struct BurnRomInfo cv_decathlnRomDesc[] = {
	{ "decathln.1",	0x02000, 0xaa99fda4, BRF_PRG | BRF_ESS },
	{ "decathln.2",	0x02000, 0x42b76bc2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_decathln, cv_decathln, cv_coleco)
STD_ROM_FN(cv_decathln)

struct BurnDriver BurnDrvcv_decathln = {
	"cv_decathln", NULL, "cv_coleco", NULL, "1984",
	"Decathlon\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_decathlnRomInfo, cv_decathlnRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Donkey Kong

static struct BurnRomInfo cv_dkongRomDesc[] = {
	{ "dkong.1",	0x02000, 0x1a63176e, BRF_PRG | BRF_ESS },
	{ "dkong.2",	0x02000, 0xad6162cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkong, cv_dkong, cv_coleco)
STD_ROM_FN(cv_dkong)

struct BurnDriver BurnDrvcv_dkong = {
	"cv_dkong", NULL, "cv_coleco", NULL, "1982",
	"Donkey Kong\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dkongRomInfo, cv_dkongRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Donkey Kong Junior

static struct BurnRomInfo cv_dkongjrRomDesc[] = {
	{ "dkongjr.1",	0x02000, 0x2c3d41bc, BRF_PRG | BRF_ESS },
	{ "dkongjr.2",	0x02000, 0xc9be6a65, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkongjr, cv_dkongjr, cv_coleco)
STD_ROM_FN(cv_dkongjr)

struct BurnDriver BurnDrvcv_dkongjr = {
	"cv_dkongjr", NULL, "cv_coleco", NULL, "1983",
	"Donkey Kong Junior\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dkongjrRomInfo, cv_dkongjrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mr. Do!'s Castle

static struct BurnRomInfo cv_docastleRomDesc[] = {
	{ "docastle.1",	0x02000, 0x525a7d10, BRF_PRG | BRF_ESS },
	{ "docastle.2",	0x02000, 0xe46ce496, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_docastle, cv_docastle, cv_coleco)
STD_ROM_FN(cv_docastle)

struct BurnDriver BurnDrvcv_docastle = {
	"cv_docastle", NULL, "cv_coleco", NULL, "1983",
	"Mr. Do!'s Castle\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_docastleRomInfo, cv_docastleRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// DragonFire

static struct BurnRomInfo cv_drgnfireRomDesc[] = {
	{ "drgnfire.1",	0x02000, 0x4272d250, BRF_PRG | BRF_ESS },
	{ "drgnfire.2",	0x02000, 0xe68e9e70, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_drgnfire, cv_drgnfire, cv_coleco)
STD_ROM_FN(cv_drgnfire)

struct BurnDriver BurnDrvcv_drgnfire = {
	"cv_drgnfire", NULL, "cv_coleco", NULL, "1984",
	"DragonFire\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_drgnfireRomInfo, cv_drgnfireRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dr. Seuss's Fix-Up the Mix-Up Puzzler

static struct BurnRomInfo cv_drseussRomDesc[] = {
	{ "drseuss.1",	0x02000, 0x47cf6908, BRF_PRG | BRF_ESS },
	{ "drseuss.2",	0x02000, 0xb524f389, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_drseuss, cv_drseuss, cv_coleco)
STD_ROM_FN(cv_drseuss)

struct BurnDriver BurnDrvcv_drseuss = {
	"cv_drseuss", NULL, "cv_coleco", NULL, "1984",
	"Dr. Seuss's Fix-Up the Mix-Up Puzzler\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_drseussRomInfo, cv_drseussRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Evolution

static struct BurnRomInfo cv_evolutioRomDesc[] = {
	{ "evolutio.1",	0x02000, 0x75a9c817, BRF_PRG | BRF_ESS },
	{ "evolutio.2",	0x02000, 0x6e32b9de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_evolutio, cv_evolutio, cv_coleco)
STD_ROM_FN(cv_evolutio)

struct BurnDriver BurnDrvcv_evolutio = {
	"cv_evolutio", NULL, "cv_coleco", NULL, "1983",
	"Evolution\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_evolutioRomInfo, cv_evolutioRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fathom

static struct BurnRomInfo cv_fathomRomDesc[] = {
	{ "fathom.1",	0x02000, 0xbf04e505, BRF_PRG | BRF_ESS },
	{ "fathom.2",	0x02000, 0x606061c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fathom, cv_fathom, cv_coleco)
STD_ROM_FN(cv_fathom)

struct BurnDriver BurnDrvcv_fathom = {
	"cv_fathom", NULL, "cv_coleco", NULL, "1983",
	"Fathom\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_fathomRomInfo, cv_fathomRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Flipper Slipper

static struct BurnRomInfo cv_flipslipRomDesc[] = {
	{ "flipslip.1",	0x02000, 0xe26569f3, BRF_PRG | BRF_ESS },
	{ "flipslip.2",	0x02000, 0xd4ab0e71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_flipslip, cv_flipslip, cv_coleco)
STD_ROM_FN(cv_flipslip)

struct BurnDriver BurnDrvcv_flipslip = {
	"cv_flipslip", NULL, "cv_coleco", NULL, "1983",
	"Flipper Slipper\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_flipslipRomInfo, cv_flipslipRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frantic Freddy

static struct BurnRomInfo cv_ffreddyRomDesc[] = {
	{ "ffreddy.1",	0x02000, 0xe33ccf02, BRF_PRG | BRF_ESS },
	{ "ffreddy.2",	0x02000, 0xd3ece1bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ffreddy, cv_ffreddy, cv_coleco)
STD_ROM_FN(cv_ffreddy)

struct BurnDriver BurnDrvcv_ffreddy = {
	"cv_ffreddy", NULL, "cv_coleco", NULL, "1983",
	"Frantic Freddy\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ffreddyRomInfo, cv_ffreddyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frogger

static struct BurnRomInfo cv_froggerRomDesc[] = {
	{ "frogger.1",	0x02000, 0xa213cda1, BRF_PRG | BRF_ESS },
	{ "frogger.2",	0x02000, 0x1556c226, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frogger, cv_frogger, cv_coleco)
STD_ROM_FN(cv_frogger)

struct BurnDriver BurnDrvcv_frogger = {
	"cv_frogger", NULL, "cv_coleco", NULL, "1983",
	"Frogger\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_froggerRomInfo, cv_froggerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frogger II: Threedeep!

static struct BurnRomInfo cv_frogger2RomDesc[] = {
	{ "frogger2.1",	0x02000, 0xdd616176, BRF_PRG | BRF_ESS },
	{ "frogger2.2",	0x02000, 0x3275dc33, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frogger2, cv_frogger2, cv_coleco)
STD_ROM_FN(cv_frogger2)

struct BurnDriver BurnDrvcv_frogger2 = {
	"cv_frogger2", NULL, "cv_coleco", NULL, "1984",
	"Frogger II: Threedeep!\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frogger2RomInfo, cv_frogger2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gateway to Apshai

static struct BurnRomInfo cv_apshaiRomDesc[] = {
	{ "apshai.1",	0x02000, 0xaa3ec181, BRF_PRG | BRF_ESS },
	{ "apshai.2",	0x02000, 0x0e440f8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_apshai, cv_apshai, cv_coleco)
STD_ROM_FN(cv_apshai)

struct BurnDriver BurnDrvcv_apshai = {
	"cv_apshai", NULL, "cv_coleco", NULL, "1984",
	"Gateway to Apshai\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_apshaiRomInfo, cv_apshaiRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gorf

static struct BurnRomInfo cv_gorfRomDesc[] = {
	{ "gorf.1",	0x02000, 0xbe7b03b6, BRF_PRG | BRF_ESS },
	{ "gorf.2",	0x02000, 0xa75a408a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gorf, cv_gorf, cv_coleco)
STD_ROM_FN(cv_gorf)

struct BurnDriver BurnDrvcv_gorf = {
	"cv_gorf", NULL, "cv_coleco", NULL, "1983",
	"Gorf\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_gorfRomInfo, cv_gorfRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gust Buster

static struct BurnRomInfo cv_gustbustRomDesc[] = {
	{ "gustbust.1",	0x02000, 0x3fb1866e, BRF_PRG | BRF_ESS },
	{ "gustbust.2",	0x02000, 0x2dcf1da7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gustbust, cv_gustbust, cv_coleco)
STD_ROM_FN(cv_gustbust)

struct BurnDriver BurnDrvcv_gustbust = {
	"cv_gustbust", NULL, "cv_coleco", NULL, "1983",
	"Gust Buster\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_gustbustRomInfo, cv_gustbustRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Gyruss

static struct BurnRomInfo cv_gyrussRomDesc[] = {
	{ "gyruss.1",	0x02000, 0x05543060, BRF_PRG | BRF_ESS },
	{ "gyruss.2",	0x02000, 0x8efb3614, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gyruss, cv_gyruss, cv_coleco)
STD_ROM_FN(cv_gyruss)

struct BurnDriver BurnDrvcv_gyruss = {
	"cv_gyruss", NULL, "cv_coleco", NULL, "1984",
	"Gyruss\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_gyrussRomInfo, cv_gyrussRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// H.E.R.O.

static struct BurnRomInfo cv_heroRomDesc[] = {
	{ "hero.1",	0x02000, 0xdcc94c49, BRF_PRG | BRF_ESS },
	{ "hero.2",	0x02000, 0xc0012665, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hero, cv_hero, cv_coleco)
STD_ROM_FN(cv_hero)

struct BurnDriver BurnDrvcv_hero = {
	"cv_hero", NULL, "cv_coleco", NULL, "1984",
	"H.E.R.O.\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_heroRomInfo, cv_heroRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Illusions

static struct BurnRomInfo cv_illusionRomDesc[] = {
	{ "illusion.1",	0x02000, 0x2b694536, BRF_PRG | BRF_ESS },
	{ "illusion.2",	0x02000, 0x95a5dfa6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_illusion, cv_illusion, cv_coleco)
STD_ROM_FN(cv_illusion)

struct BurnDriver BurnDrvcv_illusion = {
	"cv_illusion", NULL, "cv_coleco", NULL, "1984",
	"Illusions\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_illusionRomInfo, cv_illusionRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// James Bond 007

static struct BurnRomInfo cv_jbondRomDesc[] = {
	{ "jbond.1",	0x02000, 0x3e8adbd1, BRF_PRG | BRF_ESS },
	{ "jbond.2",	0x02000, 0xd76746a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jbond, cv_jbond, cv_coleco)
STD_ROM_FN(cv_jbond)

struct BurnDriver BurnDrvcv_jbond = {
	"cv_jbond", NULL, "cv_coleco", NULL, "1984",
	"James Bond 007\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_jbondRomInfo, cv_jbondRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jumpman Junior

static struct BurnRomInfo cv_jmpmanjrRomDesc[] = {
	{ "jmpmanjr.1",	0x02000, 0x18936315, BRF_PRG | BRF_ESS },
	{ "jmpmanjr.2",	0x02000, 0x2af8cc37, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jmpmanjr, cv_jmpmanjr, cv_coleco)
STD_ROM_FN(cv_jmpmanjr)

struct BurnDriver BurnDrvcv_jmpmanjr = {
	"cv_jmpmanjr", NULL, "cv_coleco", NULL, "1984",
	"Jumpman Junior\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_jmpmanjrRomInfo, cv_jmpmanjrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jumpman Junior (Alt)

static struct BurnRomInfo cv_jmpmanjraRomDesc[] = {
	{ "jmpmanjr.1",	0x02000, 0x18936315, BRF_PRG | BRF_ESS },
	{ "jmpmanjra.2",	0x02000, 0xb9dc3145, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jmpmanjra, cv_jmpmanjra, cv_coleco)
STD_ROM_FN(cv_jmpmanjra)

struct BurnDriver BurnDrvcv_jmpmanjra = {
	"cv_jmpmanjra", "cv_jmpmanjr", "cv_coleco", NULL, "1984",
	"Jumpman Junior (Alt)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_jmpmanjraRomInfo, cv_jmpmanjraRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Ken Uston Blackjack-Poker

static struct BurnRomInfo cv_kubjpokRomDesc[] = {
	{ "kubjpok.1",	0x02000, 0x5b44f5da, BRF_PRG | BRF_ESS },
	{ "kubjpok.2",	0x02000, 0x3bbede0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kubjpok, cv_kubjpok, cv_coleco)
STD_ROM_FN(cv_kubjpok)

struct BurnDriver BurnDrvcv_kubjpok = {
	"cv_kubjpok", NULL, "cv_coleco", NULL, "1983",
	"Ken Uston Blackjack-Poker\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_kubjpokRomInfo, cv_kubjpokRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Keystone Kapers

static struct BurnRomInfo cv_keykaperRomDesc[] = {
	{ "keystone.1",	0x02000, 0x35776743, BRF_PRG | BRF_ESS },
	{ "keystone.2",	0x02000, 0x4b624fb2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_keykaper, cv_keykaper, cv_coleco)
STD_ROM_FN(cv_keykaper)

struct BurnDriver BurnDrvcv_keykaper = {
	"cv_keykaper", NULL, "cv_coleco", NULL, "1984",
	"Keystone Kapers\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_keykaperRomInfo, cv_keykaperRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Lady Bug

static struct BurnRomInfo cv_ladybugRomDesc[] = {
	{ "ladybug.1",	0x02000, 0x6e63f2ed, BRF_PRG | BRF_ESS },
	{ "ladybug.2",	0x02000, 0x147b94fe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ladybug, cv_ladybug, cv_coleco)
STD_ROM_FN(cv_ladybug)

struct BurnDriver BurnDrvcv_ladybug = {
	"cv_ladybug", NULL, "cv_coleco", NULL, "1982",
	"Lady Bug\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ladybugRomInfo, cv_ladybugRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sir Lancelot

static struct BurnRomInfo cv_lancelotRomDesc[] = {
	{ "lancelot.1",	0x02000, 0xe28346ed, BRF_PRG | BRF_ESS },
	{ "lancelot.2",	0x02000, 0x1156741b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_lancelot, cv_lancelot, cv_coleco)
STD_ROM_FN(cv_lancelot)

struct BurnDriver BurnDrvcv_lancelot = {
	"cv_lancelot", NULL, "cv_coleco", NULL, "1983",
	"Sir Lancelot\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_lancelotRomInfo, cv_lancelotRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Learning with Leeper

static struct BurnRomInfo cv_leeperRomDesc[] = {
	{ "leeper.1",	0x02000, 0x55dd8811, BRF_PRG | BRF_ESS },
	{ "leeper.2",	0x02000, 0xae0c0c1f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_leeper, cv_leeper, cv_coleco)
STD_ROM_FN(cv_leeper)

struct BurnDriver BurnDrvcv_leeper = {
	"cv_leeper", NULL, "cv_coleco", NULL, "1983",
	"Learning with Leeper\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_leeperRomInfo, cv_leeperRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Logic Levels

static struct BurnRomInfo cv_logiclvlRomDesc[] = {
	{ "logiclvl.1",	0x02000, 0xd54c581b, BRF_PRG | BRF_ESS },
	{ "logiclvl.2",	0x02000, 0x257aa944, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_logiclvl, cv_logiclvl, cv_coleco)
STD_ROM_FN(cv_logiclvl)

struct BurnDriver BurnDrvcv_logiclvl = {
	"cv_logiclvl", NULL, "cv_coleco", NULL, "1984",
	"Logic Levels\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_logiclvlRomInfo, cv_logiclvlRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Linking Logic

static struct BurnRomInfo cv_linklogcRomDesc[] = {
	{ "lnklogic.1",	0x02000, 0x918f12c0, BRF_PRG | BRF_ESS },
	{ "lnklogic.2",	0x02000, 0xd8f49994, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_linklogc, cv_linklogc, cv_coleco)
STD_ROM_FN(cv_linklogc)

struct BurnDriver BurnDrvcv_linklogc = {
	"cv_linklogc", NULL, "cv_coleco", NULL, "1984",
	"Linking Logic\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_linklogcRomInfo, cv_linklogcRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Looping

static struct BurnRomInfo cv_loopingRomDesc[] = {
	{ "looping.1",	0x02000, 0x205a9c61, BRF_PRG | BRF_ESS },
	{ "looping.2",	0x02000, 0x1b5ef49e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_looping, cv_looping, cv_coleco)
STD_ROM_FN(cv_looping)

struct BurnDriver BurnDrvcv_looping = {
	"cv_looping", NULL, "cv_coleco", NULL, "1983",
	"Looping\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_loopingRomInfo, cv_loopingRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Meteoric Shower

static struct BurnRomInfo cv_meteoshoRomDesc[] = {
	{ "meteosho.1",	0x02000, 0x6a162c7d, BRF_PRG | BRF_ESS },
	{ "meteosho.2",	0x02000, 0x4fd8264f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_meteosho, cv_meteosho, cv_coleco)
STD_ROM_FN(cv_meteosho)

struct BurnDriver BurnDrvcv_meteosho = {
	"cv_meteosho", NULL, "cv_coleco", NULL, "1983",
	"Meteoric Shower\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_meteoshoRomInfo, cv_meteoshoRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Montezuma's Revenge

static struct BurnRomInfo cv_montezumRomDesc[] = {
	{ "montezum.1",	0x02000, 0xc94a29af, BRF_PRG | BRF_ESS },
	{ "montezum.2",	0x02000, 0xc27dcc42, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_montezum, cv_montezum, cv_coleco)
STD_ROM_FN(cv_montezum)

struct BurnDriver BurnDrvcv_montezum = {
	"cv_montezum", NULL, "cv_coleco", NULL, "1984",
	"Montezuma's Revenge\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_montezumRomInfo, cv_montezumRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Moonsweeper

static struct BurnRomInfo cv_moonswprRomDesc[] = {
	{ "moonswpr.1",	0x02000, 0xcbb291b1, BRF_PRG | BRF_ESS },
	{ "moonswpr.2",	0x02000, 0xff1c95e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moonswpr, cv_moonswpr, cv_coleco)
STD_ROM_FN(cv_moonswpr)

struct BurnDriver BurnDrvcv_moonswpr = {
	"cv_moonswpr", NULL, "cv_coleco", NULL, "1983",
	"Moonsweeper\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_moonswprRomInfo, cv_moonswprRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Moonsweeper (Alt)

static struct BurnRomInfo cv_moonswpraRomDesc[] = {
	{ "moonswpr.1",	0x02000, 0xcbb291b1, BRF_PRG | BRF_ESS },
	{ "moonswpra.2",	0x02000, 0xb00d161f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moonswpra, cv_moonswpra, cv_coleco)
STD_ROM_FN(cv_moonswpra)

struct BurnDriver BurnDrvcv_moonswpra = {
	"cv_moonswpra", "cv_moonswpr", "cv_coleco", NULL, "1983",
	"Moonsweeper (Alt)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_moonswpraRomInfo, cv_moonswpraRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mountain King

static struct BurnRomInfo cv_mkingRomDesc[] = {
	{ "mking.1",	0x02000, 0xc184c59f, BRF_PRG | BRF_ESS },
	{ "mking.2",	0x02000, 0x4004519c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mking, cv_mking, cv_coleco)
STD_ROM_FN(cv_mking)

struct BurnDriver BurnDrvcv_mking = {
	"cv_mking", NULL, "cv_coleco", NULL, "1984",
	"Mountain King\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mkingRomInfo, cv_mkingRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mountain King (Alt)

static struct BurnRomInfo cv_mkingaRomDesc[] = {
	{ "mking.1",	0x02000, 0xc184c59f, BRF_PRG | BRF_ESS },
	{ "mkinga.2",	0x02000, 0x59a0d836, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mkinga, cv_mkinga, cv_coleco)
STD_ROM_FN(cv_mkinga)

struct BurnDriver BurnDrvcv_mkinga = {
	"cv_mkinga", "cv_mking", "cv_coleco", NULL, "1984",
	"Mountain King (Alt)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mkingaRomInfo, cv_mkingaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Motocross Racer

static struct BurnRomInfo cv_mtcracerRomDesc[] = {
	{ "mtcracer.1",	0x02000, 0x66472edc, BRF_PRG | BRF_ESS },
	{ "mtcracer.2",	0x02000, 0x94a06c6f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtcracer, cv_mtcracer, cv_coleco)
STD_ROM_FN(cv_mtcracer)

struct BurnDriver BurnDrvcv_mtcracer = {
	"cv_mtcracer", NULL, "cv_coleco", NULL, "1984",
	"Motocross Racer\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mtcracerRomInfo, cv_mtcracerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Motocross Racer (Alt)

static struct BurnRomInfo cv_mtcraceraRomDesc[] = {
	{ "mtcracera.1",	0x02000, 0x868d6c01, BRF_PRG | BRF_ESS },
	{ "mtcracer.2",	0x02000, 0x94a06c6f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtcracera, cv_mtcracera, cv_coleco)
STD_ROM_FN(cv_mtcracera)

struct BurnDriver BurnDrvcv_mtcracera = {
	"cv_mtcracera", "cv_mtcracer", "cv_coleco", NULL, "1984",
	"Motocross Racer (Alt)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mtcraceraRomInfo, cv_mtcraceraRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mousetrap

static struct BurnRomInfo cv_mtrapRomDesc[] = {
	{ "mtrap.1",	0x02000, 0xc99d687f, BRF_PRG | BRF_ESS },
	{ "mtrap.2",	0x02000, 0x0dde86c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtrap, cv_mtrap, cv_coleco)
STD_ROM_FN(cv_mtrap)

struct BurnDriver BurnDrvcv_mtrap = {
	"cv_mtrap", NULL, "cv_coleco", NULL, "1982",
	"Mousetrap\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mtrapRomInfo, cv_mtrapRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mousetrap (Alt)

static struct BurnRomInfo cv_mtrapaRomDesc[] = {
	{ "mtrap.1",	0x02000, 0xc99d687f, BRF_PRG | BRF_ESS },
	{ "mtrapa.2",	0x02000, 0xf7b51bd5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtrapa, cv_mtrapa, cv_coleco)
STD_ROM_FN(cv_mtrapa)

struct BurnDriver BurnDrvcv_mtrapa = {
	"cv_mtrapa", "cv_mtrap", "cv_coleco", NULL, "1982",
	"Mousetrap (Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mtrapaRomInfo, cv_mtrapaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Nova Blast

static struct BurnRomInfo cv_novablstRomDesc[] = {
	{ "novablst.1",	0x02000, 0x790433cf, BRF_PRG | BRF_ESS },
	{ "novablst.2",	0x02000, 0x4f4dd2dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_novablst, cv_novablst, cv_coleco)
STD_ROM_FN(cv_novablst)

struct BurnDriver BurnDrvcv_novablst = {
	"cv_novablst", NULL, "cv_coleco", NULL, "1983",
	"Nova Blast\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_novablstRomInfo, cv_novablstRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Oil's Well

static struct BurnRomInfo cv_oilswellRomDesc[] = {
	{ "oilswell.1",	0x02000, 0xb8cccf31, BRF_PRG | BRF_ESS },
	{ "oilswell.2",	0x02000, 0xcd2da143, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_oilswell, cv_oilswell, cv_coleco)
STD_ROM_FN(cv_oilswell)

struct BurnDriver BurnDrvcv_oilswell = {
	"cv_oilswell", NULL, "cv_coleco", NULL, "1984",
	"Oil's Well\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_oilswellRomInfo, cv_oilswellRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Oil's Well (Alt)

static struct BurnRomInfo cv_oilswellaRomDesc[] = {
	{ "oilswell.1",	0x02000, 0xb8cccf31, BRF_PRG | BRF_ESS },
	{ "oilswella.2",	0x02000, 0xb2d9c86d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_oilswella, cv_oilswella, cv_coleco)
STD_ROM_FN(cv_oilswella)

struct BurnDriver BurnDrvcv_oilswella = {
	"cv_oilswella", "cv_oilswell", "cv_coleco", NULL, "1984",
	"Oil's Well (Alt)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_oilswellaRomInfo, cv_oilswellaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Omega Race

static struct BurnRomInfo cv_omegraceRomDesc[] = {
	{ "omegrace.1",	0x02000, 0x0abd47e7, BRF_PRG | BRF_ESS },
	{ "omegrace.2",	0x02000, 0x8aba596b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_omegrace, cv_omegrace, cv_coleco)
STD_ROM_FN(cv_omegrace)

struct BurnDriver BurnDrvcv_omegrace = {
	"cv_omegrace", NULL, "cv_coleco", NULL, "1983",
	"Omega Race\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_omegraceRomInfo, cv_omegraceRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// It's Only Rock 'n' Roll

static struct BurnRomInfo cv_onlyrockRomDesc[] = {
	{ "onlyrock.1",	0x02000, 0x93d46b70, BRF_PRG | BRF_ESS },
	{ "onlyrock.2",	0x02000, 0x2bfc5325, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_onlyrock, cv_onlyrock, cv_coleco)
STD_ROM_FN(cv_onlyrock)

struct BurnDriver BurnDrvcv_onlyrock = {
	"cv_onlyrock", NULL, "cv_coleco", NULL, "1984",
	"It's Only Rock 'n' Roll\0", NULL, "K-Tel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_onlyrockRomInfo, cv_onlyrockRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Panic

static struct BurnRomInfo cv_panicRomDesc[] = {
	{ "panic.1",	0x02000, 0xe06fa55b, BRF_PRG | BRF_ESS },
	{ "panic.2",	0x02000, 0x66fcda90, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_panic, cv_panic, cv_coleco)
STD_ROM_FN(cv_panic)

struct BurnDriver BurnDrvcv_panic = {
	"cv_panic", NULL, "cv_coleco", NULL, "1983",
	"Space Panic\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_panicRomInfo, cv_panicRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pepper II

static struct BurnRomInfo cv_pepper2RomDesc[] = {
	{ "pepper2.1",	0x02000, 0x2ea3deb5, BRF_PRG | BRF_ESS },
	{ "pepper2.2",	0x02000, 0xcd31ba03, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pepper2, cv_pepper2, cv_coleco)
STD_ROM_FN(cv_pepper2)

struct BurnDriver BurnDrvcv_pepper2 = {
	"cv_pepper2", NULL, "cv_coleco", NULL, "1983",
	"Pepper II\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pepper2RomInfo, cv_pepper2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitfall!

static struct BurnRomInfo cv_pitfallRomDesc[] = {
	{ "pitfall.1",	0x02000, 0x27bc4115, BRF_PRG | BRF_ESS },
	{ "pitfall.2",	0x02000, 0x24c3fc26, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitfall, cv_pitfall, cv_coleco)
STD_ROM_FN(cv_pitfall)

struct BurnDriver BurnDrvcv_pitfall = {
	"cv_pitfall", NULL, "cv_coleco", NULL, "1983",
	"Pitfall!\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pitfallRomInfo, cv_pitfallRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitfall II: Lost Caverns

static struct BurnRomInfo cv_pitfall2RomDesc[] = {
	{ "pitfall2.1",	0x02000, 0x08ad596e, BRF_PRG | BRF_ESS },
	{ "pitfall2.2",	0x02000, 0xe750c172, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitfall2, cv_pitfall2, cv_coleco)
STD_ROM_FN(cv_pitfall2)

struct BurnDriver BurnDrvcv_pitfall2 = {
	"cv_pitfall2", NULL, "cv_coleco", NULL, "1984",
	"Pitfall II: Lost Caverns\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pitfall2RomInfo, cv_pitfall2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitstop

static struct BurnRomInfo cv_pitstopRomDesc[] = {
	{ "pitstop.1",	0x02000, 0x9480724e, BRF_PRG | BRF_ESS },
	{ "pitstop.2",	0x02000, 0xce8e9e7e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitstop, cv_pitstop, cv_coleco)
STD_ROM_FN(cv_pitstop)

struct BurnDriver BurnDrvcv_pitstop = {
	"cv_pitstop", NULL, "cv_coleco", NULL, "1983",
	"Pitstop\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pitstopRomInfo, cv_pitstopRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pitstop (Alt)

static struct BurnRomInfo cv_pitstopaRomDesc[] = {
	{ "pitstop.1",	0x02000, 0x9480724e, BRF_PRG | BRF_ESS },
	{ "pitstopa.2",	0x02000, 0xa9172ddb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitstopa, cv_pitstopa, cv_coleco)
STD_ROM_FN(cv_pitstopa)

struct BurnDriver BurnDrvcv_pitstopa = {
	"cv_pitstopa", "cv_pitstop", "cv_coleco", NULL, "1983",
	"Pitstop (Alt)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pitstopaRomInfo, cv_pitstopaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Popeye

static struct BurnRomInfo cv_popeyeRomDesc[] = {
	{ "popeye.1",	0x02000, 0x0cbf4b76, BRF_PRG | BRF_ESS },
	{ "popeye.2",	0x02000, 0xf1cf5153, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_popeye, cv_popeye, cv_coleco)
STD_ROM_FN(cv_popeye)

struct BurnDriver BurnDrvcv_popeye = {
	"cv_popeye", NULL, "cv_coleco", NULL, "1983",
	"Popeye\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_popeyeRomInfo, cv_popeyeRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Popeye (Alt)

static struct BurnRomInfo cv_popeyeaRomDesc[] = {
	{ "popeye.1",	0x02000, 0x0cbf4b76, BRF_PRG | BRF_ESS },
	{ "popeyea.2",	0x02000, 0x6fabc4f0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_popeyea, cv_popeyea, cv_coleco)
STD_ROM_FN(cv_popeyea)

struct BurnDriver BurnDrvcv_popeyea = {
	"cv_popeyea", "cv_popeye", "cv_coleco", NULL, "1983",
	"Popeye (Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_popeyeaRomInfo, cv_popeyeaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Q*bert Qubes

static struct BurnRomInfo cv_qbertqubRomDesc[] = {
	{ "qbertqub.1",	0x02000, 0xf98f9356, BRF_PRG | BRF_ESS },
	{ "qbertqub.2",	0x02000, 0x6c620927, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qbertqub, cv_qbertqub, cv_coleco)
STD_ROM_FN(cv_qbertqub)

struct BurnDriver BurnDrvcv_qbertqub = {
	"cv_qbertqub", NULL, "cv_coleco", NULL, "1984",
	"Q*bert Qubes\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_qbertqubRomInfo, cv_qbertqubRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Quest for Quintana Roo

static struct BurnRomInfo cv_quintanaRomDesc[] = {
	{ "quintana.1",	0x02000, 0x4e0c1380, BRF_PRG | BRF_ESS },
	{ "quintana.2",	0x02000, 0xb9a51e9d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_quintana, cv_quintana, cv_coleco)
STD_ROM_FN(cv_quintana)

struct BurnDriver BurnDrvcv_quintana = {
	"cv_quintana", NULL, "cv_coleco", NULL, "1983",
	"Quest for Quintana Roo\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_quintanaRomInfo, cv_quintanaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Quest for Quintana Roo (Alt)

static struct BurnRomInfo cv_quintanaaRomDesc[] = {
	{ "quintana.1",	0x02000, 0x4e0c1380, BRF_PRG | BRF_ESS },
	{ "quintanaa.2",	0x02000, 0x7a5fb32f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_quintanaa, cv_quintanaa, cv_coleco)
STD_ROM_FN(cv_quintanaa)

struct BurnDriver BurnDrvcv_quintanaa = {
	"cv_quintanaa", "cv_quintana", "cv_coleco", NULL, "1983",
	"Quest for Quintana Roo (Alt)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_quintanaaRomInfo, cv_quintanaaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// River Raid

static struct BurnRomInfo cv_riveraidRomDesc[] = {
	{ "riveraid.1",	0x02000, 0x75640b2a, BRF_PRG | BRF_ESS },
	{ "riveraid.2",	0x02000, 0x5a3305e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_riveraid, cv_riveraid, cv_coleco)
STD_ROM_FN(cv_riveraid)

struct BurnDriver BurnDrvcv_riveraid = {
	"cv_riveraid", NULL, "cv_coleco", NULL, "1984",
	"River Raid\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_riveraidRomInfo, cv_riveraidRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Robin Hood

static struct BurnRomInfo cv_robinhRomDesc[] = {
	{ "robinh.1",	0x02000, 0x47030356, BRF_PRG | BRF_ESS },
	{ "robinh.2",	0x02000, 0x100b753c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robinh, cv_robinh, cv_coleco)
STD_ROM_FN(cv_robinh)

struct BurnDriver BurnDrvcv_robinh = {
	"cv_robinh", NULL, "cv_coleco", NULL, "1984",
	"Robin Hood\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_robinhRomInfo, cv_robinhRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Robin Hood (Alt)

static struct BurnRomInfo cv_robinhaRomDesc[] = {
	{ "robinh.1",	0x02000, 0x47030356, BRF_PRG | BRF_ESS },
	{ "robinha.2",	0x02000, 0xd307fb9d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robinha, cv_robinha, cv_coleco)
STD_ROM_FN(cv_robinha)

struct BurnDriver BurnDrvcv_robinha = {
	"cv_robinha", "cv_robinh", "cv_coleco", NULL, "1984",
	"Robin Hood (Alt)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_robinhaRomInfo, cv_robinhaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rock 'n Bolt

static struct BurnRomInfo cv_rockboltRomDesc[] = {
	{ "rockbolt.1",	0x02000, 0xd47a9aa5, BRF_PRG | BRF_ESS },
	{ "rockbolt.2",	0x02000, 0x2e1da551, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rockbolt, cv_rockbolt, cv_coleco)
STD_ROM_FN(cv_rockbolt)

struct BurnDriver BurnDrvcv_rockbolt = {
	"cv_rockbolt", NULL, "cv_coleco", NULL, "1984",
	"Rock 'n Bolt\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_rockboltRomInfo, cv_rockboltRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rock 'n Bolt (Alt)

static struct BurnRomInfo cv_rockboltaRomDesc[] = {
	{ "rockbolt.1",	0x02000, 0xd47a9aa5, BRF_PRG | BRF_ESS },
	{ "rockbolta.2",	0x02000, 0xd37f5c2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rockbolta, cv_rockbolta, cv_coleco)
STD_ROM_FN(cv_rockbolta)

struct BurnDriver BurnDrvcv_rockbolta = {
	"cv_rockbolta", "cv_rockbolt", "cv_coleco", NULL, "1984",
	"Rock 'n Bolt (Alt)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_rockboltaRomInfo, cv_rockboltaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rolloverture

static struct BurnRomInfo cv_rolloverRomDesc[] = {
	{ "rollover.1",	0x02000, 0x668b6bcb, BRF_PRG | BRF_ESS },
	{ "rollover.2",	0x02000, 0xb3dc2195, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rollover, cv_rollover, cv_coleco)
STD_ROM_FN(cv_rollover)

struct BurnDriver BurnDrvcv_rollover = {
	"cv_rollover", NULL, "cv_coleco", NULL, "1983",
	"Rolloverture\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_rolloverRomInfo, cv_rolloverRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sammy Lightfoot

static struct BurnRomInfo cv_sammylfRomDesc[] = {
	{ "sammylf.1",	0x02000, 0x2492bac2, BRF_PRG | BRF_ESS },
	{ "sammylf.2",	0x02000, 0x7fee3b34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sammylf, cv_sammylf, cv_coleco)
STD_ROM_FN(cv_sammylf)

struct BurnDriver BurnDrvcv_sammylf = {
	"cv_sammylf", NULL, "cv_coleco", NULL, "1983",
	"Sammy Lightfoot\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sammylfRomInfo, cv_sammylfRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sammy Lightfoot (Alt)

static struct BurnRomInfo cv_sammylfaRomDesc[] = {
	{ "sammylf.1",	0x02000, 0x2492bac2, BRF_PRG | BRF_ESS },
	{ "sammylfa.2",	0x02000, 0x8f7b8944, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sammylfa, cv_sammylfa, cv_coleco)
STD_ROM_FN(cv_sammylfa)

struct BurnDriver BurnDrvcv_sammylfa = {
	"cv_sammylfa", "cv_sammylf", "cv_coleco", NULL, "1983",
	"Sammy Lightfoot (Alt)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sammylfaRomInfo, cv_sammylfaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Slither

static struct BurnRomInfo cv_slitherRomDesc[] = {
	{ "slither.1",	0x02000, 0xadc3207c, BRF_PRG | BRF_ESS },
	{ "slither.2",	0x02000, 0xe9d2763c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_slither, cv_slither, cv_coleco)
STD_ROM_FN(cv_slither)

struct BurnDriver BurnDrvcv_slither = {
	"cv_slither", NULL, "cv_coleco", NULL, "1983",
	"Slither\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_slitherRomInfo, cv_slitherRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Slurpy

static struct BurnRomInfo cv_slurpyRomDesc[] = {
	{ "slurpy.1",	0x02000, 0xcb23c846, BRF_PRG | BRF_ESS },
	{ "slurpy.2",	0x02000, 0xadcda8e3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_slurpy, cv_slurpy, cv_coleco)
STD_ROM_FN(cv_slurpy)

struct BurnDriver BurnDrvcv_slurpy = {
	"cv_slurpy", NULL, "cv_coleco", NULL, "1984",
	"Slurpy\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_slurpyRomInfo, cv_slurpyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Rescue in Gargamel's Castle

static struct BurnRomInfo cv_smurfRomDesc[] = {
	{ "smurfrgc.1",	0x02000, 0x675bf14d, BRF_PRG | BRF_ESS },
	{ "smurfrgc.2",	0x02000, 0x0a1a2b0e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurf, cv_smurf, cv_coleco)
STD_ROM_FN(cv_smurf)

struct BurnDriver BurnDrvcv_smurf = {
	"cv_smurf", NULL, "cv_coleco", NULL, "1982",
	"Smurf Rescue in Gargamel's Castle\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfRomInfo, cv_smurfRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Rescue in Gargamel's Castle (Alt)

static struct BurnRomInfo cv_smurfaRomDesc[] = {
	{ "smurfrgc.1",	0x02000, 0x675bf14d, BRF_PRG | BRF_ESS },
	{ "smurfrgca.2",	0x02000, 0x993ed67c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfa, cv_smurfa, cv_coleco)
STD_ROM_FN(cv_smurfa)

struct BurnDriver BurnDrvcv_smurfa = {
	"cv_smurfa", "cv_smurf", "cv_coleco", NULL, "1982",
	"Smurf Rescue in Gargamel's Castle (Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfaRomInfo, cv_smurfaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Fury

static struct BurnRomInfo cv_spacfuryRomDesc[] = {
	{ "spacfury.1",	0x02000, 0x1850548f, BRF_PRG | BRF_ESS },
	{ "spacfury.2",	0x02000, 0x4d6866e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spacfury, cv_spacfury, cv_coleco)
STD_ROM_FN(cv_spacfury)

struct BurnDriver BurnDrvcv_spacfury = {
	"cv_spacfury", NULL, "cv_coleco", NULL, "1983",
	"Space Fury\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_spacfuryRomInfo, cv_spacfuryRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Spectron

static struct BurnRomInfo cv_spectronRomDesc[] = {
	{ "spectron.1",	0x02000, 0x23e2afa0, BRF_PRG | BRF_ESS },
	{ "spectron.2",	0x02000, 0x01897fc1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spectron, cv_spectron, cv_coleco)
STD_ROM_FN(cv_spectron)

struct BurnDriver BurnDrvcv_spectron = {
	"cv_spectron", NULL, "cv_coleco", NULL, "1983",
	"Spectron\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_spectronRomInfo, cv_spectronRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cross Force

static struct BurnRomInfo cv_sprcrossRomDesc[] = {
	{ "sprcross.1",	0x02000, 0xe5a53b79, BRF_PRG | BRF_ESS },
	{ "sprcross.2",	0x02000, 0x5185bd94, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sprcross, cv_sprcross, cv_coleco)
STD_ROM_FN(cv_sprcross)

struct BurnDriver BurnDrvcv_sprcross = {
	"cv_sprcross", NULL, "cv_coleco", NULL, "1983",
	"Super Cross Force\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sprcrossRomInfo, cv_sprcrossRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cross Force (Alt)

static struct BurnRomInfo cv_sprcrossaRomDesc[] = {
	{ "sprcross.1",	0x02000, 0xe5a53b79, BRF_PRG | BRF_ESS },
	{ "sprcrossa.2",	0x02000, 0xbd58d3e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sprcrossa, cv_sprcrossa, cv_coleco)
STD_ROM_FN(cv_sprcrossa)

struct BurnDriver BurnDrvcv_sprcrossa = {
	"cv_sprcrossa", "cv_sprcross", "cv_coleco", NULL, "1983",
	"Super Cross Force (Alt)\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sprcrossaRomInfo, cv_sprcrossaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Squish 'em Sam!

static struct BurnRomInfo cv_squishemRomDesc[] = {
	{ "squishem.1",	0x02000, 0x2614d406, BRF_PRG | BRF_ESS },
	{ "squishem.2",	0x02000, 0xb1ce0286, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_squishem, cv_squishem, cv_coleco)
STD_ROM_FN(cv_squishem)

struct BurnDriver BurnDrvcv_squishem = {
	"cv_squishem", NULL, "cv_coleco", NULL, "1984",
	"Squish 'em Sam!\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_squishemRomInfo, cv_squishemRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Wars: The Arcade Game

static struct BurnRomInfo cv_starwarsRomDesc[] = {
	{ "starwars.1",	0x02000, 0xa0ea5c68, BRF_PRG | BRF_ESS },
	{ "starwars.2",	0x02000, 0xe7d55444, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starwars, cv_starwars, cv_coleco)
STD_ROM_FN(cv_starwars)

struct BurnDriver BurnDrvcv_starwars = {
	"cv_starwars", NULL, "cv_coleco", NULL, "1984",
	"Star Wars: The Arcade Game\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_starwarsRomInfo, cv_starwarsRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Strike It!

static struct BurnRomInfo cv_strikeitRomDesc[] = {
	{ "strikeit.1",	0x02000, 0x7b26040d, BRF_PRG | BRF_ESS },
	{ "strikeit.2",	0x02000, 0x3a2d6226, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_strikeit, cv_strikeit, cv_coleco)
STD_ROM_FN(cv_strikeit)

struct BurnDriver BurnDrvcv_strikeit = {
	"cv_strikeit", NULL, "cv_coleco", NULL, "1983",
	"Strike It!\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_strikeitRomInfo, cv_strikeitRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tank Wars

static struct BurnRomInfo cv_tankwarsRomDesc[] = {
	{ "tankwars.1",	0x02000, 0x9ab82448, BRF_PRG | BRF_ESS },
	{ "tankwars.2",	0x02000, 0x829cce2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tankwars, cv_tankwars, cv_coleco)
STD_ROM_FN(cv_tankwars)

struct BurnDriver BurnDrvcv_tankwars = {
	"cv_tankwars", NULL, "cv_coleco", NULL, "1983",
	"Tank Wars\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tankwarsRomInfo, cv_tankwarsRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Telly Turtle

static struct BurnRomInfo cv_tellyRomDesc[] = {
	{ "telly.1",	0x02000, 0x2d18a9f3, BRF_PRG | BRF_ESS },
	{ "telly.2",	0x02000, 0xc031f478, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_telly, cv_telly, cv_coleco)
STD_ROM_FN(cv_telly)

struct BurnDriver BurnDrvcv_telly = {
	"cv_telly", NULL, "cv_coleco", NULL, "1984",
	"Telly Turtle\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tellyRomInfo, cv_tellyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Threshold

static struct BurnRomInfo cv_threshldRomDesc[] = {
	{ "threshld.1",	0x02000, 0x5575f9a7, BRF_PRG | BRF_ESS },
	{ "threshld.2",	0x02000, 0x502e5505, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_threshld, cv_threshld, cv_coleco)
STD_ROM_FN(cv_threshld)

struct BurnDriver BurnDrvcv_threshld = {
	"cv_threshld", NULL, "cv_coleco", NULL, "1983",
	"Threshold\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_threshldRomInfo, cv_threshldRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Time Pilot

static struct BurnRomInfo cv_timepltRomDesc[] = {
	{ "timeplt.1",	0x02000, 0xc7dbf3f4, BRF_PRG | BRF_ESS },
	{ "timeplt.2",	0x02000, 0x0103b17c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_timeplt, cv_timeplt, cv_coleco)
STD_ROM_FN(cv_timeplt)

struct BurnDriver BurnDrvcv_timeplt = {
	"cv_timeplt", NULL, "cv_coleco", NULL, "1983",
	"Time Pilot\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_timepltRomInfo, cv_timepltRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tomarc the Barbarian

static struct BurnRomInfo cv_tomarcRomDesc[] = {
	{ "tomarc.1",	0x02000, 0x938681c2, BRF_PRG | BRF_ESS },
	{ "tomarc.2",	0x02000, 0x58fc365b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tomarc, cv_tomarc, cv_coleco)
STD_ROM_FN(cv_tomarc)

struct BurnDriver BurnDrvcv_tomarc = {
	"cv_tomarc", NULL, "cv_coleco", NULL, "1984",
	"Tomarc the Barbarian\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tomarcRomInfo, cv_tomarcRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Turbo

static struct BurnRomInfo cv_turboRomDesc[] = {
	{ "turbo.1",	0x02000, 0x379db77c, BRF_PRG | BRF_ESS },
	{ "turbo.2",	0x02000, 0x8d49046a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_turbo, cv_turbo, cv_coleco)
STD_ROM_FN(cv_turbo)

struct BurnDriver BurnDrvcv_turbo = {
	"cv_turbo", NULL, "cv_coleco", NULL, "1982",
	"Turbo\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_turboRomInfo, cv_turboRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tutankham

static struct BurnRomInfo cv_tutankhmRomDesc[] = {
	{ "tutankhm.1",	0x02000, 0x8186ee58, BRF_PRG | BRF_ESS },
	{ "tutankhm.2",	0x02000, 0xc84f9171, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tutankhm, cv_tutankhm, cv_coleco)
STD_ROM_FN(cv_tutankhm)

struct BurnDriver BurnDrvcv_tutankhm = {
	"cv_tutankhm", NULL, "cv_coleco", NULL, "1983",
	"Tutankham\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tutankhmRomInfo, cv_tutankhmRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tutankham (Alt)

static struct BurnRomInfo cv_tutankhmaRomDesc[] = {
	{ "tutankhm.1",	0x02000, 0x8186ee58, BRF_PRG | BRF_ESS },
	{ "tutankhma.2",	0x02000, 0x208eb5a2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tutankhma, cv_tutankhma, cv_coleco)
STD_ROM_FN(cv_tutankhma)

struct BurnDriver BurnDrvcv_tutankhma = {
	"cv_tutankhma", "cv_tutankhm", "cv_coleco", NULL, "1983",
	"Tutankham (Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tutankhmaRomInfo, cv_tutankhmaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Up'n Down

static struct BurnRomInfo cv_upndownRomDesc[] = {
	{ "upndown.1",	0x02000, 0x20020b8c, BRF_PRG | BRF_ESS },
	{ "upndown.2",	0x02000, 0xecc346e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_upndown, cv_upndown, cv_coleco)
STD_ROM_FN(cv_upndown)

struct BurnDriver BurnDrvcv_upndown = {
	"cv_upndown", NULL, "cv_coleco", NULL, "1984",
	"Up'n Down\0", NULL, "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_upndownRomInfo, cv_upndownRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Venture

static struct BurnRomInfo cv_ventureRomDesc[] = {
	{ "venture.1",	0x02000, 0xd1975c29, BRF_PRG | BRF_ESS },
	{ "venture.2",	0x02000, 0x2fff758e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_venture, cv_venture, cv_coleco)
STD_ROM_FN(cv_venture)

struct BurnDriver BurnDrvcv_venture = {
	"cv_venture", NULL, "cv_coleco", NULL, "1982",
	"Venture\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ventureRomInfo, cv_ventureRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Wing War

static struct BurnRomInfo cv_wingwarRomDesc[] = {
	{ "wingwar.1",	0x02000, 0x9aaba834, BRF_PRG | BRF_ESS },
	{ "wingwar.2",	0x02000, 0x442000d0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wingwar, cv_wingwar, cv_coleco)
STD_ROM_FN(cv_wingwar)

struct BurnDriver BurnDrvcv_wingwar = {
	"cv_wingwar", NULL, "cv_coleco", NULL, "1983",
	"Wing War\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_wingwarRomInfo, cv_wingwarRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Wizard of Id's Wizmath

static struct BurnRomInfo cv_wizmathRomDesc[] = {
	{ "wizmath.1",	0x02000, 0xc0c6bda0, BRF_PRG | BRF_ESS },
	{ "wizmath.2",	0x02000, 0x4080c0a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wizmath, cv_wizmath, cv_coleco)
STD_ROM_FN(cv_wizmath)

struct BurnDriver BurnDrvcv_wizmath = {
	"cv_wizmath", NULL, "cv_coleco", NULL, "1984",
	"Wizard of Id's Wizmath\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_wizmathRomInfo, cv_wizmathRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Zenji

static struct BurnRomInfo cv_zenjiRomDesc[] = {
	{ "zenji.1",	0x02000, 0xc3bde56a, BRF_PRG | BRF_ESS },
	{ "zenji.2",	0x02000, 0xd2a19d28, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zenji, cv_zenji, cv_coleco)
STD_ROM_FN(cv_zenji)

struct BurnDriver BurnDrvcv_zenji = {
	"cv_zenji", NULL, "cv_coleco", NULL, "1984",
	"Zenji\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_zenjiRomInfo, cv_zenjiRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// One on One

static struct BurnRomInfo cv_1on1RomDesc[] = {
	{ "1on1.1",	0x02000, 0xbabea7d6, BRF_PRG | BRF_ESS },
	{ "1on1.2",	0x02000, 0x568ffb61, BRF_PRG | BRF_ESS },
	{ "1on1.3",	0x02000, 0x575c9eae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_1on1, cv_1on1, cv_coleco)
STD_ROM_FN(cv_1on1)

struct BurnDriver BurnDrvcv_1on1 = {
	"cv_1on1", NULL, "cv_coleco", NULL, "1984",
	"One on One\0", NULL, "Micro Lab", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_1on1RomInfo, cv_1on1RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// BC's Quest for Tires II: Grog's Revenge

static struct BurnRomInfo cv_bcquest2RomDesc[] = {
	{ "bcquest2.1",	0x02000, 0x6bd84eb0, BRF_PRG | BRF_ESS },
	{ "bcquest2.2",	0x02000, 0x2ffa50a8, BRF_PRG | BRF_ESS },
	{ "bcquest2.3",	0x02000, 0x4b909485, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest2, cv_bcquest2, cv_coleco)
STD_ROM_FN(cv_bcquest2)

struct BurnDriver BurnDrvcv_bcquest2 = {
	"cv_bcquest2", NULL, "cv_coleco", NULL, "1984",
	"BC's Quest for Tires II: Grog's Revenge\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bcquest2RomInfo, cv_bcquest2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// BC's Quest for Tires II: Grog's Revenge (Can)

static struct BurnRomInfo cv_bcquest2caRomDesc[] = {
	{ "bcquest2ca.1",	0x02000, 0x8898dfc3, BRF_PRG | BRF_ESS },
	{ "bcquest2ca.2",	0x02000, 0xc42f20bd, BRF_PRG | BRF_ESS },
	{ "bcquest2ca.3",	0x02000, 0x117dedbe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest2ca, cv_bcquest2ca, cv_coleco)
STD_ROM_FN(cv_bcquest2ca)

struct BurnDriver BurnDrvcv_bcquest2ca = {
	"cv_bcquest2ca", "cv_bcquest2", "cv_coleco", NULL, "1984",
	"BC's Quest for Tires II: Grog's Revenge (Can)\0", NULL, "Coleco Canada", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bcquest2caRomInfo, cv_bcquest2caRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Bump 'n' Jump

static struct BurnRomInfo cv_bnjRomDesc[] = {
	{ "bnj.1",	0x02000, 0x92b052f8, BRF_PRG | BRF_ESS },
	{ "bnj.2",	0x02000, 0x05297263, BRF_PRG | BRF_ESS },
	{ "bnj.3",	0x02000, 0xc8f6efc1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bnj, cv_bnj, cv_coleco)
STD_ROM_FN(cv_bnj)

struct BurnDriver BurnDrvcv_bnj = {
	"cv_bnj", NULL, "cv_coleco", NULL, "1984",
	"Bump 'n' Jump\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bnjRomInfo, cv_bnjRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Buck Rogers: Planet of Zoom

static struct BurnRomInfo cv_buckrogRomDesc[] = {
	{ "buckrog.1",	0x02000, 0xceb94075, BRF_PRG | BRF_ESS },
	{ "buckrog.2",	0x02000, 0x6fe3a6a0, BRF_PRG | BRF_ESS },
	{ "buckrog.3",	0x02000, 0x7f93542b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_buckrog, cv_buckrog, cv_coleco)
STD_ROM_FN(cv_buckrog)

struct BurnDriver BurnDrvcv_buckrog = {
	"cv_buckrog", NULL, "cv_coleco", NULL, "1983",
	"Buck Rogers: Planet of Zoom\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_buckrogRomInfo, cv_buckrogRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Congo Bongo

static struct BurnRomInfo cv_congoRomDesc[] = {
	{ "congo.1",	0x02000, 0xa92dfd24, BRF_PRG | BRF_ESS },
	{ "congo.2",	0x02000, 0x77e922d0, BRF_PRG | BRF_ESS },
	{ "congo.3",	0x02000, 0x824a0746, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_congo, cv_congo, cv_coleco)
STD_ROM_FN(cv_congo)

struct BurnDriver BurnDrvcv_congo = {
	"cv_congo", NULL, "cv_coleco", NULL, "1984",
	"Congo Bongo\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_congoRomInfo, cv_congoRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Defender

static struct BurnRomInfo cv_defenderRomDesc[] = {
	{ "defender.1",	0x02000, 0xbd96e222, BRF_PRG | BRF_ESS },
	{ "defender.2",	0x02000, 0x72541551, BRF_PRG | BRF_ESS },
	{ "defender.3",	0x02000, 0x400beaa2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_defender, cv_defender, cv_coleco)
STD_ROM_FN(cv_defender)

struct BurnDriver BurnDrvcv_defender = {
	"cv_defender", NULL, "cv_coleco", NULL, "1983",
	"Defender\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_defenderRomInfo, cv_defenderRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Donkey Kong (Earlier Version)

static struct BurnRomInfo cv_dkongaRomDesc[] = {
	{ "dkonga.1",	0x02000, 0xdcaf20d8, BRF_PRG | BRF_ESS },
	{ "dkonga.2",	0x02000, 0x6045f75d, BRF_PRG | BRF_ESS },
	{ "dkonga.3",	0x02000, 0x00739499, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkonga, cv_dkonga, cv_coleco)
STD_ROM_FN(cv_dkonga)

struct BurnDriver BurnDrvcv_dkonga = {
	"cv_dkonga", "cv_dkong", "cv_coleco", NULL, "1982",
	"Donkey Kong (Earlier Version)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dkongaRomInfo, cv_dkongaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frenzy

static struct BurnRomInfo cv_frenzyRomDesc[] = {
	{ "frenzy.1",	0x02000, 0x5111bca0, BRF_PRG | BRF_ESS },
	{ "frenzy.2",	0x02000, 0x5453c668, BRF_PRG | BRF_ESS },
	{ "frenzy.3",	0x02000, 0x0c7bedf0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzy, cv_frenzy, cv_coleco)
STD_ROM_FN(cv_frenzy)

struct BurnDriver BurnDrvcv_frenzy = {
	"cv_frenzy", NULL, "cv_coleco", NULL, "1983",
	"Frenzy\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frenzyRomInfo, cv_frenzyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frenzy (Alt 1)

static struct BurnRomInfo cv_frenzyaRomDesc[] = {
	{ "frenzy.1",	0x02000, 0x5111bca0, BRF_PRG | BRF_ESS },
	{ "frenzy.2",	0x02000, 0x5453c668, BRF_PRG | BRF_ESS },
	{ "frenzya.3",	0x02000, 0x2561345b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzya, cv_frenzya, cv_coleco)
STD_ROM_FN(cv_frenzya)

struct BurnDriver BurnDrvcv_frenzya = {
	"cv_frenzya", "cv_frenzy", "cv_coleco", NULL, "1983",
	"Frenzy (Alt 1)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frenzyaRomInfo, cv_frenzyaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frenzy (Alt 2)

static struct BurnRomInfo cv_frenzya2RomDesc[] = {
	{ "frenzy.1",	0x02000, 0x5111bca0, BRF_PRG | BRF_ESS },
	{ "frenzy.2",	0x02000, 0x5453c668, BRF_PRG | BRF_ESS },
	{ "frenzya2.3",	0x02000, 0x9f5f1082, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzya2, cv_frenzya2, cv_coleco)
STD_ROM_FN(cv_frenzya2)

struct BurnDriver BurnDrvcv_frenzya2 = {
	"cv_frenzya2", "cv_frenzy", "cv_coleco", NULL, "1983",
	"Frenzy (Alt 2)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frenzya2RomInfo, cv_frenzya2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Front Line

static struct BurnRomInfo cv_frontlinRomDesc[] = {
	{ "frontlin.1",	0x02000, 0x7b46a5b7, BRF_PRG | BRF_ESS },
	{ "frontlin.2",	0x02000, 0x3ea9a9bd, BRF_PRG | BRF_ESS },
	{ "frontlin.3",	0x02000, 0x91530316, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frontlin, cv_frontlin, cv_coleco)
STD_ROM_FN(cv_frontlin)

struct BurnDriver BurnDrvcv_frontlin = {
	"cv_frontlin", NULL, "cv_coleco", NULL, "1983",
	"Front Line\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frontlinRomInfo, cv_frontlinRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Front Line (Alt)

static struct BurnRomInfo cv_frontlinaRomDesc[] = {
	{ "frontlina.1",	0x02000, 0x715c764b, BRF_PRG | BRF_ESS },
	{ "frontlina.2",	0x02000, 0xeee3a3d3, BRF_PRG | BRF_ESS },
	{ "frontlina.3",	0x02000, 0x77885ebd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frontlina, cv_frontlina, cv_coleco)
STD_ROM_FN(cv_frontlina)

struct BurnDriver BurnDrvcv_frontlina = {
	"cv_frontlina", "cv_frontlin", "cv_coleco", NULL, "1983",
	"Front Line (Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_frontlinaRomInfo, cv_frontlinaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Heist

static struct BurnRomInfo cv_heistRomDesc[] = {
	{ "heist.1",	0x02000, 0x34080665, BRF_PRG | BRF_ESS },
	{ "heist.2",	0x02000, 0xd5c02ce0, BRF_PRG | BRF_ESS },
	{ "heist.3",	0x02000, 0x177a899f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_heist, cv_heist, cv_coleco)
STD_ROM_FN(cv_heist)

struct BurnDriver BurnDrvcv_heist = {
	"cv_heist", NULL, "cv_coleco", NULL, "1983",
	"The Heist\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_heistRomInfo, cv_heistRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Heist (Alt)

static struct BurnRomInfo cv_heistaRomDesc[] = {
	{ "heist.1",	0x02000, 0x34080665, BRF_PRG | BRF_ESS },
	{ "heist.2",	0x02000, 0xd5c02ce0, BRF_PRG | BRF_ESS },
	{ "heista.3",	0x02000, 0xb196db89, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_heista, cv_heista, cv_coleco)
STD_ROM_FN(cv_heista)

struct BurnDriver BurnDrvcv_heista = {
	"cv_heista", "cv_heist", "cv_coleco", NULL, "1983",
	"The Heist (Alt)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_heistaRomInfo, cv_heistaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Jungle Hunt

static struct BurnRomInfo cv_junglehRomDesc[] = {
	{ "jungleh.1",	0x02000, 0x75179eb9, BRF_PRG | BRF_ESS },
	{ "jungleh.2",	0x02000, 0xc6f5bbb2, BRF_PRG | BRF_ESS },
	{ "jungleh.3",	0x02000, 0x07911cc8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jungleh, cv_jungleh, cv_coleco)
STD_ROM_FN(cv_jungleh)

struct BurnDriver BurnDrvcv_jungleh = {
	"cv_jungleh", NULL, "cv_coleco", NULL, "1983",
	"Jungle Hunt\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_junglehRomInfo, cv_junglehRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Miner 2049er

static struct BurnRomInfo cv_mine2049RomDesc[] = {
	{ "mine2049.1",	0x02000, 0xe2814592, BRF_PRG | BRF_ESS },
	{ "mine2049.2",	0x02000, 0x3bc36ef5, BRF_PRG | BRF_ESS },
	{ "mine2049.3",	0x02000, 0x83722d88, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mine2049, cv_mine2049, cv_coleco)
STD_ROM_FN(cv_mine2049)

struct BurnDriver BurnDrvcv_mine2049 = {
	"cv_mine2049", NULL, "cv_coleco", NULL, "1983",
	"Miner 2049er\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mine2049RomInfo, cv_mine2049RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Miner 2049er (Alt)

static struct BurnRomInfo cv_mine2049aRomDesc[] = {
	{ "mine2049a.1",	0x02000, 0x90bd8c1b, BRF_PRG | BRF_ESS },
	{ "mine2049a.2",	0x02000, 0xcb0335af, BRF_PRG | BRF_ESS },
	{ "mine2049a.3",	0x02000, 0x41ed9918, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mine2049a, cv_mine2049a, cv_coleco)
STD_ROM_FN(cv_mine2049a)

struct BurnDriver BurnDrvcv_mine2049a = {
	"cv_mine2049a", "cv_mine2049", "cv_coleco", NULL, "1983",
	"Miner 2049er (Alt)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mine2049aRomInfo, cv_mine2049aRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mr. Do!

static struct BurnRomInfo cv_mrdoRomDesc[] = {
	{ "mrdo.1",	0x02000, 0xa32a3f5f, BRF_PRG | BRF_ESS },
	{ "mrdo.2",	0x02000, 0xd5196bfc, BRF_PRG | BRF_ESS },
	{ "mrdo.3",	0x02000, 0x4be41c67, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdo, cv_mrdo, cv_coleco)
STD_ROM_FN(cv_mrdo)

struct BurnDriver BurnDrvcv_mrdo = {
	"cv_mrdo", NULL, "cv_coleco", NULL, "1983",
	"Mr. Do!\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mrdoRomInfo, cv_mrdoRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Mr. Do! (Alt)

static struct BurnRomInfo cv_mrdoaRomDesc[] = {
	{ "mrdo.1",	0x02000, 0xa32a3f5f, BRF_PRG | BRF_ESS },
	{ "mrdo.2",	0x02000, 0xd5196bfc, BRF_PRG | BRF_ESS },
	{ "mrdoa.3",	0x02000, 0xd8c0e115, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdoa, cv_mrdoa, cv_coleco)
STD_ROM_FN(cv_mrdoa)

struct BurnDriver BurnDrvcv_mrdoa = {
	"cv_mrdoa", "cv_mrdo", "cv_coleco", NULL, "1983",
	"Mr. Do! (Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mrdoaRomInfo, cv_mrdoaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Roc 'n Rope

static struct BurnRomInfo cv_rocnropeRomDesc[] = {
	{ "rocnrope.1",	0x02000, 0x24d5a53c, BRF_PRG | BRF_ESS },
	{ "rocnrope.2",	0x02000, 0x3db8ad55, BRF_PRG | BRF_ESS },
	{ "rocnrope.3",	0x02000, 0xc6146a6d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rocnrope, cv_rocnrope, cv_coleco)
STD_ROM_FN(cv_rocnrope)

struct BurnDriver BurnDrvcv_rocnrope = {
	"cv_rocnrope", NULL, "cv_coleco", NULL, "1984",
	"Roc 'n Rope\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_rocnropeRomInfo, cv_rocnropeRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rocky: Super Action Boxing

static struct BurnRomInfo cv_rockyRomDesc[] = {
	{ "rocky.1",	0x02000, 0xa51498f5, BRF_PRG | BRF_ESS },
	{ "rocky.2",	0x02000, 0x5a8f2336, BRF_PRG | BRF_ESS },
	{ "rocky.3",	0x02000, 0x56fc8d0a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rocky, cv_rocky, cv_coleco)
STD_ROM_FN(cv_rocky)

struct BurnDriver BurnDrvcv_rocky = {
	"cv_rocky", NULL, "cv_coleco", NULL, "1983",
	"Rocky: Super Action Boxing\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_rockyRomInfo, cv_rockyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sector Alpha

static struct BurnRomInfo cv_secalphaRomDesc[] = {
	{ "secalpha.1",	0x02000, 0x9299539b, BRF_PRG | BRF_ESS },
	{ "secalpha.2",	0x02000, 0xc8d6e83d, BRF_PRG | BRF_ESS },
	{ "secalpha.3",	0x01000, 0x354a3b2f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_secalpha, cv_secalpha, cv_coleco)
STD_ROM_FN(cv_secalpha)

struct BurnDriver BurnDrvcv_secalpha = {
	"cv_secalpha", NULL, "cv_coleco", NULL, "1983",
	"Sector Alpha\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_secalphaRomInfo, cv_secalphaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sewer Sam

static struct BurnRomInfo cv_sewersamRomDesc[] = {
	{ "sewersam.1",	0x02000, 0x7906db21, BRF_PRG | BRF_ESS },
	{ "sewersam.2",	0x02000, 0x9ae6324e, BRF_PRG | BRF_ESS },
	{ "sewersam.3",	0x02000, 0xa17fc15a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sewersam, cv_sewersam, cv_coleco)
STD_ROM_FN(cv_sewersam)

struct BurnDriver BurnDrvcv_sewersam = {
	"cv_sewersam", NULL, "cv_coleco", NULL, "1984",
	"Sewer Sam\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sewersamRomInfo, cv_sewersamRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Trek: Strategic Operations Simulator 

static struct BurnRomInfo cv_startrekRomDesc[] = {
	{ "startrek.1",	0x02000, 0x600e431e, BRF_PRG | BRF_ESS },
	{ "startrek.2",	0x02000, 0x1d1741aa, BRF_PRG | BRF_ESS },
	{ "startrek.3",	0x02000, 0x3fa88549, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_startrek, cv_startrek, cv_coleco)
STD_ROM_FN(cv_startrek)

struct BurnDriver BurnDrvcv_startrek = {
	"cv_startrek", NULL, "cv_coleco", NULL, "1984",
	"Star Trek: Strategic Operations Simulator \0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_startrekRomInfo, cv_startrekRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Subroc

static struct BurnRomInfo cv_subrocRomDesc[] = {
	{ "subroc.1",	0x02000, 0x85a94623, BRF_PRG | BRF_ESS },
	{ "subroc.2",	0x02000, 0xb558def8, BRF_PRG | BRF_ESS },
	{ "subroc.3",	0x02000, 0x9dbbb193, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_subroc, cv_subroc, cv_coleco)
STD_ROM_FN(cv_subroc)

struct BurnDriver BurnDrvcv_subroc = {
	"cv_subroc", NULL, "cv_coleco", NULL, "1983",
	"Subroc\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_subrocRomInfo, cv_subrocRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tarzan: From out of the Jungle

static struct BurnRomInfo cv_tarzanRomDesc[] = {
	{ "tarzan.1",	0x02000, 0xb7054e41, BRF_PRG | BRF_ESS },
	{ "tarzan.2",	0x02000, 0xe3d2a4bb, BRF_PRG | BRF_ESS },
	{ "tarzan.3",	0x02000, 0xc0238775, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tarzan, cv_tarzan, cv_coleco)
STD_ROM_FN(cv_tarzan)

struct BurnDriver BurnDrvcv_tarzan = {
	"cv_tarzan", NULL, "cv_coleco", NULL, "1983",
	"Tarzan: From out of the Jungle\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tarzanRomInfo, cv_tarzanRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Victory

static struct BurnRomInfo cv_victoryRomDesc[] = {
	{ "victory.1",	0x02000, 0x811389ba, BRF_PRG | BRF_ESS },
	{ "victory.2",	0x02000, 0x9a0ffe5c, BRF_PRG | BRF_ESS },
	{ "victory.3",	0x02000, 0x0c873fde, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_victory, cv_victory, cv_coleco)
STD_ROM_FN(cv_victory)

struct BurnDriver BurnDrvcv_victory = {
	"cv_victory", NULL, "cv_coleco", NULL, "1983",
	"Victory\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_victoryRomInfo, cv_victoryRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// War Games

static struct BurnRomInfo cv_wargamesRomDesc[] = {
	{ "wargames.1",	0x02000, 0xda4bb2f5, BRF_PRG | BRF_ESS },
	{ "wargames.2",	0x02000, 0x4ef201ef, BRF_PRG | BRF_ESS },
	{ "wargames.3",	0x02000, 0xb9b1cae9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wargames, cv_wargames, cv_coleco)
STD_ROM_FN(cv_wargames)

struct BurnDriver BurnDrvcv_wargames = {
	"cv_wargames", NULL, "cv_coleco", NULL, "1984",
	"War Games\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_wargamesRomInfo, cv_wargamesRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Zaxxon

static struct BurnRomInfo cv_zaxxonRomDesc[] = {
	{ "zaxxon.1",	0x02000, 0x91ff7961, BRF_PRG | BRF_ESS },
	{ "zaxxon.2",	0x02000, 0x36dab466, BRF_PRG | BRF_ESS },
	{ "zaxxon.3",	0x02000, 0x9498a0c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zaxxon, cv_zaxxon, cv_coleco)
STD_ROM_FN(cv_zaxxon)

struct BurnDriver BurnDrvcv_zaxxon = {
	"cv_zaxxon", NULL, "cv_coleco", NULL, "1982",
	"Zaxxon\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_zaxxonRomInfo, cv_zaxxonRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 2010: The Graphic Action Game

static struct BurnRomInfo cv_2010RomDesc[] = {
	{ "2010.1",	0x02000, 0xa8820f34, BRF_PRG | BRF_ESS },
	{ "2010.2",	0x02000, 0x66f96289, BRF_PRG | BRF_ESS },
	{ "2010.3",	0x02000, 0x3c60f243, BRF_PRG | BRF_ESS },
	{ "2010.4",	0x02000, 0xa879523b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_2010, cv_2010, cv_coleco)
STD_ROM_FN(cv_2010)

struct BurnDriver BurnDrvcv_2010 = {
	"cv_2010", NULL, "cv_coleco", NULL, "1984",
	"2010: The Graphic Action Game\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_2010RomInfo, cv_2010RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Dam Busters

static struct BurnRomInfo cv_dambustRomDesc[] = {
	{ "dambust.1",	0x02000, 0x9e82b4ab, BRF_PRG | BRF_ESS },
	{ "dambust.2",	0x02000, 0x56a1b71e, BRF_PRG | BRF_ESS },
	{ "dambust.3",	0x02000, 0x1b5af735, BRF_PRG | BRF_ESS },
	{ "dambust.4",	0x02000, 0x72119879, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dambust, cv_dambust, cv_coleco)
STD_ROM_FN(cv_dambust)

struct BurnDriver BurnDrvcv_dambust = {
	"cv_dambust", NULL, "cv_coleco", NULL, "1984",
	"The Dam Busters\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dambustRomInfo, cv_dambustRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Destructor

static struct BurnRomInfo cv_destructRomDesc[] = {
	{ "destruct.1",	0x02000, 0x7026e237, BRF_PRG | BRF_ESS },
	{ "destruct.2",	0x02000, 0xc1c0b46c, BRF_PRG | BRF_ESS },
	{ "destruct.3",	0x02000, 0xf7737e17, BRF_PRG | BRF_ESS },
	{ "destruct.4",	0x02000, 0x87c11b21, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_destruct, cv_destruct, cv_coleco)
STD_ROM_FN(cv_destruct)

struct BurnDriver BurnDrvcv_destruct = {
	"cv_destruct", NULL, "cv_coleco", NULL, "1984",
	"Destructor\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_destructRomInfo, cv_destructRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Dukes of Hazzard

static struct BurnRomInfo cv_hazzardRomDesc[] = {
	{ "hazzard.1",	0x02000, 0x1971d9a2, BRF_PRG | BRF_ESS },
	{ "hazzard.2",	0x02000, 0x9821ea4a, BRF_PRG | BRF_ESS },
	{ "hazzard.3",	0x02000, 0xc3970e2e, BRF_PRG | BRF_ESS },
	{ "hazzard.4",	0x02000, 0x3433251a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hazzard, cv_hazzard, cv_coleco)
STD_ROM_FN(cv_hazzard)

struct BurnDriver BurnDrvcv_hazzard = {
	"cv_hazzard", NULL, "cv_coleco", NULL, "1984",
	"The Dukes of Hazzard\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_hazzardRomInfo, cv_hazzardRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fortune Builder

static struct BurnRomInfo cv_fortuneRomDesc[] = {
	{ "fortune.1",	0x02000, 0xb55c5448, BRF_PRG | BRF_ESS },
	{ "fortune.2",	0x02000, 0x8d7deaff, BRF_PRG | BRF_ESS },
	{ "fortune.3",	0x02000, 0x039604ee, BRF_PRG | BRF_ESS },
	{ "fortune.4",	0x02000, 0xdfb4469e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fortune, cv_fortune, cv_coleco)
STD_ROM_FN(cv_fortune)

struct BurnDriver BurnDrvcv_fortune = {
	"cv_fortune", NULL, "cv_coleco", NULL, "1984",
	"Fortune Builder\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_fortuneRomInfo, cv_fortuneRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Spy Hunter

static struct BurnRomInfo cv_spyhuntRomDesc[] = {
	{ "spyhunt.1",	0x02000, 0x0a830682, BRF_PRG | BRF_ESS },
	{ "spyhunt.2",	0x02000, 0x46b6d735, BRF_PRG | BRF_ESS },
	{ "spyhunt.3",	0x02000, 0x09474158, BRF_PRG | BRF_ESS },
	{ "spyhunt.4",	0x02000, 0xa5b57758, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spyhunt, cv_spyhunt, cv_coleco)
STD_ROM_FN(cv_spyhunt)

struct BurnDriver BurnDrvcv_spyhunt = {
	"cv_spyhunt", NULL, "cv_coleco", NULL, "1984",
	"Spy Hunter\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_spyhuntRomInfo, cv_spyhuntRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Action Football

static struct BurnRomInfo cv_safootbRomDesc[] = {
	{ "safootb.1",	0x02000, 0xf1fdec05, BRF_PRG | BRF_ESS },
	{ "safootb.2",	0x02000, 0x84104709, BRF_PRG | BRF_ESS },
	{ "safootb.3",	0x02000, 0x496d3acb, BRF_PRG | BRF_ESS },
	{ "safootb.4",	0x02000, 0xdcc8042d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_safootb, cv_safootb, cv_coleco)
STD_ROM_FN(cv_safootb)

struct BurnDriver BurnDrvcv_safootb = {
	"cv_safootb", NULL, "cv_coleco", NULL, "1984",
	"Super Action Football\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_safootbRomInfo, cv_safootbRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Action Football (Super Action Soccer clone)

static struct BurnRomInfo cv_saftsoccRomDesc[] = {
	{ "saftsocc.1",	0x02000, 0xfb9f52cb, BRF_PRG | BRF_ESS },
	{ "sasoccer.2",	0x02000, 0x014ecf5f, BRF_PRG | BRF_ESS },
	{ "sasoccer.3",	0x02000, 0x780e3e49, BRF_PRG | BRF_ESS },
	{ "saftsocc.4",	0x02000, 0xb23e1847, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_saftsocc, cv_saftsocc, cv_coleco)
STD_ROM_FN(cv_saftsocc)

struct BurnDriver BurnDrvcv_saftsocc = {
	"cv_saftsocc", "cv_sasoccer", "cv_coleco", NULL, "1984",
	"Super Action Football (Super Action Soccer clone)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_saftsoccRomInfo, cv_saftsoccRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Action Soccer

static struct BurnRomInfo cv_sasoccerRomDesc[] = {
	{ "sasoccer.1",	0x02000, 0x17447e87, BRF_PRG | BRF_ESS },
	{ "sasoccer.2",	0x02000, 0x014ecf5f, BRF_PRG | BRF_ESS },
	{ "sasoccer.3",	0x02000, 0x780e3e49, BRF_PRG | BRF_ESS },
	{ "sasoccer.4",	0x02000, 0x4f5ce13d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sasoccer, cv_sasoccer, cv_coleco)
STD_ROM_FN(cv_sasoccer)

struct BurnDriver BurnDrvcv_sasoccer = {
	"cv_sasoccer", NULL, "cv_coleco", NULL, "1984",
	"Super Action Soccer\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sasoccerRomInfo, cv_sasoccerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tapper

static struct BurnRomInfo cv_tapperRomDesc[] = {
	{ "tapper.1",	0x02000, 0x7f8b5222, BRF_PRG | BRF_ESS },
	{ "tapper.2",	0x02000, 0xe2ef863f, BRF_PRG | BRF_ESS },
	{ "tapper.3",	0x02000, 0xc14b28fa, BRF_PRG | BRF_ESS },
	{ "tapper.4",	0x02000, 0xd6a41476, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tapper, cv_tapper, cv_coleco)
STD_ROM_FN(cv_tapper)

struct BurnDriver BurnDrvcv_tapper = {
	"cv_tapper", NULL, "cv_coleco", NULL, "1984",
	"Tapper\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tapperRomInfo, cv_tapperRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 2010: The Graphic Action Game (Prototype v54)

static struct BurnRomInfo cv_2010pRomDesc[] = {
	{ "act2010-54-chip1-11-9-cd7e.bin",	0x02000, 0xd8a55441, BRF_PRG | BRF_ESS },
	{ "act2010-54-chip2-11-9-e822.bin",	0x02000, 0xcbba190e, BRF_PRG | BRF_ESS },
	{ "act2010-54-chip3-11-9-06ca.bin",	0x02000, 0x9e13e0a1, BRF_PRG | BRF_ESS },
	{ "act2010-54-chip4-11-9-e758.bin",	0x02000, 0xa3edc192, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_2010p, cv_2010p, cv_coleco)
STD_ROM_FN(cv_2010p)

struct BurnDriver BurnDrvcv_2010p = {
	"cv_2010p", "cv_2010", "cv_coleco", NULL, "1984",
	"2010: The Graphic Action Game (Prototype v54)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_2010pRomInfo, cv_2010pRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// 2010: The Graphic Action Game (Prototype v44)

static struct BurnRomInfo cv_2010p1RomDesc[] = {
	{ "act2010-rv44-chip1.bin",	0x02000, 0xd9dc972f, BRF_PRG | BRF_ESS },
	{ "act2010-rv44-chip2.bin",	0x02000, 0x84288fcb, BRF_PRG | BRF_ESS },
	{ "act2010-rv44-chip3.bin",	0x02000, 0x57807547, BRF_PRG | BRF_ESS },
	{ "act2010-rv44-chip4.bin",	0x02000, 0x44e900fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_2010p1, cv_2010p1, cv_coleco)
STD_ROM_FN(cv_2010p1)

struct BurnDriver BurnDrvcv_2010p1 = {
	"cv_2010p1", "cv_2010", "cv_coleco", NULL, "1984",
	"2010: The Graphic Action Game (Prototype v44)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_2010p1RomInfo, cv_2010p1RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Berenstain Bears (Prototype)

static struct BurnRomInfo cv_bbearsRomDesc[] = {
	{ "bbears.bin",	0x02000, 0x18864abc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bbears, cv_bbears, cv_coleco)
STD_ROM_FN(cv_bbears)

struct BurnDriver BurnDrvcv_bbears = {
	"cv_bbears", NULL, "cv_coleco", NULL, "1984",
	"The Berenstain Bears (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bbearsRomInfo, cv_bbearsRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Burgertime (Prototype)

static struct BurnRomInfo cv_btimemRomDesc[] = {
	{ "btimem.1",	0x02000, 0x4e943aeb, BRF_PRG | BRF_ESS },
	{ "btimem.2",	0x02000, 0xd7e011f2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_btimem, cv_btimem, cv_coleco)
STD_ROM_FN(cv_btimem)

struct BurnDriver BurnDrvcv_btimem = {
	"cv_btimem", "cv_btime", "cv_coleco", NULL, "1983",
	"Burgertime (Prototype)\0", NULL, "Mattel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_btimemRomInfo, cv_btimemRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cabbage Patch Kids: Adventure in the Park (Prototype)

static struct BurnRomInfo cv_cabbagep1RomDesc[] = {
	{ "cabbagep1.1",	0x02000, 0xe7214974, BRF_PRG | BRF_ESS },
	{ "cabbagep1.2",	0x02000, 0xbf35f649, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbagep1, cv_cabbagep1, cv_coleco)
STD_ROM_FN(cv_cabbagep1)

struct BurnDriver BurnDrvcv_cabbagep1 = {
	"cv_cabbagep1", "cv_cabbage", "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cabbagep1RomInfo, cv_cabbagep1RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cabbage Patch Kids: Adventure in the Park (Prototype, Alt)

static struct BurnRomInfo cv_cabbagep2RomDesc[] = {
	{ "cabbage-patch-chip1-6-30.bin",	0x02000, 0x2b793712, BRF_PRG | BRF_ESS },
	{ "cabbage-patch-chip2-6-30.bin",	0x02000, 0xd4ff94a5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbagep2, cv_cabbagep2, cv_coleco)
STD_ROM_FN(cv_cabbagep2)

struct BurnDriver BurnDrvcv_cabbagep2 = {
	"cv_cabbagep2", "cv_cabbage", "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park (Prototype, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cabbagep2RomInfo, cv_cabbagep2RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dig Dug (Prototype)

static struct BurnRomInfo cv_digdugRomDesc[] = {
	{ "digdug.1",	0x02000, 0xe13a6484, BRF_PRG | BRF_ESS },
	{ "digdug.2",	0x02000, 0x82bfa6a0, BRF_PRG | BRF_ESS },
	{ "digdug.3",	0x02000, 0x57f347e0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_digdug, cv_digdug, cv_coleco)
STD_ROM_FN(cv_digdug)

struct BurnDriver BurnDrvcv_digdug = {
	"cv_digdug", NULL, "cv_coleco", NULL, "1984",
	"Dig Dug (Prototype)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_digdugRomInfo, cv_digdugRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dragon's Lair (Prototype, 0416)

static struct BurnRomInfo cv_dlairRomDesc[] = {
	{ "dragon80-chip1-4-16-ec4a.bin",	0x02000, 0x950950d0, BRF_PRG | BRF_ESS },
	{ "dragon80-chip2-4-16-ec4a.bin",	0x02000, 0x8a778f5a, BRF_PRG | BRF_ESS },
	{ "dragon80-chip3-4-16-ec4a.bin",	0x02000, 0x8eec1020, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dlair, cv_dlair, cv_coleco)
STD_ROM_FN(cv_dlair)

struct BurnDriver BurnDrvcv_dlair = {
	"cv_dlair", NULL, "cv_coleco", NULL, "1984",
	"Dragon's Lair (Prototype, 0416)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dlairRomInfo, cv_dlairRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Escape from the Mindmaster (Prototype)

static struct BurnRomInfo cv_mindmstrRomDesc[] = {
	{ "mindmstr .1",	0x02000, 0xb8280950, BRF_PRG | BRF_ESS },
	{ "mindmstr.2",	0x02000, 0x6cade290, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mindmstr, cv_mindmstr, cv_coleco)
STD_ROM_FN(cv_mindmstr)

struct BurnDriver BurnDrvcv_mindmstr = {
	"cv_mindmstr", NULL, "cv_coleco", NULL, "1983",
	"Escape from the Mindmaster (Prototype)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mindmstrRomInfo, cv_mindmstrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fall Guy (Prototype)

static struct BurnRomInfo cv_fallguyRomDesc[] = {
	{ "fallguy.1",	0x02000, 0xf77d3ffd, BRF_PRG | BRF_ESS },
	{ "fallguy.2",	0x02000, 0xc85c0f58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fallguy, cv_fallguy, cv_coleco)
STD_ROM_FN(cv_fallguy)

struct BurnDriver BurnDrvcv_fallguy = {
	"cv_fallguy", NULL, "cv_coleco", NULL, "1983",
	"Fall Guy (Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_fallguyRomInfo, cv_fallguyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Joust (Prototype)

static struct BurnRomInfo cv_joustRomDesc[] = {
	{ "joust.1",	0x02000, 0x99402515, BRF_PRG | BRF_ESS },
	{ "joust.2",	0x02000, 0x11ef44b8, BRF_PRG | BRF_ESS },
	{ "joust.3",	0x02000, 0x0754fb6b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_joust, cv_joust, cv_coleco)
STD_ROM_FN(cv_joust)

struct BurnDriver BurnDrvcv_joust = {
	"cv_joust", NULL, "cv_coleco", NULL, "1983",
	"Joust (Prototype)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_joustRomInfo, cv_joustRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// M*A*S*H (Prototype)

static struct BurnRomInfo cv_mashRomDesc[] = {
	{ "mash.1",	0x02000, 0x1bdfdaa5, BRF_PRG | BRF_ESS },
	{ "mash.2",	0x02000, 0x55af024c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mash, cv_mash, cv_coleco)
STD_ROM_FN(cv_mash)

struct BurnDriver BurnDrvcv_mash = {
	"cv_mash", NULL, "cv_coleco", NULL, "1983",
	"M*A*S*H (Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_mashRomInfo, cv_mashRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Orbit (Prototype)

static struct BurnRomInfo cv_orbitRomDesc[] = {
	{ "orbit.bin",	0x02000, 0x2cc6f4aa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_orbit, cv_orbit, cv_coleco)
STD_ROM_FN(cv_orbit)

struct BurnDriver BurnDrvcv_orbit = {
	"cv_orbit", NULL, "cv_coleco", NULL, "1983",
	"Orbit (Prototype)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_orbitRomInfo, cv_orbitRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pac-Man (Prototype)

static struct BurnRomInfo cv_pacmanRomDesc[] = {
	{ "pacman.1",	0x02000, 0xc0b0689d, BRF_PRG | BRF_ESS },
	{ "pacman.2",	0x02000, 0xcb94e964, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pacman, cv_pacman, cv_coleco)
STD_ROM_FN(cv_pacman)

struct BurnDriver BurnDrvcv_pacman = {
	"cv_pacman", NULL, "cv_coleco", NULL, "1983",
	"Pac-Man (Prototype)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_pacmanRomInfo, cv_pacmanRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Porky's (Prototype)

static struct BurnRomInfo cv_porkysRomDesc[] = {
	{ "porkys.1",	0x02000, 0xf5aa5a81, BRF_PRG | BRF_ESS },
	{ "porkys.2",	0x02000, 0x6b1f73a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_porkys, cv_porkys, cv_coleco)
STD_ROM_FN(cv_porkys)

struct BurnDriver BurnDrvcv_porkys = {
	"cv_porkys", NULL, "cv_coleco", NULL, "1983",
	"Porky's (Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_porkysRomInfo, cv_porkysRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Power Lords: Quest for Volcan (Prototype)

static struct BurnRomInfo cv_powerRomDesc[] = {
	{ "power.1",	0x02000, 0x1137001f, BRF_PRG | BRF_ESS },
	{ "power.2",	0x02000, 0x4c872266, BRF_PRG | BRF_ESS },
	{ "power.3",	0x02000, 0x5f20b4e1, BRF_PRG | BRF_ESS },
	{ "power.4",	0x02000, 0x560207f3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_power, cv_power, cv_coleco)
STD_ROM_FN(cv_power)

struct BurnDriver BurnDrvcv_power = {
	"cv_power", NULL, "cv_coleco", NULL, "1984",
	"Power Lords: Quest for Volcan (Prototype)\0", NULL, "Probe 2000", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_powerRomInfo, cv_powerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Play and Learn (Prototype)

static struct BurnRomInfo cv_smurfplyRomDesc[] = {
	{ "smurfply.1",	0x02000, 0x1397a474, BRF_PRG | BRF_ESS },
	{ "smurfply.2",	0x02000, 0xa51e1410, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfply, cv_smurfply, cv_coleco)
STD_ROM_FN(cv_smurfply)

struct BurnDriver BurnDrvcv_smurfply = {
	"cv_smurfply", NULL, "cv_coleco", NULL, "1982",
	"Smurf Play and Learn (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfplyRomInfo, cv_smurfplyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Spy Hunter (Prototype, v22)

static struct BurnRomInfo cv_spyhuntpRomDesc[] = {
	{ "spyr22-aaad-11-12-chip1.bin",	0x02000, 0xf8fd0e1c, BRF_PRG | BRF_ESS },
	{ "spyrv22-7f14-11-12-chip2.bin",	0x02000, 0xff749b25, BRF_PRG | BRF_ESS },
	{ "spyrv22-f73b-11-12-chip3.bin",	0x02000, 0xa45ff169, BRF_PRG | BRF_ESS },
	{ "spyrv22-b3b4-11-12-chip4.bin",	0x02000, 0xda895d9c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spyhuntp, cv_spyhuntp, cv_coleco)
STD_ROM_FN(cv_spyhuntp)

struct BurnDriver BurnDrvcv_spyhuntp = {
	"cv_spyhuntp", "cv_spyhunt", "cv_coleco", NULL, "1984",
	"Spy Hunter (Prototype, v22)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_spyhuntpRomInfo, cv_spyhuntpRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Spy Hunter (Prototype, v13)

static struct BurnRomInfo cv_spyhuntp1RomDesc[] = {
	{ "spy13-chip1.bin",	0x02000, 0x01d34c94, BRF_PRG | BRF_ESS },
	{ "spy13-chip2.bin",	0x02000, 0x1aabc54a, BRF_PRG | BRF_ESS },
	{ "spy13-chip3.bin",	0x02000, 0xbd730c9f, BRF_PRG | BRF_ESS },
	{ "spy13-chip4.bin",	0x02000, 0xc85c6303, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spyhuntp1, cv_spyhuntp1, cv_coleco)
STD_ROM_FN(cv_spyhuntp1)

struct BurnDriver BurnDrvcv_spyhuntp1 = {
	"cv_spyhuntp1", "cv_spyhunt", "cv_coleco", NULL, "1984",
	"Spy Hunter (Prototype, v13)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_spyhuntp1RomInfo, cv_spyhuntp1RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Steamroller (Prototype)

static struct BurnRomInfo cv_steamRomDesc[] = {
	{ "steam.1",	0x02000, 0xdd4f0a05, BRF_PRG | BRF_ESS },
	{ "steam.2",	0x02000, 0x0bf43529, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_steam, cv_steam, cv_coleco)
STD_ROM_FN(cv_steam)

struct BurnDriver BurnDrvcv_steam = {
	"cv_steam", NULL, "cv_coleco", NULL, "1984",
	"Steamroller (Prototype)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_steamRomInfo, cv_steamRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super DK! (Prototype)

static struct BurnRomInfo cv_superdkRomDesc[] = {
	{ "superdk.1",	0x02000, 0x36253de5, BRF_PRG | BRF_ESS },
	{ "superdk.2",	0x02000, 0x88819fa6, BRF_PRG | BRF_ESS },
	{ "superdk.3",	0x02000, 0x7e3889f6, BRF_PRG | BRF_ESS },
	{ "superdk.4",	0x02000, 0xcd99ed49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_superdk, cv_superdk, cv_coleco)
STD_ROM_FN(cv_superdk)

struct BurnDriver BurnDrvcv_superdk = {
	"cv_superdk", NULL, "cv_coleco", NULL, "1982",
	"Super DK! (Prototype)\0", NULL, "Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_superdkRomInfo, cv_superdkRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sword and Sorcerer (Prototype)

static struct BurnRomInfo cv_swordRomDesc[] = {
	{ "sword-chip1-a176.bin",	0x02000, 0xa1548994, BRF_PRG | BRF_ESS },
	{ "sword-chip2-d5ba.bin",	0x02000, 0x1d03dfae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sword, cv_sword, cv_coleco)
STD_ROM_FN(cv_sword)

struct BurnDriver BurnDrvcv_sword = {
	"cv_sword", NULL, "cv_coleco", NULL, "1983",
	"Sword and Sorcerer (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_swordRomInfo, cv_swordRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Video Hustler (Prototype)

static struct BurnRomInfo cv_hustlerRomDesc[] = {
	{ "video.bin",	0x02000, 0xa9177b20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hustler, cv_hustler, cv_coleco)
STD_ROM_FN(cv_hustler)

struct BurnDriver BurnDrvcv_hustler = {
	"cv_hustler", NULL, "cv_coleco", NULL, "1984",
	"Video Hustler (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_hustlerRomInfo, cv_hustlerRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Video Hustler (Prototype, 19840727)

static struct BurnRomInfo cv_hustler1RomDesc[] = {
	{ "hustler-7-27-84-8c61.bin",	0x02000, 0x841d168f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hustler1, cv_hustler1, cv_coleco)
STD_ROM_FN(cv_hustler1)

struct BurnDriver BurnDrvcv_hustler1 = {
	"cv_hustler1", "cv_hustler", "cv_coleco", NULL, "1984",
	"Video Hustler (Prototype, 19840727)\0", NULL, "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_hustler1RomInfo, cv_hustler1RomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// War Games (Prototype, 0417)

static struct BurnRomInfo cv_wargamespRomDesc[] = {
	{ "wargams17-chip1-4-17-c44a.bin",	0x02000, 0xda4bb2f5, BRF_PRG | BRF_ESS },
	{ "wargams17-chip2-4-17-c44a.bin",	0x02000, 0x4ef201ef, BRF_PRG | BRF_ESS },
	{ "wargams17-chip3-4-17-c44a.bin",	0x02000, 0x2a95379b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wargamesp, cv_wargamesp, cv_coleco)
STD_ROM_FN(cv_wargamesp)

struct BurnDriver BurnDrvcv_wargamesp = {
	"cv_wargamesp", "cv_wargames", "cv_coleco", NULL, "1984",
	"War Games (Prototype, 0417)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_wargamespRomInfo, cv_wargamespRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// The Yolks on You (Prototype)

static struct BurnRomInfo cv_yolkRomDesc[] = {
	{ "yolk.1",	0x02000, 0xf7220275, BRF_PRG | BRF_ESS },
	{ "yolk.2",	0x02000, 0x06bce2fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yolk, cv_yolk, cv_coleco)
STD_ROM_FN(cv_yolk)

struct BurnDriver BurnDrvcv_yolk = {
	"cv_yolk", NULL, "cv_coleco", NULL, "1983",
	"The Yolks on You (Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_yolkRomInfo, cv_yolkRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// CBS Colecovision Monitor Test

static struct BurnRomInfo cv_cbsmonRomDesc[] = {
	{ "cbsmon.1",	0x02000, 0x1cc13594, BRF_PRG | BRF_ESS },
	{ "cbsmon.2",	0x02000, 0x3aa93ef3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cbsmon, cv_cbsmon, cv_coleco)
STD_ROM_FN(cv_cbsmon)

struct BurnDriver BurnDrvcv_cbsmon = {
	"cv_cbsmon", NULL, "cv_coleco", NULL, "1982",
	"CBS Colecovision Monitor Test\0", NULL, "CBS", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cbsmonRomInfo, cv_cbsmonRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Final Test Cartridge (Prototype)

static struct BurnRomInfo cv_finaltstRomDesc[] = {
	{ "colecovision final test (1982) (coleco).rom",	0x02000, 0x3ae4b58c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_finaltst, cv_finaltst, cv_coleco)
STD_ROM_FN(cv_finaltst)

struct BurnDriver BurnDrvcv_finaltst = {
	"cv_finaltst", NULL, "cv_coleco", NULL, "1982",
	"Final Test Cartridge (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_finaltstRomInfo, cv_finaltstRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Controller Test Cartridge

static struct BurnRomInfo cv_suprtestRomDesc[] = {
	{ "super action controller tester (1982) (nuvatec).rom",	0x02000, 0xd206fe58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprtest, cv_suprtest, cv_coleco)
STD_ROM_FN(cv_suprtest)

struct BurnDriver BurnDrvcv_suprtest = {
	"cv_suprtest", NULL, "cv_coleco", NULL, "1983",
	"Super Controller Test Cartridge\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_suprtestRomInfo, cv_suprtestRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Music Box (Demo)

static struct BurnRomInfo cv_musicboxRomDesc[] = {
	{ "musicbox.1",	0x02000, 0x4557ed22, BRF_PRG | BRF_ESS },
	{ "musicbox.2",	0x02000, 0x5f40d58b, BRF_PRG | BRF_ESS },
	{ "musicbox.3",	0x02000, 0x0ab26aaa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_musicbox, cv_musicbox, cv_coleco)
STD_ROM_FN(cv_musicbox)

struct BurnDriver BurnDrvcv_musicbox = {
	"cv_musicbox", NULL, "cv_coleco", NULL, "1987",
	"Music Box (Demo)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_musicboxRomInfo, cv_musicboxRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dance Fantasy

static struct BurnRomInfo cv_dncfntsyRomDesc[] = {
	{ "dncfntsy.1",	0x02000, 0xbff86a98, BRF_PRG | BRF_ESS },
	{ "dncfntsy.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dncfntsy, cv_dncfntsy, cv_coleco)
STD_ROM_FN(cv_dncfntsy)

struct BurnDriver BurnDrvcv_dncfntsy = {
	"cv_dncfntsy", NULL, "cv_coleco", NULL, "1984",
	"Dance Fantasy\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_dncfntsyRomInfo, cv_dncfntsyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Facemaker

static struct BurnRomInfo cv_facemakrRomDesc[] = {
	{ "facemakr.1",	0x02000, 0xec9dfb07, BRF_PRG | BRF_ESS },
	{ "facemakr.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_facemakr, cv_facemakr, cv_coleco)
STD_ROM_FN(cv_facemakr)

struct BurnDriver BurnDrvcv_facemakr = {
	"cv_facemakr", NULL, "cv_coleco", NULL, "1983",
	"Facemaker\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_facemakrRomInfo, cv_facemakrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fraction Fever

static struct BurnRomInfo cv_fracfevrRomDesc[] = {
	{ "fracfevr.1",	0x02000, 0x964db3bc, BRF_PRG | BRF_ESS },
	{ "fracfevr.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fracfevr, cv_fracfevr, cv_coleco)
STD_ROM_FN(cv_fracfevr)

struct BurnDriver BurnDrvcv_fracfevr = {
	"cv_fracfevr", NULL, "cv_coleco", NULL, "1983",
	"Fraction Fever\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_fracfevrRomInfo, cv_fracfevrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Juke Box

static struct BurnRomInfo cv_jukeboxRomDesc[] = {
	{ "jukebox.1",	0x02000, 0xa5511418, BRF_PRG | BRF_ESS },
	{ "jukebox.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jukebox, cv_jukebox, cv_coleco)
STD_ROM_FN(cv_jukebox)

struct BurnDriver BurnDrvcv_jukebox = {
	"cv_jukebox", NULL, "cv_coleco", NULL, "1984",
	"Juke Box\0", NULL, "Spinnaker Software Corp", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_jukeboxRomInfo, cv_jukeboxRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Memory Manor

static struct BurnRomInfo cv_memmanorRomDesc[] = {
	{ "memmanor.1",	0x02000, 0xbab520ea, BRF_PRG | BRF_ESS },
	{ "memmanor.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_memmanor, cv_memmanor, cv_coleco)
STD_ROM_FN(cv_memmanor)

struct BurnDriver BurnDrvcv_memmanor = {
	"cv_memmanor", NULL, "cv_coleco", NULL, "1984",
	"Memory Manor\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_memmanorRomInfo, cv_memmanorRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Skiing

static struct BurnRomInfo cv_skiingRomDesc[] = {
	{ "skiing.1",	0x02000, 0x4e1fb1c7, BRF_PRG | BRF_ESS },
	{ "skiing.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_skiing, cv_skiing, cv_coleco)
STD_ROM_FN(cv_skiing)

struct BurnDriver BurnDrvcv_skiing = {
	"cv_skiing", NULL, "cv_coleco", NULL, "1986",
	"Skiing\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_skiingRomInfo, cv_skiingRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Alcazar: The Forgotten Fortress

static struct BurnRomInfo cv_alcazarRomDesc[] = {
	{ "alcazar.1",	0x02000, 0xe49113c8, BRF_PRG | BRF_ESS },
	{ "alcazar.2",	0x02000, 0x4d89160d, BRF_PRG | BRF_ESS },
	{ "alcazar.3",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
	{ "alcazar.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_alcazar, cv_alcazar, cv_coleco)
STD_ROM_FN(cv_alcazar)

struct BurnDriver BurnDrvcv_alcazar = {
	"cv_alcazar", NULL, "cv_coleco", NULL, "1985",
	"Alcazar: The Forgotten Fortress\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_alcazarRomInfo, cv_alcazarRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cabbage Patch Kids Picture Show

static struct BurnRomInfo cv_cabbshowRomDesc[] = {
	{ "cabbshow.1",	0x02000, 0x5d3c41a7, BRF_PRG | BRF_ESS },
	{ "cabbshow.2",	0x02000, 0xfbd2e110, BRF_PRG | BRF_ESS },
	{ "cabbshow.3",	0x02000, 0x17295ffa, BRF_PRG | BRF_ESS },
	{ "cabbshow.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbshow, cv_cabbshow, cv_coleco)
STD_ROM_FN(cv_cabbshow)

struct BurnDriver BurnDrvcv_cabbshow = {
	"cv_cabbshow", NULL, "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids Picture Show\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cabbshowRomInfo, cv_cabbshowRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Galaxian

static struct BurnRomInfo cv_galaxianRomDesc[] = {
	{ "galaxian.1",	0x02000, 0x7d84e0e3, BRF_PRG | BRF_ESS },
	{ "galaxian.2",	0x02000, 0x9390df8b, BRF_PRG | BRF_ESS },
	{ "galaxian.3",	0x02000, 0xdd5465ce, BRF_PRG | BRF_ESS },
	{ "galaxian.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_galaxian, cv_galaxian, cv_coleco)
STD_ROM_FN(cv_galaxian)

struct BurnDriver BurnDrvcv_galaxian = {
	"cv_galaxian", NULL, "cv_coleco", NULL, "1983",
	"Galaxian\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_galaxianRomInfo, cv_galaxianRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Galaxian (Alt)

static struct BurnRomInfo cv_galaxianaRomDesc[] = {
	{ "galaxian.1",	0x02000, 0x7d84e0e3, BRF_PRG | BRF_ESS },
	{ "galaxiana.2",	0x02000, 0x4e701bc1, BRF_PRG | BRF_ESS },
	{ "galaxiana.3",	0x02000, 0x96acb931, BRF_PRG | BRF_ESS },
	{ "galaxian.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_galaxiana, cv_galaxiana, cv_coleco)
STD_ROM_FN(cv_galaxiana)

struct BurnDriver BurnDrvcv_galaxiana = {
	"cv_galaxiana", "cv_galaxian", "cv_coleco", NULL, "1983",
	"Galaxian (Alt)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_galaxianaRomInfo, cv_galaxianaRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Monkey Academy

static struct BurnRomInfo cv_monkeyRomDesc[] = {
	{ "monkey.1",	0x02000, 0xfcd8e18b, BRF_PRG | BRF_ESS },
	{ "monkey.2",	0x02000, 0xc1effcb3, BRF_PRG | BRF_ESS },
	{ "monkey.3",	0x02000, 0x5594b4c4, BRF_PRG | BRF_ESS },
	{ "monkey.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_monkey, cv_monkey, cv_coleco)
STD_ROM_FN(cv_monkey)

struct BurnDriver BurnDrvcv_monkey = {
	"cv_monkey", NULL, "cv_coleco", NULL, "1984",
	"Monkey Academy\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_monkeyRomInfo, cv_monkeyRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Paint 'n' Play Workshop

static struct BurnRomInfo cv_smurfpntRomDesc[] = {
	{ "smurfpnt.1",	0x02000, 0xb5cd2fb4, BRF_PRG | BRF_ESS },
	{ "smurfpnt.2",	0x02000, 0xe4197181, BRF_PRG | BRF_ESS },
	{ "smurfpnt.3",	0x02000, 0xa0e1ab06, BRF_PRG | BRF_ESS },
	{ "smurfpnt.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfpnt, cv_smurfpnt, cv_coleco)
STD_ROM_FN(cv_smurfpnt)

struct BurnDriver BurnDrvcv_smurfpnt = {
	"cv_smurfpnt", NULL, "cv_coleco", NULL, "1983",
	"Smurf Paint 'n' Play Workshop\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfpntRomInfo, cv_smurfpntRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Action Baseball

static struct BurnRomInfo cv_sabasebRomDesc[] = {
	{ "sabaseb.1",	0x02000, 0x2061515d, BRF_PRG | BRF_ESS },
	{ "sabaseb.2",	0x02000, 0xf29c94e3, BRF_PRG | BRF_ESS },
	{ "sabaseb.3",	0x02000, 0xa4713651, BRF_PRG | BRF_ESS },
	{ "sabaseb.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sabaseb, cv_sabaseb, cv_coleco)
STD_ROM_FN(cv_sabaseb)

struct BurnDriver BurnDrvcv_sabaseb = {
	"cv_sabaseb", NULL, "cv_coleco", NULL, "1983",
	"Super Action Baseball\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sabasebRomInfo, cv_sabasebRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super DK! Junior (Prototype)

static struct BurnRomInfo cv_suprdkjrRomDesc[] = {
	{ "suprdkjr.1",	0x02000, 0x85ecf7e3, BRF_PRG | BRF_ESS },
	{ "suprdkjr.2",	0x02000, 0x5f63bc3c, BRF_PRG | BRF_ESS },
	{ "suprdkjr.3",	0x02000, 0x6d5676fa, BRF_PRG | BRF_ESS },
	{ "suprdkjr.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprdkjr, cv_suprdkjr, cv_coleco)
STD_ROM_FN(cv_suprdkjr)

struct BurnDriver BurnDrvcv_suprdkjr = {
	"cv_suprdkjr", NULL, "cv_coleco", NULL, "1983",
	"Super DK! Junior (Prototype)\0", NULL, "Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_suprdkjrRomInfo, cv_suprdkjrRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tunnels and Trolls (Demo)

static struct BurnRomInfo cv_tunnelsRomDesc[] = {
	{ "tunnels.1",	0x02000, 0x36a12145, BRF_PRG | BRF_ESS },
	{ "tunnels.2",	0x02000, 0xb0abdb32, BRF_PRG | BRF_ESS },
	{ "tunnels.3",	0x02000, 0x53cc4957, BRF_PRG | BRF_ESS },
	{ "tunnels.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tunnels, cv_tunnels, cv_coleco)
STD_ROM_FN(cv_tunnels)

struct BurnDriver BurnDrvcv_tunnels = {
	"cv_tunnels", NULL, "cv_coleco", NULL, "1983",
	"Tunnels and Trolls (Demo)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tunnelsRomInfo, cv_tunnelsRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// War Room

static struct BurnRomInfo cv_warroomRomDesc[] = {
	{ "warroom.1",	0x02000, 0xfc6def44, BRF_PRG | BRF_ESS },
	{ "warroom.2",	0x02000, 0xa166efc7, BRF_PRG | BRF_ESS },
	{ "warroom.3",	0x02000, 0x97fa3660, BRF_PRG | BRF_ESS },
	{ "warroom.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_warroom, cv_warroom, cv_coleco)
STD_ROM_FN(cv_warroom)

struct BurnDriver BurnDrvcv_warroom = {
	"cv_warroom", NULL, "cv_coleco", NULL, "1983",
	"War Room\0", NULL, "Probe 2000", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_warroomRomInfo, cv_warroomRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Word Feud

static struct BurnRomInfo cv_wordfeudRomDesc[] = {
	{ "wordfeud.1",	0x02000, 0xf7a29c01, BRF_PRG | BRF_ESS },
	{ "wordfeud.2",	0x02000, 0xf7a29c01, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wordfeud, cv_wordfeud, cv_coleco)
STD_ROM_FN(cv_wordfeud)

struct BurnDriver BurnDrvcv_wordfeud = {
	"cv_wordfeud", NULL, "cv_coleco", NULL, "1984",
	"Word Feud\0", NULL, "K-Tel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_wordfeudRomInfo, cv_wordfeudRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tournament Tennis

static struct BurnRomInfo cv_ttennisRomDesc[] = {
	{ "ttennis.1",	0x02000, 0xdbcde1a9, BRF_PRG | BRF_ESS },
	{ "ttennis.2",	0x02000, 0xb72f9ff8, BRF_PRG | BRF_ESS },
	{ "ttennis.3",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
	{ "ttennis.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ttennis, cv_ttennis, cv_coleco)
STD_ROM_FN(cv_ttennis)

struct BurnDriver BurnDrvcv_ttennis = {
	"cv_ttennis", NULL, "cv_coleco", NULL, "1984",
	"Tournament Tennis\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ttennisRomInfo, cv_ttennisRomName, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


