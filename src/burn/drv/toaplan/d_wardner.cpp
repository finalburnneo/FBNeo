// FB Alpha Wardner driver module
// Based on MAME driver by Quench

#include "tiles_generic.h"
#include "tms32010.h"
#include "z80_intf.h"
#include "burn_ym3812.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvMCUROM;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSprRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvShareRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvSprBuf;

static UINT16 *pTempDraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 irq_enable;
static INT32 flipscreen;
static INT32 bgrambank;
static INT32 fgrombank;
static INT32 displayenable;

static INT32 z80_halt;
static UINT32 main_ram_seg;
static UINT16 dsp_addr_w;
static INT32 dsp_execute;
static INT32 dsp_BIO;
static INT32 dsp_on;

static UINT16 scrollx[4];
static UINT16 scrolly[4];
static UINT16 vidramoffs[4];

static INT32 vblank;
static UINT8 main_bank;
static INT32 coin_lockout;
static INT32 previous_coin;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo WardnerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Skip RAM Test",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wardner)

static struct BurnDIPInfo WardnerDIPList[]=
{
	{0x15, 0xff, 0xff, 0x01, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x01, 0x01, "Upright"		},
	{0x15, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x02, 0x00, "Off"			},
	{0x15, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x04, 0x00, "Off"			},
	{0x15, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x08, "Off"			},
	{0x15, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x30, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x20, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x01, "Easy"			},
	{0x16, 0x01, 0x03, 0x00, "Normal"		},
	{0x16, 0x01, 0x03, 0x02, "Hard"			},
	{0x16, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x16, 0x01, 0x0c, 0x00, "30k 80k 50k+"		},
	{0x16, 0x01, 0x0c, 0x04, "50k 100k 50k+"	},
	{0x16, 0x01, 0x0c, 0x08, "30k Only"		},
	{0x16, 0x01, 0x0c, 0x0c, "50k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x30, 0x30, "1"			},
	{0x16, 0x01, 0x30, 0x00, "3"			},
	{0x16, 0x01, 0x30, 0x10, "4"			},
	{0x16, 0x01, 0x30, 0x20, "5"			},
};

STDDIPINFO(Wardner)

static struct BurnDIPInfo WardnerjDIPList[]=
{
	{0x15, 0xff, 0xff, 0x01, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x01, 0x01, "Upright"		},
	{0x15, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x02, 0x00, "Off"			},
	{0x15, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x04, 0x00, "Off"			},
	{0x15, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x08, "Off"			},
	{0x15, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x01, "Easy"			},
	{0x16, 0x01, 0x03, 0x00, "Normal"		},
	{0x16, 0x01, 0x03, 0x02, "Hard"			},
	{0x16, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x16, 0x01, 0x0c, 0x00, "30k 80k 50k+"		},
	{0x16, 0x01, 0x0c, 0x04, "50k 100k 50k+"	},
	{0x16, 0x01, 0x0c, 0x08, "30k Only"		},
	{0x16, 0x01, 0x0c, 0x0c, "50k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x30, 0x30, "1"			},
	{0x16, 0x01, 0x30, 0x00, "3"			},
	{0x16, 0x01, 0x30, 0x10, "4"			},
	{0x16, 0x01, 0x30, 0x20, "5"			},
};

STDDIPINFO(Wardnerj)

static struct BurnDIPInfo PyrosDIPList[]=
{
	{0x15, 0xff, 0xff, 0x01, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x01, 0x01, "Upright"		},
	{0x15, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x02, 0x00, "Off"			},
	{0x15, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x04, 0x00, "Off"			},
	{0x15, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x08, "Off"			},
	{0x15, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x01, "Easy"			},
	{0x16, 0x01, 0x03, 0x00, "Normal"		},
	{0x16, 0x01, 0x03, 0x02, "Hard"			},
	{0x16, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x16, 0x01, 0x0c, 0x00, "30k 80k 50k+"		},
	{0x16, 0x01, 0x0c, 0x04, "50k 100k 50k+"	},
	{0x16, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x16, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x30, 0x30, "1"			},
	{0x16, 0x01, 0x30, 0x00, "3"			},
	{0x16, 0x01, 0x30, 0x10, "4"			},
	{0x16, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x16, 0x01, 0x40, 0x40, "No"			},
	{0x16, 0x01, 0x40, 0x00, "Yes"			},
};

STDDIPINFO(Pyros)

static void palette_write(INT32 offset)
{
	offset &= 0xffe;

	INT32 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >>  0) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void wardner_dsp(INT32 enable)
{
	enable ^= 1;
	dsp_on = enable;

	if (enable)
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_ACK); /* TMS32010 INT */
		z80_halt = 1;
		z80_ICount = 0;
	}
	else
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_NONE); /* TMS32010 INT */
	}
}

