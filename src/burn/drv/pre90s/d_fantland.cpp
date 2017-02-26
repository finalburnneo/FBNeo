// FB Alpha Fantasy Land driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "burn_ym3526.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvI8086ROM;
static UINT8 *DrvI8088ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvI8088RAM;
static UINT8 *DrvI8086RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nmi_enable;
static UINT8 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvFake[8];

static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static INT32 game_select = 0;

static struct BurnInputInfo FantlandInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"	},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fantland)

static struct BurnInputInfo WheelrunInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Left",		BIT_DIGITAL,	DrvFake + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvFake + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Left",		BIT_DIGITAL,	DrvFake + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvFake + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wheelrun)

static struct BurnDIPInfo FantlandDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xfd, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "Invulnerability"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x11, 0x01, 0x10, 0x00, "No"			},
	{0x11, 0x01, 0x10, 0x10, "Yes"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Normal"		},
	{0x11, 0x01, 0x60, 0x40, "Hard"			},
	{0x11, 0x01, 0x60, 0x20, "Harder"		},
	{0x11, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x00, "On"			},
	{0x11, 0x01, 0x80, 0x80, "Off"			},

	{0   , 0xfe, 0   ,    2, "Test Sound"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x12, 0x01, 0x0e, 0x0e, "1"			},
	{0x12, 0x01, 0x0e, 0x0c, "2"			},
	{0x12, 0x01, 0x0e, 0x0a, "3"			},
	{0x12, 0x01, 0x0e, 0x08, "4"			},
	{0x12, 0x01, 0x0e, 0x06, "5"			},
	{0x12, 0x01, 0x0e, 0x04, "6"			},
	{0x12, 0x01, 0x0e, 0x02, "7"			},
	{0x12, 0x01, 0x0e, 0x00, "8"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x30, 0x30, "800k"			},
	{0x12, 0x01, 0x30, 0x20, "1600k"		},
	{0x12, 0x01, 0x30, 0x10, "2400k"		},
	{0x12, 0x01, 0x30, 0x00, "3200k"		},
};

STDDIPINFO(Fantland)

static struct BurnDIPInfo GalaxygnDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xf9, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Normal"		},
	{0x11, 0x01, 0x60, 0x40, "Hard"			},
	{0x11, 0x01, 0x60, 0x20, "Harder"		},
	{0x11, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x00, "On"			},
	{0x11, 0x01, 0x80, 0x80, "Off"			},

	{0   , 0xfe, 0   ,    2, "Test Sound"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x12, 0x01, 0x0e, 0x0e, "1"			},
	{0x12, 0x01, 0x0e, 0x0c, "2"			},
	{0x12, 0x01, 0x0e, 0x0a, "3"			},
	{0x12, 0x01, 0x0e, 0x08, "4"			},
	{0x12, 0x01, 0x0e, 0x06, "5"			},
	{0x12, 0x01, 0x0e, 0x04, "6"			},
	{0x12, 0x01, 0x0e, 0x02, "7"			},
	{0x12, 0x01, 0x0e, 0x00, "8"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x30, 0x30, "10k"			},
	{0x12, 0x01, 0x30, 0x20, "20k"			},
	{0x12, 0x01, 0x30, 0x10, "30k"			},
	{0x12, 0x01, 0x30, 0x00, "40k"			},
};

STDDIPINFO(Galaxygn)

static struct BurnDIPInfo WheelrunDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL			},
	{0x0a, 0xff, 0xff, 0xdf, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x09, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x09, 0x01, 0x08, 0x00, "Off"			},
	{0x09, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x09, 0x01, 0x10, 0x00, "No"			},
	{0x09, 0x01, 0x10, 0x10, "Yes"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x09, 0x01, 0x60, 0x60, "Normal"		},
	{0x09, 0x01, 0x60, 0x40, "Hard"			},
	{0x09, 0x01, 0x60, 0x20, "Harder"		},
	{0x09, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Wheel Sensitivity"	},
	{0x0a, 0x01, 0xff, 0x7f, "0"			},
	{0x0a, 0x01, 0xff, 0xbf, "1"			},
	{0x0a, 0x01, 0xff, 0xdf, "2"			},
	{0x0a, 0x01, 0xff, 0xef, "3"			},
	{0x0a, 0x01, 0xff, 0xf7, "4"			},
	{0x0a, 0x01, 0xff, 0xfb, "5"			},
	{0x0a, 0x01, 0xff, 0xfd, "6"			},
	{0x0a, 0x01, 0xff, 0xfe, "7"			},
};

