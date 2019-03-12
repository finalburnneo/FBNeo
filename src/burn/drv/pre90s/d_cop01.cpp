// FB Alpha Nichibutsu Cop 01 / Mighty Guy driver module
// Based on MAME driver by Carlos A. Lozano

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "burn_ym3526.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvProtData;
static UINT8 *DrvProtRAM;
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

static UINT8 video_registers[4];
static UINT8 soundlatch;
static UINT8 timer_pulse;

// mightguy protection stuff
static UINT8  protection_command;
static UINT8  prot_rom_op;
static UINT16 prot_rom_address;
static UINT16 prot_adj_address;
static UINT16 prot_mgtimer;
static UINT32 prot_mgtimer_count;
static UINT8  prot_timer_reg;
static UINT16 prot_dac_start_address;
static UINT16 prot_dac_current_address;
static UINT16 prot_dac_freq;
static UINT8  prot_dac_playing;
static UINT8  prot_const90;

static UINT8 *dac_intrl_table; // dac freq table

static UINT8 flipscreen;
static INT32 mightguy = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Cop01InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	//{"Service Mode",BIT_DIGITAL,	DrvJoy3 + 5,	"diag"      },
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Cop01)

static struct BurnDIPInfo Cop01DIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0x20, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x12, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x12, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x00, "Off"					},
	{0x12, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x80, 0x00, "Upright"				},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x03, "Easy"					},
	{0x13, 0x01, 0x03, 0x01, "Medium"				},
	{0x13, 0x01, 0x03, 0x02, "Hard"					},
	{0x13, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x0c, 0x0c, "3"					},
	{0x13, 0x01, 0x0c, 0x04, "4"					},
	{0x13, 0x01, 0x0c, 0x08, "5"					},
	{0x13, 0x01, 0x0c, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "1st Bonus Life"		},
	{0x13, 0x01, 0x10, 0x10, "20000"				},
	{0x13, 0x01, 0x10, 0x00, "30000"				},

	{0   , 0xfe, 0   ,    4, "2nd Bonus Life"		},
	{0x13, 0x01, 0x60, 0x60, "30000"				},
	{0x13, 0x01, 0x60, 0x20, "50000"				},
	{0x13, 0x01, 0x60, 0x40, "100000"				},
	{0x13, 0x01, 0x60, 0x00, "150000"				},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x13, 0x01, 0x70, 0x70, "20k 50k 30k+"			},
	{0x13, 0x01, 0x70, 0x30, "20k 70k 50k+"			},
	{0x13, 0x01, 0x70, 0x50, "20k 120k 100k+"		},
	{0x13, 0x01, 0x70, 0x10, "20k 170k 150k+"		},
	{0x13, 0x01, 0x70, 0x60, "30k 60k 30k+"			},
	{0x13, 0x01, 0x70, 0x20, "30k 80k 50k+"			},
	{0x13, 0x01, 0x70, 0x40, "30k 130k 100k+"		},
	{0x13, 0x01, 0x70, 0x00, "30k 180k 150k+"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x20, 0x20, "Off"					},
	{0x14, 0x01, 0x20, 0x00, "On"					},
};

STDDIPINFO(Cop01)

static struct BurnDIPInfo MightguyDIPList[]=
{
	{0x12, 0xff, 0xff, 0x67, NULL					},
	{0x13, 0xff, 0xff, 0x3f, NULL					},
	{0x14, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x03, 0x03, "3"					},
	{0x12, 0x01, 0x03, 0x02, "4"					},
	{0x12, 0x01, 0x03, 0x01, "5"					},
	{0x12, 0x01, 0x03, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x12, 0x01, 0x04, 0x04, "Every 200k"			},
	{0x12, 0x01, 0x04, 0x00, "500k only"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x08, 0x08, "Off"					},
	{0x12, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x10, 0x00, "Upright"				},
	{0x12, 0x01, 0x10, 0x10, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x20, 0x20, "Off"					},
	{0x12, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x13, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x13, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0c, 0x04, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x30, 0x30, "Easy"					},
	{0x13, 0x01, 0x30, 0x20, "Normal"				},
	{0x13, 0x01, 0x30, 0x10, "Hard"					},
	{0x13, 0x01, 0x30, 0x00, "Invincibility"		},

