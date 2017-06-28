// FB Alpha Karate Champ driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "dac.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Ops;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAMA;
static UINT8 *DrvZ80RAMB;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static UINT8 nmi_enable;
static UINT8 sound_nmi_enable;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 msm_play_lo_nibble;
static UINT8 msm_data;
static UINT8 msmcounter;

static struct BurnInputInfo KchampInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up (left)",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down (left)",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left (left)",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right (left)",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Up (right)",	BIT_DIGITAL,	DrvJoy1 + 6,	"p3 up"		},
	{"P1 Down (right)",		BIT_DIGITAL,	DrvJoy1 + 7,	"p3 down"	},
	{"P1 Left (right)",	BIT_DIGITAL,	DrvJoy1 + 5,	"p3 left"	},
	{"P1 Right (right)",	BIT_DIGITAL,	DrvJoy1 + 4,	"p3 right"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Up (left)",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down (left)",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left (left)",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right (left)",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Up (right)",		BIT_DIGITAL,	DrvJoy2 + 6,	"p4 up"		},
	{"P2 Down (right)",	BIT_DIGITAL,	DrvJoy2 + 7,	"p4 down"	},
	{"P2 Left (right)",	BIT_DIGITAL,	DrvJoy2 + 5,	"p4 left"	},
	{"P2 Right (right)",	BIT_DIGITAL,	DrvJoy2 + 4,	"p4 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Kchamp)

static struct BurnDIPInfo KchampDIPList[]=
{
	{0x15, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x10, 0x00, "Hard"			},
	{0x15, 0x01, 0x10, 0x10, "Normal"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x80, 0x00, "Upright"		},
	{0x15, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Kchamp)

static struct BurnDIPInfo KchampvsDIPList[]=
{
	{0x15, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x15, 0x01, 0x10, 0x00, "Hard"			},
	{0x15, 0x01, 0x10, 0x10, "Normal"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x20, 0x20, "Off"			},
	{0x15, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x15, 0x01, 0x80, 0x00, "On"			},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
};

STDDIPINFO(Kchampvs)

static void __fastcall kchamp_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
			flipscreen = data & 1;
		return;

		case 0x81:
			nmi_enable = data & 1;
		return;

		case 0xa8:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall kchamp_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x80:
			return DrvDips[0];

		case 0x90:
			return DrvInputs[0];

		case 0x98:
			return DrvInputs[1];

		case 0xa0:
			return DrvInputs[2];

		case 0xa8:
			ZetClose();
			ZetOpen(1);
			ZetReset();
			ZetClose();
			ZetOpen(0);
			return 0;
	}

	return 0;
}

static void __fastcall kchamp_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port/2)&1, ~port & 1, data);
		return;

		case 0x04:
			DACWrite(0, data);
		return;

		case 0x05:
			sound_nmi_enable = data & 0x80;
		return;
	}
}

static UINT8 __fastcall kchamp_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x06:
			return soundlatch;
	}

	return 0;
}

static void __fastcall kchampvs_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			flipscreen = data & 1;
		return;

		case 0x01:
			nmi_enable = data & 1;
		return;

		case 0x02:
			ZetClose();
			ZetOpen(1);
			ZetReset();
			ZetClose();
			ZetOpen(0);
		return;

		case 0x40:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall kchampvs_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x40:
			return DrvInputs[1];

		case 0x80:
			return DrvInputs[2];

		case 0xc0:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall kchampvs_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port/2)&1, ~port & 1, data);
		return;

		case 0x04:
			msm_data = data;
			msm_play_lo_nibble = 1;
		return;

		case 0x05:
			MSM5205ResetWrite(0, ~data & 1);
			sound_nmi_enable = data & 0x02;
		return;
	}
}

static UINT8 __fastcall kchampvs_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
			return soundlatch;
	}

	return 0;
}

static void kchampvs_adpcm_interrupt()
{
	if (msm_play_lo_nibble)
		MSM5205DataWrite(0, msm_data & 0x0f);
	else
		MSM5205DataWrite(0, msm_data >> 4);

	msm_play_lo_nibble = !msm_play_lo_nibble;

	if (!(msmcounter ^= 1) && sound_nmi_enable) {
		ZetNmi();
	}
}

