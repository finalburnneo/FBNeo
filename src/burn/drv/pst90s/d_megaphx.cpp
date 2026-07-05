// FinalBurn Neo Mega Phoenix driver module
// Based on MAME driver by David Haywood, Dirk Best

// note: tms overclocked by 1mhz (8mhz / 8) to mitigate flickering (megaphx)

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "z80_intf.h"
#include "tms34_intf.h"
#include "dac.h"
#include "i8255.h"
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvPicROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;

// vid
static UINT8 *DrvTMSRAM;
static UINT8 *DrvVidRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *palette[3];

static UINT8 pal_index[2];
static UINT8 int_index[2];
static UINT8 inder_4bpp;
static INT32 shiftfull;
static INT32 InterleaveFromTMS;
// end vid?

static UINT8 ppi_to_pic_data;
static UINT8 ppi_to_pic_command;
static UINT8 ppi_to_pic_clock;
static UINT8 pic_to_ppi_clock;
static UINT8 pic_to_ppi_data;
static UINT8 dsw_data;
static UINT8 soundsent;
static UINT8 sounddata;
static UINT8 soundback;
static UINT8 soundbank[4];
static UINT8 soundbankselect;
static UINT8 dacvol[4];

static dtimer ls166_timer[2];

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];

static INT32 nCyclesExtra[4];

static struct BurnInputInfo MegaphxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Megaphx)

static struct BurnDIPInfo MegaphxDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x9b, NULL					},
	{0x01, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x07, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x06, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x05, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x04, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x03, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x07, 0x02, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x07, 0x01, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x07, 0x00, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x38, 0x38, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x30, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x28, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x20, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x18, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x38, 0x10, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x38, 0x08, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x38, 0x00, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0xc0, 0x00, "2"					},
	{0x00, 0x01, 0xc0, 0x40, "3"					},
	{0x00, 0x01, 0xc0, 0x80, "4"					},
	{0x00, 0x01, 0xc0, 0xc0, "5"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x02, 0x00, "Off"					},
	{0x01, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x1c, 0x00, "0"					},
	{0x01, 0x01, 0x1c, 0x04, "1"					},
	{0x01, 0x01, 0x1c, 0x08, "2"					},
	{0x01, 0x01, 0x1c, 0x0c, "3"					},
	{0x01, 0x01, 0x1c, 0x10, "4"					},
	{0x01, 0x01, 0x1c, 0x14, "5"					},
	{0x01, 0x01, 0x1c, 0x18, "6"					},
	{0x01, 0x01, 0x1c, 0x1c, "7"					},
};

STDDIPINFO(Megaphx)

static struct BurnDIPInfo HamboyDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xb6, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x00, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x03, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x05, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x01, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x06, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x07, 0x02, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x07, 0x07, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x38, 0x00, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x18, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x28, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x08, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x30, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x38, 0x10, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x38, 0x20, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x38, 0x38, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0xc0, 0x00, "1"					},
	{0x00, 0x01, 0xc0, 0x40, "2"					},
	{0x00, 0x01, 0xc0, 0x80, "3"					},
	{0x00, 0x01, 0xc0, 0xc0, "4"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x02, 0x00, "Off"					},
	{0x01, 0x01, 0x02, 0x02, "On"					},
};

STDDIPINFO(Hamboy)

static struct BurnDIPInfo YoyospelDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x9b, NULL					},
	{0x01, 0xff, 0xff, 0x80, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x07, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x06, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x05, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x04, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x07, 0x03, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x07, 0x02, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x07, 0x01, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x07, 0x00, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x38, 0x38, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x30, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x28, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x20, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x38, 0x18, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x38, 0x10, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x38, 0x08, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x38, 0x00, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0xc0, 0x00, "2"					},
	{0x00, 0x01, 0xc0, 0x40, "3"					},
	{0x00, 0x01, 0xc0, 0x80, "4"					},
	{0x00, 0x01, 0xc0, 0xc0, "5"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x02, 0x00, "Off"					},
	{0x01, 0x01, 0x02, 0x02, "On"					},
};

STDDIPINFO(Yoyospel)

static void sync_pic()
{
	INT32 cyc = (((INT64)SekTotalCycles(0) * (6000000 / 4)) / 8000000) - pic16c5xTotalCycles();
	if (cyc > 0) {
		pic16c5xRun(cyc);
	}
}

