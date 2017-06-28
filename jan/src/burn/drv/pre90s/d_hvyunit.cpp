// FB Alpha Heavy Unit driver module
// Based on MAME drivery Angelo Salese, Tomasz Slanina, and David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mermaid.h"
#include "burn_ym2203.h"
#include "pandora.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPandoraRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT16 scrollx;
static UINT16 scrolly;
static UINT8 soundlatch;
static UINT8 z80banks[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;

static struct BurnInputInfo HvyunitInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 5,	"dip"		},
};

STDINPUTINFO(Hvyunit)

static struct BurnDIPInfo HvyunitDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfe, NULL			},
	{0x12, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    1, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x00, "Upright"		},
//	{0x11, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x02, 0x02, "Off"			},
	{0x11, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x11, 0x01, 0x08, 0x08, "Mode 1"		},
	{0x11, 0x01, 0x08, 0x00, "Mode 2"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "1 Coin  6 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},
	{0x11, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"			},
	{0x12, 0x01, 0x03, 0x03, "Normal"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"			},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Bonus"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "3"			},
	{0x12, 0x01, 0x30, 0x20, "4"			},
	{0x12, 0x01, 0x30, 0x10, "5"			},
	{0x12, 0x01, 0x30, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Hvyunit)

static struct BurnDIPInfo HvyunitjDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfe, NULL			},
	{0x12, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    1, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x00, "Upright"		},
//	{0x11, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x02, 0x02, "Off"			},
	{0x11, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"			},
	{0x12, 0x01, 0x03, 0x03, "Normal"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"			},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Bonus"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "3"			},
	{0x12, 0x01, 0x30, 0x20, "4"			},
	{0x12, 0x01, 0x30, 0x10, "5"			},
	{0x12, 0x01, 0x30, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Hvyunitj)

static void __fastcall hvyunit_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xc000) {
		DrvSprRAM[address & 0xfff] = data;

		address = (address & 0x800) | ((address & 0xff) << 3) | ((address & 0x700) >> 8);

		DrvPandoraRAM[address] = data;
		return;
	}
}

static void master_bankswitch(INT32 data)
{
	z80banks[0] = data;
	INT32 bank = (data & 7) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + bank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall hvyunit_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			master_bankswitch(data);
		return;

		case 0x02:
		{
			ZetClose();
			ZetOpen(1);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
		}
		return;
	}
}

static void slave_bankswitch(INT32 data)
{
	z80banks[1] = data;
	INT32 bank = (data & 0x03) * 0x4000;

	ZetMapMemory(DrvZ80ROM1 + bank,	0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall hvyunit_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			scrollx = (scrollx & 0xff) | ((data & 0x40) << 2);
			scrolly = (scrolly & 0xff) | ((data & 0x80) << 1);
			slave_bankswitch(data);
		return;

		case 0x02:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			ZetNmi();
			ZetClose();
			ZetOpen(1);
		}
		return;

		case 0x04:
			mermaidWrite(data);
		return;

		case 0x06:
			scrolly = (scrolly & 0x100) | data;
		return;

		case 0x08:
			scrollx = (scrollx & 0x100) | data;
		return;

		case 0x0e:
			// coin counter
		return;
	}
}

static UINT8 __fastcall hvyunit_sub_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return mermaidRead();

		case 0x0c:
			return mermaidStatus();
	}

	return 0;
}