static void wardner_coin_write(UINT8 data)
{
	switch (data)
	{
		case 0x00:
		case 0x01:
			wardner_dsp(data);
			return;
		case 0x0c: coin_lockout = 0x08; break;
		case 0x0d: coin_lockout = 0x00; break;
		case 0x0e: coin_lockout = 0x10; break;
		case 0x0f: coin_lockout = 0x00; break;
	}
}

static void control_write(UINT8 data)
{
	switch (data)
	{
		case 0x04:
		case 0x05:
			irq_enable = data & 1;
		break;

		case 0x06:
		case 0x07:
			flipscreen = data & 1;
		break;

		case 0x08:
		case 0x09:
			bgrambank = ((data & 1) << 13);
		break;

		case 0x0a:
		case 0x0b:
			fgrombank = ((data & 1) << 12);
		break;

		case 0x0c:
		case 0x0d: // not for z80 cpus, only 68k! -dink
			//wardner_dsp(data & 1);
		break;

		case 0x0e:
		case 0x0f:
			displayenable = data & 1;
		break;
	}
}

static void bankswitch(INT32 data)
{
	main_bank = data;
	INT32 bank = (data & 0x07) * 0x8000;

	ZetMapMemory(DrvZ80ROM0 + bank, 0x8000, 0xffff, MAP_ROM);

	if (bank == 0) {
		ZetMapMemory(DrvSprRAM,		0x8000, 0x8fff, MAP_ROM);
		ZetMapMemory(DrvPalRAM,		0xa000, 0xafff, MAP_ROM);
		ZetMapMemory(DrvShareRAM,	0xc000, 0xc7ff, MAP_ROM);
	}
}

static void __fastcall wardner_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xa000) {
		DrvPalRAM[address & 0xfff] = data;
		palette_write(address);
		return;
	}
}

static void __fastcall wardner_main_write_port(UINT16 port, UINT8 data)
{
	INT32 offs = ((port/0x10)-1)&3;

	switch (port & 0xff)
	{
		case 0x00:
		case 0x02:
			// mc6845
		return;

		case 0x10:
		case 0x20:
		case 0x30:
			scrollx[offs] = (scrollx[offs] & 0x100) | (data);
		return;

		case 0x11:
		case 0x21:
		case 0x31:
			scrollx[offs] = (scrollx[offs] & 0x0ff) | (data << 8);
		return;

		case 0x12:
		case 0x22:
		case 0x32:
			scrolly[offs] = (scrolly[offs] & 0x100) | (data);
		return;

		case 0x13:
		case 0x23:
		case 0x33:
			scrolly[offs] = (scrolly[offs] & 0x0ff) | (data << 8);
		return;

		case 0x14:
		case 0x24:
		case 0x34:
			vidramoffs[offs] = (vidramoffs[offs] & 0xff00) | (data);
		return;

		case 0x15:
		case 0x25:
		case 0x35:
			vidramoffs[offs] = (vidramoffs[offs] & 0x00ff) | (data << 8);
		return;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43: // exscroll
		return;

		case 0x5a:
			wardner_coin_write(data);
		return;

		case 0x5c:
			control_write(data);
		return;

		case 0x60:
		case 0x61:
			DrvTxRAM[(((vidramoffs[0]*2)+(port & 1)) & 0x0fff)] = data;
		return;

		case 0x62:
		case 0x63:
			DrvBgRAM[(((vidramoffs[1]*2)+(port & 1)) & 0x1fff) + bgrambank] = data;
		return;

		case 0x64:
		case 0x65:
			DrvFgRAM[(((vidramoffs[2]*2)+(port & 1)) & 0x1fff)] = data;
		return;

		case 0x70:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall wardner_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x50:
			return DrvDips[0];

		case 0x52:
			return DrvDips[1];

		case 0x54:
			return DrvInputs[0];

		case 0x56:
			return DrvInputs[1];

		case 0x58:
			return ((DrvInputs[2] & 0x7f) & ~coin_lockout) | (vblank ? 0x80 : 0);

		case 0x60:
		case 0x61:
			return DrvTxRAM[(((vidramoffs[0]*2)+(port & 1)) & 0x0fff)];

		case 0x62:
		case 0x63:
			return DrvBgRAM[(((vidramoffs[1]*2)+(port & 1)) & 0x1fff) + bgrambank];

		case 0x64:
		case 0x65:
			return DrvFgRAM[(((vidramoffs[2]*2)+(port & 1)) & 0x1fff)];
	}

	return 0;
}