static void sync_snd()
{
	INT32 cyc = SekTotalCycles() - ZetTotalCycles();
	if (cyc > 0) {
		ZetRun(cyc);
	}
}

//-----------------------------------------------------------------------------------------
// make device

#define TTL166_CHIPS	2

typedef UINT8 (*ttl166_in_data_function)();
typedef void (*ttl166_out_data_function)(UINT8 data);
static UINT8 ttl166_ser[TTL166_CHIPS];
static UINT8 ttl166_shld[TTL166_CHIPS];
static UINT8 ttl166_data[TTL166_CHIPS];
static UINT8 ttl166_clk[TTL166_CHIPS];
static ttl166_in_data_function ttl166_in_data[TTL166_CHIPS] = { NULL, NULL };
static ttl166_out_data_function ttl166_out_data[TTL166_CHIPS] = { NULL, NULL };

static void ls166_timer_cb(int param)
{
	UINT8 chip = param & 0xff;
	UINT8 timerbyte = (param >> 8) & 0x01;

	if (ttl166_out_data[chip]) ttl166_out_data[chip](timerbyte);
}

void ttl166_serial(int chip, int state)
{
	if (chip >= TTL166_CHIPS || chip < 0) return; // fail

	ttl166_ser[chip] = state ? 1 : 0;
}

void ttl166_clock(int chip, int state)
{
	if (chip >= TTL166_CHIPS || chip < 0) return; // fail

	if (ttl166_clk[chip] == 0 && state != 0)
	{
		if (ttl166_shld[chip]) {
			ttl166_data[chip] = (ttl166_data[chip] << 1) | (ttl166_ser[chip] & 1);
		} else {
			ttl166_data[chip] = (ttl166_in_data[chip] == NULL) ? 0 : ttl166_in_data[chip]();
		}

		ls166_timer[chip].start(nsec_to_cycles(6000000/4, 25), chip | ((ttl166_data[chip] >> 7)<<8), 1, 0);
	}

	ttl166_clk[chip] = state ? 1 : 0;
}

void ttl166_load(int chip, int state)
{
	if (chip >= TTL166_CHIPS || chip < 0) return; // fail

	ttl166_shld[chip] = state ? 1 : 0;
}

void ttl166_reset()
{
	memset (ttl166_ser, 0, sizeof(ttl166_ser));
	memset (ttl166_shld, 0, sizeof(ttl166_shld));
	memset (ttl166_data, 0, sizeof(ttl166_data));
	memset (ttl166_clk, 0, sizeof(ttl166_clk));
}

void ttl166_init(INT32 chip, UINT8 (*in)(), void (*out)(UINT8 data))
{
	if (chip >= TTL166_CHIPS || chip < 0) return; // fail

	ttl166_in_data[chip] = in;
	ttl166_out_data[chip] = out;

	ttl166_reset();
}

void ttl166_scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(ttl166_ser);
		SCAN_VAR(ttl166_shld);
		SCAN_VAR(ttl166_data);
		SCAN_VAR(ttl166_clk);
		ls166_timer[0].scan();
		ls166_timer[1].scan();
	}
}


//-----------------------------------------------------------------------------------------

static void __fastcall megaphx_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x40000:
		case 0x40002:
		case 0x40004:
		case 0x40006:
			TMS34010HostWrite((address / 2) & 3, data);
		return;

		case 0x50000:
			sync_snd();
			soundsent = 0xff;
			sounddata = data;
		return;

		case 0x60000:
		case 0x60002:
		case 0x60004:
		case 0x60006:
			i8255_write((address >> 1) & 3, data & 0xff);
		return;
	}

	bprintf(0, _T("m.ww %x   %x\n"), address, data);
}

static void __fastcall megaphx_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x40000:
		case 0x40001:
		case 0x40002:
		case 0x40003:
		case 0x40004:
		case 0x40005:
		case 0x40006:
		case 0x40007:
			TMS34010HostWriteMask((address / 2) & 3, data << ((~address&1) << 3), 0xff << ((~address&1) << 3));
		return;

		case 0x50001:
			sync_snd();
			soundsent = 0xff;
			sounddata = data;
		return;

		case 0x60001:
		case 0x60003:
		case 0x60005:
		case 0x60007:
			i8255_write((address >> 1) & 3, data);
		return;
	}

	bprintf(0, _T("m.wb %x   %x\n"), address, data);
}

