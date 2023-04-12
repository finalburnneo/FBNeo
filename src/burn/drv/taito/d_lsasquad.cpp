// FinalBurn Neo Taito Land Sea Air Squad / Storming Party driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "burn_pal.h"
#include "z80_intf.h"
#include "taito_m68705.h"
#include "burn_ym2203.h"

static UINT8 *AllRam;
static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvScrRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvMCURAM;

static INT32 nmi_enable;
static INT32 nmi_pending;
static INT32 soundlatch[2];
static INT32 bank_data;
static INT32 flipscreen;

static INT32 daikaiju = 0;
static INT32 storming = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo LsasquadInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 4,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Lsasquad)

static struct BurnDIPInfo LsasquadDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x7f, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x01, 0x00, "Upright"				},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode "		},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x08, 0x00, "Off"					},
	{0x00, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x03, 0x02, "Easy"					},
	{0x01, 0x01, 0x03, 0x03, "Medium"				},
	{0x01, 0x01, 0x03, 0x01, "Hard"					},
	{0x01, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x0c, 0x08, "50000 100000"			},
	{0x01, 0x01, 0x0c, 0x0c, "80000 150000"			},
	{0x01, 0x01, 0x0c, 0x04, "100000 200000"		},
	{0x01, 0x01, 0x0c, 0x00, "150000 300000"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x00, "1"					},
	{0x01, 0x01, 0x30, 0x30, "3"					},
	{0x01, 0x01, 0x30, 0x10, "4"					},
	{0x01, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x40, 0x00, "No"					},
	{0x01, 0x01, 0x40, 0x40, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Language"				},
	{0x01, 0x01, 0x80, 0x00, "English"				},
	{0x01, 0x01, 0x80, 0x80, "Japanese"				},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x02, 0x01, 0x01, 0x01, "Off"					},
	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x02, 0x01, 0x02, 0x02, "Off"					},
	{0x02, 0x01, 0x02, 0x00, "On"					},
	
	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},
};

STDDIPINFO(Lsasquad)

static struct BurnDIPInfo DaikaijuDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xfd, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x01, 0x00, "Upright"				},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x02, 0x00, "Off"					},
	{0x00, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x08, 0x00, "Off"					},
	{0x00, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x03, 0x01, "Easy"					},
	{0x01, 0x01, 0x03, 0x03, "Medium"				},
	{0x01, 0x01, 0x03, 0x02, "Hard"					},
	{0x01, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x0c, 0x08, "100000"				},
	{0x01, 0x01, 0x0c, 0x0c, "30000"				},
	{0x01, 0x01, 0x0c, 0x04, "50000"				},
	{0x01, 0x01, 0x0c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x00, "Infinite (Cheat)"		},
	{0x01, 0x01, 0x30, 0x30, "3"					},
	{0x01, 0x01, 0x30, 0x10, "5"					},
	{0x01, 0x01, 0x30, 0x20, "4"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Daikaiju)

static INT32 lsasquad_mcu_status()
{
	INT32 ret = DrvInputs[4] ^ 0xff; // mcu

	if (!main_sent)	ret |= 0x01;
	if (!mcu_sent)	ret |= 0x02;

	if (daikaiju) {
		ret |= ((soundlatch[0] & 0x100) ? 0 : 0x10); // daikaiju
	}

	return ret;
}

static void sync_sub()
{
	INT32 todo = (ZetTotalCycles(0) * 3 / 6) - ZetTotalCycles(1);
	if (todo > 0) {
		ZetSwapActive(1);
		BurnTimerUpdate(ZetTotalCycles() + todo);
		ZetSwapActive(0);
	}
}

static void bankswitch(INT32 data)
{
	flipscreen = data & 0x10;
	bank_data  = data;

	ZetMapMemory(DrvZ80ROM[0] + 0x10000 + ((data & 7) * 0x2000), 0x8000, 0x9fff, MAP_ROM);
}