static void __fastcall wardner_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3812Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall wardner_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM3812Read(0, port & 1);
	}

	return 0;
}

static void twincobr_dsp_bio_w(UINT16 data)
{
	if (data & 0x8000) {
		dsp_BIO = 0;
	}

	if (data == 0) {
		if (dsp_execute) {
			z80_halt = 0;
			dsp_execute = 0;
			//tms32010RunEnd();
		}

		dsp_BIO = 1;
	}
}

static UINT8 twincobr_BIO_r()
{
	return dsp_BIO;
}

static void wardner_dsp_addrsel_w(UINT16 data)
{
	main_ram_seg =  (data & 0xe000);
	dsp_addr_w   = ((data & 0x07ff) << 1);

	if (main_ram_seg == 0x6000) main_ram_seg = 0x7000;
}

static UINT16 wardner_dsp_r()
{
	switch (main_ram_seg)
	{
		case 0x7000:
		case 0x8000:
		case 0xa000:
			return ZetReadByte(main_ram_seg + dsp_addr_w) | (ZetReadByte(main_ram_seg + dsp_addr_w + 1) << 8);
	}

	return 0;
}

static void wardner_dsp_w(UINT16 data)
{
	dsp_execute = 0;

	switch (main_ram_seg)
	{
		case 0x7000:    if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1;
		case 0x8000:
		case 0xa000:
			ZetWriteByte(main_ram_seg + dsp_addr_w + 0, data & 0xff);
			ZetWriteByte(main_ram_seg + dsp_addr_w + 1, data >> 8);
		return;
	}
}

static void dsp_write(INT32 port, UINT16 data)
{
	switch (port)
	{
		case 0x00: wardner_dsp_addrsel_w(data); return;
		case 0x01: wardner_dsp_w(data); return;
		case 0x03: twincobr_dsp_bio_w(data); return;
	}
	//bprintf (0, _T("DSPWP: %2.2x, %4.4x\n"), port&0xff, data);
}

static UINT16 dsp_read(INT32 port)
{
	switch (port)
	{
		case 0x01: return wardner_dsp_r();
		case 0x10: return twincobr_BIO_r();
	}
	//bprintf (0, _T("DSPRP: %2.2x\n"), port&0xff);

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate / 3500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	tms32010_reset();

	z80_halt = 0;

	coin_lockout = 0;
	previous_coin = 0;

	irq_enable = 0;
	flipscreen = 0;
	bgrambank = 0;
	fgrombank = 0;
	displayenable = 1;

	main_ram_seg = 0;
	dsp_addr_w = 0;
	dsp_execute = 0;
	dsp_BIO = 0;
	dsp_on = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x040000;
	DrvMCUROM		= Next; Next += 0x004000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	pTempDraw		= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(UINT16);

	AllRam			= Next;

	DrvSprBuf		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvMCURAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvShareRAM		= Next; Next += 0x000800;

	DrvBgRAM		= Next; Next += 0x004000;
	DrvFgRAM		= Next; Next += 0x002000;
	DrvTxRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { (0x4000*8*0), (0x4000*8*1), (0x4000*8*2) };
	INT32 Plane1[4] = { (0x8000*8*0), (0x8000*8*1), (0x8000*8*2), (0x8000*8*3) };
	INT32 Plane2[4] = { (0x10000*8*0), (0x10000*8*1), (0x10000*8*2), (0x10000*8*3) };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	GfxDecode(0x0800, 3, 8, 8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x1000, 4, 8, 8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);
	memcpy (DrvGfxROM1 + 0x40000, DrvGfxROM1, 0x40000); // mirror

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x1000, 4, 8, 8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs, YOffs1, 0x100, tmp, DrvGfxROM3);

	BurnFree(tmp);

	return 0;
}

