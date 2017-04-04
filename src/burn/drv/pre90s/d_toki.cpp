// FB Alpha Toki driver module
// Based on MAME driver by Jarek Parchanski

// Why isn't sound working for jujuba?

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"
#include "msm5205.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80DecROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvBg1RAM;
static UINT8 *DrvBg2RAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvScrollRAM;

static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[3];
static UINT8 DrvDips[3];
static UINT16 DrvInps[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;

static INT32 is_bootleg = 0;

static UINT8 TokibMSM5205Next = 0;
static UINT8 TokibMSM5205Toggle = 0;

static struct BurnInputInfo TokiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Toki)

static struct BurnInputInfo TokibInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tokib)

static struct BurnDIPInfo TokiDIPList[]=
{
	// Default Values
	{0x12, 0xff, 0xff, 0xdf, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},
	
	{0   , 0xfe, 0   ,    20, "Coinage"		},
	{0x12, 0x01, 0x1f, 0x15, "6 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x17, "5 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x19, "4 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x1b, "3 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x03, "8 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x1d, "2 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x05, "5 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x07, "3 Coins 2 Credits "	},
	{0x12, 0x01, 0x1f, 0x1f, "1 Coin 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x09, "2 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x13, "1 Coin 2 Credits "	},
	{0x12, 0x01, 0x1f, 0x11, "1 Coin 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x0f, "1 Coin 4 Credits "	},
	{0x12, 0x01, 0x1f, 0x0d, "1 Coin 5 Credits "	},
	{0x12, 0x01, 0x1f, 0x0b, "1 Coin 6 Credits "	},
	{0x12, 0x01, 0x1f, 0x1e, "A 1/1 B 1/2"		},
	{0x12, 0x01, 0x1f, 0x14, "A 2/1 B 1/3"		},
	{0x12, 0x01, 0x1f, 0x0a, "A 3/1 B 1/5"		},
	{0x12, 0x01, 0x1f, 0x00, "A 5/1 B 1/6"		},
	{0x12, 0x01, 0x1f, 0x01, "Free Play"		},
	
	{0   , 0xfe, 0   ,    2, "Joysticks"		},
	{0x12, 0x01, 0x20, 0x20, "1"			},
	{0x12, 0x01, 0x20, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x40, 0x40, "Upright"		},
	{0x12, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "Infinite (Cheat)"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x08, "50000 150000"		},
	{0x13, 0x01, 0x0c, 0x00, "70000 140000 210000"	},
	{0x13, 0x01, 0x0c, 0x0c, "70000"		},
	{0x13, 0x01, 0x0c, 0x04, "100000 200000"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x20, "Easy"			},
	{0x13, 0x01, 0x30, 0x30, "Medium"		},
	{0x13, 0x01, 0x30, 0x10, "Hard"			},
	{0x13, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x00, "No"			},
	{0x13, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x00, "Off"			},
	{0x13, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "Debug Mode (Hack)"		},
	{0x14, 0x01, 0x0f, 0x00, "Off"			},
	{0x14, 0x01, 0x0f, 0x0f, "On"			},
};

STDDIPINFO(Toki)

static struct BurnDIPInfo TokibDIPList[]=
{
	{0x12, 0xff, 0xff, 0xdf, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    20, "Coinage"		},
	{0x12, 0x01, 0x1f, 0x15, "6 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x17, "5 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x19, "4 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x1b, "3 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x03, "8 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x1d, "2 Coins 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x05, "5 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x07, "3 Coins 2 Credits "	},
	{0x12, 0x01, 0x1f, 0x1f, "1 Coin 1 Credits "	},
	{0x12, 0x01, 0x1f, 0x09, "2 Coins 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x13, "1 Coin 2 Credits "	},
	{0x12, 0x01, 0x1f, 0x11, "1 Coin 3 Credits "	},
	{0x12, 0x01, 0x1f, 0x0f, "1 Coin 4 Credits "	},
	{0x12, 0x01, 0x1f, 0x0d, "1 Coin 5 Credits "	},
	{0x12, 0x01, 0x1f, 0x0b, "1 Coin 6 Credits "	},
	{0x12, 0x01, 0x1f, 0x1e, "A 1/1 B 1/2"		},
	{0x12, 0x01, 0x1f, 0x14, "A 2/1 B 1/3"		},
	{0x12, 0x01, 0x1f, 0x0a, "A 3/1 B 1/5"		},
	{0x12, 0x01, 0x1f, 0x00, "A 5/1 B 1/6"		},
	{0x12, 0x01, 0x1f, 0x01, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Joysticks"		},
	{0x12, 0x01, 0x20, 0x20, "1"			},
	{0x12, 0x01, 0x20, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x40, 0x40, "Upright"		},
	{0x12, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "Infinite (Cheat)"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x08, "50000 150000"		},
	{0x13, 0x01, 0x0c, 0x00, "70000 140000 210000"	},
	{0x13, 0x01, 0x0c, 0x0c, "70000"		},
	{0x13, 0x01, 0x0c, 0x04, "100000 200000"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x20, "Easy"			},
	{0x13, 0x01, 0x30, 0x30, "Medium"		},
	{0x13, 0x01, 0x30, 0x10, "Hard"			},
	{0x13, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x00, "No"			},
	{0x13, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x00, "Off"			},
	{0x13, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Tokib)

static inline void palette_write(INT32 offset)
{
	UINT16 p = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + offset)));

	UINT8 r = (p >> 0) & 0x0f;
	UINT8 g = (p >> 4) & 0x0f;
	UINT8 b = (p >> 8) & 0x0f;

	DrvPalette[offset / 2] = BurnHighCol((r*16)+r, (g*16)+g, (b*16)+b, 0);
}

static void __fastcall toki_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff800) == 0x6e000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write(address & 0x7fe);
		return;
	}

	switch (address)
	{
		case 0x75001: // bootleg
		{
			*soundlatch = data & 0xff;
			ZetOpen(0);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
		}
		return;

		case 0x80000:
		case 0x80001:
		case 0x80002:
		case 0x80003:
		case 0x80004:
		case 0x80005:
		case 0x80006:
		case 0x80007:
		case 0x80008:
		case 0x80009:
		case 0x8000a:
		case 0x8000b:
		case 0x8000c:
		case 0x8000d:
			seibu_main_word_write(address & 0xf, data);
		return;
	}
}

