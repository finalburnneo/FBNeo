// FB Neo Equites driver module
// Based on MAME driver by Acho A. Tang, Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "samples.h"
#include "ad59mc07.h"
#include "alpha8201.h"
#include "ay8910.h"
#include "burn_pal.h"
#include "watchdog.h"
#include "74259.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSndRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 scrolly;
static UINT8 scrollx;
static UINT8 unknown_bit;
static UINT16 bgcolor;
static INT32 flipscreen;
static UINT8 soundlatch;

static INT32 nCyclesExtra[3];

static INT32 has_nvram = 0;
static INT32 sound_type = 0; // 0 = i8085 / 1 = z80

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo EquitesInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Equites)

static struct BurnInputInfo BngotimeInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 coin"		},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 coin"		},
	{"Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"		},
	{"Tilt Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"Tilt Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"		},
	{"Tilt Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"		},
	{"Spring Launcher",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},
	{"Buy Extra Ball",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"		},
	{"Bet",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"		},
	{"Take",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 4"		},
	{"Payout",			BIT_DIGITAL,	DrvJoy2 + 13,	"p1 fire 5"		},
	{"Keyout",			BIT_DIGITAL,	DrvJoy2 + 14,	"p1 fire 6"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Bngotime)

#include "dip_percent_equites.inc"

static struct BurnDIPInfo EquitesDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x0c, NULL						},
	{0x01, 0xff, 0xff,   34, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Buttons"					},
	{0x00, 0x01, 0x08, 0x00, "2"						},
	{0x00, 0x01, 0x08, 0x08, "3"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x00, 0x01, 0x10, 0x00, "3"						},
	{0x00, 0x01, 0x10, 0x10, "5"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x20, 0x00, "Normal"					},
	{0x00, 0x01, 0x20, 0x20, "Hard"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0xc0, 0xc0, "A 2C/1C B 3C/1C"			},
	{0x00, 0x01, 0xc0, 0x00, "A 1C/1C B 2C/1C"			},
	{0x00, 0x01, 0xc0, 0x80, "A 1C/2C B 1C/4C"			},
	{0x00, 0x01, 0xc0, 0x40, "A 1C/3C B 1C/6C"			},

	DIP_PERCENT(0x01)
};

STDDIPINFO(Equites)

static struct BurnDIPInfo GekisouDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff,   25, NULL						},

	DIP_PERCENT(0x01)
};

STDDIPINFO(Gekisou)

static struct BurnDIPInfo BullfgtrDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x40, NULL						},
	{0x01, 0xff, 0xff,   25, NULL						},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x0c, 0x0c, "3:00"						},
	{0x00, 0x01, 0x0c, 0x08, "2:00"						},
	{0x00, 0x01, 0x0c, 0x00, "1:30"						},
	{0x00, 0x01, 0x0c, 0x04, "1:00"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x20, 0x00, "Normal"					},
	{0x00, 0x01, 0x20, 0x20, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x90, 0x90, "1C/1C (3C per player)"	},
	{0x00, 0x01, 0x90, 0x00, "1C/1C (1C per player)"	},
	{0x00, 0x01, 0x90, 0x80, "A 1C/1C B 1C/4C"			},
	{0x00, 0x01, 0x90, 0x10, "A 1C/2C B 1C/3C"			},

	DIP_PERCENT(0x01)
};

STDDIPINFO(Bullfgtr)

static struct BurnDIPInfo KouyakyuDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x40, NULL						},
	{0x01, 0xff, 0xff,   25, NULL						},

	{0   , 0xfe, 0   ,    4, "Game Points"				},
	{0x00, 0x01, 0x0c, 0x08, "3000"						},
	{0x00, 0x01, 0x0c, 0x04, "4000"						},
	{0x00, 0x01, 0x0c, 0x00, "5000"						},
	{0x00, 0x01, 0x0c, 0x0c, "7000"						},

	{0   , 0xfe, 0   ,    2, "Unused"					},
	{0x00, 0x01, 0x20, 0x00, "Off"						},
	{0x00, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x90, 0x90, "1C/1C (2C per player)"	},
	{0x00, 0x01, 0x90, 0x00, "1C/1C (1C per player)"	},
	{0x00, 0x01, 0x90, 0x80, "1C/1C (1C for 2 players)"	},
	{0x00, 0x01, 0x90, 0x10, "1C/3C (1C per player)"	},

	DIP_PERCENT(0x01)
};

STDDIPINFO(Kouyakyu)

static struct BurnDIPInfo BngotimeDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff,   25, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x00, 0x01, 0x80, 0x00, "Off"						},
	{0x00, 0x01, 0x80, 0x80, "On"						},

	DIP_PERCENT(0x01)
};

STDDIPINFO(Bngotime)

static void mcu_sync()
{
	INT32 cyc = (double)SekTotalCycles(0) * (4000000 / 8 / 4) / 3000000;
	cyc -= Alpha8201TotalCycles();

	if (cyc > 0) {
		Alpha8201Run(cyc);
	}
}

static void latch_map(INT32 address, UINT8 data)
{
	switch (address) {
		case 1: flipscreen = data; break;
		case 2: Alpha8201Start(data); break;
		case 3: Alpha8201SetBusDir(data ^ 1); break;
	}
}