static INT32 LoadNibbles(UINT8 *dst, INT32 idx, INT32 len)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(len*2);

	if (BurnLoadRom(dst + 0, idx + 1, 2)) return 1;
	if (BurnLoadRom(dst + 1, idx + 3, 2)) return 1;
	if (BurnLoadRom(tmp + 0, idx + 0, 2)) return 1;
	if (BurnLoadRom(tmp + 1, idx + 2, 2)) return 1;

	for (INT32 i = 0; i < len * 2; i++) {
		dst[i] = (dst[i] & 0xf) | (tmp[i] << 4);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		memset (DrvZ80ROM0, 0xff, 0x40000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x38000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;

		if (LoadNibbles(DrvMCUROM +  0x00000,  5, 0x0400)) return 1;
		if (LoadNibbles(DrvMCUROM +  0x00800,  9, 0x0400)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000, 19, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 23, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 27, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x6fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x7000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x8000, 0x8fff, MAP_WRITE);
//	ZetMapMemory(DrvPalRAM,		0xa000, 0xafff, MAP_WRITE);
	ZetMapMemory(DrvShareRAM,	0xc000, 0xc7ff, MAP_WRITE);
	ZetSetWriteHandler(wardner_main_write);
	ZetSetOutHandler(wardner_main_write_port);
	ZetSetInHandler(wardner_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x8000, 0x80ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1 + 0x100,0xc800, 0xcfff, MAP_RAM);
	ZetSetOutHandler(wardner_sound_write_port);
	ZetSetInHandler(wardner_sound_read_port);
	ZetClose();

	tms32010_init();
	tms32010_set_write_port_handler(dsp_write);
	tms32010_set_read_port_handler(dsp_read);
	tms32010_ram = (UINT16*)DrvMCURAM;
	tms32010_rom = (UINT16*)DrvMCUROM;

	BurnYM3812Init(1, 3500000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(3500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();

	ZetExit();

	tms32010_exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0xe00; i+=2) {
		INT32 p = *((UINT16*)(DrvPalRAM + i));

		INT32 r = (p >>  0) & 0x1f;
		INT32 g = (p >>  5) & 0x1f;
		INT32 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i/2] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer(INT32 layer, INT32 rambank, INT32 rombank)
{
	UINT16 *ram[3] = { (UINT16*)DrvTxRAM, (UINT16*)DrvBgRAM, (UINT16*)DrvFgRAM };
	UINT8 *gfx[3] = { DrvGfxROM0, DrvGfxROM2, DrvGfxROM1 };
	INT32 colbank[3] = { 0x600, 0x400, 0x500 };
	INT32 colshift[3] = { 11, 12, 12 };
	INT32 depth = colshift[layer] - 8;

	INT32 transp = (layer & 1) ? 0xff : 0;

	INT32 height = (layer ? 64 : 32) * 8;

	INT32 xscroll = (scrollx[layer] + 55 ) & 0x1ff;
	INT32 yscroll = (scrolly[layer] + 30) & (height - 1);

	for (INT32 offs = 0; offs < 64 * (height/8); offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= xscroll;
		if (sx < -7) sx += 512;
		sy -= yscroll;
		if (sy < -7) sy += height;

		INT32 attr  = ram[layer][offs + (rambank/2)];
		INT32 color = attr >> colshift[layer];
		INT32 code  = (attr & ((1 << colshift[layer])-1)) + rombank;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, depth, transp, colbank[layer], gfx[layer]);
	}
}

static void predraw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	INT32 xoffs = 31;
	INT32 xoffs_flipped = 15;

	memset (pTempDraw, 0, nScreenWidth * nScreenHeight * sizeof(UINT16));

	for (INT32 offs = 0; offs < 0x1000/2; offs += 4)
	{
		INT32 attr = ram[offs + 1];
		INT32 prio = (attr >> 10) & 3;
		if (prio == 0) continue;

		INT32 sy = ram[offs + 3] >> 7;

		if (sy != 0x0100)
		{
			INT32 code  = ram[offs] & 0x7ff;
			INT32 color = (attr & 0x3f) | ((attr >> 4) & 0xc0);

			INT32 sx    = ram[offs + 2] >> 7;
			INT32 flipx = attr & 0x100;
			if (flipx) sx -= xoffs_flipped;

			INT32 flipy = attr & 0x200;

			sx -= xoffs;
			sy -= 16;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				}
			}
		}
	}
}

