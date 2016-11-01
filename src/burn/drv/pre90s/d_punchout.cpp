// FB Alpha Punch Out!!! driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "vlm5030.h"
#include "nes_apu.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvVLMROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgtRAM;
static UINT8 *DrvBgbRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSndRAM;

static UINT8 *soundlatch;
static UINT8 *interrupt_enable;
static UINT8 *DrvSprCtrl;

static UINT8 *spunchout_prot_mem;
static UINT8  spunchout_prot_mode = 0;

static UINT16 *DrvTmpDraw;
static UINT16 *DrvTmpDraw2;

static UINT32 *Palette;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo PunchoutInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Punchout)

static struct BurnInputInfo SpnchoutInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Spnchout)

static struct BurnInputInfo ArmwrestInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Armwrest)

static struct BurnDIPInfo PunchoutDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL					},
	{0x0a, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    13, "Coinage"				},
	{0x09, 0x01, 0x0f, 0x0e, "5 Coins 1 Credits"			},
	{0x09, 0x01, 0x0f, 0x0b, "4 Coins 1 Credits"			},
	{0x09, 0x01, 0x0f, 0x0c, "3 Coins 1 Credits"			},
	{0x09, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"			},
	{0x09, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"			},
	{0x09, 0x01, 0x0f, 0x08, "1 Coin/2 Credits (2 Credits/1 Play)"	},
	{0x09, 0x01, 0x0f, 0x0d, "1 Coin/3 Credits (2 Credits/1 Play)"	},
	{0x09, 0x01, 0x0f, 0x02, "1 Coin  2 Credits"			},
	{0x09, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"			},
	{0x09, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"			},
	{0x09, 0x01, 0x0f, 0x0a, "1 Coin  5 Credits"			},
	{0x09, 0x01, 0x0f, 0x07, "1 Coin  6 Credits"			},
	{0x09, 0x01, 0x0f, 0x0f, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Copyright"				},
	{0x09, 0x01, 0x80, 0x00, "Nintendo"				},
	{0x09, 0x01, 0x80, 0x80, "Nintendo of America Inc."		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x03, 0x00, "Easy"					},
	{0x0a, 0x01, 0x03, 0x01, "Medium"				},
	{0x0a, 0x01, 0x03, 0x02, "Hard"					},
	{0x0a, 0x01, 0x03, 0x03, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Time"					},
	{0x0a, 0x01, 0x0c, 0x00, "Longest"				},
	{0x0a, 0x01, 0x0c, 0x04, "Long"					},
	{0x0a, 0x01, 0x0c, 0x08, "Short"				},
	{0x0a, 0x01, 0x0c, 0x0c, "Shortest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0a, 0x01, 0x10, 0x00, "Off"					},
	{0x0a, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Rematch At A Discount"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"					},
	{0x0a, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0a, 0x01, 0x80, 0x00, "Off"					},
	{0x0a, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Punchout)

static struct BurnDIPInfo SpnchoutDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x20, NULL					},
	{0x0b, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    14, "Coinage"				},
	{0x0a, 0x01, 0x0f, 0x08, "6 Coins 1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x04, "5 Coins 1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x03, "4 Coins 1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x0c, "3 Coins 1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"			},
	{0x0a, 0x01, 0x0f, 0x0d, "1 Coin/3 Credits (2 Credits/1 Play)"	},
	{0x0a, 0x01, 0x0f, 0x02, "1 Coin  2 Credits"			},
	{0x0a, 0x01, 0x0f, 0x0b, "1 Coin/2 Credits (3 Credits/1 Play)"	},
	{0x0a, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"			},
	{0x0a, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"			},
	{0x0a, 0x01, 0x0f, 0x0a, "1 Coin  5 Credits"			},
	{0x0a, 0x01, 0x0f, 0x07, "1 Coin  6 Credits"			},
	{0x0a, 0x01, 0x0f, 0x0f, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Copyright"				},
	{0x0a, 0x01, 0x80, 0x00, "Nintendo"				},
	{0x0a, 0x01, 0x80, 0x80, "Nintendo of America Inc."		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0b, 0x01, 0x03, 0x00, "Easy"					},
	{0x0b, 0x01, 0x03, 0x01, "Medium"				},
	{0x0b, 0x01, 0x03, 0x02, "Hard"					},
	{0x0b, 0x01, 0x03, 0x03, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Time"					},
	{0x0b, 0x01, 0x0c, 0x00, "Longest"				},
	{0x0b, 0x01, 0x0c, 0x04, "Long"					},
	{0x0b, 0x01, 0x0c, 0x08, "Short"				},
	{0x0b, 0x01, 0x0c, 0x0c, "Shortest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0b, 0x01, 0x10, 0x00, "Off"					},
	{0x0b, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Rematch At A Discount"		},
	{0x0b, 0x01, 0x20, 0x00, "Off"					},
	{0x0b, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0b, 0x01, 0x80, 0x00, "Off"					},
	{0x0b, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Spnchout)

static struct BurnDIPInfo ArmwrestDIPList[]=
{
	{0x07, 0xff, 0xff, 0x00, NULL		},
	{0x08, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    16, "Coinage 1"	},
	{0x07, 0x01, 0x0f, 0x00, "0000"		},
	{0x07, 0x01, 0x0f, 0x01, "0001"		},
	{0x07, 0x01, 0x0f, 0x02, "0010"		},
	{0x07, 0x01, 0x0f, 0x03, "0011"		},
	{0x07, 0x01, 0x0f, 0x04, "0100"		},
	{0x07, 0x01, 0x0f, 0x05, "0101"		},
	{0x07, 0x01, 0x0f, 0x06, "0110"		},
	{0x07, 0x01, 0x0f, 0x07, "0111"		},
	{0x07, 0x01, 0x0f, 0x08, "1000"		},
	{0x07, 0x01, 0x0f, 0x09, "1001"		},
	{0x07, 0x01, 0x0f, 0x0a, "1010"		},
	{0x07, 0x01, 0x0f, 0x0b, "1011"		},
	{0x07, 0x01, 0x0f, 0x0c, "1100"		},
	{0x07, 0x01, 0x0f, 0x0d, "1101"		},
	{0x07, 0x01, 0x0f, 0x0e, "1110"		},
	{0x07, 0x01, 0x0f, 0x0f, "1111"		},

	{0   , 0xfe, 0   ,    2, "Coin Slots"	},
	{0x07, 0x01, 0x40, 0x40, "1"		},
	{0x07, 0x01, 0x40, 0x00, "2"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"	},
	{0x08, 0x01, 0x03, 0x00, "Easy"		},
	{0x08, 0x01, 0x03, 0x01, "Medium"	},
	{0x08, 0x01, 0x03, 0x02, "Hard"		},
	{0x08, 0x01, 0x03, 0x03, "Hardest"	},

	{0   , 0xfe, 0   ,    16, "Coinage 2"	},
	{0x08, 0x01, 0x3c, 0x00, "0000"		},
	{0x08, 0x01, 0x3c, 0x04, "0001"		},
	{0x08, 0x01, 0x3c, 0x08, "0010"		},
	{0x08, 0x01, 0x3c, 0x0c, "0011"		},
	{0x08, 0x01, 0x3c, 0x10, "0100"		},
	{0x08, 0x01, 0x3c, 0x14, "0101"		},
	{0x08, 0x01, 0x3c, 0x18, "0110"		},
	{0x08, 0x01, 0x3c, 0x1c, "0111"		},
	{0x08, 0x01, 0x3c, 0x20, "1000"		},
	{0x08, 0x01, 0x3c, 0x24, "1001"		},
	{0x08, 0x01, 0x3c, 0x28, "1010"		},
	{0x08, 0x01, 0x3c, 0x2c, "1011"		},
	{0x08, 0x01, 0x3c, 0x30, "1100"		},
	{0x08, 0x01, 0x3c, 0x34, "1101"		},
	{0x08, 0x01, 0x3c, 0x38, "1110"		},
	{0x08, 0x01, 0x3c, 0x3c, "1111"		},

	{0   , 0xfe, 0   ,    2, "Rematches"	},
	{0x08, 0x01, 0x40, 0x40, "3"		},
	{0x08, 0x01, 0x40, 0x00, "7"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x08, 0x01, 0x80, 0x00, "Off"		},
	{0x08, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Armwrest)

static UINT8 spunchout_prot_read(UINT8 offset)
{
	offset >>= 4;

	if (offset <= 0x0c)
	{
		switch (spunchout_prot_mode & 3)
		{
			case 0: // time
				switch ( offset )
				{
					case 0x00:  // 1-second counter
						return spunchout_prot_mem[0x00];

					case 0x01:  // 10-second counter
						return spunchout_prot_mem[0x01] & 0x7;

					case 0x02:  // 1-minute counter
						return spunchout_prot_mem[0x02];

					case 0x03:  // 10-minute counter
						return spunchout_prot_mem[0x03] & 0x07;

					case 0x04:  // 1-hour counter
						return spunchout_prot_mem[0x04];

					case 0x05:  // 10-hour counter
						return spunchout_prot_mem[0x05] & 0x03;

					case 0x06:  // day-of-the-week counter
						return spunchout_prot_mem[0x06] & 0x07;

					case 0x07:  // 1-day counter
						return spunchout_prot_mem[0x07];

					case 0x08:  // 10-day counter
						return spunchout_prot_mem[0x08] & 0x03;

					case 0x09:  // 1-month counter
						return spunchout_prot_mem[0x09];

					case 0x0a:  // 10-month counter
						return spunchout_prot_mem[0x0a] & 0x01;

					case 0x0b:  // 1-year counter
						return spunchout_prot_mem[0x0b];

					case 0x0c:  // 10-year counter
						return spunchout_prot_mem[0x0c];
				}
				break;

			case 1: // alarm
				switch ( offset )
				{
					case 0x00:  // n/a
						return 0x00;

					case 0x01:  // n/a
						return 0x00;

					case 0x02:  // 1-minute alarm register
						return spunchout_prot_mem[0x12];

					case 0x03:  // 10-minute alarm register
						return spunchout_prot_mem[0x13] & 0x07;

					case 0x04:  // 1-hour alarm register
						return spunchout_prot_mem[0x14];

					case 0x05:  // 10-hour alarm register
						return spunchout_prot_mem[0x15] & 0x03;

					case 0x06:  // day-of-the-week alarm register
						return spunchout_prot_mem[0x16] & 0x07;

					case 0x07:  // 1-day alarm register
						return spunchout_prot_mem[0x17];

					case 0x08:  // 10-day alarm register
						return spunchout_prot_mem[0x18] & 0x03;

					case 0x09:  // n/a
						return 0xc0; // ok?

					case 0x0a:  // /12/24 select register
						return spunchout_prot_mem[0x1a] & 0x01;

					case 0x0b:  // leap year count
						return spunchout_prot_mem[0x1b] & 0x03;

					case 0x0c:  // n/a
						return 0x00;
				}
				break;

			case 2: // RAM BLOCK 10
			case 3: // RAM BLOCK 11
				return spunchout_prot_mem[0x10 * (spunchout_prot_mode & 3) + offset];
		}
	}
	else if (offset == 0x0d)
	{
		return spunchout_prot_mode;
	}

	return 0;
}

static void spunchout_prot_write(UINT8 offset, UINT8 data)
{
	offset >>= 4;
	data &= 0x0f;

	if (offset <= 0x0c)
	{
		spunchout_prot_mem[0x10 * (spunchout_prot_mode & 3) + offset] = data;
	}
	else if (offset == 0x0d)
	{
		spunchout_prot_mode = data;
	}
}

static void __fastcall punchout_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			// nop
		return;

		case 0x02:
		case 0x03:
			soundlatch[port & 1] = data;
		return;

		case 0x04:
			vlm5030_data_write(0, data);
		return;

		case 0x05:
		case 0x06:
		return; // nop

		case 0x08:
			*interrupt_enable = data;
		return;

		case 0x09:
		case 0x0a:
		return; // nop

		case 0x0b:
			if (data & 1) M6502Reset(); // iq_132 fix this!
		return;

		case 0x0c:
			vlm5030_rst(0, data & 0x01);
		return;

		case 0x0d:
			vlm5030_st(0, data & 0x01);
		return;

		case 0x0e:
			vlm5030_vcu(0, data & 0x01);
		return;

		case 0x0f:
		return; // nop
	}

	if ((port & 0x0f) == 0x07) spunchout_prot_write(port, data);
}

static UINT8 __fastcall punchout_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0] ^ 0x40;

		case 0x01:
			return DrvInputs[1];

		case 0x02:
			return DrvDips[0];

		case 0x03:
			return (DrvDips[1] & 0xef) | ((vlm5030_bsy(0)) ? 0x00 : 0x10);
	}

	if ((port & 0x0f) == 0x07) return spunchout_prot_read(port);

	return 0;
}

static void sound_write(UINT16 a, UINT8 d)
{
	if ((a & 0xffe0) == 0x4000) {
		nesapuWrite(0, a & 0x1f, d);
		return;
	}
}

static UINT8 sound_read(UINT16 a)
{
	if ((a & 0xffe0) == 0x4000) {
		if ((a & 0xfffe) == 0x4016) {
			return soundlatch[a & 1];
		}

		return nesapuRead(0, a & 0x1f);
	}

	return 0;
}

static UINT32 punchout_vlm_sync(INT32 samples_rate)
{
	return (samples_rate * ZetTotalCycles()) / 66666 /* 8000000 / 2 / 60 */;
}

static UINT32 punchout_nesapu_sync(INT32 samples_rate)
{
	return (samples_rate * M6502TotalCycles()) / 29830 /* 1789773 / 60 */;
}

static INT32 DrvDoReset()
{
	memset (AllRam,   0, RamEnd - AllRam);
	memset (DrvNVRAM, 0, 0x400);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	vlm5030Reset(0);

	spunchout_prot_mode = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvSndROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x080000;
	DrvGfxROM3		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x001000;

	DrvVLMROM		= Next; Next += 0x010000;

	Palette			= (UINT32*)Next; Next += 0x0400 * sizeof(INT32);
	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(INT32);

	DrvNVRAM		= Next; Next += 0x000400;

	DrvTmpDraw		= (UINT16*)Next; Next += 128 * 256 * sizeof(UINT16);
	DrvTmpDraw2		= (UINT16*)Next; Next += 128 * 256 * sizeof(UINT16);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvBgtRAM		= Next; Next += 0x000800;
	DrvBgbRAM		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSndRAM		= Next; Next += 0x000800;

	soundlatch		= Next; Next += 0x000002;
	interrupt_enable	= Next; Next += 0x000001;

	spunchout_prot_mem	= Next; Next += 0x000040;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(UINT8 *rom, INT32 len, UINT8 xor1, INT32 depth) // 0x100, 0, 0x100, 0 (mod 0x40 always)
{
	UINT8 *tmp = (UINT8*)malloc(len);
	if (tmp == NULL || len == 0) return;

	for (INT32 i = 0; i < len; i++) tmp[i] = rom[i] ^ xor1; // copy & invert

	for (INT32 i = 0; i < (len / 3) * 8; i++)
	{
		rom[i]  = (tmp[(i / 8) + (len / 3) * 2] >> (7 - (i & 7))) & 1;
		rom[i] <<= 1;
		rom[i] |= (tmp[(i / 8) + (len / 3) * 1] >> (7 - (i & 7))) & 1;
		rom[i] <<= 1;
		rom[i] |= (tmp[(i / 8) + (len / 3) * 0] >> (7 - (i & 7))) & 1;
		rom[i] &= depth;
	}

	free (tmp);
}

static void DrvPaletteInit(INT32 off, INT32 bank, INT32 reverse)
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = ~DrvColPROM[i + 0x000 + 0x100 * bank] & 0x0f;
		UINT8 g = ~DrvColPROM[i + 0x200 + 0x100 * bank] & 0x0f;
		UINT8 b = ~DrvColPROM[i + 0x400 + 0x100 * bank] & 0x0f;

		r |= r << 4;
		g |= g << 4;
		b |= b << 4;

		Palette[off * 0x100 + (i^reverse)] = (r << 16) | (g << 8) | b;
	}

	DrvRecalc = 1;
}

static INT32 CommonInit(INT32 (*pInitCallback)(), INT32 punchout, INT32 reverse_palette, UINT32 gfx_xor)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	memset (DrvGfxROM0, 0xff, 0x020000);
	memset (DrvGfxROM1, 0xff, 0x020000);
	memset (DrvGfxROM2, 0xff, 0x080000);
	memset (DrvGfxROM3, 0xff, 0x040000);

	if (pInitCallback) {
		if (pInitCallback()) return 1;
	}

	DrvGfxDecode(DrvGfxROM0, 0x0c000, (gfx_xor >>  0), 3);
	DrvGfxDecode(DrvGfxROM1, 0x0c000, (gfx_xor >>  8), (punchout) ? 3 : 7);
	DrvGfxDecode(DrvGfxROM2, 0x30000, (gfx_xor >> 16), 7);
	DrvGfxDecode(DrvGfxROM3, 0x18000, (gfx_xor >> 24), 3);

	DrvPaletteInit(0, 0, (reverse_palette >> 0) & 0xff); // top bank 0
	DrvPaletteInit(1, 1, (reverse_palette >> 0) & 0xff); // top bank 1
	DrvPaletteInit(2, 6, (reverse_palette >> 8) & 0xff); // bot bank 0
	DrvPaletteInit(3, 7, (reverse_palette >> 8) & 0xff); // bot bank 1

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xc3ff, 0, DrvNVRAM);
	ZetMapArea(0xc000, 0xc3ff, 1, DrvNVRAM);
	ZetMapArea(0xc000, 0xc3ff, 2, DrvNVRAM);
	ZetMapArea(0xd000, 0xd7ff, 0, DrvZ80RAM);
	ZetMapArea(0xd000, 0xd7ff, 1, DrvZ80RAM);
	ZetMapArea(0xd000, 0xd7ff, 2, DrvZ80RAM);
	ZetMapArea(0xe000, 0xefff, 0, DrvSprRAM);
	ZetMapArea(0xe000, 0xefff, 1, DrvSprRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvSprRAM);

	if (punchout)
	{
		ZetMapArea(0xd800, 0xdfff, 0, DrvBgtRAM);
		ZetMapArea(0xd800, 0xdfff, 1, DrvBgtRAM);
		ZetMapArea(0xd800, 0xdfff, 2, DrvBgtRAM);

		ZetMapArea(0xf000, 0xffff, 0, DrvBgbRAM);
		ZetMapArea(0xf000, 0xffff, 1, DrvBgbRAM);
		ZetMapArea(0xf000, 0xffff, 2, DrvBgbRAM);

		DrvSprCtrl = DrvBgtRAM + 0x7f0;
	}
	else
	{
		ZetMapArea(0xd800, 0xdfff, 0, DrvFgRAM);
		ZetMapArea(0xd800, 0xdfff, 1, DrvFgRAM);
		ZetMapArea(0xd800, 0xdfff, 2, DrvFgRAM);

		ZetMapArea(0xf000, 0xf7ff, 0, DrvBgbRAM);
		ZetMapArea(0xf000, 0xf7ff, 1, DrvBgbRAM);
		ZetMapArea(0xf000, 0xf7ff, 2, DrvBgbRAM);

		ZetMapArea(0xf800, 0xffff, 0, DrvBgtRAM);
		ZetMapArea(0xf800, 0xffff, 1, DrvBgtRAM);
		ZetMapArea(0xf800, 0xffff, 2, DrvBgtRAM);

		DrvSprCtrl = DrvFgRAM + 0x7f0;
	}

	ZetSetOutHandler(punchout_write_port);
	ZetSetInHandler(punchout_read_port);
	ZetClose();

	M6502Init(0, TYPE_N2A03);
	M6502Open(0);
	M6502MapMemory(DrvSndRAM, 0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvSndROM, 0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(sound_write);
	M6502SetReadHandler(sound_read);
	M6502Close();

	vlm5030Init(0, 3580000, punchout_vlm_sync, DrvVLMROM, 0x4000, 1);
	vlm5030SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);

	nesapuInit(0, 1789773, punchout_nesapu_sync, 0);
	nesapuSetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	M6502Exit();
	vlm5030Exit();
	nesapuExit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static void predraw_big_sprite(UINT16 *bitmap, UINT8 *ram, UINT8 *rom, INT32 bpp, INT32 paloff)
{
	for (INT32 i = 0; i < 16 * 32; i++)
	{
		INT32 ofst = i;

		INT32 sx = (i & 0xf) * 8;
		INT32 sy = (i / 0x10) * 8;

		INT32 attr = ram[ofst * 4 + 3];
		INT32 code = ram[ofst * 4] + ((ram[i * 4 + 1] & (0x1f >> (3 - bpp))) << 8);

		INT32 color = ((attr & (0xff >> bpp)) << bpp) + paloff;
		INT32 flipx = (attr & 0x80) ? 7 : 0;

		{
			UINT8 *src = rom + (code * 8 * 8);
			UINT16 *dst = bitmap + (sy * 128) + sx; 

			for (INT32 y = 0; y < 8; y++) {
				for (INT32 x = 0; x < 8; x++) {
					INT32 t = src[(y * 8) + (x ^ flipx)];
					if (t == ((1 << bpp)-1)) t |= 0x8000;
					dst[(y * 128) + x] = t | color;
				}
			}
		}
	}
}

static void draw_layer(UINT8 *ram, UINT8 *rom, INT32 wide, INT32 paloff, INT32 scroll, INT32 allow_flip)
{
	for (INT32 i = wide * 2; i < wide * 30; i++)
	{
		INT32 sx = (i % wide) * 8;
		INT32 sy = (i / wide) * 8;

		INT32 attr = ram[i * 2 + 1];
		INT32 code = ram[i * 2 + 0] + ((attr & 0x03) << 8);
		INT32 color = (attr & 0x7c) >> 2;
		INT32 flipx = 0;

		if (allow_flip == 0) {
			code += (attr & 0x80) << 3;
		} else {
			flipx = attr & 0x80;
		}

		if (scroll)
		{
			INT32 scrollx = ram[(sy/8)*2] + ((ram[(sy/8)*2+1] & 0x01) << 8);

			sx -= (56 + scrollx) & 0x1ff;
			if (sx < -7) sx += 256;
		}

		sy -= 16;

		if (sx >= nScreenWidth) continue;

		if (flipx) {
			Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, paloff, rom);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, paloff, rom);
		}
	}
}

static void copy_roz(UINT16 *src, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy, INT32 wrap, INT32 transp, INT32 wide)
{
	UINT16 *dst = pTransDraw;

	INT32 width;
	INT32 height;
	if (wide) {
		width = 0x100;
		height = 0x80;
	} else {
		width = 0x80;
		height = 0x100;
	}

	for (INT32 sy = 0; sy < nScreenHeight; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		dst = pTransDraw + sy * nScreenWidth;

		if (transp) {
			for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++)
			{
				INT32 xx = cx / 0x10000;
				INT32 yy = cy / 0x10000;

				if (wrap) {
					xx %= width;
					yy %= height;
				} else {
					if (xx >= width) continue;
					if (yy >= height) continue;
				}

				INT32 pxl = src[(yy * width) + xx];
	
				if (!(pxl & 0x8000)) {
					*dst = pxl;
				}
			}
		} else {
			for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++)
			{
				INT32 xx = cx / 0x10000;
				INT32 yy = cy / 0x10000;

				if (wrap) {
					xx %= width;
					yy %= height;
				} else {
					if (xx >= width) continue;
					if (yy >= height) continue;
				}

				*dst = src[(yy * width) + xx] & 0xfff;
			}
		}
	}
}

static void draw_big_sprite(INT32 type2)
{
	INT32 zoom = DrvSprCtrl[0] + 256 * (DrvSprCtrl[1] & 0x0f);

	if (zoom)
	{
		INT32 sx = 4096 - (DrvSprCtrl[2] + 256 * (DrvSprCtrl[3] & 0x0f));
		if (sx > 4096-4*127) sx -= 4096;

		INT32 sy = -(DrvSprCtrl[4] + 256 * (DrvSprCtrl[5] & 1));
		if (sy <= -256 + zoom/0x40) sy += 512;
		sy += 12;

		sy -= 16;

		INT32 incxx = zoom << 6;
		INT32 incyy = zoom << 6;

		INT32 startx = -sx * 0x4000;
		INT32 starty = -sy * 0x10000;
		startx += 3740 * zoom;  /* adjustment to match the screen shots */
		starty -= 178 * zoom;   /* and make the hall of fame picture nice */

		if (DrvSprCtrl[6] & 1)   /* flip x */
		{
			if (type2) {
				startx = ((32 * 8) << 16) - startx - 1;
			} else {
				startx = ((16 * 8) << 16) - startx - 1;
			}
			incxx = -incxx;
		}

		copy_roz(DrvTmpDraw, startx, starty + 0x200*(2) * zoom, incxx,0,0,incyy, 0, 1, type2);
	}
}

static void draw_big_sprite2()
{
	INT32 incxx;

	INT32 sx = 512 - (DrvSprCtrl[0x08] + 256 * (DrvSprCtrl[0x09] & 1));
	if (sx > 512-127) sx -= 512;
	sx -= 55;   /* adjustment to match the screen shots */

	INT32 sy = -DrvSprCtrl[0x0a] + 256 * (DrvSprCtrl[0x0b] & 1);
	sy += 3;    /* adjustment to match the screen shots */

	sy -= 16;

	sx = -sx << 16;
	sy = -sy << 16;

	if (DrvSprCtrl[0x0c] & 1)   /* flip x */
	{
		sx = ((16 * 8) << 16) - sx - 1;
		incxx = -1;
	}
	else
	{
		incxx = 1;
	}

	copy_roz(DrvTmpDraw2, sx, sy, incxx << 16, 0, 0, 1 << 16, 0, 1, 0);
}

static INT32 PunchoutDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i++) {
			INT32 d = Palette[i];
			DrvPalette[i] = BurnHighCol(d >> 16, (d >> 8) & 0xff, d & 0xff, 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	INT32 palbank0 = ((DrvSprCtrl[0x0d] & 0x02) >> 1) * 0x100;
	INT32 palbank1 = ((DrvSprCtrl[0x0d] & 0x01) >> 0) * 0x100;
	predraw_big_sprite(DrvTmpDraw2, DrvSprRAM + 0x800, DrvGfxROM3, 2, palbank1 + 0x200);

	nScreenHeight -= 224;
	if (nBurnLayer & 1) draw_layer(DrvBgtRAM, DrvGfxROM0, 32, palbank0, 0, 1);

	if (DrvSprCtrl[0x07] & 0x01 && nSpriteEnable & 0x01) {
		predraw_big_sprite(DrvTmpDraw , DrvSprRAM + 0x000, DrvGfxROM2, 3, palbank0);
		draw_big_sprite(0);
	}

	pTransDraw += 224 * 256;
	if (nBurnLayer & 2) draw_layer(DrvBgbRAM, DrvGfxROM1, 64, palbank1 + 0x200, 1, 1);

	if (DrvSprCtrl[0x07] & 0x02 && nSpriteEnable & 0x02) {
		predraw_big_sprite(DrvTmpDraw , DrvSprRAM + 0x000, DrvGfxROM2, 3, palbank1 + 0x200);
		draw_big_sprite(0);
	}

	if (nSpriteEnable & 0x04)
		draw_big_sprite2();

	pTransDraw -= 224 * 256;
	nScreenHeight += 224;

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void draw_fg_layer(INT32 paloff)
{
	for (INT32 i = 32 * 2; i < 32 * 30; i++)
	{
		INT32 sx = (i % 0x20) * 8;
		INT32 sy = (i / 0x20) * 8;

		INT32 attr = DrvFgRAM[i * 2 + 1];
		INT32 code = DrvFgRAM[i * 2 + 0] + ((attr & 0x07) << 8);
		INT32 color = (attr & 0x78) >> 3;
		INT32 flipx = attr & 0x80;

		if (code == 0) continue;

		sy -= 16;

		if (flipx) {
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 7, paloff, DrvGfxROM1);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 7, paloff, DrvGfxROM1);
		}
	}
}

static void predraw_big_sprite_armwrest(INT32 paloff)
{
	INT32 fx = (DrvSprCtrl[6] & 1) ? 0x10 : 0;

	for (INT32 i = 0; i < 32 * 16; i++)
	{
		INT32 ofst = ((((i ^ fx) & 0x10) + (i >> 5)) << 4) + (i & 0xf);

		INT32 sx = (i & 0x1f) * 8;
		INT32 sy = (i / 0x20) * 8;

		INT32 attr = DrvSprRAM[ofst * 4 + 3];
		INT32 code = DrvSprRAM[ofst * 4] + ((DrvSprRAM[ofst * 4 + 1] & 0x1f) << 8);

		INT32 color = ((attr & 0x1f) << 3) + paloff;
		INT32 flipx = (attr & 0x80) ? 7 : 0;

		{
			UINT8 *src = DrvGfxROM2 + (code * 8 * 8);
			UINT16 *dst = DrvTmpDraw + (sy * 256) + sx; 

			for (INT32 y = 0; y < 8; y++) {
				for (INT32 x = 0; x < 8; x++) {
					INT32 t = src[(y * 8) + (x ^ flipx)];
					if (t == 0x07) t |= 0x8000;
					dst[(y * 256) + x] = t | color;
				}
			}
		}
	}
}

static INT32 ArmwrestDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i++) {
			INT32 d = Palette[i];
			DrvPalette[i] = BurnHighCol(d >> 16, (d >> 8) & 0xff, d & 0xff, 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	INT32 palbank0 = ((DrvSprCtrl[0x0d] & 0x02) >> 1) * 0x100;
	INT32 palbank1 = ((DrvSprCtrl[0x0d] & 0x01) >> 0) * 0x100;

	predraw_big_sprite(DrvTmpDraw2, DrvSprRAM + 0x800, DrvGfxROM3, 2, palbank1 + 0x200);

	nScreenHeight -= 224;

	if (nBurnLayer & 1) draw_layer(DrvBgtRAM, DrvGfxROM0, 32, palbank0, 0, 0);

	if (DrvSprCtrl[0x07] & 0x01 && nSpriteEnable & 0x01) {
		predraw_big_sprite_armwrest(palbank0);
		draw_big_sprite(1);
	}

	pTransDraw += 224 * 256;
	if (nBurnLayer & 2) draw_layer(DrvBgbRAM, DrvGfxROM0, 32, palbank1 + 0x200, 0, 1);

	if (DrvSprCtrl[0x07] & 0x02 && nSpriteEnable & 0x02) {
		predraw_big_sprite_armwrest(palbank1 + 0x200);
		draw_big_sprite(1);
	}

	if (nSpriteEnable & 0x04)
		draw_big_sprite2();

	draw_fg_layer(palbank1 + 0x200);

	pTransDraw -= 224 * 256;
	nScreenHeight += 224;

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 1789773 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetNewFrame();
	M6502NewFrame();

	ZetOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		nCyclesDone[0] += ZetRun(nSegment);

		nSegment = nCyclesTotal[1] / nInterleave;

		nCyclesDone[1] += M6502Run(nSegment);
	}

	if (*interrupt_enable) ZetNmi();

	M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);

	nesapuUpdate(0, pBurnSoundOut, nBurnSoundLen);
	vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);

	M6502Close();
	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		M6502Scan(nAction);
		vlm5030Scan(nAction);
		nesapuScan(nAction);

		SCAN_VAR(spunchout_prot_mode);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000400;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}

