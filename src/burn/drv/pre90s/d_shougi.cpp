// FB Neo Alpha Shougi driver module
// Based on MAME driver by Jarek Burczynski, Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "alpha8201.h"
#include "ay8910.h"
#include "watchdog.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvMCUROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvShareRAM;

static INT32 nmi_enable;
static INT32 semaphore;

static INT32 nCyclesExtra[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ShougiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shougi)

static struct BurnDIPInfo ShougiDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    8, "Time"					},
	{0x00, 0x01, 0x32, 0x00, "1 Minute"				},
	{0x00, 0x01, 0x32, 0x20, "2 Minutes"			},
	{0x00, 0x01, 0x32, 0x02, "3 Minutes"			},
	{0x00, 0x01, 0x32, 0x22, "4 Minutes"			},
	{0x00, 0x01, 0x32, 0x10, "5 Minutes"			},
	{0x00, 0x01, 0x32, 0x30, "10 Minutes"			},
	{0x00, 0x01, 0x32, 0x12, "20 Minutes"			},
	{0x00, 0x01, 0x32, 0x32, "30 Minutes"			},
};

STDDIPINFO(Shougi)

static struct BurnDIPInfo Shougi2DIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xce, NULL					},

	{0   , 0xfe, 0   ,    8, "Time"					},
	{0x00, 0x01, 0x32, 0x00, "1 Minute"				},
	{0x00, 0x01, 0x32, 0x20, "2 Minutes"			},
	{0x00, 0x01, 0x32, 0x02, "3 Minutes"			},
	{0x00, 0x01, 0x32, 0x22, "4 Minutes"			},
	{0x00, 0x01, 0x32, 0x10, "5 Minutes"			},
	{0x00, 0x01, 0x32, 0x30, "10 Minutes"			},
	{0x00, 0x01, 0x32, 0x12, "20 Minutes"			},
	{0x00, 0x01, 0x32, 0x32, "30 Minutes"			},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x00, 0x01, 0x04, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x04, 0x04, "1 Coin  1 Credits"	},
};

STDDIPINFO(Shougi2)

static void __fastcall shougi_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x7000) {
		Alpha8201WriteRam(address & 0x3ff, data & 0xff);
		return;
	}

	switch (address)
	{
		case 0x4800:
		case 0x4808:
		case 0x4802:
		case 0x480a:
		case 0x4807:
		case 0x480f: 
		case 0x4805:
		case 0x480d:
		case 0x4806:
		case 0x480e: return;

		case 0x4801:
		case 0x4809:
			nmi_enable = (address >> 3) & 1;
			if (nmi_enable == 0)
			{
				ZetSetIRQLine(0, 0x20, CPU_IRQSTATUS_NONE);
				ZetSetIRQLine(1, 0x20, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x4803:
		case 0x480b:
			Alpha8201Start((address & 8) >> 3);
		return;

		case 0x4804:
		case 0x480c: // mcu bus
			Alpha8201SetBusDir((~address & 8) >> 3);
		break;

		case 0x5800:
			BurnWatchdogWrite();
		return;

		case 0x6000:
			AY8910Write(0, 0, data);
		return;

		case 0x6800:
			AY8910Write(0, 1, data);
		return;
	}
}

static UINT8 __fastcall shougi_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x7000) { // 7000-73ff
		return Alpha8201ReadRam(address & 0x3ff);
	}

	switch (address)
	{
		case 0x4800:
			return (DrvDips[0] & 0xfe) | (DrvInputs[2] & 0x01);

		case 0x5000:
			return DrvInputs[0]; 

		case 0x5800:
			return DrvInputs[1];
	}

	return 0;
}