static void __fastcall equites_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x100000) {
		if (data == 0x5555 && (address & 0xffe) == 0) data = 0;
		*((UINT16*)(DrvSprRAM + (address & 0xffe))) = data;
		return;
	}

	if ((address & 0xfff000) == 0x080000) {
		DrvFgRAM[(address / 2) & 0x7ff] = data & 0xff;
		return;
	}

	if ((address & 0xfff000) == 0x140000) {
		mcu_sync();
		Alpha8201WriteRam((address & 0x7ff) >> 1, data & 0xff);
		return;
	}

	if (address >= 0x180000 && address <= 0x1bffff) {
		mcu_sync();
		ls259_write_a3((address & 0x3c000) >> 14, data);

		if (address != 0x180000) return; // let soundlatch through on high byte
	}

	switch (address)
	{
		case 0x180000:
			if (sound_type == 0) AD59MC07Command(data);
			soundlatch = data;
		return;

		case 0x1c0000:
			scrolly = data & 0xff;
			scrollx = data >> 8;
		return;

		case 0x380000:
			bgcolor = data;
		return;

		case 0x580000:
		case 0x5a0000:
			unknown_bit = (address >> 17) & 1;
		return;

		case 0x780000:
			BurnWatchdogWrite();
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static void __fastcall equites_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x100000) {
		DrvSprRAM[(address & 0xfff)^1] = data;
		return;
	}

	if ((address & 0xfff000) == 0x080000) {
		DrvFgRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xfff000) == 0x140000) {
		mcu_sync();
		Alpha8201WriteRam((address & 0x7ff) >> 1, data);
		return;
	}

	if (address >= 0x180000 && address <= 0x1bffff && (~address & 1)) {
		mcu_sync();
		ls259_write_a3((address & 0x3c000) >> 14, data);
		return;
	}

	switch (address)
	{
		case 0x180001:
			if (sound_type == 0) AD59MC07Command(data);
			soundlatch = data;
		return;

		case 0x1c0000:
			scrollx = data;
		return;

		case 0x1c0001:
			scrolly = data;
		return;

		case 0x380000:
		case 0x380001:
			bgcolor = data;
		return;

		case 0x580000:
		case 0x580001:
		case 0x5a0000:
		case 0x5a0001:
			unknown_bit = (address >> 17) & 1;
		return;

		case 0x780000:
		case 0x780001:
			BurnWatchdogWrite();
		return;
	}
	bprintf (0, _T("MWB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static UINT16 __fastcall equites_main_read_word(UINT32 address)
{
	SEK_DEF_READ_WORD(0, address);

	return 0xffff;
}

static UINT8 __fastcall equites_main_read_byte(UINT32 address)
{
	if ((address & 0xfff801) == 0x140001) {
		mcu_sync();
		return Alpha8201ReadRam((address & 0x7ff) >> 1);
	}

	if ((address & 0xfff001) == 0x080001) {
		return DrvFgRAM[(address / 2) & 0x7ff];
	}

	switch (address)
	{
		case 0x180000:
			return (DrvInputs[1] >> 8) | (unknown_bit << 7) | DrvDips[0];

		case 0x180001:
			return DrvInputs[1] >> 0;

		case 0x1c0000:
			return (DrvInputs[0] >> 8);

		case 0x1c0001:
			return DrvInputs[0] >> 0;
	}

	bprintf (0, _T("MRB: %5.5x PC(%5.5x)\n"), address, SekGetPC(-1));
	return 0xff;
}

static void __fastcall ad_60mc01_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc102:
			soundlatch = 0;
		return;

		case 0xc104: // dac
		case 0xc106: // audio board io >= 6
		case 0xc107:
		case 0xc108:
		case 0xc109:
		case 0xc10a:
		case 0xc10b:
		case 0xc10c:
		case 0xc10d:
		case 0xc10e:
		return;
	}
}

static UINT8 __fastcall ad_60mc01_read(UINT16 address)
{
	switch (address)
	{
		case 0xc100:
			return soundlatch;
	}

	return 0;
}

static void __fastcall ad_60mc01_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
		case 0x11:
		return; // unused ym2203

		case 0x80:
		case 0x81:
			AY8910Write(0, ~port & 1, data);
		return;
	}
}

static tilemap_callback( fg )
{
	UINT8 attr = DrvFgRAM[offs * 2 + 1];

	TILE_SET_INFO(0, DrvFgRAM[offs * 2], attr, (attr & 0x10) ? TILE_OPAQUE : 0); // tileinfo.flags |= TILE_FORCE_LAYER0;
}

