// FB Alpha Air Buster: Trouble Specialty Raid Unit driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "pandora.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvShareRAM;
static UINT8 *DrvDevRAM;
static UINT8 *DrvPandoraRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;

static UINT8 *DrvScrollRegs;
static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *sound_status;
static UINT8 *sound_status2;
static UINT8 *coin_lockout;
static UINT8 *flipscreen;
static UINT8 *bankdata;

static INT32 nExtraCycles[2];

static INT32 is_bootleg;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo AirbustrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Airbustr)

static struct BurnDIPInfo AirbustrDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x02, 0x02, "Off"					},
	{0x12, 0x01, 0x02, 0x00, "On"					},
	
	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x04, 0x04, "Off"					},
	{0x12, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x12, 0x01, 0x08, 0x08, "Mode 1"				},
	{0x12, 0x01, 0x08, 0x00, "Mode 2"				},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"					},
	{0x13, 0x01, 0x03, 0x03, "Normal"				},
	{0x13, 0x01, 0x03, 0x01, "Difficult"			},
	{0x13, 0x01, 0x03, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x13, 0x01, 0x08, 0x08, "Off"					},
	{0x13, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x30, 0x30, "3"					},
	{0x13, 0x01, 0x30, 0x20, "4"					},
	{0x13, 0x01, 0x30, 0x10, "5"					},
	{0x13, 0x01, 0x30, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"					},
	{0x13, 0x01, 0x40, 0x40, "On"					},
};

STDDIPINFO(Airbustr)

static struct BurnDIPInfo AirbustjDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x02, 0x02, "Off"					},
	{0x12, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x04, 0x04, "Off"					},
	{0x12, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"					},
	{0x13, 0x01, 0x03, 0x03, "Normal"				},
	{0x13, 0x01, 0x03, 0x01, "Difficult"			},
	{0x13, 0x01, 0x03, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x13, 0x01, 0x08, 0x08, "Off"					},
	{0x13, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x30, 0x30, "3"					},
	{0x13, 0x01, 0x30, 0x20, "4"					},
	{0x13, 0x01, 0x30, 0x10, "5"					},
	{0x13, 0x01, 0x30, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"					},
	{0x13, 0x01, 0x40, 0x40, "On"					},
};

STDDIPINFO(Airbustj)

static void bankswitch(INT32 cpu, INT32 data)
{
	UINT8 *rom[3] = { DrvZ80ROM0, DrvZ80ROM1, DrvZ80ROM2 };

	bankdata[cpu] = data;

	ZetMapMemory(rom[cpu] + 0x4000 * (data & 0x07), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall airbustr_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xc000) { // pandora
		DrvSprRAM[address & 0xfff] = data;
		address = (address & 0x800) | ((address & 0xff) << 3) | ((address & 0x700) >> 8);
		DrvPandoraRAM[address] = data;
		return;
	}
}

static UINT8 __fastcall airbustr_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xefe0:
			return BurnWatchdogRead();

		case 0xeff2:
		case 0xeff3:
		{
			INT32 r = (DrvDevRAM[0xff0] + DrvDevRAM[0xff1] * 256) * (DrvDevRAM[0xff2] + DrvDevRAM[0xff3] * 256);

			if (address & 1) return r >> 8;
			return r;
		}

		case 0xeff4:
			return BurnRandom();
	}

	if ((address & 0xf000) == 0xe000) {
		return DrvDevRAM[address & 0xfff];
	}

	return 0;
}

static void __fastcall airbustr_main_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(0, data);
		return;

		case 0x01:
		return; // nop

		case 0x02:
		{
			ZetNmi(1);
		}
		return;
	}
}

static void __fastcall airbustr_sub_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		{
			bankswitch(1, data);
			*flipscreen = data & 0x10;
			pandora_set_clear(data & 0x20);
		}
		return;

		case 0x02:
		{
			*soundlatch = data;
			*sound_status = 1;
			ZetNmi(2);
		}
		return;

		case 0x04:
		case 0x06:
		case 0x08:
		case 0x0a:
		case 0x0c:
			DrvScrollRegs[((port & 0x0f) - 0x04) / 2] = data;
		return;

		case 0x28:
			*coin_lockout = ~data & 0x0c;
			// coin counter also (0x03)
		return;

		case 0x38:
		return; // nop
	}
}