static void __fastcall lsasquad_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xea00:
			// 0x20, 0x80 probably cpu enable flags
			if (~data & 0x40 && !storming) { // reset MCU (recover from tilt depends on this)
				m67805_taito_reset();
			}
			bankswitch(data);
		return;

		case 0xec00:
			sync_sub();
			soundlatch[0] = data | 0x100;
			if (nmi_enable) {
				ZetNmi(1);
				nmi_pending = 0;
			} else {
				nmi_pending = 1;
			}
		return;

		case 0xee00:
			if (!storming) {
				standard_taito_mcu_write(data);
			}
		return;
	}
}

static UINT8 __fastcall lsasquad_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
			return DrvDips[0];

		case 0xe801:
			return DrvDips[1];

		case 0xe802:
			return DrvDips[2];

		case 0xe803:
			if (storming) return DrvInputs[4]; // coins
			return lsasquad_mcu_status();

		case 0xe804:
			return DrvInputs[0];

		case 0xe805:
			return DrvInputs[1];

		case 0xe806:
			return DrvInputs[2];

		case 0xe807:
			return DrvInputs[3];

		case 0xec00:
			soundlatch[1] &= 0xff;
			return soundlatch[1];

		case 0xec01:
			if (daikaiju) return ((soundlatch[0] & 0x100) ? 2 : 1);
			return ((soundlatch[0] & 0x100) ? 1 : 0) | ((soundlatch[1] & 0x100) ? 2 : 0);

		case 0xee00:
			return (storming) ? 0x00 : standard_taito_mcu_read();
	}

	return 0;
}

static void __fastcall lsasquad_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xc000:
		case 0xc001:
			BurnYM2203Write((address - 0xa000) / 0x2000, address & 1, data);
		return;

		case 0xd000:
			soundlatch[1] = data | 0x100;
		return;

		case 0xd400:
		case 0xd800:
			nmi_enable = address & 0x800;
			if (nmi_enable && nmi_pending) {
				ZetNmi();
				nmi_pending = 0;
			}
		return;
	}
}