static tilemap_callback( bg )
{
	UINT16 attr = *((UINT16*)(DrvBgRAM + (offs * 2)));

	TILE_SET_INFO(1, attr, attr >> 12, TILE_FLIPXY(attr >> 9));
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	Alpha8201Reset();
	ls259_reset();

	switch (sound_type)
	{
		case 0:
		{
			AD59MC07Reset();
		}
		break;

		case 1:
		{
			ZetOpen(0);
			ZetReset();
			ZetClose();
		}
		break;
	}

	AY8910Reset(0);

	BurnWatchdogReset();

	memset (nCyclesExtra, 0, sizeof(nCyclesExtra));

	scrolly = 0;
	scrollx = 0;
	unknown_bit = 0;
	bgcolor = 0;
	flipscreen = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x010000;
	DrvSndROM			= Next; Next += 0x010000;
	DrvMCUROM			= Next; Next += 0x002000;

	DrvGfxROM[0]		= Next; Next += 0x004000;
	DrvGfxROM[1]		= Next; Next += 0x020000;
	DrvGfxROM[2]		= Next; Next += 0x020000;

	DrvColPROM			= Next; Next += 0x000400;

	DrvPalette			= (UINT32*)Next; Next += 0x0180 * sizeof(UINT32);

	if (has_nvram) {
		Drv68KRAM		= Next; Next += 0x001000;
	}
	AllRam				= Next;

	if (!has_nvram){
		Drv68KRAM		= Next; Next += 0x001000;
	}
	DrvFgRAM			= Next; Next += 0x000800;
	DrvBgRAM			= Next; Next += 0x001000;
	DrvSprRAM			= Next; Next += 0x001000;
	DrvSndRAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4(8*8+3,-1), STEP4(0*8+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0*8,8) };
	INT32 Plane1[3]  = { 4, ((0x10000/2)*8), ((0x10000/2)*8)+4 };
	INT32 XOffs1[16] = { STEP4(16*8+3,-1), STEP4(32*8+3,-1), STEP4(48*8+3,-1), STEP4(0*8+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };
	INT32 Plane2[3]  = { 4, ((0x10000/2)*8), ((0x10000/2)*8)+4 };
	INT32 XOffs2[16] = { STEP4(128*0+3,-1), STEP4(128*1+3,-1), STEP4(128*2+3,-1), STEP4(128*3+3,-1) };
	// sprites are actually 14 pixels high, decode them as 16 - 0*8, 15*8 skipped in MAME
	// memset routine at bottom removes garbage found in some games
	INT32 YOffs2[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,	8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x10000);

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x10000);

	GfxDecode(0x0200, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM[2]);

	for (INT32 i = 0; i < 0x20000; i+=0x100) {
		memset (DrvGfxROM[2] + i + 0x00, 0, 0x10);
		memset (DrvGfxROM[2] + i + 0xf0, 0, 0x10);
	}

	BurnFree(tmp);

	return 0;
}

static void unpack_block(UINT8 *rom, INT32 offset, INT32 size)
{
	for (INT32 i = 0; i < size; i++)
	{
		rom[(offset + i + size)] = (rom[(offset + i)] >> 4);
		rom[(offset + i)] &= 0x0f;
	}
}

static void unpack_region(INT32 select)
{
	unpack_block(DrvGfxROM[select], 0x0000, 0x2000);
	unpack_block(DrvGfxROM[select], 0x4000, 0x2000);
}

static INT32 DrvLoadROMs()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[8] = { 0, Drv68KROM, DrvSndROM, DrvMCUROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM };
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 nType = ri.nType & 7;

		if (nType == 1)
		{
			if (BurnLoadRom(pLoad[nType] + 0, i+0, 2)) return 1;
			if (BurnLoadRom(pLoad[nType] + 1, i+1, 2)) return 1;
			pLoad[nType] += ri.nLen * 2;
			i++;
			continue;
		}

		if (nType >= 2)
		{
			if (BurnLoadRom(tmp, i, 1)) return 1;

			if (nType == 5 || nType == 6)
			{
				memcpy (pLoad[nType], tmp, 0x2000);
				if (ri.nLen == 0x4000)	// bngotime & gekisou
				{
					memcpy (pLoad[nType] + 0x4000, tmp + 0x2000, 0x2000); // swap halves
					if ((pLoad[nType] - DrvGfxROM[nType - 4]) == 0)
					{
						pLoad[nType] += 0x4000;
					}
					else
					{
						pLoad[nType] -= 0x2000; // next rom loads at += 0x2000, not 0x4000
					}
				}
				else if (((pLoad[nType] - DrvGfxROM[nType - 4]) == 0) || (pLoad[nType] - DrvGfxROM[nType - 4]) == 0x4000)
				{
					pLoad[nType] += 0x2000; // gap
					if (nType == 6 && sound_type == 1) {
						pLoad[nType] += 0x4000; // more gap for bngotime sprites
					}
				}
			}
			else
			{
				memcpy (pLoad[nType], tmp, ri.nLen);
			}
			pLoad[nType] += ri.nLen;
			continue;
		}
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs()) return 1;
		unpack_region(1);
		unpack_region(2);
		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x040000, 0x040fff, MAP_RAM);
	//SekMapMemory(DrvFgRAM,	0x080000, 0x080fff, MAP_RAM); // 8 bit (in handler)
	SekMapMemory(DrvBgRAM,		0x0c0000, 0x0c0fff, MAP_RAM); // 0-1ff bg, 200-fff ?
	SekMapMemory(DrvSprRAM,		0x100000, 0x100fff, MAP_ROM); // 0-1ff, 200 due to sek page size
	SekSetWriteWordHandler(0,	equites_main_write_word);
	SekSetWriteByteHandler(0,	equites_main_write_byte);
	SekSetReadWordHandler(0,	equites_main_read_word);
	SekSetReadByteHandler(0,	equites_main_read_byte);
	SekClose();

	switch (sound_type)
	{
		case 0:
		{
			AD59MC07Init(DrvSndROM);
		}
		break;

		case 1:
		{
			ZetInit(0);
			ZetOpen(0);
			ZetMapMemory(DrvSndROM, 0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvSndRAM,	0x8000, 0x87ff, MAP_RAM);
			ZetSetWriteHandler(ad_60mc01_write);
			ZetSetReadHandler(ad_60mc01_read);
			ZetSetOutHandler(ad_60mc01_write_port);
			ZetClose();

			AY8910Init(0, 2000000, 0);
			AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
			AY8910SetBuffered(ZetTotalCycles, 4000000);

		}
	}

	Alpha8201Init(DrvMCUROM);

	ls259_init();
	ls259_set_write_cb(latch_map);

	BurnWatchdogInit(DrvDoReset, 180); // 180?

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x004000, 0x000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 16, 16, 0x020000, 0x080, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, 0x020000, 0x100, 0x0f);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -24);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	Alpha8201Exit();
	ls259_exit();
	BurnWatchdogExit();

	switch (sound_type)
	{
		case 0:
		{
			AD59MC07Exit();
		}
		break;

		case 1:
		{
			ZetExit();
			AY8910Exit(0);
		}
	}

	BurnFreeMemIndex();

	sound_type = 0;
	has_nvram = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[i] = BurnHighCol(pal4bit(DrvColPROM[i]), pal4bit(DrvColPROM[i + 0x100]), pal4bit(DrvColPROM[i + 0x200]), 0);
	}

	for (INT32 i = 0; i < 0x80; i++)
	{
		DrvPalette[i + 0x100] = DrvPalette[DrvColPROM[0x380 + i]];
	}
}