static UINT8 __fastcall airbustr_sub_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
			*sound_status2 = 0;
			return *soundlatch2;

		case 0x0e:
			return (4 + *sound_status * 2 + (1 - *sound_status2));

		case 0x20:
			return DrvInputs[0];

		case 0x22:
			return DrvInputs[1];

		case 0x24:
			return DrvInputs[2] | *coin_lockout;
	}

	return 0;
}

static void __fastcall airbustr_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(2, data);
		return;

		case 0x02:
		case 0x03:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x04:
			MSM6295Write(0, data);
		return;

		case 0x06:
			*soundlatch2 = data;
			*sound_status2 = 1;
		return;
	}
}

static UINT8 __fastcall airbustr_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x03:
			return BurnYM2203Read(0, port & 1);

		case 0x04:
			return MSM6295Read(0);

		case 0x06:
			*sound_status = 0;
			return *soundlatch;
	}

	return 0;
}

static UINT8 DrvYM2203PortA(UINT32)
{
	return DrvDips[0];
}

static UINT8 DrvYM2203PortB(UINT32)
{
	return DrvDips[1];
}

static tilemap_callback( airbustr0 )
{
	INT32 attr = DrvVidRAM0[offs + 0x400];

	TILE_SET_INFO(0, DrvVidRAM0[offs] + ((attr & 0x0f) << 8), (attr >> 4) + 0x10, 0);
}

