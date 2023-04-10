// FB Alpha King of Boxing / Ring King driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "dac.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[4];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[4];
static UINT8 *DrvShareRAM[2];
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvColRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvUnkRAM;
static UINT8 *DrvScrRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 scrolly;
static INT32 nmi_enable;
static INT32 palette_bank;
static INT32 soundlatch;
static INT32 flipscreen;
static INT32 extra_cycles[4];
static INT32 vblank;

static UINT8 input_state = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo KingofbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kingofb)

static struct BurnInputInfo RingkingInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ringking)

static struct BurnDIPInfo KingofbDIPList[]=
{
	{0x12, 0xff, 0xff, 0x21, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Rest Up Points"		},
	{0x12, 0x01, 0x03, 0x02, "70000"				},
	{0x12, 0x01, 0x03, 0x01, "100000"				},
	{0x12, 0x01, 0x03, 0x03, "150000"				},
	{0x12, 0x01, 0x03, 0x00, "No"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x04, 0x04, "Off"					},
	{0x12, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x18, 0x00, "Easy"					},
	{0x12, 0x01, 0x18, 0x08, "Medium"				},
	{0x12, 0x01, 0x18, 0x10, "Hard"					},
	{0x12, 0x01, 0x18, 0x18, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x20, 0x20, "Upright"				},
	{0x12, 0x01, 0x20, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x00, "Off"					},
	{0x12, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x13, 0x01, 0x07, 0x07, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x13, 0x01, 0x80, 0x00, "Off"					},
	{0x13, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Kingofb)

static struct BurnDIPInfo RingkingDIPList[]=
{
	{0x14, 0xff, 0xff, 0xd7, NULL					},
	{0x15, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    4, "Replay"				},
	{0x14, 0x01, 0x03, 0x01, "70000"				},
	{0x14, 0x01, 0x03, 0x02, "100000"				},
	{0x14, 0x01, 0x03, 0x00, "150000"				},
	{0x14, 0x01, 0x03, 0x03, "No"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x04, 0x00, "Off"					},
	{0x14, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty (2P)"		},
	{0x14, 0x01, 0x18, 0x18, "Easy"					},
	{0x14, 0x01, 0x18, 0x10, "Medium"				},
	{0x14, 0x01, 0x18, 0x08, "Hard"					},
	{0x14, 0x01, 0x18, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x20, 0x00, "Upright"				},
	{0x14, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"					},
	{0x14, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x15, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x15, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty (1P)"		},
	{0x15, 0x01, 0x30, 0x30, "Easy"					},
	{0x15, 0x01, 0x30, 0x10, "Medium"				},
	{0x15, 0x01, 0x30, 0x20, "Hard"					},
	{0x15, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Boxing Match"			},
	{0x15, 0x01, 0x40, 0x40, "2 Win, End"			},
	{0x15, 0x01, 0x40, 0x00, "1 Win, End"			},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x15, 0x01, 0x80, 0x80, "Off"					},
	{0x15, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Ringking)

static void __fastcall kingobox_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd800: // ringking
		case 0xf800: // kingobox
			nmi_enable = data & 0x20;
			palette_bank = data & 0x18;
			flipscreen = data & 0x80;
		return;

		case 0xf801: // kingobox
		return; // nop

		case 0xe800: // ringking
		case 0xf802: // kingobox
			scrolly = data;
		return;

		case 0xf803: // kingobox
			scrolly = data; // ??

		case 0xd801: // ringking
			ZetClose();
			ZetOpen(2);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xd802: // ringking
		case 0xf804: // kingobox
			ZetClose();
			ZetOpen(1);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xd803: // ringking
		case 0xf807: // kingobox
			soundlatch = data;
			ZetClose();
			ZetOpen(3);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall kingobox_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000: // ringking
		case 0xfc00: // kingobox
			return DrvDips[0];

		case 0xe001: // ringking
		case 0xfc01: // kingobox
			return DrvDips[1];

		case 0xe002: // ringking
		case 0xfc02: // kingobox
			return DrvInputs[0];

		case 0xe003: // ringking
		case 0xfc03: // kingobox
			return DrvInputs[1];

		case 0xe004: // ringking
			return (DrvInputs[2] & ~0x20) | (vblank ? 0x00 : 0x20); // system

		case 0xfc04: // kingobox
			return DrvInputs[2]; // system

		case 0xe005: // ringking
		case 0xfc05: // kingobox
			return DrvInputs[3]; // extra
	}

	return 0;
}

static void __fastcall kingobox_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00: // both
			DACWrite(0, data);
		return;

		case 0x02: // ringking
		case 0x08: // kingobox
			AY8910Write(0, 1, data);
		return;

		case 0x03: // ringking
		case 0x0c: // kingobox
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall kingobox_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02: // ringking
		case 0x08: // kingobox
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 AY8910_0_port0(UINT32)
{
	return soundlatch;
}

static tilemap_callback( bg )
{
	offs ^= 0xf;
	INT32 attr = DrvColRAM[0][offs];
	INT32 code = 0;
	if (offs / 16) code = DrvVidRAM[0][offs] + ((attr & 3) << 8);
	INT32 color = ((attr >> 4) & 7) | palette_bank;

	TILE_SET_INFO(1+((attr & 4)/4), code, color, 0);
}

static tilemap_callback( fg )
{
	offs ^= 0x1f;
	INT32 attr = DrvColRAM[1][offs];
	INT32 code = DrvVidRAM[1][offs] | ((attr & 3) << 8);
	INT32 color = (attr >> 3) & 7;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	ZetClose();

	ZetOpen(3);
	ZetReset();
	DACReset();
	ZetClose();

	AY8910Reset(0);

	HiscoreReset();

	scrolly = 0;
	nmi_enable = 0;
	palette_bank = 0;
	flipscreen = 0;
	memset (extra_cycles, 0, sizeof(extra_cycles));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x00c000;
	DrvZ80ROM[1]		= Next; Next += 0x004000;
	DrvZ80ROM[2]		= Next; Next += 0x002000;
	DrvZ80ROM[3]		= Next; Next += 0x00c000;

	DrvGfxROM[0]		= Next; Next += 0x010000;
	DrvGfxROM[1]		= Next; Next += 0x080000;
	DrvGfxROM[2] 		= Next; Next += 0x040000;

	DrvColPROM			= Next; Next += 0x000c00;

	DrvPalette			= (UINT32*)Next; Next += 0x110 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x0004000;
	DrvZ80RAM[1]		= Next; Next += 0x0008000;
	DrvZ80RAM[2]		= Next; Next += 0x0008000;
	DrvZ80RAM[3]		= Next; Next += 0x0004000;

	DrvShareRAM[0]		= Next; Next += 0x0008000;
	DrvShareRAM[1]		= Next; Next += 0x0008000;

	DrvVidRAM[0]		= Next; Next += 0x0001000;
	DrvVidRAM[1]		= Next; Next += 0x0004000;
	DrvColRAM[0]		= Next; Next += 0x0001000;
	DrvColRAM[1]		= Next; Next += 0x0004000;

	DrvSprRAM			= Next; Next += 0x0004000;
	DrvUnkRAM			= Next; Next += 0x0008000;
	DrvScrRAM			= Next; Next += 0x0001000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 KingoboxGfxDecode() // 100, 0
{
	INT32 Plane0[1] = { 0 };
	INT32 Plane1[3] = { 2*0x4000*8, 1*0x4000*8, 0*0x4000*8 };
	INT32 XOffs[16] = { STEP8((3*0x4000*8),1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x02000);

	GfxDecode(0x0400, 1,  8,  8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane1, XOffs + 0, YOffs, 0x080, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x18000);

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs + 0, YOffs, 0x080, tmp, DrvGfxROM[2]);

	memcpy (DrvGfxROM[2] + 0x20000, DrvGfxROM[2], 0x20000);
	memcpy (DrvGfxROM[1] + 0x40000, DrvGfxROM[2], 0x40000);

	BurnFree(tmp);

	return 0;
}

static INT32 KingoboxInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "ringking3")) {
			if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[3] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[3] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[3] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00800, k++, 1)) return 1;

		KingoboxGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],		0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[1],	0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[0],	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvUnkRAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(kingobox_main_write);
	ZetSetReadHandler(kingobox_main_read);
	ZetClose();

