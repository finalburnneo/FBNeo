// FB Alpha Fuuki 16 Bit driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "burn_ym2203.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvPriority;
static UINT8 DrvBank;
static UINT8 soundlatch;
static UINT8 DrvOkiBank;
static UINT8 raster_timer;

static INT32 flipscreen;
static INT32 video_char_bank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo GogomileInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gogomile)

static struct BurnInputInfo PbanchoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up	"	},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 8,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pbancho)


static struct BurnDIPInfo GogomileDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x01, 0x01, "Off"			},
	{0x10, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Music"		},
	{0x10, 0x01, 0x02, 0x00, "Off"			},
	{0x10, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x0c, 0x00, "Easy"			},
	{0x10, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x10, 0x01, 0x0c, 0x08, "Hard"			},
	{0x10, 0x01, 0x0c, 0x04, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x10, 0x01, 0x30, 0x10, "Chinese"		},
	{0x10, 0x01, 0x30, 0x30, "Japanese"		},
	{0x10, 0x01, 0x30, 0x00, "Korean"		},
	{0x10, 0x01, 0x30, 0x20, "English"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0xc0, 0x00, "2"			},
	{0x10, 0x01, 0xc0, 0xc0, "3"			},
	{0x10, 0x01, 0xc0, 0x80, "4"			},
	{0x10, 0x01, 0xc0, 0x40, "5"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x1c, 0x04, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x0c, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x1c, 0x00, "Free Play"		},
};

STDDIPINFO(Gogomile)

static struct BurnDIPInfo GogomileoDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x01, 0x01, "Off"			},
	{0x10, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Music"		},
	{0x10, 0x01, 0x02, 0x00, "Off"			},
	{0x10, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x0c, 0x00, "Easy"			},
	{0x10, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x10, 0x01, 0x0c, 0x08, "Hard"			},
	{0x10, 0x01, 0x0c, 0x04, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x10, 0x01, 0x30, 0x10, "Chinese"		},
	{0x10, 0x01, 0x30, 0x30, "Japanese"		},
	{0x10, 0x01, 0x30, 0x00, "Korean"		},
	{0x10, 0x01, 0x30, 0x20, "English"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0xc0, 0x00, "2"			},
	{0x10, 0x01, 0xc0, 0xc0, "3"			},
	{0x10, 0x01, 0xc0, 0x80, "4"			},
	{0x10, 0x01, 0xc0, 0x40, "5"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x1c, 0x18, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x1c, 0x00, "Free Play"		},
};

STDDIPINFO(Gogomileo)

static struct BurnDIPInfo PbanchoDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x01, 0x01, "Off"			},
	{0x10, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    5, "Difficulty"		},
	{0x10, 0x01, 0x1c, 0x08, "Easiest"		},
	{0x10, 0x01, 0x1c, 0x10, "Easy"			},
	{0x10, 0x01, 0x1c, 0x1c, "Normal"		},
	{0x10, 0x01, 0x1c, 0x18, "Hard"			},
	{0x10, 0x01, 0x1c, 0x04, "Hardest"		},

	{0   , 0xfe, 0   ,    3, "Lives (Vs Mode)"	},
	{0x10, 0x01, 0x60, 0x00, "1"			},
	{0x10, 0x01, 0x60, 0x60, "2"			},
	{0x10, 0x01, 0x60, 0x40, "3"			},

	{0   , 0xfe, 0   ,    2, "? Senin Mode ?"	},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Versus Mode"	},
	{0x11, 0x01, 0x02, 0x00, "No"			},
	{0x11, 0x01, 0x02, 0x02, "Yes"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x1c, 0x0c, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x1c, 0x10, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x1c, 0x18, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0xe0, 0x60, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0xa0, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xe0, 0x80, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xe0, 0x40, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xe0, 0xc0, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0xe0, 0x00, "Free Play"		},
};

STDDIPINFO(Pbancho)