static void swap_gfx(UINT8 *src)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x800);

	memcpy (tmp, src + 0x800, 0x800);
	memcpy (src + 0x800, src + 0x1000, 0x800);
	memcpy (src + 0x1000, tmp, 0x800);

	BurnFree(tmp);
}

static INT32 PunchoutLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x02000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x04000,  2, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x06000,  3, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x08000,  4, 1)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x00000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x04000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x08000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x0c000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x10000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x14000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x18000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x1c000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x20000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x24000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x28000, 20, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 21, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x02000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x08000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x0a000, 24, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x00000, 25, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00200, 26, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00400, 27, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00600, 28, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00800, 29, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00a00, 30, 1)) return 1;

	if (BurnLoadRom(DrvVLMROM  + 0x00000, 32, 1)) return 1;

	return 0;
}

static INT32 ArmwrestLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x02000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x04000,  2, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x06000,  3, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x08000,  4, 1)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x00000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a000, 10, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x04000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x0c000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x10000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x14000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x1c000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x20000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x24000, 18, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM3 + 0x00000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM3 + 0x08000, 20, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x00000, 21, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00200, 22, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00400, 23, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00600, 24, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00800, 25, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00a00, 26, 1)) return 1;

	if (BurnLoadRom(DrvVLMROM  + 0x00000, 29, 1)) return 1;

	return 0;
}