static UINT16 __fastcall megaphx_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x40000:
		case 0x40002:
		case 0x40004:
		case 0x40006:
			return TMS34010HostRead((address / 2) & 3);

		case 0x50002:
			sync_snd();
			return soundback;

		case 0x60000:
		case 0x60002:
		case 0x60004:
		case 0x60006:
			return i8255_read((address >> 1) & 3);
	}

	bprintf(0, _T("m.rw %x\n"), address);
	return 0;
}

static UINT8 __fastcall megaphx_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x40000:
		case 0x40001:
		case 0x40002:
		case 0x40003:
		case 0x40004:
		case 0x40005:
		case 0x40006:
		case 0x40007:
			return TMS34010HostRead((address / 2) & 3) >> ((~address & 1) << 3);

		case 0x50003:
			sync_snd();
			return soundback;

		case 0x60001:
		case 0x60003:
		case 0x60005:
		case 0x60007:
			return i8255_read((address >> 1) & 3);
	}

	bprintf(0, _T("m.rb %x\n"), address);
	return 0;
}

static UINT8 megaphx_pic_readport(UINT16 port)
{
	switch (port)
	{
		case 0: // Port A
			ppi_to_pic_command = 0;
			return (ppi_to_pic_data << 0) | (ppi_to_pic_clock << 3);

		case 1: // Port B
			return (ppi_to_pic_command) | (dsw_data << 1) | ((DrvInputs[2] << 4) & 0x30);
	}
	bprintf(0, _T("pic_rp  %x\n"), port);
	return 0;
}

static void megaphx_pic_writeport(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0: // Port A
			pic_to_ppi_data = (data >> 0) & 1;
			pic_to_ppi_clock = (data >> 2) & 1;
		return;

		case 1: // Port B
			ttl166_load(0, (data & 0x04) >> 2);
			ttl166_load(1, (data & 0x04) >> 2);
			ttl166_clock(0, (data & 0x08) >> 3);
			ttl166_clock(1, (data & 0x08) >> 3);
			return;
	}
	bprintf(0, _T("pic_wp  %x   %x\n"), port, data);
}

static void bankswitch(INT32 data)
{
	soundbankselect = soundbank[data & 3] & 7;

	ZetMapMemory(DrvSndROM + soundbankselect * 0x8000, 0x8000, 0xffff, MAP_ROM);
}

static UINT8 __fastcall inder_sb_read(UINT16 address)
{
	if (address < 0x2000) {
		if (address >= 0x20 && address <= 0x26)	{
			bankswitch((address >> 1) & 3);
		}

		return DrvZ80ROM[address];
	}

	return 0;
}

static void __fastcall inder_sb_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x02:
		case 0x04:
		case 0x06: {
			INT16 sam = (INT8)data * dacvol[(port&6)>>1];
			DACWrite16((port&6)>>1, sam);
		}
		return;

		case 0x01:
		case 0x03:
		case 0x05:
		case 0x07:
			dacvol[(port&6)>>1] = data;
		return;

		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			soundbank[port & 3] = data;
		return;

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			z80ctc_write(port & 3, data);
		return;

		case 0x30:
			soundback = data;
		return;
	}
}

static UINT8 __fastcall inder_sb_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			return z80ctc_read(port & 3);

		case 0x30: {
			return sounddata;
		}
		case 0x31:
		{
			UINT8 ret = soundsent;
			soundsent = 0;
			return ret;
		}
	}

	return 0;
}

static void ctc_intr(INT32 state)
{
	ZetSetIRQLine(0, state);
}

static UINT8 read_dip1()
{
	return DrvDips[0];
}

static UINT8 read_dip2()
{
	return DrvDips[1];
}

static void shift_write(UINT8 data)
{
	ttl166_serial(1, data & 1);
}

static void dsw_write(UINT8 data)
{
	dsw_data = data;
}

static UINT8 ppi_read_port_A()
{
	return DrvInputs[0];
}

static UINT8 ppi_read_port_B()
{
	return DrvInputs[1];
}

static UINT8 ppi_read_port_C()
{
	sync_pic();
	return (pic_to_ppi_data << 1) | (pic_to_ppi_clock << 3);
}

static void ppi_write_port_C(UINT8 data)
{
	sync_pic();
	if (data & 0x10) ppi_to_pic_command = 1;
	ppi_to_pic_data = (data & 0x40) >> 6;
	ppi_to_pic_clock = (data & 0x80) >> 7;
}