static inline void palette_update(INT32 offset)
{
	offset &= 0x3ffe;
	UINT16 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >> 10) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void __fastcall fuuki16_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc000) == 0x700000) {
		*((UINT16*)(DrvPalRAM + (address & 0x3ffe))) = data;
		palette_update(address);
		return;
	}

	if ((address & 0xffffe0) == 0x8c0000) {
		INT32 offset = (address / 2) & 0xf;
		UINT16 *regs = (UINT16*)DrvVidRegs;
		if (regs[offset] != data) {
			if (offset == 0x0e) {
				raster_timer = data & 0xff; // 8 bits or lock up!
			}
		}
		regs[offset] = data;
		return;
	}

	switch (address)
	{
		case 0x8a0000:
			soundlatch = data;
			ZetNmi();
		return;

		case 0x8d0000:
		case 0x8d0002: // unk
		return;

		case 0x8e0000:
			DrvPriority = data & 0x000f;
		return;
	}
}

static void __fastcall fuuki16_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc000) == 0x700000) {
		DrvPalRAM[(address & 0x3fff)^1] = data;
		palette_update(address);
		return;
	}
	switch (address)
	{
		case 0x8a0001:
			soundlatch = data;
			ZetNmi();
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT8 __fastcall fuuki16_main_read_byte(UINT32 address)
{
	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall fuuki16_main_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0x8c0000) {
		//return *((UINT16*)(DrvVidRegs) + ((address / 2) & 0xf));
		INT32 offset = (address / 2) & 0xf;
		UINT16 *regs = (UINT16*)DrvVidRegs;
		return regs[offset];
	}

	switch (address)
	{
		case 0x800000:
			return DrvInputs[0];

		case 0x810000:
			return DrvInputs[1];

		case 0x880000:
			return (DrvDips[1] * 256) + DrvDips[0];
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static void bankswitch(INT32 data)
{
	if (data < 3)
	{
		DrvBank = data;

		INT32 bank = 0x8000 + (data * 0x8000);
		ZetMapMemory(DrvZ80ROM + bank, 0x8000, 0xffff, MAP_ROM);
	}
}

static void oki_bankswitch(INT32 data)
{
	INT32 bank = ((data & 6) >> 1) * 0x40000;

	DrvOkiBank = data;

	MSM6295SetBank(0, DrvSndROM + bank, 0, 0x3ffff);
}

static void __fastcall fuuki16_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x11:
		return;		// nop

		case 0x20:
			oki_bankswitch(data);
		return;

		case 0x30:
		return;		// nop

		case 0x40:
		case 0x41:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x50:
		case 0x51:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0x61:
			MSM6295Command(0, data);
		return;
	}
}

static UINT8 __fastcall fuuki16_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x11:
			return soundlatch;

		case 0x40:
		case 0x41:
			return BurnYM2203Read(0, port & 1);

		case 0x50:
		case 0x51:
			return BurnYM3812Read(0, port & 1);

		case 0x60:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static tilemap_callback( layer0 )
{
	UINT16 *ram = (UINT16*)DrvVidRAM0;
	UINT16 attr = ram[2 * offs + 1];

	TILE_SET_INFO(0, ram[offs * 2], attr, TILE_FLIPYX((attr >> 6) & 3)); // hack! y flipping is broken. see the waves and highscore table in pbancho
}

static tilemap_callback( layer1 )
{
	UINT16 *ram = (UINT16*)DrvVidRAM1;
	UINT16 attr = ram[2 * offs + 1];

	TILE_SET_INFO(1, ram[offs * 2], attr, TILE_FLIPYX((attr >> 6) & 3));
}

static tilemap_callback( layer2 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM2 + video_char_bank);
	UINT16 attr = ram[2 * offs + 1];

	TILE_SET_INFO(2, ram[offs * 2], attr, TILE_FLIPYX((attr >> 6) & 3));
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvYM2203SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)SekTotalCycles() * nSoundRate / 16000000;
}

static double DrvYM2203GetTime()
{
	return (double)SekTotalCycles() / 16000000;
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6000000;
}

static int DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	BurnYM2203Reset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;
	DrvPriority = 0;
	DrvBank = 0;
	raster_timer = 0;

	oki_bankswitch(0);
	MSM6295Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROM2		= Next; Next += 0x800000;
	DrvGfxROM3		= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x2001 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvZ80RAM		= Next; Next += 0x002000;
	DrvVidRAM0		= Next; Next += 0x002000;
	DrvVidRAM1		= Next; Next += 0x002000;
	DrvVidRAM2		= Next; Next += 0x004000;
	DrvSprRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x004000;
	DrvVidRegs		= Next; Next += 0x000020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}


