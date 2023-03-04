// FinalBurn Neo Blood Brothers Modular system driver emulator
// Emulation by IQ_132, dink

// Notes:
// 1) pri bug in stage 2-2 found to be a bug in the Seibu Sprite -> Modular
// System conversion code.  When the blimp starts shooting or appears from the
// right side, it barfs on the color/pri codes.
// 2) Slight differences in sound timbre due to using YM2203 / MSM5205 instead
// of the superior(relatively speaking) YM3812 / MSM6295 that Seibu HW has.
// 3) Clocks: some guessed, some guess-derived from pics of the pcb's

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "msm5205.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM[3];
static UINT8 *DrvZ80RAM;

static UINT8 adpcm_data;
static INT32 sound_bank;
static UINT16 fg_scrollx;
static UINT16 fg_scrolly;
static UINT8 soundlatch;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo BloodbroInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bloodbro)

static struct BurnDIPInfo BloodbroDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL					},
	{0x15, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x14, 0x01, 0x01, 0x01, "1"					},
	{0x14, 0x01, 0x01, 0x00, "2"					},

	// Coinage condition: Coin Mode 1
	{0   , 0xfe, 0   ,   16, "Coinage"				},
	{0x14, 0x02, 0x1e, 0x14, "6 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x16, "5 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x18, "4 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x1a, "3 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x02, "8 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x1c, "2 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x04, "5 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x06, "3 Coins 2 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x1e, "1 Coin  1 Credit"		},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x08, "2 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x12, "1 Coin  2 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x10, "1 Coin  3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x0e, "1 Coin  4 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x0c, "1 Coin  5 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL					},
	{0x14, 0x02, 0x1e, 0x00, "Free Play"			},
	{0x14, 0x00, 0x01, 0x01, NULL					},

	// Coin A condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x14, 0x02, 0x06, 0x00, "5 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x06, 0x02, "3 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x06, 0x04, "2 Coins 1 Credit"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x06, 0x06, "1 Coin  1 Credit"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},

	// Coin B condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x14, 0x02, 0x18, 0x18, "1 Coin 2 Credits"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x18, 0x10, "1 Coin 3 Credits"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x18, 0x08, "1 Coin 5 Credits"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},
	{0x14, 0x02, 0x18, 0x00, "1 Coin 6 Credits"		},
	{0x14, 0x00, 0x01, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Starting Coin"		},
	{0x14, 0x01, 0x20, 0x20, "Normal"				},
	{0x14, 0x01, 0x20, 0x00, "x2"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x15, 0x01, 0x03, 0x00, "1"					},
	{0x15, 0x01, 0x03, 0x02, "2"					},
	{0x15, 0x01, 0x03, 0x03, "3"					},
	{0x15, 0x01, 0x03, 0x01, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x15, 0x01, 0x0c, 0x0c, "300K, 500K Every"		},
	{0x15, 0x01, 0x0c, 0x08, "500K, 500K Every"		},
	{0x15, 0x01, 0x0c, 0x04, "500K Only"			},
	{0x15, 0x01, 0x0c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x30, 0x20, "Easy"					},
	{0x15, 0x01, 0x30, 0x30, "Normal"				},
	{0x15, 0x01, 0x30, 0x10, "Hard"					},
	{0x15, 0x01, 0x30, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x15, 0x01, 0x40, 0x00, "No"					},
	{0x15, 0x01, 0x40, 0x40, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x00, "Off"					},
	{0x15, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Bloodbro)

static void __fastcall bbms_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		// written on init, junk?
		case 0xc0100:
		return;

		case 0x102000:
		return;		// bg scroll - y or x??

		case 0x18d800:
			fg_scrollx = data - 0x1bf;
		return;

		case 0x18d802:
			fg_scrolly = data - 0xffff;
		return;

	//	default:
	//		bprintf (0, _T("WW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPPC(-1));
	}
}

static void __fastcall bbms_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		// leftovers from seibu sound
		case 0xa0001: break;
		case 0xa0009: break;
		case 0xa000d: break;
		case 0xe000f: break;

		case 0xe000e:
			soundlatch = data | 0x80;
		return;

	//	default:
	//		bprintf (0, _T("WB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPPC(-1));
	}
}

static UINT16 __fastcall bbms_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0e0000:
				return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x0e0002:
				return DrvInputs[0];

		case 0x0e0004:
				return DrvInputs[1];

	//	default:
	//		bprintf (0, _T("RW: %5.5x PC(%5.5x)\n"), address, SekGetPPC(-1));
	}

	return 0;
}

static UINT8 __fastcall bbms_main_read_byte(UINT32 address)
{
	return bbms_main_read_word(address & ~1) >> ((~address & 1) * 8);
}