static void __fastcall toki_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xff800) == 0x6e000) {
		*((UINT16*)(DrvPalRAM + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
		palette_write(address & 0x7fe);
		return;
	}

	switch (address)
	{
		case 0x75004: // bootleg
		case 0x75006:
		case 0x75008:
		case 0x7500a:
			*((UINT16*)(DrvScrollRAM + (address - 0x75004))) = BURN_ENDIAN_SWAP_INT16(data);
		return;

		case 0x80000:
		case 0x80001:
		case 0x80002:
		case 0x80003:
		case 0x80004:
		case 0x80005:
		case 0x80006:
		case 0x80007:
		case 0x80008:
		case 0x80009:
		case 0x8000a:
		case 0x8000b:
		case 0x8000c:
		case 0x8000d:
			seibu_main_word_write(address & 0xf, data);
		return;
	}
}

static UINT8 __fastcall toki_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x80000:
		case 0x80001:
		case 0x80002:
		case 0x80003:
		case 0x80004:
		case 0x80005:
		case 0x80006:
		case 0x80007:
		case 0x80008:
		case 0x80009:
		case 0x8000a:
		case 0x8000b:
		case 0x8000c:
		case 0x8000d:
			return seibu_main_word_read(address & 0x0f);

		case 0xc0000:
		case 0xc0001:
			return DrvDips[~address & 1];

		case 0xc0002: return DrvInps[0] >> 8;
		case 0xc0003: return DrvInps[0];

		case 0xc0004: return DrvInps[1] >> 8;
		case 0xc0005: return DrvInps[1];

		case 0xc000f:
			return 0xff;
	}

	return 0;
}

static UINT16 __fastcall toki_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x72000:// watchdog (bootleg)
			return 0;

		case 0x80000:
		case 0x80001:
		case 0x80002:
		case 0x80003:
		case 0x80004:
		case 0x80005:
		case 0x80006:
		case 0x80007:
		case 0x80008:
		case 0x80009:
		case 0x8000a:
		case 0x8000b:
		case 0x8000c:
		case 0x8000d:
			return seibu_main_word_read(address & 0x0f);

		case 0xc0000:
			return (DrvDips[1] << 8) | (DrvDips[0]);

		case 0xc0002:
			return DrvInps[0];

		case 0xc0004:
			return DrvInps[1];
	}

	return 0;
}

static void toki_adpcm_control_w(INT32 data)
{
	INT32 bankaddress = data & 1;

	UINT8 *RAM = DrvZ80ROM + 0x8000 + bankaddress * 0x4000;

	ZetMapArea(0x8000, 0xbfff, 0, RAM);
	ZetMapArea(0x8000, 0xbfff, 2, RAM);

	MSM5205ResetWrite(0, data & 0x08);
}

static void __fastcall tokib_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			toki_adpcm_control_w(data);
		return;

		case 0xe400:
			TokibMSM5205Next = data;
		return;

		case 0xec00:
		case 0xec08:
			BurnYM3812Write(0, 0, data);
		return;

		case 0xec01:
		case 0xec09:
			BurnYM3812Write(0, 1, data);
		return;
	}
}

static UINT8 __fastcall tokib_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xec00:
			return BurnYM3812Read(0, 0);

		case 0xf800:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	return 0;
}

inline static INT32 TokibSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 4000000);
}

static void toki_adpcm_int()
{
	MSM5205DataWrite(0, TokibMSM5205Next);
	TokibMSM5205Next >>= 4;
	
	TokibMSM5205Toggle ^= 1;
	if (TokibMSM5205Toggle) ZetNmi();
}	

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	if (is_bootleg)
	{
		ZetOpen(0);
		ZetReset();
		ZetClose();

		BurnYM3812Reset();
	}
	else
	{
		seibu_sound_reset();
	}

	return 0;
}

static INT32 TokibDoReset()
{
	TokibMSM5205Next = 0;
	TokibMSM5205Toggle = 0;
	
	MSM5205Reset();
	
	return DrvDoReset();
}

static void tokib_rom_decode()
{
	UINT8 *temp = (UINT8*)malloc(65536 * 2);
	INT32 i, offs, len;
	UINT8 *rom;

	len = 0x100000;
	rom = DrvGfxROM1;
	for (i = 0; i < len; i++) rom[i] ^= 0xff;

	len = 0x080000;
	rom = DrvGfxROM2;
	for (offs = 0; offs < len; offs += 0x20000)
	{
		UINT8 *base = &rom[offs];
		memcpy (temp, base, 65536 * 2);
		for (i = 0; i < 16; i++)
		{
			memcpy (&base[0x00000 + i * 0x800], &temp[0x0000 + i * 0x2000], 0x800);
			memcpy (&base[0x10000 + i * 0x800], &temp[0x0800 + i * 0x2000], 0x800);
			memcpy (&base[0x08000 + i * 0x800], &temp[0x1000 + i * 0x2000], 0x800);
			memcpy (&base[0x18000 + i * 0x800], &temp[0x1800 + i * 0x2000], 0x800);
		}
	}
	len = 0x080000;
	rom = DrvGfxROM3;
	for (offs = 0; offs < len; offs += 0x20000)
	{
		UINT8 *base = &rom[offs];
		memcpy (temp, base, 65536 * 2);
		for (i = 0; i < 16; i++)
		{
			memcpy (&base[0x00000 + i * 0x800], &temp[0x0000 + i * 0x2000], 0x800);
			memcpy (&base[0x10000 + i * 0x800], &temp[0x0800 + i * 0x2000], 0x800);
			memcpy (&base[0x08000 + i * 0x800], &temp[0x1000 + i * 0x2000], 0x800);
			memcpy (&base[0x18000 + i * 0x800], &temp[0x1800 + i * 0x2000], 0x800);
		}
	}

	if (temp) {
		free (temp);
		temp = NULL;
	}
}


