// FinalBurn Neo Zaccaria driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "8255ppi.h"
#include "6821pia.h"
#include "watchdog.h"
#include "m6800_intf.h"
#include "ay8910.h"
#include "tms5220.h"
#include "dac.h"
#include "biquad.h"
#include "dtimer.h"

// cpu & pia designations, to retain sanity
#define MELODY 0
#define TMS 1
#define AUDIO 1

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM[2];
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSndRAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvAttrRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *flipscreen; // x,y
static UINT8 nmi_mask;
static UINT8 dip_select;

static UINT8 host_command;
static UINT8 melody_command;
static double tromba_vol;

static INT32 game_select = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static INT32 nCyclesExtra[4];

static BIQ biqvoice;
static BIQ biqbass;
static BIQ biqhorn;
static BIQ biqhorn2;
static BIQ biqhorn3;
static BIQ biqhorn4;

static INT16 *tempsound;

static dtimer melodytimer;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo MonymonyDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x11, NULL						},
	{0x01, 0xff, 0xff, 0x01, NULL						},
	{0x02, 0xff, 0xff, 0xd6, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"	},
	{0x00, 0x01, 0x04, 0x00, "Off"						},
	{0x00, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x08, 0x00, "Easy"						},
	{0x00, 0x01, 0x08, 0x08, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x10, 0x00, "Upright"					},
	{0x00, 0x01, 0x10, 0x10, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x20, 0x00, "Off"						},
	{0x00, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"		},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x03, 0x01, "200000"					},
	{0x01, 0x01, 0x03, 0x02, "300000"					},
	{0x01, 0x01, 0x03, 0x03, "400000"					},
	{0x01, 0x01, 0x03, 0x00, "None"						},

	{0   , 0xfe, 0   ,    2, "Table Title"				},
	{0x01, 0x01, 0x04, 0x00, "Todays High Scores"		},
	{0x01, 0x01, 0x04, 0x04, "High Scores"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x01, 0x01, 0x80, 0x00, "Off"						},
	{0x01, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x02, 0x01, 0x8c, 0x8c, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x8c, 0x88, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x8c, 0x84, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x8c, 0x80, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x8c, 0x0c, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x8c, 0x08, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x8c, 0x04, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x8c, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin C"					},
	{0x02, 0x01, 0x70, 0x70, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x70, 0x60, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x70, 0x50, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x70, 0x40, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x70, 0x30, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x70, 0x20, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x70, 0x10, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x70, 0x00, "1 Coin  7 Credits"		},
};

STDDIPINFO(Monymony)

static struct BurnDIPInfo JackrabtDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x11, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},
	{0x02, 0xff, 0xff, 0xd6, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"	},
	{0x00, 0x01, 0x04, 0x00, "Off"						},
	{0x00, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x10, 0x00, "Upright"					},
	{0x00, 0x01, 0x10, 0x10, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x20, 0x00, "Off"						},
	{0x00, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"		},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Table Title"				},
	{0x01, 0x01, 0x04, 0x00, "Todays High Scores"		},
	{0x01, 0x01, 0x04, 0x04, "High Scores"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x01, 0x01, 0x80, 0x00, "Off"						},
	{0x01, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x02, 0x01, 0x8c, 0x8c, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x8c, 0x88, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x8c, 0x84, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x8c, 0x80, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x8c, 0x0c, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x8c, 0x08, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x8c, 0x04, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x8c, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin C"					},
	{0x02, 0x01, 0x70, 0x70, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x70, 0x60, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x70, 0x50, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x70, 0x40, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x70, 0x30, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x70, 0x20, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x70, 0x10, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x70, 0x00, "1 Coin  7 Credits"		},
};

STDDIPINFO(Jackrabt)

static void sync_audio()
{
	INT32 cyc = (INT64)ZetTotalCycles() * 3579545 / 3072000;
	cyc -= M6800TotalCycles(AUDIO);

	if (cyc > 0) {
		M6800Run(AUDIO, cyc);
	}
}

static void __fastcall zaccaria_write(UINT16 address, UINT8 data)
{
	if (address > 0x6000 && address <= 0x67ff) {
		address &= 0x7ff;
		// 6400-67ff is 4bit memory, the upper nibble comes from prot.
		if (game_select == 1) { // jack rabbit
			switch (address) {
				case 0x404: data |= 0x40; break;
				case 0x406: data |= 0xa0; break;
			}
		} else { // money money
			switch (address) {
				case 0x400: data |= 0x50; break;
				case 0x406: data |= 0x70; break;
			}
		}
		DrvVidRAM[address] = data;
		return;
	}

	if ((address & 0x7e00) == 0x6c00)
	{
		data &= 1;

		switch (address & 7)
		{
			case 0:
			case 1:
				if (flipscreen[address & 1] ^ data) {
					if (game_select == 1) { // jackrabt
						// when flipscreen status changes, clear 2nd column of vidram
						// fixes some issues with Coctail mode
						// NOTE: 2nd player's first game (cocktail) doesn't print the
						// lifecount - the code below doesn't cause that
						for (INT32 i = 0; i < 0x20; i++) {
							DrvVidRAM[(i * 32) + 2] = 0xff;
						}
					}
				}
				flipscreen[address & 1] = data;
			return;

			case 2: if (data & 1) {
				M6800Reset(MELODY);
				M6800Reset(AUDIO);
			}
			return;

			case 6:	return;	// coin counter

			case 7:
				nmi_mask = data;
				if (!data) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE); // nmi clear line
			return;
		}
		return;
	}

	if ((address & 0x7e07) == 0x6e00)
	{
		sync_audio();
		host_command = data;
		M6800SetIRQLine(AUDIO, 0, (data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
		return;
	}

	switch (address)
	{
		case 0x7800:
		case 0x7801:
		case 0x7802:
		case 0x7803:
			ppi8255_w(0, address & 3, data);
		return;
	}
}

static UINT8 __fastcall zaccaria_read(UINT16 address)
{
	if ((address & 0x7e00) == 0x6c00)
	{
		switch (address & 7)
		{
			case 0:	{
				INT32 ret = DrvInputs[3] & ~0xf8;
				M6800CPUPush(AUDIO);
				ret |= (~pia_get_b(TMS) & 0x08);
				M6800CPUPop();
				return ret;
			}
			case 2: return 0x10; // Jack Rabbit
			case 4: if (game_select == 0) return 0x80; // Money Money
			case 6: if (game_select == 0) return 0;    // Money Money
		}
		return 0;
	}

	if ((address & 0x7e07) == 0x6e00)
	{
		return DrvDips[dip_select];
	}

	switch (address)
	{
		case 0x7800:
		case 0x7801:
		case 0x7802:
		case 0x7803:
			return ppi8255_r(0, address & 3);

		case 0x7c00:
			return BurnWatchdogRead();
	}

	if (address >= 0x6000 && address <= 0x67ff) {
		return DrvVidRAM[address & 0x7ff];
	}

	return 0;
}

static UINT8 zaccaria_melody_read(UINT16 address)
{
	switch (address & 0xe00c)
	{
		case 0x400c: {
			return pia_read(MELODY, address & 3);
		}
	}

	return 0xff;
}

static void zaccaria_melody_write(UINT16 address, UINT8 data)
{
	switch (address & 0xe00c)
	{
		case 0x400c:
			pia_write(MELODY, address & 3, data);
			return;
	}
}

static UINT8 zaccaria_audio_read(UINT16 address)
{
	if (address < 0x80) {
		return DrvSndRAM[1][address];
	}

	if ((address & 0x7090) == 0x0090) {
		return pia_read(TMS, address & 3);
	}

	if ((address & 0x3c00) == 0x1800) {
		return host_command;
	}

	return 0xff;
}

static void zaccaria_audio_write(UINT16 address, UINT8 data)
{
	if (address < 0x80) {
		DrvSndRAM[1][address] = data;
	}

	if ((address & 0x7090) == 0x0090) {
		pia_write(TMS, address & 3, data);
		return;
	}

	if ((address & ~0x83ff) == 0x1000) {
		DACWrite(0, data);
	}

	if ((address & 0x3c00) == 0x1400) {
		M6800CPUPush(MELODY);
		pia_set_input_ca1(MELODY, (data >> 7) & 0x01);
		M6800CPUPop();
		melody_command = data;
		return;
	}
}

static UINT8 ppi0_port_A_read()
{
	return DrvInputs[0]; // p1
}

static UINT8 ppi0_port_B_read()
{
	return DrvInputs[1]; // p2
}

static UINT8 ppi0_port_C_read()
{
	return DrvInputs[2]; // system
}

static void ppi0_port_C_write(UINT8 data)
{
	for (INT32 i = 0; i < 3; i++) {
		if ((data & (1 << (i + 4))) == 0) {
			dip_select = i;
			break;
		}
	}
}

static UINT8 AY8910_0_portB_read(UINT32)
{
	return melody_command;
}

static void AY8910_0_portA_write(UINT32, UINT32 data)
{
	UINT8 d2 = data;
	d2 = ((d2 & 4) >> 1) | ((d2 & 2) << 1) | (d2 & 1);
	d2 ^= 7;
	tromba_vol = d2 * 0.14;
}

static void AY8910_1_portA_write(UINT32, UINT32 data)
{
}

static void AY8910_1_portB_write(UINT32, UINT32 data)
{
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvVidRAM[offs + 0x400];
	INT32 code  = DrvVidRAM[offs + 0x000] + (attr << 8);
	INT32 color = (DrvAttrRAM[((offs & 0x1f) ^ ((flipscreen[0]) ? 0x1f : 0)) * 2 + 1] << 2) | ((attr >> 2) & 3);

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	M6800Reset(MELODY);
	timerReset();

	M6800Open(AUDIO);
	M6800Reset();
	tms5220_reset();
	DACReset();
	M6800Close();

	AY8910Reset(0);
	AY8910Reset(1);

	biqvoice.reset();
	biqbass.reset();
	biqhorn.reset();
	biqhorn2.reset();
	biqhorn3.reset();
	biqhorn4.reset();

	ppi8255_reset();
	pia_reset();

	BurnWatchdogReset();

	nmi_mask = 0;
	dip_select = 0;
	melody_command = 0;
	host_command = 0;
	tromba_vol = 0.0;

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvSndROM[0]	= Next; Next += 0x010000;
	DrvSndROM[1]	= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x010000;
	DrvGfxROM[1]	= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000420;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;

	DrvVidRAM		= Next; Next += 0x000800;
	DrvAttrRAM		= Next; Next += 0x000100;

	DrvSndRAM[0]	= Next; Next += 0x000100; // 0x80
	DrvSndRAM[1]	= Next; Next += 0x000100; // 0x80

	flipscreen		= Next; Next += 0x000002;

	RamEnd			= Next;

	tempsound       = (INT16*)Next; Next += (96000 / 60) * 2 * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { 0x2000*2*8, 0x2000*1*8, 0x2000*0*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x6000);

	GfxDecode(0x0400, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);
	GfxDecode(0x0100, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree(tmp);

	return 0;
}

static UINT8 tms_pia_in_a(UINT16 offset)
{
	return tms5220_status();
}

static void tms_pia_out_a(UINT16 offset, UINT8 data)
{
	tms5220_write(data);
}

static void tms_pia_out_b(UINT16 offset, UINT8 data)
{
	tms5220_rsq_w(data & 1);
	tms5220_wsq_w((data & 2) >> 1);
}

static UINT8 mel_pia_in_a(UINT16 offset)
{
	UINT8 c = pia_get_b(MELODY);
	UINT8 data = 0xff;

	if ((c & 0x03) == 0x01) {
		data &= AY8910Read(0);
	}

	if ((c & 0x0c) == 0x04) {
		data &= AY8910Read(1);
	}

	return data;
}

static void mel_pia_out_a(UINT16 offset, UINT8 data)
{
	UINT8 c = pia_get_b(MELODY);

	if (c & 0x02) {
		AY8910Write(0, ~c & 1, data);
	}

	if (c & 0x08) {
		AY8910Write(1, (~c >> 2) & 1, data);
	}
}

static void mel_pia_out_b(UINT16 offset, UINT8 data)
{
	UINT8 c = pia_get_a(MELODY);

	if (data & 0x02) {
		AY8910Write(0, ~data & 1, c);
	}

	if (data & 0x08) {
		AY8910Write(1, (~data >> 2) & 1, c);
	}
}

static void mel_pia_irqa(INT32 state)
{
	M6800SetIRQLine(0x20, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void mel_pia_irqb(INT32 state)
{
	M6800SetIRQLine(0x00, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void tms_rqf(INT32 state)
{
	pia_set_input_ca2(AUDIO, state);
}

static void tms_irqf(INT32 state)
{
	pia_set_input_cb1(AUDIO, state);
}

static void melody_clk_cb(INT32 state)
{
	pia_set_input_cb1(MELODY, state);
}

static INT32 DrvInit(INT32 load_type)
{
	BurnAllocMemIndex();

	if (load_type == 0) // monymony, monymony2, jackrabt2, jackrabts
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x00000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x8000, DrvZ80ROM + 0x1000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x01000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x9000, DrvZ80ROM + 0x2000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x02000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0xa000, DrvZ80ROM + 0x3000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x03000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0xb000, DrvZ80ROM + 0x4000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x04000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0xc000, DrvZ80ROM + 0x5000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x05000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0xd000, DrvZ80ROM + 0x6000, 0x1000);

		if (BurnLoadRom(DrvSndROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x0c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x02000, k++, 1)) return 1;
		memcpy (DrvSndROM[1] + 0x6000, DrvSndROM[1] + 0x3000, 0x1000);
		if (BurnLoadRom(DrvSndROM[1] + 0x03000, k++, 1)) return 1;
		memcpy (DrvSndROM[1] + 0x7000, DrvSndROM[1] + 0x4000, 0x1000);

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;

	}
	else // jackrabt
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x00000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x8000, DrvZ80ROM + 0x1000, 0x1000);
		if (BurnLoadRom(DrvZ80ROM    + 0x01000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x03000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x05000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x09000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0b000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x0d000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x0c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x02000, k++, 1)) return 1;
		memcpy (DrvSndROM[1] + 0x6000, DrvSndROM[1] + 0x3000, 0x1000);
		if (BurnLoadRom(DrvSndROM[1] + 0x03000, k++, 1)) return 1;
		memcpy (DrvSndROM[1] + 0x7000, DrvSndROM[1] + 0x4000, 0x1000);

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x5fff, MAP_ROM);
	//ZetMapMemory(DrvVidRAM,				0x6000, 0x67ff, MAP_WRITE); // handler
	ZetMapMemory(DrvAttrRAM,			0x6800, 0x68ff, MAP_RAM); // 00-3f attr, 40-5f spr0, 81-c0 spr1 (weird!)
	ZetMapMemory(DrvZ80RAM,				0x7000, 0x77ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xdfff, MAP_ROM);
	ZetSetWriteHandler(zaccaria_write);
	ZetSetReadHandler(zaccaria_read);
	ZetClose();

	ppi8255_init(1);
	ppi8255_set_read_ports(0, ppi0_port_A_read, ppi0_port_B_read, ppi0_port_C_read);
	ppi8255_set_write_ports(0, NULL, NULL, ppi0_port_C_write);

	{
		static pia6821_interface tms_pia = {
			tms_pia_in_a, NULL, NULL, NULL, NULL, NULL,
			tms_pia_out_a, tms_pia_out_b, NULL, NULL, NULL, NULL
		};
		static pia6821_interface mel_pia = {
			mel_pia_in_a, NULL, NULL, NULL, NULL, NULL,
			mel_pia_out_a, mel_pia_out_b, NULL, NULL, mel_pia_irqa, mel_pia_irqb
		};
		pia_init();
		pia_config(MELODY, 0, &mel_pia);
		pia_config(TMS, 0, &tms_pia);
	}

	BurnWatchdogInit(DrvDoReset, 180); // guess

	M6800Init(0);
	M6800Open(0);
	M6800MapMemory(DrvSndRAM[0], 			0x0000, 0x007f, MAP_RAM);
	M6800MapMemory(DrvSndROM[0] + 0x8000,	0x8000, 0x9fff, MAP_ROM);
	M6800MapMemory(DrvSndROM[0] + 0x8000,	0xa000, 0xbfff, MAP_ROM);
	M6800MapMemory(DrvSndROM[0] + 0xc000,	0xc000, 0xdfff, MAP_ROM);
	M6800MapMemory(DrvSndROM[0] + 0xc000,	0xe000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(zaccaria_melody_write);
	M6800SetReadHandler(zaccaria_melody_read);
	M6800Close();

	M6800Init(1);
	M6800Open(1);
	//M6800MapMemory(DrvSndRAM[1], 			0x0000, 0x007f, MAP_RAM); HANDLER
	M6800MapMemory(DrvSndROM[1] + 0x2000,	0x2000, 0x7fff, MAP_ROM);
	M6800MapMemory(DrvSndROM[1] + 0x2000,	0xa000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(zaccaria_audio_write);
	M6800SetReadHandler(zaccaria_audio_read);
	M6800Close();

	AY8910Init(0, 3579545 / 2, 0);
	AY8910Init(1, 3579545 / 2, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(M6800TotalCycles, 3579545 / 4);
	AY8910SetPorts(0, NULL, &AY8910_0_portB_read, &AY8910_0_portA_write, NULL);
	AY8910SetPorts(1, NULL, NULL, &AY8910_1_portA_write, &AY8910_1_portB_write);

	tms5200_init(629200, M6800TotalCycles, 3579545 / 4);
	tms5220_volume(0.70);
	tms5220_set_readyq_func(tms_rqf);
	tms5220_set_irq_func(tms_irqf);

	DACInit(0, 0, 1, M6800TotalCycles, 3579545 / 4);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	biqvoice.init(FILT_LOWPASS, nBurnSoundRate, 2000, 0.8, 0);

	biqbass.init(FILT_LOWPASS, nBurnSoundRate, 301, 1.4, 0);

	biqhorn.init(FILT_LOWPASS, nBurnSoundRate, 460, 4.1, 0);
	biqhorn2.init(FILT_LOWPASS, nBurnSoundRate, 1100, 9, 0);
	biqhorn3.init(FILT_LOWPASS, nBurnSoundRate, 590, 4.1, 0);
	biqhorn4.init(FILT_HIGHPASS, nBurnSoundRate, 200, 3, 0);

	timerInit();
	timerAddClockSource(melodytimer, 4096, melody_clk_cb); // running at 3579545

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 3,  8,  8, 0x10000, 0x000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3, 16, 16, 0x10000, 0x100, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetScrollCols(0, 32);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	M6800Exit();
	ppi8255_exit();
	pia_exit();

	timerExit();

	AY8910Exit(0);
	AY8910Exit(1);
	tms5220_exit();
	DACExit();

	biqvoice.exit();
	biqbass.exit();
	biqhorn.exit();
	biqhorn2.exit();
	biqhorn3.exit();
	biqhorn4.exit();

	BurnWatchdogExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tmp[0x200];

	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 c = (DrvColPROM[i] & 0xf) | (DrvColPROM[i + 0x200] << 4);

		INT32 b0 = (c >> 3) & 1;
		INT32 b1 = (c >> 2) & 1;
		INT32 b2 = (c >> 1) & 1;
		INT32 r = (((b0 * 1200) + (b1 * 1000) + (b2 * 820)) * 255) / (1200 + 1000 + 820);

		b0 = (c >> 0) & 1;
		b1 = (c >> 7) & 1;
		b2 = (c >> 6) & 1;
		INT32 g = (((b0 * 1200) + (b1 * 1000) + (b2 * 820)) * 255) / (1200 + 1000 + 820);

		b0 = (c >> 5) & 1;
		b1 = (c >> 4) & 1;
		INT32 b = (((b0 * 1000) + (b1 * 820)) * 255) / (1000 + 820);

		tmp[i] = ((i & 0x38) == 0) ? 0 : BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = tmp[((i & 0x07) << 3) | ((i & 0x18) >> 2) | ((i & 0xe0) << 1) | ((i & 0x100) >> 8)];
	}
}

static void draw_sprites(INT32 offset, INT32 base_color, INT32 swap)
{
	UINT8 *ram = DrvAttrRAM + offset;

	for (INT32 i = 0; i < 0x20; i+=4)
	{
		INT32 sy    = 242 - ram[i];
		INT32 attr0 = ram[i + 1 + swap];
		INT32 attr1 = ram[i + 2 - swap];
		INT32 sx    = ram[i + 3] + 1;
		INT32 code  = (attr0 & 0x3f) | (attr1 & 0xc0);
		INT32 color = ((attr1 & 0x07) << 2) | base_color;
		INT32 flipx = attr0 & 0x40;
		INT32 flipy = attr0 & 0x80;

		if (sx == 1) continue; // fixes junk sprites

		if (flipscreen[0]) {
			sx = 240 - sx;
			flipx ^= 0x40;
		}
		if (flipscreen[1]) {
			sy = 243 - sy;
			flipy ^= 0x80;
		}

		DrawGfxMaskTile(0, 1, code, sx, sy - 16, flipx, flipy, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < 32; i++) {
		if (flipscreen[0]) {
			GenericTilemapSetScrollCol(0, 31-i, DrvAttrRAM[i * 2]);
		} else {
			GenericTilemapSetScrollCol(0, i, DrvAttrRAM[i * 2]);
		}
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(0x81, 2, 1);
	if (nSpriteEnable & 2) draw_sprites(0x40, 1, 0);
	if (nSpriteEnable & 4) draw_sprites(0xa1, 0, 1);

	BurnTransferFlip(flipscreen[0], flipscreen[1]); // coctail unflippy
	BurnTransferCopy(DrvPalette);

	return 0;
}

// mono input, stereo output
static void mix_m2s(INT16 *buf_in, INT16 *buf_out, INT32 buf_len, double volume)
{
	for (INT32 i = 0; i < buf_len; i++) {
		const INT32 a = i * 2 + 0;
		const INT32 b = i * 2 + 1;

		buf_out[a] = BURN_SND_CLIP(buf_out[a] + (buf_in[i] * volume));
		buf_out[b] = BURN_SND_CLIP(buf_out[b] + (buf_in[i] * volume));
	}
}

// mono input on stereo signal to stereo output
static void mix_sm2s(INT16 *buf_in, INT16 *buf_out, INT32 buf_len, double volume)
{
	for (INT32 i = 0; i < buf_len; i++) {
		const INT32 a = i * 2 + 0;
		const INT32 b = i * 2 + 1;

		buf_out[a] = BURN_SND_CLIP(buf_out[a] + (buf_in[a] * volume));
		buf_out[b] = BURN_SND_CLIP(buf_out[b] + (buf_in[a] * volume));
	}
}

// fuzz pedal distortion (mono stream)
static void disty(INT16 *buf_in, INT32 buf_len)
{
	for (INT32 i = 0; i < buf_len; i++) {
		double x = (double)buf_in[i] * 0.00001;
		double y = 0;
		y = tanh(sin(x));
		buf_in[i] = BURN_SND_CLIP(y * 0xffff);
	}
}

// dc blocking filter (mono stream)
static void dcblock(INT16 *buf_in, INT32 buf_len)
{
	static INT16 lastin  = 0;
	static INT16 lastout = 0;

	for (INT32 i = 0; i < buf_len; i++) {
		INT16 m = buf_in[i];

		INT16 out = m - lastin + 0.999 * lastout;

		lastin = m;
		lastout = out;
		buf_in[i] = out;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	BurnWatchdogUpdate();

	ZetNewFrame();
	M6800NewFrame();
	timerNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] = { (INT32)(3072000 / 60.57), (INT32)(3579545 / 4 / 60.57), (INT32)(3579545 / 4 / 60.57), (INT32)(3579545 / 60.57) };
	INT32 nCyclesDone[4] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2], nCyclesExtra[3] };

	M6800Idle(AUDIO, nCyclesExtra[2]); // _SYNCINT

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 239 && nmi_mask) {
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
		ZetClose();

		M6800Open(MELODY);  // melody
		CPU_RUN(1, M6800);
		CPU_RUN(3, timer); // clock for this cpu
		M6800Close();

		M6800Open(AUDIO); // audio
		CPU_RUN_SYNCINT(2, M6800);
		M6800Close();
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = M6800TotalCycles(AUDIO) - nCyclesTotal[2]; // _SYNCINT
	nCyclesExtra[3] = nCyclesDone[3] - nCyclesTotal[3];

	if (pBurnSoundOut) {
		BurnSoundClear();
		AY8910RenderInternal(nBurnSoundLen);

		// filter bass channel
		biqbass.filter_buffer(pAY8910Buffer[1], nBurnSoundLen); // bass

		// filter horn channel
		disty(pAY8910Buffer[4], nBurnSoundLen); // horn
		biqhorn.filter_buffer(pAY8910Buffer[4], nBurnSoundLen); // horn
		biqhorn3.filter_buffer(pAY8910Buffer[4], nBurnSoundLen); // horn
		biqhorn4.filter_buffer(pAY8910Buffer[4], nBurnSoundLen); // horn
		biqhorn2.filter_buffer(pAY8910Buffer[4], nBurnSoundLen); // horn
		dcblock(pAY8910Buffer[4], nBurnSoundLen); // horn

		// mix ay0
		mix_m2s(pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0.06);
		mix_m2s(pAY8910Buffer[1], pBurnSoundOut, nBurnSoundLen, 0.25);
		mix_m2s(pAY8910Buffer[2], pBurnSoundOut, nBurnSoundLen, 0.15);

		// mix ay1
		mix_m2s(pAY8910Buffer[3], pBurnSoundOut, nBurnSoundLen, 0.08);
		mix_m2s(pAY8910Buffer[4], pBurnSoundOut, nBurnSoundLen, tromba_vol/20);
		mix_m2s(pAY8910Buffer[5], pBurnSoundOut, nBurnSoundLen, 0.15);

		// render, filter & mix tms5220 (speech synth)
		memset(tempsound, 0, nBurnSoundLen*2*2);
		tms5220_update(tempsound, nBurnSoundLen);
		biqvoice.filter_buffer_mono_stereo_stream(tempsound, nBurnSoundLen);
		mix_sm2s(tempsound, pBurnSoundOut, nBurnSoundLen, 1.00);

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		M6800Scan(nAction);

		AY8910Scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		ppi8255_scan();
		pia_scan(nAction, pnMin);

		timerScan();

		BurnWatchdogScan(nAction);

		SCAN_VAR(nmi_mask);
		SCAN_VAR(dip_select);
		SCAN_VAR(melody_command);
		SCAN_VAR(host_command);
		SCAN_VAR(tromba_vol);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Money Money (set 1)

static struct BurnRomInfo monymonyRomDesc[] = {
	{ "cpu1.1a",		0x2000, 0x13c227ca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cpu2.1b",		0x2000, 0x87372545, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu3.1c",		0x2000, 0x6aea9c01, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu4.1d",		0x2000, 0x5fdec451, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu5.2a",		0x2000, 0xaf830e3c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu6.2c",		0x2000, 0x31da62b1, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "snd13.2g",		0x2000, 0x78b01b98, 2 | BRF_PRG | BRF_ESS }, //  6 M6802 Code (Melody)
	{ "snd9.1i",		0x2000, 0x94e3858b, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "snd8.1h",		0x2000, 0xaad76193, 3 | BRF_PRG | BRF_ESS }, //  8 M6802 Code (Speech)
	{ "snd7.1g",		0x2000, 0x1e8ffe3e, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "bg1.2d",			0x2000, 0x82ab4d1a, 4 | BRF_GRA },           // 10 Graphics
	{ "bg2.1f",			0x2000, 0x40d4e4d1, 4 | BRF_GRA },           // 11
	{ "bg3.1e",			0x2000, 0x36980455, 4 | BRF_GRA },           // 12

	{ "9g",				0x0200, 0xfc9a0f21, 5 | BRF_GRA },           // 13 Color PROMs
	{ "9f",				0x0200, 0x93106704, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(monymony)
STD_ROM_FN(monymony)

static INT32 monymonyInit()
{
	game_select = 0;

	return DrvInit(0);
}

struct BurnDriver BurnDrvMonymony = {
	"monymony", NULL, NULL, NULL, "1983",
	"Money Money (set 1)\0", NULL, "Zaccaria", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, monymonyRomInfo, monymonyRomName, NULL, NULL, NULL, NULL, DrvInputInfo, MonymonyDIPInfo,
	monymonyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Money Money (set 2)

static struct BurnRomInfo monymony2RomDesc[] = {
	{ "cpu1.1a",		0x2000, 0x907225b2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cpu2.1b",		0x2000, 0x87372545, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu3.1c",		0x2000, 0x3c874c16, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu4.1d",		0x2000, 0x5fdec451, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu5.2a",		0x2000, 0xaf830e3c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu6.2c",		0x2000, 0x31da62b1, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "snd13.2g",		0x2000, 0x78b01b98, 2 | BRF_PRG | BRF_ESS }, //  6 M6802 Code (Melody)
	{ "snd9.1i",		0x2000, 0x94e3858b, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "snd8.1h",		0x2000, 0xaad76193, 3 | BRF_PRG | BRF_ESS }, //  8 M6802 Code (Speech)
	{ "snd7.1g",		0x2000, 0x1e8ffe3e, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "bg1.2d",			0x2000, 0x82ab4d1a, 4 | BRF_GRA },           // 10 Graphics
	{ "bg2.1f",			0x2000, 0x40d4e4d1, 4 | BRF_GRA },           // 11
	{ "bg3.1e",			0x2000, 0x36980455, 4 | BRF_GRA },           // 12

	{ "9g",				0x0200, 0xfc9a0f21, 5 | BRF_GRA },           // 13 Color PROMs
	{ "9f",				0x0200, 0x93106704, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(monymony2)
STD_ROM_FN(monymony2)

struct BurnDriver BurnDrvMonymony2 = {
	"monymony2", "monymony", NULL, NULL, "1983",
	"Money Money (set 2)\0", NULL, "Zaccaria", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, monymony2RomInfo, monymony2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, MonymonyDIPInfo,
	monymonyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Jack Rabbit (set 1)

static struct BurnRomInfo jackrabtRomDesc[] = {
	{ "cpu-01.1a",		0x2000, 0x499efe97, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cpu-01.2l",		0x1000, 0x4772e557, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu-01.3l",		0x1000, 0x1e844228, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu-01.4l",		0x1000, 0xebffcc38, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu-01.5l",		0x1000, 0x275e0ed6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu-01.6l",		0x1000, 0x8a20977a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cpu-01.2h",		0x1000, 0x21f2be2a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cpu-01.3h",		0x1000, 0x59077027, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "cpu-01.4h",		0x1000, 0x0b9db007, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "cpu-01.5h",		0x1000, 0x785e1a01, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "cpu-01.6h",		0x1000, 0xdd5979cf, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "13snd.2g",		0x2000, 0xfc05654e, 2 | BRF_PRG | BRF_ESS }, // 11 M6802 Code (Melody)
	{ "9snd.1i",		0x2000, 0x3dab977f, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "8snd.1h",		0x2000, 0xf4507111, 3 | BRF_PRG | BRF_ESS }, // 13 M6802 Code (Speech)
	{ "7snd.1g",		0x2000, 0xc722eff8, 3 | BRF_PRG | BRF_ESS }, // 14

	{ "1bg.2d",			0x2000, 0x9f880ef5, 4 | BRF_GRA },           // 15 Graphics
	{ "2bg.1f",			0x2000, 0xafc04cd7, 4 | BRF_GRA },           // 16
	{ "3bg.1e",			0x2000, 0x14f23cdd, 4 | BRF_GRA },           // 17

	{ "jr-ic9g",		0x0200, 0x85577107, 5 | BRF_GRA },           // 18 Color PROMs
	{ "jr-ic9f",		0x0200, 0x085914d1, 5 | BRF_GRA },           // 19

	{ "jr-pal16l8.6j",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 20 plds
	{ "jr-pal16l8.6k",	0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(jackrabt)
STD_ROM_FN(jackrabt)

static INT32 jackrabtInit()
{
	game_select = 1;

	return DrvInit(1);
}

struct BurnDriver BurnDrvJackrabt = {
	"jackrabt", NULL, NULL, NULL, "1984",
	"Jack Rabbit (set 1)\0", NULL, "Zaccaria", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, jackrabtRomInfo, jackrabtRomName, NULL, NULL, NULL, NULL, DrvInputInfo, JackrabtDIPInfo,
	jackrabtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Jack Rabbit (set 2)

static struct BurnRomInfo jackrabt2RomDesc[] = {
	{ "1cpu2.1a",		0x2000, 0xf9374113, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2cpu2.1b",		0x2000, 0x0a0eea4a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3cpu2.1c",		0x2000, 0x291f5772, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4cpu2.1d",		0x2000, 0x10972cfb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5cpu2.2a",		0x2000, 0xaa95d06d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6cpu2.2c",		0x2000, 0x404496eb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "13snd.2g",		0x2000, 0xfc05654e, 2 | BRF_PRG | BRF_ESS }, //  6 M6802 Code (Melody)
	{ "9snd.1i",		0x2000, 0x3dab977f, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "8snd.1h",		0x2000, 0xf4507111, 3 | BRF_PRG | BRF_ESS }, //  8 M6802 Code (Speech)
	{ "7snd.1g",		0x2000, 0xc722eff8, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "1bg.2d",			0x2000, 0x9f880ef5, 4 | BRF_GRA },           // 10 Graphics
	{ "2bg.1f",			0x2000, 0xafc04cd7, 4 | BRF_GRA },           // 11
	{ "3bg.1e",			0x2000, 0x14f23cdd, 4 | BRF_GRA },           // 12

	{ "jr-ic9g",		0x0200, 0x85577107, 5 | BRF_GRA },           // 13 Color PROMs
	{ "jr-ic9f",		0x0200, 0x085914d1, 5 | BRF_GRA },           // 14

	{ "pal16l8.6j",		0x0104, 0xa88e52d6, 6 | BRF_OPT },           // 15 plds
	{ "pal16l8.6k",		0x0104, 0x00000000, 6 | BRF_NODUMP | BRF_OPT },           // 16
	{ "82s100.8c",		0x00f5, 0x70ddfa6d, 6 | BRF_OPT },           // 17
	{ "82s100.8n",		0x00f5, 0xe00625ee, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(jackrabt2)
STD_ROM_FN(jackrabt2)

static INT32 jackrabt2Init()
{
	game_select = 1;

	return DrvInit(0);
}

struct BurnDriver BurnDrvJackrabt2 = {
	"jackrabt2", "jackrabt", NULL, NULL, "1984",
	"Jack Rabbit (set 2)\0", NULL, "Zaccaria", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, jackrabt2RomInfo, jackrabt2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, JackrabtDIPInfo,
	jackrabt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Jack Rabbit (special)

static struct BurnRomInfo jackrabtsRomDesc[] = {
	{ "1cpu.1a",		0x2000, 0x6698dc65, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2cpu.1b",		0x2000, 0x42b32929, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3cpu.1c",		0x2000, 0x89b50c9a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4cpu.1d",		0x2000, 0xd5520665, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5cpu.2a",		0x2000, 0x0f9a093c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6cpu.2c",		0x2000, 0xf53d6356, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "13snd.2g",		0x2000, 0xfc05654e, 2 | BRF_PRG | BRF_ESS }, //  6 M6802 Code (Melody)
	{ "9snd.1i",		0x2000, 0x3dab977f, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "8snd.1h",		0x2000, 0xf4507111, 3 | BRF_PRG | BRF_ESS }, //  8 M6802 Code (Speech)
	{ "7snd.1g",		0x2000, 0xc722eff8, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "1bg.2d",			0x2000, 0x9f880ef5, 4 | BRF_GRA },           // 10 Graphics
	{ "2bg.1f",			0x2000, 0xafc04cd7, 4 | BRF_GRA },           // 11
	{ "3bg.1e",			0x2000, 0x14f23cdd, 4 | BRF_GRA },           // 12

	{ "jr-ic9g",		0x0200, 0x85577107, 5 | BRF_GRA },           // 13 Color PROMs
	{ "jr-ic9f",		0x0200, 0x085914d1, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(jackrabts)
STD_ROM_FN(jackrabts)

struct BurnDriver BurnDrvJackrabts = {
	"jackrabts", "jackrabt", NULL, NULL, "1984",
	"Jack Rabbit (special)\0", NULL, "Zaccaria", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, jackrabtsRomInfo, jackrabtsRomName, NULL, NULL, NULL, NULL, DrvInputInfo, JackrabtDIPInfo,
	jackrabt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