static INT32 SpunchoutLoadRoms()
{
	if (PunchoutLoadRoms()) return 1;

	swap_gfx(DrvGfxROM0 + 0x00000);
	swap_gfx(DrvGfxROM0 + 0x04000);
	swap_gfx(DrvGfxROM1 + 0x00000);
	swap_gfx(DrvGfxROM1 + 0x04000);
	swap_gfx(DrvGfxROM3 + 0x00000);
	swap_gfx(DrvGfxROM3 + 0x02000);
	swap_gfx(DrvGfxROM3 + 0x08000);
	swap_gfx(DrvGfxROM3 + 0x0a000);

	return 0;
}

static INT32 SpunchoutjLoadRoms()
{
	if (PunchoutLoadRoms()) return 1;

	swap_gfx(DrvGfxROM1 + 0x00000);
	swap_gfx(DrvGfxROM1 + 0x04000);
	swap_gfx(DrvGfxROM3 + 0x00000);
	swap_gfx(DrvGfxROM3 + 0x02000);
	swap_gfx(DrvGfxROM3 + 0x08000);
	swap_gfx(DrvGfxROM3 + 0x0a000);

	return 0;
}

// Punch-Out (Rev B)!!

static struct BurnRomInfo punchoutRomDesc[] = {
	{ "chp1-c.8l",	0x2000, 0xa4003adc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chp1-c.8k",	0x2000, 0x745ecf40, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chp1-c.8j",	0x2000, 0x7a7f870e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chp1-c.8h",	0x2000, 0x5d8123d7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chp1-c.8f",	0x4000, 0xc8a55ddb, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",	0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chp1-b.4c",	0x2000, 0x49b763bc, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chp1-b.4d",	0x2000, 0x08bc6d67, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",	0x2000, 0xc075f831, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",	0x2000, 0xc4cc2b5a, 4 | BRF_GRA },           //  9

