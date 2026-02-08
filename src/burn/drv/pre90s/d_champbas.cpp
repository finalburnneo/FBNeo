// FB Neo Champion Baseball driver module
// Based on MAME driver by Ernesto Corvi, Jarek Parchanski, Nicola Salmoria, hap

// note:
//	the watchdog timer isn't quite right, causes talbot to crash without the hack at a0a0

#include "tiles_generic.h"
#include "z80_intf.h"
#include "alpha8201.h"
#include "ay8910.h"
#include "dac.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvTransTab;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM[2];
static UINT8 *DrvProtRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 irq_enable;
static INT32 gfx_bank;
static INT32 flipscreen;
static INT32 palette_bank;
static INT32 watchdog;
static INT32 soundlatch;
static INT32 irqtimer;

static INT32 invert_ay8910 = 1;
static INT32 has_mcu = 0;
static INT32 has_soundcpu = 0;
static INT32 nCyclesExtra[3];
static INT32 game_select = 0; // 0 = talbot, 1 = champxx, 2 = exctsccr

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo TalbotInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 2"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Talbot)

static struct BurnInputInfo ChampbasInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Champbas)

static struct BurnDIPInfo TalbotDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x33, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x00, "3"					},
	{0x00, 0x01, 0x0c, 0x04, "4"					},
	{0x00, 0x01, 0x0c, 0x08, "5"					},
	{0x00, 0x01, 0x0c, 0x0c, "6"					},

	{0   , 0xfe, 0   ,    4, "Rabbits to Capture"	},
	{0x00, 0x01, 0x30, 0x00, "4"					},
	{0x00, 0x01, 0x30, 0x10, "5"					},
	{0x00, 0x01, 0x30, 0x20, "6"					},
	{0x00, 0x01, 0x30, 0x30, "8"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x40, 0x00, "Upright"				},
	{0x00, 0x01, 0x40, 0x40, "Cocktail"				},
};

STDDIPINFO(Talbot)

static struct BurnDIPInfo ChampbasDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x26, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x03, 0x03, "A 2/1 B 3/2"			},
	{0x00, 0x01, 0x03, 0x02, "A 1/1 B 2/1"			},
	{0x00, 0x01, 0x03, 0x01, "A 1/2 B 1/6"			},
	{0x00, 0x01, 0x03, 0x00, "A 1/3 B 1/6"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x10, 0x00, "Upright"				},
	{0x00, 0x01, 0x10, 0x10, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x00, 0x01, 0x20, 0x20, "Easy"					},
	{0x00, 0x01, 0x20, 0x00, "Hard"					},
};

STDDIPINFO(Champbas)

static struct BurnDIPInfo ExctsccrDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x13, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "A 1C/1C B 3C/1C"		},
	{0x00, 0x01, 0x03, 0x01, "A 1C/2C B 1C/4C"		},
	{0x00, 0x01, 0x03, 0x00, "A 1C/3C B 1C/6C"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x08, 0x00, "Upright"				},
	{0x00, 0x01, 0x08, 0x08, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x00, 0x01, 0x10, 0x10, "Easy"					},
	{0x00, 0x01, 0x10, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x00, 0x01, 0x60, 0x20, "1 Min."				},
	{0x00, 0x01, 0x60, 0x00, "2 Min."				},
	{0x00, 0x01, 0x60, 0x60, "3 Min."				},
	{0x00, 0x01, 0x60, 0x40, "4 Min."				},
};

STDDIPINFO(Exctsccr)

static void mcu_sync()
{
	INT32 cyc = (INT64)ZetTotalCycles(0) * (384000 / 4) / 3072000;
	cyc -= Alpha8201TotalCycles();

	if (cyc > 0) {
		Alpha8201Run(cyc);
	}
}

static void __fastcall champbas_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xa040) { // exctsccr
		DrvSprRAM[1][address & 0xf] = data;
		return;
	}

	if ((address & 0xfff0) == 0xa060) {
		DrvSprRAM[0][address & 0xf] = data;
		return;
	}

	if (address >= 0x6000 && address <= 0x63ff) {
		if (has_mcu) {
			mcu_sync();
			Alpha8201WriteRam(address & 0x3ff, data);
		} else {
			DrvProtRAM[address & 0x3ff] = data;
		}
	}

	if ((address & 0xf000) == 0x7000) address &= 0xf001; // ay8910 mirrors

	switch (address)
	{
		case 0x7000:
		case 0x7001:
			AY8910Write(0, (address ^ invert_ay8910) & 1, data);
		return;

		case 0xa000:
			irq_enable = data & 1;
			if (irq_enable == 0)
				ZetSetIRQLine(CPU_IRQLINE0, CPU_IRQSTATUS_NONE);
		return;

		case 0xa002:
			gfx_bank = data & 1;
		return;

		case 0xa003:
			flipscreen = ~data & 1;
		return;

		case 0xa004:
			palette_bank = data & 1;
		return;

		case 0xa006:
			if (has_mcu) {
				mcu_sync();
				Alpha8201Start(data & 1);
			}
		return;

		case 0xa007:
			if (has_mcu) {
				mcu_sync();
				Alpha8201SetBusDir(data & 1);
			}
		return;

		case 0xa001:
		case 0xa005:
		return;

		case 0xa080:
			soundlatch = data;
		return;

		case 0xa0c0:
			watchdog = 0;
			BurnWatchdogWrite();
		return;
	}