static UINT8 ppi_dummy_read_lo()
{
	return 0x00;
}

static void reg_increment(INT32 inc_type)
{
	int_index[inc_type]++;
	if (int_index[inc_type] == 3)
	{
		int_index[inc_type] = 0;
		pal_index[inc_type]++;
	}
}

static void indervid_sub_write(UINT32 address, UINT16 data)
{
	switch (address & ~0xf)
	{
		case 0x4000000:
			pal_index[0] = data;
			int_index[0] = 0;
		return;

		case 0x4000010:
			palette[int_index[0] & 3][pal_index[0]] = data;
			reg_increment(0);
		return;

		case 0x4000030:
			pal_index[1] = data;
			int_index[1] = 0;
		return;
	}
}

static UINT16 indervid_sub_read(UINT32 address)
{
	switch (address & ~0xf)
	{
		case 0x4000010:
		{
			UINT8 ret;
			ret = palette[int_index[1] & 3][pal_index[1]];
			reg_increment(1);
			return ret;
		}
	}

	return 0xffff;
}

static void tms_to_shift(UINT32 address, UINT16 *dst)
{
	if (shiftfull == 0)
	{
		UINT16 *ram = (UINT16*)DrvVidRAM;
		memcpy (dst, &ram[(address & ~0x1fff) >> 4], 0x400);
		shiftfull = 1;
	}
}

static void tms_from_shift(UINT32 address, UINT16 *src)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	memcpy (&ram[(address & ~0x1fff) >> 4], src, 0x400);

	shiftfull = 0;
}

static INT32 scanline_cb(INT32 s_scanline, TMS34010Display *params)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	UINT16 *vram = &ram[(params->rowaddr << 8) & 0x3ff00];
	s_scanline -= params->veblnk; // top clip
	UINT16 *dst = pTransDraw + s_scanline * nScreenWidth;

	bool display_params = false;
	if (InterleaveFromTMS != params->vtotal && params->vtotal != 0) {
		bprintf(0, _T("TMS34010 - vertical resolution changed to: %d  (was: %d)\n"),params->vtotal, InterleaveFromTMS);
		InterleaveFromTMS = params->vtotal;
		display_params = true;
	}

#if 1
	if (display_params) {
		bprintf(0, _T("video ENAB %d\n"), params->enabled);
		bprintf(0, _T("htotal %d  (not always correct)\n"), params->htotal);
		bprintf(0, _T("he %d\n"), params->heblnk);
		bprintf(0, _T("hs %d\n"), params->hsblnk);
		bprintf(0, _T("calculated htotal (hs-he) %d\n"), params->hsblnk - params->heblnk);

		bprintf(0, _T("vtotal %d  (not always correct)\n"), params->vtotal);
		bprintf(0, _T("ve %d\n"), params->veblnk);
		bprintf(0, _T("vs %d\n"), params->vsblnk);
		bprintf(0, _T("calculated vtotal (vs-ve) %d\n"), params->vsblnk - params->veblnk);
	}
#endif

	if (s_scanline < 0 || s_scanline >= nScreenHeight) return 0;

	INT32 coladdr = params->coladdr;

	if (inder_4bpp) {
		for (INT32 x = params->heblnk; x < params->hsblnk; x += 4)
		{
			UINT16 pixels = vram[coladdr++ & 0xff];
			INT32 dx = x - params->heblnk; // side clip

			if (dx < 0 || (dx+3) >= nScreenWidth) continue;

			dst[dx + 3] = (pixels & 0xf000) >> 12;
			dst[dx + 2] = (pixels & 0x0f00) >> 8;
			dst[dx + 1] = (pixels & 0x00f0) >> 4;
			dst[dx + 0] = (pixels & 0x000f) >> 0;
		}
	} else {
		for (INT32 x = params->heblnk; x < params->hsblnk; x += 2)
		{
			UINT16 pixels = vram[coladdr++ & 0xff];
			INT32 dx = x - params->heblnk; // side clip

			if (dx < 0 || (dx+1) >= nScreenWidth) continue;

			dst[dx + 0] = pixels & 0xff;
			dst[dx + 1] = pixels >> 8;
		}
	}
	return 0;
}