	ZetInit(1); // video
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[0],	0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],		0xc000, 0xc0ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[0],		0xc400, 0xc4ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[1],		0xc800, 0xcbff, MAP_RAM);
	ZetMapMemory(DrvColRAM[1],		0xcc00, 0xcfff, MAP_RAM);
	ZetClose();

	ZetInit(2); // sprite
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[1],	0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,			0xc400, 0xc4ff, MAP_RAM);
	ZetClose();

	ZetInit(3); // sound
	ZetOpen(3);
	ZetMapMemory(DrvZ80ROM[3],		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[3],		0xc000, 0xc3ff, MAP_RAM);
	ZetSetOutHandler(kingobox_sound_write_port);
	ZetSetInHandler(kingobox_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, &AY8910_0_port0, &AY8910_0_port0, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1,  8,  8, 0x10000, 0x100, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 16, 16, 0x40000, 0x000, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, 0x20000, 0x000, 0x1f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 RingkingGfxDecode() // 100, 0
{
	INT32 Plane0[1]  = { 0 };
	INT32 XOffs0A[8] = { STEP4(7,-1), STEP4(0x8000+7, -1) };
	INT32 XOffs0B[8] = { STEP4(3,-1), STEP4(0x8000+3, -1) };
	INT32 YOffs0[8]  = { STEP8(7*8,-8) };

	INT32 Plane1[3]  = { 0x10000*8*0, 0x10000*8*1, 0x10000*8*2 };
	INT32 XOffs1[16] = { STEP8(7,-1), STEP8(128+7,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	INT32 Plane2[3]  = { 0x4000*8+4, 0, 4 };
	INT32 XOffs2[16] = { STEP4(128+3,-1), STEP4((0x2000*8)+3,-1), STEP4(3,-1), STEP4((0x2010*8)+3,-1) };
	INT32 YOffs2[16] = { STEP16(15*8,-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x02000);

	GfxDecode(0x0200, 1,  8,  8, Plane0, XOffs0A, YOffs0, 0x040, tmp, DrvGfxROM[0] + 0x0000);
	GfxDecode(0x0200, 1,  8,  8, Plane0, XOffs0B, YOffs0, 0x040, tmp, DrvGfxROM[0] + 0x8000);

	memcpy (tmp, DrvGfxROM[1], 0x40000);

	GfxDecode(0x0600, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x08000);

	GfxDecode(0x0100, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static void RingkingColpromFix()
{
	for (INT32 i = 0; i < 0x100; i++) { // expand color proms
		DrvColPROM[i + 0x800] = DrvColPROM[i + 0x400];
		DrvColPROM[i + 0x400] = DrvColPROM[i + 0x000] & 0xf;
		DrvColPROM[i + 0x000] >>= 4;
	}
}

static INT32 RingkingInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[3] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[3] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1; // char

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1; // sp
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x08000, k++, 1)) return 1; // sp
		if (BurnLoadRom(DrvGfxROM[1] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x28000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1; // bk
		if (BurnLoadRom(DrvGfxROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00400, k++, 1)) return 1;

		RingkingGfxDecode();
		RingkingColpromFix();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],		0xc000, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[1],	0xc800, 0xcfff, MAP_RAM); // spr
	ZetMapMemory(DrvShareRAM[0],	0xd000, 0xd7ff, MAP_RAM); // vid
	ZetMapMemory(DrvUnkRAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(kingobox_main_write);
	ZetSetReadHandler(kingobox_main_read);
	ZetClose();

	ZetInit(1); // video
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[0],	0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],		0xa800, 0xa8ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[0],		0xac00, 0xacff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[1],		0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[1],		0xa400, 0xa7ff, MAP_RAM);
	ZetClose();

	ZetInit(2); // sprite
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[2],		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM[1],	0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,			0xa400, 0xa4ff, MAP_RAM);
	ZetClose();

	ZetInit(3); // sound
	ZetOpen(3);
	ZetMapMemory(DrvZ80ROM[3],		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[3],		0xc000, 0xc3ff, MAP_RAM);
	ZetSetOutHandler(kingobox_sound_write_port);
	ZetSetInHandler(kingobox_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, &AY8910_0_port0, &AY8910_0_port0, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1,  8,  8, 0x10000, 0x100, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[2], 3, 16, 16, 0x10000, 0x000, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, 0x10000, 0x000, 0x1f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	input_state = 0xff;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	DACExit();

	input_state = 0;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = (((bit0 * 180) + (bit1 * 300) + (bit2 * 750) + (bit3 * 1500)) * 255) / (1500 + 750 + 360 + 180);

		bit0 = (DrvColPROM[i + 0x400] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x400] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x400] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x400] >> 3) & 0x01;
		INT32 g = (((bit0 * 180) + (bit1 * 300) + (bit2 * 750) + (bit3 * 1500)) * 255) / (1500 + 750 + 360 + 180);

		bit0 = (DrvColPROM[i + 0x800] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x800] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x800] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x800] >> 3) & 0x01;
		INT32 b = (((bit0 * 180) + (bit1 * 300) + (bit2 * 750) + (bit3 * 1500)) * 255) / (1500 + 750 + 360 + 180);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0x101; i < 0x110; i += 2)
	{
		DrvPalette[i] = BurnHighCol((i&8)?0xff:0, (i&4)?0xff:0, (i&2)?0xff:0, 0);
	}
}