//	bprintf (0, _T("MWB: %4.4x, %2.2x\n"), address, data);
}

static UINT8 __fastcall champbas_main_read(UINT16 address)
{
	if (address >= 0x6000 && address <= 0x63ff) {
		if (has_mcu) {
			mcu_sync();
			return Alpha8201ReadRam(address & 0x3ff);
		} else {
			return DrvProtRAM[address & 0x3ff];
		}
	}

	switch (address)
	{
		case 0xa000:
			return DrvInputs[1]; // p1

		case 0xa040:
			return DrvInputs[2]; // p2

		case 0xa080:
		case 0xa0a0:
		{
			UINT8 ret = DrvDips[0] & 0x7f;
			if (game_select == 0)
				ret |= (ZetReadByte(0x8c00) & 0x04) << 5;	// talbot hack! (protection)
			else
				ret |= (((0x10 - watchdog) >> 2) << 7);
			return ret;
		}

		case 0xa0c0:
			return DrvInputs[0]; // system
	}

//	bprintf (0, _T("MRB: %4.4x\n"), address);

	return 0;
}

static void __fastcall champbas_sound_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x1fff)
	{
		case 0x8000:
		return; // nop

		case 0xa000:
			soundlatch = 0;
		return;

		case 0xc000:
			DACSignedWrite(0, data << 2);
		return;
	}
}

static UINT8 __fastcall champbas_sound_read(UINT16 address)
{
	switch (address & ~0x1fff)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall exctsccr_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc00c:
			soundlatch = 0;
		return;

		case 0xc008:
		case 0xc009:
			DACSignedWrite(address & 1, data << 2);
		return;
	}
}

static UINT8 __fastcall exctsccr_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc00d:
			return soundlatch;
	}

	return 0;
}

static void __fastcall exctsccr_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x82:
		case 0x83:
		case 0x86:
		case 0x87:
		case 0x8a:
		case 0x8b:
		case 0x8e:
		case 0x8f:
			AY8910Write((port & 0xc) >> 2, ~port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 code = DrvVidRAM[offs] | (gfx_bank << 8);
	INT32 color = (DrvVidRAM[offs + 0x400] & 0x1f) | 0x20 | (palette_bank << 6);

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( Exctsccrbg )
{
	INT32 code = DrvVidRAM[offs] | (gfx_bank << 8);
	INT32 color = DrvVidRAM[offs + 0x400] & 0x0f;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	DACReset();
	ZetClose();

	if (has_mcu) Alpha8201Reset();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);
	AY8910Reset(3);

	BurnWatchdogReset();

	memset (nCyclesExtra, 0, sizeof(nCyclesExtra));

	irq_enable = 0;
	gfx_bank = 0;
	flipscreen = 0;
	palette_bank = 0;
	watchdog = 0;
	soundlatch = 0;
	irqtimer = -256*60; // startup delay for sound irq

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]	= Next; Next += 0x008000;
	DrvZ80ROM[1]	= Next; Next += 0x009000;
	DrvMCUROM		= Next; Next += 0x002000;

	DrvGfxROM[0]	= Next; Next += 0x008000;
	DrvGfxROM[1]	= Next; Next += 0x008000;
	DrvGfxROM[2]	= Next; Next += 0x008000;

	DrvTransTab		= Next; Next += 0x000200;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM[0]	= Next; Next += 0x000c00;
	DrvZ80RAM[1]	= Next; Next += 0x000800;
	DrvProtRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM[0]	= Next; Next += 0x000010;
	DrvSprRAM[1]	= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 4 };
	INT32 XOffs0[8] = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[16]= { STEP4(8*8,1), STEP4(16*8,1), STEP4(24*8,1), STEP4(0,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs0, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x2000);

	GfxDecode(0x0080, 2, 16, 16, Plane, XOffs1, YOffs, 0x200, tmp, DrvGfxROM[1]);

	BurnFree(tmp);

	return 0;
}