STDDIPINFO(Wheelrun)

static void __fastcall fantland_main_write(UINT32 address, UINT8 data)
{
	//bprintf(0, _T("mw %X %X. "), address ,data);
	switch (address)
	{
		case 0x53000:
		//case 0x53001:
		case 0xa3000:
		case 0xa3001:
			nmi_enable = data & 0x08;
		return;

		case 0x53002:
	//	case 0x53003:
		case 0xa3002:
	//	case 0xa3003:
			soundlatch = data;
			if (game_select < 2) {
				VezClose();
				VezOpen(1);
				VezSetIRQLineAndVector(0x20, 0xff, CPU_IRQSTATUS_AUTO);
				VezClose();
				VezOpen(0);
			} else {
				ZetNmi();
			}
		return;
	}
}

static UINT8 __fastcall fantland_main_read(UINT32 address)
{
	switch (address)
	{
		case 0x53000:
		case 0x53001:
		case 0xa3000:
		case 0xa3001:
		case 0x53002:
		case 0x53003:
		case 0xa3002:
		case 0xa3003:
			return DrvInputs[address & 3];
	}

	return 0;
}

static void __fastcall fantland_sound_write_port(UINT32 port, UINT8 data)
{
	switch (port)
	{
		case 0x0100:
			BurnYM2151SelectRegister(data);
		return;

		case 0x0101:
			BurnYM2151WriteRegister(data);
		return;

		case 0x0180:
			DACSignedWrite(0, data);
		return;
	}
	bprintf(0, _T("wp %X %X. "), port, data);
}

static UINT8 __fastcall fantland_sound_read_port(UINT32 port)
{
	switch (port)
	{
		case 0x0080:
			return soundlatch;

		case 0x0101:
			return BurnYM2151ReadStatus();
	}
	bprintf(0, _T("rp %X. "), port);
	return 0;
}

static void __fastcall wheelrun_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM3526Write(address & 1, data);
		return;

		case 0xb000:
		case 0xc000:
		return;
	}
}

static UINT8 __fastcall wheelrun_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM3526Read(address & 1);

		case 0xd000:
			return soundlatch;
	}

	return 0;
}

static void DrvYM3526IrqHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	if (VezGetActive() == -1) return;

	if (nStatus) VezSetIRQLineAndVector(0, 0x80/4, CPU_IRQSTATUS_AUTO);
	// Galaxygn soundcpu will eventually crash with the traditional method, below...
	//VezSetIRQLineAndVector(0, 0x80/4, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (VezTotalCycles() / (8000000.0000 / (nBurnFPS / 100.0000))));
}