static void draw_sprites(INT32 scrambled)
{
	INT32 soffs[2][4] = { { 0, 2, 3, 1 }, { 0, 1, 2, 3 } };

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 roffs;
		if (scrambled) {
			roffs = ((offs & 0xfc0f) | ((offs & 0x300) >> 4) | ((offs & 0xe0) << 1) | ((offs & 0x10) << 5)) ^ 0x3c;
			if (roffs & 0x200) roffs ^= 0x1c0;
		} else {
			roffs = offs;
		}

		INT32 attr	= DrvSprRAM[roffs + soffs[scrambled][3]];
		INT32 code	= DrvSprRAM[roffs + soffs[scrambled][2]] + ((attr & 0x07) << 8);
		INT32 sx	= DrvSprRAM[roffs + soffs[scrambled][1]];
		INT32 sy	= DrvSprRAM[roffs + soffs[scrambled][0]];
		INT32 color	= ((attr & 0x70) >> 4) | palette_bank;
		INT32 flipx	= 0;
		INT32 flipy	= (attr ^ input_state) & 0x80;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 3, 0, 0, DrvGfxROM[1]);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	GenericTilemapSetScrollY(0, -scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(input_state ? 0 : 1);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

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
		memset (DrvInputs, input_state, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[4] = { 4000000 / 60, 4000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 7) vblank = 0;
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 99 && nmi_enable) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 99 && nmi_enable) ZetNmi();
		ZetClose();

		ZetOpen(2);
		CPU_RUN(2, Zet);
		if (i == 99 && nmi_enable) ZetNmi();
		ZetClose();

		ZetOpen(3);
		CPU_RUN(3, Zet);
		ZetNmi(); // 100x per frame
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(palette_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(extra_cycles);
	}

	return 0;
}