static INT32 ExctsccrGfxDecode()
{
	INT32 Plane0[3]  = { 0x2000*8+4, 0, 4 };
	INT32 Plane1[3]  = { 0x2000*8+4, 0, 4 };
	INT32 Plane2[4]  = { 0x1000*8+0, 0x1000*8+4, 0, 4 };
	INT32 XOffs0[8]  = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[16] = { STEP4(8*8,1), STEP4(16*8,1), STEP4(24*8,1), STEP4(0,1) };
	INT32 YOffs[16]  = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x4000);

	GfxDecode(0x0200, 3,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x4000);

	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x2000);

	GfxDecode(0x0040, 4, 16, 16, Plane2, XOffs1, YOffs, 0x200, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static void ChampbasGfxReorder()
{
	for (INT32 i = 0; i < 0x1000; i++)
	{
		UINT8 t = DrvGfxROM[0][i + 0x1000];
		DrvGfxROM[0][i + 0x1000] = DrvGfxROM[1][i];
		DrvGfxROM[1][i] = t;
	}
}

static void ExctsccrGfxReorder()
{
	for (INT32 i = 0; i < 0x1000; i++)
	{
		DrvGfxROM[1][i + 0x3000] = DrvGfxROM[0][i + 0x3000] >> 4;
		DrvGfxROM[1][i + 0x2000] = DrvGfxROM[0][i + 0x3000] & 0x0f;
	}

	for (INT32 i = 0; i < 0x1000; i++)
	{
		DrvGfxROM[0][i + 0x3000] = DrvGfxROM[0][i + 0x2000] >> 4;
		DrvGfxROM[0][i + 0x2000] &= 0x0f;
	}
}

static INT32 DrvLoadROMs()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[8] = { 0, DrvZ80ROM[0], DrvZ80ROM[1], DrvMCUROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 nType = ri.nType & 7;

		if (nType)
		{
			if (nType == 1 && ri.nLen == 0x800) pLoad[nType] += 0x1800; // champbb2 / champbb2j
			if (nType == 2) has_soundcpu = 1;
			if (nType == 3) has_mcu = 1;
			if (BurnLoadRom(pLoad[nType], i, 1)) return 1;
			pLoad[nType] += ri.nLen;
			continue;
		}
	}

	switch (pLoad[4] - DrvGfxROM[0])
	{
		case 0x1000: game_select = 0; DrvGfxDecode(); break; // talbot
		case 0x2000: game_select = 1; ChampbasGfxReorder(); DrvGfxDecode(); break; // champbas/champbb2
		case 0x4000: game_select = 2; ChampbasGfxReorder(); ExctsccrGfxReorder(); ExctsccrGfxDecode(); break; // exctsccr
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],			0x0000, 0x5fff, MAP_ROM);
	//ZetMapMemory(DrvProtRAM,			0x6000, 0x63ff, MAP_RAM); conditional - see handler!
	ZetMapMemory(DrvZ80ROM[0] + 0x6800,	0x6800, 0x6fff, MAP_ROM); // tbaseball / champbjsa
	ZetMapMemory(DrvZ80ROM[0] + 0x7800,	0x7800, 0x7fff, MAP_ROM); // tbaseball / champbb2
	if (game_select == 2)
		ZetMapMemory(DrvZ80RAM[0] + 0x0800, 0x7c00, 0x7fff, MAP_RAM); // exctsccr
	ZetMapMemory(DrvVidRAM,				0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],			0x8800, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(champbas_main_write);
	ZetSetReadHandler(champbas_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	if (game_select == 2)
	{
		ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x8fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM[1],		0xa000, 0xa7ff, MAP_RAM);
		ZetSetWriteHandler(exctsccr_sound_write);
		ZetSetReadHandler(exctsccr_sound_read);
		ZetSetOutHandler(exctsccr_sound_write_port);
	}
	else
	{
		ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x5fff, MAP_ROM);
		for (INT32 i = 0; i < 0x2000; i+= 0x400) {
			ZetMapMemory(DrvZ80RAM[1],		0xe000 + i, 0xe3ff + i, MAP_RAM);
		}
		ZetSetWriteHandler(champbas_sound_write);
		ZetSetReadHandler(champbas_sound_read);
	}
	ZetClose();

	if (has_mcu) {
		Alpha8201Init(DrvMCUROM);
	}

	BurnWatchdogInit(DrvDoReset, 16);

	const INT32 audiocpu_speed = (game_select == 2) ? 3579545 : 3072000;

	AY8910Init(0, (game_select == 2) ? 1940000 : 1536000, 0); // exctsccr's first ay runs a bit faster
	AY8910Init(1, 1789772, 1);
	AY8910Init(2, 1789772, 1);
	AY8910Init(3, 1789772, 1);
	AY8910SetAllRoutes(0, 0.08, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(3, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, audiocpu_speed);

	DACInit(0, 0, 1, ZetTotalCycles, audiocpu_speed);
	DACInit(1, 0, 1, ZetTotalCycles, audiocpu_speed);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.65, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	static const INT32 gfxconfig[3][5] = { { 2, 0x100, 0x07f, 0x000, 0x03f }, { 2, 0x000, 0x07f, 0x000, 0x07f }, { 3, 0x000, 0x00f, 0x080, 0x00f } };

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, game_select == 2 ? Exctsccrbg_map_callback : bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], gfxconfig[game_select][0],  8,  8, 0x8000, gfxconfig[game_select][1], gfxconfig[game_select][2]);
	GenericTilemapSetGfx(1, DrvGfxROM[1], gfxconfig[game_select][0], 16, 16, 0x8000, gfxconfig[game_select][3], gfxconfig[game_select][4]);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x4000, 0x100, 0x0f); // exctsccr only
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	if (has_mcu) {
		Alpha8201Exit();
	}
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	AY8910Exit(3);
	DACExit();

	BurnFreeMemIndex();

	invert_ay8910 = 1;
	has_mcu = 0;
	has_soundcpu = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit2 * 1000 + bit1 * 470 + bit0 * 220;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit2 * 1000 + bit1 * 470 + bit0 * 220;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit1 * 470 + bit0 * 220;

		pal[i] = BurnHighCol((r * 255) / 1690, (g * 255) / 1690, (b * 255) / 690, 0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 col = DrvColPROM[0x20 + (i & 0xff)] & 0xf;
		DrvTransTab[i] = col;
		DrvPalette[i] = pal[col | ((i & 0x100) >> 4)];
	}
}

