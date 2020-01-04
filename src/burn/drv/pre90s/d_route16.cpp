// FB Alpha Route 16 driver module
// Based on MAME driver by Zsolt Vasvari

// Todo: revise route16 protection

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "dac.h"
#include "sn76477.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 palette_1;
static UINT8 palette_2;
static UINT8 flipscreen;
static INT32 speakres_vrx;
static UINT8 ttmahjng_port_select;
static INT32 protection_data;

static INT32 program_size;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvJoy8[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo Route16InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Route16)

static struct BurnInputInfo StratvoxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Stratvox)

static struct BurnInputInfo TtmahjngInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy8 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"A",			BIT_DIGITAL,	DrvJoy1 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy2 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy3 + 0,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy4 + 0,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy1 + 1,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy2 + 1,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy3 + 1,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy4 + 1,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy1 + 2,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy2 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy1 + 4,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy4 + 2,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy1 + 3,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy2 + 3,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy3 + 3,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy1 + 4,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy3 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy2 + 4,	"mah reach"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"A",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy8 + 0,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy8 + 1,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy6 + 4,	"mah reach"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ttmahjng)

static struct BurnDIPInfo Route16DIPList[]=
{
	{0x0f, 0xff, 0xff, 0xa0, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0f, 0x01, 0x01, 0x00, "3"			},
	{0x0f, 0x01, 0x01, 0x01, "5"			},

	{0   , 0xfe, 0   ,    3, "Coinage"		},
	{0x0f, 0x01, 0x18, 0x08, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x18, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x18, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"			},
	{0x0f, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x80, 0x00, "Off"			},
	{0x0f, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Route16)

static struct BurnDIPInfo StratvoxDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x20, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0b, 0x01, 0x01, 0x00, "3"			},
	{0x0b, 0x01, 0x01, 0x01, "5"			},

	{0   , 0xfe, 0   ,    2, "Replenish Astronouts"	},
	{0x0b, 0x01, 0x02, 0x00, "No"			},
	{0x0b, 0x01, 0x02, 0x02, "Yes"			},