// King of Boxer (World)

static struct BurnRomInfo kingofbRomDesc[] = {
	{ "22.d9",				0x4000, 0x6220bfa2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "23.e9",				0x4000, 0x5782fdd8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "21.b9",				0x4000, 0x3fb39489, 2 | BRF_GRA },           //  2 Z80 #1 (Video) Code

	{ "17.j9",				0x2000, 0x379f4f84, 3 | BRF_GRA },           //  3 Z80 #2 (Sprite) Code

	{ "18.f4",				0x4000, 0xc057e28e, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #3 (Sound) Code
	{ "19.h4",				0x4000, 0x060253dd, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "13.d14",				0x2000, 0xe36d4f4f, 5 | BRF_GRA },           //  7 Characters

	{ "1.b1",				0x4000, 0xce6580af, 6 | BRF_GRA },           //  8 Sprites and Background Tiles
	{ "3.b4",				0x4000, 0xcf74ea50, 6 | BRF_GRA },           //  9
	{ "5.b7",				0x4000, 0xd8b53975, 6 | BRF_GRA },           // 10
	{ "2.b3",				0x4000, 0x4ab506d2, 6 | BRF_GRA },           // 11
	{ "4.b5",				0x4000, 0xecf95a2c, 6 | BRF_GRA },           // 12
	{ "6.b8",				0x4000, 0x8200cb2b, 6 | BRF_GRA },           // 13
	{ "7.d1",				0x2000, 0x3d472a22, 6 | BRF_GRA },           // 14
	{ "9.d4",				0x2000, 0xcc002ea9, 6 | BRF_GRA },           // 15
	{ "11.d7",				0x2000, 0x23c1b3ee, 6 | BRF_GRA },           // 16
	{ "8.d3",				0x2000, 0xd6b1b8fe, 6 | BRF_GRA },           // 17
	{ "10.d5",				0x2000, 0xfce71e5a, 6 | BRF_GRA },           // 18
	{ "12.d8",				0x2000, 0x3f68b991, 6 | BRF_GRA },           // 19

	{ "vb14_col.bin",		0x0100, 0xc58e5121, 9 | BRF_GRA },           // 20 Color PROMs
	{ "vb15_col.bin",		0x0100, 0x5ab06f25, 9 | BRF_GRA },           // 21
	{ "vb16_col.bin",		0x0100, 0x1171743f, 9 | BRF_GRA },           // 22

	{ "pal12h6-vh02.bin",	0x0034, 0x6cc0fdf2, 0 | BRF_OPT },           // 23 PLDs
	{ "pal14h4-vh07.bin",	0x003c, 0x7e59d45a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(kingofb)
STD_ROM_FN(kingofb)

struct BurnDriver BurnDrvKingofb = {
	"kingofb", NULL, NULL, NULL, "1985",
	"King of Boxer (World)\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, kingofbRomInfo, kingofbRomName, NULL, NULL, NULL, NULL, KingofbInputInfo, KingofbDIPInfo,
	KingoboxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};


// King of Boxer (Japan)

static struct BurnRomInfo kingofbjRomDesc[] = {
	{ "22.d9",				0x4000, 0x6220bfa2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "23.e9",				0x4000, 0x5782fdd8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "21.b9",				0x4000, 0x3fb39489, 2 | BRF_GRA },           //  2 Z80 #1 (Video) Code

	{ "17.j9",				0x2000, 0x379f4f84, 3 | BRF_GRA },           //  3 Z80 #2 (Sprite) Code

	{ "18.f4",				0x4000, 0xc057e28e, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #3 (Sound) Code
	{ "19.h4",				0x4000, 0x060253dd, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "13.d14",				0x2000, 0x988a77bf, 5 | BRF_GRA },           //  7 Characters

	{ "1.b1",				0x4000, 0x7b6f390e, 6 | BRF_GRA },           //  8 Sprites and Background Tiles
	{ "3.b4",				0x4000, 0xcf74ea50, 6 | BRF_GRA },           //  9
	{ "5.b7",				0x4000, 0xd8b53975, 6 | BRF_GRA },           // 10
	{ "2.b3",				0x4000, 0x4ab506d2, 6 | BRF_GRA },           // 11
	{ "4.b5",				0x4000, 0xecf95a2c, 6 | BRF_GRA },           // 12
	{ "6.b8",				0x4000, 0x8200cb2b, 6 | BRF_GRA },           // 13
	{ "7.d1",				0x2000, 0x3d472a22, 6 | BRF_GRA },           // 14
	{ "9.d4",				0x2000, 0xcc002ea9, 6 | BRF_GRA },           // 15
	{ "11.d7",				0x2000, 0x23c1b3ee, 6 | BRF_GRA },           // 16
	{ "8.d3",				0x2000, 0xd6b1b8fe, 6 | BRF_GRA },           // 17
	{ "10.d5",				0x2000, 0xfce71e5a, 6 | BRF_GRA },           // 18
	{ "12.d8",				0x2000, 0x3f68b991, 6 | BRF_GRA },           // 19

	{ "vb14_col.bin",		0x0100, 0xc58e5121, 9 | BRF_GRA },           // 20 Color PROMs
	{ "vb15_col.bin",		0x0100, 0x5ab06f25, 9 | BRF_GRA },           // 21
	{ "vb16_col.bin",		0x0100, 0x1171743f, 9 | BRF_GRA },           // 22

	{ "pal12h6-vh02.bin",	0x0034, 0x6cc0fdf2, 0 | BRF_OPT },           // 23 PLDs
	{ "pal14h4-vh07.bin",	0x003c, 0x7e59d45a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(kingofbj)
STD_ROM_FN(kingofbj)

struct BurnDriver BurnDrvKingofbj = {
	"kingofbj", "kingofb", NULL, NULL, "1985",
	"King of Boxer (Japan)\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, kingofbjRomInfo, kingofbjRomName, NULL, NULL, NULL, NULL, KingofbInputInfo, KingofbDIPInfo,
	KingoboxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};


// Ring King (US, Wood Place Inc.)

static struct BurnRomInfo ringkingwRomDesc[] = {
	{ "15.d9",				0x4000, 0x8263f517, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "16.e9",				0x4000, 0xdaadd700, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "14.b9",				0x4000, 0x76a73c95, 2 | BRF_GRA },           //  2 Z80 #1 (Video) Code

	{ "17.j9",				0x2000, 0x379f4f84, 3 | BRF_GRA },           //  3 Z80 #2 (Sprite) Code

	{ "18.f4",				0x4000, 0xc057e28e, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #3 (Sound) Code
	{ "19.h4",				0x4000, 0x060253dd, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "13.d14",				0x2000, 0xe36d4f4f, 5 | BRF_GRA },           //  7 Characters

	{ "1.b1",				0x4000, 0xce6580af, 6 | BRF_GRA },           //  8 Sprites and Background Tiles
	{ "3.b4",				0x4000, 0xcf74ea50, 6 | BRF_GRA },           //  9
	{ "5.b7",				0x4000, 0xd8b53975, 6 | BRF_GRA },           // 10
	{ "2.b3",				0x4000, 0x4ab506d2, 6 | BRF_GRA },           // 11
	{ "4.b5",				0x4000, 0xecf95a2c, 6 | BRF_GRA },           // 12
	{ "6.b8",				0x4000, 0x8200cb2b, 6 | BRF_GRA },           // 13
	{ "7.d1",				0x2000, 0x019a88b0, 6 | BRF_GRA },           // 14
	{ "9.d4",				0x2000, 0xbfdc741a, 6 | BRF_GRA },           // 15
	{ "11.d7",				0x2000, 0x3cc7bdc5, 6 | BRF_GRA },           // 16
	{ "8.d3",				0x2000, 0x65f1281b, 6 | BRF_GRA },           // 17
	{ "10.d5",				0x2000, 0xaf5013e7, 6 | BRF_GRA },           // 18
	{ "12.d8",				0x2000, 0x1f6654d6, 6 | BRF_GRA },           // 19

	{ "prom2.bin",			0x0400, 0x8ce34029, 9 | BRF_GRA },           // 20 Color PROMs
	{ "prom3.bin",			0x0400, 0x54cfe913, 9 | BRF_GRA },           // 21
	{ "prom1.bin",			0x0400, 0x913f5975, 9 | BRF_GRA },           // 22
};

STD_ROM_PICK(ringkingw)
STD_ROM_FN(ringkingw)

static void RingkingwColpromFix()
{
	UINT8 *dst = (UINT8*)BurnMalloc(0xc00);

	for (int i = 0, j = 0; j < 0x40; i++, j++)
	{
		if ((i & 0xf) == 8)
			i += 8;

		for (int k = 0; k < 4; k++)
		{
			dst[j + 0x000 + 0x40 * k] = DrvColPROM[i + 0x000 + 0x100 * k]; /* R */
			dst[j + 0x400 + 0x40 * k] = DrvColPROM[i + 0x400 + 0x100 * k]; /* G */
			dst[j + 0x800 + 0x40 * k] = DrvColPROM[i + 0x800 + 0x100 * k]; /* B */
		}
	}

	memcpy (DrvColPROM, dst, 0xc00);

	BurnFree (dst);
}

static INT32 RingkingwInit()
{
	INT32 nRet = KingoboxInit();

	if (nRet == 0)
	{
		RingkingwColpromFix();
	}

	return nRet;
}

struct BurnDriver BurnDrvRingkingw = {
	"ringkingw", "kingofb", NULL, NULL, "1985",
	"Ring King (US, Wood Place Inc.)\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, ringkingwRomInfo, ringkingwRomName, NULL, NULL, NULL, NULL, KingofbInputInfo, KingofbDIPInfo,
	RingkingwInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};


// Ring King (US set 3)

static struct BurnRomInfo ringking3RomDesc[] = {
	{ "14.d9",				0x4000, 0x63627b8b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "15.e9",				0x4000, 0xe7557489, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "16.f9",				0x4000, 0xa3b3bb16, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "13.b9",				0x4000, 0xf33f94a2, 2 | BRF_GRA },           //  3 Z80 #1 (Video) Code

	{ "j09_dcr.bin",		0x2000, 0x379f4f84, 3 | BRF_GRA },           //  4 Z80 #2 (Sprite) Code

	{ "18.f4",				0x4000, 0xc057e28e, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 (Sound) Code
	{ "19.h4",				0x4000, 0x060253dd, 4 | BRF_PRG | BRF_ESS }, //  6
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "12.d15",				0x2000, 0x988a77bf, 5 | BRF_GRA },           //  8 Characters

	{ "1.b1",				0x4000, 0xce6580af, 6 | BRF_GRA },           //  9 Sprites and Background Tiles
	{ "3.b4",				0x4000, 0xcf74ea50, 6 | BRF_GRA },           // 10
	{ "5.b7",				0x4000, 0xd8b53975, 6 | BRF_GRA },           // 11
	{ "2.b3",				0x4000, 0x4ab506d2, 6 | BRF_GRA },           // 12
	{ "4.b5",				0x4000, 0xecf95a2c, 6 | BRF_GRA },           // 13
	{ "6.b8",				0x4000, 0x8200cb2b, 6 | BRF_GRA },           // 14
	{ "7.d1",				0x2000, 0x019a88b0, 6 | BRF_GRA },           // 15
	{ "9.d4",				0x2000, 0xbfdc741a, 6 | BRF_GRA },           // 16
	{ "11.d7",				0x2000, 0x3cc7bdc5, 6 | BRF_GRA },           // 17
	{ "8.d3",				0x2000, 0x65f1281b, 6 | BRF_GRA },           // 18
	{ "10.d5",				0x2000, 0xaf5013e7, 6 | BRF_GRA },           // 19
	{ "12.d8",				0x2000, 0x1f6654d6, 6 | BRF_GRA },           // 20

	{ "82s135.2a",			0x0100, 0x0e723a83, 9 | BRF_GRA },           // 21 Color PROMs
	{ "82s129.1a",			0x0100, 0xd345cbb3, 9 | BRF_GRA },           // 22
};

STD_ROM_PICK(ringking3)
STD_ROM_FN(ringking3)

static INT32 Ringking3Init()
{
	INT32 nRet = KingoboxInit();

	if (nRet == 0)
	{
		RingkingColpromFix();
	}

	return nRet;
}

struct BurnDriver BurnDrvRingking3 = {
	"ringking3", "kingofb", NULL, NULL, "1985",
	"Ring King (US set 3)\0", NULL, "Wood Place Inc. (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, ringking3RomInfo, ringking3RomName, NULL, NULL, NULL, NULL, KingofbInputInfo, KingofbDIPInfo,
	Ringking3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};


// Ring King (US, Wood Place Inc.)

static struct BurnRomInfo ringkingRomDesc[] = {
	{ "cx13.9f",			0x8000, 0x93e38c02, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "cx14.11f",			0x4000, 0xa435acb0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cx07.10c",			0x4000, 0x9f074746, 2 | BRF_GRA },           //  2 Z80 #1 (Video) Code

	{ "cx00.4c",			0x2000, 0x880b8aa7, 3 | BRF_GRA },           //  3 Z80 #2 (Sprite) Code

	{ "cx12.4ef",			0x8000, 0x1d5d6c6b, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #3 (Sound) Code
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "cx08.13b",			0x2000, 0xdbd7c1c2, 5 | BRF_GRA },           //  6 Characters

	{ "cx04.11j",			0x8000, 0x506a2ed9, 6 | BRF_GRA },           //  7 Sprites
	{ "cx02.8j",			0x8000, 0x009dde6a, 6 | BRF_GRA },           //  8
	{ "cx06.13j",			0x8000, 0xd819a3b2, 6 | BRF_GRA },           //  9
	{ "cx03.9j",			0x4000, 0x682fd1c4, 6 | BRF_GRA },           // 10
	{ "cx01.7j",			0x4000, 0x85130b46, 6 | BRF_GRA },           // 11
	{ "cx05.12j",			0x4000, 0xf7c4f3dc, 6 | BRF_GRA },           // 12

	{ "cx09.17d",			0x4000, 0x37a082cf, 7 | BRF_GRA },           // 13 Background Tiles
	{ "cx10.17e",			0x4000, 0xab9446c5, 7 | BRF_GRA },           // 14

	{ "82s135.2a",			0x0100, 0x0e723a83, 9 | BRF_GRA },           // 15 Color PROMs
	{ "82s129.1a",			0x0100, 0xd345cbb3, 9 | BRF_GRA },           // 16
};

STD_ROM_PICK(ringking)
STD_ROM_FN(ringking)

struct BurnDriver BurnDrvRingking = {
	"ringking", "kingofb", NULL, NULL, "1985",
	"Ring King (US set 1)\0", NULL, "Wood Place Inc. (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, ringkingRomInfo, ringkingRomName, NULL, NULL, NULL, NULL, RingkingInputInfo, RingkingDIPInfo,
	RingkingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};


// Ring King (US set 2)

static struct BurnRomInfo ringking2RomDesc[] = {
	{ "rkngm1.bin",			0x8000, 0x086921ea, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 (Master) Code
	{ "rkngm2.bin",			0x4000, 0xc0b636a4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rkngtram.bin",		0x4000, 0xd9dc1a0a, 2 | BRF_GRA },           //  2 Z80 #1 (Video) Code

	{ "cx00.4c",			0x2000, 0x880b8aa7, 3 | BRF_GRA },           //  3 Z80 #2 (Sprite) Code

	{ "cx12.4ef",			0x8000, 0x1d5d6c6b, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #3 (Sound) Code
	{ "20.j4",				0x4000, 0x64c137a4, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "cx08.13b",			0x2000, 0xdbd7c1c2, 5 | BRF_GRA },           //  6 Characters

	{ "cx04.11j",			0x8000, 0x506a2ed9, 6 | BRF_GRA },           //  7 Sprites
	{ "cx02.8j",			0x8000, 0x009dde6a, 6 | BRF_GRA },           //  8
	{ "cx06.13j",			0x8000, 0xd819a3b2, 6 | BRF_GRA },           //  9
	{ "cx03.9j",			0x4000, 0x682fd1c4, 6 | BRF_GRA },           // 10
	{ "cx01.7j",			0x4000, 0x85130b46, 6 | BRF_GRA },           // 11
	{ "cx05.12j",			0x4000, 0xf7c4f3dc, 6 | BRF_GRA },           // 12

	{ "cx09.17d",			0x4000, 0x37a082cf, 7 | BRF_GRA },           // 13 Background Tiles
	{ "cx10.17e",			0x4000, 0xab9446c5, 7 | BRF_GRA },           // 14

	{ "82s135.2a",			0x0100, 0x0e723a83, 9 | BRF_GRA },           // 15 Color PROMs
	{ "82s129.1a",			0x0100, 0xd345cbb3, 9 | BRF_GRA },           // 16
};

STD_ROM_PICK(ringking2)
STD_ROM_FN(ringking2)

struct BurnDriver BurnDrvRingking2 = {
	"ringking2", "kingofb", NULL, NULL, "1985",
	"Ring King (US set 2)\0", NULL, "Wood Place Inc. (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, ringking2RomInfo, ringking2RomName, NULL, NULL, NULL, NULL, RingkingInputInfo, RingkingDIPInfo,
	RingkingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x108,
	224, 256, 3, 4
};