static INT32 TokibGfxDecode()
{
	INT32 Plane0[4]  = { 4096*8*8*3,  4096*8*8*2,  4096*8*8*1,  4096*8*8*0 };
	INT32 Plane1[4]  = { 8192*16*16*3,8192*16*16*2,8192*16*16*1,8192*16*16*0 };
	INT32 Plane2[4]  = { 4096*16*16*3,4096*16*16*2,4096*16*16*1,4096*16*16*0 };
	INT32 XOffs0[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs0[16] = { STEP16(0,8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(0x8000*8,1) };
	INT32 YOffs1[16] = { STEP8(0,8), STEP8(0x10000*8,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(4096, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(8192, 4, 16, 16, Plane1, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(4096, 4, 16, 16, Plane2, XOffs1, YOffs1, 0x040, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x080000);

	GfxDecode(4096, 4, 16, 16, Plane2, XOffs1, YOffs1, 0x040, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 4096*16*8+0, 4096*16*8+4, 0, 4 };
	INT32 Plane1[4]  = { 2*4, 3*4, 0*4, 1*4 };
	INT32 XOffs0[8]  = { STEP4(3,-1), STEP4(11,-1) };
	INT32 YOffs0[8]  = { STEP8(0, 16) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(19,-1), STEP4((64*8)+3, -1), STEP4((64*8+16+3),-1) };
	INT32 YOffs1[32] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(4096, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(8192, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(4096, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x080000);

	GfxDecode(4096, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x060000;

	SeibuZ80ROM	= Next;
	DrvZ80ROM	= Next; Next += 0x020000;
	SeibuZ80DecROM	= Next;
	DrvZ80DecROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x100000;
	
	MSM6295ROM	= Next;
	DrvSndROM       = Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam		= Next;

	DrvBg1RAM	= Next; Next += 0x000800;
	DrvBg2RAM	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x000800;
	Drv68KRAM	= Next; Next += 0x00e000;
	SeibuZ80RAM	= Next;
	DrvZ80RAM	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvSprBuf	= Next; Next += 0x000800;

	DrvScrollRAM	= Next; Next += 0x000400;

	soundlatch	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	is_bootleg = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();
	
	if (BurnLoadRom(Drv68KROM + 0x00001,	 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x00000,	 1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40001,	 2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40000,	 3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x00000,     4, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x10000,     5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,    6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x10000,    7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,    8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x80000,    9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00000,   10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x00000,   11, 1)) return 1;
		
	if (BurnLoadRom(DrvSndROM  + 0x00000,   12, 1)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 	0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x06d7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x06d800, 0x06dfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x06e000, 0x06e7ff, MAP_ROM);
	SekMapMemory(DrvBg1RAM,		0x06e800, 0x06efff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x06f000, 0x06f7ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x06f800, 0x06ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x0a0000, 0x0a0057, MAP_RAM);
	SekSetWriteByteHandler(0,	toki_write_byte);
	SekSetWriteWordHandler(0,	toki_write_word);
	SekSetReadByteHandler(0,	toki_read_byte);
	SekSetReadWordHandler(0,	toki_read_word);
	SekClose();

	UINT8 *Temp = (UINT8*)BurnMalloc(0x20000);
	memcpy(Temp, DrvSndROM, 0x20000);
	for (INT32 i = 0; i < 0x20000; i++ ) {
		DrvSndROM[i] = Temp[BITSWAP24(i, 23, 22, 21, 20, 19, 18, 17, 16, 13, 14, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)];
	}
	BurnFree(Temp);

	seibu_sound_init(0, 0x2000, 3579545, 3579545, 1000000 / 132);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 TokipInit()
{
	is_bootleg = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();
	
	if (BurnLoadRom(Drv68KROM + 0x00001,	 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x00000,	 1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40001,	 2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40000,	 3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x00000,     4, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x10000,     5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,    6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x10000,    7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,    8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x00001,    9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x40000,   10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x40001,   11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x80000,   12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x80001,   13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0xc0000,   14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0xc0001,   15, 2)) return 1;
	
	if (BurnLoadRom(DrvGfxROM2 + 0x00000,   16, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,   17, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x40000,   18, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x40001,   19, 2)) return 1;
	
	if (BurnLoadRom(DrvGfxROM3 + 0x00000,   20, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x00001,   21, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x40000,   22, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x40001,   23, 2)) return 1;
		
	if (BurnLoadRom(DrvSndROM  + 0x00000,   24, 1)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 	0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x06d7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x06d800, 0x06dfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x06e000, 0x06e7ff, MAP_ROM);
	SekMapMemory(DrvBg1RAM,		0x06e800, 0x06efff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x06f000, 0x06f7ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x06f800, 0x06ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x0a0000, 0x0a0057, MAP_RAM);
	SekSetWriteByteHandler(0,	toki_write_byte);
	SekSetWriteWordHandler(0,	toki_write_word);
	SekSetReadByteHandler(0,	toki_read_byte);
	SekSetReadWordHandler(0,	toki_read_word);
	SekClose();

	UINT8 *Temp = (UINT8*)BurnMalloc(0x20000);
	memcpy(Temp, DrvSndROM, 0x20000);
	for (INT32 i = 0; i < 0x20000; i++ ) {
		DrvSndROM[i] = Temp[BITSWAP24(i, 23, 22, 21, 20, 19, 18, 17, 16, 13, 14, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)];
	}
	BurnFree(Temp);

	seibu_sound_init(0, 0x2000, 3579545, 3579545, 1000000 / 132);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 JujubaInit()
{
	is_bootleg = 0;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();
	
	if (BurnLoadRom(Drv68KROM + 0x20001,	 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x20000,	 1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x00001,	 2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x00000,	 3, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40001,	 4, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x40000,	 5, 2)) return 1;
	
	if (BurnLoadRom(DrvZ80ROM + 0x00000,     6, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x10000,     7, 1)) return 1;
	memcpy(DrvZ80ROM, DrvZ80ROM + 0x2000, 0x2000);
		
	if (BurnLoadRom(DrvGfxROM0 + 0x00000,    8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x10000,    9, 1)) return 1;
		
	if (BurnLoadRom(DrvGfxROM1 + 0x00000,   10, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x00001,   11, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x40000,   12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x40001,   13, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x80000,   14, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x80001,   15, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0xc0000,   16, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0xc0001,   17, 2)) return 1;
		
	if (BurnLoadRom(DrvGfxROM2 + 0x00001,   18, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x20001,   19, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x40001,   20, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x60001,   21, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x00000,   22, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x20000,   23, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x40000,   24, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x60000,   25, 2)) return 1;
		
	if (BurnLoadRom(DrvGfxROM3 + 0x00001,   26, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x20001,   27, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x40001,   28, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x60001,   29, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x00000,   30, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x20000,   31, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x40000,   32, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x60000,   33, 2)) return 1;
	
	if (BurnLoadRom(DrvSndROM  + 0x00000,   34, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x10000,   35, 1)) return 1;
	
	UINT16 *PrgRom = (UINT16*)Drv68KROM;
	for (INT32 i = 0; i < 0x30000; i++) {
		PrgRom[i] = BITSWAP16(PrgRom[i], 15, 12, 13, 14, 11, 10, 9, 8, 7, 6, 5, 3, 4, 2, 1, 0);
	}
	
	UINT8 *Decrypt = DrvZ80DecROM;
	UINT8 *Rom = DrvZ80ROM;
	memcpy(Decrypt, Rom, 0x2000);
	for (INT32 i = 0;i < 0x2000; i++) {
		UINT8 Src = Decrypt[i];
		Rom[i] = Src ^ 0x55;
	}

	DrvGfxDecode();
	
	UINT8 *Temp = (UINT8*)BurnMalloc(0x20000);
	memcpy(Temp, DrvSndROM, 0x20000);
	for (INT32 i = 0; i < 0x20000; i++ ) {
		DrvSndROM[i] = Temp[BITSWAP24(i, 23, 22, 21, 20, 19, 18, 17, 16, 13, 14, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)];
	}
	BurnFree(Temp);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 	0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x06d7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x06d800, 0x06dfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x06e000, 0x06e7ff, MAP_ROM);
	SekMapMemory(DrvBg1RAM,		0x06e800, 0x06efff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x06f000, 0x06f7ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x06f800, 0x06ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,	0x0a0000, 0x0a0057, MAP_RAM);
	SekSetWriteByteHandler(0,	toki_write_byte);
	SekSetWriteWordHandler(0,	toki_write_word);
	SekSetReadByteHandler(0,	toki_read_byte);
	SekSetReadWordHandler(0,	toki_read_word);
	SekClose();

	seibu_sound_init(0, 0x2000, 3579545, 3579545, 1000000 / 132);

	ZetOpen(0);
	ZetMapArea(0x0000, 0x1fff, 2, DrvZ80DecROM, DrvZ80ROM );
	ZetClose();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 TokibInit()
{
	is_bootleg = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x00001,	 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000,	 1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x40001,	 2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x40000,	 3, 2)) return 1;

		for (INT32 i = 0; i < 4; i++)
			if (BurnLoadRom(DrvGfxROM0 + i * 0x8000,  5 + i, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x20000,  9 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + i * 0x10000, 17 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3 + i * 0x10000, 25 + i, 1)) return 1;
		}
		
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,   4, 1)) return 1;

		tokib_rom_decode();

		TokibGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 	0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x06dfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x06e000, 0x06e7ff, MAP_ROM);
	SekMapMemory(DrvBg1RAM,		0x06e800, 0x06efff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x06f000, 0x06f7ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x06f800, 0x06ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x071800, 0x0718ff, MAP_RAM);
	SekSetWriteByteHandler(0,	toki_write_byte);
	SekSetWriteWordHandler(0,	toki_write_word);
	SekSetReadByteHandler(0,	toki_read_byte);
	SekSetReadWordHandler(0,	toki_read_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM + 0x8000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM + 0x8000);
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(tokib_sound_write);
	ZetSetReadHandler(tokib_sound_read);
	ZetClose();

	BurnYM3812Init(1, 3579545, NULL, &TokibSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(3579545);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	
	MSM5205Init(0, TokibSynchroniseStream, 384000, toki_adpcm_int, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	TokibDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	if (is_bootleg) {
		BurnYM3812Exit();
		MSM5205Exit();
		ZetExit();
	} else {
		seibu_sound_exit();
	}

	SekExit();

	BurnFree (AllMem);
	
	TokibMSM5205Next = 0;
	TokibMSM5205Toggle = 0;
	is_bootleg = 0;

	return 0;
}

static void draw_text_layer()
{
	UINT16 *vram = (UINT16*)DrvFgRAM;

	for (INT32 offs = 0x040; offs < 0x3c0; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;
		    sy -= 16;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 color = code >> 12;
		    code &= 0xfff;

		if (code == 0) continue;

		Render8x8Tile_Mask(pTransDraw, code, sx, sy, color, 4, 0x0f, 0x100, DrvGfxROM0);
	}
}

static void draw_bg_layer(UINT8 *vidsrc, UINT8 *gfxbase, INT32 transp, INT32 gfxoffs, INT32 scrollx, INT32 scrolly)
{
	UINT16 *vram = (UINT16*)vidsrc;

	scrollx &= 0x1ff;
	scrolly &= 0x1ff;

	for (INT32 offs = 0; offs < 0x800 / 2; offs++)
	{
		INT32 sx = (offs & 0x1f) << 4;
		INT32 sy = (offs >> 5) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 0x200;
		sy -= scrolly;
		if (sy < -15) sy += 0x200;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 color = code >> 12;

		if (transp) {
			Render16x16Tile_Mask_Clip(pTransDraw, code & 0xfff, sx, sy, color, 4, 0x0f, gfxoffs, gfxbase);
		} else {
			Render16x16Tile_Clip(pTransDraw, code & 0xfff, sx, sy, color, 4, gfxoffs, gfxbase);
		}
	}
}

static void draw_bg_layer_by_line(UINT8 *vidsrc, UINT8 *gfxbase, INT32 transp, INT32 gfxoffs, INT32 scrollx, INT32 scrolly, INT32 line)
{
	UINT8 *src;
	UINT16 *vram = (UINT16*)vidsrc;

	INT32 starty = line + scrolly;
	if (starty > 0x1ff) starty -= 0x200;
	starty = (starty & 0x1f0) << 1;

	for (INT32 offs = starty; offs < starty + 0x20; offs++)
	{
		INT32 sx = (offs & 0x1f) << 4;
		INT32 sy = (offs >> 5) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 0x200;
		sy -= scrolly;
		if (sy < -15) sy += 0x200;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs]);
		INT32 color = ((code >> 8) & 0xf0) | gfxoffs;

		if (sx < -15 || sx >= nScreenWidth) continue;
 		src = gfxbase + ((code & 0xfff) << 8) + ((line - sy) << 4);

		sy = line * nScreenWidth;

		if (transp) {
			for (INT32 x = 0; x < 16; x++, sx++) {
				if (src[x] != 0x0f && sx >= 0 && sx < nScreenWidth) {
					pTransDraw[sy + sx] = color | src[x];
				}
			}
		} else {
			for (INT32 x = 0; x < 16; x++, sx++) {
				if (sx >= 0 && sx < nScreenWidth) {
					pTransDraw[sy + sx] = color | src[x];
				}
			}
		}
	}
}

static void tokib_draw_sprites()
{
	INT32 x,y,code,flipx,color,offs;
	UINT16 *sprite_buffer = (UINT16*)(DrvSprBuf + 0xe);
	UINT16 *sprite_word;

	for (offs = 0;offs < 0x642 / 2;offs += 4)
	{
		sprite_word = &sprite_buffer[offs];

		if (BURN_ENDIAN_SWAP_INT16(sprite_word[0]) == 0xf100)
			break;

		if (BURN_ENDIAN_SWAP_INT16(sprite_word[2]))
		{
			x = BURN_ENDIAN_SWAP_INT16(sprite_word[3]) & 0x1ff;
			if (x > 256)
				x -= 512;

			y = BURN_ENDIAN_SWAP_INT16(sprite_word[0]) & 0x1ff;
			if (y > 256)
				y = (512-y)+240;
			else
				y = 240-y;

			flipx   = BURN_ENDIAN_SWAP_INT16(sprite_word[1]) & 0x4000;
			code    = BURN_ENDIAN_SWAP_INT16(sprite_word[1]) & 0x1fff;
			color   = BURN_ENDIAN_SWAP_INT16(sprite_word[2]) >> 12;

			y-=1+16;

			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
			}
		}
	}
}

static void toki_draw_sprites()
{
	INT32 x,y,xoffs,yoffs,code,flipx,flipy,color,offs;
	UINT16 *sprite_buffer = (UINT16*)DrvSprBuf;
	UINT16 *sprite_word;

	for (offs = (0x800/2)-4;offs >= 0;offs -= 4)
	{
		sprite_word = &sprite_buffer[offs];

		if ((BURN_ENDIAN_SWAP_INT16(sprite_word[2]) != 0xf000) && (BURN_ENDIAN_SWAP_INT16(sprite_word[0]) != 0xffff))
		{
			xoffs = (BURN_ENDIAN_SWAP_INT16(sprite_word[0]) &0xf0);
			x = (BURN_ENDIAN_SWAP_INT16(sprite_word[2]) + xoffs) & 0x1ff;
			if (x > 256)
				x -= 512;

			yoffs = (BURN_ENDIAN_SWAP_INT16(sprite_word[0]) &0xf) << 4;
			y = (BURN_ENDIAN_SWAP_INT16(sprite_word[3]) + yoffs) & 0x1ff;
			if (y > 256)
				y -= 512;

			color = BURN_ENDIAN_SWAP_INT16(sprite_word[1]) >> 12;
			flipx   = BURN_ENDIAN_SWAP_INT16(sprite_word[0]) & 0x100;
			flipy   = 0;
			code    = (BURN_ENDIAN_SWAP_INT16(sprite_word[1]) & 0xfff) + ((BURN_ENDIAN_SWAP_INT16(sprite_word[2]) & 0x8000) >> 3);

			if (0) { // flipscreen
				x=240-x;
				y=240-y;
				if (flipx) flipx=0; else flipx=1;
				flipy=1;
			}

			y-=16;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 4, 0x0f, 0, DrvGfxROM1);
				}
			}
		}
	}
}

static INT32 TokibDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	UINT16 *scrollram = (UINT16 *)DrvScrollRAM;

	if (BURN_ENDIAN_SWAP_INT16(scrollram[3]) & 0x2000) {
		draw_bg_layer(DrvBg1RAM, DrvGfxROM2, 0, 0x200, BURN_ENDIAN_SWAP_INT16(scrollram[1])-0x103, BURN_ENDIAN_SWAP_INT16(scrollram[0])+1+16);
		draw_bg_layer(DrvBg2RAM, DrvGfxROM3, 1, 0x300, BURN_ENDIAN_SWAP_INT16(scrollram[3])-0x101, BURN_ENDIAN_SWAP_INT16(scrollram[2])+1+16);
	} else {
		draw_bg_layer(DrvBg2RAM, DrvGfxROM3, 0, 0x300, BURN_ENDIAN_SWAP_INT16(scrollram[3])-0x101, BURN_ENDIAN_SWAP_INT16(scrollram[2])+1+16);
		draw_bg_layer(DrvBg1RAM, DrvGfxROM2, 1, 0x200, BURN_ENDIAN_SWAP_INT16(scrollram[1])-0x103, BURN_ENDIAN_SWAP_INT16(scrollram[0])+1+16);
	}

	tokib_draw_sprites();

	draw_text_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	UINT16 *scrollram = (UINT16*)DrvScrollRAM;
	INT32 bgscrolly,bgscrollx,fgscrolly,fgscrollx;

	bgscrollx = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x06]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x06]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x05]) & 0x10) << 4);
	bgscrolly = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0e]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0e]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0d]) & 0x10) << 4);

	fgscrollx = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x16]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x16]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x15]) & 0x10) << 4);
	fgscrolly = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1e]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1e]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1d]) & 0x10) << 4);

	if (~nBurnLayer & 1) memset (pTransDraw, 0, nScreenWidth * nScreenHeight * 2);

	if (BURN_ENDIAN_SWAP_INT16(scrollram[0x28]) & 0x0100) {
		if (nBurnLayer & 1) draw_bg_layer(DrvBg1RAM, DrvGfxROM2, 0, 0x200, bgscrollx, bgscrolly+16);
		if (nBurnLayer & 2) draw_bg_layer(DrvBg2RAM, DrvGfxROM3, 1, 0x300, fgscrollx, fgscrolly+16);
	} else {
		if (nBurnLayer & 2) draw_bg_layer(DrvBg2RAM, DrvGfxROM3, 0, 0x300, fgscrollx, fgscrolly+16);
		if (nBurnLayer & 1) draw_bg_layer(DrvBg1RAM, DrvGfxROM2, 1, 0x200, bgscrollx, bgscrolly+16);
	}

	//flipscreen = (scrollram[0x28]&0x8000)==0);

	toki_draw_sprites();

	draw_text_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrawByLine(INT32 line)
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	UINT16 *scrollram = (UINT16*)DrvScrollRAM;
	INT32 bgscrolly,bgscrollx,fgscrolly,fgscrollx;

	bgscrollx = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x06]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x06]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x05]) & 0x10) << 4);
	bgscrolly = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0e]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0e]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x0d]) & 0x10) << 4);

	fgscrollx = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x16]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x16]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x15]) & 0x10) << 4);
	fgscrolly = ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1e]) & 0x7f) << 1) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1e]) & 0x80) >> 7) | ((BURN_ENDIAN_SWAP_INT16(scrollram[0x1d]) & 0x10) << 4);