	{0   , 0xfe, 0   ,    4, "2 Attackers At Wave"	},
	{0x0b, 0x01, 0x0c, 0x00, "2"			},
	{0x0b, 0x01, 0x0c, 0x04, "3"			},
	{0x0b, 0x01, 0x0c, 0x08, "4"			},
	{0x0b, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Astronauts Kidnapped"	},
	{0x0b, 0x01, 0x10, 0x00, "Less Often"		},
	{0x0b, 0x01, 0x10, 0x10, "More Often"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x20, 0x20, "Upright"		},
	{0x0b, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0b, 0x01, 0x40, 0x00, "Off"			},
	{0x0b, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Voices"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Stratvox)

static struct BurnDIPInfo SpeakresDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x20, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0b, 0x01, 0x03, 0x00, "3"			},
	{0x0b, 0x01, 0x03, 0x01, "4"			},
	{0x0b, 0x01, 0x03, 0x02, "5"			},
	{0x0b, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "2 Attackers At Wave"	},
	{0x0b, 0x01, 0x0c, 0x00, "2"			},
	{0x0b, 0x01, 0x0c, 0x04, "3"			},
	{0x0b, 0x01, 0x0c, 0x08, "4"			},
	{0x0b, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0b, 0x01, 0x10, 0x00, "5000"			},
	{0x0b, 0x01, 0x10, 0x10, "8000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x20, 0x20, "Upright"		},
	{0x0b, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0b, 0x01, 0x40, 0x00, "Off"			},
	{0x0b, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Voices"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Speakres)

static struct BurnDIPInfo SpacechoDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x20, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0b, 0x01, 0x01, 0x00, "3"			},
	{0x0b, 0x01, 0x01, 0x01, "5"			},

	{0   , 0xfe, 0   ,    2, "Replenish Astronouts"	},
	{0x0b, 0x01, 0x02, 0x02, "No"			},
	{0x0b, 0x01, 0x02, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    4, "2 Attackers At Wave"	},
	{0x0b, 0x01, 0x0c, 0x00, "2"			},
	{0x0b, 0x01, 0x0c, 0x04, "3"			},
	{0x0b, 0x01, 0x0c, 0x08, "4"			},
	{0x0b, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Astronauts Kidnapped"	},
	{0x0b, 0x01, 0x10, 0x00, "Less Often"		},
	{0x0b, 0x01, 0x10, 0x10, "More Often"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x20, 0x20, "Upright"		},
	{0x0b, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0b, 0x01, 0x40, 0x00, "Off"			},
	{0x0b, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Voices"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Spacecho)

static void __fastcall route16_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x4000) {
		DrvShareRAM[address & 0x3ff] = data;

		if (address >= 0x4313 && address <= 0x4319 && data == 0xff) {
			ZetRunEnd();
		}

		return;
	}

	switch (address)
	{
		case 0x3000:
		case 0x3001:
			//bprintf (0, _T("prot w: %4.4x %2.2x, (%4.4x)\n"), address, data, ZetGetPC(-1));
		return;

		case 0x4800:
			palette_1 = data & 0x1f;
		//	coin counter = data & 0x20
		return;

		case 0x5000:
			palette_2 = data & 0x1f;
			flipscreen = data & 0x20;
		return;

		case 0x5800:
			speakres_vrx = 0;
			ttmahjng_port_select = data;
		return;

		case 0x6800: // ttmahjng
			AY8910Write(0, 1, data);
		return;

		case 0x6900: // ttmahjng
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 speakres_in3_read()
{
	UINT8 ret = 0xff;

	speakres_vrx++;

	if (speakres_vrx > 0x300) ret &= ~5;
	if (speakres_vrx > 0x200) ret &= ~2;

	return ret;
}

static UINT8 route16_protection_read()
{
// neither of these work -dink
#if 0
	// this is enough to bypass the protection
	protection_data++;
	return (1 << ((protection_data >> 1) & 7));
#endif
	// This gives exact values we're looking for
	INT32 pc = ZetGetPC(-1);

	if (DrvZ80ROM0[pc - 2] == 0xcb && (DrvZ80ROM0[pc] & 0xf7) == 0x20)
	{
		INT32 shift = (DrvZ80ROM0[pc - 1] >> 3) & 7;
		INT32 bit = (DrvZ80ROM0[pc] >> 3) & 1;
		protection_data = bit << shift;
		return protection_data;
	}

	return protection_data;
}

static UINT8 __fastcall route16_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
		case 0x3001:
			//bprintf (0, _T("prot r: %4.4x, %2.2x "), address, DrvShareRAM[0x40]);
			return route16_protection_read();

		case 0x4800:
			return DrvDips[0];

		case 0x5000:
			return DrvInputs[0];

		case 0x5800:
			return DrvInputs[1];

		case 0x6000:
			return speakres_in3_read();

		case 0x6400: // routex protection
			if (ZetGetPC(-1) == 0x2f) return 0xfb;
			return 0;
	}

	return 0;
}

static UINT8 __fastcall ttmahjng_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x4800:
			return 0; // dips (not used)

		case 0x5000:
			for (INT32 i = 0; i < 4; i++)
				if (ttmahjng_port_select == (1 << i)) return DrvInputs[4 + i];
			return 0;

		case 0x5800:
			for (INT32 i = 0; i < 4; i++)
				if (ttmahjng_port_select == (1 << i)) return DrvInputs[i];
			return 0;
	}

	return 0;
}

static void __fastcall route16_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x100)
	{
		case 0x000:
			AY8910Write(0, 1, data);
		return;

		case 0x100:
			AY8910Write(0, 0, data);
		return;
	}
}

static void __fastcall route16_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x4000) {
		DrvShareRAM[address & 0x3ff] = data;

		if (address >= 0x4313 && address <= 0x4319 && data == 0xff) {
			ZetRunEnd();
		}

		return;
	}

	switch (address)
	{
		case 0x2800:
			DACWrite(0, data);
		return;
	}
}