static void expand_4bpp_pixels(UINT8 *src, INT32 len)
{
	// byteswap (each rom is byteswapped)
	BurnByteswap(src,len);

	// expand byte into nibbles
	for (INT32 i = (len - 1); i >= 0; i--) {
		src[i*2+0] = src[i] >> 4;
		src[i*2+1] = src[i] & 0xf;
	}
}

static void expand_8bpp_pixels(UINT8 *src, INT32 len)
{
	// swap words (each rom is byteswapped)
	for (INT32 i = 0; i < len; i+=4) {
		UINT16 t = *((UINT16*)(src + i + 0));
		*((UINT16*)(src + i + 0)) = *((UINT16*)(src + i + 2));
		*((UINT16*)(src + i + 2)) = t;
	}

	// swap nibbles around a bit
	for (INT32 i = 0; i < len; i+=2) {
		UINT8 a = src[i];
		UINT8 b = src[i+1];

		src[i+1] = ((b << 4) & 0xf0) | (a & 0xf);
		src[i+0] = (b & 0xf0) | ((a >> 4) & 0xf);
	}
}

static INT32 DrvInit(INT32 nGame)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  5, 2)) return 1;

		if (nGame == 0)
		{
			if (BurnLoadRom(DrvGfxROM2 + 0x0400000,  6, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x0000001,  7, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x0400001,  8, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  9, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x0000000, 10, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvGfxROM2 + 0x0000001,  6, 2)) return 1;
			//memcpy (DrvGfxROM2 + 0x400000, DrvGfxROM2, 0x400000);

			if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  7, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x0000000,  8, 1)) return 1;
		}

		expand_4bpp_pixels(DrvGfxROM0, 0x200000);
		expand_4bpp_pixels(DrvGfxROM1, 0x200000);
		expand_8bpp_pixels(DrvGfxROM2, 0x800000);
		expand_4bpp_pixels(DrvGfxROM3, 0x200000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x400000, 0x40ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,	0x500000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0x502000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	0x504000, 0x507fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x600000, 0x601fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x608000, 0x609fff, MAP_RAM); // mirror
	SekMapMemory(DrvPalRAM,		0x700000, 0x703fff, MAP_ROM); // written in handler
	SekSetWriteWordHandler(0,	fuuki16_main_write_word);
	SekSetWriteByteHandler(0,	fuuki16_main_write_byte);
	SekSetReadWordHandler(0,	fuuki16_main_read_word);
	SekSetReadByteHandler(0,	fuuki16_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x6000, 0x7fff, MAP_RAM);
	// bank 8000-ffff
	ZetSetOutHandler(fuuki16_sound_write_port);
	ZetSetInHandler(fuuki16_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 3580000, &DrvFMIRQHandler, DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(6000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.30, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 3580000, NULL, DrvYM2203SynchroniseStream, DrvYM2203GetTime, 1);
	BurnTimerAttachSek(16000000);
	BurnYM2203SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.85, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x400000, 0x000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x800000, 0x400, 0x3f); // bpp is actually, 8, but handled as 4
	GenericTilemapSetGfx(2, DrvGfxROM3, 4,  8,  8, 0x400000, 0xc00, 0x3f);
	GenericTilemapSetTransparent(0, 0x0f);
	GenericTilemapSetTransparent(1, 0xff);
	GenericTilemapSetTransparent(2, 0x0f);

	// enable line drawing. line drawing is fastest for single scanline updates
	GenericTilemapSetScrollRows(0, 512);
	GenericTilemapSetScrollRows(1, 512);
	GenericTilemapSetScrollRows(2, 256);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();

	BurnYM3812Exit();
	BurnYM2203Exit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x4000/2; i++)
	{
		INT32 r = (p[i] >> 10) & 0x1f;
		INT32 g = (p[i] >>  5) & 0x1f;
		INT32 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;

	int max_x = nScreenWidth;
	int max_y = nScreenHeight;

	for (INT32 offs = (0x2000 - 8) / 2; offs >=0; offs -= 8 / 2)
	{
		INT32 x, y, xstart, ystart, xend, yend, xinc, yinc;
		INT32 xnum, ynum, xzoom, yzoom, flipx, flipy;
		INT32 pri_mask;

		INT32 sx = spriteram16[offs + 0];
		INT32 sy = spriteram16[offs + 1];
		INT32 attr = spriteram16[offs + 2];
		INT32 code = spriteram16[offs + 3];

		INT32 color = (attr & 0x3f) * 16 + 0x800;

		if (sx & 0x400)
			continue;

		flipx = sx & 0x0800;
		flipy = sy & 0x0800;

		xnum = ((sx >> 12) & 0xf) + 1;
		ynum = ((sy >> 12) & 0xf) + 1;

		xzoom = 16 * 8 - (8 * ((attr >> 12) & 0xf)) / 2;
		yzoom = 16 * 8 - (8 * ((attr >>  8) & 0xf)) / 2;

		switch ((attr >> 6) & 3)
		{
			case 3: pri_mask = 0xf0 | 0xcc | 0xaa;  break;  // behind all layers
			case 2: pri_mask = 0xf0 | 0xcc;         break;  // behind fg + middle layer
			case 1: pri_mask = 0xf0;                break;  // behind fg layer
			case 0:
			default:    pri_mask = 0;                       // above all
		}

		sx = (sx & 0x1ff) - (sx & 0x200);
		sy = (sy & 0x1ff) - (sy & 0x200);

		if (flipscreen)
		{
			flipx = !flipx;     sx = max_x - sx - xnum * 16;
			flipy = !flipy;     sy = max_y - sy - ynum * 16;
		}

		if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
		else        { xstart = 0;       xend = xnum;  xinc = +1; }

		if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
		else        { ystart = 0;       yend = ynum;  yinc = +1; }

		for (y = ystart; y != yend; y += yinc)
		{
			for (x = xstart; x != xend; x += xinc)
			{
				if (xzoom == (16*8) && yzoom == (16*8)) {
					RenderPrioSprite(pTransDraw, DrvGfxROM0, code++, color, 0xf, sx + x * 16, sy + y * 16, flipx, flipy, 16, 16, pri_mask);
				} else {
					RenderZoomedPrioSprite(pTransDraw, DrvGfxROM0, code++, color, 0xf, sx + (x * xzoom) / 8, sy + (y * yzoom) / 8, flipx, flipy, 16, 16, (0x10000/0x10/8) * (xzoom + 8),(0x10000/0x10/8) * (yzoom + 8), pri_mask);
				}
			}
		}
	}
}

