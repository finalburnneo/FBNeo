// FB Alpha Mega Zone driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "i8039.h"
#include "flt_rc.h"
#include "resnet.h"
#include "ay8910.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809DecROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvI8039ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvColRAM0;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 irq_enable;
static UINT8 soundlatch;
static UINT8 i8039_status;

static INT32 watchdog;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MegazoneInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Megazone)

static struct BurnDIPInfo MegazoneDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0x5b, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x10, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x10, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x03, "3"			},
	{0x11, 0x01, 0x03, 0x02, "4"			},
	{0x11, 0x01, 0x03, 0x01, "5"			},
	{0x11, 0x01, 0x03, 0x00, "7"			},

//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x11, 0x01, 0x04, 0x00, "Upright"		},
//	{0x11, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x18, 0x18, "20k 70k 70k+"		},
	{0x11, 0x01, 0x18, 0x10, "20k 80k 80k+"		},
	{0x11, 0x01, 0x18, 0x08, "30k 90k 90k+"		},
	{0x11, 0x01, 0x18, 0x00, "30k 100k 100k+"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Easy"			},
	{0x11, 0x01, 0x60, 0x40, "Normal"		},
	{0x11, 0x01, 0x60, 0x20, "Hard"			},
	{0x11, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Megazone)

static struct BurnDIPInfo MegazonaDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0x5b, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x10, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x10, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x03, "2"			},
	{0x11, 0x01, 0x03, 0x02, "3"			},
	{0x11, 0x01, 0x03, 0x01, "4"			},
	{0x11, 0x01, 0x03, 0x00, "7"			},

//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x11, 0x01, 0x04, 0x00, "Upright"		},
//	{0x11, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x18, 0x18, "20k 70k 70k+"		},
	{0x11, 0x01, 0x18, 0x10, "20k 80k 80k+"		},
	{0x11, 0x01, 0x18, 0x08, "30k 90k 90k+"		},
	{0x11, 0x01, 0x18, 0x00, "30k 100k 100k+"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x60, 0x60, "Easy"			},
	{0x11, 0x01, 0x60, 0x40, "Normal"		},
	{0x11, 0x01, 0x60, 0x20, "Hard"			},
	{0x11, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Megazona)

static void megazone_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0000:
		case 0x0001:	// coin counter
		return;

		case 0x0005:	// flipscreen
		return;

		case 0x0007:
			irq_enable = data & 0x01;
		return;

		case 0x0800:
			watchdog = 0;
		return;

		case 0x1000:
			scrollx = data;
		return;

		case 0x1800:
			scrolly = data;
		return;
	}
}

static void __fastcall megazone_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			I8039SetIrqState(1);
		return;

		case 0x4000:
			soundlatch = data;
		return;

		case 0xa000:
		case 0xc000:
		return;

		case 0xc001:
			watchdog = 0;
		return;
	}
}

static UINT8 __fastcall megazone_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return DrvInputs[0];

		case 0x6001:
			return DrvInputs[1];

		case 0x6002:
			return DrvInputs[2];

		case 0x8000:
			return DrvDips[1];

		case 0x8001:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall megazone_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			AY8910Write(0, 0, data);
		return;

		case 0x02:
			AY8910Write(0, 1, data);
		return;
	}
}

static UINT8 __fastcall megazone_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 __fastcall megazone_i8039_read(UINT32 address)
{
	return DrvI8039ROM[address & 0x0fff];
}

static void __fastcall megazone_i8039_write_port(UINT32 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case I8039_p1:
			DACWrite(0, data);
		return;

		case I8039_p2:
			if ((data & 0x80) == 0) {
				I8039SetIrqState(0);
			}
			i8039_status = (data >> 4) & 0x07;
		return;
	}
}

static UINT8 __fastcall megazone_i8039_read_port(UINT32 port)
{
	if ((port & 0x1ff) < 0x100) {
		return soundlatch;
	}

	return 0;
}

static UINT8 AY8910_0_port_A_Read(UINT32)
{
	if (ZetGetActive() == -1) return i8039_status;

	UINT8 timer = (((ZetTotalCycles() * 7159) / 12288) / 512) & 0x0f;

	return (timer << 4) | i8039_status;
}