static UINT8 __fastcall shougi_sub_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			semaphore ^= 1;
			return semaphore;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	ZetReset(1);
	AY8910Reset(0);
	Alpha8201Reset();

	BurnWatchdogReset();

	nmi_enable = 0;
	semaphore = 0;

	memset (nCyclesExtra, 0, sizeof(nCyclesExtra));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x004000;
	DrvZ80ROM[1]	= Next; Next += 0x006000;

	DrvMCUROM		= Next; Next += 0x002000;

	DrvColPROM		= Next; Next += 0x000020;

	BurnPalette		= (UINT32*)Next; Next += 0x020 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x008000;
	DrvShareRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x3000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x3000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x4000, k++, 1)) return 1;
		if (game == 1) if (BurnLoadRom(DrvZ80ROM[1] + 0x5000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM    + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000, k++, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x4000, 0x43ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0x7800, 0x7bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(shougi_main_write);
	ZetSetReadHandler(shougi_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0x6000, 0x63ff, MAP_RAM);
	ZetSetInHandler(shougi_sub_read_port);
	ZetClose();

	Alpha8201Init(DrvMCUROM);

	BurnWatchdogInit(DrvDoReset, 16);

	AY8910Init(0, 1250000, 0);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	Alpha8201Exit();

	BurnWatchdogExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = ((bit0 * 1000 + bit1 * 470 + bit2 * 220) * 0xff) / 1690;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = ((bit0 * 1000 + bit1 * 470 + bit2 * 220) * 0xff) / 1690;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = ((bit0 * 470 + bit1 * 220) * 0xff) / 690;

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 0x4000; offs++)
	{
		UINT8 sx = (offs >> 8) << 2;
		UINT8 sy = ~offs;
		UINT8 d0 = DrvVidRAM[0x0000 + offs];
		UINT8 d1 = DrvVidRAM[0x0000 + offs] >> 4;
		UINT8 d2 = DrvVidRAM[0x4000 + offs];
		UINT8 d3 = DrvVidRAM[0x4000 + offs] >> 4;

		for (INT32 x = 0; x < 4; x++)
		{
			INT32 pxl = 0;

			pxl |= ((d2 >> x) & 1) << 0;
			pxl |= ((d3 >> x) & 1) << 1;
			pxl |= ((d0 >> x) & 1) << 2;
			pxl |= ((d1 >> x) & 1) << 3;

			pTransDraw[sy * nScreenWidth + ((sx + x) ^ 0xff)] = pxl;
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	draw_layer();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	Alpha8201NewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 2500000 / 60, 2500000 / 60, 312500 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], 0, nCyclesExtra[2] };
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();

		CPU_RUN(2, Alpha8201);

		if (i == 255 && nmi_enable != 0) {
			ZetSetIRQLine(0, 0x20, CPU_IRQSTATUS_ACK);
			ZetSetIRQLine(1, 0x20, CPU_IRQSTATUS_ACK);
		}
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(semaphore);
		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Shougi

static struct BurnRomInfo shougiRomDesc[] = {
	{ "1.3a",							0x1000, 0xb601303f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "3.3c",							0x1000, 0x2b8c7314, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.3b",							0x1000, 0x09cb831f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.3d",							0x1000, 0xad1a642a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5.3e",							0x1000, 0xff1f07d0, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "8.3j",							0x1000, 0x6230c4c1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "6.3f",							0x1000, 0xd5a91b16, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "9.3k",							0x1000, 0xdbbfa66e, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "7.3h",							0x1000, 0x7ea8ec4a, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "alpha-8201_44801a75_2f25.bin",	0x2000, 0xb77931ac, 3 | BRF_GRA },           //  9 MCU Code

	{ "pr.2l",							0x0020, 0xcd3559ff, 4 | BRF_GRA },           // 10 Color PROM
};

STD_ROM_PICK(shougi)
STD_ROM_FN(shougi)

static INT32 ShougiInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvShougi = {
	"shougi", NULL, NULL, NULL, "1982",
	"Shougi\0", NULL, "Alpha Denshi Co. (Tehkan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, shougiRomInfo, shougiRomName, NULL, NULL, NULL, NULL, ShougiInputInfo, ShougiDIPInfo,
	ShougiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


// Shougi Part II

static struct BurnRomInfo shougi2RomDesc[] = {
	{ "1-2.3a",							0x1000, 0x16d75306, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "3-2.3c",							0x1000, 0x35b6d98b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2-2.3b",							0x1000, 0xb38affed, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4-2.3d",							0x1000, 0x1abdb6bf, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5-2.3e",							0x1000, 0x0ba89dd4, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "8-2.3j",							0x1000, 0x0ae0c8c1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "6-2.3f",							0x1000, 0xd98abcae, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "9-2.3k",							0x1000, 0x4e0e6c90, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "7-2.3h",							0x1000, 0x5f37ebc6, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "10-2.3l",						0x1000, 0xa26385fd, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "alpha-8201_44801a75_2f25.bin",	0x2000, 0xb77931ac, 3 | BRF_GRA },           // 10 MCU Code

	{ "pr.2l",							0x0020, 0xcd3559ff, 4 | BRF_GRA },           // 11 Color PROM
};

STD_ROM_PICK(shougi2)
STD_ROM_FN(shougi2)

static INT32 Shougi2Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvShougi2 = {
	"shougi2", NULL, NULL, NULL, "1982",
	"Shougi Part II\0", NULL, "Alpha Denshi Co. (Tehkan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, shougi2RomInfo, shougi2RomName, NULL, NULL, NULL, NULL, ShougiInputInfo, Shougi2DIPInfo,
	Shougi2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x20,
	256, 256, 4, 3
};