static void stratvox_sn76477_write(UINT32, UINT32 data)
{
	SN76477_enable_w(0, (data >> 0) & 1);
	SN76477_vco_w(0, (data >> 1) & 1);
	SN76477_envelope_1_w(0, (data >> 2) & 1);
	SN76477_envelope_2_w(0, (data >> 3) & 1);
	SN76477_mixer_a_w(0, (data >> 4) & 1);
	SN76477_mixer_b_w(0, (data >> 5) & 1);
	SN76477_mixer_c_w(0, (data >> 6) & 1);
#if 0
	SN76477_mixer_w(0,(data >> 4) & 7);
	SN76477_envelope_w(0,(data >> 2) & 3);
	SN76477_vco_w(0,(data >> 1) & 1);
	SN76477_enable_w(0,data & 1);
#endif
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (2500000.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	DACReset();
	SN76477_reset(0);
	ZetClose();

	AY8910Reset(0);

	palette_1 = 0;
	palette_2 = 0;
	flipscreen = 0;
	speakres_vrx = 0;
	ttmahjng_port_select = 0;
	protection_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x004000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x00008 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x000400;
	DrvVidRAM0		= Next; Next += 0x004000;
	DrvVidRAM1		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 LoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad0 = DrvZ80ROM0;
	UINT8 *pLoad1 = DrvZ80ROM1;
	UINT8 *cLoad  = DrvColPROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(pLoad0, i, 1)) return 1;
			pLoad0 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(pLoad1, i, 1)) return 1;
			pLoad1 += ri.nLen;
			if (ri.nType & 8) pLoad1 += ri.nLen; // space echo
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += ri.nLen;
			continue;
		}		
	}

	program_size = pLoad0 - DrvZ80ROM0;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, program_size - 1, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x4000, 0x43ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM0,		0x8000, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(route16_main_write);
	ZetSetReadHandler((BurnDrvGetGenreFlags() & GBF_MAHJONG) ? ttmahjng_main_read : route16_main_read);
	ZetSetOutHandler(route16_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x4000, 0x43ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM1,		0x8000, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(route16_sound_write);
	ZetClose();

	SN76477_init(0);
	SN76477_set_noise_res(0, RES_K(47));
	SN76477_set_filter_res(0, RES_K(150));
	SN76477_set_filter_cap(0, CAP_U(0.001));
	SN76477_set_decay_res(0, RES_M(3.3));
	SN76477_set_attack_decay_cap(0, CAP_U(1.0));
	SN76477_set_attack_res(0, RES_K(4.7));
	SN76477_set_amplitude_res(0, RES_K(200));
	SN76477_set_feedback_res(0, RES_K(55));
	SN76477_set_oneshot_res(0, RES_K(4.7));
	SN76477_set_oneshot_cap(0, CAP_U(2.2));
	SN76477_set_pitch_voltage(0, 5.0);
	SN76477_set_slf_res(0, RES_K(75));
	SN76477_set_slf_cap(0, CAP_U(1.0));
	SN76477_set_vco_res(0, RES_K(100));
	SN76477_set_vco_cap(0, CAP_U(0.022));
	SN76477_set_vco_voltage(0, 5.0*2/(2+10));
	SN76477_mixer_w(0, 0);
	SN76477_envelope_w(0, 0);
	SN76477_set_mastervol(0, 10.00); // weird

	// call after SN76477!
	AY8910Init(0, 1250000, 0);
	AY8910SetPorts(0, NULL, NULL, &stratvox_sn76477_write, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2500000);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	AY8910Exit(0);
	SN76477_exit(0);
	DACExit();

	ZetExit();

	GenericTilesExit();

	BurnFree(AllMem);

	return 0;
}

static void draw_layer()
{
	UINT8 *color_prom1 = DrvColPROM + 0x000;
	UINT8 *color_prom2 = DrvColPROM + 0x100;

	INT32 col0 = ((palette_1 << 6) & 0x80) | (palette_1 << 2);
	INT32 col1 = ((palette_2 << 6) & 0x80) | (palette_2 << 2);

	for (INT32 offs = 0; offs < 0x4000; offs++)
	{
		UINT8 y = offs >> 6;
		UINT8 x = offs << 2;

		UINT8 data1 = DrvVidRAM0[offs];
		UINT8 data2 = DrvVidRAM1[offs];

		for (INT32 i = 0; i < 4; i++)
		{
			UINT8 color1 = color_prom1[col0 | ((data1 >> 3) & 0x02) | (data1 & 0x01)];
			UINT8 color2 = color_prom2[col1 | ((data2 >> 3) & 0x02) | (data2 & 0x01) | (((color1 << 6) & 0x80) | ((color1 << 7) & 0x80))];

			UINT8 final_color = (color1 | color2) & 0x07;

			if (flipscreen)
				pTransDraw[(255 - y) * nScreenWidth + (255 - x)] = final_color;
			else
				pTransDraw[y * nScreenWidth + x] = final_color;

			x++;
			data1 >>= 1;
			data2 >>= 1;
		}
	}
}

