// FB Alpha Kick Boy / Dacholer / Itazura Tenshi driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm5205.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bgbank;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 music_interrupt_enable;
static UINT8 sound_interrupt_enable;
static UINT8 msm_toggle;
static UINT8 msm_data;
static UINT8 sound_ack;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 itaten = 0;
static INT32 dacholer = 0;

static struct BurnInputInfo DacholerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dacholer)

static struct BurnInputInfo ItatenInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Itaten)

static struct BurnDIPInfo DacholerDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x0f, NULL				},
	{0x01, 0xff, 0xff, 0x3d, NULL				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x00, 0x01, 0x0c, 0x00, "6 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x04, "5 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x08, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x01, 0x01, 0x03, 0x03, "1"				},
	{0x01, 0x01, 0x03, 0x02, "2"				},
	{0x01, 0x01, 0x03, 0x01, "3"				},
	{0x01, 0x01, 0x03, 0x00, "4"				},

	{0   , 0xfe, 0   ,    13, "Bonus Life"			},
	{0x01, 0x01, 0x3c, 0x3c, "20k 70k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x38, "30k 80k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x34, "40k 90k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x2c, "20k 90k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x28, "30k 100k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x24, "40k 110k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x1c, "20k 120k then every 100k"	},
	{0x01, 0x01, 0x3c, 0x18, "30k 130k then every 100k"	},
	{0x01, 0x01, 0x3c, 0x14, "40k 140k then every 100k"	},
	{0x01, 0x01, 0x3c, 0x0c, "20k only"			},
	{0x01, 0x01, 0x3c, 0x08, "30k only"			},
	{0x01, 0x01, 0x3c, 0x04, "40k only"			},
	{0x01, 0x01, 0x3c, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x40, 0x40, "Off"				},
	{0x01, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x01, 0x01, 0x80, 0x00, "Upright"			},
	{0x01, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(Dacholer)

static struct BurnDIPInfo KickboyDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x0f, NULL				},
	{0x01, 0xff, 0xff, 0x7d, NULL				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x0c, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x01, 0x01, 0x03, 0x03, "3"				},
	{0x01, 0x01, 0x03, 0x02, "4"				},
	{0x01, 0x01, 0x03, 0x01, "5"				},
	{0x01, 0x01, 0x03, 0x00, "99 (Cheat)"			},

	{0   , 0xfe, 0   ,    13, "Bonus Life"			},
	{0x01, 0x01, 0x3c, 0x3c, "20k 50k then every 30k"	},
	{0x01, 0x01, 0x3c, 0x38, "30k 60k then every 30k"	},
	{0x01, 0x01, 0x3c, 0x34, "40k 70k then every 30k"	},
	{0x01, 0x01, 0x3c, 0x2c, "20k 70k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x28, "30k 80k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x24, "40k 90k then every 50k"	},
	{0x01, 0x01, 0x3c, 0x1c, "20k 90k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x18, "30k 100k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x14, "40k 110k then every 70k"	},
	{0x01, 0x01, 0x3c, 0x0c, "20k only"			},
	{0x01, 0x01, 0x3c, 0x08, "30k only"			},
	{0x01, 0x01, 0x3c, 0x04, "40k only"			},
	{0x01, 0x01, 0x3c, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x01, 0x01, 0x40, 0x40, "Easy"				},
	{0x01, 0x01, 0x40, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x01, 0x01, 0x80, 0x00, "Upright"			},
	{0x01, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(Kickboy)

static struct BurnDIPInfo ItatenDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL				},
	{0x01, 0xff, 0xff, 0x3f, NULL				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x00, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x03, 0x03, "Easy"				},
	{0x01, 0x01, 0x03, 0x02, "Medium"			},
	{0x01, 0x01, 0x03, 0x01, "Hard"				},
	{0x01, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x01, 0x01, 0x0c, 0x0c, "3"				},
	{0x01, 0x01, 0x0c, 0x08, "4"				},
	{0x01, 0x01, 0x0c, 0x04, "5"				},
	{0x01, 0x01, 0x0c, 0x00, "6"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x30, 0x30, "30k then every 50k"		},
	{0x01, 0x01, 0x30, 0x20, "60k then every 50k"		},
	{0x01, 0x01, 0x30, 0x10, "30k then every 90k"		},
	{0x01, 0x01, 0x30, 0x00, "60k then every 90k"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x40, 0x40, "Off"				},
	{0x01, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x01, 0x01, 0x80, 0x00, "Upright"			},
	{0x01, 0x01, 0x80, 0x80, "Cocktail"			},
};

STDDIPINFO(Itaten)