static void m68k_gen_int(INT32 state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam,   0, RamEnd - AllRam);

	memcpy (Drv68KRAM, Drv68KROM + 0x100000, 0x80); // vectors

	SekOpen(0);
	SekReset();
	SekClose();

	memset(Drv68KRAM, 0x00, 0x80);

	pic16c5xReset();

	ZetOpen(0);
	ZetReset();
	ZetClose();

//	indervid_reset(); // tms
	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();
	InterleaveFromTMS = 262;

	memset (pal_index, 0, sizeof(pal_index));
	memset (int_index, 0, sizeof(int_index));
// vid reset end

	DACReset();

	ttl166_reset();
	i8255_reset();

	ppi_to_pic_data = 0;
	ppi_to_pic_command = 0;
	ppi_to_pic_clock = 0;
	pic_to_ppi_clock = 0;
	pic_to_ppi_data = 0;
	dsw_data = 0;
	soundsent = 0;
	sounddata = 0;
	soundback = 0;
	soundbank[0] = 0;
	soundbank[1] = 0;
	soundbank[2] = 0;
	soundbank[3] = 0;
	soundbankselect = 0;
	memset(dacvol, 0, sizeof(dacvol));

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;
	Drv68KROM			= Next; Next += 0x140000;

	DrvPicROM			= Next; Next += 0x000800;
	DrvZ80ROM			= Next; Next += 0x002000;

	DrvSndROM			= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x010000;
	DrvZ80RAM			= Next; Next += 0x004000;

	// vid
	DrvTMSRAM			= Next; Next += 0x080000;
	DrvVidRAM			= Next; Next += 0x080000;

	palette[0]			= Next; Next += 0x000100;
	palette[1]			= Next; Next += 0x000100;
	palette[2]			= Next; Next += 0x000100;
	// !vid

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvLoad()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[8] = { 0, Drv68KROM + 0x100000, Drv68KROM, DrvSndROM, DrvZ80ROM, DrvPicROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) >= 1 && (ri.nType & 7) <= 2)
		{
			if (BurnLoadRom(pLoad[ri.nType & 7] + 0, i + 0, 2)) return 1;
			if (BurnLoadRom(pLoad[ri.nType & 7] + 1, i + 1, 2)) return 1;
			pLoad[ri.nType & 7] += ri.nLen * 2;
			i++;
			continue;
		}
		
		if ((ri.nType & 7) >= 3)
		{
			if (BurnLoadRom(pLoad[ri.nType & 7], i + 0, 1)) return 1;
			pLoad[ri.nType & 7] += ri.nLen;
			continue;
		}
	}

	return 0;
}

static INT32 MegaphxInit()
{
	BurnSetRefreshRate(52.63); // tms sets this on the first mode change of hamboy
	BurnAllocMemIndex();

	if (DrvLoad()) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRAM,				0x000000, 0x00ffff, MAP_RAM);
	SekMapMemory(Drv68KROM,				0x800000, 0x8fffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x100000,	0xfc0000, 0xffffff, MAP_ROM); // boot
	SekSetWriteWordHandler(0,			megaphx_main_write_word);
	SekSetWriteByteHandler(0,			megaphx_main_write_byte);
	SekSetReadWordHandler(0,			megaphx_main_read_word);
	SekSetReadByteHandler(0,			megaphx_main_read_byte);
	SekClose();

	DrvPicROM[0x2c] = 1;
	pic16c5xInit(0, 0x16c54, DrvPicROM);
	pic16c5xSetReadPortHandler(megaphx_pic_readport);
	pic16c5xSetWritePortHandler(megaphx_pic_writeport);

	// timer for ls166, synced to pic16c5x
	timerInit();
	timerAdd(ls166_timer[0], 0, ls166_timer_cb);
	timerAdd(ls166_timer[1], 0, ls166_timer_cb);
	pic16c5xSetCallback(timerRun);

	ZetInit(0);
	ZetOpen(0);
    ZetDaisyInit(Z80_CTC, 0);
	z80ctc_init(4000000, 8000000, 0, ctc_intr, NULL, NULL, NULL);