static void bankswitch(INT32 data)
{
	sound_bank = data;
	ZetMapMemory(DrvZ80ROM + 0x8000 + (sound_bank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall bbms_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		{
			if (sound_bank != (data >> 7)) {
				// it's assumed d7 also controls YM2203 #0 CS line
				bankswitch(data >> 7);
			}

			MSM5205ResetWrite(0, (data & 0x10)>>4);

			adpcm_data = data & 0xf;
		}
		return;

	//	case 0xd008: // adpcm??
	//	return;

		case 0xdff0: // what's this??
		case 0xdff1:
		case 0xdff2:
		case 0xdff3:
		case 0xdff5:
		return;

		case 0xdffe: // ?
			soundlatch &= 0x7f;	// works as well as soundlatch = 0;
		return;

		case 0xe000:
		case 0xe001:
			if (sound_bank == 0) BurnYM2203Write(0, address & 1, data);
		return;

		case 0xe002:
		case 0xe003:
			if (sound_bank == 0) BurnYM2203Write(1, address & 1, data);
		return;

	//	default:
	//		bprintf (0, _T("SWB: %4.4x %2.2x PC(%5.5x)\n"), address, data, ZetGetPC(-1));
	}
}

static UINT8 __fastcall bbms_sound_read(UINT16 address)
{
	switch (address)
	{
	//	case 0xd000:
	//	case 0xd008:
	//		return 0; // ??  Internal latches (mirror of 0xc00x)

		case 0xdffe:
			return soundlatch & 0x7f;

		case 0xdfff:
			return soundlatch & 0x80; // sound status - returning 0x80 always seems to work just as well?

		case 0xe008:
		case 0xe009:
			return BurnYM2203Read(0, address & 1);

		case 0xe00a:
		case 0xe00b:
			return BurnYM2203Read(1, address & 1);

	//	default:
	//		bprintf (0, _T("SRB: %5.5x PC(%5.5x)\n"), address, ZetGetPC(-1));
	}

	return 0;
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvVidRAM[0];

	TILE_SET_INFO(1, BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvVidRAM[1];

	TILE_SET_INFO(0, BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static tilemap_callback( text )
{
	UINT16 *ram = (UINT16*)DrvVidRAM[2];

	TILE_SET_INFO(3, BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static void bbms_adpcm_vck()
{
	MSM5205DataWrite(0, adpcm_data);

	ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 4000000);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	MSM5205Reset();
	ZetClose();

	soundlatch = 0;
	sound_bank = -1;
	adpcm_data = 0;
	fg_scrollx = 0;
	fg_scrolly = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM[3]	= Next; Next += 0x040000;
	DrvGfxROM[0]	= Next; Next += 0x100000;
	DrvGfxROM[1]	= Next; Next += 0x100000;
	DrvGfxROM[2]	= Next; Next += 0x200000;

	BurnPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	BurnPalRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;

	DrvVidRAM[0]	= Next; Next += 0x001000;
	DrvVidRAM[1]	= Next; Next += 0x001000;
	DrvVidRAM[2]	= Next; Next += 0x001000;

	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(INT32 n, INT32 type, INT32 len)
{
	INT32 Plane0[4] = { 0x8000*8*3,0x8000*8*2,0x8000*8*1,0x8000*8*0 };
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	INT32 Plane1[4]  = { STEP4(0,8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(512,1) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM[n], len);

	if (type == 0) GfxDecode((len * 2) / ( 8 *  8), 4,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM[n]);
	if (type == 1) GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[n]);

	BurnFree (tmp);
}

static void descramble_16x16tiles(INT32 n)
{
	INT32 len = 0x80000;
	UINT8 *buffer = (UINT8*)BurnMalloc(len);

	for (INT32 i = 0; i < len; i++) {
		buffer[(i & 0xf801f) | ((i & 0x1fe0) << 2) | ((i & 0x6000) >> 8)] = DrvGfxROM[n][i];
	}

	memcpy (DrvGfxROM[n], buffer, len);

	BurnFree (buffer);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x040001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000000, k++, 4, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000000, k++, 4, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3] + 0x000000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x008000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x010000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3] + 0x018000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000000, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x040003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x040002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x040001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x040000, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x080003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x080002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x080001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x080000, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x0c0003, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x0c0002, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x0c0001, k++, 4, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x0c0000, k++, 4, LD_INVERT)) return 1;

		descramble_16x16tiles(0);
		descramble_16x16tiles(1);
		DrvGfxDecode(0, 1, 0x80000);
		DrvGfxDecode(1, 1, 0x80000);
		DrvGfxDecode(2, 1, 0x100000);
		DrvGfxDecode(3, 0, 0x20000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x100000, 0x100fff, MAP_RAM); // split (0-400,400-800)
	SekMapMemory(DrvSprRAM,		0x101000, 0x101fff, MAP_RAM); // 1000-17ff
	SekMapMemory(DrvVidRAM[0],	0x08c000, 0x08cfff, MAP_RAM); // 0-3ff
	SekMapMemory(DrvVidRAM[2],	0x08d800, 0x08dfff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],	0x18d000, 0x18d7ff, MAP_RAM);
	SekSetWriteWordHandler(0,	bbms_main_write_word);
	SekSetWriteByteHandler(0,	bbms_main_write_byte);
	SekSetReadWordHandler(0,	bbms_main_read_word);
	SekSetReadByteHandler(0,	bbms_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xd000, 0xd7ff, MAP_RAM); // mirror
	ZetSetWriteHandler(bbms_sound_write);
	ZetSetReadHandler(bbms_sound_read);
	ZetClose();

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.70, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.70, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.75, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.75, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, bbms_adpcm_vck, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 1.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, text_map_callback,        8,  8, 32, 32);

	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, 0x100000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x300, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x200000, 0x100, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,  8,  8, 0x040000, 0x000, 0xf); // sprite gfx
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	ZetExit();
	BurnYM2203Exit();
	MSM5205Exit();

	BurnFreeMemIndex();

	return 0;
}