static void AY8910_0_port_A_Write(UINT32, UINT32 data)
{
	if (ZetGetActive() == -1) return;

	for (INT32 i = 0; i < 3; i++, data>>=2)
	{
		INT32 C = 0;
		if (data & 1) C +=  10000;	//  10000pF = 0.01uF
		if (data & 2) C += 220000;	// 220000pF = 0.22uF

		filter_rc_set_RC(i, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(C));
	}
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3072000.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	AY8910Reset(0);
	ZetClose();

	I8039Open(0);
	I8039Reset();
	I8039Close();

	DACReset();

	scrollx = 0;
	scrolly = 0;
	irq_enable = 0;
	soundlatch = 0;
	i8039_status = 0;

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6809DecROM		= Next; Next += 0x010000;

	DrvZ80ROM		= Next; Next += 0x002000;

	DrvI8039ROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000260;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvColRAM0		= Next; Next += 0x000400;
	DrvVidRAM0		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvProgramDecode()
{
	for (INT32 i = 0x4000; i < 0x10000; i++) {
		DrvM6809DecROM[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
}

static INT32 DrvGfxDecode() // 0, 100
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 Plane1[4]  = { 0x4000*8+4, 0x4000*8+0, 4, 0 };
	INT32 XOffs0[8]  = { STEP8(0,4) };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs1[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0100, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0200, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);

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

	{
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvI8039ROM + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x06000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 15, 1)) return 1;

		DrvProgramDecode();
		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM0,		0x2000, 0x23ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1,		0x2400, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvColRAM0,		0x2800, 0x2bff, MAP_RAM);
	M6809MapMemory(DrvColRAM1,		0x2c00, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x4000,	0x4000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809DecROM + 0x4000,	0x4000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(megazone_main_write);
//	M6809SetReadHandler(megazone_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(megazone_sound_write);
	ZetSetReadHandler(megazone_sound_read);
	ZetSetOutHandler(megazone_sound_write_port);
	ZetSetInHandler(megazone_sound_read_port);
	ZetClose();

	I8039Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(megazone_i8039_read);
	I8039SetCPUOpReadHandler(megazone_i8039_read);
	I8039SetCPUOpReadArgHandler(megazone_i8039_read);
	I8039SetIOReadHandler(megazone_i8039_read_port);
	I8039SetIOWriteHandler(megazone_i8039_write_port);
	I8039Close();

	AY8910Init(0, 1789750, 0);
	AY8910SetPorts(0, &AY8910_0_port_A_Read, NULL, &AY8910_0_port_A_Write, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 2200, 200, CAP_P(0), 1);
	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();
	I8039Exit();

	AY8910Exit(0);
	DACExit();
	filter_rc_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	static const int resistances_rg[3] = { 1000, 470, 220 };
	static const int resistances_b [2] = { 470, 220 };
	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 255, -1.0,
			3, &resistances_rg[0], rweights, 1000, 0,
			3, &resistances_rg[0], gweights, 1000, 0,
			2, &resistances_b[0],  bweights, 1000, 0);

	UINT32 pens[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = combine_2_weights(bweights, bit0, bit1);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = pens[(DrvColPROM[i+0x20] & 0xf) | ((i >> 4) & 0x10)];
	}
}