	{0   , 0xfe, 0   ,    8, "Starting Area"		},
	{0x14, 0x01, 0x07, 0x07, "1"					},
	{0x14, 0x01, 0x07, 0x06, "2"					},
	{0x14, 0x01, 0x07, 0x05, "3"					},
	{0x14, 0x01, 0x07, 0x04, "4"					},
	{0x14, 0x01, 0x07, 0x03, "5"					},
	{0x14, 0x01, 0x07, 0x02, "6"					},
	{0x14, 0x01, 0x07, 0x01, "7"					},
	{0x14, 0x01, 0x07, 0x00, "8"					},
};

STDDIPINFO(Mightguy)

static void __fastcall cop01_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
			video_registers[port & 3] = data;
		return;

		case 0x44:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x45:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall cop01_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x03:
		case 0x04:
			return DrvDips[(port - 3) & 0xff];
	}

	return 0;
}

static UINT8 __fastcall mightguy_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x03:
			return (DrvDips[0] & 0x7f) | ((DrvDips[2] << 5) & 0x80);

		case 0x04:
			return (DrvDips[1] & 0x3f) | ((DrvDips[2] << 6) & 0xc0);
	}

	return 0;
}

static UINT8 __fastcall cop01_sound_read(UINT16 address)
{
	if (address == 0x8000) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return 0;
	}

	return 0;
}

static void __fastcall cop01_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			AY8910Write((port / 2) & 3, port & 1, data);
		return;
	}	
}

static void mightguy_prot_mgtimer_cb()
{
	prot_timer_reg = 1;
	prot_mgtimer = 0;
}

static void mightguy_prot_dac_clk()
{
	if (prot_dac_playing) {
		UINT8 data = DrvProtData[prot_dac_current_address++];
		DACSignedWrite(0, data);
		prot_dac_current_address &= 0x1fff;
		if (data == 0x80) prot_dac_playing = 0;
	}
}

static void dac_recalc_freq()
{
	INT32 LastIdx = -1;
	INT32 Idx = 0;
	const INT32 interleave = 256;

	for (INT32 i = 0; i < interleave; i++)
	{
		Idx = (INT32)round(((double)prot_dac_freq / 60 / (double)interleave) * (double)i);

		if (Idx != LastIdx) {
			dac_intrl_table[i] = 1;
		} else dac_intrl_table[i] = 0;
		LastIdx = Idx;
	}
}

static void mightguy_prot_data_write(UINT8 data)
{
	switch (protection_command)
	{
		case 0x32: {
			prot_rom_op = data;
			if (data == 2) {
				prot_dac_current_address = prot_dac_start_address & 0x1fff;
				prot_dac_playing = 1;
			};
			break;
		}
		case 0x33: prot_rom_address &= 0x00ff; prot_rom_address |= data << 8; break;
		case 0x34: prot_rom_address &= 0xff00; prot_rom_address |= data; break;
		case 0x35: prot_adj_address &= 0x00ff; prot_adj_address |= data << 8; break;
		case 0x36: prot_adj_address &= 0xff00; prot_adj_address |= data; break;
		case 0x40: prot_mgtimer = 0x17*160; prot_mgtimer_count = 0; break;
		case 0x41: prot_mgtimer = 0; prot_timer_reg = 0; break;
		case 0x51: prot_dac_start_address &= 0x00ff; prot_dac_start_address |= data << 8; break;
		case 0x52: prot_dac_start_address &= 0xff00; prot_dac_start_address |= data; break;
		case 0x18: prot_dac_freq = data*18; dac_recalc_freq(); break;
		case 0x90: prot_const90 = data; break;
#if 0
		case 0x42: break;
		case 0x43: break;
		case 0x92: break;
#endif
	    default: DrvProtRAM[protection_command] = data; break; //bprintf(0, _T("%02X %02X\n"), protection_command, data); break;
	}
}

static UINT8 mightguy_prot_data_read()
{
	switch (protection_command)
	{
		case 0x37: {
			UINT8 prot_adj = (0x43 - DrvProtData[prot_adj_address]) & 0xff;
			return DrvProtData[prot_rom_address & 0x1fff] - prot_adj;
		}
		case 0x41: return prot_timer_reg;
		case 0x90: return prot_const90;

		default: return DrvProtRAM[protection_command];
	}

	return 0;
}