static INT32 previous_previous_line = 0;
static INT32 previous_line = 0;

static void set_clipping(INT32 scanline)
{
	// Set the clip to the current scanline as the final point, and the starting point is one pixel higher than the
	// last drawing pass. This allows for a partial screen update
	GenericTilesSetClip(-1, -1, previous_line, scanline+1);

	// save parameters for use later (updating scroll rows)
	previous_previous_line = previous_line;
	previous_line = scanline+1;
}

static INT32 DrvDraw()
{
	static const int pri_table[6][3] = {
		{ 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 }
	};

	UINT16 *regs = (UINT16*)DrvVidRegs;

	flipscreen = regs[0xf] & 1;

	INT32 layer[3] = { pri_table[DrvPriority][2], pri_table[DrvPriority][1], pri_table[DrvPriority][0] };

	INT32 scrolly_offs = regs[6] - 0x1f3;
	INT32 scrollx_offs = regs[7] - 0x3f6;

	GenericTilemapSetScrollY(0, regs[0] + scrolly_offs);
	GenericTilemapSetScrollY(1, regs[2] + scrolly_offs);
	GenericTilemapSetScrollY(2, regs[4] + scrolly_offs);

	for (INT32 i = previous_previous_line; i < previous_line; i++) {
		GenericTilemapSetScrollRow(0, (i + regs[0] + scrolly_offs) & 0x1ff, regs[1] + scrollx_offs);
		GenericTilemapSetScrollRow(1, (i + regs[2] + scrolly_offs) & 0x1ff, regs[3] + scrollx_offs);
		GenericTilemapSetScrollRow(2, (i + regs[4] + scrolly_offs) & 0x0ff, regs[5] + scrollx_offs + 0x10);
	}

	video_char_bank = (regs[0xf] & 0x40) * 0x80; // 0x2000

	for (INT32 i = 0; i < 3; i++) {
		if (nBurnLayer & (1 << layer[i])) GenericTilemapDraw(layer[i], pTransDraw, 1 << i);
	}

	return 0;
}