static UINT8 __fastcall lsasquad_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM2203Read(0, address & 1);

		case 0xd000:
			soundlatch[0] &= 0xff;
			return soundlatch[0];

		case 0xd800:
			if (daikaiju) return ((soundlatch[0] & 0x100) ? 2 : 1);
			return ((soundlatch[0] & 0x100) ? 1 : 0) | ((soundlatch[1] & 0x100) ? 2 : 0);
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	m67805_taito_reset();

	soundlatch[0] = 0;
	soundlatch[1] = 0;
	bank_data = 0;
	flipscreen = 0;
	nmi_enable = 0;
	nmi_pending = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x020000;
	DrvZ80ROM[1]		= Next; Next += 0x008000;

	DrvMCUROM			= Next; Next += 0x001000;

	DrvGfxROM[0]		= Next; Next += 0x040000;
	DrvGfxROM[1]		= Next; Next += 0x040000;

	DrvColPROM			= Next; Next += 0x001000;

	BurnPalette			= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x002000;
	DrvZ80RAM[1]		= Next; Next += 0x000800;

	DrvVidRAM			= Next; Next += 0x002000;
	DrvScrRAM			= Next; Next += 0x000400;
	DrvSprRAM			= Next; Next += 0x000400;

	DrvMCURAM			= Next; Next += 0x000080;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { 0x10000*8+0, 0x10000*8+4, 0, 4 };
	INT32 XOffs[16] = { STEP4(3,-1), STEP4(8+3,-1), STEP4(128+3,-1), STEP4(136+3,-1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x20000);

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x20000);

	GfxDecode(0x0400, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM[1]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	if (game_select == 1) storming = 1;
	if (game_select == 2) daikaiju = 1;

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x18000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (!storming)
			if (BurnLoadRom(DrvMCUROM   + 0x00000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x08000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x10000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x18000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1] + 0x00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x08000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x10000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x18000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00c00, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],				0xa000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,					0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,					0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,					0xe400, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(lsasquad_main_write);
	ZetSetReadHandler(lsasquad_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],				0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(lsasquad_sound_write);
	ZetSetReadHandler(lsasquad_sound_read);
	ZetClose();

	m67805_taito_init(DrvMCUROM, DrvMCURAM, &standard_m68705_interface);

	BurnYM2203Init(2, 3000000, &DrvYM2203IRQHandler, 0);
//	BurnYM2203SetPorts(0, NULL, NULL, &DrvYM2203WritePortA, &DrvYM2203WritePortB); // unknown writes (filter?)
	BurnTimerAttachZet(3000000);
	BurnYM2203SetAllRoutes(0, 0.65, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.06);
	BurnYM2203SetAllRoutes(1, 0.06, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x40000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x40000, 0x100, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	m67805_taito_exit();
	BurnYM2203Exit();

	BurnFreeMemIndex();

	storming = 0;
	daikaiju = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x400 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x400 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x400 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x400 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x800 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x800 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x800 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x800 + i] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer(UINT8 *scrollram)
{
	INT32 scrollx =  scrollram[3];
	INT32 scrolly = -scrollram[0];

	for (INT32 x = 0; x < 32; x++)
	{
		INT32 base = scrollram[x * 4 + 1] * 64;
		INT32 sx = (x * 8 + scrollx);

		if (flipscreen) sx = 248 - sx;

		sx &= 0xff;

		for (INT32 y = 0; y < 32; y++)
		{
			INT32 sy = (y * 8 + scrolly) & 0xff;

			if (flipscreen) sy = 248 - sy;

			INT32 attr  = DrvVidRAM[(base + y * 2 + 1) & 0x1fff];
			INT32 code  = DrvVidRAM[(base + y * 2 + 0) & 0x1fff] + (attr << 8);
			INT32 color = attr >> 4;

			DrawGfxMaskTile(0, 0, code, sx, sy - 16, flipscreen, flipscreen, color, 0xf);
			if (sx > 248) DrawGfxMaskTile(0, 0, code, sx - 256, sy - 16, flipscreen, flipscreen, color, 0xf); // wrap
		}
	}
}

static INT32 draw_layer_daikaiju(INT32 offs, INT32 *previd, INT32 type)
{
	INT32 initoffs = offs;
	INT32 globalscrollx = 0;

	INT32 id = DrvScrRAM[offs + 2];

	for( ; offs < 0x400; offs += 4)
	{
		if (id != DrvScrRAM[offs + 2])
		{
			*previd = id;
			return offs;
		}
		else
		{
			id = DrvScrRAM[offs + 2];
		}

		if ((DrvScrRAM[offs + 0] | DrvScrRAM[offs + 1] | DrvScrRAM[offs + 2] | DrvScrRAM[offs + 3]) == 0)
			continue;

		INT32 scrolly = -DrvScrRAM[offs + 0];
		INT32 scrollx =  DrvScrRAM[offs + 3];

		if (*previd != 1)
		{
			if (offs != initoffs)
			{
				scrollx += globalscrollx;
			}
			else
			{
				globalscrollx = scrollx;
			}
		}

		INT32 base = 64 * DrvScrRAM[offs + 1];
		INT32 sx = scrollx;

		if (flipscreen)
			sx = 248 - sx;
		sx &= 0xff;

		for (INT32 y = 0; y < 32; y++)
		{
			INT32 sy = 8 * y + scrolly;
			if (flipscreen)
				sy = 248 - sy;
			sy &= 0xff;

			INT32 attr = DrvVidRAM[base + 2 * y + 1];
			INT32 code = DrvVidRAM[base + 2 * y + 0] + (attr << 8);
			INT32 color = attr >> 4;

			if ((type == 0 && color != 0x0d) || (type != 0 && color == 0x0d))
			{
				DrawGfxMaskTile(0, 0, code, sx, sy - 16, flipscreen, flipscreen, color, 0xf);
				if (sx > 248) DrawGfxMaskTile(0, 0, code, sx - 256, sy - 16, flipscreen, flipscreen, color, 0xf); // wrap
			}
		}
	}

	return offs;
}

static void drawbg(INT32 type)
{
	INT32 i = 0;
	INT32 id = -1;

	while (i < 0x400)
	{
		if (~DrvScrRAM[i + 2] & 1)
		{
			i = draw_layer_daikaiju(i, &id, type);
		}
		else
		{
			id = DrvScrRAM[i + 2];
			i += 4;
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x400 - 4; offs >= 0; offs -= 4)
	{
		INT32 sy 	= 240 - DrvSprRAM[offs + 0];
		INT32 attr	= DrvSprRAM[offs + 1];
		INT32 code	= DrvSprRAM[offs + 2] + ((attr & 0x30) << 4);
		INT32 sx	= DrvSprRAM[offs + 3];
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		DrawGfxMaskTile(0, 1, code, sx, sy - 16, flipx, flipy, color, 0xf);
		DrawGfxMaskTile(0, 1, code, sx - 256, sy - 16, flipx, flipy, color, 0xf);
	}
}

static INT32 LsasquadDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear(0x1ff);

	if (nBurnLayer & 1) draw_layer(DrvScrRAM + 0x000);
	if (nBurnLayer & 2) draw_layer(DrvScrRAM + 0x080);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) draw_layer(DrvScrRAM + 0x100);

	BurnTransferFlip(flipscreen, flipscreen);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 daikaijuDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear(0x1ff);

	if (nBurnLayer & 1) drawbg(0);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) drawbg(1);

	BurnTransferFlip(flipscreen, flipscreen);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256*2;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {

		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 239*2) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		ZetClose();

		if (!storming) { // no mcu
			CPU_RUN(2, m6805);
		}
	}

	m6805Close();

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029672;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		m68705_taito_scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(nmi_pending);
		SCAN_VAR(soundlatch);
		SCAN_VAR(bank_data);
		SCAN_VAR(flipscreen);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bank_data);
		ZetClose();
	}

	return 0;
}


