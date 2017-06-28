// FB Alpha Alpha Denshi Co 68k Type I driver module
// Based on MAME driver by Pierpaolo Prazzoli, Bryan McPhail, and Stephane Humbert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvColPROM;
static UINT8 *DrvLutROM;
static UINT8 *DrvZ80RAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 port_fc;

static INT32 cpu_clock;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static UINT32 tnextspc2mode = 0;

static struct BurnInputInfo PaddlemaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"P4 Up",		BIT_DIGITAL,	DrvJoy3 + 8,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy3 + 9,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy3 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 14,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 13,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Paddlema)

static struct BurnInputInfo TnextspcInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Tnextspc)

static struct BurnDIPInfo PaddlemaDIPList[]=
{
	{0x1e, 0xff, 0xff, 0x73, NULL				},
	{0x1f, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x1e, 0x01, 0x03, 0x03, "1 Coin  2 Credits"		},
	{0x1e, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},
	{0x1e, 0x01, 0x03, 0x02, "1 Coin  4 Credits"		},
	{0x1e, 0x01, 0x03, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x1e, 0x01, 0x0c, 0x0c, "4 Coins 1 Credits"		},
	{0x1e, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"		},
	{0x1e, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x1e, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x1e, 0x01, 0x30, 0x00, "Default Time"			},
	{0x1e, 0x01, 0x30, 0x20, "+10 Seconds"			},
	{0x1e, 0x01, 0x30, 0x10, "+20 Seconds"			},
	{0x1e, 0x01, 0x30, 0x30, "+30 Seconds"			},

	{0   , 0xfe, 0   ,    3, "Match Type"			},
	{0x1e, 0x01, 0xc0, 0x80, "A to B"			},
	{0x1e, 0x01, 0xc0, 0x00, "A to C"			},
	{0x1e, 0x01, 0xc0, 0x40, "A to E"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1f, 0x01, 0x01, 0x00, "Off"				},
	{0x1f, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    4, "Game Mode"			},
	{0x1f, 0x01, 0x30, 0x20, "Demo Sounds Off"		},
	{0x1f, 0x01, 0x30, 0x00, "Demo Sounds On"		},
	{0x1f, 0x01, 0x30, 0x10, "Win Match Against CPU (Cheat)"},
	{0x1f, 0x01, 0x30, 0x30, "Freeze"			},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x1f, 0x01, 0x40, 0x00, "English"			},
	{0x1f, 0x01, 0x40, 0x40, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x1f, 0x01, 0x80, 0x80, "No"				},
	{0x1f, 0x01, 0x80, 0x00, "Yes"				},
};

STDDIPINFO(Paddlema)

static struct BurnDIPInfo TnextspcDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x01, 0x01, "Off"				},
	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Additional Bonus Life"	},
	{0x11, 0x01, 0x04, 0x04, "2nd Extend ONLY"		},
	{0x11, 0x01, 0x04, 0x00, "Every Extend"			},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x11, 0x01, 0x30, 0x30, "A 1C/1C B 1C/2C"		},
	{0x11, 0x01, 0x30, 0x20, "A 2C/1C B 1C/3C"		},
	{0x11, 0x01, 0x30, 0x10, "A 3C/1C B 1C/5C"		},
	{0x11, 0x01, 0x30, 0x00, "A 4C/1C B 1C/6C"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0xc0, "2"				},
	{0x11, 0x01, 0xc0, 0x80, "3"				},
	{0x11, 0x01, 0xc0, 0x40, "4"				},
	{0x11, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Game Mode"			},
	{0x12, 0x01, 0x04, 0x00, "Freeze"			},
	{0x12, 0x01, 0x04, 0x04, "Infinite Lives (Cheat)"	},

	{0   , 0xfe, 0   ,    2, "Demo Sound/Game Mode"		},
	{0x12, 0x01, 0x08, 0x08, "Demo Sounds"			},
	{0x12, 0x01, 0x08, 0x00, "Game Mode"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x30, 0x30, "100000 200000"		},
	{0x12, 0x01, 0x30, 0x20, "150000 300000"		},
	{0x12, 0x01, 0x30, 0x10, "300000 500000"		},
	{0x12, 0x01, 0x30, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x40, 0x00, "No"				},
	{0x12, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Tnextspc)

static void __fastcall alpha68k_i_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x180000:
		case 0x180001:
		return;		// nop

		case 0x380001:
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 __fastcall alpha68k_i_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
			return 0;

		case 0x180001:
			return DrvDips[0];

		case 0x180008:
			return 0;

		case 0x180009:
			return DrvDips[1];

		case 0x300000:
			return DrvInputs[0] >> 8;

		case 0x300001:
			return DrvInputs[0];

		case 0x340000:
			return DrvInputs[1] >> 8;

		case 0x340001:
			return DrvInputs[1];

		case 0x380000:
			return DrvInputs[2] >> 8;

		case 0x380001:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall tnextspc_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0d0001:
		case 0x0e0007:
		case 0x0e000f:
		return;		// nop

		case 0x0f0003:
		case 0x0f0005:
			// coin counters
		return;

		case 0x0f0009:
			soundlatch = data;
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 __fastcall tnextspc_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0e0009:
			return DrvDips[0];

		case 0x0e000b:
			return DrvDips[1];

		case 0x0e0001:
			return DrvInputs[0];

		case 0x0e0003:
			return DrvInputs[1];

		case 0x0e0005:
			return DrvInputs[2];

		case 0x0e0019:
			return 1; // sound status?
	}

	return 0;
}