static void draw_sprites_block(INT32 start, INT32 end)
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	GenericTilesGfx *pGfx = &GenericGfxData[2];

	for (INT32 offs = end - 2; offs >= start; offs -= 2)
	{
		INT32 attr = ram[offs + 1] ^ 0xfe00;
		if (attr & 0x800)
		{
			INT32 code 	= attr & 0x1ff;
			INT32 flipx = attr & 0x400;
			INT32 flipy = attr & 0x200;
			INT32 color = attr >> 12;
			INT32 sx 	= ram[offs] >> 8;
			INT32 sy 	= ram[offs] & 0xff;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			sx -= 4;			// align

			RenderTileTranstabOffset(pTransDraw, pGfx->gfxbase, code % pGfx->code_mask, color << pGfx->depth, 0, sx, sy - 24, flipx, flipy, pGfx->width, pGfx->height, DrvColPROM + 0x380, pGfx->color_offset);
		}
	}
}

static void draw_sprites()
{
	draw_sprites_block(0x000/2, 0x060/2);
	draw_sprites_block(0x0e0/2, 0x100/2);
	draw_sprites_block(0x1a4/2, 0x200/2);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(bgcolor);

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly + (flipscreen ? -10 : 0));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	if (sound_type == 0) AD59MC07NewFrame();
	SekNewFrame();
	Alpha8201NewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		//bprintf(0, _T("dip1: %x %d\n"), DrvDips[1], DrvDips[1]);
		AD59MC07SetRate(DrvDips[1]);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 3000000 / 60, ((sound_type) ? 4000000 : (6144000/2)) / 60, (4000000 / 8 / 4) / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	Alpha8201Idle(nCyclesExtra[2]);

	SekOpen(0);
	switch (sound_type)
	{
		case 0: break;
		case 1: ZetOpen(0); break;
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		switch (sound_type)
		{
			case 0: CPU_RUN(1, AD59MC07); break;
			case 1: CPU_RUN(1, Zet); if (i == 0 || i == 128) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); break;
		}

		CPU_RUN_SYNCINT(2, Alpha8201);

		if (i == 23) SekSetIRQLine(1, CPU_IRQSTATUS_HOLD);
		if (i == 231) SekSetIRQLine(2, CPU_IRQSTATUS_HOLD);
	}

	switch (sound_type)
	{
		case 0: break;
		case 1: ZetClose(); break;
	}

	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = Alpha8201TotalCycles() - nCyclesTotal[2];

	if (pBurnSoundOut) {
		switch (sound_type) {
			case 0: AD59MC07Update(pBurnSoundOut, nBurnSoundLen); break;
			case 1: AY8910Render(pBurnSoundOut, nBurnSoundLen); break;
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE)
	{
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");

		SekScan(nAction);
		switch (sound_type)
		{
			case 0:
				AD59MC07Scan(nAction, pnMin);
			break;
			case 1:
				ZetScan(nAction);
				AY8910Scan(nAction, pnMin);
				SCAN_VAR(soundlatch);
			break;
		}

		Alpha8201Scan(nAction, pnMin);
		ls259_scan();
		BurnWatchdogScan(nAction);

		SCAN_VAR(scrolly);
		SCAN_VAR(scrollx);
		SCAN_VAR(unknown_bit);
		SCAN_VAR(bgcolor);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nCyclesExtra);
	}

	if ((nAction & ACB_NVRAM) && has_nvram) {
		ScanVar(Drv68KRAM, 0x1000, "NV RAM");
	}

	return 0;
}


// Equites (World)