//	memset (pTransDraw + line * 2, 0, nScreenWidth * 2);
	if (~nBurnLayer & 1) memset (pTransDraw, 0, nScreenWidth * nScreenHeight * 2);

	if (BURN_ENDIAN_SWAP_INT16(scrollram[0x28]) & 0x0100) {
		if (nBurnLayer & 1) draw_bg_layer_by_line(DrvBg1RAM, DrvGfxROM2, 0, 0x200, bgscrollx, bgscrolly+16, line);
		if (nBurnLayer & 2) draw_bg_layer_by_line(DrvBg2RAM, DrvGfxROM3, 1, 0x300, fgscrollx, fgscrolly+16, line);
	} else {
		if (nBurnLayer & 2) draw_bg_layer_by_line(DrvBg2RAM, DrvGfxROM3, 0, 0x300, fgscrollx, fgscrolly+16, line);
		if (nBurnLayer & 1) draw_bg_layer_by_line(DrvBg1RAM, DrvGfxROM2, 1, 0x200, bgscrollx, bgscrolly+16, line);
	}

	return 0;
}

static void assemble_inputs(UINT16 in0base, UINT16 in1base)
{
	DrvInps[0] = in0base;
	DrvInps[1] = in1base;

	for (INT32 i = 0; i < 16; i++) {
		DrvInps[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInps[1] ^= (DrvJoy2[i] & 1) << i;
	}

	seibu_coin_input = ((DrvJoy3[1]&1)<<1)|(DrvJoy3[0]&1);
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	Drv68KROM[0x488] = ((DrvDips[2]) ? 0x0f : 0x00); // debug mode dip

	SekNewFrame();
	ZetNewFrame();

	assemble_inputs(0xffff, 0xffff);

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (10000000 * 100) / 5961, 3579545 / 60 }; // 59.61 fps
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 segment = (nCyclesTotal[0] / nInterleave) * (i + 1);
		if (SekTotalCycles() < segment) {
			nCyclesDone[0] += SekRun(segment - SekTotalCycles());
		}

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (pTransDraw && i >= 16 && i < 240) {
			DrawByLine(i - 16);
		}

		// when the line clears, the timer starts counting for the scroll regs to be written!
		if (i == nInterleave-1) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	}
	
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		toki_draw_sprites();
		draw_text_layer();
		BurnTransferCopy(DrvPalette);
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x800);

	return 0;
}