static INT32 DrvFrame()
{	
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2 * sizeof (UINT16));

		for (INT32 i = 0 ; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	// set up drawing for this frame
	// we are on line 0
	previous_line = 0;

	// clear drawing surface
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x1fff;
		pPrioDraw[i] = 0;
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((nCyclesTotal[0] / nInterleave) * (i + 1));

		if (i == 239+3) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO); // vblank - +3 for occasional coin-up issues in gogomile in demo mode
		if (i == 0) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO); // level 1 - start at line 0, fixes glitches during bootup/huge tilemap animations
		if (i == raster_timer+1) {
			// update layers when calling for a raster update
			if (i < nScreenHeight) {
				set_clipping(i);
				BurnDrvRedraw();
			}
			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
			raster_timer = nInterleave - 2; // fix glitch in middle of pink x+y scroll "mile smile" screen
		}

		nCyclesDone[1] += BurnTimerUpdateYM3812((nCyclesTotal[1] / nInterleave) * (i + 1));
	}

	BurnTimerEndFrame(nCyclesTotal[0]);
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw)
	{
		if (DrvRecalc) {
			DrvPaletteUpdate();
			DrvRecalc = 0;
		}

		set_clipping(nScreenHeight-1);

		DrvDraw();

		GenericTilesClearClip();

		if (nSpriteEnable & 1) draw_sprites();

		BurnTransferCopy(DrvPalette);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		ZetOpen(0);
		SekOpen(0);
		BurnYM3812Scan(nAction, pnMin);
		BurnYM2203Scan(nAction, pnMin);
		SekClose();
		ZetClose();

		MSM6295Scan(0, nAction);

		SCAN_VAR(DrvBank);
		SCAN_VAR(DrvOkiBank);
		SCAN_VAR(DrvPriority);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(raster_timer);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(DrvBank);
		ZetClose();

		oki_bankswitch(DrvOkiBank);
	}

	return 0;
}


// Susume! Mile Smile / Go Go! Mile Smile (newer)

static struct BurnRomInfo gogomileRomDesc[] = {
	{ "fp2n.rom2",		0x080000, 0xe73583a0, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "fp1n.rom1",		0x080000, 0x7b110824, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fs1.rom24",		0x020000, 0x4e4bd371, 2 | BRF_PRG | BRF_ESS }, //  2 z80 code

	{ "lh537k2r.rom20",	0x200000, 0x525dbf51, 3 | BRF_GRA },           //  3 sprites

	{ "lh5370h6.rom3",	0x200000, 0xe2ca7107, 4 | BRF_GRA },           //  4 16 x 16 x 4bpp (layer 0) tiles

	{ "lh5370h8.rom11",	0x200000, 0x9961c925, 5 | BRF_GRA },           //  5 16 x 16 x 8bpp (layer 1) tiles
	{ "lh5370ha.rom12",	0x200000, 0x5f2a87de, 5 | BRF_GRA },           //  6
	{ "lh5370h7.rom15",	0x200000, 0x34921680, 5 | BRF_GRA },           //  7
	{ "lh5370h9.rom16",	0x200000, 0xe0118483, 5 | BRF_GRA },           //  8

	{ "lh5370hb.rom19",	0x200000, 0xbd1e896f, 6 | BRF_GRA },           //  9 8 x 8 x 4bpp (layer 2) tiles