static void draw_sprites(INT32 priority)
{
	priority <<= 10;

	for (INT32 y = 0;y < nScreenHeight; y++)
	{
		UINT16* src = pTempDraw + y * nScreenWidth;
		UINT16* dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0;x < nScreenWidth; x++)
		{
			UINT16 pix = src[x];

			if (pix & 0xf)
			{
				if ((pix & 0xc00) == priority)
				{
					dst[x] = pix & 0x3ff;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (displayenable)
	{
		predraw_sprites();

		draw_layer(1, bgrambank, 0);
		draw_sprites(1);
		draw_layer(2, 0, fgrombank);
		draw_sprites(2);
		draw_layer(0, 0, 0);
		draw_sprites(3);
	}

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
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// 1 coin per frame.
		if (DrvInputs[2] & 0x18 && previous_coin & 0x18) {
			DrvInputs[2] &= ~0x18;
		} else previous_coin = DrvInputs[2];
	}

	INT32 nInterleave = 286;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 3500000 / 60, 14000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		if (z80_halt) {
			nCyclesDone[0] += nSegment;
			ZetIdle(nSegment);
		} else {
			nCyclesDone[0] += ZetRun(nSegment);

			if (i == 240 && irq_enable) {
				irq_enable = 0;
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // or hold?
			}
		}

		nSegment = nCyclesTotal[2] / nInterleave;
		if (dsp_on) tms32010_execute(nSegment);

		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
		ZetClose();

		if (i == 240) {
			if (pBurnDraw) {
				DrvDraw();
			}

			memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

			vblank = 1;
		}
	}

	ZetOpen(1);

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM)
	{	
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.nAddress	= 0;
		ba.szName	= "All RAM";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA)
	{
		ZetScan(nAction);
		tms32010_scan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(z80_halt);
		SCAN_VAR(irq_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bgrambank);
		SCAN_VAR(fgrombank);
		SCAN_VAR(displayenable);
		SCAN_VAR(main_ram_seg);
		SCAN_VAR(dsp_addr_w);
		SCAN_VAR(dsp_execute);
		SCAN_VAR(dsp_BIO);
		SCAN_VAR(main_bank);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			bankswitch(main_bank);
			ZetClose();
		}

	}

	return 0;
}


// Wardner (World)

static struct BurnRomInfo wardnerRomDesc[] = {
	{ "wardner.17",	0x08000, 0xc5dd56fd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "b25-18.rom",	0x10000, 0x9aab8ee2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b25-19.rom",	0x10000, 0x95b68813, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wardner.20",	0x08000, 0x347f411b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b25-16.rom",	0x08000, 0xe5202ff8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 code