static INT32 SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3000000;
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3000000.0000 / (nBurnFPS / 100.0000))));
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
	MSM5205Reset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	nmi_enable = 0;
	sound_nmi_enable = 0;
	soundlatch = 0;
	flipscreen = 0;
	msm_play_lo_nibble = 1;
	msm_data = 0;
	msmcounter = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80Ops		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAMA		= Next; Next += 0x002000;
	DrvZ80RAMB		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x00a000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0x4000*8, 0 };
	INT32 Plane1[2] = { 0xc000*8, 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(0x2000*8,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(8*8,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0800, 2,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x18000);

	GfxDecode(0x0200, 2, 16, 16, Plane1, XOffs, YOffs, 0x080, tmp + 0x8000, DrvGfxROM1 + 0x40000);
	GfxDecode(0x0200, 2, 16, 16, Plane1, XOffs, YOffs, 0x080, tmp + 0x4000, DrvGfxROM1 + 0x20000);
	GfxDecode(0x0200, 2, 16, 16, Plane1, XOffs, YOffs, 0x080, tmp + 0x0000, DrvGfxROM1 + 0x00000);

	BurnFree(tmp);

	return 0;
}

static INT32 KchampInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x06000,  9, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x08000, 10, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x0a000, 11, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x0c000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x12000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x14000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x16000, 26, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 27, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 28, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 29, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAMA,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xea00, 0xeaff, MAP_RAM);
	ZetMapMemory(DrvZ80RAMB,		0xeb00, 0xffff, MAP_RAM);
	ZetSetOutHandler(kchamp_main_write_port);
	ZetSetInHandler(kchamp_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xe000, 0xe2ff, MAP_RAM);
	ZetSetOutHandler(kchamp_sound_write_port);
	ZetSetInHandler(kchamp_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	// not used in this hardware
	MSM5205Init(0, SynchroniseStream, 375000, kchampvs_adpcm_interrupt, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 0.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static void decode()
{
	for (int A = 0; A < 0x10000; A++)
		DrvZ80Ops[A] = (DrvZ80ROM0[A] & 0x55) | ((DrvZ80ROM0[A] & 0x88) >> 2) | ((DrvZ80ROM0[A] & 0x22) << 2);
}

static INT32 KchampvsInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  5, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0xe000, DrvZ80ROM0 + 0xc000, 0x2000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x12000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x14000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x16000, 24, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 25, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 26, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 27, 1)) return 1;

		DrvGfxDecode();
		decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAMA,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd800, 0xd9ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAMB,		0xd900, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0xe000,	0xe000, 0xffff, MAP_ROM);
//	ZetMapMemory(DrvZ80Ops,			0x0000, 0xffff, MAP_FETCH);
	ZetMapArea(0x0000, 0xffff, 2, DrvZ80Ops, DrvZ80ROM0);
	ZetSetOutHandler(kchampvs_main_write_port);
	ZetSetInHandler(kchampvs_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x6000, 0xffff, MAP_RAM);
	ZetSetOutHandler(kchampvs_sound_write_port);
	ZetSetInHandler(kchampvs_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, SynchroniseStream, 375000, kchampvs_adpcm_interrupt, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	// not used on this hardware
	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 0.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	MSM5205Exit();
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = DrvColPROM[0x000 + i] & 0xf;
		UINT8 g = DrvColPROM[0x100 + i] & 0xf;
		UINT8 b = DrvColPROM[0x200 + i] & 0xf;

		r += r * 16;
		g += g * 16;
		b += b * 16;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 offs = (2 * 32); offs < (32 * 32) - (2 * 32); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr = DrvColRAM[offs];
		INT32 code = DrvVidRAM[offs] + ((attr & 0x07) * 256);
		INT32 color= (attr >> 3) & 0x1f;

		Render8x8Tile(pTransDraw, code, sx, sy - 16, color, 2, 0x80, DrvGfxROM0);
	}
}

static void draw_sprites(INT32 sxoffs, INT32 syoffs)
{
	INT32 banks[4] = { 0x400, 0x200, 0 };

	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs + 2];
		INT32 code = DrvSprRAM[offs + 1] + ((attr & 0x10) << 4) + banks[(attr >> 5) & 3];
		INT32 color = attr & 0x0f;
		INT32 flipx = 0;
		INT32 flipy = attr & 0x80;
		INT32 sx = DrvSprRAM[offs + 3] - sxoffs;
		INT32 sy = syoffs - DrvSprRAM[offs];

		if ((nBurnLayer & (1 << ((attr>>5)&3))) == 0) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 KchampDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();
	draw_sprites(8,247);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 KchampFrame()
{
	if (DrvReset) {
		DrvDoReset();
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

	INT32 nInterleave = 40;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (nmi_enable && i == 39) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (sound_nmi_enable && (i == 20 || i == 39)) ZetNmi();
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		KchampDraw();
	}

	return 0;
}