// Land Sea Air Squad / Riku Kai Kuu Saizensen

static struct BurnRomInfo lsasquadRomDesc[] = {
	{ "a64-21.4",		0x8000, 0x5ff6b017, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a64-20.3",		0x8000, 0x7f8b4979, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a64-19.2",		0x8000, 0xba31d34a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a64-04.44",		0x8000, 0xc238406a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a64-05.35",		0x0800, 0x572677b9, 3 | BRF_GRA },           //  4 M68705 Code

	{ "a64-10.27",		0x8000, 0xbb4f1b37, 4 | BRF_GRA },           //  5 Layer Tiles
	{ "a64-22.28",		0x8000, 0x58e03b89, 4 | BRF_GRA },           //  6
	{ "a64-11.40",		0x8000, 0xa3bbc0b3, 4 | BRF_GRA },           //  7
	{ "a64-23.41",		0x8000, 0x377a538b, 4 | BRF_GRA },           //  8

	{ "a64-14.2",		0x8000, 0xa72e2041, 5 | BRF_GRA },           //  9 Sprites
	{ "a64-16.3",		0x8000, 0x05206333, 5 | BRF_GRA },           // 10
	{ "a64-15.25",		0x8000, 0x01ed5851, 5 | BRF_GRA },           // 11
	{ "a64-17.26",		0x8000, 0x6eaf3735, 5 | BRF_GRA },           // 12

	{ "a64-07.22",		0x0400, 0x82802bbb, 6 | BRF_GRA },           // 13 Color PROM
	{ "a64-08.23",		0x0400, 0xaa9e1dbd, 6 | BRF_GRA },           // 14
	{ "a64-09.24",		0x0400, 0xdca86295, 6 | BRF_GRA },           // 15

	{ "a64-06.9",		0x0400, 0x7ced30ba, 7 | BRF_GRA },           // 16 Priority PROM

	{ "pal16l8a.14",	0x0104, 0xa7cc157d, 8 | BRF_OPT },           // 17 PLDs
};

STD_ROM_PICK(lsasquad)
STD_ROM_FN(lsasquad)