	{ "82s137.1d",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  5 tms32010 code
	{ "82s137.1e",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137.3d",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137.3e",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s131.3b",	0x00200, 0x9dfffaff, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s131.3a",	0x00200, 0x712bad47, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "82s131.2a",	0x00200, 0xac843ca6, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "82s131.1a",	0x00200, 0x50452ff8, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "wardner.07",	0x04000, 0x1392b60d, 4 | BRF_GRA },           // 13 Text characters
	{ "wardner.06",	0x04000, 0x0ed848da, 4 | BRF_GRA },           // 14
	{ "wardner.05",	0x04000, 0x79792c86, 4 | BRF_GRA },           // 15

	{ "b25-12.rom",	0x08000, 0x15d08848, 5 | BRF_GRA },           // 16 Background tiles
	{ "b25-15.rom",	0x08000, 0xcdd2d408, 5 | BRF_GRA },           // 17
	{ "b25-14.rom",	0x08000, 0x5a2aef4f, 5 | BRF_GRA },           // 18
	{ "b25-13.rom",	0x08000, 0xbe21db2b, 5 | BRF_GRA },           // 19

	{ "b25-08.rom",	0x08000, 0x883ccaa3, 6 | BRF_GRA },           // 20 Foreground tiles
	{ "b25-11.rom",	0x08000, 0xd6ebd510, 6 | BRF_GRA },           // 21
	{ "b25-10.rom",	0x08000, 0xb9a61e81, 6 | BRF_GRA },           // 22
	{ "b25-09.rom",	0x08000, 0x585411b7, 6 | BRF_GRA },           // 23

	{ "b25-01.rom",	0x10000, 0x42ec01fb, 7 | BRF_GRA },           // 24 Sprites
	{ "b25-02.rom",	0x10000, 0x6c0130b7, 7 | BRF_GRA },           // 25
	{ "b25-03.rom",	0x10000, 0xb923db99, 7 | BRF_GRA },           // 26
	{ "b25-04.rom",	0x10000, 0x8059573c, 7 | BRF_GRA },           // 27

	{ "82s129.b19",	0x00100, 0x24e7d62f, 8 | BRF_GRA },           // 28 Proms (not used)
	{ "82s129.b18",	0x00100, 0xa50cef09, 8 | BRF_GRA },           // 29
	{ "82s123.b21",	0x00020, 0xf72482db, 8 | BRF_GRA },           // 30
	{ "82s123.c6",	0x00020, 0xbc88cced, 8 | BRF_GRA },           // 31
	{ "82s123.f1",	0x00020, 0x4fb5df2a, 8 | BRF_GRA },           // 32
};

STD_ROM_PICK(wardner)
STD_ROM_FN(wardner)

struct BurnDriver BurnDrvWardner = {
	"wardner", NULL, NULL, NULL, "1987",
	"Wardner (World)\0", NULL, "Toaplan / Taito Corporation Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TOAPLAN_MISC, GBF_PLATFORM, 0,
	NULL, wardnerRomInfo, wardnerRomName, NULL, NULL, WardnerInputInfo, WardnerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	320, 240, 4, 3
};


// Pyros (US)

static struct BurnRomInfo pyrosRomDesc[] = {
	{ "b25-29.rom",	0x08000, 0xb568294d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "b25-18.rom",	0x10000, 0x9aab8ee2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b25-19.rom",	0x10000, 0x95b68813, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b25-30.rom",	0x08000, 0x5056c799, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b25-16.rom",	0x08000, 0xe5202ff8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 code

	{ "82s137.1d",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  5 tms32010 code
	{ "82s137.1e",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137.3d",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137.3e",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s131.3b",	0x00200, 0x9dfffaff, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s131.3a",	0x00200, 0x712bad47, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "82s131.2a",	0x00200, 0xac843ca6, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "82s131.1a",	0x00200, 0x50452ff8, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "b25-35.rom",	0x04000, 0xfec6f0c0, 4 | BRF_GRA },           // 13 Text characters
	{ "b25-34.rom",	0x04000, 0x02505dad, 4 | BRF_GRA },           // 14
	{ "b25-33.rom",	0x04000, 0x9a55fcb9, 4 | BRF_GRA },           // 15

	{ "b25-12.rom",	0x08000, 0x15d08848, 5 | BRF_GRA },           // 16 Background tiles
	{ "b25-15.rom",	0x08000, 0xcdd2d408, 5 | BRF_GRA },           // 17
	{ "b25-14.rom",	0x08000, 0x5a2aef4f, 5 | BRF_GRA },           // 18
	{ "b25-13.rom",	0x08000, 0xbe21db2b, 5 | BRF_GRA },           // 19

	{ "b25-08.rom",	0x08000, 0x883ccaa3, 6 | BRF_GRA },           // 20 Foreground tiles
	{ "b25-11.rom",	0x08000, 0xd6ebd510, 6 | BRF_GRA },           // 21
	{ "b25-10.rom",	0x08000, 0xb9a61e81, 6 | BRF_GRA },           // 22
	{ "b25-09.rom",	0x08000, 0x585411b7, 6 | BRF_GRA },           // 23

	{ "b25-01.rom",	0x10000, 0x42ec01fb, 7 | BRF_GRA },           // 24 Sprites
	{ "b25-02.rom",	0x10000, 0x6c0130b7, 7 | BRF_GRA },           // 25
	{ "b25-03.rom",	0x10000, 0xb923db99, 7 | BRF_GRA },           // 26
	{ "b25-04.rom",	0x10000, 0x8059573c, 7 | BRF_GRA },           // 27

	{ "82s129.b19",	0x00100, 0x24e7d62f, 8 | BRF_GRA },           // 28 Proms (not used)
	{ "82s129.b18",	0x00100, 0xa50cef09, 8 | BRF_GRA },           // 29
	{ "82s123.b21",	0x00020, 0xf72482db, 8 | BRF_GRA },           // 30
	{ "82s123.c6",	0x00020, 0xbc88cced, 8 | BRF_GRA },           // 31
	{ "82s123.f1",	0x00020, 0x4fb5df2a, 8 | BRF_GRA },           // 32
};

STD_ROM_PICK(pyros)
STD_ROM_FN(pyros)

struct BurnDriver BurnDrvPyros = {
	"pyros", "wardner", NULL, NULL, "1987",
	"Pyros (US)\0", NULL, "Toaplan / Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TOAPLAN_MISC, GBF_PLATFORM, 0,
	NULL, pyrosRomInfo, pyrosRomName, NULL, NULL, WardnerInputInfo, PyrosDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	320, 240, 4, 3
};


// Wardner no Mori (Japan)

static struct BurnRomInfo wardnerjRomDesc[] = {
	{ "b25-17.bin",	0x08000, 0x4164dca9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "b25-18.rom",	0x10000, 0x9aab8ee2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b25-19.rom",	0x10000, 0x95b68813, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b25-20.bin",	0x08000, 0x1113ad38, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b25-16.rom",	0x08000, 0xe5202ff8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 code