static void __fastcall alpha68k_i_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			soundlatch = 0;
		return;

		case 0xe800:
			BurnYM3812Write(0, 0, data); // control
		return;

		case 0xec00:
			BurnYM3812Write(0, 1, data); // write
		return;

		case 0xfc00:
			port_fc = data; // what is this???
		return;
	}
}

static UINT8 __fastcall alpha68k_i_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0xe800:
			return BurnYM3812Read(0, 0);

		case 0xfc00:	// nop?
			return port_fc;
	}

	return 0;
}

static void __fastcall tnextspc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			soundlatch = 0;
		return;
	}

	if (data) return; // kill warnings
}

static UINT8 __fastcall tnextspc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void __fastcall tnextspc_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM3812Write(0, 0, data); // control
		return;

		case 0x20:
			BurnYM3812Write(0, 1, data); // write
		return;
	}
}

static UINT8 __fastcall tnextspc_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0, 0);

		case 0x3b:	// nop?
		case 0x3d:	// nop?
		case 0x7b:	// nop?
			return 0;
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_HOLD : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;
	port_fc = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;

	DrvColPROM		= Next; Next += 0x001000;

	DrvLutROM		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x000401 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	Drv68KRAM		= Next; Next += 0x004000;
	DrvSprRAM		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,4) };
	INT32 XOffs[8] = { 8*16+3, 8*16+2, 8*16+1, 8*16+0, 3, 2, 1, 0 };
	INT32 YOffs[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x80000);

	GfxDecode(((0x80000 * 8) / 4) / (8 * 8), 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	BurnFree(tmp);

	// merge color prom nibbles
	for (INT32 i = 0; i < 0x400; i++) {
		DrvColPROM[0x300 + i] = (DrvColPROM[0x300 + i] & 0xf) | (DrvColPROM[0x700 + i] << 4);
	}

	return 0;
}

static INT32 PaddlemaInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

	//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060001, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, 13, 2)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000100, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000200, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000300, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000700, 18, 1)) return 1;

		if (BurnLoadRom(DrvLutROM +  0x000000, 19, 1)) return 1;

		DrvGfxDecode();
	}

	cpu_clock = 6000000;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x083fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x100000, 0x103fff, MAP_RAM);
	SekSetWriteByteHandler(0,	alpha68k_i_write_byte);
	SekSetReadByteHandler(0,	alpha68k_i_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(alpha68k_i_sound_write);
	ZetSetReadHandler(alpha68k_i_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 TnextspcInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (tnextspc2mode)
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, 10, 2)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000100, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000200, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000300, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000700, 15, 1)) return 1;

		if (BurnLoadRom(DrvLutROM +  0x000000, 16, 1)) return 1;

		DrvGfxDecode();
	}
	else
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
		BurnByteswap(DrvGfxROM0, 0x80000);

		if (BurnLoadRom(DrvColPROM + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000100,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000200,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000300,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000700,  8, 1)) return 1;

		if (BurnLoadRom(DrvLutROM +  0x000000,  9, 1)) return 1;

		DrvGfxDecode();
	}

	cpu_clock = 9000000;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x070000, 0x073fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x0a0000, 0x0a3fff, MAP_RAM);
	SekSetWriteByteHandler(0,	tnextspc_write_byte);
	SekSetReadByteHandler(0,	tnextspc_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(tnextspc_sound_write);
	ZetSetReadHandler(tnextspc_sound_read);
	ZetSetOutHandler(tnextspc_sound_write_port);
	ZetSetInHandler(tnextspc_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 Tnextspc2Init()
{
	tnextspc2mode = 1;

	return TnextspcInit();
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();

	SekExit();
	ZetExit();

	BurnFree(AllMem);

	tnextspc2mode = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = pal4bit(DrvColPROM[i + 0x000]);
		UINT8 g = pal4bit(DrvColPROM[i + 0x100]);
		UINT8 b = pal4bit(DrvColPROM[i + 0x200]);

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x400; i++) {
		DrvPalette[i] = pal[DrvColPROM[0x300 + i]];
	}

	DrvPalette[0x400] = BurnHighCol(0,0,0,0); // black
}

static void draw_sprites(INT32 c, INT32 d, INT32 yshift)
{
	UINT8 *color_prom = DrvLutROM;
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400; offs += 0x20)
	{
		INT32 sx = ram[offs + c];
		INT32 sy = (yshift - (sx >> 8)) & 0xff;
		sx &= 0xff;

		for (INT32 i = 0; i < 0x20; i++)
		{
			UINT16 attr  = ram[offs + d + i];
			UINT16 code  = attr & 0x3fff;
			UINT16 flipy = attr & 0x4000;
			UINT8 color  = color_prom[(code << 1) | (attr >> 15)];

			if (flipy) {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0, DrvGfxROM0);
			}

			sy = (sy + 8) & 0xff;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x400;
	}

	INT32 shift = (cpu_clock == 9000000) ? 1 : 0;

	draw_sprites(2, 0x800, shift);
	draw_sprites(3, 0xc00, shift);
	draw_sprites(1, 0x400, shift);

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
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}
	}

	INT32 nInterleave = 253;
	INT32 nCyclesTotal[2] = { cpu_clock / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == 248) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029682;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(port_fc);
	}

	return 0;
}