static INT32 LsasquadInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvLsasquad = {
	"lsasquad", NULL, NULL, NULL, "1986",
	"Land Sea Air Squad / Riku Kai Kuu Saizensen\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, lsasquadRomInfo, lsasquadRomName, NULL, NULL, NULL, NULL, LsasquadInputInfo, LsasquadDIPInfo,
	LsasquadInit, DrvExit, DrvFrame, LsasquadDraw, DrvScan, &BurnRecalc, 0x200,
	224, 256, 3, 4
};


// Storming Party / Riku Kai Kuu Saizensen (set 1)

static struct BurnRomInfo stormingRomDesc[] = {
	{ "stpartyj.001",	0x8000, 0x07e6bc61, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "stpartyj.002",	0x8000, 0x1c7fe5d5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "stpartyj.003",	0x8000, 0x159f23a6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a64-04.44",		0x8000, 0xc238406a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a64-10.27",		0x8000, 0xbb4f1b37, 3 | BRF_GRA },           //  4 Layer Tiles
	{ "stpartyj.009",	0x8000, 0x8ee2443b, 3 | BRF_GRA },           //  5
	{ "a64-11.40",		0x8000, 0xa3bbc0b3, 3 | BRF_GRA },           //  6
	{ "stpartyj.011",	0x8000, 0xf342d42f, 3 | BRF_GRA },           //  7

	{ "a64-14.2",		0x8000, 0xa72e2041, 4 | BRF_GRA },           //  8 Sprites
	{ "a64-16.3",		0x8000, 0x05206333, 4 | BRF_GRA },           //  9
	{ "a64-15.25",		0x8000, 0x01ed5851, 4 | BRF_GRA },           // 10
	{ "a64-17.26",		0x8000, 0x6eaf3735, 4 | BRF_GRA },           // 11

	{ "a64-07.22",		0x0400, 0x82802bbb, 5 | BRF_GRA },           // 12 Color PROM
	{ "a64-08.23",		0x0400, 0xaa9e1dbd, 5 | BRF_GRA },           // 13
	{ "a64-09.24",		0x0400, 0xdca86295, 5 | BRF_GRA },           // 14

	{ "a64-06.9",		0x0400, 0x7ced30ba, 6 | BRF_GRA },           // 15 Priority PROM

	{ "pal16l8a.14",	0x0104, 0xa7cc157d, 7 | BRF_OPT },           // 16 PLDs
};

STD_ROM_PICK(storming)
STD_ROM_FN(storming)

static INT32 StormingInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvStorming = {
	"storming", "lsasquad", NULL, NULL, "1986",
	"Storming Party / Riku Kai Kuu Saizensen (set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, stormingRomInfo, stormingRomName, NULL, NULL, NULL, NULL, LsasquadInputInfo, LsasquadDIPInfo,
	StormingInit, DrvExit, DrvFrame, LsasquadDraw, DrvScan, &BurnRecalc, 0x200,
	224, 256, 3, 4
};


// Storming Party / Riku Kai Kuu Saizensen (set 2)

static struct BurnRomInfo stormingaRomDesc[] = {
	{ "1.ic4.1e",		0x8000, 0x07e6bc61, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.ic3.1c",		0x8000, 0x1c7fe5d5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.ic2.1b",		0x8000, 0x159f23a6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.ic44.5g",		0x8000, 0xc238406a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "5.ic29.2c",		0x8000, 0xbb4f1b37, 3 | BRF_GRA },           //  4 Layer Tiles
	{ "7.ic30.3c",		0x8000, 0x8ee2443b, 3 | BRF_GRA },           //  5
	{ "6.ic42.2d",		0x8000, 0xa3bbc0b3, 3 | BRF_GRA },           //  6
	{ "8.ic43.3d",		0x8000, 0xf342d42f, 3 | BRF_GRA },           //  7