static void draw_layer(UINT8 *cram, UINT8 *vram, INT32 xscroll, UINT8 yscroll, INT32 limit, INT32 offset)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if ((offs & 0x1f) > limit && limit != 0) continue;

		sx -= xscroll;
		if (sx < -7) sx += 256;
		sx += offset;

		sy -= yscroll;
		if (sy < -7) sy += 256;

		INT32 attr  = cram[offs];
		INT32 code  = vram[offs] + ((attr & 0x80) << 1);
		INT32 color =(attr & 0x0f);
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x20;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			}
		}

		if (sx >= 32 || limit) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx+256, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx+256, sy, color, 4, 0x100, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx+256, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx+256, sy, color, 4, 0x100, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x800 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx    =  DrvSprRAM[offs + 3] + 32;
		INT32 sy    = 255 - ((DrvSprRAM[offs + 1] + 16) & 0xff);
		INT32 code  =  DrvSprRAM[offs + 2];
		INT32 color =  DrvSprRAM[offs + 0] & 0x0f;
		INT32 flipx = ~DrvSprRAM[offs + 0] & 0x40;
		INT32 flipy =  DrvSprRAM[offs + 0] & 0x80;

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color * 16, 0, sx, sy - 16, flipx, flipy, 16, 16, DrvColPROM + 0x20);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvColRAM0, DrvVidRAM0, scrollx, scrolly+16, 0, 32);
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_layer(DrvColRAM1, DrvVidRAM1, 0, 16, 5, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
		bprintf (0, _T("Watchdog triggered!\n"));
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	ZetNewFrame();
	I8039NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 5; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
 			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 2048000 / 60, 3072000 / 60, 477266 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	M6809Open(0);
	ZetOpen(0);
	I8039Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += M6809Run(nSegment - nCyclesDone[0]);
		if (i == 240 && irq_enable) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nSegment = (nCyclesTotal[1] * (i + 1)) / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		nSegment = (nCyclesTotal[2] * (i + 1)) / nInterleave;
		nCyclesDone[2] += I8039Run(nSegment - nCyclesDone[2]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Update(0, &pAY8910Buffer[0], nSegmentLength);
			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Update(0, &pAY8910Buffer[0], nSegmentLength);
			filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
		}

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	I8039Close();
	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		M6809Scan(nAction);
		I8039Scan(nAction,pnMin);

		ZetOpen(0);
		AY8910Scan(nAction, pnMin);
		ZetClose();

		DACScan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(irq_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(i8039_status);
	}

	return 0;
}


// Mega Zone (program code L)