static void update_palette()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		UINT16 data = (BurnPalRAM[i+0x400] << 8) | BurnPalRAM[i];

		BurnPalette[i^1] = BurnHighCol(pal4bit(data >> 0), pal4bit(data >> 4), pal4bit(data >> 8), 0);		// palette data is byteswapped!!!
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;
	GenericTilesGfx *gfx = &GenericGfxData[2];

	for (INT32 i = 0; i < 0x400/2; i+=2)
	{
		UINT16 attr0 = ram[i + 0];
		UINT16 attr1 = ram[i + 1];
		UINT16 attr2 = ram[i + (0x400/2)];

		INT32 code	=((attr0 & 0xff00) >> 8) | ((attr1 & 0x001f) << 8);
		INT32 sy	= (attr0 & 0x00ff);
		INT32 flipx = (attr1 & 0x0020);
		INT32 sx	=((attr1 & 0xff00) >> 8) | ((attr2 & 0x8000) >> 7);
		INT32 flipy = (attr2 & 0x4000);
		INT32 color = (attr2 & 0x0f00) >> 8;
		INT32 pri 	= (attr2 & 0x0800) ? 2 : 0;	// seems right? (see note @ top)

		RenderPrioSprite(pTransDraw, gfx->gfxbase, code % gfx->code_mask, (color << gfx->depth) + gfx->color_offset, 0xf, sx - 256, (0xff - sy) - 30, flipx, flipy, gfx->width, gfx->height, pri);
	}
}

static INT32 DrvDraw()
{
	//if (BurnRecalc) {
		update_palette();
	//}

	BurnTransferClear();

	GenericTilemapSetScrollX(1, fg_scrollx);
	GenericTilemapSetScrollY(1, fg_scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 22118400 / 2 / 60, 4000000 / 60 }; // Frequencies guessed!
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetNewFrame();

	MSM5205NewFrame(0, 4000000, nInterleave);

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		CPU_RUN_TIMER(1);
		MSM5205UpdateScanline(i);
	}

	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(sound_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(adpcm_data);
		SCAN_VAR(fg_scrollx);
		SCAN_VAR(fg_scrolly);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(sound_bank);
		ZetClose();
	}

	return 0;
}


// Blood Bros. (Modular System)

static struct BurnRomInfo bloodbromRomDesc[] = {
	{ "6-1_bb606.ic8",					0x10000, 0x3c069061, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "6-1_bb603.ic17",					0x10000, 0x10f4c8e9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "6-1_bb605.ic11",					0x10000, 0x2dc3fb8c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6-1_bb602.ic20",					0x10000, 0x8e507cce, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6-1_bb604.ic25",					0x10000, 0xcc069a40, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6-1_bb601.ic26",					0x10000, 0xd06bf68d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "1-2_bb101.ic12",					0x10000, 0x3e184e74, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "4-3-a_bb4a1.ic17",				0x20000, 0x499c91db, 3 | BRF_GRA },           //  7 Foreground Tiles
	{ "4-3-a_bb4a2.ic16",				0x20000, 0xe8f87153, 3 | BRF_GRA },           //  8
	{ "4-3-a_bb4a3.ic15",				0x20000, 0x13b888f2, 3 | BRF_GRA },           //  9
	{ "4-3-a_bb4a4.ic14",				0x20000, 0x19bc0508, 3 | BRF_GRA },           // 10