static void draw_layer_type2()
{
	UINT8 *color_prom1 = DrvColPROM + 0x000;
	UINT8 *color_prom2 = DrvColPROM + 0x100;

	INT32 col0 = (palette_1 << 2);
	INT32 col1 = (palette_2 << 2);

	for (INT32 offs = 0; offs < 0x4000; offs++)
	{
		UINT8 y = offs >> 6;
		UINT8 x = offs << 2;

		UINT8 data1 = DrvVidRAM0[offs];
		UINT8 data2 = DrvVidRAM1[offs];

		for (INT32 i = 0; i < 4; i++)
		{
			UINT8 color1 = color_prom1[col0 | ((data1 >> 3) & 0x02) | (data1 & 0x01)];
			UINT8 color2 = color_prom2[col1 | ((data2 >> 3) & 0x02) | (data2 & 0x01) | (((data1 << 3) & 0x80) | ((data1 << 7) & 0x80))];

			UINT8 final_color = (color1 | color2) & 0x07;

			if (flipscreen)
				pTransDraw[(255 - y) * nScreenWidth + (255 - x)] = final_color;
			else
				pTransDraw[y * nScreenWidth + x] = final_color;

			x++;
			data1 >>= 1;
			data2 >>= 1;
		}
	}
}

static INT32 Route16Draw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 8; i++) {
			DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
		}
		DrvRecalc = 0;
	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TtmahjngDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 8; i++) {
			DrvPalette[i] = BurnHighCol((i&4)?0xff:0, (i&2)?0xff:0, (i&1)?0xff:0, 0);
		}
		DrvRecalc = 0;
	}

	draw_layer_type2();

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
		memset (DrvInputs, (BurnDrvGetGenreFlags() & GBF_MAHJONG) ? 0xff: 0, 8);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
			DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 2500000 / (nBurnFPS / 100), 2500000 / (nBurnFPS / 100) };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		SN76477_sound_update(0, pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(palette_1);
		SCAN_VAR(palette_2);		
		SCAN_VAR(ttmahjng_port_select);
		SCAN_VAR(speakres_vrx);
		SCAN_VAR(protection_data);
	}

	return 0;
}


// Route 16 (set 1)

static struct BurnRomInfo route16RomDesc[] = {
	{ "tvg54.a0",     	0x0800, 0xaef9ffc1, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "tvg55.a1",     	0x0800, 0x389bc077, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "tvg56.a2",     	0x0800, 0x1065a468, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "tvg57.a3",     	0x0800, 0x0b1987f3, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "tvg58.a4",     	0x0800, 0xf67d853a, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "tvg59.a5",     	0x0800, 0xd85cf758, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "tvg60.b0",     	0x0800, 0x0f9588a7, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "tvg61.b1",     	0x0800, 0x2b326cf9, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "tvg62.b2",     	0x0800, 0x529cad13, 2 | BRF_ESS | BRF_PRG }, //  8
	{ "tvg63.b3",     	0x0800, 0x3bd8b899, 2 | BRF_ESS | BRF_PRG }, //  9