static void ExctsccrPaletteInit()
{
	UINT32 pal[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (int i = 0; i < 0x100; i++)
	{
		INT32 j = ((i & 0xf8) >> 1) | ((i & 0x04) << 5) | (i & 3);
		DrvPalette[i + 0x000] = pal[(DrvColPROM[0x020 + j] & 0x0f) | ((i & 0x80) >> 3)];
		DrvPalette[i + 0x100] = pal[(DrvColPROM[0x120 + i] & 0x0f) | 0x10];
		DrvTransTab[i + 0x000] = (i & 0x07);
		DrvTransTab[i + 0x100] = DrvColPROM[0x120 + i] & 0x0f;
	}
}

static void draw_sprites(UINT8 *ram0, UINT8 *ram1, INT32 gfxnum)
{
	GenericTilesGfx *gfx = &GenericGfxData[gfxnum];

	for (INT32 offs = 0x10-2; offs >= 0; offs -= 2)
	{
		INT32 sx 	= ram0[offs + 1] - 16;
		INT32 sy 	= ram0[offs + 0] ^ 0xff;
		INT32 attr  = ram1[offs + 0] ^ 3;
		INT32 attr1	= ram1[offs + 1];

		INT32 code  =(attr >> 2) & 0x3f;
		INT32 color =(((attr1 & 0x1f) | (palette_bank << 6)) & gfx->color_mask) << gfx->depth;
		INT32 flipx = attr & 1;
		INT32 flipy = attr & 2;
		if (gfxnum == 1 && game_select == 2) code |= ((attr1 & 0x10) << 2);
		else code |= (gfx_bank << 6);
		code %= gfx->code_mask;

		RenderTileTranstabOffset(pTransDraw, gfx->gfxbase, code, color, 0, sx,       sy - 16, flipx, flipy, 16, 16, DrvTransTab + gfx->color_offset, gfx->color_offset);
		RenderTileTranstabOffset(pTransDraw, gfx->gfxbase, code, color, 0, sx + 256, sy - 16, flipx, flipy, 16, 16, DrvTransTab + gfx->color_offset, gfx->color_offset);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	if (~nBurnLayer & 1) BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM[0], DrvZ80RAM[0] + 0x7f0, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 ExctsccrDraw()
{
	if (DrvRecalc) {
		ExctsccrPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	if (~nBurnLayer & 1) BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM[0], DrvVidRAM    + 0x000, 1);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM[1], DrvZ80RAM[0] + 0x000, 2);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	if (has_mcu) Alpha8201NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 3072000 / 60, 3072000 / 60, 384000 / 4 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };
	if (game_select == 2) {
		nCyclesTotal[1] = 3579545 / 60;
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (irq_enable != 0 && i == 239) ZetSetIRQLine(CPU_IRQLINE0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		if (has_soundcpu) CPU_RUN(1, Zet);
		if (game_select == 2) {
			if ((irqtimer > -1) && (i&0x3) == 0x3) ZetNmi();
			if (irqtimer == 204) {
				ZetSetIRQLine(CPU_IRQLINE0, CPU_IRQSTATUS_HOLD);
				irqtimer = 0;
			}
			irqtimer++;
		}
		ZetClose();

		if (has_mcu) CPU_RUN_SYNCINT(2, Alpha8201);
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	watchdog++; // here!

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);

		if (has_mcu) {
			Alpha8201Scan(nAction, pnMin);
		}

		BurnWatchdogScan(nAction);
		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(irq_enable);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(palette_bank);
		SCAN_VAR(watchdog);
		SCAN_VAR(soundlatch);
		SCAN_VAR(irqtimer);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Talbot (World)

static struct BurnRomInfo talbotRomDesc[] = {
	{ "11.10g",							0x1000, 0x0368607d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "12.11g",							0x1000, 0x400e633b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13.10h",							0x1000, 0xbe575d9e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "14.11h",							0x1000, 0x56464614, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "15.10i",							0x1000, 0x0225b7ef, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "16.11i",							0x1000, 0x1612adf5, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha-8201_44801a75_2f25.bin",	0x2000, 0xb77931ac, 3 | BRF_PRG | BRF_ESS }, //  6 Alpha 8xxx MCU Code

	{ "7.6a",							0x1000, 0xbde14194, 4 | BRF_GRA },           //  7 Background Tiles

	{ "8.6b",							0x1000, 0xddcd227a, 5 | BRF_GRA },           //  8 Sprites

	{ "mb7051.7h",						0x0020, 0x7a153c60, 7 | BRF_GRA },           //  9 Color PROMs
	{ "mb7052.5e",						0x0100, 0xa3189986, 7 | BRF_GRA },           // 10
};

STD_ROM_PICK(talbot)
STD_ROM_FN(talbot)

struct BurnDriver BurnDrvTalbot = {
	"talbot", NULL, NULL, NULL, "1982",
	"Talbot (World)\0", NULL, "Alpha Denshi Co. (Volt Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, talbotRomInfo, talbotRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, TalbotDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Champion Base Ball (World)

static struct BurnRomInfo champbasRomDesc[] = {
	{ "champbb.1",						0x2000, 0x218de21e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "champbb.2",						0x2000, 0x5ddd872e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "champbb.3",						0x2000, 0xf39a7046, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "champbb.6",						0x2000, 0x26ab3e16, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "champbb.7",						0x2000, 0x7c01715f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "champbb.8",						0x2000, 0x3c911786, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "champbb.4",						0x2000, 0x1930fb52, 4 | BRF_GRA },           //  6 Background Tiles

	{ "champbb.5",						0x2000, 0xa4cef5a1, 5 | BRF_GRA },           //  7 Sprites

	{ "champbb.pr2",					0x0020, 0x2585ffb0, 7 | BRF_GRA },           //  8 Color PROMs
	{ "champbb.pr1",					0x0100, 0x872dd450, 7 | BRF_GRA },           //  9
};

STD_ROM_PICK(champbas)
STD_ROM_FN(champbas)

struct BurnDriver BurnDrvChampbas = {
	"champbas", NULL, NULL, NULL, "1983",
	"Champion Base Ball (World)\0", NULL, "Alpha Denshi Co. (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbasRomInfo, champbasRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Champion Base Ball (Japan, set 1)

static struct BurnRomInfo champbasjRomDesc[] = {
	{ "11.2e",							0x2000, 0xe2dfc166, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "12.2g",							0x2000, 0x7b4e5faa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13.2h",							0x2000, 0xb201e31f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "16.2k",							0x2000, 0x24c482ee, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "17.2l",							0x2000, 0xf10b148b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "18.2n",							0x2000, 0x2dc484dd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha-8201_44801a75_2f25.bin",	0x2000, 0xb77931ac, 3 | BRF_PRG | BRF_ESS }, //  6 Alpha 8xxx MCU Code

	{ "14.5e",							0x2000, 0x1b8202b3, 4 | BRF_GRA },           //  7 Background Tiles

	{ "15.5g",							0x2000, 0xa67c0c40, 5 | BRF_GRA },           //  8 Sprites

	{ "1e.bpr",							0x0020, 0xf5ce825e, 7 | BRF_GRA },           //  9 Color PROMs
	{ "5k.bpr",							0x0100, 0x2e481ffa, 7 | BRF_GRA },           // 10
};

STD_ROM_PICK(champbasj)
STD_ROM_FN(champbasj)

struct BurnDriver BurnDrvChampbasj = {
	"champbasj", "champbas", NULL, NULL, "1983",
	"Champion Base Ball (Japan, set 1)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbasjRomInfo, champbasjRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Champion Base Ball (Japan, set 2)

static struct BurnRomInfo champbasjaRomDesc[] = {
	{ "10",								0x2000, 0xf7cdaf8e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "09",								0x2000, 0x9d39e5b3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "08",								0x2000, 0x53468a0f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "16.2k",							0x2000, 0x24c482ee, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "17.2l",							0x2000, 0xf10b148b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "18.2n",							0x2000, 0x2dc484dd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "14.5e",							0x2000, 0x1b8202b3, 4 | BRF_GRA },           //  6 Background Tiles

	{ "15.5g",							0x2000, 0xa67c0c40, 5 | BRF_GRA },           //  7 Sprites

	{ "clr",							0x0020, 0x8f989357, 7 | BRF_GRA },           //  8 Color PROMs
	{ "5k.bpr",							0x0100, 0x2e481ffa, 7 | BRF_GRA },           //  9
};

STD_ROM_PICK(champbasja)
STD_ROM_FN(champbasja)

static void ChampbasjaCallback()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 data = 0;

		if (i & 0x01) data |= 0x80;
		if (i & 0x40) data |= 0x19;

		DrvZ80ROM[0][0x6800 + i] = data;
	}
}

static INT32 ChampbasjaInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0) {
		ChampbasjaCallback();
	}

	return nRet;
}

struct BurnDriver BurnDrvChampbasja = {
	"champbasja", "champbas", NULL, NULL, "1983",
	"Champion Base Ball (Japan, set 2)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbasjaRomInfo, champbasjaRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	ChampbasjaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Champion Base Ball (Japan, set 3)

static struct BurnRomInfo champbasjbRomDesc[] = {
	{ "1.2e",							0x2000, 0x4dcf2e03, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.2g",							0x2000, 0xccbd0eff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.2h",							0x2000, 0x4c7f1de4, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.2k",							0x2000, 0x24c482ee, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "7.2l",							0x2000, 0xf10b148b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8.2n",							0x2000, 0x2dc484dd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "4.5e",							0x2000, 0x1930fb52, 4 | BRF_GRA },           //  6 Background Tiles

	{ "5.5g",							0x2000, 0xa67c0c40, 5 | BRF_GRA },           //  7 Sprites

	{ "1e.bpr",							0x0020, 0xf5ce825e, 7 | BRF_GRA },           //  8 Color PROMs
	{ "5k.bpr",							0x0100, 0x2e481ffa, 7 | BRF_GRA },           //  9
};

STD_ROM_PICK(champbasjb)
STD_ROM_FN(champbasjb)

struct BurnDriver BurnDrvChampbasjb = {
	"champbasjb", "champbas", NULL, NULL, "1983",
	"Champion Base Ball (Japan, set 3)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbasjbRomInfo, champbasjbRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Champion Base Ball Part-2 (World, set 1)

static struct BurnRomInfo champbb2RomDesc[] = {
	{ "epr5932",						0x2000, 0x528e3c78, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr5929",						0x2000, 0x17b6057e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr5930",						0x2000, 0xb6570a90, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr5931",						0x0800, 0x0592434d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr5933",						0x2000, 0x26ab3e16, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "epr5934",						0x2000, 0x7c01715f, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "epr5935",						0x2000, 0x3c911786, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  7 Alpha 8xxx MCU Code

	{ "epr5936",						0x2000, 0xc4a4df75, 4 | BRF_GRA },           //  8 Background Tiles

	{ "epr5937",						0x2000, 0x5c80ec42, 5 | BRF_GRA },           //  9 Sprites

	{ "pr5957",							0x0020, 0xf5ce825e, 7 | BRF_GRA },           // 10 Color PROMs
	{ "pr5956",							0x0100, 0x872dd450, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(champbb2)
STD_ROM_FN(champbb2)

struct BurnDriver BurnDrvChampbb2 = {
	"champbb2", NULL, NULL, NULL, "1983",
	"Champion Base Ball Part-2 (World, set 1)\0", NULL, "Alpha Denshi Co. (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbb2RomInfo, champbb2RomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Taikyoku Base Ball (Japan)

static struct BurnRomInfo tbasebalRomDesc[] = {
	{ "1.2e.2764",						0x2000, 0x9b75b44d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2_p9.2g.2764",					0x2000, 0x736a1b62, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.2h.2764",						0x2000, 0xcf5f28cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic2.2764",						0x2000, 0xaacb9647, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.2k.2764",						0x2000, 0x24c482ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "7.2l.2764",						0x2000, 0xf10b148b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "8.2n.2764",						0x2000, 0x2dc484dd, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic3_mc68705p3_rom.bin",			0x0800, 0x6b477f5f, 3 | BRF_PRG | BRF_ESS }, //  7 M68705 MCU Code

	{ "4.5e.2764",						0x2000, 0xc4a4df75, 4 | BRF_GRA },           //  8 Background Tiles

	{ "5.5g.2764",						0x2000, 0x5c80ec42, 5 | BRF_GRA },           //  9 Sprites

	{ "1e.bpr",							0x0020, 0xf5ce825e, 7 | BRF_GRA },           // 10 Color PROMs
	{ "5k.82s129",						0x0100, 0x2e481ffa, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(tbasebal)
STD_ROM_FN(tbasebal)

struct BurnDriverD BurnDrvTbasebal = {
	"tbasebal", "champbb2", NULL, NULL, "1983",
	"Taikyoku Base Ball (Japan)\0", "Not working", "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tbasebalRomInfo, tbasebalRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Champion Base Ball Part-2 (Japan)

static struct BurnRomInfo champbb2jRomDesc[] = {
	{ "1.10g",							0x2000, 0xc76056d5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.10h",							0x2000, 0x7a1ea3ea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.10i",							0x2000, 0x4b2f6ac4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "0.11g",							0x0800, 0xbe0e180d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.15c",							0x2000, 0x24c482ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "7.15d",							0x2000, 0xf10b148b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "8.15e",							0x2000, 0x2dc484dd, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  7 Alpha 8xxx MCU Code

	{ "4.6a",							0x2000, 0xc4a4df75, 4 | BRF_GRA },           //  8 Background Tiles

	{ "5.6b",							0x2000, 0x5c80ec42, 5 | BRF_GRA },           //  9 Sprites

	{ "bpr.7h",							0x0020, 0x500db3d9, 7 | BRF_GRA },           // 10 Color PROMs
	{ "bpr.5e",							0x0100, 0x2e481ffa, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(champbb2j)
STD_ROM_FN(champbb2j)

static INT32 Champbb2jInit()
{
	invert_ay8910 = 0;

	return DrvInit();
}

struct BurnDriver BurnDrvChampbb2j = {
	"champbb2j", "champbb2", NULL, NULL, "1983",
	"Champion Base Ball Part-2 (Japan)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, champbb2jRomInfo, champbb2jRomName, NULL, NULL, NULL, NULL, ChampbasInputInfo, ChampbasDIPInfo,
	Champbb2jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Exciting Soccer (World)

static struct BurnRomInfo exctsccrRomDesc[] = {
	{ "1_g10.bin",						0x2000, 0xaa68df66, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2_h10.bin",						0x2000, 0x2d8f8326, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_j10.bin",						0x2000, 0xdce4a04d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "0_h6.bin",						0x2000, 0x3babbd6b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "9_f6.bin",						0x2000, 0x639998f5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8_d6.bin",						0x2000, 0x88651ee1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7_c6.bin",						0x2000, 0x6d51521e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1_a6.bin",						0x1000, 0x20f2207e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha 8xxx MCU Code

	{ "4_a5.bin",						0x2000, 0xc342229b, 4 | BRF_GRA },           //  9 Background Tiles
	{ "6_c5.bin",						0x2000, 0xeda40e32, 4 | BRF_GRA },           // 10

	{ "5_b5.bin",						0x2000, 0x35f4f8c9, 5 | BRF_GRA },           // 11 Sprites (3bpp)

	{ "2.5k",							0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 12 Sprites (4bpp)
	{ "3.5l",							0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 13

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 14 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 15
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(exctsccr)
STD_ROM_FN(exctsccr)

struct BurnDriver BurnDrvExctsccr = {
	"exctsccr", NULL, NULL, NULL, "1983",
	"Exciting Soccer (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccrRomInfo, exctsccrRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer (alternate music)

static struct BurnRomInfo exctsccraRomDesc[] = {
	{ "1_g10.bin",						0x2000, 0xaa68df66, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2_h10.bin",						0x2000, 0x2d8f8326, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_j10.bin",						0x2000, 0xdce4a04d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "exctsccc.000",					0x2000, 0x642fc42f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "exctsccc.009",					0x2000, 0xd88b3236, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8_d6.bin",						0x2000, 0x88651ee1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7_c6.bin",						0x2000, 0x6d51521e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1_a6.bin",						0x1000, 0x20f2207e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha 8xxx MCU Code

	{ "4_a5.bin",						0x2000, 0xc342229b, 4 | BRF_GRA },           //  9 Background Tiles
	{ "6_c5.bin",						0x2000, 0xeda40e32, 4 | BRF_GRA },           // 10

	{ "5_b5.bin",						0x2000, 0x35f4f8c9, 5 | BRF_GRA },           // 11 Sprites (3bpp)

	{ "2.5k",							0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 12 Sprites (4bpp)
	{ "3.5l",							0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 13

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 14 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 15
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(exctsccra)
STD_ROM_FN(exctsccra)

struct BurnDriver BurnDrvExctsccra = {
	"exctsccra", "exctsccr", NULL, NULL, "1983",
	"Exciting Soccer (alternate music)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccraRomInfo, exctsccraRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer (US)

static struct BurnRomInfo exctsccruRomDesc[] = {
	{ "vr1u.g10",						0x2000, 0xef39676d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "vr2u.h10",						0x2000, 0x37994b86, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vr3u.j10",						0x2000, 0x2ed3c6bb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "vr0u.6h",						0x2000, 0xcbb035c6, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "9_f6.bin",						0x2000, 0x639998f5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8_d6.bin",						0x2000, 0x88651ee1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7_c6.bin",						0x2000, 0x6d51521e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1_a6.bin",						0x1000, 0x20f2207e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha 8xxx MCU Code

	{ "vr4u.a5",						0x2000, 0x103bb739, 4 | BRF_GRA },           //  9 Background Tiles
	{ "vr6u.c5",						0x2000, 0xa5b2b303, 4 | BRF_GRA },           // 10

	{ "5_b5.bin",						0x2000, 0x35f4f8c9, 5 | BRF_GRA },           // 11 Sprites (3bpp)

	{ "2.5k",							0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 12 Sprites (4bpp)
	{ "3.5l",							0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 13

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 14 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 15
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(exctsccru)
STD_ROM_FN(exctsccru)

struct BurnDriver BurnDrvExctsccru = {
	"exctsccru", "exctsccr", NULL, NULL, "1983",
	"Exciting Soccer (US)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccruRomInfo, exctsccruRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer (Japan)

static struct BurnRomInfo exctsccrjRomDesc[] = {
	{ "1_10g.bin",						0x2000, 0x310298a2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2_10h.bin",						0x2000, 0x030fd0b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_10j.bin",						0x2000, 0x1a51ff1f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "0_h6.bin",						0x2000, 0x3babbd6b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "9_f6.bin",						0x2000, 0x639998f5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8_d6.bin",						0x2000, 0x88651ee1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7_c6.bin",						0x2000, 0x6d51521e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1_a6.bin",						0x1000, 0x20f2207e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha 8xxx MCU Code

	{ "4_5a.bin",						0x2000, 0x74cc71d6, 4 | BRF_GRA },           //  9 Background Tiles
	{ "6_5c.bin",						0x2000, 0x7c4cd1b6, 4 | BRF_GRA },           // 10

	{ "5_5b.bin",						0x2000, 0x35f4f8c9, 5 | BRF_GRA },           // 11 Sprites (3bpp)

	{ "2.5k",							0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 12 Sprites (4bpp)
	{ "3.5l",							0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 13

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 14 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 15
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(exctsccrj)
STD_ROM_FN(exctsccrj)

struct BurnDriver BurnDrvExctsccrj = {
	"exctsccrj", "exctsccr", NULL, NULL, "1983",
	"Exciting Soccer (Japan)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccrjRomInfo, exctsccrjRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer (Japan, older)

static struct BurnRomInfo exctsccrjoRomDesc[] = {
	{ "1.10g",							0x2000, 0xd1bfdf75, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.10h",							0x2000, 0x5c61f0fe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.10j",							0x2000, 0x8f213b10, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "0.6h",							0x2000, 0x548b08a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "9.f6",							0x2000, 0x639998f5, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "8.6d",							0x2000, 0xb6b209a5, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "7.6c",							0x2000, 0x8856452a, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha-8302_44801b35.bin",		0x2000, 0xedabac6c, 3 | BRF_PRG | BRF_ESS }, //  7 Alpha 8xxx MCU Code

	{ "4.5a",							0x2000, 0xc4259307, 4 | BRF_GRA },           //  8 Background Tiles
	{ "6.5c",							0x2000, 0xcca53367, 4 | BRF_GRA },           //  9

	{ "5.5b",							0x2000, 0x851d1a18, 5 | BRF_GRA },           // 10 Sprites (3bpp)

	{ "2.5k",							0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 11 Sprites (4bpp)
	{ "3.5l",							0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 12

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 13 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 14
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(exctsccrjo)
STD_ROM_FN(exctsccrjo)

struct BurnDriver BurnDrvExctsccrjo = {
	"exctsccrjo", "exctsccr", NULL, NULL, "1983",
	"Exciting Soccer (Japan, older)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccrjoRomInfo, exctsccrjoRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer (bootleg)

static struct BurnRomInfo exctsccrbRomDesc[] = {
	{ "es-1.e2",						0x2000, 0x997c6a82, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "es-2.g2",						0x2000, 0x5c66e792, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "es-3.h2",						0x2000, 0xe0d504c0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "es-a.k2",						0x2000, 0x99e87b78, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "es-b.l2",						0x2000, 0x8b3db794, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "es-c.m2",						0x2000, 0x7bed2f81, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha-8201_44801a75_2f25.bin",	0x2000, 0xb77931ac, 3 | BRF_PRG | BRF_ESS }, //  6 Alpha 8xxx MCU Code

	{ "4_a5.bin",						0x2000, 0xc342229b, 4 | BRF_GRA },           //  7 Background Tiles
	{ "6_c5.bin",						0x2000, 0xeda40e32, 4 | BRF_GRA },           //  8

	{ "5_b5.bin",						0x2000, 0x35f4f8c9, 5 | BRF_GRA },           //  9 Sprites (3bpp)

	{ "2_k5.bin",						0x1000, 0x7f9cace2, 6 | BRF_GRA },           // 10 Sprites (4bpp)
	{ "3_l5.bin",						0x1000, 0xdb2d9e0d, 6 | BRF_GRA },           // 11

	{ "prom1.e1",						0x0020, 0xd9b10bf0, 7 | BRF_GRA },           // 12 Color PROMs
	{ "prom3.k5",						0x0100, 0xb5db1c2c, 7 | BRF_GRA },           // 13
	{ "prom2.8r",						0x0100, 0x8a9c0edf, 7 | BRF_GRA },           // 14
};

STD_ROM_PICK(exctsccrb)
STD_ROM_FN(exctsccrb)

struct BurnDriver BurnDrvExctsccrb = {
	"exctsccrb", "exctsccr", NULL, NULL, "1983",
	"Exciting Soccer (bootleg)\0", NULL, "bootleg (Kazutomi)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctsccrbRomInfo, exctsccrbRomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Exciting Soccer II (World)

static struct BurnRomInfo exctscc2RomDesc[] = {
	{ "eprom_1_vr_b_alpha_denshi.3j",	0x2000, 0xc6115362, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "eprom_2_vr_alpha_denshi.3k",		0x2000, 0xde36ba00, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "eprom_3_vr_v_alpha_denshi.3l",	0x2000, 0x1ddfdf65, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "eprom_0_vr_alpha_denshi.7d",		0x2000, 0x2c675a43, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "eprom_9_vr_alpha_denshi.7e",		0x2000, 0xe571873d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "eprom_8_vr_alpha_denshi.7f",		0x2000, 0x88651ee1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "eprom_7_vr_alpha_denshi.7h",		0x2000, 0x6d51521e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "eprom_1_vr_alpha_denshi.7k",		0x1000, 0x20f2207e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha-8303_44801b42.1d",			0x2000, 0x66adcb37, 3 | BRF_PRG | BRF_ESS }, //  8 Alpha 8xxx MCU Code

	{ "eprom_4_vr_alpha_denshi.5a",		0x2000, 0x4ff1783d, 4 | BRF_GRA },           //  9 Background Tiles
	{ "eprom_6_vr_alpha_denshi.5c",		0x2000, 0x1fb84ee6, 4 | BRF_GRA },           // 10

	{ "eprom_5_vr_alpha_denshi.5b",		0x2000, 0x5605b60b, 5 | BRF_GRA },           // 11 Sprites (3bpp)

	{ "eprom_7_vr_alpha_denshi.5k",		0x1000, 0x1d37edfa, 6 | BRF_GRA },           // 12 Sprites (4bpp)
	{ "eprom_8_vr_alpha_denshi.5l",		0x1000, 0xb97f396c, 6 | BRF_GRA },           // 13

	{ "tbp18s030.5j",					0x0020, 0x899d153d, 7 | BRF_GRA },           // 14 Color PROMs
	{ "tbp24s10.61d",					0x0100, 0x75613784, 7 | BRF_GRA },           // 15
	{ "tbp24s10.60h",					0x0100, 0x1a52d6eb, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(exctscc2)
STD_ROM_FN(exctscc2)

struct BurnDriver BurnDrvExctscc2 = {
	"exctscc2", NULL, NULL, NULL, "1984",
	"Exciting Soccer II (World)\0", NULL, "Alpha Denshi Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, exctscc2RomInfo, exctscc2RomName, NULL, NULL, NULL, NULL, TalbotInputInfo, ExctsccrDIPInfo,
	DrvInit, DrvExit, DrvFrame, ExctsccrDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
