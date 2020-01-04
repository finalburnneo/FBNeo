// FB Alpha Time Pilot '84 driver module
// Based on MAME driver by Marc Lafontaine

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "flt_rc.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvTransTable;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprBuf;
static UINT8 *DrvShareRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8   DrvRecalc;

static INT16 *pSoundBuffer[3];

static UINT8 palettebank;
static UINT8 flipscreenx;
static UINT8 flipscreeny;
static UINT8 soundlatch;
static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 sub_irqmask;

static INT32 scanline;
static INT32 watchdog;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Tp84InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tp84)

static struct BurnDIPInfo Tp84DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x52, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "2"			},
	{0x13, 0x01, 0x03, 0x02, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "10000 and every 50000"},
	{0x13, 0x01, 0x18, 0x10, "20000 and every 60000"},
	{0x13, 0x01, 0x18, 0x08, "30000 and every 70000"},
	{0x13, 0x01, 0x18, 0x00, "40000 and every 80000"},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Tp84)

static struct BurnDIPInfo Tp84aDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x52, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x02, "4"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "10000 and every 50000"},
	{0x13, 0x01, 0x18, 0x10, "20000 and every 60000"},
	{0x13, 0x01, 0x18, 0x08, "30000 and every 70000"},
	{0x13, 0x01, 0x18, 0x00, "40000 and every 80000"},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x60, "Easy"			},
	{0x13, 0x01, 0x60, 0x40, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Tp84a)

static void tp84_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			watchdog = 0;
		return;

		case 0x2800:
			palettebank = data;
		return;

		case 0x3000:
		return;

		case 0x3004:
			flipscreenx = data & 0x01;
		return;

		case 0x3005:
			flipscreeny = data & 0x01;
		return;

		case 0x3800:
			ZetOpen(0);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
		return;

		case 0x3a00:
			soundlatch = data;
		return;

		case 0x3c00:
			scrollx = data;
		return;

		case 0x3e00:
			scrolly = data;
		return;
	}
}

static UINT8 tp84_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x2800:
			return DrvInputs[0];

		case 0x2820:
			return DrvInputs[1];

		case 0x2840:
			return DrvInputs[2];

		case 0x2860:
			return DrvDips[0];

		case 0x3000:
			return DrvDips[1];
	}

	return 0;
}

static void tp84b_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1800:
			watchdog = 0;
		return;

		case 0x1a00:
			palettebank = data;
		return;

		case 0x1c00:
		return;

		case 0x1c04:
			flipscreenx = data & 0x01;
		return;

		case 0x1c05:
			flipscreeny = data & 0x01;
		return;

		case 0x1e00:
			ZetOpen(0);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
		return;

		case 0x1e80:
			soundlatch = data;
		return;

		case 0x1f00:
			scrollx = data;
		return;

		case 0x1f80:
			scrolly = data;
		return;
	}
}

static UINT8 tp84b_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1a00:
			return DrvInputs[0];

		case 0x1a20:
			return DrvInputs[1];

		case 0x1a40:
			return DrvInputs[2];

		case 0x1a60:
			return DrvDips[0];

		case 0x1c00:
			return DrvDips[1];
	}

	return 0;
}

static void tp84_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			sub_irqmask = data & 0x01;
		return;
	}
}

static UINT8 tp84_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
			return (M6809TotalCycles() / 100); // ((1536000 / 256) / 60)
	}

	return 0;
}

static void __fastcall tp84_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xa000) {
		INT32 C = 0;
		if (address & 0x008) C +=  47000;    //  47000pF = 0.047uF
		if (address & 0x010) C += 470000;    // 470000pF = 0.47uF

		filter_rc_set_RC(0, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(C));

		C = 0;
		if (address & 0x080) C += 470000;    // 470000pF = 0.47uF
		filter_rc_set_RC(1, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(C));

		C = 0;
		if (address & 0x100) C += 470000;    // 470000pF = 0.47uF
		filter_rc_set_RC(2, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(C));
		return;
	}

	switch (address)
	{
		case 0xc000:
		return;		// nop

		case 0xc001:
			SN76496Write(0, data);
		return;

		case 0xc003:
			SN76496Write(1, data);
		return;

		case 0xc004:
			SN76496Write(2, data);
		return;
	}
}