static UINT8 __fastcall cop01_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x03:
			return mightguy_prot_data_read();

		case 0x06:
		{
			if ((ZetTotalCycles() / 11475) & 1) {
				if (timer_pulse == 0) {
					timer_pulse = 1;
					return ((soundlatch & 0x7f) << 1) | 1;
				}
			} else {
				timer_pulse = 0;
			}
			return ((soundlatch & 0x7f) << 1);
		}
	}

	return 0;
}

static void __fastcall mightguy_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3526Write(port & 1, data);
		return;

		case 0x02:
			protection_command = data;
		return;

		case 0x03:
			mightguy_prot_data_write(data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 code = DrvBgRAM[offs];
	INT32 attr = DrvBgRAM[offs + 0x800];

	INT32 pri = attr >> 7;
	if (attr & 0x10) pri = 0;

	TILE_SET_INFO(0, code + ((attr & 0x03) << 8), attr >> 2, TILE_GROUP(pri));
	*category = pri;
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(1, DrvFgRAM[offs], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);

	ZetReset(1);

	if (mightguy == 0)
	{
		AY8910Reset(0);
		AY8910Reset(1);
		AY8910Reset(2);
	}
	else
	{
		DACReset();
		BurnYM3526Reset();
	}

	soundlatch = 0;
	timer_pulse = 0;

	protection_command = 0xff;
	prot_rom_address = 0;
	prot_adj_address = 0;
	prot_rom_op = 0;
	prot_const90 = 0x18; // fixes coin sample if inserted at first title screen
	prot_dac_current_address = prot_dac_start_address = 0;
	prot_dac_freq = 4000;
	prot_dac_playing = 0;
	prot_timer_reg = 0;

	memset (video_registers, 0, 4);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c0000;
	DrvZ80ROM1		= Next; Next += 0x0080000;

	DrvProtData		= Next; Next += 0x0080000;

	DrvGfxROM0		= Next; Next += 0x0080000;
	DrvGfxROM1		= Next; Next += 0x0100000;
	DrvGfxROM2		= Next; Next += 0x0400000;

	DrvColPROM		= Next; Next += 0x0005000;

	DrvPalette		= (UINT32*)Next; Next += 0x300 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x0010000;
	DrvZ80RAM1		= Next; Next += 0x0008000;
	DrvBgRAM		= Next; Next += 0x0010000;
	DrvFgRAM		= Next; Next += 0x0004000;
	DrvSprRAM		= Next; Next += 0x0001000;
	DrvProtRAM      = Next; Next += 0x0000100;

	RamEnd			= Next;

	dac_intrl_table = Next; Next += 0x0000100;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *rom, UINT32 len)
{
	for (INT32 i = len-1; i >= 0; i--) {
		rom[i*2+1] = rom[i] >> 4;
		rom[i*2+0] = rom[i] & 0xf;
	}
}

static INT32 Cop01Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x4000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x8001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0xc001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x8000, 14, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0xc000, 15, 2)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0300, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0400, 20, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x02000);
		DrvGfxExpand(DrvGfxROM1, 0x08000);
		DrvGfxExpand(DrvGfxROM2, 0x10000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe0ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xf000, 0xf3ff, MAP_RAM);
	ZetSetOutHandler(cop01_main_write_port);
	ZetSetInHandler(cop01_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(cop01_sound_read);
	ZetSetOutHandler(cop01_sound_write_port);
	ZetSetInHandler(cop01_sound_read_port);
	ZetClose();

	AY8910Init(0, 1250000, 0);
	AY8910Init(1, 1250000, 1);
	AY8910Init(2, 1250000, 1);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 8, 8, 0x10000, 0x100, 7);
	GenericTilemapSetGfx(1, DrvGfxROM0, 4, 8, 8, 0x04000, 0x000, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapCategoryConfig(0, 2);
	GenericTilemapSetTransMask(0, 0, 0x0000);
	GenericTilemapSetTransMask(0, 1, 0x0fff);
	GenericTilemapSetTransparent(1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (4000000 / (nBurnFPS / 100.0000))));
}