static INT32 TokibFrame()
{
	if (DrvReset) {
		TokibDoReset();
	}
	
	INT32 nInterleave = MSM5205CalcInterleave(0, 4000000);

	SekNewFrame();
	ZetNewFrame();

	assemble_inputs(0x3f3f, 0xff1f);
	
	INT32 nCyclesToDo[2] = { 10000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext, nCyclesSegment;

		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesToDo[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == (nInterleave - 1)) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		SekClose();

		ZetOpen(0);
		BurnTimerUpdateYM3812((i + 1) * (nCyclesToDo[1] / nInterleave));
		MSM5205Update();
		ZetClose();
	}
	
	ZetOpen(0);
	BurnTimerEndFrameYM3812(nCyclesToDo[1]);
	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();	
	
	if (pBurnDraw) {
		TokibDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x800);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
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

		if (is_bootleg) {
			BurnYM3812Scan(nAction, pnMin);
			MSM5205Scan(nAction, pnMin);
			ZetScan(nAction);
		} else {
			seibu_sound_scan(pnMin, nAction);
		}

		SCAN_VAR(TokibMSM5205Next);
		SCAN_VAR(TokibMSM5205Toggle);

		DrvRecalc = 1;
	}

	return 0;
}


// Toki (World, set 1)

static struct BurnRomInfo tokiRomDesc[] = {
	{ "6e.m10",				0x20000, 0x94015d91, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4e.k10",				0x20000, 0x531bd3ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.m12",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.k12",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.m3",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7.m7",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.c5",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  6 Characters
	{ "2.c3",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "toki_obj1.c20",		0x80000, 0xa27a80ba, 4 | BRF_GRA },           //  8 Background Tiles
	{ "toki_obj2.c22",		0x80000, 0xfa687718, 4 | BRF_GRA },           //  9