//	ZetMapMemory(DrvZ80ROM,			0x0000, 0x00ff, MAP_ROM); // in handler
	ZetMapMemory(DrvZ80ROM + 0x100,	0x0100, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x7fff, MAP_RAM);
	ZetSetReadHandler(inder_sb_read);
	ZetSetOutHandler(inder_sb_write_port);
	ZetSetInHandler(inder_sb_read_port);
	ZetClose();

	inder_4bpp = !strncmp(BurnDrvGetTextA(DRV_NAME), "hamboy", 6);
	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010MapMemory(DrvVidRAM,	0x00000000, 0x003fffff, MAP_RAM);
	TMS34010MapMemory(DrvTMSRAM,	0x7fc00000, 0x7fffffff, MAP_RAM);
	TMS34010MapMemory(DrvTMSRAM,	0xffc00000, 0xffffffff, MAP_RAM); // mirror
	TMS34010SetHandlers(0,			indervid_sub_read, indervid_sub_write);
	TMS34010SetScanlineRender(scanline_cb);
	TMS34010SetToShift(tms_to_shift);
	TMS34010SetFromShift(tms_from_shift);
	TMS34010SetOutputINT(m68k_gen_int);
	TMS34010SetPixClock(48000000 / 12, 2); // slightly overclocked to get rid of flickering
	TMS34010SetCpuCyclesPerFrame((INT32)(48000000 / 8 / 52.63));
	TMS34010SetHaltOnReset(1);
	TMS34010Close();
// !vid init

	ttl166_init(0, read_dip1, shift_write);
	ttl166_init(1, read_dip2, dsw_write);

	i8255_init();
	i8255_set_ports_read(ppi_read_port_A, ppi_read_port_B, ppi_read_port_C);
	i8255_set_ports_write(NULL, NULL, ppi_write_port_C);
	i8255_set_ports_tri_read(NULL, NULL, ppi_dummy_read_lo); // tri_c needs to return 0

	DACInit(0, 0, 0, ZetTotalCycles, 8000000);
	DACInit(1, 0, 0, ZetTotalCycles, 8000000);
	DACInit(2, 0, 0, ZetTotalCycles, 8000000);
	DACInit(3, 0, 0, ZetTotalCycles, 8000000);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);
	DACSetRoute(2, 0.25, BURN_SND_ROUTE_BOTH);
	DACSetRoute(3, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	pic16c5xExit();
	timerExit();
	ZetExit();

//	indervid_exit();
	TMS34010Exit();

	DACExit();
	i8255_exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i] = BurnHighCol(palette[0][i], palette[1][i], palette[2][i], 0);
	}
}

static INT32 IndervidDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	pic16c5xNewFrame();
	TMS34010NewFrame();
	ZetNewFrame();
	timerNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	const double hz = 52.63;
	INT32 nInterleave = InterleaveFromTMS; // 68k, pic16c, tms34010, z80
	INT32 nCyclesTotal[4] = { 8000000 / hz, 6000000 / 4 / hz, 48000000 / 8 / hz, 8000000 / hz };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	SekOpen(0);
    SekIdle(nCyclesExtra[0]);
    pic16c5xIdle(nCyclesExtra[1]);
	TMS34010Open(0);
    TMS34010Idle(nCyclesExtra[2]);
	ZetOpen(0);
    ZetIdle(nCyclesExtra[3]);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN_SYNCINT(0, Sek);
		CPU_RUN_SYNCINT(1, pic16c5x);
		CPU_RUN_SYNCINT(2, TMS34010);
		TMS34010GenerateScanline(i);
		CPU_RUN_SYNCINT(3, Zet);
	}

	nCyclesExtra[0] = SekTotalCycles() - nCyclesTotal[0];
	nCyclesExtra[1] = pic16c5xTotalCycles() - nCyclesTotal[1];
	nCyclesExtra[2] = TMS34010TotalCycles() - nCyclesTotal[2];
	nCyclesExtra[3] = ZetTotalCycles() - nCyclesTotal[3];

	//bprintf(0, _T("cpu cyc:  main / z80: %d   %d  pic  %d  tms   %d\n"), SekTotalCycles(), ZetTotalCycles(),  pic16c5xTotalCycles(), TMS34010TotalCyclesi32());
	ZetClose();
	TMS34010Close();
	SekClose();

	if (pBurnSoundOut) {
		BurnSoundClear();
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029697;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);
		pic16c5xScan(nAction);

	//	indervid_scan(nAction, pnMin);
		TMS34010Scan(nAction);

		SCAN_VAR(pal_index);
		SCAN_VAR(int_index);

		SCAN_VAR(shiftfull);
	// !vid

		DACScan(nAction, pnMin);

		i8255_scan();
		ttl166_scan(nAction);
		timerScan();

		SCAN_VAR(ppi_to_pic_data);
		SCAN_VAR(ppi_to_pic_command);
		SCAN_VAR(ppi_to_pic_clock);
		SCAN_VAR(pic_to_ppi_clock);
		SCAN_VAR(pic_to_ppi_data);

		SCAN_VAR(dsw_data);

		SCAN_VAR(soundsent);
		SCAN_VAR(sounddata);
		SCAN_VAR(soundback);
		SCAN_VAR(soundbank);
		SCAN_VAR(soundbankselect);
		SCAN_VAR(dacvol);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(soundbankselect);
		ZetClose();
	}

	return 0;
}


