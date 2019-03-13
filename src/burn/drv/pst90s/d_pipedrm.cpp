// FB Alpha Pipe Dream & Hatris driver module
// Based on MAME driver by Bryan McPhail and Aaron Giles

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2610.h"
#include "burn_ym2608.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 pending_command;
static UINT8 soundlatch;
static UINT8 z80_bank[2];
static UINT8 scroll[4];
static UINT8 flipscreen;
static UINT8 crtc_register;
static INT32 crtc_timer = 0;
static INT32 crtc_timer_enable = 0;

static INT32 nmi_enable; // 0-hatris/1-pipedream

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo PipedrmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pipedrm)

static struct BurnInputInfo HatrisInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hatris)

static struct BurnDIPInfo PipedrmDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    15, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x06, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "6 Coins/4 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x02, "5 Coins/6 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    15, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x60, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "6 Coins/4 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x20, "5 Coins/6 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Super"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x0c, 0x0c, "1"			},
	{0x13, 0x01, 0x0c, 0x08, "2"			},
	{0x13, 0x01, 0x0c, 0x04, "3"			},
	{0x13, 0x01, 0x0c, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x10, 0x00, "Off"			},
	{0x13, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x20, 0x20, "Off"			},
	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Training Mode"	},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Pipedrm)

static struct BurnDIPInfo HatrisDIPList[]=
{
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    15, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x09, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "6 Coins/4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "5 Coins/6 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    15, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x90, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "6 Coins/4 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "5 Coins/6 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "4 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Hat Fall Velocity"	},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Super"		},

	{0   , 0xfe, 0   ,    4, "End Line Position"	},
	{0x15, 0x01, 0x0c, 0x04, "Easy"			},
	{0x15, 0x01, 0x0c, 0x00, "Normal"		},
	{0x15, 0x01, 0x0c, 0x08, "Hard"			},
	{0x15, 0x01, 0x0c, 0x0c, "Super"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x10, 0x00, "Off"			},
	{0x15, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x20, 0x00, "Off"			},
	{0x15, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Hatris)

static void main_bankswitch(UINT8 data)
{
	z80_bank[0] = data;

	flipscreen = data & 0x40;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + ((data & 7) * 0x2000),	0xa000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM + ((data >> 3) & 1) * 0x3000,		0xd000, 0xffff, MAP_RAM); 
}

static void __fastcall pipedrm_main_write_port(UINT16 port, UINT8 data)
{
//	bprintf (0, _T("MW: %4.4x, %2.2x\n"), port,data);

	switch (port & 0xff)
	{
		case 0x00:
			if (crtc_register == 0x0b) {
				crtc_timer_enable = 1;
				if (data > 0x80) {
					crtc_timer = 0x7f;
				} else {
					crtc_timer = 0xff;
				}
			}
		return;

		case 0x11:
			crtc_register = data;
		return;

		case 0x20:
			pending_command = 1;
			soundlatch = data;
			if (nmi_enable) {
				ZetNmi(1);
			}
		return;

		case 0x21:
			if (nmi_enable == 0) data = ((~data & 2) << 2) | ((data & 1) << 6);
			main_bankswitch(data);
		return;

		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
			scroll[(port & 0xff) - 0x22] = data;
		return;
	}
}

static UINT8 __fastcall pipedrm_main_read_port(UINT16 port)
{
//	bprintf (0, _T("MR: %4.4x\n"), port);

	switch (port & 0xff)
	{
		case 0x20:
			return DrvInputs[0];

		case 0x21:
			return DrvInputs[1];

		case 0x22:
			return DrvDips[0];

		case 0x23:
			return DrvDips[1];

		case 0x24:
			return DrvInputs[2];

		case 0x25:
			return pending_command;
	}

	return 0;
}

static void sound_bankswitch(UINT8 data)
{
	z80_bank[1] = data;

	ZetMapMemory(DrvZ80ROM1 + 0x10000 + ((data & 1) * 0x8000),	0x8000, 0xffff, MAP_ROM);
}