static INT32 MightguyInit()
{
	mightguy = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvProtData + 0x000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 11, 2)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0300, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0400, 16, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x04000);
		DrvGfxExpand(DrvGfxROM1, 0x08000);
		DrvGfxExpand(DrvGfxROM2, 0x14000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe0ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xf000, 0xf3ff, MAP_RAM);
	ZetSetOutHandler(cop01_main_write_port);
	ZetSetInHandler(mightguy_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(cop01_sound_read);
	ZetSetOutHandler(mightguy_sound_write_port);
	ZetSetInHandler(cop01_sound_read_port);
	ZetClose();

	BurnYM3526Init(4000000, NULL, 0);
	BurnTimerAttachYM3526(&ZetConfig, 4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 8, 8, 0x10000, 0x100, 7);
	GenericTilemapSetGfx(1, DrvGfxROM0, 4, 8, 8, 0x04000, 0x000, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapCategoryConfig(0, 2);
	GenericTilemapSetTransMask(0, 0, 0x0000);
	GenericTilemapSetTransMask(0, 1, 0x0fff);
	GenericTilemapSetTransparent(1, 0xf);

	// patch to enable level select from dips
	{
		DrvZ80ROM0[0x00e4] = 0x07; // rlca
		DrvZ80ROM0[0x00e5] = 0x07; // rlca
		DrvZ80ROM0[0x00e6] = 0x07; // rlca
		DrvZ80ROM0[0x027f] = 0x00; // checksum
		DrvZ80ROM0[0x0280] = 0x00; // checksum
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (mightguy == 0)
	{
		AY8910Exit(0);
		AY8910Exit(1);
		AY8910Exit(2);
	}
	else
	{
		DACExit();
		BurnYM3526Exit();
	}

	BurnFree(AllMem);

	mightguy = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = DrvColPROM[i + 0x000] & 0xf;
		UINT8 g = DrvColPROM[i + 0x100] & 0xf;
		UINT8 b = DrvColPROM[i + 0x200] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16,g+g*16,b+b*16,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[0x100 + i] = DrvPalette[0xc0 | (i & 0x30) | (DrvColPROM[0x300 | ((i & 0x40) >> 2) | (i & 0x0f)] & 0x0f)];
		DrvPalette[0x200 + i] = DrvPalette[0x80 | (DrvColPROM[0x400 + i] & 0xf)];
	}
}

static void draw_sprites()
{
	INT32 bank = (video_registers[0] & 0x30) << 3;

	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 code = DrvSprRAM[offs + 1];
		INT32 attr = DrvSprRAM[offs + 2];
		INT32 color = attr>>4;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x08;

		INT32 sx = (DrvSprRAM[offs + 3] - 0x80) + (256 * (attr & 0x01));
		INT32 sy = 240 - DrvSprRAM[offs];

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (code & 0x80) code += bank;

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, video_registers[1] + (video_registers[2] * 256));
	GenericTilemapSetScrollY(0, video_registers[3]);
	
	flipscreen = video_registers[0] & 0x04;

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);
	draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Cop01Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	//ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[2] = (DrvInputs[2] & ~0x20) + (DrvDips[2] & 0x20); // Service Mode
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 MightguyFrame()
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

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		INT32 lastcyc = ZetTotalCycles();
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (dac_intrl_table[i]) { // clock-in the dac @ prot_dac_freq (see dac_recalc_freq())
			mightguy_prot_dac_clk();
		}

		if (prot_mgtimer) {
			prot_mgtimer_count += ZetTotalCycles() - lastcyc;
			if (prot_mgtimer_count >= prot_mgtimer) {
				mightguy_prot_mgtimer_cb();
			}
		}

		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrameYM3526(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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

		if (mightguy == 0)
		{
			AY8910Scan(nAction, pnMin);
		}
		else
		{
			DACScan(nAction, pnMin);
			BurnYM3526Scan(nAction, pnMin);
		}

		SCAN_VAR(timer_pulse);
		SCAN_VAR(video_registers);
		SCAN_VAR(soundlatch);

		SCAN_VAR(protection_command);
		SCAN_VAR(prot_rom_op);
		SCAN_VAR(prot_rom_address);
		SCAN_VAR(prot_adj_address);
		SCAN_VAR(prot_mgtimer);
		SCAN_VAR(prot_mgtimer_count);
		SCAN_VAR(prot_timer_reg);
		SCAN_VAR(prot_dac_start_address);
		SCAN_VAR(prot_dac_current_address);
		SCAN_VAR(prot_dac_freq);
		SCAN_VAR(prot_dac_playing);
		SCAN_VAR(prot_const90);
	}

	return 0;
}