	{ "mb7052.59",    	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10 Graphics
	{ "mb7052.61",    	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 11
};

STD_ROM_PICK(route16)
STD_ROM_FN(route16)

static INT32 route16Init()
{
	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		// Patch protection
		DrvZ80ROM0[0x00e9] = 0x3a;

		DrvZ80ROM0[0x0105] = 0x00; // jp nz,$4109 (nirvana) - NOP's in route16c
		DrvZ80ROM0[0x0106] = 0x00;
		DrvZ80ROM0[0x0107] = 0x00;

		DrvZ80ROM0[0x072a] = 0x00; // jp nz,$4238 (nirvana)
		DrvZ80ROM0[0x072b] = 0x00;
		DrvZ80ROM0[0x072c] = 0x00;

		DrvZ80ROM0[0x0754] = 0xc3;
		DrvZ80ROM0[0x0755] = 0x63;
		DrvZ80ROM0[0x0756] = 0x07;
	}

	return nRet;
}

struct BurnDriver BurnDrvroute16 = {
	"route16", NULL, NULL, NULL, "1981",
	"Route 16 (set 1)\0", NULL, "Tehkan/Sun (Centuri license)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, route16RomInfo, route16RomName, NULL, NULL, NULL, NULL, Route16InputInfo, Route16DIPInfo,
	route16Init, DrvExit, DrvFrame, Route16Draw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Route 16 (set 2)

static struct BurnRomInfo route16aRomDesc[] = {
	{ "vg-54",        	0x0800, 0x0c966319, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "vg-55",        	0x0800, 0xa6a8c212, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "vg-56",        	0x0800, 0x5c74406a, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "vg-57",        	0x0800, 0x313e68ab, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "vg-58",       	0x0800, 0x40824e3c, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "vg-59",       	0x0800, 0x9313d2c2, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "route16.b0",   	0x0800, 0x0f9588a7, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "vg-61",        	0x0800, 0xb216c88c, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "route16.b2",   	0x0800, 0x529cad13, 2 | BRF_ESS | BRF_PRG }, //  8
	{ "route16.b3",   	0x0800, 0x3bd8b899, 2 | BRF_ESS | BRF_PRG }, //  9

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 11
};

STD_ROM_PICK(route16a)
STD_ROM_FN(route16a)

static INT32 route16aInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		// Patch protection
		DrvZ80ROM0[0x00e9] = 0x3a;

		DrvZ80ROM0[0x0105] = 0x00; // jp nz,$4109 (nirvana) - NOP's in route16c
		DrvZ80ROM0[0x0106] = 0x00;
		DrvZ80ROM0[0x0107] = 0x00;

		DrvZ80ROM0[0x0731] = 0x00; // jp nz,$4238 (nirvana)
		DrvZ80ROM0[0x0732] = 0x00;
		DrvZ80ROM0[0x0733] = 0x00;

		DrvZ80ROM0[0x0747] = 0xc3;
		DrvZ80ROM0[0x0748] = 0x56;
		DrvZ80ROM0[0x0749] = 0x07;
	}

	return nRet;
}

struct BurnDriver BurnDrvroute16a = {
	"route16a", "route16", NULL, NULL, "1981",
	"Route 16 (set 2)\0", NULL, "Tehkan/Sun (Centuri license)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, route16aRomInfo, route16aRomName, NULL, NULL, NULL, NULL, Route16InputInfo, Route16DIPInfo,
	route16aInit, DrvExit, DrvFrame, Route16Draw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Route 16 (set 3, bootleg?)

static struct BurnRomInfo route16cRomDesc[] = {
	{ "route16.a0",  	0x0800, 0x8f9101bd, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "route16.a1",   	0x0800, 0x389bc077, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "route16.a2",   	0x0800, 0x1065a468, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "route16.a3",   	0x0800, 0x0b1987f3, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "route16.a4",   	0x0800, 0xf67d853a, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "route16.a5",   	0x0800, 0xd85cf758, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "route16.b0",   	0x0800, 0x0f9588a7, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "route16.b1",   	0x0800, 0x2b326cf9, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "route16.b2",   	0x0800, 0x529cad13, 2 | BRF_ESS | BRF_PRG }, //  8
	{ "route16.b3",   	0x0800, 0x3bd8b899, 2 | BRF_ESS | BRF_PRG }, //  9

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 11
};

STD_ROM_PICK(route16c)
STD_ROM_FN(route16c)

static INT32 route16cInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		// Patch protection
		DrvZ80ROM0[0x00e9] = 0x3a;

		DrvZ80ROM0[0x0754] = 0xc3;
		DrvZ80ROM0[0x0755] = 0x63;
		DrvZ80ROM0[0x0756] = 0x07;
	}

	return nRet;
}

