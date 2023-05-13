// FinalBurn Neo Tatsumi Lock-on driver module
// Based on MAME driver by Philip Bennett

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "watchdog.h"
#include "burn_ym2203.h"
#include "dtimer.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvV30ROM[3];
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[7];
static UINT8 *DrvColPROM;
static UINT8 *DrvZoomROM;
static UINT8 *DrvV30RAM[3];
static UINT8 *DrvZ80RAM;
static UINT8 *DrvHudRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvSceneRAM;
static UINT8 *DrvGroundRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvColorLut;
static UINT8 *DrvObjPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *scroll;

static INT32 m_iden;
static INT32 m_ctrl_reg;
static INT32 m_xsal;
static INT32 m_x0ll;
static INT32 m_dx0ll;
static INT32 m_dxll;
static INT32 m_ysal;
static INT32 m_y0ll;
static INT32 m_dy0ll;
static INT32 m_dyll;
static INT32 main_inten;
static INT32 ground_ctrl;
static INT32 m_obj_pal_latch;
static INT32 m_obj_pal_addr;
static INT32 back_buffer_select;
static INT32 sound_volume;

static void objects_draw(); // forward

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static ButtonToggle Diag;

static INT16 Analog[2];

static INT32 nExtraCycles[5];

static dtimer bufend_timer;

static INT32 reload_hack = 0; // necessary for rewind/runahead/states

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo LockonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},

	A("P1 Stick X",		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Stick Y",		BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3 "
		"(Missile)",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},
	{"P1 Button 4 "
		"(Hover)",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A
STDINPUTINFO(Lockon)

static struct BurnDIPInfo LockonDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xd0, NULL						},
	{0x01, 0xff, 0xff, 0x40, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x01, "2"						},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x0c, 0x04, "Easy"						},
	{0x00, 0x01, 0x0c, 0x00, "Medium"					},
	{0x00, 0x01, 0x0c, 0x08, "Hard"						},
	{0x00, 0x01, 0x0c, 0x0c, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x10, 0x00, "150K & every 200K"		},
	{0x00, 0x01, 0x10, 0x10, "200K & every 200K"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x20, 0x20, "No"						},
	{0x00, 0x01, 0x20, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x01, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x07, 0x06, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x07, 0x04, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0x07, 0x05, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0x07, 0x07, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x01, 0x01, 0x38, 0x30, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x38, 0x00, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x38, 0x10, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x38, 0x20, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0x38, 0x28, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0x38, 0x38, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Enable H/W Tests Menu"	},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Jumper 1"					},
	{0x02, 0x01, 0x40, 0x40, "Off"						},
	{0x02, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Jumper 0"					},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Lockon)

static struct BurnDIPInfo LockoneDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xd0, NULL						},
	{0x01, 0xff, 0xff, 0x40, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x01, "2"						},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x0c, 0x04, "Easy"						},
	{0x00, 0x01, 0x0c, 0x00, "Medium"					},
	{0x00, 0x01, 0x0c, 0x08, "Hard"						},
	{0x00, 0x01, 0x0c, 0x0c, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x10, 0x00, "150K & every 200K"		},
	{0x00, 0x01, 0x10, 0x10, "200K & every 200K"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x20, 0x20, "No"						},
	{0x00, 0x01, 0x20, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x01, 0x01, 0x07, 0x03, "4 Coins 1 Credit"			},
	{0x01, 0x01, 0x07, 0x02, "3 Coins 1 Credit"			},
	{0x01, 0x01, 0x07, 0x01, "2 Coins 1 Credit"			},
	{0x01, 0x01, 0x07, 0x00, "1 Coin  1 Credit"			},
	{0x01, 0x01, 0x07, 0x06, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x07, 0x07, "2 Coin  4 Credits"		},
	{0x01, 0x01, 0x07, 0x04, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Buy-in"					},
	{0x01, 0x01, 0x08, 0x08, "2 Coins 1 Credit"			},
	{0x01, 0x01, 0x08, 0x00, "1 Coin  1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Enable H/W Tests Menu"	},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Jumper 1"					},
	{0x02, 0x01, 0x40, 0x40, "Off"						},
	{0x02, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Jumper 0"					},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Lockone)

static void write_byte(INT32 cpu, INT32 page, UINT16 offset, UINT8 data)
{
	VezCPUPush(cpu);
	VezWriteByte((page * 0x10000) + offset, data);
	VezCPUPop();
}

static UINT8 read_byte(INT32 cpu, INT32 page, UINT16 offset)
{
	VezCPUPush(cpu);
	UINT8 ret = VezReadByte((page * 0x10000) + offset);
	VezCPUPop();

	return ret;
}

static void grab_16bit(INT32 &var, INT32 addy, UINT8 data, INT32 mask)
{
	switch (addy & 1) {
		case 0:
			var = (var & 0xff00) | (data << 0);
			break;
		case 1:
			var = (var & 0x00ff) | (data << 8);
			break;
	}
	var &= mask;
}

static void __fastcall lockon_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff001) == 0x0c000) {
		INT32 offset = (address / 2) & 0x7ff;
		DrvColorLut[offset] = data & 0xff; // + 0x300
		DrvPalette[0x400 + offset] = DrvPalette[0x300 + DrvColorLut[offset]];
		return;
	}

	if ((address & 0xff000) == 0x0b000)
	{
		switch ((address & 0xf) / 2)
		{
			case 0: grab_16bit(m_xsal, address, data, 0x1ff); break;
			case 1: grab_16bit(m_x0ll, address, data, 0xff); break;
			case 2: grab_16bit(m_dx0ll, address, data, 0x1ff); break;
			case 3: grab_16bit(m_dxll, address, data, 0x1ff); break;
			case 4: grab_16bit(m_ysal, address, data, 0x1ff); break;
			case 5: grab_16bit(m_y0ll, address, data, 0xff); break;
			case 6: grab_16bit(m_dy0ll, address, data, 0x1ff); break;
			case 7: grab_16bit(m_dyll, address, data, 0x3ff); break;
		}
		return;
	}

	if ((address & 0xff800) == 0x10000) {
		write_byte(1, m_ctrl_reg & 3, address, data);
		write_byte(2, (m_ctrl_reg >> 3) & 3, address, data);
		return;
	}

	if ((address & 0xf0001) == 0x20000) { // 8 bit! &~1 is the low bit on LE-cpu (Vez)
		ZetWriteByte((address & 0xffff) / 2, data);
		return;
	}

	if ((address & 0xf0000) == 0x30000) {
		write_byte(1, m_ctrl_reg & 3, address, data);
		return;
	}

	if ((address & 0xf0000) == 0x40000) {
		write_byte(2, (m_ctrl_reg >> 3) & 3, address, data);
		return;
	}

	switch (address)
	{
		case 0x04000:
		case 0x04001:
		case 0x04002:
		case 0x04003:
			// crtc - not used in emulation
		return;

		case 0x0a000:
		{
			m_ctrl_reg = data & 0xff;

			VezSetHALT(1, data & 0x04); // ground
			VezSetHALT(2, data & 0x20); // sprite

			ZetSetHALT(~data & 0x40);
		}
		return;

		case 0x0e000:
			main_inten = 1;
		return;

		case 0x0f000:
			BurnWatchdogWrite();
			main_inten = 0;
		return;
	}
}