static void __fastcall dacholer_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x20:
			// coins & leds
		return;

		case 0x21:
			bgbank = data & 3;
			flipscreen = data & 0x0c;
		return;

		case 0x22:
			scrollx = data;
		return;

		case 0x23:
			scrolly = data + 16;
		return;

		case 0x24:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x27:
		{
			soundlatch = data;
			ZetNmi(1);
		}
		return;
	}
}

static UINT8 __fastcall dacholer_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x03:
			return (DrvDips[0] & 0xef) | (sound_ack ? 0x10 : 0); // correct???

		case 0x04:
			return DrvDips[1];

		case 0x05:
			return 0; // nop
	}

	return 0;
}

static void __fastcall dacholer_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = 0;
		return;

		case 0x04:
			music_interrupt_enable = data;
		return;

		case 0x08:
			sound_interrupt_enable = data;
			if (data) MSM5205ResetWrite(0, 0);
		return;

		case 0x0c:
			sound_ack = data;
		return;

		case 0x80:
			msm_data = data;
			msm_toggle = 0;
		return;

		case 0x86:
		case 0x87:
			AY8910Write(0, ~port & 1, data);
		return;

		case 0x8a:
		case 0x8b:
			AY8910Write(1, ~port & 1, data);
		return;

		case 0x8e:
		case 0x8f:
			AY8910Write(2, ~port & 1, data);
		return;
	}
}

static UINT8 __fastcall dacholer_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return soundlatch;
	}

	return 0;
}

static void adpcm_int()
{
	if (sound_interrupt_enable == 1 || (sound_interrupt_enable == 0 && msm_toggle == 1))
	{
		MSM5205DataWrite(0, msm_data >> 4);

		msm_data <<= 4;
		msm_toggle ^= 1;

		if (msm_toggle == 0)
		{
			ZetSetVector(0x38);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	} else {
		MSM5205ResetWrite(0, 1);
	}
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	if (ZetGetActive() == -1) return 0;

	return (INT64)(double)ZetTotalCycles() * nSoundRate / 2496000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);
	MSM5205Reset();
	ZetClose();

	bgbank = 0;
	flipscreen = 0;
	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;
	music_interrupt_enable = 0;
	sound_interrupt_enable = 0;
	msm_toggle = 0;
	msm_data = 0;
	sound_ack = 0;
	MSM5205ResetWrite(0, 1);

	HiscoreReset();

	return 0;
}

static tilemap_callback( bg )
{
	INT32 Code = DrvBgRAM[offs] + (bgbank * 0x100);

	TILE_SET_INFO(1, Code, (itaten) ? 0 : 0x10, 0);
}