// Mega Phoenix

static struct BurnRomInfo megaphxRomDesc[] = {
	{ "mph6.u32",						0x020000, 0xb99703d4,  1 | BRF_PRG | BRF_ESS },	//  0 68K Boot Code
	{ "mph7.u21",						0x020000, 0xf11e7449,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "mph0.u38",						0x020000, 0xb63dd20f,  2 | BRF_PRG | BRF_ESS },	//  2 68K Code
	{ "mph1.u27",						0x020000, 0x4dcbf44b,  2 | BRF_PRG | BRF_ESS },	//  3 
	{ "mph2.u37",						0x020000, 0xa0f69c27,  2 | BRF_PRG | BRF_ESS },	//  4 
	{ "mph3.u26",						0x020000, 0x4db84cc5,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "mph4.u36",						0x020000, 0xc8e0725e,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "mph5.u25",						0x020000, 0xc95ccb69,  2 | BRF_PRG | BRF_ESS },	//  7 

	{ "sonido_mph1.u39",				0x020000, 0xf5e65557,  3 | BRF_PRG | BRF_ESS },	//  8 Z80 Bank Data
	{ "sonido_mph2.u38",				0x020000, 0x7444d0f9,  3 | BRF_PRG | BRF_ESS },	//  9 

	{ "sonido_mph0.u35",				0x002000, 0xabc1b140,  4 | BRF_PRG | BRF_ESS },	// 10 Z80 Code

	{ "pic16c54-xt.bin",				0x000430, 0x21f396fb,  5 | BRF_PRG | BRF_ESS },	// 11 pic16C54 Code

	{ "p31_u31_palce16v8h-25.jed",		0x000bd4, 0x05ef04b7,  0 | BRF_OPT },			// 12 PALs
	{ "p40_u29_palce16v8h-25.jed",		0x000bd4, 0x44b7e51c,  0 | BRF_OPT },			// 13 
};

STD_ROM_PICK(megaphx)
STD_ROM_FN(megaphx)

struct BurnDriver BurnDrvMegaphx = {
	"megaphx", NULL, NULL, NULL, "1991",
	"Mega Phoenix\0", NULL, "Dinamic / Inder", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, megaphxRomInfo, megaphxRomName, NULL, NULL, NULL, NULL, MegaphxInputInfo, MegaphxDIPInfo,
	MegaphxInit, DrvExit, DrvFrame, IndervidDraw, DrvScan, &DrvRecalc, 0x100,
	338, 246, 4, 3
};


// Hammer Boy

static struct BurnRomInfo hamboyRomDesc[] = {
	{ "hb8.u32",						0x020000, 0x4f7b142a,  1 | BRF_PRG | BRF_ESS },	//  0 68K Boot Code
	{ "hb9.u21",						0x020000, 0x138e294f,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "hb0.u38",						0x020000, 0xb946a47f,  2 | BRF_PRG | BRF_ESS },	//  2 68K Code
	{ "hb1.u27",						0x020000, 0x890e1571,  2 | BRF_PRG | BRF_ESS },	//  3 
	{ "hb2.u37",						0x020000, 0xb71b0aad,  2 | BRF_PRG | BRF_ESS },	//  4 
	{ "hb3.u26",						0x020000, 0x1d0d61b9,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "hb4.u36",						0x020000, 0x9b81948e,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "hb5.u25",						0x020000, 0x23885e08,  2 | BRF_PRG | BRF_ESS },	//  7 
	{ "hb6.u35",						0x020000, 0x0c479648,  2 | BRF_PRG | BRF_ESS },	//  8 
	{ "hb7.u24",						0x020000, 0x297a6944,  2 | BRF_PRG | BRF_ESS },	//  9 