	{ "chp1-v.2r",	0x4000, 0xbd1d4b2e, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chp1-v.2t",	0x4000, 0xdd9a688a, 5 | BRF_GRA },           // 11
	{ "chp1-v.2u",	0x2000, 0xda6a3c4b, 5 | BRF_GRA },           // 12
	{ "chp1-v.2v",	0x2000, 0x8c734a67, 5 | BRF_GRA },           // 13
	{ "chp1-v.3r",	0x4000, 0x2e74ad1d, 5 | BRF_GRA },           // 14
	{ "chp1-v.3t",	0x4000, 0x630ba9fb, 5 | BRF_GRA },           // 15
	{ "chp1-v.3u",	0x2000, 0x6440321d, 5 | BRF_GRA },           // 16
	{ "chp1-v.3v",	0x2000, 0xbb7b7198, 5 | BRF_GRA },           // 17
	{ "chp1-v.4r",	0x4000, 0x4e5b0fe9, 5 | BRF_GRA },           // 18
	{ "chp1-v.4t",	0x4000, 0x37ffc940, 5 | BRF_GRA },           // 19
	{ "chp1-v.4u",	0x2000, 0x1a7521d4, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",	0x2000, 0x75be7aae, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",	0x2000, 0xdaf74de0, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",	0x2000, 0x4cb7ea82, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",	0x2000, 0x1c0d09aa, 6 | BRF_GRA },           // 24

	{ "chp1-b-6e_pink.6e",	0x0200, 0xe9ca3ac6, 7 | BRF_GRA },       // 25 Color PROMs
	{ "chp1-b-6f_pink.6f",	0x0200, 0x02be56ab, 7 | BRF_GRA },       // 26
	{ "chp1-b-7f_pink.7f",	0x0200, 0x11de55f1, 7 | BRF_GRA },       // 27
	{ "chp1-b-7e_white.7e",	0x0200, 0x47adf7a2, 7 | BRF_GRA },       // 28
	{ "chp1-b-8e_white.8e",	0x0200, 0xb0fc15a8, 7 | BRF_GRA },       // 29
	{ "chp1-b-8f_white.8f",	0x0200, 0x1ffd894a, 7 | BRF_GRA },       // 30
	
	{ "chp1-v-2d.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA },   // 31 Timing PROM

	{ "chp1-c.6p",	0x4000, 0xea0bbb31, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chp1-b-6e_white.6e",	0x0200, 0xddac5f0e, 0 | BRF_OPT },
	{ "chp1-b-6f_white.6f",	0x0200, 0x846c6261, 0 | BRF_OPT },
	{ "chp1-b-7f_white.7f",	0x0200, 0x1682dd30, 0 | BRF_OPT },
	{ "chp1-b-7e_pink.7e",	0x0200, 0xfddaa777, 0 | BRF_OPT },
	{ "chp1-b-8e_pink.8e",	0x0200, 0xc3d5d71f, 0 | BRF_OPT },
	{ "chp1-b-8f_pink.8f",	0x0200, 0xa3037155, 0 | BRF_OPT },   
};

STD_ROM_PICK(punchout)
STD_ROM_FN(punchout)

static INT32 PunchoutInit()
{
	return CommonInit(SpunchoutLoadRoms, 1, 0xff00, 0x00000000);
}

struct BurnDriver BurnDrvPunchout = {
	"punchout", NULL, NULL, NULL, "1984",
	"Punch-Out!! (Rev B)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, punchoutRomInfo, punchoutRomName, NULL, NULL, PunchoutInputInfo, PunchoutDIPInfo,
	PunchoutInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Punch-Out (Rev A)!!

static struct BurnRomInfo punchoutaRomDesc[] = {
	{ "chp1-c.8l",	0x2000, 0xa4003adc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chp1-c.8k",	0x2000, 0x745ecf40, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chp1-c.8j",	0x2000, 0x7a7f870e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chp1-c.8h",	0x2000, 0x5d8123d7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chp1-c.8f",	0x4000, 0xc8a55ddb, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",	0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chp1-b.4c",	0x2000, 0xe26dc8b3, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chp1-b.4d",	0x2000, 0xdd1310ca, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",	0x2000, 0x20fb4829, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",	0x2000, 0xedc34594, 4 | BRF_GRA },           //  9

	{ "chp1-v.2r",	0x4000, 0xbd1d4b2e, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chp1-v.2t",	0x4000, 0xdd9a688a, 5 | BRF_GRA },           // 11
	{ "chp1-v.2u",	0x2000, 0xda6a3c4b, 5 | BRF_GRA },           // 12
	{ "chp1-v.2v",	0x2000, 0x8c734a67, 5 | BRF_GRA },           // 13
	{ "chp1-v.3r",	0x4000, 0x2e74ad1d, 5 | BRF_GRA },           // 14
	{ "chp1-v.3t",	0x4000, 0x630ba9fb, 5 | BRF_GRA },           // 15
	{ "chp1-v.3u",	0x2000, 0x6440321d, 5 | BRF_GRA },           // 16
	{ "chp1-v.3v",	0x2000, 0xbb7b7198, 5 | BRF_GRA },           // 17
	{ "chp1-v.4r",	0x4000, 0x4e5b0fe9, 5 | BRF_GRA },           // 18
	{ "chp1-v.4t",	0x4000, 0x37ffc940, 5 | BRF_GRA },           // 19
	{ "chp1-v.4u",	0x2000, 0x1a7521d4, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",	0x2000, 0x16588f7a, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",	0x2000, 0xdc743674, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",	0x2000, 0xc2db5b4e, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",	0x2000, 0xe6af390e, 6 | BRF_GRA },           // 24

	{ "chp1-b-6e_pink.6e",	0x0200, 0xe9ca3ac6, 7 | BRF_GRA },       // 25 Color PROMs
	{ "chp1-b-6f_pink.6f",	0x0200, 0x02be56ab, 7 | BRF_GRA },       // 26
	{ "chp1-b-7f_pink.7f",	0x0200, 0x11de55f1, 7 | BRF_GRA },       // 27
	{ "chp1-b-7e_white.7e",	0x0200, 0x47adf7a2, 7 | BRF_GRA },       // 28
	{ "chp1-b-8e_white.8e",	0x0200, 0xb0fc15a8, 7 | BRF_GRA },       // 29
	{ "chp1-b-8f_white.8f",	0x0200, 0x1ffd894a, 7 | BRF_GRA },       // 30
	
	{ "chp1-v-2d.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA },   // 31 Timing PROM

	{ "chp1-c.6p",	0x4000, 0xea0bbb31, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chp1-b-6e_white.6e",	0x0200, 0xddac5f0e, 0 | BRF_OPT },
	{ "chp1-b-6f_white.6f",	0x0200, 0x846c6261, 0 | BRF_OPT },
	{ "chp1-b-7f_white.7f",	0x0200, 0x1682dd30, 0 | BRF_OPT },
	{ "chp1-b-7e_pink.7e",	0x0200, 0xfddaa777, 0 | BRF_OPT },
	{ "chp1-b-8e_pink.8e",	0x0200, 0xc3d5d71f, 0 | BRF_OPT },
	{ "chp1-b-8f_pink.8f",	0x0200, 0xa3037155, 0 | BRF_OPT },   
};

STD_ROM_PICK(punchouta)
STD_ROM_FN(punchouta)

static INT32 PunchoutaInit()
{
	return CommonInit(PunchoutLoadRoms, 1, 0xff00, 0xff00ffff);
}

struct BurnDriver BurnDrvPunchouta = {
	"punchouta", "punchout", NULL, NULL, "1984",
	"Punch-Out!! (Rev A)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, punchoutaRomInfo, punchoutaRomName, NULL, NULL, PunchoutInputInfo, PunchoutDIPInfo,
	PunchoutaInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Punch-Out!! (Japan)

static struct BurnRomInfo punchoutjRomDesc[] = {
	{ "chp1-c_8l_a.8l",	0x2000, 0x9735eb5a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chp1-c_8k_a.8k",	0x2000, 0x98baba41, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chp1-c_8j_a.8j",	0x2000, 0x7a7f870e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chp1-c_8h_a.8h",	0x2000, 0x5d8123d7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chp1-c_8f_a.8f",	0x4000, 0xea52cda1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c_4k_a.4k",	0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chp1-b_4c_a.4c",	0x2000, 0xe26dc8b3, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chp1-b_4d_a.4d",	0x2000, 0xdd1310ca, 3 | BRF_GRA },           //  7

	{ "chp1-b_4a_a.4a",	0x2000, 0x20fb4829, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b_4b_a.4b",	0x2000, 0xedc34594, 4 | BRF_GRA },           //  9

	{ "chp1-v_2r_a.2r",	0x4000, 0xbd1d4b2e, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chp1-v_2t_a.2t",	0x4000, 0xdd9a688a, 5 | BRF_GRA },           // 11
	{ "chp1-v_2u_a.2u",	0x2000, 0xda6a3c4b, 5 | BRF_GRA },           // 12
	{ "chp1-v_2v_a.2v",	0x2000, 0x8c734a67, 5 | BRF_GRA },           // 13
	{ "chp1-v_3r_a.3r",	0x4000, 0x2e74ad1d, 5 | BRF_GRA },           // 14
	{ "chp1-v_3t_a.3t",	0x4000, 0x630ba9fb, 5 | BRF_GRA },           // 15
	{ "chp1-v_3u_a.3u",	0x2000, 0x6440321d, 5 | BRF_GRA },           // 16
	{ "chp1-v_3v_a.3v",	0x2000, 0xbb7b7198, 5 | BRF_GRA },           // 17
	{ "chp1-v_4r_a.4r",	0x4000, 0x4e5b0fe9, 5 | BRF_GRA },           // 18
	{ "chp1-v_4t_a.4t",	0x4000, 0x37ffc940, 5 | BRF_GRA },           // 19
	{ "chp1-v_4u_a.4u",	0x2000, 0x1a7521d4, 5 | BRF_GRA },           // 20

	{ "chp1-v_6p_a.6p",	0x2000, 0x16588f7a, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v_6n_a.6n",	0x2000, 0xdc743674, 6 | BRF_GRA },           // 22
	{ "chp1-v_8p_a.8p",	0x2000, 0xc2db5b4e, 6 | BRF_GRA },           // 23
	{ "chp1-v_8n_a.8n",	0x2000, 0xe6af390e, 6 | BRF_GRA },           // 24

	{ "chp1-b-6e_pink.6e",	0x0200, 0xe9ca3ac6, 7 | BRF_GRA },       // 25 Color PROMs
	{ "chp1-b-6f_pink.6f",	0x0200, 0x02be56ab, 7 | BRF_GRA },       // 26
	{ "chp1-b-7f_pink.7f",	0x0200, 0x11de55f1, 7 | BRF_GRA },       // 27
	{ "chp1-b-7e_white.7e",	0x0200, 0x47adf7a2, 7 | BRF_GRA },       // 28
	{ "chp1-b-8e_white.8e",	0x0200, 0xb0fc15a8, 7 | BRF_GRA },       // 29
	{ "chp1-b-8f_white.8f",	0x0200, 0x1ffd894a, 7 | BRF_GRA },       // 30

	{ "chp1-v-2d.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA },       // 31 Timing PROM

	{ "chp1-c_6p_a.6p",	0x4000, 0x597955ca, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chp1-b-6e_white.6e",	0x0200, 0xddac5f0e, 0 | BRF_OPT },
	{ "chp1-b-6f_white.6f",	0x0200, 0x846c6261, 0 | BRF_OPT },
	{ "chp1-b-7f_white.7f",	0x0200, 0x1682dd30, 0 | BRF_OPT },
	{ "chp1-b-7e_pink.7e",	0x0200, 0xfddaa777, 0 | BRF_OPT },
	{ "chp1-b-8e_pink.8e",	0x0200, 0xc3d5d71f, 0 | BRF_OPT },
	{ "chp1-b-8f_pink.8f",	0x0200, 0xa3037155, 0 | BRF_OPT },
};

STD_ROM_PICK(punchoutj)
STD_ROM_FN(punchoutj)

struct BurnDriver BurnDrvPunchoutj = {
	"punchoutj", "punchout", NULL, NULL, "1984",
	"Punch-Out!! (Japan)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, punchoutjRomInfo, punchoutjRomName, NULL, NULL, PunchoutInputInfo, PunchoutDIPInfo,
	PunchoutaInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Punch-Out!! (Italian bootleg)

static struct BurnRomInfo punchitaRomDesc[] = {
	{ "chp1-c.8l",		0x2000, 0x1d595ce2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chp1-c.8k",		0x2000, 0xc062fa5c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chp1-c.8j",		0x2000, 0x48d453ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chp1-c.8h",		0x2000, 0x67f5aedc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chp1-c.8f",		0x4000, 0x761de4f3, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",		0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chp1-b.4c",		0x2000, 0x9a9ff1d3, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chp1-b.4d",		0x2000, 0x4c23350f, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",		0x2000, 0xc075f831, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",		0x2000, 0xc4cc2b5a, 4 | BRF_GRA },           //  9

	{ "chp1-v.2r",		0x4000, 0xbd1d4b2e, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chp1-v.2t",		0x4000, 0xdd9a688a, 5 | BRF_GRA },           // 11
	{ "chp1-v.2u",		0x2000, 0xda6a3c4b, 5 | BRF_GRA },           // 12
	{ "chp1-v.2v",		0x2000, 0x8c734a67, 5 | BRF_GRA },           // 13
	{ "chp1-v.3r",		0x4000, 0x2e74ad1d, 5 | BRF_GRA },           // 14
	{ "chp1-v.3t",		0x4000, 0x630ba9fb, 5 | BRF_GRA },           // 15
	{ "chp1-v.3u",		0x2000, 0x6440321d, 5 | BRF_GRA },           // 16
	{ "chp1-v.3v",		0x2000, 0xbb7b7198, 5 | BRF_GRA },           // 17
	{ "chp1-v.4r",		0x4000, 0x4e5b0fe9, 5 | BRF_GRA },           // 18
	{ "chp1-v.4t",		0x4000, 0x37ffc940, 5 | BRF_GRA },           // 19
	{ "chp1-v.4u",		0x2000, 0x1a7521d4, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",		0x2000, 0x75be7aae, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",		0x2000, 0xdaf74de0, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",		0x2000, 0x4cb7ea82, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",		0x2000, 0x1c0d09aa, 6 | BRF_GRA },           // 24

	{ "chp1-b-6e_pink.6e",	0x0200, 0xe9ca3ac6, 7 | BRF_GRA },       // 25 Color PROMs
	{ "chp1-b-6f_pink.6f",	0x0200, 0x02be56ab, 7 | BRF_GRA },       // 26
	{ "chp1-b-7f_pink.7f",	0x0200, 0x11de55f1, 7 | BRF_GRA },       // 27
	{ "chp1-b-7e_white.7e",	0x0200, 0x47adf7a2, 7 | BRF_GRA },       // 28
	{ "chp1-b-8e_white.8e",	0x0200, 0xb0fc15a8, 7 | BRF_GRA },       // 29
	{ "chp1-b-8f_white.8f",	0x0200, 0x1ffd894a, 7 | BRF_GRA },       // 30

	{ "chp1-v-2d.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA },       // 31 Timing PROM

	{ "chp1-c.6p",	0x4000, 0xea0bbb31, 8 | BRF_SND },           	 // 32 VLM5030 Samples
	
	{ "chp1-b-6e_white.6e",	0x0200, 0xddac5f0e, 0 | BRF_OPT },
	{ "chp1-b-6f_white.6f",	0x0200, 0x846c6261, 0 | BRF_OPT },
	{ "chp1-b-7f_white.7f",	0x0200, 0x1682dd30, 0 | BRF_OPT },
	{ "chp1-b-7e_pink.7e",	0x0200, 0xfddaa777, 0 | BRF_OPT },
	{ "chp1-b-8e_pink.8e",	0x0200, 0xc3d5d71f, 0 | BRF_OPT },
	{ "chp1-b-8f_pink.8f",	0x0200, 0xa3037155, 0 | BRF_OPT },
};

STD_ROM_PICK(punchita)
STD_ROM_FN(punchita)

static INT32 SpnchoutInit()
{
	return CommonInit(SpunchoutLoadRoms, 1, 0xff00, 0x00000000);
}

struct BurnDriver BurnDrvPunchita = {
	"punchita", "punchout", NULL, NULL, "1984",
	"Punch-Out!! (Italian bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, punchitaRomInfo, punchitaRomName, NULL, NULL, PunchoutInputInfo, PunchoutDIPInfo,
	SpnchoutInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Super Punch-Out!! (Rev B)

static struct BurnRomInfo spnchoutRomDesc[] = {
	{ "chs1-c.8l",		0x2000, 0x703b9780, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chs1-c.8k",		0x2000, 0xe13719f6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chs1-c.8j",		0x2000, 0x1fa629e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chs1-c.8h",		0x2000, 0x15a6c068, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chs1-c.8f",		0x4000, 0x4ff3cdd9, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",		0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chs1-b.4c",		0x2000, 0x9f2ede2d, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chs1-b.4d",		0x2000, 0x143ae5c6, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",		0x2000, 0xc075f831, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",		0x2000, 0xc4cc2b5a, 4 | BRF_GRA },           //  9

	{ "chs1-v.2r",		0x4000, 0xff33405d, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chs1-v.2t",		0x4000, 0xf507818b, 5 | BRF_GRA },           // 11
	{ "chs1-v.2u",		0x4000, 0x0995fc95, 5 | BRF_GRA },           // 12
	{ "chs1-v.2v",		0x2000, 0xf44d9878, 5 | BRF_GRA },           // 13
	{ "chs1-v.3r",		0x4000, 0x09570945, 5 | BRF_GRA },           // 14
	{ "chs1-v.3t",		0x4000, 0x42c6861c, 5 | BRF_GRA },           // 15
	{ "chs1-v.3u",		0x4000, 0xbf5d02dd, 5 | BRF_GRA },           // 16
	{ "chs1-v.3v",		0x2000, 0x5673f4fc, 5 | BRF_GRA },           // 17
	{ "chs1-v.4r",		0x4000, 0x8e155758, 5 | BRF_GRA },           // 18
	{ "chs1-v.4t",		0x4000, 0xb4e43448, 5 | BRF_GRA },           // 19
	{ "chs1-v.4u",		0x4000, 0x74e0d956, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",		0x2000, 0x75be7aae, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",		0x2000, 0xdaf74de0, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",		0x2000, 0x4cb7ea82, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",		0x2000, 0x1c0d09aa, 6 | BRF_GRA },           // 24

	{ "chs1-b-6e_pink.6e",	0x0200, 0x0ad4d727, 7 | BRF_GRA },           // 25 Color PROMs
	{ "chs1-b-6f_pink.6f",	0x0200, 0x86f5cfdb, 7 | BRF_GRA },           // 26
	{ "chs1-b-7f_pink.7f",	0x0200, 0x8bd406f8, 7 | BRF_GRA },           // 27
	{ "chs1-b-7e_white.7e",	0x0200, 0x9e170f64, 7 | BRF_GRA },           // 28
	{ "chs1-b-8e_white.8e",	0x0200, 0x3a2e333b, 7 | BRF_GRA },           // 29
	{ "chs1-b-8f_white.8f",	0x0200, 0x1663eed7, 7 | BRF_GRA },           // 30

	{ "chs1-v.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA | BRF_OPT }, // 31 Timing PROM

	{ "chs1-c.6p",		0x4000, 0xad8b64b8, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chs1-b-6e_white.6e",	0x0200, 0x8efd867f, 0 | BRF_OPT },           //
	{ "chs1-b-6f_white.6f",	0x0200, 0x279d6cbc, 0 | BRF_OPT },           //
	{ "chs1-b-7f_white.7f",	0x0200, 0xcad6b7ad, 0 | BRF_OPT },           //
	{ "chs1-b-7e_pink.7e",	0x0200, 0x4c7e3a67, 0 | BRF_OPT },           //
	{ "chs1-b-8e_pink.8e",	0x0200, 0xec659313, 0 | BRF_OPT },           //
	{ "chs1-b-8f_pink.8f",	0x0200, 0x8b493c09, 0 | BRF_OPT },           //
};

STD_ROM_PICK(spnchout)
STD_ROM_FN(spnchout)

struct BurnDriver BurnDrvSpnchout = {
	"spnchout", NULL, NULL, NULL, "1984",
	"Super Punch-Out!! (Rev B)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, spnchoutRomInfo, spnchoutRomName, NULL, NULL, SpnchoutInputInfo, SpnchoutDIPInfo,
	SpnchoutInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Super Punch-Out!! (Rev A)

static struct BurnRomInfo spnchoutaRomDesc[] = {
	{ "chs1-c.8l",		0x2000, 0x703b9780, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chs1-c.8k",		0x2000, 0xe13719f6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chs1-c.8j",		0x2000, 0x1fa629e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chs1-c.8h",		0x2000, 0x15a6c068, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chs1-c.8f",		0x4000, 0x4ff3cdd9, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",		0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chs1-b.4c",		0x2000, 0xb017e1e9, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "chs1-b.4d",		0x2000, 0xe3de9d18, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",		0x2000, 0x20fb4829, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",		0x2000, 0xedc34594, 4 | BRF_GRA },           //  9

	{ "chs1-v.2r",		0x4000, 0xff33405d, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chs1-v.2t",		0x4000, 0xf507818b, 5 | BRF_GRA },           // 11
	{ "chs1-v.2u",		0x4000, 0x0995fc95, 5 | BRF_GRA },           // 12
	{ "chs1-v.2v",		0x2000, 0xf44d9878, 5 | BRF_GRA },           // 13
	{ "chs1-v.3r",		0x4000, 0x09570945, 5 | BRF_GRA },           // 14
	{ "chs1-v.3t",		0x4000, 0x42c6861c, 5 | BRF_GRA },           // 15
	{ "chs1-v.3u",		0x4000, 0xbf5d02dd, 5 | BRF_GRA },           // 16
	{ "chs1-v.3v",		0x2000, 0x5673f4fc, 5 | BRF_GRA },           // 17
	{ "chs1-v.4r",		0x4000, 0x8e155758, 5 | BRF_GRA },           // 18
	{ "chs1-v.4t",		0x4000, 0xb4e43448, 5 | BRF_GRA },           // 19
	{ "chs1-v.4u",		0x4000, 0x74e0d956, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",		0x2000, 0x16588f7a, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",		0x2000, 0xdc743674, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",		0x2000, 0xc2db5b4e, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",		0x2000, 0xe6af390e, 6 | BRF_GRA },           // 24

	{ "chs1-b-6e_pink.6e",	0x0200, 0x0ad4d727, 7 | BRF_GRA },           // 25 Color PROMs
	{ "chs1-b-6f_pink.6f",	0x0200, 0x86f5cfdb, 7 | BRF_GRA },           // 26
	{ "chs1-b-7f_pink.7f",	0x0200, 0x8bd406f8, 7 | BRF_GRA },           // 27
	{ "chs1-b-7e_white.7e",	0x0200, 0x9e170f64, 7 | BRF_GRA },           // 28
	{ "chs1-b-8e_white.8e",	0x0200, 0x3a2e333b, 7 | BRF_GRA },           // 29
	{ "chs1-b-8f_white.8f",	0x0200, 0x1663eed7, 7 | BRF_GRA },           // 30

	{ "chs1-v.2d",		0x0100, 0x71dc0d48, 0 | BRF_GRA | BRF_OPT }, // 31 Timing PROM

	{ "chs1-c.6p",		0x4000, 0xad8b64b8, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chs1-b-6e_white.6e",	0x0200, 0x8efd867f, 0 | BRF_OPT },           //
	{ "chs1-b-6f_white.6f",	0x0200, 0x279d6cbc, 0 | BRF_OPT },           //
	{ "chs1-b-7f_white.7f",	0x0200, 0xcad6b7ad, 0 | BRF_OPT },           //
	{ "chs1-b-7e_pink.7e",	0x0200, 0x4c7e3a67, 0 | BRF_OPT },           //
	{ "chs1-b-8e_pink.8e",	0x0200, 0xec659313, 0 | BRF_OPT },           //
	{ "chs1-b-8f_pink.8f",	0x0200, 0x8b493c09, 0 | BRF_OPT },           //
};

STD_ROM_PICK(spnchouta)
STD_ROM_FN(spnchouta)

struct BurnDriver BurnDrvSpnchouta = {
	"spnchouta", "spnchout", NULL, NULL, "1984",
	"Super Punch-Out!! (Rev A)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, spnchoutaRomInfo, spnchoutaRomName, NULL, NULL, SpnchoutInputInfo, SpnchoutDIPInfo,
	PunchoutaInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Super Punch-Out!! (Japan)

static struct BurnRomInfo spnchoutjRomDesc[] = {
	{ "chs1c8la.bin",	0x2000, 0xdc2a592b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chs1c8ka.bin",	0x2000, 0xce687182, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chs1-c.8j",		0x2000, 0x1fa629e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chs1-c.8h",		0x2000, 0x15a6c068, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chs1c8fa.bin",	0x4000, 0xf745b5d5, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",		0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "b_4c_01a.bin",	0x2000, 0xb017e1e9, 3 | BRF_GRA },           //  6 Top Screen Background Characters
	{ "b_4d_01a.bin",	0x2000, 0xe3de9d18, 3 | BRF_GRA },           //  7

	{ "chp1-b.4a",		0x2000, 0xc075f831, 4 | BRF_GRA },           //  8 Bottom Screen Background Characters
	{ "chp1-b.4b",		0x2000, 0xc4cc2b5a, 4 | BRF_GRA },           //  9

	{ "chs1-v.2r",		0x4000, 0xff33405d, 5 | BRF_GRA },           // 10 Big Sprite Characters
	{ "chs1-v.2t",		0x4000, 0xf507818b, 5 | BRF_GRA },           // 11
	{ "chs1-v.2u",		0x4000, 0x0995fc95, 5 | BRF_GRA },           // 12
	{ "chs1-v.2v",		0x2000, 0xf44d9878, 5 | BRF_GRA },           // 13
	{ "chs1-v.3r",		0x4000, 0x09570945, 5 | BRF_GRA },           // 14
	{ "chs1-v.3t",		0x4000, 0x42c6861c, 5 | BRF_GRA },           // 15
	{ "chs1-v.3u",		0x4000, 0xbf5d02dd, 5 | BRF_GRA },           // 16
	{ "chs1-v.3v",		0x2000, 0x5673f4fc, 5 | BRF_GRA },           // 17
	{ "chs1-v.4r",		0x4000, 0x8e155758, 5 | BRF_GRA },           // 18
	{ "chs1-v.4t",		0x4000, 0xb4e43448, 5 | BRF_GRA },           // 19
	{ "chs1-v.4u",		0x4000, 0x74e0d956, 5 | BRF_GRA },           // 20

	{ "chp1-v.6p",		0x2000, 0x75be7aae, 6 | BRF_GRA },           // 21 Bottom Screen Big Sprite Characters
	{ "chp1-v.6n",		0x2000, 0xdaf74de0, 6 | BRF_GRA },           // 22
	{ "chp1-v.8p",		0x2000, 0x4cb7ea82, 6 | BRF_GRA },           // 23
	{ "chp1-v.8n",		0x2000, 0x1c0d09aa, 6 | BRF_GRA },           // 24

	{ "chs1-b-6e_white.6e",	0x0200, 0x8efd867f, 0 | BRF_GRA },       // 25 Color PROMS
	{ "chs1-b-6f_white.6f",	0x0200, 0x279d6cbc, 0 | BRF_GRA },       // 26
	{ "chs1-b-7f_white.7f",	0x0200, 0xcad6b7ad, 0 | BRF_GRA },       // 27
	{ "chs1-b-7e_white.7e",	0x0200, 0x9e170f64, 7 | BRF_GRA },       // 28     
	{ "chs1-b-8e_white.8e",	0x0200, 0x3a2e333b, 7 | BRF_GRA },       // 29    
	{ "chs1-b-8f_white.8f",	0x0200, 0x1663eed7, 7 | BRF_GRA },		 // 30
	
	{ "chs1-v.2d",		0x0100, 0x71dc0d48, 0 | BRF_OPT },           // 31 Timing PROM

	{ "chs1c6pa.bin",	0x4000, 0xd05fb730, 8 | BRF_SND },           // 32 VLM5030 Samples
	
	{ "chs1-b-6e_pink.6e",	0x0200, 0x0ad4d727, 7 | BRF_OPT },           
	{ "chs1-b-6f_pink.6f",	0x0200, 0x86f5cfdb, 7 | BRF_OPT },            
	{ "chs1-b-7f_pink.7f",	0x0200, 0x8bd406f8, 7 | BRF_OPT }, 
	{ "chs1-b-7e_pink.7e",	0x0200, 0x4c7e3a67, 0 | BRF_OPT },       
	{ "chs1-b-8e_pink.8e",	0x0200, 0xec659313, 0 | BRF_OPT },       
	{ "chs1-b-8f_pink.8f",	0x0200, 0x8b493c09, 0 | BRF_OPT },       	
};

STD_ROM_PICK(spnchoutj)
STD_ROM_FN(spnchoutj)

static INT32 SpnchoutjInit()
{
	return CommonInit(SpunchoutjLoadRoms, 1, 0xffff, 0x000000ff);
}

struct BurnDriver BurnDrvSpnchoutj = {
	"spnchoutj", "spnchout", NULL, NULL, "1984",
	"Super Punch-Out!! (Japan)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, spnchoutjRomInfo, spnchoutjRomName, NULL, NULL, SpnchoutInputInfo, SpnchoutDIPInfo,
	SpnchoutjInit, DrvExit, DrvFrame, PunchoutDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};


// Arm Wrestling

static struct BurnRomInfo armwrestRomDesc[] = {
	{ "chv1-c.8l",		0x2000, 0xb09764c1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "chv1-c.8k",		0x2000, 0x0e147ff7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chv1-c.8j",		0x2000, 0xe7365289, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chv1-c.8h",		0x2000, 0xa2118eec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chpv-c.8f",		0x4000, 0x664a07c4, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "chp1-c.4k",		0x2000, 0xcb6ef376, 2 | BRF_PRG | BRF_ESS }, //  5 N2A03 Code

	{ "chpv-b.2e",		0x4000, 0x8b45f365, 3 | BRF_GRA },           //  6 Background Characters
	{ "chpv-b.2d",		0x4000, 0xb1a2850c, 3 | BRF_GRA },           //  7

	{ "chpv-b.2m",		0x4000, 0x19245b37, 4 | BRF_GRA },           //  8 Bottom Screen Foreground Characters
	{ "chpv-b.2l",		0x4000, 0x46797941, 4 | BRF_GRA },           //  9
	{ "chpv-b.2k",		0x2000, 0xde189b00, 4 | BRF_GRA },           // 10

	{ "chv1-v.2r",		0x4000, 0xd86056d9, 5 | BRF_GRA },           // 11 Big Sprite Characters
	{ "chv1-v.2t",		0x4000, 0x5ad77059, 5 | BRF_GRA },           // 12
	{ "chv1-v.2v",		0x4000, 0xa0fd7338, 5 | BRF_GRA },           // 13
	{ "chv1-v.3r",		0x4000, 0x690e26fb, 5 | BRF_GRA },           // 14
	{ "chv1-v.3t",		0x4000, 0xea5d7759, 5 | BRF_GRA },           // 15
	{ "chv1-v.3v",		0x4000, 0xceb37c05, 5 | BRF_GRA },           // 16
	{ "chv1-v.4r",		0x4000, 0xe291cba0, 5 | BRF_GRA },           // 17
	{ "chv1-v.4t",		0x4000, 0xe01f3b59, 5 | BRF_GRA },           // 18

	{ "chv1-v.6p",		0x2000, 0xd834e142, 6 | BRF_GRA },           // 19 Bottom Screen Big Sprite Characters
	{ "chv1-v.8p",		0x2000, 0xa2f531db, 6 | BRF_GRA },           // 20

	{ "chpv-b.7b",		0x0200, 0xdf6fdeb3, 7 | BRF_GRA },           // 21 Color PROMs
	{ "chpv-b.7c",		0x0200, 0xb1da5f42, 7 | BRF_GRA },           // 22
	{ "chpv-b.7d",		0x0200, 0x4ede813e, 7 | BRF_GRA },           // 23
	{ "chpv-b.4b",		0x0200, 0x9d51416e, 7 | BRF_GRA },           // 24
	{ "chpv-b.4c",		0x0200, 0xb8a25795, 7 | BRF_GRA },           // 25
	{ "chpv-b.4d",		0x0200, 0x474fc3b1, 7 | BRF_GRA },           // 26

	{ "chv1-b.3c",		0x0100, 0xc3f92ea2, 0 | BRF_OPT },           // 27 Priority PROM
	{ "chpv-v.2d",		0x0100, 0x71dc0d48, 0 | BRF_OPT },           // 28 Timing PROM

	{ "chv1-c.6p",		0x4000, 0x31b52896, 8 | BRF_SND },           // 29 VLM5030 Samples
};

STD_ROM_PICK(armwrest)
STD_ROM_FN(armwrest)

static INT32 ArmwrestInit()
{
	return CommonInit(ArmwrestLoadRoms, 0, 0x0000, 0xff000000);
}

struct BurnDriver BurnDrvArmwrest = {
	"armwrest", NULL, NULL, NULL, "1985",
	"Arm Wrestling\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, armwrestRomInfo, armwrestRomName, NULL, NULL, ArmwrestInputInfo, ArmwrestDIPInfo,
	ArmwrestInit, DrvExit, DrvFrame, ArmwrestDraw, DrvScan, &DrvRecalc, 0x400,
	256, 448, 4, 6
};