static UINT8 __fastcall tp84_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return soundlatch;

		case 0x8000:
			return (ZetTotalCycles() >> 10) & 0xf;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	SN76496Reset();

	palettebank = 0;
	flipscreenx = 0;
	flipscreeny = 0;
	soundlatch = 0;
	scrollx = 0;
	scrolly = 0;
	sub_irqmask = 0;

	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { ((0x8000/2) * 8) + 4, ((0x8000/2) * 8) + 0, 4, 0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x00400, 2,  8,  8, Plane + 2, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x00100, 4, 16, 16, Plane + 0, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x008000;
	DrvM6809ROM1		= Next; Next += 0x002000;
	DrvZ80ROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvTransTable		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	DrvSprBuf		= Next; Next += 0x60 * 256;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvColRAM0		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;
	DrvVidRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvShareRAM		= Next; Next += 0x000800;

	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	pSoundBuffer[0]		= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16) * 2;
	pSoundBuffer[1]		= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16) * 2;
	pSoundBuffer[2]		= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16) * 2;

	MemEnd			= Next;

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
		if (BurnLoadRom(DrvM6809ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x06000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x02000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x06000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00300, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00400, 16, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM0,		0x4000, 0x43ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1,		0x4400, 0x47ff, MAP_RAM);
	M6809MapMemory(DrvColRAM0,		0x4800, 0x4bff, MAP_RAM);
	M6809MapMemory(DrvColRAM1,		0x4c00, 0x4fff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x5000, 0x57ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tp84_main_write);
	M6809SetReadHandler(tp84_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvSprRAM,		0x6000, 0x67ff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x8000, 0x87ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tp84_sub_write);
	M6809SetReadHandler(tp84_sub_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(tp84_sound_write);
	ZetSetReadHandler(tp84_sound_read);
	ZetClose();

	SN76489AInit(0, 3579545/2, 0);
	SN76489AInit(1, 3579545/2, 0);
	SN76489AInit(2, 3579545/2, 0);
	SN76496SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.75, BURN_SND_ROUTE_BOTH);

    SN76496SetBuffered(ZetTotalCycles, 3579545);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 1);

	filter_rc_set_src_gain(0, 0.55);
	filter_rc_set_src_gain(1, 0.55);
	filter_rc_set_src_gain(2, 0.55);
	filter_rc_set_src_stereo(0);
	filter_rc_set_src_stereo(1);
	filter_rc_set_src_stereo(2);

	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvbInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x04000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00300, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00400, 11, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM0,		0x0000, 0x03ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1,		0x0400, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvColRAM0,		0x0800, 0x0bff, MAP_RAM);
	M6809MapMemory(DrvColRAM1,		0x0c00, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tp84b_main_write);
	M6809SetReadHandler(tp84b_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvSprRAM,		0x6000, 0x67ff, MAP_RAM);
	M6809MapMemory(DrvShareRAM,		0x8000, 0x87ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tp84_sub_write);
	M6809SetReadHandler(tp84_sub_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x43ff, MAP_RAM);
	ZetSetWriteHandler(tp84_sound_write);
	ZetSetReadHandler(tp84_sound_read);
	ZetClose();

	SN76489AInit(0, 3579545/2, 0);
	SN76489AInit(1, 3579545/2, 0);
	SN76489AInit(2, 3579545/2, 0);
	SN76496SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.75, BURN_SND_ROUTE_BOTH);

    SN76496SetBuffered(ZetTotalCycles, 3579545);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 2200, 1000, CAP_P(0), 1);

	filter_rc_set_src_gain(0, 0.55);
	filter_rc_set_src_gain(1, 0.55);
	filter_rc_set_src_gain(2, 0.55);
	filter_rc_set_src_stereo(0);
	filter_rc_set_src_stereo(1);
	filter_rc_set_src_stereo(2);

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

	SN76496Exit();
	filter_rc_exit();

	BurnFree (AllMem);
	AllMem = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = ((bit0 * 1424) + (bit1 * 3134) + (bit2 * 6696) + (bit3 * 14246) + 50) / 100;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = ((bit0 * 1424) + (bit1 * 3134) + (bit2 * 6696) + (bit3 * 14246) + 50) / 100;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = ((bit0 * 1424) + (bit1 * 3134) + (bit2 * 6696) + (bit3 * 14246) + 50) / 100;

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			UINT8 ctabentry = ((~i & 0x100) >> 1) | (j << 4) | (DrvColPROM[i + 0x300] & 0x0f);
			DrvPalette[((i & 0x100) << 3) | (j << 8) | (i & 0xff)] = pens[ctabentry];

			DrvTransTable[((i & 0x100) << 3) | (j << 8) | (i & 0xff)] = DrvColPROM[i + 0x300] & 0x0f;
		}
	}
}