static tilemap_callback( fg )
{
	INT32 Code = DrvFgRAM[offs];

	TILE_SET_INFO(0, Code, 0, 0);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00a000;
	DrvZ80ROM1		= Next; Next += 0x006000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000100;
	DrvFgRAM		= Next; Next += 0x000400;
	DrvZ80RAM0		= Next; Next += 0x001800;
	DrvZ80RAM1		= Next; Next += 0x001800;
	DrvBgRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { 4,0,12,8,20,16,28,24,36,32,44,40,52,48,60,56 };
	INT32 YOffs0[8] = { STEP8(0,32) };
	INT32 YOffs1[16]= { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 4,  8,  8, Plane, XOffs, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x0400, 4,  8,  8, Plane, XOffs, YOffs0, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	BurnAllocMemIndex();

	if (type == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  6, 1)) return 1;
		
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00020, 15, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00040, 16, 1)) return 1;

		dacholer = 1;
	}
	else if (type == 1)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  6, 1)) return 1;
		
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 13, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00020, 14, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00040, 15, 1)) return 1;

		dacholer = 1; // kickboy (dacholer clone)
	}
	else if (type == 2)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  6, 1)) return 1;
		
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x06000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x06000, 15, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 16, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00020, 17, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00040, 18, 1)) return 1;

		itaten = 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvBgRAM,		0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xc400, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe0ff, MAP_RAM);

	if (itaten)
	{
		ZetMapMemory(DrvZ80ROM0 + 0x8000, 0x8000, 0x9fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,	  0xa000, 0xb7ff, MAP_RAM);
	}
	else
	{
		ZetMapMemory(DrvZ80RAM0,	  0x8800, 0x97ff, MAP_RAM);
	}

	ZetSetOutHandler(dacholer_main_write_port);
	ZetSetInHandler(dacholer_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xd000, 0xe7ff, MAP_RAM);
	ZetSetOutHandler(dacholer_sound_write_port);
	ZetSetInHandler(dacholer_sound_read_port);
	ZetClose();

	AY8910Init(0, 1248000, 0);
	AY8910Init(1, 1248000, 1);
	AY8910Init(2, 1248000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2496000);

	MSM5205Init(0, DrvSynchroniseStream, 384000, adpcm_int, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,  8,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x100 * 8 * 8, 0x00, 0x01);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4,  8,  8, 0x400 * 8 * 8, 0x00, 0x01);
	GenericTilemapSetTransparent(1, 0);
    GenericTilemapSetOffsets(1, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	MSM5205Exit();

	BurnFreeMemIndex();

	itaten = 0;
	dacholer = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0;i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit0 * 33 + bit1 * 71 + bit2 * 151; //bit0 * 151 + bit1 * 71 + bit2 * 33;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit0 * 33 + bit1 * 71 + bit2 * 151; //bit0 * 151 + bit1 * 71 + bit2 * 33;


		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit0 * 81 + bit1 * 174; //bit0 * 174 + bit1 * 81;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs + 1];
		INT32 attr  = DrvSprRAM[offs + 2];

		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;

		INT32 sx = (DrvSprRAM[offs + 3] - 128) + 256 * (attr & 0x01);
		INT32 sy = 255 - DrvSprRAM[offs];

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, 0, 4, 0, 0x10, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	INT32 sx = (flipscreen) ? (256 - scrollx) : scrollx;
	INT32 sy = (flipscreen) ? ((256 + 30) - scrolly) : scrolly;

	GenericTilemapSetScrollX(0, sx);
	GenericTilemapSetScrollY(0, sy);

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferFlip(flipscreen, flipscreen); // coctail unflippy
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
		UINT8 *DrvJoy[3] = { DrvJoy1, DrvJoy2, DrvJoy3 };
		UINT32 DrvJoyInit[3] = { 0x00, 0x00, 0xff };

		CompileInput(DrvJoy, (void*)DrvInputs, 3, 8, DrvJoyInit);

		if (dacholer) {
			// Convert to 4-way for Dacholer
			ProcessJoystick(&DrvInputs[0], 0, 0,1,2,3, INPUT_4WAY | INPUT_MAKEACTIVELOW | INPUT_CLEAROPPOSITES);
			ProcessJoystick(&DrvInputs[1], 1, 0,1,2,3, INPUT_4WAY | INPUT_MAKEACTIVELOW | INPUT_CLEAROPPOSITES);
		} else {
			ProcessJoystick(&DrvInputs[0], 0, 0,1,2,3, INPUT_MAKEACTIVELOW | INPUT_CLEAROPPOSITES);
			ProcessJoystick(&DrvInputs[1], 1, 0,1,2,3, INPUT_MAKEACTIVELOW | INPUT_CLEAROPPOSITES);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 2496000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	MSM5205NewFrame(0, 2496000, nInterleave);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 240 && music_interrupt_enable == 1) {
			ZetSetVector(0x30);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		MSM5205UpdateScanline(i);
		ZetClose();
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
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
		AY8910Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(bgbank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(music_interrupt_enable);
		SCAN_VAR(sound_interrupt_enable);
		SCAN_VAR(msm_toggle);
		SCAN_VAR(msm_data);
		SCAN_VAR(sound_ack);
	}

	return 0;
}


// Dacholer
/* AM-1-A & AM-1-B two board stack */

static struct BurnRomInfo dacholerRomDesc[] = {
	{ "dp_1.5k",	0x2000, 0x8b73a441, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "dp_2.5l",	0x2000, 0x9499289f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dp_3.5m",	0x2000, 0x39d37281, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dp_4.5n",	0x2000, 0xbb781ea4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ds_1.6g",	0x2000, 0xcc3a4b68, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "ds_2.6h",	0x2000, 0xaa18e126, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ds_3.6j",	0x2000, 0x3b0131c7, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "dc_7.12j",	0x2000, 0xfd649d36, 3 | BRF_GRA },           //  7 Characters

	{ "dc_3.13a",	0x2000, 0x9cca0fd2, 4 | BRF_GRA },           //  8 Background tiles
	{ "dc_2.12a",	0x2000, 0xc1322b27, 4 | BRF_GRA },           //  9
	{ "dc_1.11a",	0x2000, 0x9e1e7198, 4 | BRF_GRA },           // 10

	{ "dc_5.2d",	0x2000, 0xdd4818f0, 5 | BRF_GRA },           // 11 Sprites
	{ "dc_4.1d",	0x2000, 0x7f338ae0, 5 | BRF_GRA },           // 12
	{ "dc_6.3d",	0x2000, 0x0a6d4ec4, 5 | BRF_GRA },           // 13

	{ "dc.13d",		0x0020, 0xd273abe5, 6 | BRF_GRA },           // 14 Color data
	
	{ "af-2.1h",	0x0020, 0xe1cac297, 0 | BRF_OPT },           // 15 Unknown data
	{ "af-1.3n",	0x0020, 0x5638e485, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(dacholer)
STD_ROM_FN(dacholer)

static INT32 DacholerInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvDacholer = {
	"dacholer", NULL, NULL, NULL, "1983",
	"Dacholer\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, dacholerRomInfo, dacholerRomName, NULL, NULL, NULL, NULL, DacholerInputInfo, DacholerDIPInfo,
	DacholerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};


// Kick Boy

static struct BurnRomInfo kickboyRomDesc[] = {
	{ "k_1.5k",		0x2000, 0x525746f1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "k_2.5l",		0x2000, 0x9d091725, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k_3.5m",		0x2000, 0xd61b6ff6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k_4.5n",		0x2000, 0xa8985bfe, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "k_1.6g",		0x2000, 0xcc3a4b68, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "k_2.6h",		0x2000, 0xaa18e126, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "k_3.6j",		0x2000, 0x3b0131c7, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "k_7.12j",	0x2000, 0x22be46e8, 3 | BRF_GRA },           //  7 Characters

	{ "k_3.13a",	0x2000, 0x7eac2a64, 4 | BRF_GRA },           //  8 Background tiles
	{ "k_2.12a",	0x2000, 0xb8829572, 4 | BRF_GRA },           //  9

	{ "k_5.2d",		0x2000, 0x4b769a1c, 5 | BRF_GRA },           // 10 Sprites
	{ "k_4.1d",		0x2000, 0x45199750, 5 | BRF_GRA },           // 11
	{ "k_6.3d",		0x2000, 0xd1795506, 5 | BRF_GRA },           // 12

	{ "k.13d",		0x0020, 0x82f87a36, 6 | BRF_GRA },           // 13 Color data
	
	{ "af-2.1h",	0x0020, 0xe1cac297, 0 | BRF_OPT },           // 14 Unknown data
	{ "af-1.3n",	0x0020, 0x5638e485, 0 | BRF_OPT },           // 15
};

STD_ROM_PICK(kickboy)
STD_ROM_FN(kickboy)

static INT32 KickboyInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvKickboy = {
	"kickboy", NULL, NULL, NULL, "1983",
	"Kick Boy\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, kickboyRomInfo, kickboyRomName, NULL, NULL, NULL, NULL, DacholerInputInfo, KickboyDIPInfo,
	KickboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};


// Itazura Tenshi (Japan)

static struct BurnRomInfo itatenRomDesc[] = {
	{ "1.5k",		0x2000, 0x84c8a010, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.5l",		0x2000, 0x19946038, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.5m",		0x2000, 0x4f9e26fd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.1f",		0x2000, 0x35f85aeb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.1e",		0x2000, 0x6cf30924, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.6g",		0x2000, 0xdfcb1a3e, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "7.6h",		0x1000, 0x844e78d6, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "16.12j",		0x2000, 0x8af2bfb8, 3 | BRF_GRA },           //  7 Characters

	{ "11.13a",		0x2000, 0xed3279d5, 4 | BRF_GRA },           //  8 Background tiles
	{ "10.12a",		0x2000, 0xd2b60e5d, 4 | BRF_GRA },           //  9
	{ "9.11a",		0x2000, 0x919cac5e, 4 | BRF_GRA },           // 10
	{ "8.10a",		0x2000, 0xc32b0859, 4 | BRF_GRA },           // 11

	{ "13.2d",		0x2000, 0xd32559f5, 5 | BRF_GRA },           // 12 Sprites
	{ "12.1d",		0x2000, 0xf0f64636, 5 | BRF_GRA },           // 13
	{ "14.3d",		0x2000, 0x8c532c74, 5 | BRF_GRA },           // 14
	{ "15.4d",		0x2000, 0xd119b483, 5 | BRF_GRA },           // 15

	{ "af-3.13d",	0x0020, 0x875429ba, 6 | BRF_GRA },           // 16 Color data

	{ "af-2.1h",	0x0020, 0xe1cac297, 0 | BRF_OPT },           // 17 Unknown data
	{ "af-1.3n",	0x0020, 0x5638e485, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(itaten)
STD_ROM_FN(itaten)

static INT32 ItatenInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvItaten = {
	"itaten", NULL, NULL, NULL, "1984",
	"Itazura Tenshi (Japan)\0", NULL, "Nichibutsu / Alice", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, itatenRomInfo, itatenRomName, NULL, NULL, NULL, NULL, ItatenInputInfo, ItatenDIPInfo,
	ItatenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	256, 224, 4, 3
};