	{ "lh538n1d.rom25",	0x100000, 0x01622a95, 7 | BRF_SND },           // 10 oki samples
};

STD_ROM_PICK(gogomile)
STD_ROM_FN(gogomile)

static INT32 GogomileInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvGogomile = {
	"gogomile", NULL, NULL, NULL, "1995",
	"Susume! Mile Smile / Go Go! Mile Smile (newer)\0", NULL, "Fuuki", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, gogomileRomInfo, gogomileRomName, NULL, NULL, GogomileInputInfo, GogomileDIPInfo,
	GogomileInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// Susume! Mile Smile / Go Go! Mile Smile (older)

static struct BurnRomInfo gogomileoRomDesc[] = {
	{ "fp2.rom2",		0x080000, 0x28fd3e4e, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "fp1.rom1",		0x080000, 0x35a5fc45, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fs1.rom24",		0x020000, 0x4e4bd371, 2 | BRF_PRG | BRF_ESS }, //  2 z80 code

	{ "lh537k2r.rom20",	0x200000, 0x525dbf51, 3 | BRF_GRA },           //  3 sprites

	{ "lh5370h6.rom3",	0x200000, 0xe2ca7107, 4 | BRF_GRA },           //  4 16 x 16 x 4bpp (layer 0) tiles

	{ "lh5370h8.rom11",	0x200000, 0x9961c925, 5 | BRF_GRA },           //  5 16 x 16 x 8bpp (layer 1) tiles
	{ "lh5370ha.rom12",	0x200000, 0x5f2a87de, 5 | BRF_GRA },           //  6
	{ "lh5370h7.rom15",	0x200000, 0x34921680, 5 | BRF_GRA },           //  7
	{ "lh5370h9.rom16",	0x200000, 0xe0118483, 5 | BRF_GRA },           //  8

	{ "lh5370hb.rom19",	0x200000, 0xbd1e896f, 6 | BRF_GRA },           //  9 8 x 8 x 4bpp (layer 2) tiles

	{ "lh538n1d.rom25",	0x100000, 0x01622a95, 7 | BRF_SND },           // 10 oki samples
};

STD_ROM_PICK(gogomileo)
STD_ROM_FN(gogomileo)

struct BurnDriver BurnDrvGogomileo = {
	"gogomileo", "gogomile", NULL, NULL, "1995",
	"Susume! Mile Smile / Go Go! Mile Smile (older)\0", NULL, "Fuuki", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, gogomileoRomInfo, gogomileoRomName, NULL, NULL, GogomileInputInfo, GogomileoDIPInfo,
	GogomileInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// Gyakuten!! Puzzle Bancho (Japan)

static struct BurnRomInfo pbanchoRomDesc[] = {
	{ "no1.rom2",		0x080000, 0x1b4fd178, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "no2,rom1",		0x080000, 0x9cf510a5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "no4.rom23",		0x020000, 0xdfbfdb81, 2 | BRF_PRG | BRF_ESS }, //  2 z80 code

	{ "58.rom20",		0x200000, 0x4dad0a2e, 3 | BRF_GRA },           //  3 sprites

	{ "60.rom3",		0x200000, 0xa50a3c1b, 4 | BRF_GRA },           //  4 16 x 16 x 4bpp (layer 0) tiles

	{ "61.rom11",		0x200000, 0x7f1213b9, 5 | BRF_GRA },           //  5 16 x 16 x 8bpp (layer 1) tiles
	{ "59.rom15",		0x200000, 0xb83dcb70, 5 | BRF_GRA },           //  6

	{ "60.rom3",		0x200000, 0xa50a3c1b, 6 | BRF_GRA },           //  7 8 x 8 x 4bpp (layer 2) tiles

	{ "n03.rom25",		0x040000, 0xa7bfb5ea, 7 | BRF_SND },           //  8 oki samples
};

STD_ROM_PICK(pbancho)
STD_ROM_FN(pbancho)

static INT32 PbanchoInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvPbancho = {
	"pbancho", NULL, NULL, NULL, "1996",
	"Gyakuten!! Puzzle Bancho (Japan)\0", NULL, "Fuuki", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pbanchoRomInfo, pbanchoRomName, NULL, NULL, PbanchoInputInfo, PbanchoDIPInfo,
	PbanchoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};