// Paddle Mania

static struct BurnRomInfo paddlemaRomDesc[] = {
	{ "padlem.6g",		0x10000, 0xc227a6e8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "padlem.3g",		0x10000, 0xf11a21aa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "padlem.6h",		0x10000, 0x8897555f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "padlem.3h",		0x10000, 0xf0fe9b9d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "padlem.18c",		0x10000, 0x9269778d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "alpha.mcu",		0x01000, 0      , 3 | BRF_NODUMP | BRF_OPT }, //  5 MCU Code (not used)

	{ "padlem.9m",		0x10000, 0x4ee4970d, 4 | BRF_GRA },           //  6 Graphics
	{ "padlem.16m",		0x10000, 0x0984fb4d, 4 | BRF_GRA },           //  7
	{ "padlem.9n",		0x10000, 0xa1756f15, 4 | BRF_GRA },           //  8
	{ "padlem.16n",		0x10000, 0x4249e047, 4 | BRF_GRA },           //  9
	{ "padlem.6m",		0x10000, 0x3f47910c, 4 | BRF_GRA },           // 10
	{ "padlem.13m",		0x10000, 0xfd9dbc27, 4 | BRF_GRA },           // 11
	{ "padlem.6n",		0x10000, 0xfe337655, 4 | BRF_GRA },           // 12
	{ "padlem.13n",		0x10000, 0x1d460486, 4 | BRF_GRA },           // 13

	{ "padlem.a",		0x00100, 0xcae6bcd6, 5 | BRF_GRA },           // 14 Palette Data
	{ "padlem.b",		0x00100, 0xb6df8dcb, 5 | BRF_GRA },           // 15
	{ "padlem.c",		0x00100, 0x39ca9b86, 5 | BRF_GRA },           // 16

	{ "padlem.17j",		0x00400, 0x86170069, 6 | BRF_GRA },           // 17 Palette Look-up Data
	{ "padlem.16j",		0x00400, 0x8da58e2c, 6 | BRF_GRA },           // 18

	{ "padlem.18n",		0x08000, 0x06506200, 7 | BRF_GRA },           // 19 Color Look-up Data
};

STD_ROM_PICK(paddlema)
STD_ROM_FN(paddlema)

struct BurnDriver BurnDrvPaddlema = {
	"paddlema", NULL, NULL, NULL, "1988",
	"Paddle Mania\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BALLPADDLE, 0,
	NULL, paddlemaRomInfo, paddlemaRomName, NULL, NULL, PaddlemaInputInfo, PaddlemaDIPInfo,
	PaddlemaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// The Next Space (set 1)

static struct BurnRomInfo tnextspcRomDesc[] = {
	{ "ns_4.bin",		0x20000, 0x4617cba3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ns_3.bin",		0x20000, 0xa6c47fef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ns_1.bin",		0x10000, 0xfc26853c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ns_5678.bin",	0x80000, 0x22756451, 3 | BRF_GRA },           //  3 Graphics

	{ "2.p2",		0x00100, 0x1f388d48, 4 | BRF_GRA },           //  4 Palette Data
	{ "3.p3",		0x00100, 0x0254533a, 4 | BRF_GRA },           //  5
	{ "1.p1",		0x00100, 0x488fd0e9, 4 | BRF_GRA },           //  6