static void __fastcall pipedrm_sound_write_port(UINT16 port, UINT8 data)
{
//	bprintf (0, _T("SW: %4.4x, %2.2x\n"), port,data);

	switch (port & 0xff)
	{
		case 0x00: // hatris
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b: if (!nmi_enable)
			BurnYM2608Write(port & 0x03, data);
		return;

		case 0x04: // pipedrm
			sound_bankswitch(data);
		return;

		case 0x05: // hatris
		case 0x17: // pipedrm
			pending_command = 0;
			// clear nmi!!
		return;

		case 0x18: // pipedrm
		case 0x19:
		case 0x1a:
		case 0x1b: if (nmi_enable)
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall pipedrm_sound_read_port(UINT16 port)
{
//	bprintf (0, _T("SR: %4.4x\n"), port);

	switch (port & 0xff)
	{
		case 0x00: // hatris
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return (!nmi_enable) ? BurnYM2608Read(port & 3) : 0;


		case 0x04:
		case 0x16:
			return soundlatch;

		case 0x05:
			return pending_command;

		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			return (nmi_enable) ? BurnYM2610Read(port & 3) : 0;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[0x0000 + offs];
	INT32 code = ((attr & 0x80) << 9) | (DrvVidRAM[0x1000 + offs] << 8) | DrvVidRAM[0x2000 + offs];

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvVidRAM[0x3000 + offs];
	INT32 code = ((attr & 0x80) << 9) | (DrvVidRAM[0x4000 + offs] << 8) | DrvVidRAM[0x5000 + offs];

	TILE_SET_INFO(1, code, attr, 0);
}

static void DrvFMIRQHandler(INT32, INT32 status)
{
	ZetSetIRQLine(0, (status & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE );
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	main_bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	sound_bankswitch(0);
	ZetReset();
	if (nmi_enable) {
		BurnYM2610Reset();
	} else {
		BurnYM2608Reset();
	}
	ZetClose();

	memset (scroll, 0, 4);
	soundlatch = 0;
	pending_command = 0;
	crtc_register = 0;
	crtc_timer = 0;
	crtc_timer_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x200000;
	DrvGfxROM2		= Next; Next += 0x100000;

	DrvSndROM0		= Next; Next += 0x080000;
	DrvSndROM1		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x006000;
	DrvSprRAM 		= Next + 0xc00;		// DrvPalRAM + 0xc00
	DrvPalRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]   = { STEP4(0,1) };
	INT32 XOffs0[8]  = { 4, 0, 12, 8, 20, 16, 28, 24 };
	INT32 YOffs0[4]  = { STEP4(0,32) };
	INT32 XOffs1[16] = { 12, 8, 28, 24, 4, 0, 20, 16, 44, 40, 60, 56, 36, 32, 52, 48 };
	INT32 YOffs1[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x100000);

	GfxDecode(0x10000, 4,  8,  4, Plane, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x10000, 4,  8,  4, Plane, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x01000, 4, 16, 16, Plane, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game_select)
	{   // pipedrm
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x010000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x010000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x80000, DrvGfxROM0 + 0x80000, 0x80000);

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 10, 1)) return 1;

		nmi_enable = 1;
	}
	else
	{   // hatris
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000,  3, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x80000, DrvGfxROM0, 0x80000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000,  5, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x40000, DrvGfxROM1, 0x40000);
		memcpy (DrvGfxROM1 + 0x80000, DrvGfxROM1, 0x40000);
		memcpy (DrvGfxROM1 + 0xc0000, DrvGfxROM1, 0x40000);

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 0x80, 1)) return 1;

		nmi_enable = 0;
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
//	ZetMapMemory(DrvZ80ROM0,	0xa000, 0xbfff, MAP_ROM); // banked
	ZetMapMemory(DrvPalRAM,		0xc000, 0xcfff, MAP_RAM);
//	ZetMapMemory(DrvVidRAM,		0xd000, 0xffff, MAP_RAM); // banked
	ZetSetOutHandler(pipedrm_main_write_port);
	ZetSetInHandler(pipedrm_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x7800, 0x7fff, MAP_RAM);
