// FB Alpha Xyonix driver module
// Based on MAME driver by David Haywood and Stephh

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 m_prev_coin;
static UINT8 m_credits;
static UINT8 m_coins;
static UINT8 e0_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo XyonixInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Xyonix)

static struct BurnDIPInfo XyonixDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfb, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x03, 0x03, "Easy"				},
	{0x11, 0x01, 0x03, 0x02, "Normal"			},
	{0x11, 0x01, 0x03, 0x01, "Hard"				},
	{0x11, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x04, 0x04, "No"				},
	{0x11, 0x01, 0x04, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x08, 0x08, "Off"				},
	{0x11, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},
};

STDDIPINFO(Xyonix)

static void handle_coins(INT32 coin)
{
	static const INT32 coinage_table[4][2] = {{2,3},{2,1},{1,2},{1,1}};
	INT32 tmp = 0;

	if (coin & 1)   // Coin 2 !
	{
		tmp = (DrvDips[0] & 0xc0) >> 6;
		m_coins++;
		if (m_coins >= coinage_table[tmp][0])
		{
			m_credits += coinage_table[tmp][1];
			m_coins -= coinage_table[tmp][0];
		}
	}

	if (coin & 2)   // Coin 1 !
	{
		tmp = (DrvDips[0] & 0x30) >> 4;
		m_coins++;
		if (m_coins >= coinage_table[tmp][0])
		{
			m_credits += coinage_table[tmp][1];
			m_coins -= coinage_table[tmp][0];
		}
	}

	if (m_credits >= 9)
		m_credits = 9;
}

static UINT8 io_read()
{
	INT32 regPC = ZetGetPC(-1);

	if (regPC == 0x27ba)
		return 0x88;

	if (regPC == 0x27c2)
		return e0_data;

	if (regPC == 0x27c7)
	{
		INT32 coin;

		switch (e0_data)
		{
			case 0x81 :
				return DrvInputs[0] & 0x7f;
			case 0x82 :
				return DrvInputs[1] & 0x7f;
			case 0x91:// check coin inputs
				coin = (DrvInputs[0] >> 7) | ((DrvInputs[1] & 0x80) >> 6);
				if (coin ^ m_prev_coin && coin != 3)
				{
					if (m_credits < 9) handle_coins(coin);
				}
				m_prev_coin = coin;
				return m_credits;
			case 0x92:
				return (DrvInputs[0] >> 7) | ((DrvInputs[1] & 0x80) >> 6);
			case 0xe0: // reset?
				m_coins = 0;
				m_credits = 0;
				return 0xff;
			case 0xe1:
				m_credits--;
				return 0xff;
			case 0xfe: // Dip Switches 1 to 4
				return DrvDips[0] & 0x0f;
			case 0xff: // Dip Switches 5 to 8
				return DrvDips[0] >> 4;
		}
	}

	return 0xff;
}

static void __fastcall xyonix_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x20:
		case 0x21:
			SN76496Write(port & 1, data);
		return;

		case 0x40:
		case 0x60:
		case 0x61: // nop
		return;

		case 0x50:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xe0:
			e0_data = data;
		return;
	}
}

static UINT8 __fastcall xyonix_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0xe0:
			return io_read();
	}

	return 0;
}

static tilemap_callback( xyonix )
{
	INT32 attr = DrvVidRAM[offs + 0x1001];

	TILE_SET_INFO(0, DrvVidRAM[offs+1] + (attr << 8), attr >> 4, 0);
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	m_prev_coin = 0;
	m_credits = 0;
	m_coins = 0;
	e0_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM	= Next; Next += 0x020000;

	DrvColPROM	= Next; Next += 0x000100;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM	= Next; Next += 0x002000;
	DrvVidRAM	= Next; Next += 0x002000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 Plane[4] = { 0, 4, 0x8000*8, 0x8000*8+4 };
	static INT32 XOffs[4] = { STEP4(3,-1) };
	static INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM, 0x10000);

	GfxDecode(0x1000, 4, 4, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree (tmp);
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM + 0x00000,  0, 1)) return 1;

	//	if (BurnLoadRom(DrvMCUROM + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  4, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(xyonix_write_port);
	ZetSetInHandler(xyonix_read_port);
	ZetClose();

	SN76496Init(0, 4000000, 0);
	SN76496Init(1, 4000000, 1);
	SN76496SetRoute(0, 0.70, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.70, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, xyonix_map_callback, 4, 8, 80, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 4, 8, 0x20000, 0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	INT32 bit0, bit1, bit2;

	for (INT32 i = 0; i < 0x100; i++)
	{
		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		UINT8 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 5) & 0x01;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		UINT8 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		UINT8 b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		if ((i & 0x3f) == 0x3d) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK); // 4x per frame
		if (i == (nInterleave - 1)) ZetNmi();
	}

	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(m_prev_coin);
		SCAN_VAR(m_credits);
		SCAN_VAR(m_coins);
		SCAN_VAR(e0_data);
	}

	return 0;
}


// Xyonix

static struct BurnRomInfo xyonixRomDesc[] = {
	{ "xyonix3.bin",	0x10000, 0x1960a74e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "mc68705p3s.e7",	0x00780, 0xf60cdd86, 2 | BRF_OPT },        //  1 Undumped MCU

	{ "xyonix1.bin",	0x08000, 0x3dfa9596, 3 | BRF_GRA },           //  2 Tiles
	{ "xyonix2.bin",	0x08000, 0xdb87343e, 3 | BRF_GRA },           //  3

	{ "xyonix.pr",		0x00100, 0x0012cfc9, 4 | BRF_GRA },           //  4 Color data
};

STD_ROM_PICK(xyonix)
STD_ROM_FN(xyonix)

struct BurnDriver BurnDrvXyonix = {
	"xyonix", NULL, NULL, NULL, "1989",
	"Xyonix\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, xyonixRomInfo, xyonixRomName, NULL, NULL, NULL, NULL, XyonixInputInfo, XyonixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};