	{ "5.p5",		0x00400, 0x9c8527bf, 5 | BRF_GRA },           //  7 Palette Look-up Data
	{ "4.p4",		0x00400, 0xcc9ff769, 5 | BRF_GRA },           //  8

	{ "ns_2.bin",		0x08000, 0x05771d48, 6 | BRF_GRA },           //  9 Color Look-up Data
};

STD_ROM_PICK(tnextspc)
STD_ROM_FN(tnextspc)

struct BurnDriver BurnDrvTnextspc = {
	"tnextspc", NULL, NULL, NULL, "1989",
	"The Next Space (set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tnextspcRomInfo, tnextspcRomName, NULL, NULL, TnextspcInputInfo, TnextspcDIPInfo,
	TnextspcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// The Next Space (set 2)

static struct BurnRomInfo tnextspc2RomDesc[] = {
	{ "ns_4.bin",		0x20000, 0x4617cba3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ns_3.bin",		0x20000, 0xa6c47fef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ns_1.bin",		0x10000, 0xfc26853c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b3.ic49",		0x10000, 0x2bddf94d, 3 | BRF_GRA },           //  3 Graphics
	{ "b7.ic53",		0x10000, 0xa8b13a9a, 3 | BRF_GRA },           //  4
	{ "b4.ic50",		0x10000, 0x80c6c841, 3 | BRF_GRA },           //  5
	{ "b8.ic54",		0x10000, 0xbf0762a0, 3 | BRF_GRA },           //  6
	{ "b5.ic51",		0x10000, 0xe487750b, 3 | BRF_GRA },           //  7
	{ "b9.ic55",		0x10000, 0x45d730b9, 3 | BRF_GRA },           //  8
	{ "b6.ic52",		0x10000, 0x0618cf49, 3 | BRF_GRA },           //  9
	{ "b10.ic56",		0x10000, 0xf48819df, 3 | BRF_GRA },           // 10

	{ "2.p2",		0x00100, 0x1f388d48, 4 | BRF_GRA },           // 11 Palette Data
	{ "3.p3",		0x00100, 0x0254533a, 4 | BRF_GRA },           // 12
	{ "1.p1",		0x00100, 0x488fd0e9, 4 | BRF_GRA },           // 13

	{ "5.p5",		0x00400, 0x9c8527bf, 5 | BRF_GRA },           // 14 Palette Look-up Data
	{ "4.p4",		0x00400, 0xcc9ff769, 5 | BRF_GRA },           // 15

	{ "ns_2.bin",		0x08000, 0x05771d48, 6 | BRF_GRA },           // 16 Color Look-up Data
};

STD_ROM_PICK(tnextspc2)
STD_ROM_FN(tnextspc2)

struct BurnDriver BurnDrvTnextspc2 = {
	"tnextspc2", "tnextspc", NULL, NULL, "1989",
	"The Next Space (set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tnextspc2RomInfo, tnextspc2RomName, NULL, NULL, TnextspcInputInfo, TnextspcDIPInfo,
	Tnextspc2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// The Next Space (Japan)

static struct BurnRomInfo tnextspcjRomDesc[] = {
	{ "ns_ver1_j4.bin",	0x20000, 0x5cdf710d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ns_ver1_j3.bin",	0x20000, 0xcd9532d0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ns_1.bin",		0x10000, 0xfc26853c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ns_5678.bin",	0x80000, 0x22756451, 3 | BRF_GRA },           //  3 Graphics

	{ "2.p2",		0x00100, 0x1f388d48, 4 | BRF_GRA },           //  4 Palette Data
	{ "3.p3",		0x00100, 0x0254533a, 4 | BRF_GRA },           //  5
	{ "1.p1",		0x00100, 0x488fd0e9, 4 | BRF_GRA },           //  6

	{ "5.p5",		0x00400, 0x9c8527bf, 5 | BRF_GRA },           //  7 Palette Look-up Data
	{ "4.p4",		0x00400, 0xcc9ff769, 5 | BRF_GRA },           //  8

	{ "ns_2.bin",		0x08000, 0x05771d48, 6 | BRF_GRA },           //  9 Color Look-up Data
};

STD_ROM_PICK(tnextspcj)
STD_ROM_FN(tnextspcj)

struct BurnDriver BurnDrvTnextspcj = {
	"tnextspcj", "tnextspc", NULL, NULL, "1989",
	"The Next Space (Japan)\0", NULL, "SNK (Pasadena International Corp. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tnextspcjRomInfo, tnextspcjRomName, NULL, NULL, TnextspcInputInfo, TnextspcDIPInfo,
	TnextspcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};