	{ "toki_bk1.cd8",		0x80000, 0xfdaa5f4b, 5 | BRF_GRA },           // 10 Foreground Tiles

	{ "toki_bk2.ef8",		0x80000, 0xd86ac664, 6 | BRF_GRA },           // 11 Sprites

	{ "9.m1",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 12 MSM6295 Samples
};

STD_ROM_PICK(toki)
STD_ROM_FN(toki)

struct BurnDriver BurnDrvToki = {
	"toki", NULL, NULL, NULL, "1989",
	"Toki (World, set 1)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokiRomInfo, tokiRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Toki (World, set 2)

static struct BurnRomInfo tokiaRomDesc[] = {
	{ "6.m10",				0x20000, 0x03d726b1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4c.k10",				0x20000, 0xb2c345c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.m12",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.k12",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.m3",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7.m7",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.c5",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  6 Characters
	{ "2.c3",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "toki_obj1.c20",		0x80000, 0xa27a80ba, 4 | BRF_GRA },           //  8 Background Tiles
	{ "toki_obj2.c22",		0x80000, 0xfa687718, 4 | BRF_GRA },           //  9

	{ "toki_bk1.cd8",		0x80000, 0xfdaa5f4b, 5 | BRF_GRA },           // 10 Foreground Tiles

	{ "toki_bk2.ef8",		0x80000, 0xd86ac664, 6 | BRF_GRA },           // 11 Sprites

	{ "9.m1",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 12 MSM6295 Samples
};

STD_ROM_PICK(tokia)
STD_ROM_FN(tokia)

struct BurnDriver BurnDrvTokia = {
	"tokia", "toki", NULL, NULL, "1989",
	"Toki (World, set 2)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokiaRomInfo, tokiaRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Toki (US, set 1)

static struct BurnRomInfo tokiuRomDesc[] = {
	{ "6b.10m",				0x20000, 0x3674d9fe, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "14.10k",				0x20000, 0xbfdd48af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.m12",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.k12",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.m3",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7.m7",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.c5",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  6 Characters
	{ "2.c3",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "toki_obj1.c20",		0x80000, 0xa27a80ba, 4 | BRF_GRA },           //  8 Background Tiles
	{ "toki_obj2.c22",		0x80000, 0xfa687718, 4 | BRF_GRA },           //  9

	{ "toki_bk1.cd8",		0x80000, 0xfdaa5f4b, 5 | BRF_GRA },           // 10 Foreground Tiles

	{ "toki_bk2.ef8",		0x80000, 0xd86ac664, 6 | BRF_GRA },           // 11 Sprites

	{ "9.m1",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 12 MSM6295 Samples
};

STD_ROM_PICK(tokiu)
STD_ROM_FN(tokiu)

struct BurnDriver BurnDrvTokiu = {
	"tokiu", "toki", NULL, NULL, "1989",
	"Toki (US, set 1)\0", NULL, "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokiuRomInfo, tokiuRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Toki (US, set 2)

static struct BurnRomInfo tokiuaRomDesc[] = {
	{ "6.m10",		    	0x20000, 0x03d726b1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4u.k10",				0x20000, 0xca2f50d9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.m12",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.k12",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.m3",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7.m7",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.c5",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  6 Characters
	{ "2.c3",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "toki_obj1.c20",		0x80000, 0xa27a80ba, 4 | BRF_GRA },           //  8 Background Tiles
	{ "toki_obj2.c22",		0x80000, 0xfa687718, 4 | BRF_GRA },           //  9

	{ "toki_bk1.cd8",		0x80000, 0xfdaa5f4b, 5 | BRF_GRA },           // 10 Foreground Tiles

	{ "toki_bk2.ef8",		0x80000, 0xd86ac664, 6 | BRF_GRA },           // 11 Sprites

	{ "9.m1",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 12 MSM6295 Samples
};

STD_ROM_PICK(tokiua)
STD_ROM_FN(tokiua)

struct BurnDriver BurnDrvTokiua = {
	"tokiua", "toki", NULL, NULL, "1989",
	"Toki (US, set 2)\0", NULL, "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokiuaRomInfo, tokiuaRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Toki (US, prototype?)

static struct BurnRomInfo tokipRomDesc[] = {
	{ "6 10-M",		    	0x20000, 0x91b554a3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4 10-K",				0x20000, 0x404220f7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5 12-M",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3 12-K",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8 3-M",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7 7-M",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1 5-C",				0x10000, 0xfd0ff303, 3 | BRF_GRA },           //  6 Characters
	{ "2 3-C",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "OBJ 1-0.ROM10",		0x20000, 0xa027bd8e, 4 | BRF_GRA },           //  8 Background Tiles
	{ "OBJ 1-1.ROM9",		0x20000, 0x43a767ea, 4 | BRF_GRA },           //  9
	{ "OBJ 1-2.ROM12",		0x20000, 0x1aecc9d8, 4 | BRF_GRA },           // 10
	{ "OBJ 1-3.ROM11",		0x20000, 0xd65c0c6d, 4 | BRF_GRA },           // 11
	{ "OBJ 2-0.ROM14",		0x20000, 0xcedaccaf, 4 | BRF_GRA },           // 12
	{ "OBJ 2-1.ROM13",		0x20000, 0x013f539b, 4 | BRF_GRA },           // 13
	{ "OBJ 2-2.ROM16",		0x20000, 0x6a8e6e22, 4 | BRF_GRA },           // 14
	{ "OBJ 2-3.ROM15",		0x20000, 0x25d9a16c, 4 | BRF_GRA },           // 15

	{ "BACK 1-0.ROM5",		0x20000, 0xfac7e32f, 5 | BRF_GRA },           // 16 Foreground Tiles
	{ "BACK 1-1.ROM6",		0x20000, 0xee1135d6, 5 | BRF_GRA },           // 17
	{ "BACK 1-2.ROM7",		0x20000, 0x78db8d57, 5 | BRF_GRA },           // 18
	{ "BACK 1-3.ROM8",		0x20000, 0xd719de71, 5 | BRF_GRA },           // 19