static void sound_bankswitch(INT32 data)
{
	z80banks[2] = data;
	INT32 bank = (data & 0x03) * 0x4000;

	ZetMapMemory(DrvZ80ROM2 + bank,	0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall hvyunit_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			sound_bankswitch(data);
		return;

		case 0x02:
		case 0x03:
			BurnYM2203Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall hvyunit_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x03:
			return BurnYM2203Read(0, port & 1);

		case 0x04:
			return soundlatch;
	}
	
	return 0;
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 6000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	master_bankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	slave_bankswitch(0);
	ZetClose();

	ZetOpen(2);
	ZetReset();
	sound_bankswitch(0);
	BurnYM2203Reset();
	ZetClose();

	mermaidReset();

	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]  = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x010000;

	DrvMCUROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x001000;
	DrvPandoraRAM		= Next; Next += 0x001000;
	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvShareRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvZ80RAM2		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	BurnSetRefreshRate(58);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x000000,  3, 1)) return 1;

		switch (select)
		{
			case 0: // hvyunit, hvyunitjo
			{
				if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x120000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x140000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x160000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x180000,  9, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1a0000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1c0000, 11, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000,  12, 1)) return 1;
			}
			break;

			case 1: // hvyunitj
			{
				if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x110000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x120000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x130000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x140000,  9, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x150000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x160000, 11, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000,  12, 1)) return 1;
			}
			break;

			case 2: // hvyunitu
			{
				if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x120000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x140000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x160000,  8, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000,   9, 1)) return 1;
			}
			break;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(hvyunit_main_write);
	//ZetSetReadHandler(hvyunit_main_read);
	ZetSetOutHandler(hvyunit_main_write_port);
	//ZetSetInHandler(hvyunit_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xc400, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM + 0x200,	0xd000, 0xd1ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xd800, 0xd9ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(hvyunit_sub_write_port);
	ZetSetInHandler(hvyunit_sub_read_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetOutHandler(hvyunit_sound_write_port);
	ZetSetInHandler(hvyunit_sound_read_port);
	ZetClose();

	mermaidInit(DrvMCUROM, DrvInputs);

	BurnYM2203Init(1, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	pandora_init(DrvPandoraRAM, DrvGfxROM0, (0x400000/0x100)-1, 0x100, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	pandora_exit();

	GenericTilesExit();

	ZetExit();
	mermaidExit();

	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvRecalcPalette()
{
	for (INT32 i = 0; i < 0x400/2; i++) {
		INT32 r = DrvPalRAM[0x200+i] & 0xf;
		INT32 g = DrvPalRAM[i] >> 4;
		INT32 b = DrvPalRAM[i] & 0x0f;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 16;

		sx -= (scrollx + 96) & 0x1ff;
		sy -= (scrolly + 16);
		if (sx < -15) sx += 512;
		if (sy < -15) sy += 512;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code  = DrvVidRAM[offs] | (DrvColRAM[offs] << 8);
		INT32 color = code >> 12;

		Render16x16Tile_Clip(pTransDraw, code & 0xfff, sx, sy, color, 4, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	DrvRecalcPalette();

	draw_layer();

	pandora_flipscreen = 0;
	pandora_update(pTransDraw);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	// mermaid new frame

	{
		memset (DrvInputs, 0xff, 4);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[2] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[0] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] =  { 6000000 / 58, 6000000 / 58, 6000000 / 58, 6000000 / 58 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {

		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 64) {
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 240) {
			ZetSetVector(0xfd);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		nSegment = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		nSegment -= ZetTotalCycles();
		if (mermaid_sub_z80_reset) {
			nCyclesDone[1] += nSegment;
			ZetIdle(nSegment);
		} else {
			nCyclesDone[1] += ZetRun(nSegment);
			if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		nSegment = ZetTotalCycles();
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdate(nSegment);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		nCyclesDone[3] += mermaidRun(nSegment - nCyclesDone[3]);

		if (i == 239)
			pandora_buffer_sprites();
	}

	ZetOpen(2);
	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
	if (pBurnDraw) {
		DrvDraw();
	}

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
		mermaidScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(z80banks);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		master_bankswitch(z80banks[0]);
		ZetClose();

		ZetOpen(1);
		slave_bankswitch(z80banks[1]);
		ZetClose();

		ZetOpen(2);
		sound_bankswitch(z80banks[2]);
		ZetClose();
	}

	return 0;
}


// Heavy Unit (World)

static struct BurnRomInfo hvyunitRomDesc[] = {
	{ "b73_10.5c",		0x20000, 0xca52210f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b73_11.5p",		0x10000, 0xcb451695, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b73_12.7e",		0x10000, 0xd1d24fab, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "mermaid.bin",	0x00e00, 0x88c5dd27, 4 | BRF_PRG | BRF_ESS }, //  3 I80C51 (mermaid) Code

	{ "b73_08.2f",		0x80000, 0xf83dd808, 5 | BRF_GRA },           //  4 Sprites
	{ "b73_07.2c",		0x10000, 0x5cffa42c, 5 | BRF_GRA },           //  5
	{ "b73_06.2b",		0x10000, 0xa98e4aea, 5 | BRF_GRA },           //  6
	{ "b73_01.1b",		0x10000, 0x3a8a4489, 5 | BRF_GRA },           //  7
	{ "b73_02.1c",		0x10000, 0x025c536c, 5 | BRF_GRA },           //  8
	{ "b73_03.1d",		0x10000, 0xec6020cf, 5 | BRF_GRA },           //  9
	{ "b73_04.1f",		0x10000, 0xf7badbb2, 5 | BRF_GRA },           // 10
	{ "b73_05.1h",		0x10000, 0xb8e829d2, 5 | BRF_GRA },           // 11

	{ "b73_09.2p",		0x80000, 0x537c647f, 6 | BRF_GRA },           // 12 Background Tiles
};

STD_ROM_PICK(hvyunit)
STD_ROM_FN(hvyunit)

static INT32 hvyunitInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvHvyunit = {
	"hvyunit", NULL, NULL, NULL, "1988",
	"Heavy Unit (World)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_MISC, GBF_VERSHOOT, 0,
	NULL, hvyunitRomInfo, hvyunitRomName, NULL, NULL, HvyunitInputInfo, HvyunitDIPInfo,
	hvyunitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Heavy Unit (Japan, Newer)

static struct BurnRomInfo hvyunitjRomDesc[] = {
	{ "b73_30.5c",		0x20000, 0x600af545, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b73_14.5p",		0x10000, 0x0dfb51d4, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b73_12.7e",		0x10000, 0xd1d24fab, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "mermaid.bin",	0x00e00, 0x88c5dd27, 4 | BRF_PRG | BRF_ESS }, //  3 I80C51 (mermaid) Code

	{ "b73_08.2f",		0x80000, 0xf83dd808, 5 | BRF_GRA },           //  4 Sprites
	{ "b73_07.2c",		0x10000, 0x5cffa42c, 5 | BRF_GRA },           //  5
	{ "b73_06.2b",		0x10000, 0xa98e4aea, 5 | BRF_GRA },           //  6
	{ "b73_01.1b",		0x10000, 0x3a8a4489, 5 | BRF_GRA },           //  7
	{ "b73_02.1c",		0x10000, 0x025c536c, 5 | BRF_GRA },           //  8
	{ "b73_03.1d",		0x10000, 0xec6020cf, 5 | BRF_GRA },           //  9
	{ "b73_04.1f",		0x10000, 0xf7badbb2, 5 | BRF_GRA },           // 10
	{ "b73_05.1h",		0x10000, 0xb8e829d2, 5 | BRF_GRA },           // 11

	{ "b73_09.2p",		0x80000, 0x537c647f, 6 | BRF_GRA },           // 12 Background Tiles
};

STD_ROM_PICK(hvyunitj)
STD_ROM_FN(hvyunitj)

static INT32 hvyunitjInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvHvyunitj = {
	"hvyunitj", "hvyunit", NULL, NULL, "1988",
	"Heavy Unit (Japan, Newer)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_VERSHOOT, 0,
	NULL, hvyunitjRomInfo, hvyunitjRomName, NULL, NULL, HvyunitInputInfo, HvyunitjDIPInfo,
	hvyunitjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Heavy Unit (Japan, Older)

static struct BurnRomInfo hvyunitjoRomDesc[] = {
	{ "b73_13.5c",		0x20000, 0xe2874601, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b73_14.5p",		0x10000, 0x0dfb51d4, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b73_12.7e",		0x10000, 0xd1d24fab, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "mermaid.bin",	0x00e00, 0x88c5dd27, 4 | BRF_PRG | BRF_ESS }, //  3 I80C51 (mermaid) Code

	{ "b73_08.2f",		0x80000, 0xf83dd808, 5 | BRF_GRA },           //  4 Sprites
	{ "b73_07.2c",		0x10000, 0x5cffa42c, 5 | BRF_GRA },           //  5
	{ "b73_06.2b",		0x10000, 0xa98e4aea, 5 | BRF_GRA },           //  6
	{ "b73_01.1b",		0x10000, 0x3a8a4489, 5 | BRF_GRA },           //  7
	{ "b73_02.1c",		0x10000, 0x025c536c, 5 | BRF_GRA },           //  8
	{ "b73_03.1d",		0x10000, 0xec6020cf, 5 | BRF_GRA },           //  9
	{ "b73_04.1f",		0x10000, 0xf7badbb2, 5 | BRF_GRA },           // 10
	{ "b73_05.1h",		0x10000, 0xb8e829d2, 5 | BRF_GRA },           // 11

	{ "b73_09.2p",		0x80000, 0x537c647f, 6 | BRF_GRA },           // 12 Background Tiles
};

STD_ROM_PICK(hvyunitjo)
STD_ROM_FN(hvyunitjo)

struct BurnDriver BurnDrvHvyunitjo = {
	"hvyunitjo", "hvyunit", NULL, NULL, "1988",
	"Heavy Unit (Japan, Older)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_VERSHOOT, 0,
	NULL, hvyunitjoRomInfo, hvyunitjoRomName, NULL, NULL, HvyunitInputInfo, HvyunitjDIPInfo,
	hvyunitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Heavy Unit -U.S.A. Version- (US)

static struct BurnRomInfo hvyunituRomDesc[] = {
	{ "b73_34.5c",		0x20000, 0x05c30a90, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "b73_35.6p",		0x10000, 0xaed1669d, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b73_12.7e",		0x10000, 0xd1d24fab, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "mermaid.bin",	0x00e00, 0x88c5dd27, 4 | BRF_PRG | BRF_ESS }, //  3 I80C51 (mermaid) Code

	{ "b73_08.2f",		0x80000, 0xf83dd808, 5 | BRF_GRA },           //  4 Sprites
	{ "b73_28.2c",		0x20000, 0xa02e08d6, 5 | BRF_GRA },           //  5
	{ "b73_27.2b",		0x20000, 0x8708f97c, 5 | BRF_GRA },           //  6
	{ "b73_25.0b",		0x20000, 0x2f13f81e, 5 | BRF_GRA },           //  7
	{ "b73_26.0c",		0x10000, 0xb8e829d2, 5 | BRF_GRA },           //  8

	{ "b73_09.2p",		0x80000, 0x537c647f, 6 | BRF_GRA },           //  9 Background Tiles
};

STD_ROM_PICK(hvyunitu)
STD_ROM_FN(hvyunitu)

static INT32 hvyunituInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvHvyunitu = {
	"hvyunitu", "hvyunit", NULL, NULL, "1988",
	"Heavy Unit -U.S.A. Version- (US)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_VERSHOOT, 0,
	NULL, hvyunituRomInfo, hvyunituRomName, NULL, NULL, HvyunitInputInfo, HvyunitjDIPInfo,
	hvyunituInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