static INT32 KchampvsDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();
	draw_sprites(0,240);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 KchampvsFrame()
{
	if (DrvReset) {
		DrvDoReset();
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

	INT32 nInterleave = MSM5205CalcInterleave(0, 3000000);
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (nmi_enable && i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		MSM5205Update();

		ZetClose();
	}

	ZetOpen(1);
	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

	if (pBurnDraw) {
		KchampvsDraw();
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
		MSM5205Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(msm_play_lo_nibble);
		SCAN_VAR(msm_data);
	}

	return 0;
}


// Karate Champ (US)

static struct BurnRomInfo kchampRomDesc[] = {
	{ "b014.bin",	0x2000, 0x0000d1a0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code #0
	{ "b015.bin",	0x2000, 0x03fae67e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b016.bin",	0x2000, 0x3b6e1d08, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b017.bin",	0x2000, 0xc1848d1a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "b018.bin",	0x2000, 0xb824abc7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "b019.bin",	0x2000, 0x3b487a46, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "b026.bin",	0x2000, 0x999ed2c7, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code #1
	{ "b025.bin",	0x2000, 0x33171e07, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "b024.bin",	0x2000, 0x910b48b9, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "b023.bin",	0x2000, 0x47f66aac, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "b022.bin",	0x2000, 0x5928e749, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "b021.bin",	0x2000, 0xca17e3ba, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "b020.bin",	0x2000, 0xada4f2cd, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "b000.bin",	0x2000, 0xa4fa98a1, 3 | BRF_GRA },           // 13 Characters
	{ "b001.bin",	0x2000, 0xfea09f7c, 3 | BRF_GRA },           // 14

	{ "b013.bin",	0x2000, 0xeaad4168, 4 | BRF_GRA },           // 15 Sprites
	{ "b004.bin",	0x2000, 0x10a47e2d, 4 | BRF_GRA },           // 16
	{ "b012.bin",	0x2000, 0xb4842ea9, 4 | BRF_GRA },           // 17
	{ "b003.bin",	0x2000, 0x8cd166a5, 4 | BRF_GRA },           // 18
	{ "b011.bin",	0x2000, 0x4cbd3aa3, 4 | BRF_GRA },           // 19
	{ "b002.bin",	0x2000, 0x6be342a6, 4 | BRF_GRA },           // 20
	{ "b007.bin",	0x2000, 0xcb91d16b, 4 | BRF_GRA },           // 21
	{ "b010.bin",	0x2000, 0x489c9c04, 4 | BRF_GRA },           // 22
	{ "b006.bin",	0x2000, 0x7346db8a, 4 | BRF_GRA },           // 23
	{ "b009.bin",	0x2000, 0xb78714fc, 4 | BRF_GRA },           // 24
	{ "b005.bin",	0x2000, 0xb2557102, 4 | BRF_GRA },           // 25
	{ "b008.bin",	0x2000, 0xc85aba0e, 4 | BRF_GRA },           // 26

	{ "br27",	0x0100, 0xf683c54a, 5 | BRF_GRA },           // 27 Color data
	{ "br26",	0x0100, 0x3ddbb6c4, 5 | BRF_GRA },           // 28
	{ "br25",	0x0100, 0xba4a5651, 5 | BRF_GRA },           // 29
};

STD_ROM_PICK(kchamp)
STD_ROM_FN(kchamp)

struct BurnDriver BurnDrvKchamp = {
	"kchamp", NULL, NULL, NULL, "1984",
	"Karate Champ (US)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, kchampRomInfo, kchampRomName, NULL, NULL, KchampInputInfo, KchampDIPInfo,
	KchampInit, DrvExit, KchampFrame, KchampDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Karate Dou (Japan)

static struct BurnRomInfo karatedoRomDesc[] = {
	{ "be14",	0x2000, 0x44e60aa0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code #0 (Encrypted)
	{ "be15",	0x2000, 0xa65e3793, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "be16",	0x2000, 0x151d8872, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "be17",	0x2000, 0x8f393b6a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "be18",	0x2000, 0xa09046ad, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "be19",	0x2000, 0x0cdc4da9, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "be26",	0x2000, 0x999ab0a3, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code #1
	{ "be25",	0x2000, 0x253bf0da, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "be24",	0x2000, 0xe2c188af, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "be23",	0x2000, 0x25262de1, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "be22",	0x2000, 0x38055c48, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "be21",	0x2000, 0x5f0efbe7, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "be20",	0x2000, 0xcbe8a533, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "be00",	0x2000, 0xcec020f2, 3 | BRF_GRA },           // 13 Characters
	{ "be01",	0x2000, 0xcd96271c, 3 | BRF_GRA },           // 14

	{ "be13",	0x2000, 0xfb358707, 4 | BRF_GRA },           // 15 Sprites
	{ "be04",	0x2000, 0x48372bf8, 4 | BRF_GRA },           // 16
	{ "b012.bin",	0x2000, 0xb4842ea9, 4 | BRF_GRA },           // 17
	{ "b003.bin",	0x2000, 0x8cd166a5, 4 | BRF_GRA },           // 18
	{ "b011.bin",	0x2000, 0x4cbd3aa3, 4 | BRF_GRA },           // 19
	{ "b002.bin",	0x2000, 0x6be342a6, 4 | BRF_GRA },           // 20
	{ "be07",	0x2000, 0x40f2b6fb, 4 | BRF_GRA },           // 21
	{ "be10",	0x2000, 0x325c0a97, 4 | BRF_GRA },           // 22
	{ "b006.bin",	0x2000, 0x7346db8a, 4 | BRF_GRA },           // 23
	{ "b009.bin",	0x2000, 0xb78714fc, 4 | BRF_GRA },           // 24
	{ "b005.bin",	0x2000, 0xb2557102, 4 | BRF_GRA },           // 25
	{ "b008.bin",	0x2000, 0xc85aba0e, 4 | BRF_GRA },           // 26

	{ "br27",	0x0100, 0xf683c54a, 5 | BRF_GRA },           // 27 Color data
	{ "br26",	0x0100, 0x3ddbb6c4, 5 | BRF_GRA },           // 28
	{ "br25",	0x0100, 0xba4a5651, 5 | BRF_GRA },           // 29
};

STD_ROM_PICK(karatedo)
STD_ROM_FN(karatedo)

struct BurnDriver BurnDrvKaratedo = {
	"karatedo", "kchamp", NULL, NULL, "1984",
	"Karate Dou (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, karatedoRomInfo, karatedoRomName, NULL, NULL, KchampInputInfo, KchampDIPInfo,
	KchampInit, DrvExit, KchampFrame, KchampDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Karate Champ (US VS version, set 1)

static struct BurnRomInfo kchampvsRomDesc[] = {
	{ "bs24.d13",	0x2000, 0x829da69b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code #0 (Encrypted)
	{ "bs23.d11",	0x2000, 0x091f810e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bs22.d10",	0x2000, 0xd4df2a52, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bs21.d8",	0x2000, 0x3d4ef0da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bs20.d7",	0x2000, 0x623a467b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bs19.d6",	0x4000, 0x43e196c4, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "bs18.d4",	0x2000, 0xeaa646eb, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code #1
	{ "bs17.d2",	0x2000, 0xd71031ad, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bs16.d1",	0x2000, 0x6f811c43, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "bs12.k1",	0x2000, 0x4c574ecd, 3 | BRF_GRA },           //  9 Characters
	{ "bs13.k3",	0x2000, 0x750b66af, 3 | BRF_GRA },           // 10
	{ "bs14.k5",	0x2000, 0x9ad6227c, 3 | BRF_GRA },           // 11
	{ "bs15.k6",	0x2000, 0x3b6d5de5, 3 | BRF_GRA },           // 12

	{ "bs00.a1",	0x2000, 0x51eda56c, 4 | BRF_GRA },           // 13 Sprites
	{ "bs06.c1",	0x2000, 0x593264cf, 4 | BRF_GRA },           // 14
	{ "bs01.a3",	0x2000, 0xb4842ea9, 4 | BRF_GRA },           // 15
	{ "bs07.c3",	0x2000, 0x8cd166a5, 4 | BRF_GRA },           // 16
	{ "bs02.a5",	0x2000, 0x4cbd3aa3, 4 | BRF_GRA },           // 17
	{ "bs08.c5",	0x2000, 0x6be342a6, 4 | BRF_GRA },           // 18
	{ "bs03.a6",	0x2000, 0x8dcd271a, 4 | BRF_GRA },           // 19
	{ "bs09.c6",	0x2000, 0x4ee1dba7, 4 | BRF_GRA },           // 20
	{ "bs04.a8",	0x2000, 0x7346db8a, 4 | BRF_GRA },           // 21
	{ "bs10.c8",	0x2000, 0xb78714fc, 4 | BRF_GRA },           // 22
	{ "bs05.a10",	0x2000, 0xb2557102, 4 | BRF_GRA },           // 23
	{ "bs11.c10",	0x2000, 0xc85aba0e, 4 | BRF_GRA },           // 24

	{ "br27.k10",	0x0100, 0xf683c54a, 5 | BRF_GRA },           // 25 Color data
	{ "br26.k9",	0x0100, 0x3ddbb6c4, 5 | BRF_GRA },           // 26
	{ "br25.k8",	0x0100, 0xba4a5651, 5 | BRF_GRA },           // 27
};

STD_ROM_PICK(kchampvs)
STD_ROM_FN(kchampvs)

static void patch_decode()
{
	UINT8 *rom = DrvZ80ROM0;

	DrvZ80Ops[0] = rom[0];
	INT32 A = rom[1] + 256 * rom[2];
	DrvZ80Ops[A] = rom[A];
	rom[A+1] ^= 0xee;
	A = rom[A+1] + 256 * rom[A+2];
	DrvZ80Ops[A] = rom[A];
	A += 2;
	DrvZ80Ops[A] = rom[A];

	ZetOpen(0);
	ZetReset(); // catch vectors
	ZetClose();
}

static INT32 KchampvsInit1()
{
	INT32 nRet = KchampvsInit();

	if (nRet == 0) {
		patch_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvKchampvs = {
	"kchampvs", "kchamp", NULL, NULL, "1984",
	"Karate Champ (US VS version, set 1)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, kchampvsRomInfo, kchampvsRomName, NULL, NULL, KchampInputInfo, KchampvsDIPInfo,
	KchampvsInit1, DrvExit, KchampvsFrame, KchampvsDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Karate Champ (US VS version, set 2)

static struct BurnRomInfo kchampvs2RomDesc[] = {
	{ "lt.d13",	0x2000, 0xeef41aa8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code #0 (Encrypted)
	{ "lt.d11",	0x2000, 0x091f810e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lt.d10",	0x2000, 0xd4df2a52, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lt.d8",	0x2000, 0x3d4ef0da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "lt.d7",	0x2000, 0x623a467b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "lt.d6",	0x4000, 0xc3bc6e46, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "lt.d4",	0x2000, 0xeaa646eb, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code #1
	{ "lt.d2",	0x2000, 0xd71031ad, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "lt.d1",	0x2000, 0x6f811c43, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "lt.k1",	0x2000, 0x4c574ecd, 3 | BRF_GRA },           //  9 Characters
	{ "lt.k3",	0x2000, 0x750b66af, 3 | BRF_GRA },           // 10
	{ "lt.k5",	0x2000, 0x9ad6227c, 3 | BRF_GRA },           // 11
	{ "lt.k6",	0x2000, 0x3b6d5de5, 3 | BRF_GRA },           // 12

	{ "lt.a1",	0x2000, 0x51eda56c, 4 | BRF_GRA },           // 13 Sprites
	{ "lt.c1",	0x2000, 0x593264cf, 4 | BRF_GRA },           // 14
	{ "lt.a3",	0x2000, 0xb4842ea9, 4 | BRF_GRA },           // 15
	{ "lt.c3",	0x2000, 0x8cd166a5, 4 | BRF_GRA },           // 16
	{ "lt.a5",	0x2000, 0x4cbd3aa3, 4 | BRF_GRA },           // 17
	{ "lt.c5",	0x2000, 0x6be342a6, 4 | BRF_GRA },           // 18
	{ "lt.a6",	0x2000, 0x8dcd271a, 4 | BRF_GRA },           // 19
	{ "lt.c6",	0x2000, 0x4ee1dba7, 4 | BRF_GRA },           // 20
	{ "lt.a8",	0x2000, 0x7346db8a, 4 | BRF_GRA },           // 21
	{ "lt.c8",	0x2000, 0xb78714fc, 4 | BRF_GRA },           // 22
	{ "lt.a10",	0x2000, 0xb2557102, 4 | BRF_GRA },           // 23
	{ "lt.c10",	0x2000, 0xc85aba0e, 4 | BRF_GRA },           // 24

	{ "lt.k10",	0x0100, 0xf683c54a, 5 | BRF_GRA },           // 25 Color data
	{ "lt.k9",	0x0100, 0x3ddbb6c4, 5 | BRF_GRA },           // 26
	{ "lt.k8",	0x0100, 0xba4a5651, 5 | BRF_GRA },           // 27
};

STD_ROM_PICK(kchampvs2)
STD_ROM_FN(kchampvs2)

struct BurnDriver BurnDrvKchampvs2 = {
	"kchampvs2", "kchamp", NULL, NULL, "1984",
	"Karate Champ (US VS version, set 2)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, kchampvs2RomInfo, kchampvs2RomName, NULL, NULL, KchampInputInfo, KchampvsDIPInfo,
	KchampvsInit, DrvExit, KchampvsFrame, KchampvsDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Taisen Karate Dou (Japan VS version)

static struct BurnRomInfo karatevsRomDesc[] = {
	{ "br24.d13",	0x2000, 0xea9cda49, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code #0 (Encrypted)
	{ "br23.d11",	0x2000, 0x46074489, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "br22.d10",	0x2000, 0x294f67ba, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "br21.d8",	0x2000, 0x934ea874, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "br20.d7",	0x2000, 0x97d7816a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "br19.d6",	0x4000, 0xdd2239d2, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "br18.d4",	0x2000, 0x00ccb8ea, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code #1
	{ "bs17.d2",	0x2000, 0xd71031ad, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "br16.d1",	0x2000, 0x2512d961, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "br12.k1",	0x2000, 0x9ed6f00d, 3 | BRF_GRA },           //  9 Characters
	{ "bs13.k3",	0x2000, 0x750b66af, 3 | BRF_GRA },           // 10
	{ "br14.k5",	0x2000, 0xfc399229, 3 | BRF_GRA },           // 11
	{ "bs15.k6",	0x2000, 0x3b6d5de5, 3 | BRF_GRA },           // 12

	{ "br00.a1",	0x2000, 0xc46a8b88, 4 | BRF_GRA },           // 13 Sprites
	{ "br06.c1",	0x2000, 0xcf8982ff, 4 | BRF_GRA },           // 14
	{ "bs01.a3",	0x2000, 0xb4842ea9, 4 | BRF_GRA },           // 15
	{ "bs07.c3",	0x2000, 0x8cd166a5, 4 | BRF_GRA },           // 16
	{ "bs02.a5",	0x2000, 0x4cbd3aa3, 4 | BRF_GRA },           // 17
	{ "bs08.c5",	0x2000, 0x6be342a6, 4 | BRF_GRA },           // 18
	{ "br03.a6",	0x2000, 0xbde8a52b, 4 | BRF_GRA },           // 19
	{ "br09.c6",	0x2000, 0xe9a5f945, 4 | BRF_GRA },           // 20
	{ "bs04.a8",	0x2000, 0x7346db8a, 4 | BRF_GRA },           // 21
	{ "bs10.c8",	0x2000, 0xb78714fc, 4 | BRF_GRA },           // 22
	{ "bs05.a10",	0x2000, 0xb2557102, 4 | BRF_GRA },           // 23
	{ "bs11.c10",	0x2000, 0xc85aba0e, 4 | BRF_GRA },           // 24

	{ "br27.k10",	0x0100, 0xf683c54a, 5 | BRF_GRA },           // 25 Color data
	{ "br26.k9",	0x0100, 0x3ddbb6c4, 5 | BRF_GRA },           // 26
	{ "br25.k8",	0x0100, 0xba4a5651, 5 | BRF_GRA },           // 27
};

STD_ROM_PICK(karatevs)
STD_ROM_FN(karatevs)

struct BurnDriver BurnDrvKaratevs = {
	"karatevs", "kchamp", NULL, NULL, "1984",
	"Taisen Karate Dou (Japan VS version)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, karatevsRomInfo, karatevsRomName, NULL, NULL, KchampInputInfo, KchampvsDIPInfo,
	KchampvsInit1, DrvExit, KchampvsFrame, KchampvsDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