// Cop 01 (set 1)

static struct BurnRomInfo cop01RomDesc[] = {
	{ "cop01.2b",		0x4000, 0x5c2734ab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "cop02.4b",		0x4000, 0x9c7336ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cop03.5b",		0x4000, 0x2566c8bf, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cop15.17b",		0x4000, 0x6a5f08fa, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "cop16.18b",		0x4000, 0x56bf6946, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "cop14.15g",		0x2000, 0x066d1c55, 3 | BRF_GRA },           //  5 Characters

	{ "cop04.15c",		0x4000, 0x622d32e6, 4 | BRF_GRA },           //  6 Tiles
	{ "cop05.16c",		0x4000, 0xc6ac5a35, 4 | BRF_GRA },           //  7

	{ "cop06.3g",		0x2000, 0xf1c1f4a5, 5 | BRF_GRA },           //  8 Sprites
	{ "cop07.5g",		0x2000, 0x11db7b72, 5 | BRF_GRA },           //  9
	{ "cop08.6g",		0x2000, 0xa63ddda6, 5 | BRF_GRA },           // 10
	{ "cop09.8g",		0x2000, 0x855a2ec3, 5 | BRF_GRA },           // 11
	{ "cop10.3e",		0x2000, 0x444cb19d, 5 | BRF_GRA },           // 12
	{ "cop11.5e",		0x2000, 0x9078bc04, 5 | BRF_GRA },           // 13
	{ "cop12.6e",		0x2000, 0x257a6706, 5 | BRF_GRA },           // 14
	{ "cop13.8e",		0x2000, 0x07c4ea66, 5 | BRF_GRA },           // 15