static UINT8 __fastcall lockon_main_read(UINT32 address)
{
	if ((address & 0xff0000) == 0x10000) return 0xff;

	if ((address & 0xf0000) == 0x20000) {
		// 16bit -> 8bit bus, high byte is always 0xff!
		return (address & 1) ? 0xff : ZetReadByte((address & 0xffff) / 2);
	}

	if ((address & 0xf0000) == 0x30000) {
		return read_byte(1, m_ctrl_reg & 3, address);
	}

	if ((address & 0xf0000) == 0x40000) {
		return read_byte(2, (m_ctrl_reg >> 3) & 3, address);
	}

	switch (address)
	{
		case 0x04000:
		case 0x04001:
		case 0x04002:
		case 0x04003:
			return 0xff; // crtc

		case 0x06000:
			return DrvDips[0];

		case 0x06001:
			return DrvDips[1];
	}

	return 0xff;
}

static UINT8 __fastcall lockon_ground_read(UINT32 address)
{
	return 0xff;
}

static void __fastcall lockon_ground_write(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0c000:
		case 0x0c001:
		case 0x0c002:
		case 0x0c003:
			scroll[address & 3] = data;
		return;

		case 0x0c004:
			ground_ctrl = data; // to major tom
		return;
	}
}

static void __fastcall lockon_object_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff001) == 0x08000) {
		if (m_iden) {
			m_obj_pal_latch = data & 0xff;
			m_obj_pal_addr = (address / 2) & 0xf;
			objects_draw();
		}
		return;
	}

	switch (address)
	{
		case 0x04000:
			m_iden = data & 1;
		return;
	}
}

static UINT8 __fastcall lockon_object_read(UINT32 address)
{
	switch (address)
	{
		case 0x04000:
			VezSetIRQLineAndVector(NEC_INPUT_LINE_POLL, 0xff, CPU_IRQSTATUS_NONE); //vector ok??
			return 0xff;
	}

	return 0xff;
}

static const double vols[0x10] = {0.46, 0.44, 0.43, 0.41, 0.39, 0.37, 0.35, 0.32, 0.27, 0.25, 0.22, 0.19, 0.14, 0.10, 0.06, 0.01 };

static void __fastcall lockon_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7000:
			sound_volume = data;

			BurnYM2203SetLeftVolume (0, BURN_SND_YM2203_YM2203_ROUTE, vols[(data >> 0) & 0xf] * 2.0);
			BurnYM2203SetRightVolume(0, BURN_SND_YM2203_YM2203_ROUTE, vols[(data >> 4) & 0xf] * 2.0);
			BurnYM2203SetLeftVolume (0, BURN_SND_YM2203_AY8910_ROUTE_1, vols[(data >> 0) & 0xf]);
			BurnYM2203SetRightVolume(0, BURN_SND_YM2203_AY8910_ROUTE_1, vols[(data >> 4) & 0xf]);
			BurnYM2203SetLeftVolume (0, BURN_SND_YM2203_AY8910_ROUTE_2, vols[(data >> 0) & 0xf]);
			BurnYM2203SetRightVolume(0, BURN_SND_YM2203_AY8910_ROUTE_2, vols[(data >> 4) & 0xf]);
			BurnYM2203SetLeftVolume (0, BURN_SND_YM2203_AY8910_ROUTE_3, vols[(data >> 0) & 0xf]);
			BurnYM2203SetRightVolume(0, BURN_SND_YM2203_AY8910_ROUTE_3, vols[(data >> 4) & 0xf]);
		return;

		case 0x7400:
		case 0x7401:
		case 0x7402:
		case 0x7403:
			// nop
		return;
	}
}

static UINT8 __fastcall lockon_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x7400:
			return ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x00, 0xff);

		case 0x7401:
			return ProcessAnalog(Analog[1], 0, INPUT_DEADZONE, 0x00, 0xff);

		case 0x7402:
			return DrvInputs[2];

		case 0x7403:
			return DrvInputs[1];
	}

	return 0xff;
}

static void __fastcall lockon_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x02:
		return; // nop
	}
}

static UINT8 __fastcall lockon_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);
	}

	return 0;
}

static tilemap_callback( char )
{
	INT32 attr = DrvCharRAM[offs * 2 + 1];
	INT32 code = DrvCharRAM[offs * 2 + 0] | (attr << 8);

	TILE_SET_INFO(0, code&0x3ff, ((attr >> 2) & 0x1f) | ((attr & 0x80) >> 1), 0);
}

static void YM2203_write_B(UINT32, UINT32 data)
{
	// coin counter & led
	if (data) return; // warnings
}