	{ "4-3-b_bb4b1.ic17",				0x20000, 0xaa86ae59, 4 | BRF_GRA },           // 11 Background Tiles
	{ "4-3-b_bb4b2.ic16",				0x20000, 0xf25dd182, 4 | BRF_GRA },           // 12
	{ "4-3-b_bb4b3.ic15",				0x20000, 0x3efcb6aa, 4 | BRF_GRA },           // 13
	{ "4-3-b_bb4b4.ic14",				0x20000, 0x6b5254fa, 4 | BRF_GRA },           // 14

	{ "4-3_bb401.ic17",    				0x08000, 0x07e12bd2, 3 | BRF_GRA },           // 15 Characters
	{ "4-3_bb402.ic16",   				0x08000, 0xeca374ea, 3 | BRF_GRA },           // 16
	{ "4-3_bb403.ic15",    				0x08000, 0xd77b84d3, 3 | BRF_GRA },           // 17
	{ "4-3_bb404.ic14",    				0x08000, 0xf8d2d4dc, 3 | BRF_GRA },           // 18

	{ "51-1-b_bb503.ic3",				0x10000, 0x9d2a382d, 6 | BRF_GRA },           // 19 Sprites
	{ "51-1-b_bb512.ic12",				0x10000, 0x83bbb220, 6 | BRF_GRA },           // 20
	{ "51-1-b_bb518.ic18",				0x10000, 0xefcf5b1d, 6 | BRF_GRA },           // 21
	{ "51-1-b_bb524.ic24",				0x10000, 0xc4ccf38d, 6 | BRF_GRA },           // 22
	{ "51-1-b_bb504.ic4",				0x10000, 0x1fc7f229, 6 | BRF_GRA },           // 23
	{ "51-1-b_bb513.ic13",				0x10000, 0x3767456b, 6 | BRF_GRA },           // 24
	{ "51-1-b_bb519.ic19",				0x10000, 0x77670244, 6 | BRF_GRA },           // 25
	{ "51-1-b_bb525.ic25",				0x10000, 0x25b4e119, 6 | BRF_GRA },           // 26
	{ "51-1-b_bb505.ic5",				0x10000, 0x3ec650ce, 6 | BRF_GRA },           // 27
	{ "51-1-b_bb514.ic14",				0x10000, 0xa29a2f44, 6 | BRF_GRA },           // 28
	{ "51-1-b_bb520.ic20",				0x10000, 0xd7f3b09a, 6 | BRF_GRA },           // 29
	{ "51-1-b_bb526.ic26",				0x10000, 0x1c2d70b0, 6 | BRF_GRA },           // 30
	{ "51-1-b_bb506.ic6",				0x10000, 0x10dba663, 6 | BRF_GRA },           // 31
	{ "51-1-b_bb515.ic15",				0x10000, 0x30110411, 6 | BRF_GRA },           // 32
	{ "51-1-b_bb521.ic21",				0x10000, 0xfb8cff4c, 6 | BRF_GRA },           // 33
	{ "51-1-b_bb527.ic27",				0x10000, 0xa73cd7a5, 6 | BRF_GRA },           // 34

	{ "1-2_110_tbp18s030.ic20",			0x00020, 0xe26e680a, 7 | BRF_GRA | BRF_OPT }, // 35 Unknown PROMs
	{ "2_211_82s129.ic4",				0x00100, 0x4f8c3e63, 7 | BRF_GRA | BRF_OPT }, // 36
	{ "2_202_82s129.ic12",				0x00100, 0xe434128a, 7 | BRF_GRA | BRF_OPT }, // 37
	{ "51-1_p0502_82s129n.ic10",		0x00100, 0x15085e44, 7 | BRF_GRA | BRF_OPT }, // 38

	{ "6-1_606_gal16v8-20hb1.ic13",		0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 39 PLDs
	{ "6-1_646_gal16v8-20hb1.ic7",		0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 40
	{ "4-3_403_gal16v8-25hb1.ic29",		0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 41
	{ "4-3-a_p0403_pal16r8acn.ic29",	0x00104, 0x506156cc, 8 | BRF_OPT },           // 42
	{ "4-3-b_403_gal16v8-25hb1.ic19",	0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 43
	{ "51-1_503_gal16v8-25lp.ic48",		0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 44
	{ "51-1-b_5146_gal16v8-20hb1.ic9",	0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 45
	{ "51-1-b_5246_gal16v8-20hb1.ic8",	0x00117, 0x00000000, 8 | BRF_NODUMP | BRF_OPT },           // 46
};

STD_ROM_PICK(bloodbrom)
STD_ROM_FN(bloodbrom)

struct BurnDriver BurnDrvBloodbrom = {
	"bloodbrom", "bloodbro", NULL, NULL, "199?",
	"Blood Bros. (Modular System)\0", NULL, "bootleg (Gaelco / Ervisa)", "Modular System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbromRomInfo, bloodbromRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	256, 224, 4, 3
};