	{ "copproma.13d",	0x0100, 0x97f68a7a, 6 | BRF_GRA },           // 16 Color data
	{ "coppromb.14d",	0x0100, 0x39a40b4c, 6 | BRF_GRA },           // 17
	{ "coppromc.15d",	0x0100, 0x8181748b, 6 | BRF_GRA },           // 18
	{ "coppromd.19d",	0x0100, 0x6a63dbb8, 6 | BRF_GRA },           // 19
	{ "copprome.2e",	0x0100, 0x214392fa, 6 | BRF_GRA },           // 20
	{ "13b",			0x0100, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(cop01)
STD_ROM_FN(cop01)

struct BurnDriver BurnDrvCop01 = {
	"cop01", NULL, NULL, NULL, "1985",
	"Cop 01 (set 1)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cop01RomInfo, cop01RomName, NULL, NULL, NULL, NULL, Cop01InputInfo, Cop01DIPInfo,
	Cop01Init, DrvExit, Cop01Frame, DrvDraw, DrvScan, &DrvRecalc, 0x190,
	256, 224, 4, 3
};


// Cop 01 (set 2)

static struct BurnRomInfo cop01aRomDesc[] = {
	{ "cop01alt.001",	0x4000, 0xa13ee0d3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "cop01alt.002",	0x4000, 0x20bad28e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cop01alt.003",	0x4000, 0xa7e10b79, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "cop01alt.015",	0x4000, 0x95be9270, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "cop01alt.016",	0x4000, 0xc20bf649, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "cop01alt.014",	0x2000, 0xedd8a474, 3 | BRF_GRA },           //  5 Characters

	{ "cop04.15c",		0x4000, 0x622d32e6, 4 | BRF_GRA },           //  6 Tiles
	{ "cop05.16c",		0x4000, 0xc6ac5a35, 4 | BRF_GRA },           //  7

	{ "cop01alt.006",	0x2000, 0xcac7dac8, 5 | BRF_GRA },           //  8 Sprites
	{ "cop07.5g",		0x2000, 0x11db7b72, 5 | BRF_GRA },           //  9
	{ "cop08.6g",		0x2000, 0xa63ddda6, 5 | BRF_GRA },           // 10
	{ "cop09.8g",		0x2000, 0x855a2ec3, 5 | BRF_GRA },           // 11
	{ "cop01alt.010",	0x2000, 0x94aee9d6, 5 | BRF_GRA },           // 12
	{ "cop11.5e",		0x2000, 0x9078bc04, 5 | BRF_GRA },           // 13
	{ "cop12.6e",		0x2000, 0x257a6706, 5 | BRF_GRA },           // 14
	{ "cop13.8e",		0x2000, 0x07c4ea66, 5 | BRF_GRA },           // 15

	{ "copproma.13d",	0x0100, 0x97f68a7a, 6 | BRF_GRA },           // 16 Color data
	{ "coppromb.14d",	0x0100, 0x39a40b4c, 6 | BRF_GRA },           // 17
	{ "coppromc.15d",	0x0100, 0x8181748b, 6 | BRF_GRA },           // 18
	{ "coppromd.19d",	0x0100, 0x6a63dbb8, 6 | BRF_GRA },           // 19
	{ "copprome.2e",	0x0100, 0x214392fa, 6 | BRF_GRA },           // 20
	{ "13b",			0x0100, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(cop01a)
STD_ROM_FN(cop01a)

struct BurnDriver BurnDrvCop01a = {
	"cop01a", "cop01", NULL, NULL, "1985",
	"Cop 01 (set 2)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cop01aRomInfo, cop01aRomName, NULL, NULL, NULL, NULL, Cop01InputInfo, Cop01DIPInfo,
	Cop01Init, DrvExit, Cop01Frame, DrvDraw, DrvScan, &DrvRecalc, 0x190,
	256, 224, 4, 3
};


// Mighty Guy

static struct BurnRomInfo mightguyRomDesc[] = {
	{ "1.2b",			0x4000, 0xbc8e4557, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.4b",			0x4000, 0xfb73d684, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.5b",			0x4000, 0xb14b6ab8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "11.15b",			0x4000, 0x576183ea, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "10.ic2",			0x8000, 0x1a5d2bb1, 3 | BRF_GRA },           //  4 Protection data

	{ "10.15g",			0x2000, 0x17874300, 4 | BRF_GRA },           //  5 Characters

	{ "4.15c",			0x4000, 0x84d29e76, 5 | BRF_GRA },           //  6 Tiles
	{ "5.16c",			0x4000, 0xf7bb8d82, 5 | BRF_GRA },           //  7

	{ "6.3g",			0x2000, 0x6ff88615, 6 | BRF_GRA },           //  8 Sprites
	{ "7.8g",			0x8000, 0xe79ea66f, 6 | BRF_GRA },           //  9
	{ "8.3e",			0x2000, 0x29f6eb44, 6 | BRF_GRA },           // 10
	{ "9.8e",			0x8000, 0xb9f64c6f, 6 | BRF_GRA },           // 11

	{ "clr.13d",		0x0100, 0xc4cf0bdd, 7 | BRF_GRA },           // 12 Color data
	{ "clr.14d",		0x0100, 0x5b3b8a9b, 7 | BRF_GRA },           // 13
	{ "clr.15d",		0x0100, 0x6c072a64, 7 | BRF_GRA },           // 14
	{ "clr.19d",		0x0100, 0x19b66ac6, 7 | BRF_GRA },           // 15
	{ "2e",				0x0100, 0xd9c45126, 7 | BRF_GRA },           // 16
	{ "13b",			0x0100, 0x4a6f9a6d, 7 | BRF_OPT },           // 17
};

STD_ROM_PICK(mightguy)
STD_ROM_FN(mightguy)

struct BurnDriver BurnDrvMightguy = {
	"mightguy", NULL, NULL, NULL, "1986",
	"Mighty Guy\0", "Sound issues", "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mightguyRomInfo, mightguyRomName, NULL, NULL, NULL, NULL, Cop01InputInfo, MightguyDIPInfo,
	MightguyInit, DrvExit, MightguyFrame, DrvDraw, DrvScan, &DrvRecalc, 0x190,
	224, 256, 3, 4
};