static struct BurnRomInfo megazoneRomDesc[] = {
	{ "319_l07.11h",	0x2000, 0x73b616ca, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "319_l06.9h",		0x2000, 0x0ced03f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "319_l05.8h",		0x2000, 0x9dc3b5a1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "319_l04.7h",		0x2000, 0x785b983d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "319_l03.6h",		0x2000, 0xa5318686, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319e02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319e01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_g12.8c",		0x2000, 0x07b8b24b, 5 | BRF_GRA },           // 11 Characters
	{ "319_g13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "319b15.e8",		0x0020, 0x31fd7ab9, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazone)
STD_ROM_FN(megazone)

struct BurnDriver BurnDrvMegazone = {
	"megazone", NULL, NULL, NULL, "1983",
	"Mega Zone (program code L)\0", NULL, "Konami", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazoneRomInfo, megazoneRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mega Zone (program code J)
// Interlogic + Kosuka license set

static struct BurnRomInfo megazonejRomDesc[] = {
	{ "319_j07.11h",	0x2000, 0x5161a523, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "319_j06.9h",		0x2000, 0x7344c3de, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "319_j05.8h",		0x2000, 0xaffa492b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "319_j04.7h",		0x2000, 0x03544ab3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "319_j03.6h",		0x2000, 0x0d95cc0a, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319e02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319e01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_g12.8c",		0x2000, 0x07b8b24b, 5 | BRF_GRA },           // 11 Characters
	{ "319_g13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "319b15.e8",		0x0020, 0x31fd7ab9, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazonej)
STD_ROM_FN(megazonej)

struct BurnDriver BurnDrvMegazonej = {
	"megazonej", "megazone", NULL, NULL, "1983",
	"Mega Zone (program code J)\0", NULL, "Konami (Interlogic / Kosuka license)", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazonejRomInfo, megazonejRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mega Zone (program code I)

static struct BurnRomInfo megazoneiRomDesc[] = {
	{ "319_i07.11h",	0x2000, 0x94b22ea8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "319_i06.9h",		0x2000, 0x0468b619, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "319_i05.8h",		0x2000, 0xac59000c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "319_i04.7h",		0x2000, 0x1e968603, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "319_i03.6h",		0x2000, 0x0888b803, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319e02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319e01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_e12.8c",		0x2000, 0xe0fb7835, 5 | BRF_GRA },           // 11 Characters
	{ "319_e13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "319b15.e8",		0x0020, 0x31fd7ab9, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazonei)
STD_ROM_FN(megazonei)

struct BurnDriver BurnDrvMegazonei = {
	"megazonei", "megazone", NULL, NULL, "1983",
	"Mega Zone (program code I)\0", NULL, "Konami", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazoneiRomInfo, megazoneiRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mega Zone (program code H)
// Kosuka license set

static struct BurnRomInfo megazonehRomDesc[] = {
	{ "319_h07.11h",	0x2000, 0x8ca47f64, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "319_h06.9h",		0x2000, 0xed35b12e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "319_h05.8h",		0x2000, 0xc3655ccd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "319_h04.7h",		0x2000, 0x9e221177, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "319_h03.6h",		0x2000, 0x9048955b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319e02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319h01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_g12.8c",		0x2000, 0x07b8b24b, 5 | BRF_GRA },           // 11 Characters
	{ "319_g13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "prom.48",		0x0020, 0x796dea94, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazoneh)
STD_ROM_FN(megazoneh)

struct BurnDriver BurnDrvMegazoneh = {
	"megazoneh", "megazone", NULL, NULL, "1983",
	"Mega Zone (program code H)\0", NULL, "Konami (Kosuka license)", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazonehRomInfo, megazonehRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mega Zone (unknown program code 1)

static struct BurnRomInfo megazoneaRomDesc[] = {
	{ "ic59_cpu.bin",	0x2000, 0xf41922a0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "ic58_cpu.bin",	0x2000, 0x7fd7277b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic57_cpu.bin",	0x2000, 0xa4b33b51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic56_cpu.bin",	0x2000, 0x2aabcfbf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic55_cpu.bin",	0x2000, 0xb33a3c37, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319e02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319e01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_g12.8c",		0x2000, 0x07b8b24b, 5 | BRF_GRA },           // 11 Characters
	{ "319_g13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "319b15.e8",		0x0020, 0x31fd7ab9, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazonea)
STD_ROM_FN(megazonea)

struct BurnDriver BurnDrvMegazonea = {
	"megazonea", "megazone", NULL, NULL, "1983",
	"Mega Zone (unknown program code 1)\0", NULL, "Konami (Interlogic / Kosuka license)", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazoneaRomInfo, megazoneaRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mega Zone (unknown program code 2)

static struct BurnRomInfo megazonebRomDesc[] = {
	{ "7.11h",			0x2000, 0xd42d67bf, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "6.9h",			0x2000, 0x692398eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.8h",			0x2000, 0x620ffec3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.7h",			0x2000, 0x28650971, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3.6h",			0x2000, 0xf264018f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "319h02.6d",		0x2000, 0xd5d45edb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "319h01.3a",		0x1000, 0xed5725a0, 3 | BRF_PRG | BRF_ESS }, //  6 I8039 Code

	{ "319e11.3e",		0x2000, 0x965a7ff6, 4 | BRF_GRA },           //  7 Sprites
	{ "319e09.2e",		0x2000, 0x5eaa7f3e, 4 | BRF_GRA },           //  8
	{ "319e10.3d",		0x2000, 0x7bb1aeee, 4 | BRF_GRA },           //  9
	{ "319e08.2d",		0x2000, 0x6add71b1, 4 | BRF_GRA },           // 10

	{ "319_e12.8c",		0x2000, 0xe0fb7835, 5 | BRF_GRA },           // 11 Characters
	{ "319_g13.10c",	0x2000, 0x3d8f3743, 5 | BRF_GRA },           // 12

	{ "319b18.a16",		0x0020, 0x23cb02af, 6 | BRF_GRA },           // 13 Color PROMs
	{ "319b16.c6",		0x0100, 0x5748e933, 6 | BRF_GRA },           // 14
	{ "319b17.a11",		0x0100, 0x1fbfce73, 6 | BRF_GRA },           // 15
	{ "319b14.e7",		0x0020, 0x55044268, 6 | BRF_OPT },           // 16
	{ "319b15.e8",		0x0020, 0x31fd7ab9, 6 | BRF_OPT },           // 17
};

STD_ROM_PICK(megazoneb)
STD_ROM_FN(megazoneb)

struct BurnDriver BurnDrvMegazoneb = {
	"megazoneb", "megazone", NULL, NULL, "1983",
	"Mega Zone (unknown program code 2)\0", NULL, "Konami", "GX319",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, megazonebRomInfo, megazonebRomName, NULL, NULL, NULL, NULL, MegazoneInputInfo, MegazonaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};