static tilemap_callback( airbustr1 )
{
	INT32 attr = DrvVidRAM1[offs + 0x400];

	TILE_SET_INFO(0, DrvVidRAM1[offs] + ((attr & 0x0f) << 8), (attr >> 4), 0);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	// Notes:
	// On fresh boot: game sets up banking and loads up the interrupt vectors.
	// Then its the watchdog's turn to come in and reboot the game into game-mode.

	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
//	bankswitch(0,0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
//	bankswitch(1,0);
	ZetReset();
	ZetClose();

	ZetOpen(2);
//	bankswitch(2,0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	MSM6295Reset();

	BurnRandomSetSeed(0x92462313);

	BurnWatchdogReset();
	BurnWatchdogRead(); // Turn ON the Watchdog.

	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x020000;
	DrvZ80ROM2		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvZ80RAM2		= Next; Next += 0x002000;

	DrvShareRAM		= Next; Next += 0x001000;
	DrvDevRAM		= Next; Next += 0x001000;

	DrvPandoraRAM	= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x001000;

	DrvScrollRegs	= Next; Next += 0x000008;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	sound_status	= Next; Next += 0x000001;
	sound_status2	= Next; Next += 0x000001;

	coin_lockout	= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;

	bankdata		= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]  = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x080000; i++) {
		tmp[i] = (DrvGfxROM0[i] << 4) | (DrvGfxROM0[i] >> 4);
	}

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	is_bootleg = BurnDrvGetFlags() & BDF_BOOTLEG;

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x000000,  2, 1)) return 1;

		if (is_bootleg)
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000001,  4, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040000,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040001,  6, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x020000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x060000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000, 11, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000, 12, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x020000, 13, 1)) return 1;
		}
		else
		{
		//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  3, 2)) return 1;.

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000,  6, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000,  7, 1)) return 1;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xcfff, MAP_ROM); // handler
	ZetMapMemory(DrvZ80RAM0,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvDevRAM,			0xe000, 0xefff, (is_bootleg) ? MAP_RAM : (MAP_WRITE|MAP_FETCH));
	ZetMapMemory(DrvShareRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(airbustr_main_write);
	ZetSetReadHandler(airbustr_main_read);
	ZetSetOutHandler(airbustr_main_out);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetOutHandler(airbustr_sub_out);
	ZetSetInHandler(airbustr_sub_in);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0xc000, 0xdfff, MAP_RAM);
	ZetSetOutHandler(airbustr_sound_out);
	ZetSetInHandler(airbustr_sound_in);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2203Init(1, 3000000, NULL, 0);
	BurnYM2203SetPorts(0, &DrvYM2203PortA, &DrvYM2203PortB, NULL, NULL);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);
	BurnTimerAttach(&ZetConfig, 6000000);

	MSM6295Init(0, 3000000 / 132, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, airbustr0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, airbustr1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, 0x100000, 0, 0x1f);
	GenericTilemapSetTransparent(1, 0);

	pandora_init(DrvPandoraRAM, DrvGfxROM1, (0x400000/0x100)-1, 0x200, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	pandora_exit();

	GenericTilesExit();

	ZetExit();
	MSM6295Exit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static inline void DrvRecalcPalette()
{
	for (INT32 i = 0; i < 0x600; i+=2)
	{
		UINT16 d = DrvPalRAM[i + 1] * 256 + DrvPalRAM[i + 0];

		UINT8 r = (d >>  5) & 0x1f;
		UINT8 g = (d >> 10) & 0x1f;
		UINT8 b = (d >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i / 2] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_layer(INT32 layer, INT32 r0, INT32 r1, INT32 r2, INT32 r3)
{
	INT32 scrollx = DrvScrollRegs[r0] + ((~DrvScrollRegs[4] << r1) & 0x100);
	INT32 scrolly = DrvScrollRegs[r2] + ((~DrvScrollRegs[4] << r3) & 0x100) + 16;

	if (*flipscreen) {
		scrollx = (scrollx + 0x06a) & 0x1ff;
		scrolly = (scrolly + 0x1ff) & 0x1ff;
	} else {
		scrollx = (scrollx - 0x094) & 0x1ff;
		scrolly = (scrolly - 0x100) & 0x1ff;
	}

	GenericTilemapSetFlip(layer, (*flipscreen) ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollX(layer, scrollx);
	GenericTilemapSetScrollY(layer, scrolly);
	GenericTilemapDraw(layer, pTransDraw, 0);
}

static INT32 DrvDraw()
{
	DrvRecalcPalette();

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(0, 3, 6, 2, 5);
	if (nBurnLayer & 2) draw_layer(1, 1, 8, 0, 7);

	pandora_flipscreen = *flipscreen;
	if (nBurnLayer & 4) pandora_update(pTransDraw);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	if (is_bootleg == 0) {
		BurnWatchdogUpdate();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { (INT32)((double)6000000 / 57.40), (INT32)((double)6000000 / 57.40), (INT32)((double)6000000 / 57.40) };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) {
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 64) {
			ZetSetVector(0xfd);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 240) {
			ZetSetVector(0xfd);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(2);
		CPU_RUN_TIMER(2);
		if (i == 240) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}

			pandora_buffer_sprites();
		}
		ZetClose();
	}

	ZetOpen(2);
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		BurnRandomScan(nAction);

		BurnWatchdogScan(nAction);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		for (INT32 i = 0; i < 3; i++)
		{
			ZetOpen(i);
			bankswitch(i, bankdata[i]);
			ZetClose();
		}
	}

	return 0;
}


// Air Buster: Trouble Specialty Raid Unit (World)

static struct BurnRomInfo airbustrRomDesc[] = {
	{ "pr12.h19",			0x20000, 0x91362eb2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "pr13.l15",			0x20000, 0x13b2257b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "pr-21.bin",			0x20000, 0x6e0a5df0, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "i80c51",				0x01000, 0x00000000, 4 | BRF_NODUMP },	      //  3 80c51 MCU

	{ "pr-000.bin",			0x80000, 0x8ca68f0d, 5 | BRF_GRA }, 	      //  4 Tiles

	{ "pr-001.bin",			0x80000, 0x7e6cb377, 6 | BRF_GRA }, 	      //  5 Sprites
	{ "pr-02.bin",			0x10000, 0x6bbd5e46, 6 | BRF_GRA }, 	      //  6

	{ "pr-200.bin",			0x40000, 0xa4dd3390, 7 | BRF_SND }, 	      //  7 OKI M6295
	
	{ "pal16l8-pr-500.16f",	0x00104, 0xe0f901e1, 0 | BRF_OPT }, 	      //  8 Plds
	{ "gal16v8a-pr-501.13j",0x00117, 0xd8bf8cb5, 0 | BRF_OPT }, 	      //  
};

STD_ROM_PICK(airbustr)
STD_ROM_FN(airbustr)

struct BurnDriver BurnDrvAirbustr = {
	"airbustr", NULL, NULL, NULL, "1990",
	"Air Buster: Trouble Specialty Raid Unit (World)\0", NULL, "Kaneko (Namco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_MISC, GBF_HORSHOOT, 0,
	NULL, airbustrRomInfo, airbustrRomName, NULL, NULL, NULL, NULL, AirbustrInputInfo, AirbustrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x0300,
	256, 224, 4, 3
};


// Air Buster: Trouble Specialty Raid Unit (Japan)

static struct BurnRomInfo airbustrjRomDesc[] = {
	{ "pr-14j.bin",			0x20000, 0x6b9805bd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "pr-11j.bin",			0x20000, 0x85464124, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "pr-21.bin",			0x20000, 0x6e0a5df0, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "i80c51",				0x01000, 0x00000000, 4 | BRF_NODUMP },	      //  3 80c51 MCU

	{ "pr-000.bin",			0x80000, 0x8ca68f0d, 5 | BRF_GRA }, 	      //  4 Tiles

	{ "pr-001.bin",			0x80000, 0x7e6cb377, 6 | BRF_GRA }, 	      //  5 Sprites
	{ "pr-02.bin",			0x10000, 0x6bbd5e46, 6 | BRF_GRA }, 	      //  6

	{ "pr-200.bin",			0x40000, 0xa4dd3390, 7 | BRF_SND }, 	      //  7 OKI M6295
	
	{ "pal16l8-pr-500.16f",	0x00104, 0xe0f901e1, 0 | BRF_OPT }, 	      //  8 Plds
	{ "gal16v8a-pr-501.13j",0x00117, 0xd8bf8cb5, 0 | BRF_OPT }, 	      //
};

STD_ROM_PICK(airbustrj)
STD_ROM_FN(airbustrj)

struct BurnDriver BurnDrvAirbustrj = {
	"airbustrj", "airbustr", NULL, NULL, "1990",
	"Air Buster: Trouble Specialty Raid Unit (Japan)\0", NULL, "Kaneko (Namco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_MISC, GBF_HORSHOOT, 0,
	NULL, airbustrjRomInfo, airbustrjRomName, NULL, NULL, NULL, NULL, AirbustrInputInfo, AirbustjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x0300,
	256, 224, 4, 3
};


// Air Buster: Trouble Specialty Raid Unit (bootleg)

static struct BurnRomInfo airbustrbRomDesc[] = {
	{ "5.bin",		0x20000, 0x9e4216a2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1.bin",		0x20000, 0x85464124, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "2.bin",		0x20000, 0x6e0a5df0, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "7.bin",		0x20000, 0x2e3bf0a2, 4 | BRF_GRA }, 	      //  3 Tiles
	{ "9.bin",		0x20000, 0x2c23c646, 4 | BRF_GRA }, 	      //  4
	{ "6.bin",		0x20000, 0x0d6cd470, 4 | BRF_GRA }, 	      //  5
	{ "8.bin",		0x20000, 0xb3372e51, 4 | BRF_GRA }, 	      //  6

	{ "13.bin",		0x20000, 0x75dee86d, 5 | BRF_GRA }, 	      //  7 Sprites
	{ "12.bin",		0x20000, 0xc98a8333, 5 | BRF_GRA }, 	      //  8
	{ "11.bin",		0x20000, 0x4e9baebd, 5 | BRF_GRA }, 	      //  9
	{ "10.bin",		0x20000, 0x63dc8cd8, 5 | BRF_GRA }, 	      // 10
	{ "14.bin",		0x10000, 0x6bbd5e46, 5 | BRF_GRA }, 	      // 11

	{ "4.bin",		0x20000, 0x21d9bfe3, 6 | BRF_SND }, 	      // 12 OKI M6295
	{ "3.bin",		0x20000, 0x58cd19e2, 6 | BRF_SND }, 	      // 13
};

STD_ROM_PICK(airbustrb)
STD_ROM_FN(airbustrb)

struct BurnDriver BurnDrvAirbustrb = {
	"airbustrb", "airbustr", NULL, NULL, "1990",
	"Air Buster: Trouble Specialty Raid Unit (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_MISC, GBF_HORSHOOT, 0,
	NULL, airbustrbRomInfo, airbustrbRomName, NULL, NULL, NULL, NULL, AirbustrInputInfo, AirbustjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x0300,
	256, 224, 4, 3
};