static INT32 SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 9000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	VezOpen(0);
	VezReset();
	VezClose();

	if (game_select < 2)
	{
		VezOpen(1);
		VezReset();
		DACReset();
		BurnYM2151Reset();
		VezClose();
	}
	else if (game_select == 2)
	{
		ZetOpen(0);
		ZetReset();
		BurnYM3526Reset();
		ZetClose();
	}

	soundlatch = 0;
	nmi_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8086ROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next;
	DrvI8088ROM		= Next; Next += 0x100000;

	DrvGfxROM		= Next; Next += 0x600000;

	DrvPalette		= (UINT32*)Next; Next += 0x01000 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8086RAM		= Next; Next += 0x008000;
	DrvZ80RAM		= Next;
	DrvI8088RAM		= Next; Next += 0x002000;

	DrvPalRAM		= Next; Next += 0x000200;
	DrvSprRAM0		= Next; Next += 0x002800;
	DrvSprRAM1		= Next; Next += 0x010000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[6] = { 0,1,2,3,4,5 };
	INT32 XOffs[16] = { STEP4(3*6,-6),STEP4(7*6,-6),STEP4(11*6,-6),STEP4(15*6,-6) };
	INT32 YOffs[16] = { STEP16(0,16*6) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x480000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x480000);

	GfxDecode(0x6000, 6, 16, 16, Plane, XOffs, YOffs, 0x600, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 FantlandInit()
{
	game_select = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvI8086ROM + 0x00000,  0, 2)) return 1;
		if (BurnLoadRom(DrvI8086ROM + 0x00001,  1, 2)) return 1;
		memcpy (DrvI8086ROM + 0x40000, DrvI8086ROM, 0x40000);
		if (BurnLoadRom(DrvI8086ROM + 0xe0000,  2, 2)) return 1;
		if (BurnLoadRom(DrvI8086ROM + 0xe0001,  3, 2)) return 1;

		if (BurnLoadRom(DrvI8088ROM + 0x80000,  4, 1)) return 1;
		if (BurnLoadRom(DrvI8088ROM + 0xc0000,  5, 1)) return 1;
		memcpy (DrvI8088ROM + 0xe0000, DrvI8088ROM + 0xc0000, 0x20000);

		UINT8 *tmp = (UINT8*)BurnMalloc(0x280000);
		if (BurnLoadRom(tmp       + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(tmp       + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(tmp       + 0x100000,  9, 1)) return 1;
		if (BurnLoadRom(tmp       + 0x180000, 10, 1)) return 1;
		if (BurnLoadRom(tmp       + 0x200000, 12, 1)) return 1;

		for (INT32 i = 0, j = 0; i < 0x280000; i+=2, j+=3) {
			memcpy (DrvGfxROM + j, tmp + i, 2);
		}

		BurnFree(tmp);

		if (BurnLoadRom(DrvGfxROM + 0x000002,  8, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x180002, 11, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x3c0001, 13, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x3c0000, 14, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x300002, 15, 3)) return 1;

		DrvGfxDecode();
	}

	VezInit(0, V30_TYPE, 8000000);
	VezOpen(0);
	VezMapMemory(DrvI8086RAM,		0x00000, 0x07fff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0x08000,	0x08000, 0x7ffff, MAP_ROM);
	VezMapMemory(DrvPalRAM,			0xa2000, 0xa21ff, MAP_RAM);
	VezMapMemory(DrvSprRAM0,		0xa4000, 0xa67ff, MAP_RAM);
	VezMapMemory(DrvSprRAM1,		0xc0000, 0xcffff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0xe0000,	0xe0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(fantland_main_write);
	VezSetReadHandler(fantland_main_read);
	VezClose();

	VezInit(1, V20_TYPE, 8000000);
	VezOpen(1);
	VezMapMemory(DrvI8088RAM,		0x00000, 0x01fff, MAP_RAM);
	VezMapMemory(DrvI8088ROM + 0x80000,	0x80000, 0xfffff, MAP_ROM);
	VezSetWritePort(fantland_sound_write_port);
	VezSetReadPort(fantland_sound_read_port);
	VezClose();

	BurnYM2151Init(3000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.55, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.55, BURN_SND_ROUTE_RIGHT);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 GalaxygnInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvI8086ROM + 0x10000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8086ROM + 0x20000,  1, 1)) return 1;
		if (BurnLoadRom(DrvI8086ROM + 0xf0000,  2, 1)) return 1;
		memcpy (DrvI8086ROM + 0x70000, DrvI8086ROM + 0xf0000, 0x10000);

		if (BurnLoadRom(DrvI8088ROM + 0xc0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x000000,   4, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000001,   5, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000002,   6, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030000,   7, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030001,   8, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030002,   9, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060000,  10, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060001,  11, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060002,  12, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090000,  13, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090001,  14, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090002,  15, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0c0000,  16, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0c0001,  17, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0c0002,  18, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0f0000,  19, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0f0001,  20, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0f0002,  21, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x120000,  22, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x120001,  23, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x120002,  24, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x150000,  25, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x150001,  26, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x150002,  27, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x180000,  28, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x180001,  29, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x180002,  30, 3)) return 1;

		DrvGfxDecode();
	}

	VezInit(0, V20_TYPE, 8000000);
	VezOpen(0);
	VezMapMemory(DrvI8086RAM,		0x00000, 0x07fff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0x10000,	0x10000, 0x2ffff, MAP_ROM);
	VezMapMemory(DrvPalRAM,			0x52000, 0x521ff, MAP_RAM);
	VezMapMemory(DrvSprRAM0,		0x54000, 0x567ff, MAP_RAM);
	VezMapMemory(DrvSprRAM1,		0x60000, 0x6ffff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0x70000,	0x70000, 0x7ffff, MAP_ROM);
	VezMapMemory(DrvI8086ROM + 0xf0000,	0xf0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(fantland_main_write);
	VezSetReadHandler(fantland_main_read);
	VezClose();

	VezInit(1, V20_TYPE, 8000000);
	VezOpen(1);
	VezMapMemory(DrvI8088RAM,		0x00000, 0x01fff, MAP_RAM);
	VezMapMemory(DrvI8088ROM + 0xc0000,	0xc0000, 0xcffff, MAP_ROM);
	VezMapMemory(DrvI8088ROM + 0xc0000,	0xd0000, 0xdffff, MAP_ROM);
	VezMapMemory(DrvI8088ROM + 0xc0000,	0xe0000, 0xeffff, MAP_ROM);
	VezMapMemory(DrvI8088ROM + 0xc0000,	0xf0000, 0xfffff, MAP_ROM);
	VezSetWritePort(fantland_sound_write_port);
	VezSetReadPort(fantland_sound_read_port);
	VezClose();

	BurnYM2151Init(3000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.55, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.55, BURN_SND_ROUTE_RIGHT);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 WheelrunInit()
{
	game_select = 2;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvI8086ROM + 0x30000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8086ROM + 0xf0000,  1, 1)) return 1;
		memcpy (DrvI8086ROM + 0x70000, DrvI8086ROM + 0xf0000, 0x10000);

		if (BurnLoadRom(DrvZ80ROM + 0x00000,    2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x000000,   3, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000001,   4, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000002,   5, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030000,   6, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030001,   7, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x030002,   8, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060000,   9, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060001,  10, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x060002,  11, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090000,  12, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090001,  13, 3)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x090002,  14, 3)) return 1;

		DrvGfxDecode();
	}

	VezInit(0, V20_TYPE, 9000000);
	VezOpen(0);
	VezMapMemory(DrvI8086RAM,		0x00000, 0x07fff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0x30000,	0x30000, 0x3ffff, MAP_ROM);
	VezMapMemory(DrvPalRAM,			0x52000, 0x521ff, MAP_RAM);
	VezMapMemory(DrvSprRAM0,		0x54000, 0x567ff, MAP_RAM);
	VezMapMemory(DrvSprRAM1,		0x60000, 0x6ffff, MAP_RAM);
	VezMapMemory(DrvI8086ROM + 0x70000,	0x70000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(fantland_main_write);
	VezSetReadHandler(fantland_main_read);
	VezClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(wheelrun_sound_write);
	ZetSetReadHandler(wheelrun_sound_read);
	ZetClose();

	BurnYM3526Init(3500000, &DrvYM3526IrqHandler, &SynchroniseStream, 0);
	BurnTimerAttachZetYM3526(9000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();

	if (game_select < 2) {
		BurnYM2151Exit();
		DACExit();
	} else if (game_select == 2) {
		ZetExit();
		BurnYM3526Exit();
	}

	BurnFree(AllMem);

	game_select = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 r = (p[i] >> 10) & 0x1f;
		INT32 g = (p[i] >>  5) & 0x1f;
		INT32 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT8 *spriteram_2 =  DrvSprRAM1;
	UINT8 *indx_ram    =   DrvSprRAM0 + 0x2000;	// this ram contains indexes into offs_ram
	UINT8 *offs_ram    =   DrvSprRAM0 + 0x2400;	// this ram contains x,y offsets or indexes into spriteram_2
	UINT8 *ram         =   DrvSprRAM0;		// current sprite pointer in spriteram
	UINT8 *ram2        =   indx_ram;		// current sprite pointer in indx_ram

	INT32 special = (nScreenHeight < 256) ? 1 : 0;	// wheelrun is the only game with a smaller visible area

	for ( ; ram < indx_ram; ram += 8,ram2++)
	{
		INT32 attr,code,color, x,y,xoffs,yoffs,flipx,flipy, idx;

		attr    =   ram[1];

		x       =   ram[0];
		code    =   ram[3] + (ram[2] << 8);
		y       =   ram[4];

		color   =   (attr & 0x03);
		flipy   =   (attr & 0x10) ? 1 : 0;
		flipx   =   (attr & 0x20) ? 1 : 0;

		y       +=  (attr & 0x40) << 2;
		x       +=  (attr & 0x80) << 1;

		idx     =   ram2[0] * 4;

		if (offs_ram[idx + 2] & 0x80)
		{
			idx     =   (((offs_ram[idx + 2] << 8) + offs_ram[idx + 3]) & 0x3fff) * 4;

			yoffs   =   spriteram_2[idx + 0] + (spriteram_2[idx + 1] << 8);
			xoffs   =   spriteram_2[idx + 2] + (spriteram_2[idx + 3] << 8);

			code    +=  (yoffs & 0x3e00) >> 9;
			flipy   ^=  (yoffs & 0x4000) ? 1 : 0;
			flipx   ^=  (yoffs & 0x8000) ? 1 : 0;
		}
		else
		{
			yoffs   =   ((offs_ram[idx + 3] & 0x01) << 8) + offs_ram[idx + 1];
			xoffs   =   ((offs_ram[idx + 2] & 0x01) << 8) + offs_ram[idx + 0];
		}

		yoffs   =   (yoffs & 0xff) - (yoffs & 0x100);
		xoffs   =   (xoffs & 0x1ff);

		if (xoffs >= 0x180) xoffs -= 0x200;

		y       +=  yoffs;
		x       +=  xoffs;

		if (special && y > 0) y &= 0xff;

		y       =   (y & 0xff) - (y & 0x100);
		x       =   (x & 0x1ff);

		if (x >= 0x180) x -= 0x200;

		code %= 0x6000; // largest graphics size (fantland)

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 6, 0, 0, DrvGfxROM);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 6, 0, 0, DrvGfxROM);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 6, 0, 0, DrvGfxROM);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 6, 0, 0, DrvGfxROM);
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	BurnTransferClear();

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 FantlandFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		DrvInputs[2] = DrvDips[0];
		DrvInputs[3] = DrvDips[1];
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 133; // periodic int for sound cpu (dac)
	INT32 nCyclesTotal[2] = { 8000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSegment = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		VezOpen(0);
		nSegment = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += VezRun(nSegment - nCyclesDone[0]);
		if (i == (nInterleave - 1) && nmi_enable) VezSetIRQLineAndVector(0x20, 0xff, CPU_IRQSTATUS_AUTO);
		VezClose();

		VezOpen(1);
		nSegment = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += VezRun(nSegment - nCyclesDone[1]);
		if (game_select == 0) VezSetIRQLineAndVector(0, 0x80/4, CPU_IRQSTATUS_AUTO);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
		VezClose();
	}

	VezOpen(1);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	VezClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}