static struct BurnRomInfo equitesRomDesc[] = {
	{ "ep1",						0x2000, 0x6a4fe5f7, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "ep5",						0x2000, 0x00faa3eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-ep2.12d",				0x2000, 0x0c1bc2e7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-ep6.12b",				0x2000, 0xbbed3dcc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-ep3.10d",				0x2000, 0x5f2d059a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-ep7.10b",				0x2000, 0xa8f6b8aa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ep4",						0x2000, 0xb636e086, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ep8",						0x2000, 0xd7ee48b0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ev1.1m",						0x2000, 0x43faaf2e, 2 | BRF_PRG | BRF_ESS }, //  8 I8085 Code
	{ "ev2.1l",						0x2000, 0x09e6702d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ev3.1k",						0x2000, 0x10ff140b, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "ev4.1h",						0x2000, 0xb7917264, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, // 12 Alpha 8xxx MCU Code

	{ "ep9",						0x1000, 0x0325be11, 4 | BRF_GRA },           // 13 Characters

	{ "eb5.7h",						0x2000, 0xcbef7da5, 5 | BRF_GRA },           // 14 Background Tiles
	{ "eb6.8h",						0x2000, 0x1e5e5475, 5 | BRF_GRA },           // 15
	{ "eb1.7f",						0x2000, 0x9a236583, 5 | BRF_GRA },           // 16
	{ "eb2.8f",						0x2000, 0xf0fb6355, 5 | BRF_GRA },           // 17
	{ "eb3.10f",					0x2000, 0xdbd0044b, 5 | BRF_GRA },           // 18
	{ "eb4.11f",					0x2000, 0xf8f8e600, 5 | BRF_GRA },           // 19

	{ "es5.5h",						0x2000, 0xd5b82e6a, 6 | BRF_GRA },           // 20 Sprites
	{ "es6.4h",						0x2000, 0xcb4f5da9, 6 | BRF_GRA },           // 21
	{ "es1.5f",						0x2000, 0xcf81a2cd, 6 | BRF_GRA },           // 22
	{ "es2.4f",						0x2000, 0xae3111d8, 6 | BRF_GRA },           // 23
	{ "es3.2f",						0x2000, 0x3d44f815, 6 | BRF_GRA },           // 24
	{ "es4.1f",						0x2000, 0x16e6d18a, 6 | BRF_GRA },           // 25

	{ "bprom.3a",					0x0100, 0x2fcdf217, 7 | BRF_GRA },           // 26 Color PROMs
	{ "bprom.1a",					0x0100, 0xd7e6cd1f, 7 | BRF_GRA },           // 27
	{ "bprom.2a",					0x0100, 0xe3d106e8, 7 | BRF_GRA },           // 28
	{ "bprom.6b",					0x0100, 0x6294cddf, 7 | BRF_GRA },           // 29

	{ "bprom.7b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 30 Unused PROMs
	{ "bprom.9b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 31
	{ "bprom.10b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 32
	{ "bprom.3h",					0x0020, 0x33b98466, 0 | BRF_OPT },           // 33
};

STD_ROM_PICK(equites)
STD_ROM_FN(equites)

struct BurnDriver BurnDrvEquites = {
	"equites", NULL, NULL, "ad59mc07", "1984",
	"Equites (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, equitesRomInfo, equitesRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, EquitesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};


// Equites (World, Alt)

static struct BurnRomInfo equitessRomDesc[] = {
	{ "epr-ep1.13d",				0x2000, 0xc6edf1cd, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "epr-ep5.13b",				0x2000, 0xc11f0759, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-ep2.12d",				0x2000, 0x0c1bc2e7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-ep6.12b",				0x2000, 0xbbed3dcc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-ep3.10d",				0x2000, 0x5f2d059a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-ep7.10b",				0x2000, 0xa8f6b8aa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-ep4.9d",					0x2000, 0x956a06bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "epr-ep8.9b",					0x2000, 0x4c78d60d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ev1.1m",						0x2000, 0x43faaf2e, 2 | BRF_PRG | BRF_ESS }, //  8 I8085 Code
	{ "ev2.1l",						0x2000, 0x09e6702d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ev3.1k",						0x2000, 0x10ff140b, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "ev4.1h",						0x2000, 0xb7917264, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, // 12 Alpha 8xxx MCU Code

	{ "epr-ep0.3e",					0x1000, 0x3f5a81c3, 4 | BRF_GRA },           // 13 Characters

	{ "eb5.7h",						0x2000, 0xcbef7da5, 5 | BRF_GRA },           // 14 Background Tiles
	{ "eb6.8h",						0x2000, 0x1e5e5475, 5 | BRF_GRA },           // 15
	{ "eb1.7f",						0x2000, 0x9a236583, 5 | BRF_GRA },           // 16
	{ "eb2.8f",						0x2000, 0xf0fb6355, 5 | BRF_GRA },           // 17
	{ "eb3.10f",					0x2000, 0xdbd0044b, 5 | BRF_GRA },           // 18
	{ "eb4.11f",					0x2000, 0xf8f8e600, 5 | BRF_GRA },           // 19

	{ "es5.5h",						0x2000, 0xd5b82e6a, 6 | BRF_GRA },           // 20 Sprites
	{ "es6.4h",						0x2000, 0xcb4f5da9, 6 | BRF_GRA },           // 21
	{ "es1.5f",						0x2000, 0xcf81a2cd, 6 | BRF_GRA },           // 22
	{ "es2.4f",						0x2000, 0xae3111d8, 6 | BRF_GRA },           // 23
	{ "es3.2f",						0x2000, 0x3d44f815, 6 | BRF_GRA },           // 24
	{ "es4.1f",						0x2000, 0x16e6d18a, 6 | BRF_GRA },           // 25

	{ "bprom.3a",					0x0100, 0x2fcdf217, 7 | BRF_GRA },           // 26 Color PROMs
	{ "bprom.1a",					0x0100, 0xd7e6cd1f, 7 | BRF_GRA },           // 27
	{ "bprom.2a",					0x0100, 0xe3d106e8, 7 | BRF_GRA },           // 28
	{ "bprom.6b",					0x0100, 0x6294cddf, 7 | BRF_GRA },           // 29

	{ "bprom.7b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 30 Unused PROMs
	{ "bprom.9b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 31
	{ "bprom.10b",					0x0100, 0x6294cddf, 0 | BRF_OPT },           // 32
	{ "bprom.3h",					0x0020, 0x33b98466, 0 | BRF_OPT },           // 33
};

STD_ROM_PICK(equitess)
STD_ROM_FN(equitess)

struct BurnDriver BurnDrvEquitess = {
	"equitess", "equites", NULL, "ad59mc07", "1984",
	"Equites (World, Alt)\0", NULL, "Alpha Denshi Co. (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, equitessRomInfo, equitessRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, EquitesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};


// Bull Fighter (World)

static struct BurnRomInfo bullfgtrRomDesc[] = {
	{ "hp1vr.bin",					0x2000, 0xe5887586, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "hp5vr.bin",					0x2000, 0xb49fa09f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hp2vr.bin",					0x2000, 0x845bdf28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hp6vr.bin",					0x2000, 0x3dfadcf4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hp3vr.bin",					0x2000, 0xd3a21f8a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hp7vr.bin",					0x2000, 0x665cc015, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "hv1vr.bin",					0x2000, 0x2a8e6fcf, 2 | BRF_PRG | BRF_ESS }, //  6 I8085 Code
	{ "hv2vr.bin",					0x2000, 0x026e1533, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "hv3vr.bin",					0x2000, 0x51ee751c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "hv4vr.bin",					0x2000, 0x62c7a25b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, // 10 Alpha 8xxx MCU Code

	{ "h.bin",						0x1000, 0xc6894c9a, 4 | BRF_GRA },           // 11 Characters

	{ "hb5vr.bin",					0x2000, 0x6d05e9f2, 5 | BRF_GRA },           // 12 Background Tiles
	{ "hb6vr.bin",					0x2000, 0x016340ae, 5 | BRF_GRA },           // 13
	{ "hb1vr.bin",					0x2000, 0x4352d069, 5 | BRF_GRA },           // 14
	{ "hb2vr.bin",					0x2000, 0x24edfd7d, 5 | BRF_GRA },           // 15
	{ "hb3vr.bin",					0x2000, 0x4947114e, 5 | BRF_GRA },           // 16
	{ "hb4vr.bin",					0x2000, 0xfa296cb3, 5 | BRF_GRA },           // 17

	{ "hs5vr.bin",					0x2000, 0x48394389, 6 | BRF_GRA },           // 18 Sprites
	{ "hs6vr.bin",					0x2000, 0x141409ec, 6 | BRF_GRA },           // 19
	{ "hs1vr.bin",					0x2000, 0x7c69b473, 6 | BRF_GRA },           // 20
	{ "hs2vr.bin",					0x2000, 0xc3dc713f, 6 | BRF_GRA },           // 21
	{ "hs3vr.bin",					0x2000, 0x883f93fd, 6 | BRF_GRA },           // 22
	{ "hs4vr.bin",					0x2000, 0x578ace7b, 6 | BRF_GRA },           // 23

	{ "24s10n.a3",					0x0100, 0xe8a9d159, 7 | BRF_GRA },           // 24 Color PROMs
	{ "24s10n.a1",					0x0100, 0x3956af86, 7 | BRF_GRA },           // 25
	{ "24s10n.a2",					0x0100, 0xf50f8ec5, 7 | BRF_GRA },           // 26
	{ "24s10n.b6",					0x0100, 0x8835a069, 7 | BRF_GRA },           // 27

	{ "24s10n.b7",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 28 Unused PROMs
	{ "24s10n.b9",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 29
	{ "24s10n.b10",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 30
	{ "18s030.h3",					0x0020, 0x33b98466, 0 | BRF_OPT },           // 31
};

STD_ROM_PICK(bullfgtr)
STD_ROM_FN(bullfgtr)

struct BurnDriver BurnDrvBullfgtr = {
	"bullfgtr", NULL, NULL, "ad59mc07", "1984",
	"Bull Fighter (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, bullfgtrRomInfo, bullfgtrRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, BullfgtrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};


// Bull Fighter (World, Alt)

static struct BurnRomInfo bullfgtrsRomDesc[] = {
	{ "m_d13.bin",					0x2000, 0x7c35dd4b, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "m_b13.bin",					0x2000, 0xc4adddce, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m_d12.bin",					0x2000, 0x5d51be2b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m_b12.bin",					0x2000, 0xd98390ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "m_d10.bin",					0x2000, 0x21875752, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "m_b10.bin",					0x2000, 0x9d84f678, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "hv1vr.bin",					0x2000, 0x2a8e6fcf, 2 | BRF_PRG | BRF_ESS }, //  6 I8085 Code
	{ "hv2vr.bin",					0x2000, 0x026e1533, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "hv3vr.bin",					0x2000, 0x51ee751c, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "hv4vr.bin",					0x2000, 0x62c7a25b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "alpha-8303_44801b42.bin",	0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, // 10 Alpha 8xxx MCU Code

	{ "h.bin",						0x1000, 0xc6894c9a, 4 | BRF_GRA },           // 11 Characters

	{ "hb5vr.bin",					0x2000, 0x6d05e9f2, 5 | BRF_GRA },           // 12 Background Tiles
	{ "hb6vr.bin",					0x2000, 0x016340ae, 5 | BRF_GRA },           // 13
	{ "hb1vr.bin",					0x2000, 0x4352d069, 5 | BRF_GRA },           // 14
	{ "hb2vr.bin",					0x2000, 0x24edfd7d, 5 | BRF_GRA },           // 15
	{ "hb3vr.bin",					0x2000, 0x4947114e, 5 | BRF_GRA },           // 16
	{ "hb4vr.bin",					0x2000, 0xfa296cb3, 5 | BRF_GRA },           // 17

	{ "hs5vr.bin",					0x2000, 0x48394389, 6 | BRF_GRA },           // 18 Sprites
	{ "hs6vr.bin",					0x2000, 0x141409ec, 6 | BRF_GRA },           // 19
	{ "hs1vr.bin",					0x2000, 0x7c69b473, 6 | BRF_GRA },           // 20
	{ "hs2vr.bin",					0x2000, 0xc3dc713f, 6 | BRF_GRA },           // 21
	{ "hs3vr.bin",					0x2000, 0x883f93fd, 6 | BRF_GRA },           // 22
	{ "hs4vr.bin",					0x2000, 0x578ace7b, 6 | BRF_GRA },           // 23

	{ "24s10n.a3",					0x0100, 0xe8a9d159, 7 | BRF_GRA },           // 24 Color PROMs
	{ "24s10n.a1",					0x0100, 0x3956af86, 7 | BRF_GRA },           // 25
	{ "24s10n.a2",					0x0100, 0xf50f8ec5, 7 | BRF_GRA },           // 26
	{ "24s10n.b6",					0x0100, 0x8835a069, 7 | BRF_GRA },           // 27

	{ "24s10n.b7",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 28 Unused PROMs
	{ "24s10n.b9",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 29
	{ "24s10n.b10",					0x0100, 0x8835a069, 0 | BRF_OPT },           // 30
	{ "18s030.h3",					0x0020, 0x33b98466, 0 | BRF_OPT },           // 31
};

STD_ROM_PICK(bullfgtrs)
STD_ROM_FN(bullfgtrs)

struct BurnDriver BurnDrvBullfgtrs = {
	"bullfgtrs", "bullfgtr", NULL, "ad59mc07", "1984",
	"Bull Fighter (World, Alt)\0", NULL, "Alpha Denshi Co. (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, bullfgtrsRomInfo, bullfgtrsRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, BullfgtrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};


// Koukou Yakyuu, The (Japan)

static struct BurnRomInfo kouyakyuRomDesc[] = {
	{ "epr-6704.bin",				0x2000, 0xc7ac2292, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "epr-6707.bin",				0x2000, 0x9cb2962e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-6705.bin",				0x2000, 0x985327cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-6708.bin",				0x2000, 0xf8863dc5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-6706.bin",				0x2000, 0x79e94cd2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-6709.bin",				0x2000, 0xf41cb58c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "epr-6703.bin",				0x2000, 0xfbff3a86, 2 | BRF_PRG | BRF_ESS }, //  6 I8085 Code
	{ "epr-6702.bin",				0x2000, 0x27ddf031, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "epr-6701.bin",				0x2000, 0x3c83588a, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "epr-6700.bin",				0x2000, 0xee579266, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "epr-6699.bin",				0x2000, 0x9bfa4a72, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "epr-6698.bin",				0x2000, 0x7adfd1ff, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "alpha-8505_44801c57.bin",	0x2000, 0x1f5a1405, 3 | BRF_PRG | BRF_ESS }, // 12 Alpha 8xxx MCU Code

	{ "epr-6710.bin",				0x1000, 0xaccda190, 4 | BRF_GRA },           // 13 Characters

	{ "epr-6695.bin",				0x2000, 0x22bea465, 5 | BRF_GRA },           // 14 Background Tiles
	{ "epr-6694.bin",				0x2000, 0x51a7345e, 5 | BRF_GRA },           // 15
	{ "epr-6689.bin",				0x2000, 0x53bf7587, 5 | BRF_GRA },           // 16
	{ "epr-6688.bin",				0x2000, 0xceb76c5b, 5 | BRF_GRA },           // 17
	{ "epr-6687.bin",				0x2000, 0x9c1f49df, 5 | BRF_GRA },           // 18
	{ "epr-6686.bin",				0x2000, 0x3d9e516f, 5 | BRF_GRA },           // 19

	{ "epr-6696.bin",				0x2000, 0x0625f48e, 6 | BRF_GRA },           // 20 Sprites
	{ "epr-6697.bin",				0x2000, 0xf18afabe, 6 | BRF_GRA },           // 21
	{ "epr-6690.bin",				0x2000, 0xa142a11d, 6 | BRF_GRA },           // 22
	{ "epr-6691.bin",				0x2000, 0xb640568c, 6 | BRF_GRA },           // 23
	{ "epr-6692.bin",				0x2000, 0xb91d8172, 6 | BRF_GRA },           // 24
	{ "epr-6693.bin",				0x2000, 0x874e3acc, 6 | BRF_GRA },           // 25

	{ "pr6627.bpr",					0x0100, 0x5ec5480d, 7 | BRF_GRA },           // 26 Color PROMs
	{ "pr6629.bpr",					0x0100, 0x29c7a393, 7 | BRF_GRA },           // 27
	{ "pr6628.bpr",					0x0100, 0x8af247a4, 7 | BRF_GRA },           // 28
	{ "pr6630a.bpr",				0x0100, 0xd6e202da, 7 | BRF_GRA },           // 29

	{ "pr6630b.bpr",				0x0100, 0xd6e202da, 0 | BRF_OPT },           // 30 Unused PROMs
	{ "pr6630c.bpr",				0x0100, 0xd6e202da, 0 | BRF_OPT },           // 31
	{ "pr6630d.bpr",				0x0100, 0xd6e202da, 0 | BRF_OPT },           // 32
	{ "pr.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 33
};

STD_ROM_PICK(kouyakyu)
STD_ROM_FN(kouyakyu)

struct BurnDriver BurnDrvKouyakyu = {
	"kouyakyu", NULL, NULL, "ad59mc07", "1985",
	"Koukou Yakyuu, The (Japan)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	L"Koukou Yakyuu, The (Japan)\0\u30b6\u30fb\u9ad8\u6821\u91ce\u7403\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, kouyakyuRomInfo, kouyakyuRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, KouyakyuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 208, 4, 3
};


// Gekisou (Japan)

static struct BurnRomInfo gekisouRomDesc[] = {
	{ "1.15b",						0x4000, 0x945fd546, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "2.15d",						0x4000, 0x3c057150, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.14b",						0x4000, 0x7c1cf4d0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.14d",						0x4000, 0xc7282391, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "v1.1l",						0x4000, 0xdc6af437, 2 | BRF_PRG | BRF_ESS }, //  4 I8085 Code
	{ "v2.1h",						0x4000, 0xcb12582e, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "v3.1e",						0x4000, 0x0ab5e777, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha-8505_44801c57.bin",	0x2000, 0x1f5a1405, 3 | BRF_PRG | BRF_ESS }, //  7 Alpha 8xxx MCU Code

	{ "0.5c",						0x1000, 0x7e8bf4d1, 4 | BRF_GRA },           //  8 Characters

	{ "7.18r",						0x4000, 0xa1918b6c, 5 | BRF_GRA },           //  9 Background Tiles
	{ "5.16r",						0x4000, 0x88ef550a, 5 | BRF_GRA },           // 10
	{ "6.15r",						0x4000, 0x473e3fbf, 5 | BRF_GRA },           // 11

	{ "10.9r",						0x4000, 0x11d89c73, 6 | BRF_GRA },           // 12 Sprites
	{ "8.8r",						0x4000, 0x2e0c392c, 6 | BRF_GRA },           // 13
	{ "9.6r",						0x4000, 0x56a03b08, 6 | BRF_GRA },           // 14

	{ "1b.bpr",						0x0100, 0x11a1c0aa, 7 | BRF_GRA },           // 15 Color PROMs
	{ "4b.bpr",						0x0100, 0xc7ebe52c, 7 | BRF_GRA },           // 16
	{ "2b.bpr",						0x0100, 0x4f5d4141, 7 | BRF_GRA },           // 17
	{ "2n.bpr",						0x0100, 0xc7333120, 7 | BRF_GRA },           // 18

	{ "3n.bpr",						0x0100, 0xc7333120, 0 | BRF_OPT },           // 19 Unused PROMs
	{ "4n.bpr",						0x0100, 0xc7333120, 0 | BRF_OPT },           // 20
	{ "5n.bpr",						0x0100, 0xc7333120, 0 | BRF_OPT },           // 21
	{ "3h.bpr",						0x0020, 0x33b98466, 0 | BRF_OPT },           // 22
};

STD_ROM_PICK(gekisou)
STD_ROM_FN(gekisou)

static INT32 GekisouInit()
{
	has_nvram = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvGekisou = {
	"gekisou", NULL, NULL, "ad59mc07", "1985",
	"Gekisou (Japan)\0", NULL, "Eastern Corp.", "Miscellaneous",
	L"Gekisou (Japan)\0\u6fc0\u8d70\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_RACING, 0,
	NULL, gekisouRomInfo, gekisouRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, EquitesInputInfo, GekisouDIPInfo,
	GekisouInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};


// Bingo Time (World)

static struct BurnRomInfo bngotimeRomDesc[] = {
	{ "1.b15",						0x4000, 0x34a27f5c, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "0.d15",						0x4000, 0x21c738ee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.b14",						0x4000, 0xe22555ab, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.d14",						0x4000, 0x0f328bde, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "11.sub",						0x2000, 0x9b063c07, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "alpha-8505_44801c57.bin",	0x2000, 0x1f5a1405, 3 | BRF_PRG | BRF_ESS }, //  5 Alpha 8xxx MCU Code

	{ "9.d5",						0x1000, 0x3c356e82, 4 | BRF_GRA },           //  6 Characters

	{ "6.r18",						0x4000, 0xe85790f2, 5 | BRF_GRA },           //  7 Background Tiles
	{ "4.r16",						0x4000, 0x58479aaf, 5 | BRF_GRA },           //  8
	{ "5.r15",						0x4000, 0x561dbbf6, 5 | BRF_GRA },           //  9

	{ "8.r9",						0x2000, 0x067fd3a1, 6 | BRF_GRA },           // 10 Sprites
	{ "7.r8",						0x2000, 0x4f50006a, 6 | BRF_GRA },           // 11

	{ "82s129_br.b1",				0x0100, 0xfd98b98a, 7 | BRF_GRA },           // 12 Color PROMs
	{ "82s129_bg.b4",				0x0100, 0x68d61fca, 7 | BRF_GRA },           // 13
	{ "82s129_bb.b2",				0x0100, 0x839bc7a3, 7 | BRF_GRA },           // 14
	{ "82s129_bs.n2",				0x0100, 0x1ecbeb37, 7 | BRF_GRA },           // 15

	{ "82s129_bs.n3",				0x0100, 0x1ecbeb37, 0 | BRF_OPT },           // 16 Unused PROMs
	{ "82s129_bs.n4",				0x0100, 0x1ecbeb37, 0 | BRF_OPT },           // 17
	{ "82s129_bs.n5",				0x0100, 0x1ecbeb37, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(bngotime)
STD_ROM_FN(bngotime)

static INT32 BngotimeInit()
{
	sound_type = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvBngotime = {
	"bngotime", NULL, NULL, "ad59mc07", "1986",
	"Bingo Time (World)\0", NULL, "CLS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, bngotimeRomInfo, bngotimeRomName, NULL, NULL, mc07SampleInfo, mc07SampleName, BngotimeInputInfo, BngotimeDIPInfo,
	BngotimeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	208, 256, 3, 4
};