//	ZetMapMemory(DrvZ80ROM1,	0x8000, 0xffff, MAP_ROM); // banked
	ZetSetOutHandler(pipedrm_sound_write_port);
	ZetSetInHandler(pipedrm_sound_read_port);
	ZetClose();

	if (nmi_enable) {
		INT32 nSndROMLen0 = 0x80000;
		INT32 nSndROMLen1 = 0x80000;
		BurnYM2610Init(8000000, DrvSndROM0, &nSndROMLen0, DrvSndROM1, &nSndROMLen1, &DrvFMIRQHandler, 0);
		BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
		BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
		BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);
	} else {
		INT32 nSndROMLen0 = 0x20000;
		BurnYM2608Init(8000000, DrvSndROM0, &nSndROMLen0, DrvSndROM1, &DrvFMIRQHandler, 0);
		BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
		BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
		BurnYM2608SetRoute(BURN_SND_YM2608_AY8910_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);
	}

	BurnTimerAttachZet(3579500);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 4, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 4, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 4, 0x200000, 0, 0x7f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 4, 0x200000, 0, 0x7f);
	GenericTilemapSetTransparent(1, 0xf);

	GenericTilemapSetOffsets(TMAP_GLOBAL, game_select ? 0 : -256, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetOpen(1);
	if (nmi_enable) {
		BurnYM2610Exit();
	} else {
		BurnYM2608Exit();
	}
	ZetClose();

	ZetExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT16 *p = (UINT16*)DrvPalRAM;

		UINT8 r = (p[i] >> 10) & 0x1f;
		UINT8 g = (p[i] >>  5) & 0x1f;
		UINT8 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 pri_param)
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 first = (4 * ram[0x1fe]) & 0x1ff;
	if (first > 0x1fc) first = 0x1fc;

	for (INT32 offs = first; offs != 0x1fc; offs += 4)
	{
		if ((ram[offs+2] & 0x0080) == 0)	// sprite disabled
			continue;

		INT32 oy    =  (ram[offs+0] & 0x01ff) - 6;
		INT32 zoomy =  (ram[offs+0] & 0xf000) >> 12;
		INT32 ox =     (ram[offs+1] & 0x01ff) - 13;
		INT32 zoomx =  (ram[offs+1] & 0xf000) >> 12;
		INT32 xsize =  (ram[offs+2] & 0x0700) >> 8;
		INT32 flipx =  (ram[offs+2] & 0x0800);
		INT32 ysize =  (ram[offs+2] & 0x7000) >> 12;
		INT32 flipy =  (ram[offs+2] & 0x8000);
		INT32 color = ((ram[offs+2] & 0x000f) << 4) + 0x400;
		INT32 pri =    (ram[offs+2] & 0x0010) >> 4;
		INT32 map =    (ram[offs+3]);

		if (pri != pri_param)
			continue;

		zoomx = 32 - zoomx;
		zoomy = 32 - zoomy;

		for (INT32 y = 0; y <= ysize; y++)
		{
			INT32 sx, sy;

			if (flipy)
				sy = ((oy + zoomy * (ysize - y)/2 + 16) & 0x1ff) - 16;
			else
				sy = ((oy + zoomy * y / 2 + 16) & 0x1ff) - 16;

			for (INT32 x = 0; x <= xsize; x++, map++)
			{
				if (flipx)
					sx = ((ox + zoomx * (xsize - x) / 2 + 16) & 0x1ff) - 16;
				else
					sx = ((ox + zoomx * x / 2 + 16) & 0x1ff) - 16;

				RenderZoomedTile(pTransDraw, DrvGfxROM2, map&0xfff, color, 0xf, sx-0x000, sy-0x000, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
				RenderZoomedTile(pTransDraw, DrvGfxROM2, map&0xfff, color, 0xf, sx-0x200, sy-0x000, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
				RenderZoomedTile(pTransDraw, DrvGfxROM2, map&0xfff, color, 0xf, sx-0x000, sy-0x200, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
				RenderZoomedTile(pTransDraw, DrvGfxROM2, map&0xfff, color, 0xf, sx-0x200, sy-0x200, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
			}

			if (xsize == 2) map += 1;
			if (xsize == 4) map += 3;
			if (xsize == 5) map += 2;
			if (xsize == 6) map += 1;
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	//BurnTransferClear();

	GenericTilemapSetScrollY(0, scroll[1] - 0xf9);
	GenericTilemapSetScrollY(1, scroll[3] - 0xf9);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 4) draw_sprites(0);
	if (nBurnLayer & 8) draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 FromanceDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	//BurnTransferClear();

	GenericTilemapSetScrollX(0, scroll[2] - 0x1f7);
	GenericTilemapSetScrollY(0, scroll[3] - 0xf9);
	GenericTilemapSetScrollX(1, scroll[0] - 0x1f7);
	GenericTilemapSetScrollY(1, scroll[1] - 0xf9);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
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
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3579500 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == nInterleave-1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
		if (crtc_timer_enable) {
			if ((i & crtc_timer) == crtc_timer) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		}
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		if (nmi_enable) {
			BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2608Update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	ZetClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

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

	if (nAction & ACB_DRIVER_DATA)
	{
		ZetScan(nAction);

		ZetOpen(0);
		if (nmi_enable) {
			BurnYM2610Scan(nAction, pnMin);
		} else {
			BurnYM2608Scan(nAction, pnMin);
		}
		ZetClose();

		SCAN_VAR(pending_command);
		SCAN_VAR(soundlatch);
		SCAN_VAR(z80_bank);
		SCAN_VAR(scroll);
		// flipscreen & vrambank saved in z80 bank
		SCAN_VAR(crtc_register);
		SCAN_VAR(crtc_timer);
		SCAN_VAR(crtc_timer_enable);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		main_bankswitch(z80_bank[0]);
		ZetClose();

		ZetOpen(1);
		sound_bankswitch(z80_bank[1]);
		ZetClose();
	}

	return 0;
}


// YM2608 ROM
static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo Ym2608RomDesc[] = {
#if !defined (ROM_VERIFY)
	{ "ym2608_adpcm_rom.bin",  0x002000, 0x23c9e0d8, BRF_ESS | BRF_PRG | BRF_BIOS },
#else
	{ "",  0x000000, 0x00000000, BRF_ESS | BRF_PRG | BRF_BIOS },
#endif
};

// Pipe Dream (World)

static struct BurnRomInfo pipedrmRomDesc[] = {
	{ "ya.u129",		0x08000, 0x9b4d84a2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "yb.u110",		0x10000, 0x7416554a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u4.u86",		0x08000, 0x497fad4c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "u3.u99",		0x10000, 0x4800322a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "s73",		0x80000, 0x63f4e10c, 3 | BRF_GRA },           //  4 Background tiles
	{ "s72",		0x80000, 0x4e669e97, 3 | BRF_GRA },           //  5

	{ "s71",		0x80000, 0x431485ee, 4 | BRF_GRA },           //  6 Foreground tiles

	{ "a30.u12",		0x40000, 0x50bc5e98, 5 | BRF_GRA },           //  7 Sprites
	{ "a29.u2",		0x40000, 0xa240a448, 5 | BRF_GRA },           //  8

	{ "g71.u118",		0x80000, 0x488e2fd1, 6 | BRF_SND },           //  9 YM2610 Samples

	{ "g72.u83",		0x80000, 0xdc3d14be, 7 | BRF_SND },           // 10 YM2610 Delta-T Samples

	{ "palce16v8h.114",	0x00117, 0x1f3a3816, 8 | BRF_OPT },           // 11 plds
	{ "gal16v8a.115",	0x00117, 0x2b32e239, 8 | BRF_OPT },           // 12
	{ "gal16v8a.116",	0x00117, 0x3674f043, 8 | BRF_OPT },           // 13
	{ "gal16v8a.127",	0x00117, 0x7115d95c, 8 | BRF_OPT },           // 14
};

STD_ROM_PICK(pipedrm)
STD_ROM_FN(pipedrm)

static INT32 pipedrmInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvPipedrm = {
	"pipedrm", NULL, NULL, NULL, "1990",
	"Pipe Dream (World)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pipedrmRomInfo, pipedrmRomName, NULL, NULL, NULL, NULL, PipedrmInputInfo, PipedrmDIPInfo,
	pipedrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Pipe Dream (US)

static struct BurnRomInfo pipedrmuRomDesc[] = {
	{ "01.u129",		0x08000, 0x9fe261fb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "02.u110",		0x10000, 0xc8209b67, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u4.u86",		0x08000, 0x497fad4c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "u3.u99",		0x10000, 0x4800322a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "s73",		0x80000, 0x63f4e10c, 3 | BRF_GRA },           //  4 Background tiles
	{ "s72",		0x80000, 0x4e669e97, 3 | BRF_GRA },           //  5

	{ "s71",		0x80000, 0x431485ee, 4 | BRF_GRA },           //  6 Foreground tiles

	{ "a30.u12",		0x40000, 0x50bc5e98, 5 | BRF_GRA },           //  7 Sprites
	{ "a29.u2",		0x40000, 0xa240a448, 5 | BRF_GRA },           //  8

	{ "g71.u118",		0x80000, 0x488e2fd1, 6 | BRF_SND },           //  9 YM2610 Samples

	{ "g72.u83",		0x80000, 0xdc3d14be, 7 | BRF_SND },           // 10 YM2610 Delta-T Samples

	{ "palce16v8h.114",	0x00117, 0x1f3a3816, 8 | BRF_OPT },           // 11 plds
	{ "gal16v8a.115",	0x00117, 0x2b32e239, 8 | BRF_OPT },           // 12
	{ "gal16v8a.116",	0x00117, 0x3674f043, 8 | BRF_OPT },           // 13
	{ "gal16v8a.127",	0x00117, 0x7115d95c, 8 | BRF_OPT },           // 14
};

STD_ROM_PICK(pipedrmu)
STD_ROM_FN(pipedrmu)

struct BurnDriver BurnDrvPipedrmu = {
	"pipedrmu", "pipedrm", NULL, NULL, "1990",
	"Pipe Dream (US)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pipedrmuRomInfo, pipedrmuRomName, NULL, NULL, NULL, NULL, PipedrmInputInfo, PipedrmDIPInfo,
	pipedrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Pipe Dream (Japan)

static struct BurnRomInfo pipedrmjRomDesc[] = {
	{ "1.u129",		0x08000, 0xdbfac46b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.u110",		0x10000, 0xb7adb99a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u4.u86",		0x08000, 0x497fad4c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "u3.u99",		0x10000, 0x4800322a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "s73",		0x80000, 0x63f4e10c, 3 | BRF_GRA },           //  4 Background tiles
	{ "s72",		0x80000, 0x4e669e97, 3 | BRF_GRA },           //  5

	{ "s71",		0x80000, 0x431485ee, 4 | BRF_GRA },           //  6 Foreground tiles

	{ "a30.u12",		0x40000, 0x50bc5e98, 5 | BRF_GRA },           //  7 Sprites
	{ "a29.u2",		0x40000, 0xa240a448, 5 | BRF_GRA },           //  8

	{ "g71.u118",		0x80000, 0x488e2fd1, 6 | BRF_SND },           //  9 YM2610 Samples

	{ "g72.u83",		0x80000, 0xdc3d14be, 7 | BRF_SND },           // 10 YM2610 Delta-T Samples

	{ "palce16v8h.114",	0x00117, 0x1f3a3816, 8 | BRF_OPT },           // 11 plds
	{ "gal16v8a.115",	0x00117, 0x2b32e239, 8 | BRF_OPT },           // 12
	{ "gal16v8a.116",	0x00117, 0x3674f043, 8 | BRF_OPT },           // 13
	{ "gal16v8a.127",	0x00117, 0x7115d95c, 8 | BRF_OPT },           // 14
};

STD_ROM_PICK(pipedrmj)
STD_ROM_FN(pipedrmj)

struct BurnDriver BurnDrvPipedrmj = {
	"pipedrmj", "pipedrm", NULL, NULL, "1990",
	"Pipe Dream (Japan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pipedrmjRomInfo, pipedrmjRomName, NULL, NULL, NULL, NULL, PipedrmInputInfo, PipedrmDIPInfo,
	pipedrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Pipe Dream (Taiwan)

static struct BurnRomInfo pipedrmtRomDesc[] = {
	{ "t1.u129",		0x08000, 0x335401a4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "t2.u110",		0x10000, 0xc8209b67, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u4.u86",		0x08000, 0x497fad4c, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "u3.u99",		0x10000, 0x4800322a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "s73",		0x80000, 0x63f4e10c, 3 | BRF_GRA },           //  4 Background tiles
	{ "s72",		0x80000, 0x4e669e97, 3 | BRF_GRA },           //  5

	{ "s71",		0x80000, 0x431485ee, 4 | BRF_GRA },           //  6 Foreground tiles

	{ "a30.u12",		0x40000, 0x50bc5e98, 5 | BRF_GRA },           //  7 Sprites
	{ "a29.u2",		0x40000, 0xa240a448, 5 | BRF_GRA },           //  8

	{ "g71.u118",		0x80000, 0x488e2fd1, 6 | BRF_SND },           //  9 YM2610 Samples

	{ "g72.u83",		0x80000, 0xdc3d14be, 7 | BRF_SND },           // 10 YM2610 Delta-T Samples

	{ "palce16v8h.114",	0x00117, 0x1f3a3816, 8 | BRF_OPT },           // 11 plds
	{ "gal16v8a.115",	0x00117, 0x2b32e239, 8 | BRF_OPT },           // 12
	{ "gal16v8a.116",	0x00117, 0x3674f043, 8 | BRF_OPT },           // 13
	{ "gal16v8a.127",	0x00117, 0x7115d95c, 8 | BRF_OPT },           // 14
};

STD_ROM_PICK(pipedrmt)
STD_ROM_FN(pipedrmt)

struct BurnDriver BurnDrvPipedrmt = {
	"pipedrmt", "pipedrm", NULL, NULL, "1990",
	"Pipe Dream (Taiwan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pipedrmtRomInfo, pipedrmtRomName, NULL, NULL, NULL, NULL, PipedrmInputInfo, PipedrmDIPInfo,
	pipedrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Hatris (US)

static struct BurnRomInfo hatrisRomDesc[] = {
	{ "2.ic79",		0x08000, 0x4ab50b54, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1-ic81.bin",		0x08000, 0xdb25e166, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b0-ic56.bin",	0x20000, 0x34f337a4, 3 | BRF_GRA },           //  2 Background tiles
	{ "b1-ic73.bin",	0x08000, 0x6351d0ba, 3 | BRF_GRA },           //  3

	{ "a0-ic55.bin",	0x20000, 0x7b7bc619, 4 | BRF_GRA },           //  4 Foreground tiles
	{ "a1-ic60.bin",	0x20000, 0xf74d4168, 4 | BRF_GRA },           //  5

	{ "pc-ic53.bin",	0x20000, 0x07147712, 5 | BRF_SND },           //  6 YM2608 Samples
};

STDROMPICKEXT(hatris, hatris, Ym2608)
STD_ROM_FN(hatris)

static INT32 hatrisInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvHatris = {
	"hatris", NULL, "ym2608", NULL, "1990",
	"Hatris (US)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hatrisRomInfo, hatrisRomName, NULL, NULL, NULL, NULL, HatrisInputInfo, HatrisDIPInfo,
	hatrisInit, DrvExit, DrvFrame, FromanceDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Hatris (Japan)

static struct BurnRomInfo hatrisjRomDesc[] = {
	{ "2-ic79.bin",		0x08000, 0xbbcaddbf, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1-ic81.bin",		0x08000, 0xdb25e166, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "b0-ic56.bin",	0x20000, 0x34f337a4, 3 | BRF_GRA },           //  2 Background tiles
	{ "b1-ic73.bin",	0x08000, 0x6351d0ba, 3 | BRF_GRA },           //  3

	{ "a0-ic55.bin",	0x20000, 0x7b7bc619, 4 | BRF_GRA },           //  4 Foreground tiles
	{ "a1-ic60.bin",	0x20000, 0xf74d4168, 4 | BRF_GRA },           //  5

	{ "pc-ic53.bin",	0x20000, 0x07147712, 5 | BRF_SND },           //  6 YM2608 Samples
};

STDROMPICKEXT(hatrisj, hatrisj, Ym2608)
STD_ROM_FN(hatrisj)

struct BurnDriver BurnDrvHatrisj = {
	"hatrisj", "hatris", "ym2608", NULL, "1990",
	"Hatris (Japan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hatrisjRomInfo, hatrisjRomName, NULL, NULL, NULL, NULL, HatrisInputInfo, HatrisDIPInfo,
	hatrisInit, DrvExit, DrvFrame, FromanceDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};