static INT32 WheelrunFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		DrvInputs[2] = DrvDips[0];
		DrvInputs[3] = DrvDips[1];

		DrvInputs[0] &= ~0x70;
		DrvInputs[1] &= ~0x70;

		UINT8 nWheel = ((DrvFake[0] ? 5 : 0) | (DrvFake[1] ? 3 : 0)) << 4;
		if (nWheel == 0) nWheel = 0x40;
		DrvInputs[0] |= nWheel;
		nWheel = ((DrvFake[2] ? 5 : 0) | (DrvFake[3] ? 3 : 0)) << 4;
		if (nWheel == 0) nWheel = 0x40;
		DrvInputs[1] |= nWheel;
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 9000000 / 60, 9000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	VezOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += VezRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1) && nmi_enable) VezSetIRQLineAndVector(0x20, 0, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3526(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	VezClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		VezScan(nAction);

		if (game_select < 2)
		{
			BurnYM2151Scan(nAction);
			DACScan(nAction,pnMin);
		}

		if (game_select == 2)
		{
			ZetScan(nAction);

			ZetOpen(0);
			BurnYM3526Scan(nAction, pnMin);
			ZetClose();
		}

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
	}

	return 0;
}


// Fantasy Land (set 1)

static struct BurnRomInfo fantlandRomDesc[] = {
	{ "fantasyl.ev2",	0x20000, 0xf5bdca0e, 1 | BRF_PRG | BRF_ESS }, //  0 I8086 code
	{ "fantasyl.od2",	0x20000, 0x9db35023, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fantasyl.ev1",	0x10000, 0x70e0ee30, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fantasyl.od1",	0x10000, 0x577b4bd7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fantasyl.s2",	0x20000, 0xf23837d8, 2 | BRF_PRG | BRF_ESS }, //  4 I8088 code
	{ "fantasyl.s1",	0x20000, 0x1a324a69, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "fantasyl.m00",	0x80000, 0x82d819ff, 3 | BRF_GRA },           //  6 Sprites
	{ "fantasyl.m01",	0x80000, 0x70a94139, 3 | BRF_GRA },           //  7
	{ "fantasyl.05",	0x80000, 0x62b9a00b, 3 | BRF_GRA },           //  8
	{ "fantasyl.m02",	0x80000, 0xae52bf37, 3 | BRF_GRA },           //  9
	{ "fantasyl.m03",	0x80000, 0xf3f534a1, 3 | BRF_GRA },           // 10
	{ "fantasyl.06",	0x80000, 0x867fa549, 3 | BRF_GRA },           // 11
	{ "fantasyl.m04",	0x80000, 0xe7b1918c, 3 | BRF_GRA },           // 12
	{ "fantasyl.d0",	0x20000, 0x0f907f19, 3 | BRF_GRA },           // 13
	{ "fantasyl.d1",	0x20000, 0x10d10389, 3 | BRF_GRA },           // 14
	{ "fantasyl.07",	0x80000, 0x162ad422, 3 | BRF_GRA },           // 15
};

STD_ROM_PICK(fantland)
STD_ROM_FN(fantland)

struct BurnDriver BurnDrvFantland = {
	"fantland", NULL, NULL, NULL, "19??",
	"Fantasy Land (set 1)\0", NULL, "Electronic Devices Italy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, fantlandRomInfo, fantlandRomName, NULL, NULL, FantlandInputInfo, FantlandDIPInfo,
	FantlandInit, DrvExit, FantlandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 256, 4, 3
};


// Fantasy Land (set 2)

static struct BurnRomInfo fantlandaRomDesc[] = {
	{ "fantasyl.ev2",	0x20000, 0xf5bdca0e, 1 | BRF_PRG | BRF_ESS }, //  0 I8086 code
	{ "fantasyl.od2",	0x20000, 0x9db35023, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "02.bin",		0x10000, 0x8b835eed, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "01.bin",		0x10000, 0x4fa3eb8b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fantasyl.s2",	0x20000, 0xf23837d8, 2 | BRF_PRG | BRF_ESS }, //  4 I8088 code
	{ "fantasyl.s1",	0x20000, 0x1a324a69, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "fantasyl.m00",	0x80000, 0x82d819ff, 3 | BRF_GRA },           //  6 Sprites
	{ "fantasyl.m01",	0x80000, 0x70a94139, 3 | BRF_GRA },           //  7
	{ "fantasyl.05",	0x80000, 0x62b9a00b, 3 | BRF_GRA },           //  8
	{ "fantasyl.m02",	0x80000, 0xae52bf37, 3 | BRF_GRA },           //  9
	{ "fantasyl.m03",	0x80000, 0xf3f534a1, 3 | BRF_GRA },           // 10
	{ "fantasyl.06",	0x80000, 0x867fa549, 3 | BRF_GRA },           // 11
	{ "fantasyl.m04",	0x80000, 0xe7b1918c, 3 | BRF_GRA },           // 12
	{ "fantasyl.d0",	0x20000, 0x0f907f19, 3 | BRF_GRA },           // 13
	{ "fantasyl.d1",	0x20000, 0x10d10389, 3 | BRF_GRA },           // 14
	{ "fantasyl.07",	0x80000, 0x162ad422, 3 | BRF_GRA },           // 15
};

STD_ROM_PICK(fantlanda)
STD_ROM_FN(fantlanda)

struct BurnDriver BurnDrvFantlanda = {
	"fantlanda", "fantland", NULL, NULL, "19??",
	"Fantasy Land (set 2)\0", NULL, "Electronic Devices Italy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, fantlandaRomInfo, fantlandaRomName, NULL, NULL, FantlandInputInfo, FantlandDIPInfo,
	FantlandInit, DrvExit, FantlandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 256, 4, 3
};


// Galaxy Gunners

static struct BurnRomInfo galaxygnRomDesc[] = {
	{ "gg03.bin",		0x10000, 0x9e469189, 1 | BRF_PRG | BRF_ESS }, //  0 I8086 code
	{ "gg02.bin",		0x10000, 0xb87a438f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gg01.bin",		0x10000, 0xad0e5b29, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gg20.bin",		0x10000, 0xf5c65a85, 2 | BRF_PRG | BRF_ESS }, //  3 I8088 code