struct BurnDriver BurnDrvroute16c = {
	"route16c", "route16", NULL, NULL, "1981",
	"Route 16 (set 3, bootleg?)\0", NULL, "Tehkan/Sun (Centuri license)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, route16cRomInfo, route16cRomName, NULL, NULL, NULL, NULL, Route16InputInfo, Route16DIPInfo,
	route16cInit, DrvExit, DrvFrame, Route16Draw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Route 16 (bootleg)

static struct BurnRomInfo route16blRomDesc[] = {
	{ "rt16.0",       	0x0800, 0xb1f0f636, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "rt16.1",       	0x0800, 0x3ec52fe5, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "rt16.2",       	0x0800, 0xa8e92871, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "rt16.3",       	0x0800, 0xa0fc9fc5, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "rt16.4",       	0x0800, 0x6dcaf8c4, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "rt16.5",       	0x0800, 0x63d7b05b, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "rt16.6",       	0x0800, 0xfef605f3, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "rt16.7",       	0x0800, 0xd0d6c189, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "rt16.8",      	0x0800, 0xdefc5797, 2 | BRF_ESS | BRF_PRG }, //  8
	{ "rt16.9",       	0x0800, 0x88d94a66, 2 | BRF_ESS | BRF_PRG }, //  9

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 11
};

STD_ROM_PICK(route16bl)
STD_ROM_FN(route16bl)

struct BurnDriver BurnDrvroute16bl = {
	"route16bl", "route16", NULL, NULL, "1981",
	"Route 16 (bootleg)\0", NULL, "bootleg (Leisure and Allied)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, route16blRomInfo, route16blRomName, NULL, NULL, NULL, NULL, Route16InputInfo, Route16DIPInfo,
	DrvInit, DrvExit, DrvFrame, Route16Draw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Route X (bootleg)

static struct BurnRomInfo routexRomDesc[] = {
	{ "routex01.a0",  	0x0800, 0x99b500e7, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "rt16.1",       	0x0800, 0x3ec52fe5, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "rt16.2",       	0x0800, 0xa8e92871, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "rt16.3",       	0x0800, 0xa0fc9fc5, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "routex05.a4",  	0x0800, 0x2fef7653, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "routex06.a5", 	0x0800, 0xa39ef648, 1 | BRF_ESS | BRF_PRG }, //  5
	{ "routex07.a6",  	0x0800, 0x89f80c1c, 1 | BRF_ESS | BRF_PRG }, //  6

	{ "routex11.b0",  	0x0800, 0xb51edd1d, 2 | BRF_ESS | BRF_PRG }, //  7 Z80 #1 Code
	{ "rt16.7",       	0x0800, 0xd0d6c189, 2 | BRF_ESS | BRF_PRG }, //  8
	{ "rt16.8",      	0x0800, 0xdefc5797, 2 | BRF_ESS | BRF_PRG }, //  9
	{ "rt16.9",       	0x0800, 0x88d94a66, 2 | BRF_ESS | BRF_PRG }, // 10