	{ "sonido_hammerboy_0.u39",			0x020000, 0x8d94ac97,  3 | BRF_PRG | BRF_ESS },	// 10 Z80 Bank Data
	{ "sonido_hammerboy_1.u38",			0x020000, 0xf92e5098,  3 | BRF_PRG | BRF_ESS },	// 11 

	{ "hammerboy.u35",					0x002000, 0xcd22f2a4,  4 | BRF_PRG | BRF_ESS },	// 12 Z80 Code

	{ "pic16c54-xt.bin",				0x000430, 0x21f396fb,  5 | BRF_PRG | BRF_ESS },	// 13 pic16C54 Code
};

STD_ROM_PICK(hamboy)
STD_ROM_FN(hamboy)

struct BurnDriver BurnDrvHamboy = {
	"hamboy", NULL, NULL, NULL, "1990",
	"Hammer Boy\0", NULL, "Dinamic / Inder", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, hamboyRomInfo, hamboyRomName, NULL, NULL, NULL, NULL, MegaphxInputInfo, HamboyDIPInfo,
	MegaphxInit, DrvExit, DrvFrame, IndervidDraw, DrvScan, &DrvRecalc, 0x100,
	320, 200, 4, 3
};


// YoYo Spell (prototype)

static struct BurnRomInfo yoyospelRomDesc[] = {
	{ "yo-yo_8.u21",					0x020000, 0x9f350036,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "yo-yo_9.u32",					0x020000, 0x7de27e36,  1 | BRF_PRG | BRF_ESS },	//  0 68K Boot Code

	{ "yo-yo_1.u38",					0x020000, 0x3f09bbf3,  2 | BRF_PRG | BRF_ESS },	//  2 68K Code
	{ "yo-yo_0.u27",					0x020000, 0x5aeeac9a,  2 | BRF_PRG | BRF_ESS },	//  3 
	{ "yo-yo_3.u37",					0x020000, 0x1af108b9,  2 | BRF_PRG | BRF_ESS },	//  4 
	{ "yo-yo_2.u26",					0x020000, 0xf578e99b,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "yo-yo_5.u36",					0x020000, 0xd1ebd4a4,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "yo-yo_4.u25",					0x020000, 0x7e3219c6,  2 | BRF_PRG | BRF_ESS },	//  7 
	{ "yo-yo_7.u35",					0x020000, 0x3139630c,  2 | BRF_PRG | BRF_ESS },	//  8 
	{ "yo-yo_6.u24",					0x020000, 0xb1066c4c,  2 | BRF_PRG | BRF_ESS },	//  9 

	{ "yo-yo_son_1.u39",				0x020000, 0xa075806e,  3 | BRF_PRG | BRF_ESS },	// 10 Z80 Bank Data
	{ "yo-yo_son_2.u38",				0x020000, 0xb31ad4a1,  3 | BRF_PRG | BRF_ESS },	// 11 

	{ "yo-yo_son_0.u35",				0x002000, 0x7f9a2dcf,  4 | BRF_PRG | BRF_ESS },	// 12 Z80 Code

	{ "pic16c54-xt.bin",				0x000430, 0x21f396fb,  5 | BRF_PRG | BRF_ESS },	// 13 pic16C54 Code

	{ "b5_pal20v8.bin",					0x000157, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 14 PALs
	{ "b26_pal16v10.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 15 
	{ "b15_pal16v8.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 16 
	{ "s33_pal16v8.u23",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 17 
	{ "s32_pal16v8.u32",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 18 
	{ "s31_pal16v8.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 19 
	{ "p29_pal16v8.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 20 
	{ "p31_pal16v8.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 21 
	{ "p28_pal16v8.bin",				0x000117, 0x00000000,  0 | BRF_OPT | BRF_NODUMP },			// 22 
};

STD_ROM_PICK(yoyospel)
STD_ROM_FN(yoyospel)

struct BurnDriver BurnDrvYoyospel = {
	"yoyospel", "littlerb", NULL, NULL, "1992",
	"YoYo Spell (prototype)\0", NULL, "Inder", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, yoyospelRomInfo, yoyospelRomName, NULL, NULL, NULL, NULL, MegaphxInputInfo, YoyospelDIPInfo,
	MegaphxInit, DrvExit, DrvFrame, IndervidDraw, DrvScan, &DrvRecalc, 0x100,
	338, 288, 4, 3
};