	{ "gg54.bin",		0x10000, 0xb3621119, 3 | BRF_GRA },           //  4 Sprites
	{ "gg38.bin",		0x10000, 0x52b70f3e, 3 | BRF_GRA },           //  5
	{ "gg22.bin",		0x10000, 0xea49fee4, 3 | BRF_GRA },           //  6
	{ "gg55.bin",		0x10000, 0xbffe278f, 3 | BRF_GRA },           //  7
	{ "gg39.bin",		0x10000, 0x3f7df1e6, 3 | BRF_GRA },           //  8
	{ "gg23.bin",		0x10000, 0x4dcbbc99, 3 | BRF_GRA },           //  9
	{ "gg56.bin",		0x10000, 0x0306069e, 3 | BRF_GRA },           // 10
	{ "gg40.bin",		0x10000, 0xf635aa7e, 3 | BRF_GRA },           // 11
	{ "gg24.bin",		0x10000, 0x733f5dcf, 3 | BRF_GRA },           // 12
	{ "gg57.bin",		0x10000, 0xc3919bef, 3 | BRF_GRA },           // 13
	{ "gg41.bin",		0x10000, 0x1f2757de, 3 | BRF_GRA },           // 14
	{ "gg25.bin",		0x10000, 0x1d094f95, 3 | BRF_GRA },           // 15
	{ "gg58.bin",		0x10000, 0x4a459cb8, 3 | BRF_GRA },           // 16
	{ "gg42.bin",		0x10000, 0xae7a8e1e, 3 | BRF_GRA },           // 17
	{ "gg26.bin",		0x10000, 0xc2f310b4, 3 | BRF_GRA },           // 18
	{ "gg59.bin",		0x10000, 0xc8d4fbc2, 3 | BRF_GRA },           // 19
	{ "gg43.bin",		0x10000, 0x74d3a0df, 3 | BRF_GRA },           // 20
	{ "gg27.bin",		0x10000, 0xc2cfd3f9, 3 | BRF_GRA },           // 21
	{ "gg60.bin",		0x10000, 0x6e32b549, 3 | BRF_GRA },           // 22
	{ "gg44.bin",		0x10000, 0xfcda6efa, 3 | BRF_GRA },           // 23
	{ "gg28.bin",		0x10000, 0x4d4fc01c, 3 | BRF_GRA },           // 24
	{ "gg61.bin",		0x10000, 0x177a767a, 3 | BRF_GRA },           // 25
	{ "gg45.bin",		0x10000, 0x2ba49d47, 3 | BRF_GRA },           // 26
	{ "gg29.bin",		0x10000, 0xc1c68148, 3 | BRF_GRA },           // 27
	{ "gg62.bin",		0x10000, 0x0fb2d41a, 3 | BRF_GRA },           // 28
	{ "gg46.bin",		0x10000, 0x5f1bf8ad, 3 | BRF_GRA },           // 29
	{ "gg30.bin",		0x10000, 0xded7cacf, 3 | BRF_GRA },           // 30
};

STD_ROM_PICK(galaxygn)
STD_ROM_FN(galaxygn)

struct BurnDriver BurnDrvGalaxygn = {
	"galaxygn", NULL, NULL, NULL, "1989",
	"Galaxy Gunners\0", "No Sound.", "Electronic Devices Italy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, galaxygnRomInfo, galaxygnRomName, NULL, NULL, FantlandInputInfo, GalaxygnDIPInfo,
	GalaxygnInit, DrvExit, FantlandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 352, 3, 4
};


// Wheels Runner

static struct BurnRomInfo wheelrunRomDesc[] = {
	{ "4.4",		0x10000, 0x359303df, 1 | BRF_PRG | BRF_ESS }, //  0 V20 code
	{ "3.3",		0x10000, 0xc28d0b31, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.1",		0x10000, 0x67b5f31f, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "7.7",		0x10000, 0xe0e5ff64, 3 | BRF_GRA },           //  3 Sprites
	{ "11.11",		0x10000, 0xce9718fb, 3 | BRF_GRA },           //  4
	{ "15.15",		0x10000, 0xf6665f31, 3 | BRF_GRA },           //  5
	{ "8.8",		0x10000, 0xfa1ec091, 3 | BRF_GRA },           //  6
	{ "12.12",		0x10000, 0x8923dce4, 3 | BRF_GRA },           //  7
	{ "16.16",		0x10000, 0x49801733, 3 | BRF_GRA },           //  8
	{ "9.9",		0x10000, 0x9fea30d0, 3 | BRF_GRA },           //  9
	{ "13.13",		0x10000, 0x8b0aae8d, 3 | BRF_GRA },           // 10
	{ "17.17",		0x10000, 0xbe8ab48d, 3 | BRF_GRA },           // 11
	{ "10.10",		0x10000, 0xc5bdd367, 3 | BRF_GRA },           // 12
	{ "14.14",		0x10000, 0xe592302f, 3 | BRF_GRA },           // 13
	{ "18.18",		0x10000, 0x6bd42d8e, 3 | BRF_GRA },           // 14

	{ "pal16r6cn.pal3",		0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 15 plds
	{ "pal16r6cn.pal4",		0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 16
	{ "pal16r6cn.pal5",		0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 17
	{ "tibpal16l8-25cn.pal1",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 18
	{ "tibpal16l8-25cn.pal13",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 19
	{ "tibpal16l8-25cn.pal14",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 20
	{ "tibpal16l8-25cn.pal7",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 21
	{ "tibpal16l8-25cn.pal8",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 22
	{ "tibpal16r4-25cn.pal10",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 23
	{ "tibpal16r4-25cn.pal15",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 24
	{ "tibpal16r4-25cn.pal6",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 25
	{ "tibpal16r4-25cn.pal9",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 26
	{ "tibpal16r8-25cn.pal2",	0x104, 0, 4 | BRF_NODUMP | BRF_OPT }, // 27
	{ "pal20l8acns.pal11",		0x144, 0, 4 | BRF_NODUMP | BRF_OPT }, // 28
	{ "pal20l8acns.pal12",		0x144, 0, 4 | BRF_NODUMP | BRF_OPT }, // 29
};

STD_ROM_PICK(wheelrun)
STD_ROM_FN(wheelrun)

struct BurnDriver BurnDrvWheelrun = {
	"wheelrun", NULL, NULL, NULL, "19??",
	"Wheels Runner\0", NULL, "International Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, wheelrunRomInfo, wheelrunRomName, NULL, NULL, WheelrunInputInfo, WheelrunDIPInfo,
	WheelrunInit, DrvExit, WheelrunFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