	{ "im5623.f10",		0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 11 Graphics
	{ "im5623.f12",		0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 12
};

STD_ROM_PICK(routex)
STD_ROM_FN(routex)

struct BurnDriver BurnDrvroutex = {
	"routex", "route16", NULL, NULL, "1981",
	"Route X (bootleg)\0", NULL, "bootleg", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, routexRomInfo, routexRomName, NULL, NULL, NULL, NULL, Route16InputInfo, Route16DIPInfo,
	DrvInit, DrvExit, DrvFrame, Route16Draw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Speak & Rescue

static struct BurnRomInfo speakresRomDesc[] = {
	{ "speakres.1",   	0x0800, 0x6026e4ea, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "speakres.2",   	0x0800, 0x93f0d4da, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "speakres.3",  	0x0800, 0xa3874304, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "speakres.4",   	0x0800, 0xf484be3a, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "speakres.5",   	0x0800, 0x61b12a67, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "speakres.6",   	0x0800, 0x220e0ab2, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "speakres.7",   	0x0800, 0xd417be13, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "speakres.8",   	0x0800, 0x52485d60, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "im5623.f10",  	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  8 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9
};

STD_ROM_PICK(speakres)
STD_ROM_FN(speakres)

struct BurnDriver BurnDrvspeakres = {
	"speakres", NULL, NULL, NULL, "1980",
	"Speak & Rescue\0", NULL, "Sun Electronics", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, speakresRomInfo, speakresRomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, SpeakresDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Speak & Rescue (bootleg)

static struct BurnRomInfo speakresbRomDesc[] = {
	{ "hmi1.27",   	  	0x0800, 0x6026e4ea, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "hmi2.28",      	0x0800, 0x93f0d4da, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hmi3.29",      	0x0800, 0xa3874304, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hmi4.30",      	0x0800, 0xf484be3a, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "hmi5.31",      	0x0800, 0xaa2aaabe, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "hmi6.32",      	0x0800, 0x220e0ab2, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "hmi.33",       	0x0800, 0xbeafe7c5, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "hmi.34",       	0x0800, 0x12ecd87b, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "hmi.62",       	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  8 Graphics
	{ "hmi.64",       	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9
};

STD_ROM_PICK(speakresb)
STD_ROM_FN(speakresb)

struct BurnDriver BurnDrvspeakresb = {
	"speakresb", "speakres", NULL, NULL, "1980",
	"Speak & Rescue (bootleg)\0", NULL, "bootleg", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, speakresbRomInfo, speakresbRomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, SpeakresDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Stratovox

static struct BurnRomInfo stratvoxRomDesc[] = {
	{ "ls01.bin",     	0x0800, 0xbf4d582e, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "ls02.bin",     	0x0800, 0x16739dd4, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "ls03.bin",     	0x0800, 0x083c28de, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "ls04.bin",     	0x0800, 0xb0927e3b, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "ls05.bin",     	0x0800, 0xccd25c4e, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "ls06.bin",     	0x0800, 0x07a907a7, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "ls07.bin",     	0x0800, 0x4d333985, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "ls08.bin",     	0x0800, 0x35b753fc, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  8 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9
};

STD_ROM_PICK(stratvox)
STD_ROM_FN(stratvox)

struct BurnDriver BurnDrvstratvox = {
	"stratvox", "speakres", NULL, NULL, "1980",
	"Stratovox\0", NULL, "[Sun Electronics] (Taito license)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stratvoxRomInfo, stratvoxRomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, StratvoxDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Stratovox (bootleg)

static struct BurnRomInfo stratvobRomDesc[] = {
	{ "j0-1",         	0x0800, 0x93c78274, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "j0-2",         	0x0800, 0x93b2b02d, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "j0-3",         	0x0800, 0x655facb5, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "j0-4",         	0x0800, 0xb0927e3b, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "j0-5",         	0x0800, 0x9d2178d9, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "j0-6",         	0x0800, 0x79118ffc, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "b0-a",         	0x0800, 0x4d333985, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "j0-a",         	0x0800, 0x3416a830, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  8 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9
};

STD_ROM_PICK(stratvob)
STD_ROM_FN(stratvob)

struct BurnDriver BurnDrvstratvob = {
	"stratvoxb", "speakres", NULL, NULL, "1980",
	"Stratovox (bootleg)\0", NULL, "bootleg", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stratvobRomInfo, stratvobRomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, StratvoxDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Space Echo (set 1)

static struct BurnRomInfo spacechoRomDesc[] = {
	{ "rom.a0",       	0x0800, 0x40d74dce, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "rom.a1",      	0x0800, 0xa5f0a34f, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "rom.a2",       	0x0800, 0xcbbb3acb, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "rom.a3",       	0x0800, 0x311050ca, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "rom.a4",       	0x0800, 0x28943803, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "rom.a5",       	0x0800, 0x851c9f28, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "rom.b0",       	0x0800, 0xdb45689d, 2|8|BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "rom.b2",      	0x0800, 0x1e074157, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "rom.b3",       	0x0800, 0xd50a8b20, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "im5623.f10",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9 Graphics
	{ "im5623.f12",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10
};

STD_ROM_PICK(spacecho)
STD_ROM_FN(spacecho)

struct BurnDriver BurnDrvspacecho = {
	"spacecho", "speakres", NULL, NULL, "1980",
	"Space Echo (set 1)\0", NULL, "bootleg (Gayton Games)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacechoRomInfo, spacechoRomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, SpacechoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Space Echo (set 2)

static struct BurnRomInfo spacecho2RomDesc[] = {
	{ "c11.5.6t",     	0x0800, 0x90637f25, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "c2.5t",        	0x0800, 0xa5f0a34f, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "c3.4.5t",      	0x0800, 0xcbbb3acb, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "c4.4t",        	0x0800, 0x311050ca, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "cb5.3t",       	0x0800, 0x28943803, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "cb6.2.3t",     	0x0800, 0x851c9f28, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "cb7.5b",      	0x0800, 0xdb45689d, 2|8|BRF_ESS | BRF_PRG }, //  6 Z80 #1 Code
	{ "cb9.4b",       	0x0800, 0x1e074157, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "cb10.3b",     	0x0800, 0xd50a8b20, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "mb7052.6k",    	0x0100, 0x08793ef7, 3 | BRF_GRA },	     //  9 Graphics
	{ "mb7052.6m",   	0x0100, 0x08793ef7, 3 | BRF_GRA },	     // 10
};

STD_ROM_PICK(spacecho2)
STD_ROM_FN(spacecho2)

struct BurnDriver BurnDrvspacecho2 = {
	"spacecho2", "speakres", NULL, NULL, "1980",
	"Space Echo (set 2)\0", NULL, "bootleg (Gayton Games)", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacecho2RomInfo, spacecho2RomName, NULL, NULL, NULL, NULL, StratvoxInputInfo, SpacechoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// Jongputer

static struct BurnRomInfo jongputeRomDesc[] = {
	{ "j2",        		0x1000, 0x6690b6a4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "j3",     	   	0x1000, 0x985723d3, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "j4", 	       	0x1000, 0xf35ab1e6, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "j5",	        	0x1000, 0x77074618, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "j6", 	       	0x1000, 0x54b349b0, 2 | BRF_ESS | BRF_PRG }, //  4 Z80 #1 Code
	
	{ "j1", 	       	0x1000, 0x6d6ba272, 0 | BRF_OPT | BRF_SND }, //  5 sampling voice

	{ "ju03",         	0x0100, 0x27d47624, 3 | BRF_GRA },	     //  7 Graphics
	{ "ju09",        	0x0100, 0x27d47624, 3 | BRF_GRA },	     //  8
};

STD_ROM_PICK(jongpute)
STD_ROM_FN(jongpute)

struct BurnDriver BurnDrvjongpute = {
	"jongpute", NULL, NULL, NULL, "1980",
	"Jongputer\0", NULL, "Taito", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAHJONG, 0,
	NULL, jongputeRomInfo, jongputeRomName, NULL, NULL, NULL, NULL, TtmahjngInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};


// T.T Mahjong

static struct BurnRomInfo ttmahjngRomDesc[] = {
	{ "ju04",        	0x1000, 0xfe7c693a, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "ju05",        	0x1000, 0x985723d3, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "ju06",        	0x1000, 0x2cd69bc8, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "ju07",        	0x1000, 0x30e8ec63, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "ju01",        	0x0800, 0x0f05ca3c, 2 | BRF_ESS | BRF_PRG }, //  4 Z80 #1 Code
	{ "ju02",        	0x0800, 0xc1ffeceb, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "ju08",        	0x0800, 0x2dcc76b5, 2 | BRF_ESS | BRF_PRG }, //  6

	{ "ju03",         	0x0100, 0x27d47624, 3 | BRF_GRA },	     //  7 Graphics
	{ "ju09",        	0x0100, 0x27d47624, 3 | BRF_GRA },	     //  8
};

STD_ROM_PICK(ttmahjng)
STD_ROM_FN(ttmahjng)

struct BurnDriver BurnDrvttmahjng = {
	"ttmahjng", "jongpute", NULL, NULL, "1980",
	"T.T Mahjong\0", NULL, "Taito", "Route 16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAHJONG, 0,
	NULL, ttmahjngRomInfo, ttmahjngRomName, NULL, NULL, NULL, NULL, TtmahjngInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, TtmahjngDraw, DrvScan, &DrvRecalc, 0x8,
	256, 256, 3, 4
};