	{ "9.ic2.14a",		0x8000, 0xa72e2041, 4 | BRF_GRA },           //  8 Sprites
	{ "a.ic3.16a",		0x8000, 0x05206333, 4 | BRF_GRA },           //  9
	{ "10.ic27.14b",	0x8000, 0x01ed5851, 4 | BRF_GRA },           // 10
	{ "b.ic28.16b",		0x8000, 0x6eaf3735, 4 | BRF_GRA },           // 11

	{ "ic22.2l",		0x0400, 0x82802bbb, 5 | BRF_GRA },           // 12 Color PROM
	{ "ic23.2m",		0x0400, 0xaa9e1dbd, 5 | BRF_GRA },           // 13
	{ "ic24.2m",		0x0400, 0xdca86295, 5 | BRF_GRA },           // 14

	{ "ic9.1j",			0x0400, 0x7ced30ba, 6 | BRF_GRA },           // 15 Priority PROM

	{ "pal16l8cj.ic14.2c",	0x0104, 0xa7cc157d, 7 | BRF_OPT },           // 16 PLDs
};

STD_ROM_PICK(storminga)
STD_ROM_FN(storminga)

struct BurnDriver BurnDrvStorminga = {
	"storminga", "lsasquad", NULL, NULL, "1986",
	"Storming Party / Riku Kai Kuu Saizensen (set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, stormingaRomInfo, stormingaRomName, NULL, NULL, NULL, NULL, LsasquadInputInfo, LsasquadDIPInfo,
	StormingInit, DrvExit, DrvFrame, LsasquadDraw, DrvScan, &BurnRecalc, 0x200,
	224, 256, 3, 4
};


// Daikaiju no Gyakushu

static struct BurnRomInfo daikaijuRomDesc[] = {
	{ "a74_01-1.ic4",	0x8000, 0x89c13d7f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a74_02.ic3",		0x8000, 0x8ddf6131, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a74_03.ic2",		0x8000, 0x3911ffed, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a74_04.ic44",	0x8000, 0x98a6a703, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a74_05.ic35",	0x0800, 0xd66df06f, 3 | BRF_GRA },           //  4 M68705 Code

	{ "a74_10.ic27",	0x8000, 0x3123158e, 4 | BRF_GRA },           //  5 Layer Tiles
	{ "a74_12.ic28",	0x8000, 0x8a4e6c3a, 4 | BRF_GRA },           //  6
	{ "a74_11.ic40",	0x8000, 0x6432ae38, 4 | BRF_GRA },           //  7
	{ "a74_13.ic41",	0x8000, 0x1a1be4bb, 4 | BRF_GRA },           //  8

	{ "a74_14.ic2",		0x8000, 0xc28e9c35, 5 | BRF_GRA },           //  9 Sprites
	{ "a74_16.ic3",		0x8000, 0x4b1c7921, 5 | BRF_GRA },           // 10
	{ "a74_16.ic25",	0x8000, 0xef4d1945, 5 | BRF_GRA },           // 11
	{ "a74_17.ic26",	0x8000, 0xd1077878, 5 | BRF_GRA },           // 12

	{ "a74_07.ic22",	0x0400, 0x66132341, 6 | BRF_GRA },           // 13 Color PROM
	{ "a74_08.ic23",	0x0400, 0xfb3f0273, 6 | BRF_GRA },           // 14
	{ "a74_09.ic24",	0x0400, 0xbed6709d, 6 | BRF_GRA },           // 15

	{ "a74_06.ic9",		0x0400, 0xcad554e7, 7 | BRF_GRA },           // 16 Priority PROM
};

STD_ROM_PICK(daikaiju)
STD_ROM_FN(daikaiju)

static INT32 DaikaijuInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvDaikaiju = {
	"daikaiju", NULL, NULL, NULL, "1986",
	"Daikaiju no Gyakushu\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, daikaijuRomInfo, daikaijuRomName, NULL, NULL, NULL, NULL, LsasquadInputInfo, DaikaijuDIPInfo,
	DaikaijuInit, DrvExit, DrvFrame, daikaijuDraw, DrvScan, &BurnRecalc, 0x200,
	224, 256, 3, 4
};