	{ "BACK 2-0.ROM1",		0x20000, 0x949d8025, 6 | BRF_GRA },           // 20 Sprites
	{ "BACK 2-1.ROM2",		0x20000, 0x4b28b4b4, 6 | BRF_GRA },           // 21
	{ "BACK 2-2.ROM3",		0x20000, 0x1aa9a5cf, 6 | BRF_GRA },           // 22
	{ "BACK 2-3.ROM4",		0x20000, 0x6759571f, 6 | BRF_GRA },           // 23

	{ "9 1-M",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 24 MSM6295 Samples
};

STD_ROM_PICK(tokip)
STD_ROM_FN(tokip)

struct BurnDriver BurnDrvTokip = {
	"tokip", "toki", NULL, NULL, "1989",
	"Toki (US, prototype?)\0", NULL, "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokipRomInfo, tokipRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	TokipInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// JuJu Densetsu (Japan)

static struct BurnRomInfo jujuRomDesc[] = {
	{ "6.m10",				0x20000, 0x03d726b1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4.k10",				0x20000, 0x54a45e12, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.m12",				0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.k12",				0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.m3",				0x02000, 0x6c87c4c5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)
	{ "7.m7",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.c5",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  6 Characters
	{ "2.c3",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  7

	{ "toki_obj1.c20",		0x80000, 0xa27a80ba, 4 | BRF_GRA },           //  8 Background Tiles
	{ "toki_obj2.c22",		0x80000, 0xfa687718, 4 | BRF_GRA },           //  9

	{ "toki_bk1.cd8",		0x80000, 0xfdaa5f4b, 5 | BRF_GRA },           // 10 Foreground Tiles

	{ "toki_bk2.ef8",		0x80000, 0xd86ac664, 6 | BRF_GRA },           // 11 Sprites

	{ "9.m1",				0x20000, 0xae7a6b8b, 7 | BRF_SND },           // 12 MSM6295 Samples
};

STD_ROM_PICK(juju)
STD_ROM_FN(juju)

struct BurnDriver BurnDrvJuju = {
	"juju", "toki", NULL, NULL, "1989",
	"JuJu Densetsu (Japan)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, jujuRomInfo, jujuRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// JuJu Densetsu (Playmark bootleg)

static struct BurnRomInfo jujubRomDesc[] = {
	{ "jujub_playmark.e3",	0x20000, 0xb50c73ec, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "jujub_playmark.e5",	0x20000, 0xb2812942, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tokijp.005",			0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tokijp.003",			0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "toki.e1",			0x10000, 0x2832ef75, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)

	{ "toki.e21",			0x08000, 0xbb8cacbd, 3 | BRF_GRA },           //  5 Characters
	{ "toki.e13",			0x08000, 0x052ad275, 3 | BRF_GRA },           //  6
	{ "toki.e22",			0x08000, 0x04dcdc21, 3 | BRF_GRA },           //  7
	{ "toki.e7",			0x08000, 0x70729106, 3 | BRF_GRA },           //  8

	{ "toki.e26",			0x20000, 0xa8ba71fc, 4 | BRF_GRA },           //  9 Background Tiles
	{ "toki.e28",			0x20000, 0x29784948, 4 | BRF_GRA },           // 10
	{ "toki.e34",			0x20000, 0xe5f6e19b, 4 | BRF_GRA },           // 11
	{ "toki.e36",			0x20000, 0x96e8db8b, 4 | BRF_GRA },           // 12
	{ "toki.e30",			0x20000, 0x770d2b1b, 4 | BRF_GRA },           // 13
	{ "toki.e32",			0x20000, 0xc289d246, 4 | BRF_GRA },           // 14
	{ "toki.e38",			0x20000, 0x87f4e7fb, 4 | BRF_GRA },           // 15
	{ "toki.e40",			0x20000, 0x96e87350, 4 | BRF_GRA },           // 16

	{ "toki.e23",			0x10000, 0xfeb13d35, 5 | BRF_GRA },           // 17 Foreground Tiles
	{ "toki.e24",			0x10000, 0x5b365637, 5 | BRF_GRA },           // 18
	{ "toki.e15",			0x10000, 0x617c32e6, 5 | BRF_GRA },           // 19
	{ "toki.e16",			0x10000, 0x2a11c0f0, 5 | BRF_GRA },           // 20
	{ "toki.e17",			0x10000, 0xfbc3d456, 5 | BRF_GRA },           // 21
	{ "toki.e18",			0x10000, 0x4c2a72e1, 5 | BRF_GRA },           // 22
	{ "toki.e8",			0x10000, 0x46a1b821, 5 | BRF_GRA },           // 23
	{ "toki.e9",			0x10000, 0x82ce27f6, 5 | BRF_GRA },           // 24

	{ "toki.e25",			0x10000, 0x63026cad, 6 | BRF_GRA },           // 25 Sprites
	{ "toki.e20",			0x10000, 0xa7f2ce26, 6 | BRF_GRA },           // 26
	{ "toki.e11",			0x10000, 0x48989aa0, 6 | BRF_GRA },           // 27
	{ "toki.e12",			0x10000, 0xc2ad9342, 6 | BRF_GRA },           // 28
	{ "toki.e19",			0x10000, 0x6cd22b18, 6 | BRF_GRA },           // 29
	{ "toki.e14",			0x10000, 0x859e313a, 6 | BRF_GRA },           // 30
	{ "toki.e10",			0x10000, 0xe15c1d0f, 6 | BRF_GRA },           // 31
	{ "toki.e6",			0x10000, 0x6f4b878a, 6 | BRF_GRA },           // 32
};

STD_ROM_PICK(jujub)
STD_ROM_FN(jujub)

struct BurnDriver BurnDrvJujub = {
	"jujub", "toki", NULL, NULL, "1989",
	"JuJu Densetsu (Playmark bootleg)\0", NULL, "bootleg (Playmark)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, jujubRomInfo, jujubRomName, NULL, NULL, TokibInputInfo, TokibDIPInfo,
	TokibInit, DrvExit, TokibFrame, TokibDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// JuJu Densetsu (Japan, bootleg)

static struct BurnRomInfo jujubaRomDesc[] = {
	{ "8.19g",				0x10000, 0x208fb08a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "5.19e",				0x10000, 0x722e5183, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.20g",				0x10000, 0xcb82cc33, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6.20e",				0x10000, 0x826ab39d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "10.21g",				0x10000, 0x6c7a3ffe, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7.21e",				0x10000, 0xb0628230, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "3.9c",				0x08000, 0x808f5e44, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (encrypted)
	{ "4.11c",				0x10000, 0xa67969c4, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "5.19h",				0x10000, 0x8aa964a2, 3 | BRF_GRA },           //  8 Characters
	{ "6.20h",				0x10000, 0x86e87e48, 3 | BRF_GRA },           //  9

	{ "1.17d",				0x20000, 0xa027bd8e, 4 | BRF_GRA },           // 10 Background Tiles
	{ "27.17b",				0x20000, 0x43a767ea, 4 | BRF_GRA },           // 11
	{ "2.18d",				0x20000, 0x1aecc9d8, 4 | BRF_GRA },           // 12
	{ "28.18b",				0x20000, 0xd65c0c6d, 4 | BRF_GRA },           // 13
	{ "3.20d",				0x20000, 0xcedaccaf, 4 | BRF_GRA },           // 14
	{ "29.20b",				0x20000, 0x013f539b, 4 | BRF_GRA },           // 15
	{ "4.21d",				0x20000, 0x6a8e6e22, 4 | BRF_GRA },           // 16
	{ "30.21b",				0x20000, 0x25d9a16c, 4 | BRF_GRA },           // 17

	{ "11.1j",				0x10000, 0x6ad15560, 5 | BRF_GRA },           // 18 Foreground Tiles
	{ "12.2j",				0x10000, 0x68534844, 5 | BRF_GRA },           // 19
	{ "13.4j",				0x10000, 0xf271be5a, 5 | BRF_GRA },           // 20
	{ "14.5j",				0x10000, 0x5d4c187a, 5 | BRF_GRA },           // 21
	{ "19.1l",				0x10000, 0x10afdf03, 5 | BRF_GRA },           // 22
	{ "20.2l",				0x10000, 0x2dc54f41, 5 | BRF_GRA },           // 23
	{ "21.4l",				0x10000, 0x946862a3, 5 | BRF_GRA },           // 24
	{ "22.5l",				0x10000, 0xb45f5608, 5 | BRF_GRA },           // 25

	{ "15.18j",				0x10000, 0xcb8b1d31, 6 | BRF_GRA },           // 26 Sprites
	{ "16.19j",				0x10000, 0x81594e0a, 6 | BRF_GRA },           // 27
	{ "17.20j",				0x10000, 0x4acd44ce, 6 | BRF_GRA },           // 28
	{ "18.21j",				0x10000, 0x25cfe9c3, 6 | BRF_GRA },           // 29
	{ "23.18l",				0x10000, 0x06c8d622, 6 | BRF_GRA },           // 30
	{ "24.19l",				0x10000, 0x362a0506, 6 | BRF_GRA },           // 31
	{ "25.20l",				0x10000, 0xbe064c4b, 6 | BRF_GRA },           // 32
	{ "26.21l",				0x10000, 0xf8b5b38d, 6 | BRF_GRA },           // 33

	{ "1.6a",				0x10000, 0x377153ad, 7 | BRF_SND },           // 34 oki
	{ "2.7a",				0x10000, 0x093ca15d, 7 | BRF_SND },           // 35
};

STD_ROM_PICK(jujuba)
STD_ROM_FN(jujuba)

struct BurnDriverD BurnDrvJujuba = {
	"jujuba", "toki", NULL, NULL, "1989",
	"JuJu Densetsu (Japan, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, jujubaRomInfo, jujubaRomName, NULL, NULL, TokiInputInfo, TokiDIPInfo,
	JujubaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};

// Toki (Datsu bootleg)

static struct BurnRomInfo tokibRomDesc[] = {
	{ "toki.e3",			0x20000, 0xae9b3da4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "toki.e5",			0x20000, 0x66a5a1d6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tokijp.005",			0x10000, 0xd6a82808, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tokijp.003",			0x10000, 0xa01a5b10, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "toki.e1",			0x10000, 0x2832ef75, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code (encrypted)

	{ "toki.e21",			0x08000, 0xbb8cacbd, 3 | BRF_GRA },           //  5 Characters
	{ "toki.e13",			0x08000, 0x052ad275, 3 | BRF_GRA },           //  6
	{ "toki.e22",			0x08000, 0x04dcdc21, 3 | BRF_GRA },           //  7
	{ "toki.e7",			0x08000, 0x70729106, 3 | BRF_GRA },           //  8

	{ "toki.e26",			0x20000, 0xa8ba71fc, 4 | BRF_GRA },           //  9 Background Tiles
	{ "toki.e28",			0x20000, 0x29784948, 4 | BRF_GRA },           // 10
	{ "toki.e34",			0x20000, 0xe5f6e19b, 4 | BRF_GRA },           // 11
	{ "toki.e36",			0x20000, 0x96e8db8b, 4 | BRF_GRA },           // 12
	{ "toki.e30",			0x20000, 0x770d2b1b, 4 | BRF_GRA },           // 13
	{ "toki.e32",			0x20000, 0xc289d246, 4 | BRF_GRA },           // 14
	{ "toki.e38",			0x20000, 0x87f4e7fb, 4 | BRF_GRA },           // 15
	{ "toki.e40",			0x20000, 0x96e87350, 4 | BRF_GRA },           // 16

	{ "toki.e23",			0x10000, 0xfeb13d35, 5 | BRF_GRA },           // 17 Foreground Tiles
	{ "toki.e24",			0x10000, 0x5b365637, 5 | BRF_GRA },           // 18
	{ "toki.e15",			0x10000, 0x617c32e6, 5 | BRF_GRA },           // 19
	{ "toki.e16",			0x10000, 0x2a11c0f0, 5 | BRF_GRA },           // 20
	{ "toki.e17",			0x10000, 0xfbc3d456, 5 | BRF_GRA },           // 21
	{ "toki.e18",			0x10000, 0x4c2a72e1, 5 | BRF_GRA },           // 22
	{ "toki.e8",			0x10000, 0x46a1b821, 5 | BRF_GRA },           // 23
	{ "toki.e9",			0x10000, 0x82ce27f6, 5 | BRF_GRA },           // 24

	{ "toki.e25",			0x10000, 0x63026cad, 6 | BRF_GRA },           // 25 Sprites
	{ "toki.e20",			0x10000, 0xa7f2ce26, 6 | BRF_GRA },           // 26
	{ "toki.e11",			0x10000, 0x48989aa0, 6 | BRF_GRA },           // 27
	{ "toki.e12",			0x10000, 0xc2ad9342, 6 | BRF_GRA },           // 28
	{ "toki.e19",			0x10000, 0x6cd22b18, 6 | BRF_GRA },           // 29
	{ "toki.e14",			0x10000, 0x859e313a, 6 | BRF_GRA },           // 30
	{ "toki.e10",			0x10000, 0xe15c1d0f, 6 | BRF_GRA },           // 31
	{ "toki.e6",			0x10000, 0x6f4b878a, 6 | BRF_GRA },           // 32
};

STD_ROM_PICK(tokib)
STD_ROM_FN(tokib)

struct BurnDriver BurnDrvTokib = {
	"tokib", "toki", NULL, NULL, "1989",
	"Toki (bootleg)\0", NULL, "bootleg (Datsu)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, tokibRomInfo, tokibRomName, NULL, NULL, TokibInputInfo, TokibDIPInfo,
	TokibInit, DrvExit, TokibFrame, TokibDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