	{ "82s137.1d",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  5 tms32010 code
	{ "82s137.1e",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137.3d",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137.3e",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s131.3b",	0x00200, 0x9dfffaff, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s131.3a",	0x00200, 0x712bad47, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "82s131.2a",	0x00200, 0xac843ca6, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "82s131.1a",	0x00200, 0x50452ff8, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "b25-07.bin",	0x04000, 0x50e329e0, 4 | BRF_GRA },           // 13 Text characters
	{ "b25-06.bin",	0x04000, 0x3bfeb6ae, 4 | BRF_GRA },           // 14
	{ "b25-05.bin",	0x04000, 0xbe36a53e, 4 | BRF_GRA },           // 15

	{ "b25-12.rom",	0x08000, 0x15d08848, 5 | BRF_GRA },           // 16 Background tiles
	{ "b25-15.rom",	0x08000, 0xcdd2d408, 5 | BRF_GRA },           // 17
	{ "b25-14.rom",	0x08000, 0x5a2aef4f, 5 | BRF_GRA },           // 18
	{ "b25-13.rom",	0x08000, 0xbe21db2b, 5 | BRF_GRA },           // 19

	{ "b25-08.rom",	0x08000, 0x883ccaa3, 6 | BRF_GRA },           // 20 Foreground tiles
	{ "b25-11.rom",	0x08000, 0xd6ebd510, 6 | BRF_GRA },           // 21
	{ "b25-10.rom",	0x08000, 0xb9a61e81, 6 | BRF_GRA },           // 22
	{ "b25-09.rom",	0x08000, 0x585411b7, 6 | BRF_GRA },           // 23

	{ "b25-01.rom",	0x10000, 0x42ec01fb, 7 | BRF_GRA },           // 24 Sprites
	{ "b25-02.rom",	0x10000, 0x6c0130b7, 7 | BRF_GRA },           // 25
	{ "b25-03.rom",	0x10000, 0xb923db99, 7 | BRF_GRA },           // 26
	{ "b25-04.rom",	0x10000, 0x8059573c, 7 | BRF_GRA },           // 27

	{ "82s129.b19",	0x00100, 0x24e7d62f, 8 | BRF_GRA },           // 28 Proms (not used)
	{ "82s129.b18",	0x00100, 0xa50cef09, 8 | BRF_GRA },           // 29
	{ "82s123.b21",	0x00020, 0xf72482db, 8 | BRF_GRA },           // 30
	{ "82s123.c6",	0x00020, 0xbc88cced, 8 | BRF_GRA },           // 31
	{ "82s123.f1",	0x00020, 0x4fb5df2a, 8 | BRF_GRA },           // 32
};

STD_ROM_PICK(wardnerj)
STD_ROM_FN(wardnerj)

struct BurnDriver BurnDrvWardnerj = {
	"wardnerj", "wardner", NULL, NULL, "1987",
	"Wardner no Mori (Japan)\0", NULL, "Toaplan / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TOAPLAN_MISC, GBF_PLATFORM, 0,
	NULL, wardnerjRomInfo, wardnerjRomName, NULL, NULL, WardnerInputInfo, WardnerjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	320, 240, 4, 3
};