static void draw_bg_layer()
{
	INT32 palbank = ((palettebank & 0x07) << 6) | ((palettebank & 0x18) << 1);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 256;
		sy -= scrolly;
		if (sy < -7) sy += 256;

		INT32 attr  = DrvColRAM0[offs];
		INT32 code  = DrvVidRAM0[offs] | ((attr & 0x30) << 4);
		INT32 color = (attr & 0x0f) | palbank;
		INT32 flipy =  attr & 0x80;
		INT32 flipx =  attr & 0x40;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_fg_layer()
{
	INT32 palbank = ((palettebank & 0x07) << 6) | ((palettebank & 0x18) << 1);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (sx >= 16 && sx < 240) continue;

		INT32 attr  = DrvColRAM1[offs];
		INT32 code  = DrvVidRAM1[offs] | ((attr & 0x30) << 4);
		INT32 color = (attr & 0x0f) | palbank;
		INT32 flipy =  attr & 0x80;
		INT32 flipx =  attr & 0x40;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy - 16, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	INT32 palbank = ((palettebank & 0x07) << 4);

	for (INT32 line = 16; line < 240; line++)
	{
		UINT8 *sprram = DrvSprBuf + line * 0x60;
		UINT16 *dst = pTransDraw + ((line - 16) * nScreenWidth);

		for (INT32 offs = 0x5c; offs >= 0; offs -= 4)
		{
			INT32 sy     = 240 - sprram[offs + 3];
			if (sy < (line-15) || sy > line) continue;

			INT32 sx     = sprram[offs + 0];
			INT32 code   = sprram[offs + 1];
			INT32 attr   = sprram[offs + 2];
			INT32 color  = (attr & 0x0f) | palbank | 0x80;
			INT32 flipx  = ~attr & 0x40;
			INT32 flipy  =  attr & 0x80;

			{
				UINT8 *gfx = DrvGfxROM1 + (code * 0x100);
				color = (color << 4);
				flipx = (flipx ? 0x0f : 0);
				flipy = (flipy ? 0xf0 : 0);
				gfx += (((line - sy) * 0x10) ^ flipy);

				for (INT32 x = 0; x < 16; x++, sx++)
				{
					if (sx < 0 || sx >= nScreenWidth) continue;

					INT32 pxl = gfx[x^flipx]+color;

					if (DrvTransTable[pxl]) {
						dst[sx] = pxl;
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) draw_bg_layer();
	if ( nSpriteEnable & 1) draw_sprites();
	if ( nBurnLayer & 2) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog == 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
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
	INT32 nCyclesTotal[3] = { 1536000 / 60, 1536000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		M6809Open(0);
		INT32 nSegment = ((i + 1) * nCyclesTotal[0]) / nInterleave;
		nCyclesDone[0] += M6809Run(nSegment - nCyclesDone[0]);
		if (i == 240) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6809Close();

		M6809Open(1);
		nSegment = ((i + 1) * nCyclesTotal[1]) / nInterleave;
		nCyclesDone[1] += M6809Run(nSegment - nCyclesDone[1]);
		if (i == 240 && sub_irqmask) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6809Close();

		ZetOpen(0);
		nSegment = ((i + 1) * nCyclesTotal[2]) / nInterleave;
		nCyclesDone[2] += ZetRun(nSegment - nCyclesDone[2]);
		ZetClose();

		memcpy (DrvSprBuf + i * 0x60, DrvSprRAM + 0x7a0, 0x60);

	}

    if (pBurnSoundOut) {
        BurnSoundClear();

        SN76496Update(0, pSoundBuffer[0], nBurnSoundLen);
        SN76496Update(1, pSoundBuffer[1], nBurnSoundLen);
        SN76496Update(2, pSoundBuffer[2], nBurnSoundLen);

        filter_rc_update(0, pSoundBuffer[0], pBurnSoundOut, nBurnSoundLen);
        filter_rc_update(1, pSoundBuffer[1], pBurnSoundOut, nBurnSoundLen);
        filter_rc_update(2, pSoundBuffer[2], pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);

		SCAN_VAR(palettebank);
		SCAN_VAR(flipscreenx);
		SCAN_VAR(flipscreeny);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(sub_irqmask);
	}

	return 0;
}


// Time Pilot '84 (set 1)

static struct BurnRomInfo tp84RomDesc[] = {
	{ "388_f04.7j",		0x2000, 0x605f61c7, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "388_05.8j",		0x2000, 0x4b4629a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "388_f06.9j",		0x2000, 0xdbd5333b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "388_07.10j",		0x2000, 0xa45237c4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "388_f08.10d",	0x2000, 0x36462ff1, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "388j13.6a",		0x2000, 0xc44414da, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "388_h02.2j",		0x2000, 0x05c7508f, 4 | BRF_GRA },           //  6 Characters
	{ "388_d01.1j",		0x2000, 0x498d90b7, 4 | BRF_GRA },           //  7

	{ "388_e09.12a",	0x2000, 0xcd682f30, 5 | BRF_GRA },           //  8 Sprites
	{ "388_e10.13a",	0x2000, 0x888d4bd6, 5 | BRF_GRA },           //  9
	{ "388_e11.14a",	0x2000, 0x9a220b39, 5 | BRF_GRA },           // 10
	{ "388_e12.15a",	0x2000, 0xfac98397, 5 | BRF_GRA },           // 11

	{ "388d14.2c",		0x0100, 0xd737eaba, 6 | BRF_GRA },           // 12 Color PROMs
	{ "388d15.2d",		0x0100, 0x2f6a9a2a, 6 | BRF_GRA },           // 13
	{ "388d16.1e",		0x0100, 0x2e21329b, 6 | BRF_GRA },           // 14
	{ "388d18.1f",		0x0100, 0x61d2d398, 6 | BRF_GRA },           // 15
	{ "388j17.16c",		0x0100, 0x13c4e198, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(tp84)
STD_ROM_FN(tp84)

struct BurnDriver BurnDrvTp84 = {
	"tp84", NULL, NULL, NULL, "1984",
	"Time Pilot '84 (set 1)\0", NULL, "Konami", "GX388",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, tp84RomInfo, tp84RomName, NULL, NULL, NULL, NULL, Tp84InputInfo, Tp84DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Time Pilot '84 (set 2)

static struct BurnRomInfo tp84aRomDesc[] = {
	{ "388_f04.7j",		0x2000, 0x605f61c7, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "388_f05.8j",		0x2000, 0xe97d5093, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "388_f06.9j",		0x2000, 0xdbd5333b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "388_f07.10j",	0x2000, 0x8fbdb4ef, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "388_f08.10d",	0x2000, 0x36462ff1, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "388j13.6a",		0x2000, 0xc44414da, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "388_h02.2j",		0x2000, 0x05c7508f, 4 | BRF_GRA },           //  6 Characters
	{ "388_d01.1j",		0x2000, 0x498d90b7, 4 | BRF_GRA },           //  7

	{ "388_e09.12a",	0x2000, 0xcd682f30, 5 | BRF_GRA },           //  8 Sprites
	{ "388_e10.13a",	0x2000, 0x888d4bd6, 5 | BRF_GRA },           //  9
	{ "388_e11.14a",	0x2000, 0x9a220b39, 5 | BRF_GRA },           // 10
	{ "388_e12.15a",	0x2000, 0xfac98397, 5 | BRF_GRA },           // 11

	{ "388d14.2c",		0x0100, 0xd737eaba, 6 | BRF_GRA },           // 12 Color PROMs
	{ "388d15.2d",		0x0100, 0x2f6a9a2a, 6 | BRF_GRA },           // 13
	{ "388d16.1e",		0x0100, 0x2e21329b, 6 | BRF_GRA },           // 14
	{ "388d18.1f",		0x0100, 0x61d2d398, 6 | BRF_GRA },           // 15
	{ "388d17.16c",		0x0100, 0xaf8f839c, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(tp84a)
STD_ROM_FN(tp84a)

struct BurnDriver BurnDrvTp84a = {
	"tp84a", "tp84", NULL, NULL, "1984",
	"Time Pilot '84 (set 2)\0", NULL, "Konami", "GX388",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, tp84aRomInfo, tp84aRomName, NULL, NULL, NULL, NULL, Tp84InputInfo, Tp84aDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Time Pilot '84 (set 3)

static struct BurnRomInfo tp84bRomDesc[] = {
	{ "388j05.8j",		0x4000, 0xa59e2fda, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "388j07.10j",		0x4000, 0xd25d18e6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "388j08.10d",		0x2000, 0x2aea6b42, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "388j13.6a",		0x2000, 0xc44414da, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "388j02.2j",		0x4000, 0xe1225f53, 4 | BRF_GRA },           //  4 Characters

	{ "388j09.12a",		0x4000, 0xaec90936, 5 | BRF_GRA },           //  5 Sprites
	{ "388j11.14a",		0x4000, 0x29257f03, 5 | BRF_GRA },           //  6

	{ "388j14.2c",		0x0100, 0xd737eaba, 6 | BRF_GRA },           //  7 Color PROMs
	{ "388j15.2d",		0x0100, 0x2f6a9a2a, 6 | BRF_GRA },           //  8
	{ "388j16.1e",		0x0100, 0x2e21329b, 6 | BRF_GRA },           //  9
	{ "388j18.1f",		0x0100, 0x61d2d398, 6 | BRF_GRA },           // 10
	{ "388j17.16c",		0x0100, 0x13c4e198, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(tp84b)
STD_ROM_FN(tp84b)

struct BurnDriver BurnDrvTp84b = {
	"tp84b", "tp84", NULL, NULL, "1984",
	"Time Pilot '84 (set 3)\0", NULL, "Konami", "GX388",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, tp84bRomInfo, tp84bRomName, NULL, NULL, NULL, NULL, Tp84InputInfo, Tp84DIPInfo,
	DrvbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};