static UINT8 YM2203_read_A(UINT32)
{
	return (DrvDips[2] & 0xc0) | (DrvInputs[0] & 0x3f);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void bufend_cb(INT32 param)
{
	VezSetIRQLineAndVector(1, 0, 0xff, CPU_IRQSTATUS_ACK); // ground
	VezSetIRQLineAndVector(2, NEC_INPUT_LINE_POLL, 0xff, CPU_IRQSTATUS_ACK); // sprite
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV30ROM[0]	= Next; Next += 0x080000;
	DrvV30ROM[1]	= Next; Next += 0x040000;
	DrvV30ROM[2]	= Next; Next += 0x040000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM[0]	= Next; Next += 0x010000;
	DrvGfxROM[1]	= Next; Next += 0x040000;
	DrvGfxROM[2]	= Next; Next += 0x020000;
	DrvGfxROM[3]	= Next; Next += 0x060000;
	DrvGfxROM[4]	= Next; Next += 0x100000;
	DrvGfxROM[5]	= Next; Next += 0x010000;
	DrvGfxROM[6]	= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000800;
	DrvZoomROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0004000 * sizeof(UINT32);

	AllRam			= Next;

	DrvV30RAM[0]	= Next; Next += 0x004000;
	DrvV30RAM[1]	= Next; Next += 0x004000;
	DrvV30RAM[2]	= Next; Next += 0x004000;

	DrvZ80RAM		= Next; Next += 0x000800;

	DrvHudRAM		= Next; Next += 0x000200;
	DrvCharRAM		= Next; Next += 0x001000;
	DrvSceneRAM		= Next; Next += 0x001000;
	DrvGroundRAM	= Next; Next += 0x001000;
	DrvSpriteRAM	= Next; Next += 0x000200;
	DrvObjPalRAM    = Next; Next += 0x000800;

	DrvColorLut		= Next; Next += 0x000800;

	scroll			= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	VezReset(0);
	VezReset(1);
	VezReset(2);

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	BurnWatchdogReset();

	timerReset();

	m_iden = 0;
	m_ctrl_reg = 0;
	m_xsal = 0;
	m_x0ll = 0;
	m_dx0ll = 0;
	m_dxll = 0;
	m_ysal = 0;
	m_y0ll = 0;
	m_dy0ll = 0;
	m_dyll = 0;
	main_inten = 0;
	ground_ctrl = 0;
	m_obj_pal_latch = 0;
	m_obj_pal_addr = 0;

	back_buffer_select = 1;

	sound_volume = 0x00;
	lockon_sound_write(0x7000, sound_volume);

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[2] = { 0, 8*8*1024 };
	INT32 XOffs[8]  = { STEP8(0,1) };
	INT32 YOffs[8]  = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	BurnSetRefreshRate(55.80);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvV30ROM[0] + 0x60000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[0] + 0x60001, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[0] + 0x50000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[0] + 0x50001, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[0] + 0x70000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[0] + 0x70001, k++, 2)) return 1;

		if (BurnLoadRom(DrvV30ROM[1] + 0x20000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[1] + 0x20001, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[1] + 0x30000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[1] + 0x30001, k++, 2)) return 1;

		if (BurnLoadRom(DrvV30ROM[2] + 0x30000, k++, 2)) return 1;
		if (BurnLoadRom(DrvV30ROM[2] + 0x30001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;	// char
		if (BurnLoadRom(DrvGfxROM[0] + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;	// scene
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;  // hud

		if (BurnLoadRom(DrvGfxROM[3] + 0x00000, k++, 1)) return 1;  // ground
		if (BurnLoadRom(DrvGfxROM[3] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x30000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x50000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0x00000, k++, 1)) return 1;  // object (sprites)
		if (BurnLoadRom(DrvGfxROM[4] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x28000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x30000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x38000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x48000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x50000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x58000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x60000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x68000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x70000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x78000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0x80000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x88000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x90000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0x98000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xa0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xa8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xb0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xb8000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0xc0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xc8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xd0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xd8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xe0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xe8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xf0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[4] + 0xf8000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[5] + 0x00000, k++, 1)) return 1;  // object LUT
		if (BurnLoadRom(DrvGfxROM[5] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[6] + 0x00000, k++, 2)) return 1;  // object chunk LUT
		if (BurnLoadRom(DrvGfxROM[6] + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;  // color proms
		if (BurnLoadRom(DrvColPROM   + 0x00400, k++, 1)) return 1;

		if (BurnLoadRom(DrvZoomROM   + 0x00000, k++, 1)) return 1;  // scale proms
		if (BurnLoadRom(DrvZoomROM   + 0x00800, k++, 1)) return 1;

		for (INT32 i = 0; i < 0x800; i++) {
			DrvZoomROM[i] = (DrvZoomROM[i] & 0xf) | (DrvZoomROM[i + 0x800] << 4);
		}

		DrvGfxDecode();
	}

	VezInit(0, V30_TYPE);
	VezOpen(0);
	VezMapMemory(DrvV30RAM[0],				0x00000, 0x03fff, MAP_RAM);
	VezMapMemory(DrvHudRAM,					0x08000, 0x081ff, MAP_RAM);
	VezMapMemory(DrvCharRAM,				0x09000, 0x09fff, MAP_RAM);
	VezMapMemory(DrvV30ROM[0] + 0x50000,	0x50000, 0x7ffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[0] + 0x50000,	0xd0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(lockon_main_write);
	VezSetReadHandler(lockon_main_read);
	VezClose();

	VezInit(1, V30_TYPE);
	VezOpen(1);
	VezMapMemory(DrvV30RAM[1],				0x00000, 0x03fff, MAP_RAM);
	VezMapMemory(DrvSceneRAM,				0x04000, 0x04fff, MAP_RAM);
	VezMapMemory(DrvGroundRAM,				0x08000, 0x08fff, MAP_RAM);
	VezMapMemory(DrvV30ROM[1] + 0x20000,	0x20000, 0x3ffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[1] + 0x20000,	0x60000, 0x7ffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[1] + 0x20000,	0xa0000, 0xbffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[1] + 0x20000,	0xe0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(lockon_ground_write);
	VezSetReadHandler(lockon_ground_read);
	VezClose();

	VezInit(2, V30_TYPE);
	VezOpen(2);
	VezMapMemory(DrvV30RAM[2],				0x00000, 0x03fff, MAP_RAM);
	VezMapMemory(DrvSpriteRAM,				0x0c000, 0x0c1ff, MAP_RAM);
	VezMapMemory(DrvV30ROM[2] + 0x30000,	0x30000, 0x3ffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[2] + 0x30000,	0x70000, 0x7ffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[2] + 0x30000,	0xb0000, 0xbffff, MAP_ROM); // mirror
	VezMapMemory(DrvV30ROM[2] + 0x30000,	0xf0000, 0xfffff, MAP_ROM);
	VezSetWriteHandler(lockon_object_write);
	VezSetReadHandler(lockon_object_read);
	VezClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,					0x0000, 0x6fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,					0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,					0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(lockon_sound_write);
	ZetSetReadHandler(lockon_sound_read);
	ZetSetOutHandler(lockon_sound_write_port);
	ZetSetInHandler(lockon_sound_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2203Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnYM2203SetPorts(0, &YM2203_read_A, NULL, NULL, &YM2203_write_B);
	BurnTimerAttach(&ZetConfig, 4000000);
	bYM2203UseSeperateVolumes = 1;
	BurnYM2203SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.48);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, char_map_callback,   8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2, 8, 8, 0x400 * 8 * 8, 0x000, 0x7f);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0);

	BurnBitmapAllocate(1, 512, 512, false);
	BurnBitmapAllocate(2, 512, 512, false);

 	timerInit();
	timerAdd(bufend_timer, 0, bufend_cb);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();

	ZetExit();
	VezExit();

	GenericTilesExit();

	timerExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	const res_net_info lockon_net_info =
	{
		RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT,
		{
			{RES_NET_AMP_NONE, 560, 0, 5, {4700, 2200, 1000, 470, 220}},
			{RES_NET_AMP_NONE, 560, 0, 5, {4700, 2200, 1000, 470, 220}},
			{RES_NET_AMP_NONE, 560, 0, 5, {4700, 2200, 1000, 470, 220}}
		}
	};

	const res_net_info lockon_pd_net_info =
	{
		RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT,
		{
			{RES_NET_AMP_NONE, 560, 580, 5, {4700, 2200, 1000, 470, 220}},
			{RES_NET_AMP_NONE, 560, 580, 5, {4700, 2200, 1000, 470, 220}},
			{RES_NET_AMP_NONE, 560, 580, 5, {4700, 2200, 1000, 470, 220}}
		}
	};

	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 p1 = DrvColPROM[i];
		INT32 p2 = DrvColPROM[i + 0x400];
#if 0
		// it's close..
		INT32 r = ((p2 & 4) ? 220 : 0) + ((p2 & 8) ? 470 : 0) + ((p2 & 16) ? 1000 : 0) + ((p2 & 32) ? 2200 : 0) + ((p2 & 64) ? 4700 : 0);
		INT32 g = ((p1 & 32) ? 220 : 0) + ((p1 & 64) ? 470 : 0) + ((p1 & 128) ? 1000 : 0) + ((p2 & 1) ? 2200 : 0) + ((p2 & 2) ? 4700 : 0);
		INT32 b = ((p1 & 1) ? 220 : 0) + ((p1 & 2) ? 470 : 0) + ((p1 & 4) ? 1000 : 0) + ((p1 & 8) ? 2200 : 0) + ((p1 & 16) ? 4700 : 0);

		INT32 rtotal = 8590;

		rtotal += 5.0 * 560;
		if (p2 & 0x80) rtotal -= 4.0 * 580;

		r = (r * 255) / rtotal;
		g = (g * 255) / rtotal;
		b = (b * 255) / rtotal;
#endif
		const res_net_info *netinfo;

		netinfo = (p2 & 0x80) ? &lockon_net_info : &lockon_pd_net_info;

		INT32 r = compute_res_net((p2 >> 2) & 0x1f, 0, *netinfo);
		INT32 g = compute_res_net(((p1 >> 5) & 0x7) | (p2 & 3) << 3, 1, *netinfo);
		INT32 b = compute_res_net((p1 & 0x1f), 2, *netinfo);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 0x800; i++) {
		DrvPalette[i + 0x400] = DrvPalette[0x300 + DrvColorLut[i]];
	}
}

#define BIT(x,y)	(((x) >> (y)) & 1)

static void scene_draw()
{
	/* 3bpp characters */
	const UINT8 *const gfx1 = DrvGfxROM[1];//gfx2
	const UINT8 *const gfx2 = gfx1 + 0x10000;
	const UINT8 *const gfx3 = gfx1 + 0x20000;
	const UINT8 *const clut = gfx1 + 0x30000;

	INT32 m_scroll_h = (scroll[1] * 256) + scroll[0];
	INT32 m_scroll_v = (scroll[3] * 256) + scroll[2];
	UINT16 *sceneram = (UINT16*)DrvSceneRAM;

	for (UINT32 y = 0; y < 416; ++y) // 357.2  // 416.744186 (dink): ((10000000 / (431-1)) / (7000000/(448*280)))
	{
		UINT32 d0 = 0, d1 = 0, d2 = 0;
		UINT32 colour = 0;
		UINT32 y_offs;
		UINT32 x_offs;
		UINT32 y_gran;
		UINT16 *bmpaddr;
		UINT32 ram_mask = 0x7ff;

		y_offs = (y + m_scroll_v) & 0x1ff;

		/* Clamp - stops tilemap wrapping when screen is rotated */
		if ((m_scroll_v & 0x8000) && y_offs & 0x100)
			ram_mask = 0x7;

		x_offs = (m_scroll_h - 8) & 0x1ff;
		y_gran = y_offs & 7;

		if (x_offs & 7)
		{
			UINT32 tileidx;
			UINT16 addr = ((y_offs & ~7) << 3) + ((x_offs >> 3) & 0x3f);
			UINT16 ram_val = sceneram[addr & ram_mask];

			colour = (clut[ram_val & 0x7fff] & 0x3f) << 3;
			tileidx = ((ram_val & 0x0fff) << 3) + y_gran;

			d0 = *(gfx1 + tileidx);
			d1 = *(gfx2 + tileidx);
			d2 = *(gfx3 + tileidx);
		}

		bmpaddr = BurnBitmapGetPosition(1 + back_buffer_select, 0, y); //&m_back_buffer->pix16(y);

		for (UINT32 x = 0; x < 431; ++x)
		{
			UINT32 x_gran = (x_offs & 7) ^ 7;
			UINT32 col;

			if (!(x_offs & 7))
			{
				UINT32 tileidx;
				UINT16 addr = ((y_offs & ~7) << 3) + ((x_offs >> 3) & 0x3f);
				UINT16 ram_val = sceneram[addr & ram_mask];

				colour = (clut[ram_val & 0x7fff] & 0x3f) << 3;
				tileidx = ((ram_val & 0x0fff) << 3) + y_gran;

				d0 = *(gfx1 + tileidx);
				d1 = *(gfx2 + tileidx);
				d2 = *(gfx3 + tileidx);
			}

			col = colour
					| (((d2 >> x_gran) & 1) << 2)
					| (((d1 >> x_gran) & 1) << 1)
					| ( (d0 >> x_gran) & 1);

			*bmpaddr++ = 0xa00 + col;

			x_offs = (x_offs + 1) & 0x1ff;
		}

	}
}

/* Get data for a each 8x8x3 ground tile */
#define GET_GROUND_DATA()                                                                \
{                                                                                        \
	UINT32 gfx_a4_3  = (ls163 & 0xc) << 1;                                               \
	UINT32 lut_addr  = lut_address + ((ls163 >> 4) & 0xf);                               \
	UINT32 gfx_a14_7 = lut_rom[lut_addr] << 7;                                           \
	clut_addr = (lut_rom[lut_addr] << 4) | clut_a14_12 | clut_a4_3 | (ls163 & 0xc) >> 2; \
	gfx_addr  = gfx_a15 | gfx_a14_7 | gfx_a6_5 | gfx_a4_3 | gfx_a2_0;                    \
	pal = (clut_rom[clut_addr] << 3);                                                    \
	rom_data1 = gfx_rom[gfx_addr];                                                       \
	rom_data2 = gfx_rom[gfx_addr + 0x10000];                                             \
	rom_data3 = gfx_rom[gfx_addr + 0x20000];                                             \
}

static void ground_draw(INT32 from_drvdraw)
{
	/* ROM pointers */
	const UINT8 *const gfx_rom  = DrvGfxROM[3];
	const UINT8 *const lut_rom  = gfx_rom + 0x30000 + (((ground_ctrl >> 2) & 0x3) ? 0x10000 : 0);
	const UINT8 *const clut_rom = gfx_rom + 0x50000;

	UINT32 lut_a15_14   = (ground_ctrl & 0x3) << 14;
	UINT32 clut_a14_12 = (ground_ctrl & 0x70) << 8;
	UINT32 gfx_a15 = (ground_ctrl & 0x40) << 9;
	UINT32 offs = 3;
	UINT32 y;

	UINT16 *m_ground_ram = (UINT16*)DrvGroundRAM;

	/* TODO: Clean up and emulate CS of GFX ROMs? */
	for (y = 0; y < 512; ++y)
	{
		UINT16 *bmpaddr = BurnBitmapGetPosition(1 + back_buffer_select, 0, y); //&m_back_buffer->pix16(y);
		UINT8 ls163;
		UINT32 clut_addr;
		UINT32 gfx_addr;
		UINT8 rom_data1 = 0;
		UINT8 rom_data2 = 0;
		UINT8 rom_data3 = 0;
		UINT32 pal = 0;
		UINT32 x;

		/* Draw this line? */
		if (!(m_ground_ram[offs] & 0x8000))
		{
			UINT32 gfx_a2_0  =  m_ground_ram[offs] & 0x0007;
			UINT32 gfx_a6_5  = (m_ground_ram[offs] & 0x0018) << 2;
			UINT32 clut_a4_3 = (m_ground_ram[offs] & 0x0018) >> 1;
			UINT8   tz2213_x  = m_ground_ram[offs + 1] & 0xff;
			UINT8   tz2213_dx = m_ground_ram[offs + 2] & 0xff;

			UINT32 lut_address = lut_a15_14 + ((m_ground_ram[offs] & 0x7fe0) >> 1);
			UINT32 cy = m_ground_ram[offs + 2] & 0x0100;
			UINT32 color;
			UINT32 gpbal2_0_prev;

			ls163 = m_ground_ram[offs + 1] >> 8;

			gpbal2_0_prev = ((ls163 & 3) << 1) | BIT(tz2213_x, 7);

			if (gpbal2_0_prev)
				GET_GROUND_DATA();

			for (x = 0; x < 431; x++)
			{
				UINT32 tz2213_cy;
				UINT32 gpbal2_0 = ((ls163 & 3) << 1) | BIT(tz2213_x, 7);

				/* Stepped into a new tile? */
				if (gpbal2_0 < gpbal2_0_prev)
					GET_GROUND_DATA();

				gpbal2_0_prev = gpbal2_0;

				color = pal;
				color += (rom_data1 >> gpbal2_0) & 0x1;
				color += ((rom_data2 >> gpbal2_0) & 0x1) << 1;
				color += ((rom_data3 >> gpbal2_0) & 0x1) << 2;

				*bmpaddr++ = 0x800 + color;

				/* Update the counters */
				tz2213_cy = (UINT8)tz2213_dx > (UINT8)~(tz2213_x);
				tz2213_x = (tz2213_x + tz2213_dx);

				/* Carry? */
				if (tz2213_cy || cy)
					++ls163;
			}
		}

		offs += 3;

		/* End of list marker */
		if (m_ground_ram[offs + 2] & 0x8000 && !from_drvdraw)
		{
			bufend_timer.start(y * 431, -1, 1, 0);
		}
	}
}

#define DRAW_OBJ_PIXEL(COLOR)                            \
do {                                                     \
	if (px < 431)                          \
	if (COLOR != 0xf)                                    \
	{                                                    \
		UINT8 clr = DrvObjPalRAM[(pal << 4) + COLOR];     \
		UINT16 *_pix = (line + px);                       \
		if (!(clr == 0xff && ((*_pix & 0xe00) == 0xa00))) \
			*_pix = 0x400 + clr;          \
	}                                                    \
	px = (px + 1) & 0x7ff;                               \
} while(0)

static void objects_draw()
{
	UINT32 offs;

	const UINT8  *const romlut = DrvGfxROM[5];
	const UINT16 *const chklut = (UINT16*)DrvGfxROM[6];
	const UINT8  *const gfxrom = DrvGfxROM[4];
	const UINT8  *const sproms = DrvZoomROM;
	UINT16 *m_object_ram = (UINT16*)DrvSpriteRAM;

	for (offs = 0; offs < 0x200; offs += 4)
	{
		/* Retrieve the object attributes */
		UINT32 ypos    = m_object_ram[offs] & 0x03ff;
		UINT32 xpos    = m_object_ram[offs + 3] & 0x07ff;
		UINT32 ysize   = (m_object_ram[offs] >> 10) & 0x3;
		UINT32 xsize   = (m_object_ram[offs] >> 12) & 0x3;
		UINT32 yflip   = BIT(m_object_ram[offs], 14);
		UINT32 xflip   = BIT(m_object_ram[offs], 15);
		UINT32 scale   = m_object_ram[offs + 1] & 0xff;
		UINT32 pal = (m_object_ram[offs + 1] >> 8) & 0x7f;
		UINT32 opsta   = m_object_ram[offs + 2];

		if (m_iden)
		{
			DrvObjPalRAM[(pal << 4) + m_obj_pal_addr] = m_obj_pal_latch;
			break;
		}

		/* How many lines will this sprite occupy? The PAL @ IC154 knows... */
		UINT32 lines = scale >> (3 - ysize);

		UINT32 opsta15_8 = opsta & 0xff00;

		/* Account for line buffering */
		ypos -=1;

		for (UINT32 y = 0; y < 416; y++)
		{
			UINT32 cy = (y + ypos) & 0x3ff;
			UINT32 optab;
			UINT32 lutaddr;
			UINT32 tile;
			UINT8   cnt;
			UINT32 yidx;
			UINT16 *line = BurnBitmapGetPosition(1 + back_buffer_select, 0, y); //&m_back_buffer->pix16(y);
			UINT32 px = xpos;

			/* Outside the limits? */
			if (cy & 0x300)
				continue;

			if ((cy  & 0xff) >= lines)
				break;

			lutaddr = (scale & 0x80 ? 0x8000 : 0) | ((scale & 0x7f) << 8) | (cy & 0xff);
			optab = romlut[lutaddr] & 0x7f;

			if (yflip)
				optab ^= 7;

			yidx = (optab & 7);

			/* Now calculate the lower 7-bits of the LUT ROM address. PAL @ IC157 does this */
			cnt = (optab >> 3) * (1 << xsize);

			if (xflip)
				cnt ^= 7 >> (3 - xsize);
			if (yflip)
				cnt ^= (0xf >> (3 - ysize)) * (1 << xsize);

			cnt = (cnt + (opsta & 0xff));

			/* Draw! */
			for (tile = 0; tile < (1 << xsize); ++tile)
			{
				UINT16 sc;
				UINT16 scl;
				UINT32 x;
				UINT32 tileaddr;
				UINT16 td0, td1, td2, td3;
				UINT32 j;
				UINT32 bank;

				scl = scale & 0x7f;
				tileaddr = (chklut[opsta15_8 + cnt] & 0x7fff);
				bank = ((tileaddr >> 12) & 3) * 0x40000;
				tileaddr = bank + ((tileaddr & ~0xf000) << 3);

				if (xflip)
					--cnt;
				else
					++cnt;

				/* Draw two 8x8 tiles */
				for (j = 0; j < 2; ++j)
				{
					/* Get tile data */
					UINT32 tileadd = tileaddr + (0x20000 * (j ^ xflip));

					/* Retrieve scale values from PROMs */
					sc = sproms[(scl << 4) + (tile * 2) + j];

					/* Data from ROMs is inverted */
					td3 = gfxrom[tileadd + yidx] ^ 0xff;
					td2 = gfxrom[tileadd + 0x8000 + yidx] ^ 0xff;
					td1 = gfxrom[tileadd + 0x10000 + yidx] ^ 0xff;
					td0 = gfxrom[tileadd + 0x18000 + yidx] ^ 0xff;

					if (scale & 0x80)
					{
						for (x = 0; x < 8; ++x)
						{
							UINT8 col;
							UINT8 pix = x;

							if (!xflip)
								pix ^= 0x7;

							col = BIT(td0, pix)
								| (BIT(td1, pix) << 1)
								| (BIT(td2, pix) << 2)
								| (BIT(td3, pix) << 3);

							DRAW_OBJ_PIXEL(col);

							if (BIT(sc, x))
								DRAW_OBJ_PIXEL(col);
						}
					}
					else
					{
						for (x = 0; x < 8; ++x)
						{
							UINT8 col;
							UINT8 pix = x;

							if (BIT(sc, x))
							{
								if (!xflip)
									pix ^= 0x7;

								col = BIT(td0, pix)
									| (BIT(td1, pix) << 1)
									| (BIT(td2, pix) << 2)
									| (BIT(td3, pix) << 3);

								DRAW_OBJ_PIXEL(col);
							}
						}
					}
				}
			}
		}

		/* Check for the end of list marker */
		if (m_object_ram[offs + 1] & 0x8000)
			return;
	}
}

#define INCREMENT(ACC, CNT)              \
do {                                     \
	carry = (UINT8)d##ACC > (UINT8)~ACC; \
	ACC += d##ACC;                       \
	if (carry) ++CNT;                    \
} while(0)

#define DECREMENT(ACC, CNT)              \
do {                                     \
	carry = (UINT8)d##ACC > (UINT8)ACC;  \
	ACC -= d##ACC;                       \
	if (carry) --CNT;                    \
} while(0)

static void rotate_draw()
{
	/* Counters */
	UINT32 cxy = m_xsal & 0xff;
	UINT32 cyy = m_ysal & 0x1ff;

	/* Accumulator values and deltas */
	UINT8 axy  = m_x0ll & 0xff;
	UINT8 daxy = m_dx0ll & 0xff;
	UINT8 ayy  = m_y0ll & 0xff;
	UINT8 dayy = m_dy0ll & 0xff;
	UINT8 dayx = m_dyll & 0xff;
	UINT8 daxx = m_dxll & 0xff;

	UINT32 xy_up = BIT(m_xsal, 8);
	UINT32 yx_up = BIT(m_dyll, 9);
	UINT32 axx_en  = !BIT(m_dxll, 8);
	UINT32 ayx_en  = !BIT(m_dyll, 8);
	UINT32 axy_en  = !BIT(m_dx0ll, 8);
	UINT32 ayy_en  = !BIT(m_dy0ll, 8);

	for (UINT32 y = 0; y < nScreenHeight; ++y)
	{
		UINT32 carry;
		UINT16 *dst = BurnBitmapGetPosition(0, 0, y);

		UINT32 cx = cxy;
		UINT32 cy = cyy;

		UINT8 axx = axy;
		UINT8 ayx = ayy;

		for (UINT32 x = 0; x < nScreenWidth; ++x)
		{
			cx &= 0x1ff;
			cy &= 0x1ff;

			*dst++ = BurnBitmapGetPosition(1 + (back_buffer_select ^ 1), cx, cy)[0]; //m_front_buffer->pix16(cy, cx);

			if (axx_en)
				INCREMENT(axx, cx);
			else
				++cx;

			if (ayx_en)
			{
				if (yx_up)
					INCREMENT(ayx, cy);
				else
					DECREMENT(ayx, cy);
			}
			else
			{
				if (yx_up)
					++cy;
				else
					--cy;
			}
		}

		if (axy_en)
		{
			if (xy_up)
				INCREMENT(axy, cxy);
			else
				DECREMENT(axy, cxy);
		}
		else
		{
			if (xy_up)
				++cxy;
			else
				--cxy;
		}

		if (ayy_en)
			INCREMENT(ayy, cyy);
		else
			++cyy;

		cxy &= 0xff;
		cyy &= 0x1ff;
	}
}

static void hud_draw()
{
	UINT8  *tile_rom = DrvGfxROM[2];
	UINT16 *m_hud_ram = (UINT16*)DrvHudRAM;

	for (UINT32 offs = 0x0; offs < 0x200; offs += 2)
	{
		UINT32 y_pos;
		UINT32 x_pos;
		UINT32 y_size;
		UINT32 x_size;
		UINT32 layout;
		UINT16 colour;
		UINT32 code;
		UINT32 rom_a12_7;

		/* End of sprite list marker */
		if (m_hud_ram[offs + 1] & 0x8000)
			break;

		y_pos   = m_hud_ram[offs] & 0x1ff;
		x_pos   = m_hud_ram[offs + 1] & 0x1ff;
		x_size = (m_hud_ram[offs + 1] >> 12) & 7;
		code    = (m_hud_ram[offs] >> 9) & 0x7f;
		colour = 0x200 + ((m_hud_ram[offs + 1] >> 9) & 7);
		layout = (code >> 5) & 3;
		rom_a12_7 = (code & 0xfe) << 6;

		/* Account for line buffering */
		y_pos -= 1;

		if (layout == 3)
			y_size = 32;
		else if (layout == 2)
			y_size = 16;
		else
			y_size = 8;

		for (UINT32 y = 0; y < nScreenHeight; ++y)
		{
			UINT32 xt;
			UINT32 cy;

			cy = y_pos + y;

			if (cy < 0x200)
				continue;

			if ((cy & 0xff) == y_size)
				break;

			for (xt = 0; xt <= x_size; ++xt)
			{
				UINT32 rom_a6_3;
				UINT32 px;
				UINT8  gfx_strip;

				if (layout == 3)
					rom_a6_3 = (BIT(cy, 4) << 3) | (BIT(cy, 3) << 2) | (BIT(xt, 1) << 1) | BIT(xt, 0);
				else if (layout == 2)
					rom_a6_3 = ((BIT(code, 0) << 3) | (BIT(xt, 1) << 2) | (BIT(cy, 3) << 1) | (BIT(xt, 0)));
				else
					rom_a6_3 = (BIT(code, 0) << 3) | (xt & 7);

				rom_a6_3 <<= 3;

				/* Get tile data */
				gfx_strip = tile_rom[rom_a12_7 | rom_a6_3 | (cy & 7)];

				if (gfx_strip == 0)
					continue;

				/* Draw */
				for (px = 0; px < 8; ++px)
				{
					UINT32 x = x_pos + (xt << 3) + px;

					if (x < nScreenWidth)
					{
						UINT16 *const dst = BurnBitmapGetPosition(0, x, y);

						if (BIT(gfx_strip, px ^ 7) && *dst > 255)
							*dst = colour;
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (reload_hack) {
		scene_draw();
		ground_draw(1);
		objects_draw();
		back_buffer_select ^= 1;

		reload_hack = 0;
	}

	if (m_ctrl_reg & 0x80) {
		if (nBurnLayer & 1) rotate_draw();

		if (nBurnLayer & 2) GenericTilemapDraw(0, 0, 0);

		if (nBurnLayer & 4) hud_draw();
	} else {
		BurnTransferClear(0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	VezNewFrame();
	ZetNewFrame();
	timerNewFrame();

	{
		Diag.Toggle(DrvJoy1[3]);

		DrvInputs[0] = 0x00;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		DrvInputs[1] = (DrvJoy2[0] & 1) ? 0 : 0xff;
		DrvInputs[2] = (DrvJoy3[0] & 1) ? 0 : 0xff;
	}

	INT32 nInterleave = 280 * 8;
	INT32 nCyclesTotal[5] = { (INT32)(8000000 / 55.803571), (INT32)(8000000 / 55.803571), (INT32)(8000000 / 55.803571), (INT32)(4000000 / 55.803571), (INT32)(10000000 / 55.803571) };
	INT32 nCyclesDone[5] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2], nExtraCycles[3], nExtraCycles[4] };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		VezOpen(0);
		if (i == (239 * 8) + 4 && main_inten) VezSetIRQLineAndVector(0, 0, 0xff, CPU_IRQSTATUS_ACK); // should be 239y, 168x (hits at 239y 160x)
		CPU_RUN(0, Vez);
		VezClose();

		VezOpen(1);
		CPU_RUN(1, Vez);
		VezClose();

		VezOpen(2);
		CPU_RUN(2, Vez);
		VezClose();

		CPU_RUN_TIMER(3); // fm timers

		CPU_RUN(4, timer); // bufend timer (@10mhz, frame buffer)

		if (i == nInterleave - 1) {
			back_buffer_select ^= 1;
			scene_draw();
			ground_draw(0);
			objects_draw();

			if (pBurnDraw) BurnDrvRedraw();
		}
	}

	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];
	nExtraCycles[3] = 0;//nCyclesDone[3] - nCyclesTotal[3]; // fm timer doesn't need
	nExtraCycles[4] = nCyclesDone[4] - nCyclesTotal[4];

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin =  0x029671;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		VezScan(nAction);
		ZetScan(nAction);

		BurnWatchdogScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		timerScan();

		Diag.Scan();

		SCAN_VAR(m_iden);
		SCAN_VAR(m_ctrl_reg);
		SCAN_VAR(m_xsal);
		SCAN_VAR(m_x0ll);
		SCAN_VAR(m_dx0ll);
		SCAN_VAR(m_dxll);
		SCAN_VAR(m_ysal);
		SCAN_VAR(m_y0ll);
		SCAN_VAR(m_dy0ll);
		SCAN_VAR(m_dyll);
		SCAN_VAR(main_inten);
		SCAN_VAR(ground_ctrl);
		SCAN_VAR(m_obj_pal_latch);
		SCAN_VAR(m_obj_pal_addr);
		SCAN_VAR(back_buffer_select);
		SCAN_VAR(sound_volume);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		reload_hack = 1; // flux capacitor makes time travel possible
		lockon_sound_write(0x7000, sound_volume); // set volumes
	}

	return 0;
}


// Lock-On (rev. E)

static struct BurnRomInfo lockonRomDesc[] = {
	{ "lo1_02c.89",		0x08000, 0xbbf17263, 1 | BRF_PRG | BRF_ESS }, //  0 Main V30 Code
	{ "lo1_03c.88",		0x08000, 0xfa58fd36, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lo1_04c.77",		0x08000, 0x4a88576e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lo1_05c.76",		0x08000, 0x5a171b02, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "lo1_00e.96",		0x08000, 0x25eec97f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "lo1_01e.95",		0x08000, 0x03f0391d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "lo3_01a.30",		0x08000, 0x3eacdb6b, 2 | BRF_PRG | BRF_ESS }, //  6 Ground V30 Code
	{ "lo3_03a.33",		0x08000, 0x4ce96d71, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "lo3_00b.29",		0x08000, 0x1835dccb, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "lo3_02b.32",		0x08000, 0x2b8931d3, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "lo4_00b",		0x08000, 0x5f6b5a50, 3 | BRF_PRG | BRF_ESS }, // 10 Object V30 Code
	{ "lo4_01b",		0x08000, 0x7e88bcf2, 3 | BRF_PRG | BRF_ESS }, // 11

	{ "lo1_08b.24",		0x08000, 0x73860ec9, 4 | BRF_PRG | BRF_ESS }, // 12 Sound Code

	{ "lo1_07a",		0x02000, 0x73673b79, 5 | BRF_GRA },           // 13 Characters
	{ "lo1_06a",		0x02000, 0xc8205913, 5 | BRF_GRA },           // 14

	{ "lo3_12a.120",	0x10000, 0xa34262a7, 6 | BRF_GRA },           // 15 Scene Tiles & Clut
	{ "lo3_11a.119",	0x10000, 0x018efa36, 6 | BRF_GRA },           // 16
	{ "lo3_10a.118",	0x10000, 0xd5f4a8f3, 6 | BRF_GRA },           // 17
	{ "lo3_13a.121",	0x10000, 0xe44774a7, 6 | BRF_GRA },           // 18

	{ "lo2_00.53",		0x02000, 0x8affea15, 7 | BRF_GRA },           // 19 Hud Sprites

	{ "lo3_07a.94",		0x10000, 0xcebc50e1, 8 | BRF_GRA },           // 20 Ground Graphics & Lut
	{ "lo3_06a.92",		0x10000, 0xf6b6ebdd, 8 | BRF_GRA },           // 21
	{ "lo3_05a.90",		0x10000, 0x5b6f4c8e, 8 | BRF_GRA },           // 22
	{ "lo3_08.104",		0x10000, 0xf418cecd, 8 | BRF_GRA },           // 23
	{ "lo3_09.105",		0x10000, 0x3c245568, 8 | BRF_GRA },           // 24
	{ "lo3_04a.88",		0x10000, 0x80b67ba9, 8 | BRF_GRA },           // 25

	{ "lo5_28a.76",		0x08000, 0x1186f9b4, 9 | BRF_GRA },           // 26 Object Tiles
	{ "lo5_20a.48",		0x08000, 0x3c1a67b5, 9 | BRF_GRA },           // 27
	{ "lo5_08a.58",		0x08000, 0x9dedeff5, 9 | BRF_GRA },           // 28
	{ "lo5_00a.14",		0x08000, 0xe9f23ce6, 9 | BRF_GRA },           // 29
	{ "lo5_24a.62",		0x08000, 0x1892d083, 9 | BRF_GRA },           // 30
	{ "lo5_16a.32",		0x08000, 0xc4500159, 9 | BRF_GRA },           // 31
	{ "lo5_04a.28",		0x08000, 0x099323bc, 9 | BRF_GRA },           // 32
	{ "lo5_12a.8",		0x08000, 0x2f5164ab, 9 | BRF_GRA },           // 33
	{ "lo5_29a.77",		0x08000, 0x45353d8d, 9 | BRF_GRA },           // 34
	{ "lo5_21a.49",		0x08000, 0x39ce2000, 9 | BRF_GRA },           // 35
	{ "lo5_10a.72",		0x08000, 0x23eeec5a, 9 | BRF_GRA },           // 36
	{ "lo5_01a.15",		0x08000, 0x528d1395, 9 | BRF_GRA },           // 37
	{ "lo5_25a.93",		0x08000, 0x7f3418bd, 9 | BRF_GRA },           // 38
	{ "lo5_17a.33",		0x08000, 0xccf138d3, 9 | BRF_GRA },           // 39
	{ "lo5_06a.44",		0x08000, 0xbe539b01, 9 | BRF_GRA },           // 40
	{ "lo5_14a.22",		0x08000, 0xe63cd59e, 9 | BRF_GRA },           // 41
	{ "lo5_30a.78",		0x08000, 0x7d3993c5, 9 | BRF_GRA },           // 42
	{ "lo5_22a.50",		0x08000, 0xb1ed0361, 9 | BRF_GRA },           // 43
	{ "lo5_09a.59",		0x08000, 0x953289bc, 9 | BRF_GRA },           // 44
	{ "lo5_02a.16",		0x08000, 0x07aa32a1, 9 | BRF_GRA },           // 45
	{ "lo5_26a.64",		0x08000, 0xa0b5c040, 9 | BRF_GRA },           // 46
	{ "lo5_18a.34",		0x08000, 0x89884b24, 9 | BRF_GRA },           // 47
	{ "lo5_05a.29",		0x08000, 0xf6b775a2, 9 | BRF_GRA },           // 48
	{ "lo5_13a.9",		0x08000, 0x67fbb061, 9 | BRF_GRA },           // 49
	{ "lo5_31a.79",		0x08000, 0xd3595292, 9 | BRF_GRA },           // 50
	{ "lo5_23a.51",		0x08000, 0x1487895b, 9 | BRF_GRA },           // 51
	{ "lo5_11a.73",		0x08000, 0x9df0b287, 9 | BRF_GRA },           // 52
	{ "lo5_03a.17",		0x08000, 0x7aca5d83, 9 | BRF_GRA },           // 53
	{ "lo5_27a.65",		0x08000, 0x119ff70a, 9 | BRF_GRA },           // 54
	{ "lo5_19a.35",		0x08000, 0x5aaa6a53, 9 | BRF_GRA },           // 55
	{ "lo5_07a.45",		0x08000, 0x313f127f, 9 | BRF_GRA },           // 56
	{ "lo5_15a.23",		0x08000, 0x66f9c5db, 9 | BRF_GRA },           // 57

	{ "lo4_02.109",		0x08000, 0x0832edde, 10 | BRF_GRA },          // 58 Object Luts
	{ "lo4_03.108",		0x08000, 0x1efac891, 10 | BRF_GRA },          // 59

	{ "lo4_04a.119",	0x10000, 0x098f4151, 11 | BRF_GRA },          // 60 Object chunk Luts
	{ "lo4_05a.118",	0x10000, 0x3b21667c, 11 | BRF_GRA },          // 61

	{ "lo1a.5",			0x00400, 0x82391f30, 12 | BRF_GRA },          // 62 Color PROMs
	{ "lo2a.2",			0x00400, 0x2bfc6288, 12 | BRF_GRA },          // 63

	{ "lo_3.69",		0x00800, 0x9d9c41a9, 13 | BRF_GRA },          // 64 Object Scale PROMs
	{ "lo_4.68",		0x00800, 0xca4874ef, 13 | BRF_GRA },          // 65
};

STD_ROM_PICK(lockon)
STD_ROM_FN(lockon)

struct BurnDriver BurnDrvLockon = {
	"lockon", NULL, NULL, NULL, "1986",
	"Lock-On (rev. E)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lockonRomInfo, lockonRomName, NULL, NULL, NULL, NULL, LockonInputInfo, LockonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0xc00,
	320, 240, 4, 3
};


// Lock-On (rev. C)

static struct BurnRomInfo lockoncRomDesc[] = {
	{ "lo1_02c.89",		0x08000, 0xbbf17263, 1 | BRF_PRG | BRF_ESS }, //  0 Main V30 Code
	{ "lo1_03c.88",		0x08000, 0xfa58fd36, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lo1_04c.77",		0x08000, 0x4a88576e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lo1_05c.76",		0x08000, 0x5a171b02, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "lo1_00c.96",		0x08000, 0xe2db493b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "lo1_01c.95",		0x08000, 0x3e6065e0, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "lo3_01a.30",		0x08000, 0x3eacdb6b, 2 | BRF_PRG | BRF_ESS }, //  6 Ground V30 Code
	{ "lo3_03a.33",		0x08000, 0x4ce96d71, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "lo3_00b.29",		0x08000, 0x1835dccb, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "lo3_02b.32",		0x08000, 0x2b8931d3, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "lo4_00b",		0x08000, 0x5f6b5a50, 3 | BRF_PRG | BRF_ESS }, // 10 Object V30 Code
	{ "lo4_01b",		0x08000, 0x7e88bcf2, 3 | BRF_PRG | BRF_ESS }, // 11

	{ "lo1_08b.24",		0x08000, 0x73860ec9, 4 | BRF_PRG | BRF_ESS }, // 12 Sound Code

	{ "lo1_07a",		0x02000, 0x73673b79, 5 | BRF_GRA },           // 13 Characters
	{ "lo1_06a",		0x02000, 0xc8205913, 5 | BRF_GRA },           // 14

	{ "lo3_12a.120",	0x10000, 0xa34262a7, 6 | BRF_GRA },           // 15 Scene Tiles & Clut
	{ "lo3_11a.119",	0x10000, 0x018efa36, 6 | BRF_GRA },           // 16
	{ "lo3_10a.118",	0x10000, 0xd5f4a8f3, 6 | BRF_GRA },           // 17
	{ "lo3_13a.121",	0x10000, 0xe44774a7, 6 | BRF_GRA },           // 18

	{ "lo2_00.53",		0x02000, 0x8affea15, 7 | BRF_GRA },           // 19 Hud Sprites

	{ "lo3_07a.94",		0x10000, 0xcebc50e1, 8 | BRF_GRA },           // 20 Ground Graphics & Lut
	{ "lo3_06a.92",		0x10000, 0xf6b6ebdd, 8 | BRF_GRA },           // 21
	{ "lo3_05a.90",		0x10000, 0x5b6f4c8e, 8 | BRF_GRA },           // 22
	{ "lo3_08.104",		0x10000, 0xf418cecd, 8 | BRF_GRA },           // 23
	{ "lo3_09.105",		0x10000, 0x3c245568, 8 | BRF_GRA },           // 24
	{ "lo3_04a.88",		0x10000, 0x80b67ba9, 8 | BRF_GRA },           // 25

	{ "lo5_28a.76",		0x08000, 0x1186f9b4, 9 | BRF_GRA },           // 26 Object Tiles
	{ "lo5_20a.48",		0x08000, 0x3c1a67b5, 9 | BRF_GRA },           // 27
	{ "lo5_08a.58",		0x08000, 0x9dedeff5, 9 | BRF_GRA },           // 28
	{ "lo5_00a.14",		0x08000, 0xe9f23ce6, 9 | BRF_GRA },           // 29
	{ "lo5_24a.62",		0x08000, 0x1892d083, 9 | BRF_GRA },           // 30
	{ "lo5_16a.32",		0x08000, 0xc4500159, 9 | BRF_GRA },           // 31
	{ "lo5_04a.28",		0x08000, 0x099323bc, 9 | BRF_GRA },           // 32
	{ "lo5_12a.8",		0x08000, 0x2f5164ab, 9 | BRF_GRA },           // 33
	{ "lo5_29a.77",		0x08000, 0x45353d8d, 9 | BRF_GRA },           // 34
	{ "lo5_21a.49",		0x08000, 0x39ce2000, 9 | BRF_GRA },           // 35
	{ "lo5_10a.72",		0x08000, 0x23eeec5a, 9 | BRF_GRA },           // 36
	{ "lo5_01a.15",		0x08000, 0x528d1395, 9 | BRF_GRA },           // 37
	{ "lo5_25a.93",		0x08000, 0x7f3418bd, 9 | BRF_GRA },           // 38
	{ "lo5_17a.33",		0x08000, 0xccf138d3, 9 | BRF_GRA },           // 39
	{ "lo5_06a.44",		0x08000, 0xbe539b01, 9 | BRF_GRA },           // 40
	{ "lo5_14a.22",		0x08000, 0xe63cd59e, 9 | BRF_GRA },           // 41
	{ "lo5_30a.78",		0x08000, 0x7d3993c5, 9 | BRF_GRA },           // 42
	{ "lo5_22a.50",		0x08000, 0xb1ed0361, 9 | BRF_GRA },           // 43
	{ "lo5_09a.59",		0x08000, 0x953289bc, 9 | BRF_GRA },           // 44
	{ "lo5_02a.16",		0x08000, 0x07aa32a1, 9 | BRF_GRA },           // 45
	{ "lo5_26a.64",		0x08000, 0xa0b5c040, 9 | BRF_GRA },           // 46
	{ "lo5_18a.34",		0x08000, 0x89884b24, 9 | BRF_GRA },           // 47
	{ "lo5_05a.29",		0x08000, 0xf6b775a2, 9 | BRF_GRA },           // 48
	{ "lo5_13a.9",		0x08000, 0x67fbb061, 9 | BRF_GRA },           // 49
	{ "lo5_31a.79",		0x08000, 0xd3595292, 9 | BRF_GRA },           // 50
	{ "lo5_23a.51",		0x08000, 0x1487895b, 9 | BRF_GRA },           // 51
	{ "lo5_11a.73",		0x08000, 0x9df0b287, 9 | BRF_GRA },           // 52
	{ "lo5_03a.17",		0x08000, 0x7aca5d83, 9 | BRF_GRA },           // 53
	{ "lo5_27a.65",		0x08000, 0x119ff70a, 9 | BRF_GRA },           // 54
	{ "lo5_19a.35",		0x08000, 0x5aaa6a53, 9 | BRF_GRA },           // 55
	{ "lo5_07a.45",		0x08000, 0x313f127f, 9 | BRF_GRA },           // 56
	{ "lo5_15a.23",		0x08000, 0x66f9c5db, 9 | BRF_GRA },           // 57

	{ "lo4_02.109",		0x08000, 0x0832edde, 10 | BRF_GRA },          // 58 Object Luts
	{ "lo4_03.108",		0x08000, 0x1efac891, 10 | BRF_GRA },          // 59

	{ "lo4_04a.119",	0x10000, 0x098f4151, 11 | BRF_GRA },          // 60 Object chunk Luts
	{ "lo4_05a.118",	0x10000, 0x3b21667c, 11 | BRF_GRA },          // 61

	{ "lo1a.5",			0x00400, 0x82391f30, 12 | BRF_GRA },          // 62 Color PROMs
	{ "lo2a.2",			0x00400, 0x2bfc6288, 12 | BRF_GRA },          // 63

	{ "lo_3.69",		0x00800, 0x9d9c41a9, 13 | BRF_GRA },          // 64 Object Scale PROMs
	{ "lo_4.68",		0x00800, 0xca4874ef, 13 | BRF_GRA },          // 65
};

STD_ROM_PICK(lockonc)
STD_ROM_FN(lockonc)

struct BurnDriver BurnDrvLockonc = {
	"lockonc", "lockon", NULL, NULL, "1986",
	"Lock-On (rev. C)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lockoncRomInfo, lockoncRomName, NULL, NULL, NULL, NULL, LockonInputInfo, LockoneDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0xc00,
	320, 240, 4, 3
};
