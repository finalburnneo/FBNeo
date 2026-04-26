// FB Neo Atari System 1 hardware driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarijsa.h"
#include "burn_ym2151.h"
#include "slapstic.h"
#include "burn_gun.h"
#include "pokey.h"
#include "tms5220.h"
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvAlphaROM;
static UINT8 *DrvTileROM;
static UINT8 *DrvGfxROM[16];
static UINT8 *DrvMapPROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static dtimer joy_timer;
static INT32 joystick_int;
static INT32 joystick_int_enable;
static INT32 joystick_value;
static INT32 scanline_int_state;
static INT32 video_int_state;
static UINT16 scrollx;
static UINT16 scrolly, scrolly_adj;
static UINT16 playfield_priority_pens;
static UINT8 playfield_tile_bank;
static UINT8 atarisy1_bankselect;
static INT32 next_timer_scanline;

static INT32 scanline;
static INT32 vblank;
static UINT8 invert_input0 = 0;
static UINT16 playfield_lookup[256];
static UINT8 bank_gfx[3][8];
static UINT8 bank_color_shift[16];
static UINT8 (*joystick_callback)(INT32) = NULL;

static INT32 has_trackball = 0;
static INT32 is_roadb = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;
static INT16 Analog[4];

static INT32 nExtraCycles;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo MarbleInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	A("P1 Trackball X",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"	),
	A("P1 Trackball Y",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"	),

	{"P2 Coin",			    BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	A("P2 Trackball X",		BIT_ANALOG_REL,	&Analog[2],		"p2 x-axis"	),
	A("P2 Trackball Y",		BIT_ANALOG_REL,	&Analog[3],		"p2 y-axis"	),

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Marble)

static struct BurnInputInfo PeterpakInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"Up",					BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"Down",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"Left",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"Right",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"Left Throw/P1 Start",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"Right Throw/P2 Start",BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"Jump",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Peterpak)

static struct BurnInputInfo IndytempInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"Up",					BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"Down",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"Left",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"Right",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"Left Whip/P1 Start",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"Right Whip/P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Indytemp)

static struct BurnInputInfo Reliefs1InputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Stick X",     	BIT_ANALOG_REL, &Analog[3],		"p1 x-axis" ),
	A("P1 Stick Y",     	BIT_ANALOG_REL, &Analog[2],		"p1 y-axis" ),

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	A("P2 Stick X",     	BIT_ANALOG_REL, &Analog[1],		"p2 x-axis" ),
	A("P2 Stick Y",     	BIT_ANALOG_REL, &Analog[0],		"p2 y-axis" ),

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Reliefs1)

static struct BurnInputInfo RoadrunnInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"Left Hop/P1 Start",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"Right Hop/P2 Starrt",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	A("P1 Stick X",     	BIT_ANALOG_REL, &Analog[1],		"p1 x-axis" ),
	A("P1 Stick Y",     	BIT_ANALOG_REL, &Analog[0],		"p1 y-axis" ),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Roadrunn)

static struct BurnInputInfo RoadblstInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"Lasers",				BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"Special Weapon",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	A("P1 Wheel",       	BIT_ANALOG_REL, &Analog[0],		"p1 x-axis" ),
	A("P1 Accelerator", 	BIT_ANALOG_REL, &Analog[1],		"p1 fire 3" ),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Roadblst)

#undef A

#define SERVICE_MODE												\
	{0   , 0xfe, 0   ,    2, "Service Mode"					},		\
	{0x00, 0x01, 0x40, 0x40, "Off"							},		\
	{0x00, 0x01, 0x40, 0x00, "On"							},		\

#define DIPCONFIG(_name, _startinp, _defaultbios)					\
static struct BurnDIPInfo _name##DIPList[]=							\
{																	\
	{_startinp, 0xf0, 0xff, 0xff, NULL						},		\
	{0x00, 0xff, 0xff, 0x60 | _defaultbios, NULL			},		\
																	\
	{0   , 0xfe, 0   ,    3, "BIOS Version"					},		\
	{0x00, 0x01, 0x03, 0x00, "TTL Motherboard (Version 2)"	},		\
	{0x00, 0x01, 0x03, 0x01, "TTL Motherboard (Version 1)"	},		\
	{0x00, 0x01, 0x03, 0x02, "LSI Motherboard"				},		\
																	\
	SERVICE_MODE													\
};																	\
																	\
STDDIPINFO(_name)

#define DIPLSIONLY(_name, _startinp)								\
static struct BurnDIPInfo _name##DIPList[]=							\
{																	\
	{_startinp, 0xf0, 0xff, 0xff, NULL						},		\
	{0x00, 0xff, 0xff, 0x62, NULL							},		\
																	\
	SERVICE_MODE													\
};																	\
																	\
STDDIPINFO(_name)

static struct BurnDIPInfo IndytemcDIPList[]=
{
	{0x0a, 0xf0, 0xff, 0xff, NULL							},
	{0x00, 0xff, 0xff, 0x42, NULL							}, // LSI BIOS!

	SERVICE_MODE

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x00, 0x01, 0x20, 0x20, "On (Not Supported)"			},
	{0x00, 0x01, 0x20, 0x00, "Off"							},
};

STDDIPINFO(Indytemc)

DIPCONFIG(Marble,		0x0a, 0x00)
DIPCONFIG(Peterpak,		0x0b, 0x00)
DIPCONFIG(Indytemp,		0x0a, 0x00)
DIPCONFIG(Roadrunn,		0x08, 0x00)
DIPCONFIG(Roadblst,		0x08, 0x00)
DIPLSIONLY(Reliefs1,    0x0c)
DIPLSIONLY(RoadblstLSI,	0x08)

static void partial_update(); // forward

static void sync_sound()
{
	BurnTimerUpdate(SekTotalCycles() / 4);
}

static void update_interrupts()
{
	INT32 state = 0;

	if (joystick_int && joystick_int_enable) state = 2;
	if (scanline_int_state) state = 3;
	if (video_int_state) state = 4;
	if (atarijsa_int_state) state = 6;

	if (state) {
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	}
}

static void update_timers(INT32 line)
{
	UINT16 *ram = (UINT16*)DrvMobRAM;
	UINT16 *base = &ram[atarimo_get_bank(0) * 64 * 4];
	int link = 0, best = line, found = 0;
	UINT8 spritevisit[64];

	memset(spritevisit, 0, sizeof(spritevisit));

	while (!spritevisit[link])
	{
		if (base[link + 0x40] == 0xffff)
		{
			int data = base[link];
			int vsize = (data & 15) + 1;
			int ypos = (256 - (data >> 5) - vsize * 8 - 1) & 0x1ff;

			found = 1;

			if (best <= line)
			{
				if ((ypos <= scanline && ypos < best) || ypos > scanline)
					best = ypos;
			}
			else
			{
				if (ypos < best)
					best = ypos;
			}
		}

		spritevisit[link] = 1;
		link = base[link + 0xc0] & 0x3f;
	}

	if (!found)
		best = -1;

	/* update the timer */
	if (best != next_timer_scanline)
	{
		next_timer_scanline = best;
	}
}

static void bankselect(UINT8 data)
{
	UINT8 diff = atarisy1_bankselect ^ data;

	if (diff & 0x80) {
		sync_sound();
		AtariJSAResetWrite(~data & 0x80); // 0 = assert, 1 = clear
	}

	if (diff & 0x3c) {
		partial_update();
	}

	atarimo_set_bank(0, (data >> 3) & 7);
	update_timers(scanline);

	playfield_tile_bank = (data >> 2) & 1;

	atarisy1_bankselect = data;
}

static void __fastcall main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0xa02000) {
		INT32 active_bank = atarimo_get_bank(0);
		INT32 offset = (address & 0xfff) / 2;
		UINT16 *ram = (UINT16*)DrvMobRAM;
		INT32 oldword = ram[offset];

		if (oldword != data && (offset >> 8) == active_bank)
		{
			if (((offset & 0xc0) == 0x00 && ram[offset | 0x40] == 0xffff) ||
			    ((offset & 0xc0) == 0x40 && (data == 0xffff || oldword == 0xffff)))
			{
				ram[offset] = data;
				AtariMoWrite(0, (address & 0x0fff) >> 1, data);
				update_timers(scanline);
			}
			else
			{
				scanline += 2; // needed for roadrunner seed meter
				partial_update();
				scanline -= 2;
			}
		}
		ram[offset] = data;
		AtariMoWrite(0, (address & 0x0fff) >> 1, data);
		return;
	}

	if ((address & 0xffffe0) == 0xf40000) {
		joystick_int_enable = ((address >> 4) & 1) ^ 1;
		return;
	}

	switch (address)
	{
		case 0x800000:
			partial_update();
			scrollx = data;
		return;

		case 0x820000:
			partial_update();
			scrolly = data;
			scrolly_adj = data - (scanline + 1);
		return;

		case 0x840000:
			partial_update();
			playfield_priority_pens = data;
		return;

		case 0x860000:
			bankselect(data & 0xff);
		return;

		case 0x880000:
		case 0x880002: // peterpak writes here
			BurnWatchdogWrite();
		return;

		case 0x8a0000:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x8c0000:
			AtariEEPROMUnlockWrite();
		return;

		case 0xf80000:
		case 0xfe0000:
			sync_sound();
			AtariJSAWrite(data & 0xff);
		return;
	}
	if (address >= 0x80000) // lots of bad writes (roadblst)
		bprintf (0, _T("WW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static void __fastcall main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0xa02000) {
		DrvMobRAM[(address & 0x0fff)^1] = data;
		if (address&1)
			AtariMoWrite(0, (address & 0x0fff) >> 1, BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMobRAM + (address & 0x0ffe)))));
		return;
	}

	if ((address & 0xffffe0) == 0xf40000) {
		joystick_int_enable = ((address >> 4) & 1) ^ 1;
		return;
	}

	switch (address)
	{
		case 0x840000:
		case 0x840001:
			partial_update();
			playfield_priority_pens = data;
		return;

		case 0x860001:
			bankselect(data);
		return;

		case 0x880001:
			BurnWatchdogWrite();
		return;

		case 0x8a0001:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x8c0001:
			AtariEEPROMUnlockWrite();
		return;

		case 0xf80001:
		case 0xfe0001:
			sync_sound();
			AtariJSAWrite(data);
		return;
	}
	if (address >= 0x80000) // lots of bad writes (reliefs1)
		bprintf (0, _T("WB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static UINT8 joystick_read(INT32 address)
{
	joystick_int_enable = ((address >> 4) & 1) ^ 1;

	joystick_int = 0;
	update_interrupts();

	joy_timer.start(715, -1, 1, 0);

	UINT8 ret = joystick_value;
	joystick_value = joystick_callback(address);
	return ret;
}

static UINT16 trackball_read(INT32 address)
{
	if (is_roadb) {
		return ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x00, 0x7f);
	}

	const INT32 player = (address & 2) >> 1;
	const UINT8 x = BurnTrackballRead(player, 0);
	const UINT8 y = BurnTrackballRead(player, 1);

	return (address & 1) ? (x - y) : (x + y);
}


static UINT16 __fastcall main_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0xf40000) {
		return joystick_read(address);
	}

	switch (address)
	{
		case 0x2e0000:
			return scanline_int_state ? 0x0080 : 0x0000;
		case 0x2e0002:
			return joystick_int ? 0x0080 : 0x0000; // maybe?

		case 0xf20000:
		case 0xf20002:
		case 0xf20004:
		case 0xf20006:
			return trackball_read((address & 7) >> 1);

		case 0xf60000:
		case 0xf60002:
		{
			sync_sound();
			UINT16 ret = (DrvInputs[1] & ~0x200) | ((DrvDips[0] & 0x20) << 4); // indytempc
			ret &= ~0x00d0;
			ret |= vblank ? 0 : 0x10;
			ret |= DrvDips[0] & 0x40;
			ret |= atarigen_cpu_to_sound_ready ? 0x0080 : 0;
			return ret;
		}

		case 0xfc0000:
		case 0xfc0001:
			sync_sound();
			return AtariJSARead();
	}
	bprintf (0, _T("RW: %5.5x, PC(%5.5x)\n"), address, SekGetPC(-1));

	return 0;
}

static UINT8 __fastcall main_read_byte(UINT32 address)
{
	return (main_read_word(address & ~1) >> ((~address & 1) * 8)) & 0xff;
}

static UINT8 digital_joystick(INT32 address)
{
	INT32 select = 0x80 >> ((address & 0x1e) >> 1); // 0xe?
	return (DrvInputs[0] & select) ? 0xf0 : 0x00;
}

static UINT8 analog_joystick(INT32 address)
{
	address = (address & 0x1e) >> 1;

	// roadrunner, reliefs1
	switch (address) {
		case 4: // reliefs1 p1
			return ProcessAnalog(Analog[2], 0, INPUT_DEADZONE, 0x10, 0xef);
		case 5:
			return ProcessAnalog(Analog[3], 1, INPUT_DEADZONE, 0x10, 0xef);

		case 6: // roadrunn p1, reliefs1 p2
			return ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x10, 0xef);
		case 7:
			return ProcessAnalog(Analog[1], 1, INPUT_DEADZONE, 0x10, 0xef);
	}

	return 0x80;
}

static UINT8 analog_pedal(INT32 address)
{
	return ProcessAnalog(Analog[1], 0, INPUT_DEADZONE | INPUT_MIGHTBEDIGITAL | INPUT_LINEAR, 0x00, 0xff);
}

static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvPfRAM + (offs * 2)));

	UINT16 lookup = playfield_lookup[((data >> 8) & 0x7f) | (playfield_tile_bank << 7)];
	INT32 gfxindex = (lookup >> 8) & 0xf;
	INT32 code = ((lookup & 0xff) << 8) | (data & 0xff);
	INT32 color = 0x20 + (((lookup >> 12) & 15) << bank_color_shift[gfxindex]);

	TILE_SET_INFO(gfxindex, code, color, ((data >> 15) & 1)); // flipx?
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + (offs * 2)));
	INT32 code = data & 0x3ff;
	INT32 color = (data >> 10) & 0x07;
	INT32 opaque = data & 0x2000;

	TILE_SET_INFO(16, code, color, opaque ? TILE_OPAQUE : 0);
}

static INT32 DrvDoReset(INT32 nClearMem)
{
	if (nClearMem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	INT32 bios = (DrvDips[0] & 3) * 8;
	if ((DrvDips[0] & 3) != 3)
	{
		bprintf(0, _T("Load Bios: %d\n"), bios);
		BurnLoadRom(Drv68KROM + 0x000001, 0x80 + bios + 0, 2);
		BurnLoadRom(Drv68KROM + 0x000000, 0x80 + bios + 1, 2);
	}

	SekOpen(0);
	SekReset();
	timerReset();
	SekClose();

	BurnWatchdogReset();
	AtariEEPROMReset();
	AtariJSAReset();
	AtariSlapsticReset();

	BurnWatchdogReset();

	joystick_int = 0;
	joystick_int_enable = 0;
	joystick_value = 0;

	scanline_int_state = 0;
	video_int_state = 0;
	scrollx = 0;
	scrolly = 0;
	playfield_priority_pens = 0;
	playfield_tile_bank = 0;
	atarisy1_bankselect = 0;
	next_timer_scanline = -1;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecodeOne(UINT8 *src, UINT8 *dst, INT32 length, INT32 tiles, INT32 type)
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 XOffs0[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[8] = { STEP8(0,16) };

	INT32 Plane1[6] = { 5*8*0x10000, 4*8*0x10000, 3*8*0x10000, 2*8*0x10000, 1*8*0x10000, 0*8*0x10000 };
	INT32 XOffs1[8] = { STEP8(0,1) };
	INT32 YOffs1[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(length);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, src, length);

	if (type == 0)
	{
		GfxDecode(tiles, 2, 8, 8, Plane0, XOffs0, YOffs0, 0x080, tmp, dst);
	}
	else if (type < 4)
	{
		GfxDecode(tiles, 6 - (3 - type), 8, 8, Plane1 + (3 - type), XOffs1, YOffs1, 0x040, tmp, dst);
	}

	BurnFree(tmp);

	return 0;
}

// the color and remap PROMs are mapped as followss
#define PROM1_BANK_4			0x80		// active low
#define PROM1_BANK_3			0x40		// active low
#define PROM1_BANK_2			0x20		// active low
#define PROM1_BANK_1			0x10		// active low
#define PROM1_OFFSET_MASK		0x0f		// postive logic

#define PROM2_BANK_6_OR_7		0x80		// active low
#define PROM2_BANK_5			0x40		// active low
#define PROM2_PLANE_5_ENABLE	0x20		// active high
#define PROM2_PLANE_4_ENABLE	0x10		// active high
#define PROM2_PF_COLOR_MASK		0x0f		// negative logic
#define PROM2_BANK_7			0x08		// active low, plus PROM2_BANK_6_OR_7 low as well
#define PROM2_MO_COLOR_MASK		0x07		// negative logic

static INT32 get_bank(UINT8 prom1, UINT8 prom2, int bpp, INT32 &gfx_index)
{
	int bank_index = 0;

	if ((prom1 & PROM1_BANK_1) == 0)
		bank_index = 1;
	else if ((prom1 & PROM1_BANK_2) == 0)
		bank_index = 2;
	else if ((prom1 & PROM1_BANK_3) == 0)
		bank_index = 3;
	else if ((prom1 & PROM1_BANK_4) == 0)
		bank_index = 4;
	else if ((prom2 & PROM2_BANK_5) == 0)
		bank_index = 5;
	else if ((prom2 & PROM2_BANK_6_OR_7) == 0)
	{
		if ((prom2 & PROM2_BANK_7) == 0)
			bank_index = 7;
		else
			bank_index = 6;
	}
	else {
		return 0;
	}

	if (bank_gfx[bpp - 4][bank_index])
		return bank_gfx[bpp - 4][bank_index];

	if (0x80000 * (bank_index - 1) >= 0x380000) {
		return 0;
	}

	UINT8 *srcdata = DrvTileROM + (0x80000 * (bank_index - 1));

	if (bpp >= 4 && bpp <= 6) {
		DrvGfxDecodeOne(srcdata, DrvGfxROM[gfx_index], 0x80000, 0x1000, bpp - 3);
	}
	bank_color_shift[gfx_index] = bpp - 3;
	return bank_gfx[bpp - 4][bank_index] = gfx_index++;
}

static int decode_gfx(UINT16 *pflookup, UINT16 *molookup)
{
	UINT8 *prom1 = DrvMapPROM + 0x000;
	UINT8 *prom2 = DrvMapPROM + 0x200;

	INT32 gfx_index = 1; // must start at 1! -dink
	memset(&bank_gfx, 0, sizeof(bank_gfx));

	for (INT32 obj = 0; obj < 2; obj++)
	{
		for (INT32 i = 0; i < 256; i++, prom1++, prom2++)
		{
			INT32 bpp = 4;
			if (*prom2 & PROM2_PLANE_4_ENABLE)
			{
				bpp = 5;
				if (*prom2 & PROM2_PLANE_5_ENABLE)
					bpp = 6;
			}

			INT32 offset = *prom1 & PROM1_OFFSET_MASK;
			INT32 bank = get_bank(*prom1, *prom2, bpp, gfx_index);

			if (obj == 0)
			{
				INT32 color = (~*prom2 & PROM2_PF_COLOR_MASK) >> (bpp - 4);
				if (bank == 0)
				{
					bank = 1;
					offset = color = 0;
				}
				pflookup[i] = offset | (bank << 8) | (color << 12);
			}
			else
			{
				INT32 color = (~*prom2 & PROM2_MO_COLOR_MASK) >> (bpp - 4);
				molookup[i] = offset | (bank << 8) | (color << 12);
			}
		}
	}

	return 1;
}


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x090000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvAlphaROM			= Next; Next += 0x008000 * 2; // roadblstcg has twice the characters
	DrvTileROM			= Next; Next += 0x400000;
	for (INT32 i = 0; i < 16; i++) {
		DrvGfxROM[i]	= Next; Next += 0x040000;
	}

	DrvMapPROM			= Next; Next += 0x000400;

	DrvPalette			= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam				= Next;

	DrvPalRAM			= Next; Next += 0x000800;
	DrvPfRAM			= Next; Next += 0x002000;
	DrvMobRAM			= Next; Next += 0x001000;
	DrvAlphaRAM			= Next; Next += 0x001000;
	Drv68KRAM[0]		= Next; Next += 0x002000;
	Drv68KRAM[1]		= Next; Next += 0x100000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void joy_timer_cb(int param)
{
	joystick_int = 1;
	update_interrupts();
}


static INT32 CommonInit(INT32 (*pLoadCallback)(), INT32 slapstic_id, UINT8 (*joystick_cb)(INT32))
{
	static const struct atarimo_desc modesc =
	{
		0,					// index to which gfx system
		8,					// number of motion object banks
		1,					// are the entries linked?
		1,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		0,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0x38,				// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0,0,0,0x003f }},	// mask for the link
		{{ 0,0xff00,0,0 }},	// mask for the graphics bank
		{{ 0,0xffff,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0xff00,0,0 }},	// mask for the color
		{{ 0,0,0x3fe0,0 }},	// mask for the X position
		{{ 0x3fe0,0,0,0 }},	// mask for the Y position
		{{ 0 }},			// mask for the width, in tiles
		{{ 0x000f,0,0,0 }},	// mask for the height, in tiles
		{{ 0x8000,0,0,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0,0,0x8000,0 }},	// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0,0xffff,0,0 }},	// mask for the special value
		0xffff,				// resulting value to indicate "special"
		0					// callback routine for special entries
	};

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvAlphaROM, 0x82, 1)) return 1; // load alpha font rom

		if (pLoadCallback) {
			if (pLoadCallback()) return 1;
		}

		DrvGfxDecodeOne(DrvAlphaROM, DrvAlphaROM, 0x4000, 0x0200, 0);
	}

	SekInit(0, 0x68010);
	SekOpen(0);
	AtariSlapsticInit(Drv68KROM + 0x80000, slapstic_id);
	AtariEEPROMInit(0x1000);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	AtariSlapsticInstallMap(1,	0x080000);
	SekMapMemory(Drv68KRAM[0],	0x400000, 0x401fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],	0x900000, 0x9fffff, MAP_RAM);
	SekMapMemory(DrvPfRAM,		0xa00000, 0xa01fff, MAP_RAM);
	SekMapMemory(DrvMobRAM,		0xa02000, 0xa02fff, MAP_ROM); // handler
	SekMapMemory(DrvAlphaRAM,	0xa03000, 0xa03fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xb00000, 0xb007ff, MAP_RAM);
	AtariEEPROMInstallMap(2, 	0xf00000, 0xf00fff);
	SekSetWriteWordHandler(0,	main_write_word);
	SekSetWriteByteHandler(0,	main_write_byte);
	SekSetReadWordHandler(0,	main_read_word);
	SekSetReadByteHandler(0,	main_read_byte);
	m68k_set_insn_cb(timerRun);
	SekClose();

	timerInit();
	timerAdd(joy_timer, 0, joy_timer_cb);

	AtariSys1JSAInit(DrvM6502ROM, &update_interrupts);

	BurnWatchdogInit(DrvDoReset, 8+1); // marble 2,3,5 need 1 more frame while booting.  mame uses 8, but, different frame boundaries)

	if (has_trackball) {
		BurnTrackballInit(2);
	}
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,    8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);

	for (INT32 i = 0; i < 16; i++) {
		GenericTilemapSetGfx(i, DrvGfxROM[i], 3, 8, 8, 0x40000, 0x100, 0x3ff);
	}
	GenericTilemapSetGfx(16, DrvAlphaROM,  2, 8, 8, 0x08000, 0, 0x3f);
	GenericTilemapSetTransparent(1, 0);
	//GenericTilemapSetOffsets(0, 0, -16);

	AtariMoInit(0, &modesc);
	atarimo_set_yscroll(0, 256);
	atarimo_set_palettebase(0, 0x100);

	{
		INT32 size = 0;
		UINT16 motable[256];

		memset(motable, 0, sizeof(motable));
		memset(playfield_lookup, 0, sizeof(playfield_lookup));

		decode_gfx(&playfield_lookup[0], &motable[0]);

		UINT16 *codelookup = atarimo_get_code_lookup(0, &size);

		for (INT32 i = 0; i < size; i++)
		{
			codelookup[i] = (i & 0xff) | ((motable[i >> 8] & 0xff) << 8);
		}

		UINT8 *colorlookup = atarimo_get_color_lookup(0, &size);
		UINT8 *gfxlookup = atarimo_get_gfx_lookup(0, &size);

		for (INT32 i = 0; i < size; i++)
		{
			colorlookup[i] = ((motable[i] >> 12) & 0xf) << 1;
			gfxlookup[i] = (motable[i] >> 8) & 0xf;
		}
	}

	joystick_callback = joystick_cb;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	timerExit();
	AtariJSAExit();
	AtariMoExit();
	AtariEEPROMExit();
	BurnWatchdogExit();
	AtariSlapsticExit();

	BurnFreeMemIndex();

	invert_input0 = 0;
	joystick_callback = NULL;

	is_roadb = 0;

	if (has_trackball) {
		BurnTrackballExit();
		has_trackball = 0;
	}
	return 0;
}

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++) {
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)	{
			if (mo[x] != 0xffff) {
				if (mo[x] & ATARIMO_PRIORITY_MASK) {
					if ((mo[x] & 0x0f) != 1) {
						pf[x] = 0x300 + ((pf[x] & 0x0f) << 4) + (mo[x] & 0x0f);
					}
				} else {
					if ((pf[x] & 0xf8) != 0 || !(playfield_priority_pens & (1 << (pf[x] & 0x07)))) {
						pf[x] = mo[x];
					}
				}

				mo[x] = 0xffff;
			}
		}
	}
}

static INT32 lastline = 0;
static INT32 partialcount = 0;

static void DrvDrawBegin()
{
	AtariPaletteUpdate4IRGB(DrvPalRAM, DrvPalette, 0x800);

	//bprintf(2, _T("             --  new frame  (partials: %d) --\n"), partialcount);

	partialcount = 0;

	lastline = 0;

	if (pBurnDraw) BurnTransferClear();
}

// Note:
// partial updates need to update to and _including_ the current scanline. -dink april 2026
// due to how the game buffers and latches things on the hw, scrollx/y & etc are for the next line.

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight /*|| scanline == lastline*/ || lastline > scanline) return;

	//bprintf(0, _T("%07d: partial %d - %d.    scrollx %d   scrolly %d\n"), nCurrentFrame, lastline, scanline, scrollx, scrolly_adj);
	partialcount++;

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline+1);

	struct atarimo_rect_list rectlist;
	AtariMoRender(0, &rectlist);
	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly_adj);
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nSpriteEnable & 1) copy_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	GenericTilesClearClip();

	lastline = scanline+1;
}

// DrvDraw() - this is called at the end of the partial chain, or, if the system
// requests an update of the video when paused or something like that.
static INT32 DrvDraw()
{
	if (DrvRecalc) {
		// for mode changes while paused.  The palette update @ DrvDrawBegin()
		// is the important one :)
		AtariPaletteUpdate4IRGB(DrvPalRAM, DrvPalette, 0x800);
		DrvRecalc = 0;
	}

	scanline = 240; // end partial updates.

	partial_update();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	//bprintf(0, _T("\n---   NEW FRAME   ---\n"));

	BurnWatchdogUpdate();

	SekNewFrame();
	M6502NewFrame();
	timerNewFrame();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0 ^ invert_input0;		// indytempc is low! ffff
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0x0087;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x40;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;

		if (has_trackball) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0]*4, Analog[1]*4, 0x00, 0x3f);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(1, Analog[2]*4, Analog[3]*4, 0x00, 0x3f);
			BurnTrackballUpdate(1);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)((double)7159090 / 59.92), (INT32)((double)1789773 / 59.92) };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	SekOpen(0);
	SekIdle(nExtraCycles); // SYNCINT
	timerIdle(nExtraCycles); // timer is 100% synced to Sek, use its rollover cycles
	M6502Open(0);

	vblank = 0;

	DrvDrawBegin();
	if (pBurnSoundOut) {
		BurnSoundClear();
	}

	scrolly_adj = scrolly; // reset scrolly_adj!

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (i == next_timer_scanline) {
			scanline_int_state = 1;
			update_interrupts();
			next_timer_scanline = -1;
			update_timers(scanline);
			CPU_RUN_SYNCINT(0, Sek); // finish line (since we're up to date, the next CPU_RUN_SYNCINT(0, Sek) will do nothing)
			scanline_int_state = 0; // off
			update_interrupts();
		}

		if ((i & 0x3f) == 0x3f) {
			// forced partial update: every 64th line
			partial_update();
		}

		if (i == 240) {
			partial_update();

			vblank = 1;
			video_int_state = 1;
			update_interrupts();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		CPU_RUN_SYNCINT(0, Sek);
		CPU_RUN_TIMER(1); // M6502
	}

	nExtraCycles = SekTotalCycles() - nCyclesTotal[0];

	M6502Close();
	SekClose();

	if (pBurnSoundOut) {
		AtariJSAUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		SekScan(nAction);
		timerScan();

		AtariMoScan(nAction, pnMin);
		AtariJSAScan(nAction, pnMin);
		AtariSlapsticScan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		if (has_trackball) {
			BurnTrackballScan();
		}

		SCAN_VAR(joystick_int);
		SCAN_VAR(joystick_int_enable);
		SCAN_VAR(joystick_value);
		SCAN_VAR(scanline_int_state);
		SCAN_VAR(video_int_state);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(playfield_priority_pens);
		SCAN_VAR(playfield_tile_bank);
		SCAN_VAR(atarisy1_bankselect);
		SCAN_VAR(next_timer_scanline);

		SCAN_VAR(nExtraCycles);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}



// Atari System 1 BIOS

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // needed for setting bios in romsets
static struct BurnRomInfo atarisy1RomDesc[] = {
	// TTL Motherboard (Rev 2)
	{ "136032.205.l13",			0x04000, 0x88d0be26, BRF_BIOS | BRF_PRG | BRF_ESS }, //  0 68K BIOS
	{ "136032.206.l12",			0x04000, 0x3c79ef05, BRF_BIOS | BRF_PRG | BRF_ESS }, //  1
	{ "136032.104.f5",			0x02000, 0x7a29dc07, BRF_BIOS | BRF_GRA | BRF_ESS }, //  2 Alphanumeric Tiles
	{ "136032.101.e3",			0x00100, 0x7e84972a, BRF_BIOS | BRF_OPT },           //  3 PROMs
	{ "136032.102.e5",			0x00100, 0xebf1e0ae, BRF_BIOS | BRF_OPT },           //  4
	{ "136032.103.f7",			0x000eb, 0x92d6a0b4, BRF_BIOS | BRF_OPT },           //  5
	{ "",						0,       0,          0 },                            //  6
	{ "",						0,       0,          0 },                            //  70
	// TTL Motherboard (Rev 1)
	{ "136032.105.l13",			0x04000, 0x79021d3c, BRF_BIOS | BRF_PRG | BRF_ESS }, //  8 68K BIOS
	{ "136032.106.l12",			0x04000, 0x76ee86c4, BRF_BIOS | BRF_PRG | BRF_ESS }, //  9
	{ "136032.104.f5",			0x02000, 0x7a29dc07, BRF_BIOS | BRF_GRA | BRF_ESS }, // 10 Alphanumeric Tiles
	{ "136032.101.e3", 			0x00100, 0x7e84972a, BRF_BIOS | BRF_OPT },           // 11 PROMs
	{ "136032.102.e5", 			0x00100, 0xebf1e0ae, BRF_BIOS | BRF_OPT },           // 12
	{ "136032.103.f7", 			0x000eb, 0x92d6a0b4, BRF_BIOS | BRF_OPT },           // 13
	{ "",						0,       0,          0 },                            // 14
	{ "",						0,       0,          0 },                            // 15
	// LSI Motherboard
	{ "136032.114.j11", 		0x04000, 0x195c54ad, BRF_BIOS | BRF_PRG | BRF_ESS }, // 16 68K BIOS
	{ "136032.115.j10", 		0x04000, 0x7275b4dc, BRF_BIOS | BRF_PRG | BRF_ESS }, // 17
	{ "136032.107.b2",			0x02000, 0x7a29dc07, BRF_BIOS | BRF_GRA | BRF_ESS }, // 18 Alphanumeric Tiles
	{ "",						0,       0,          0 },                            // 19
	{ "",						0,       0,          0 },                            // 20
	{ "",						0,       0,          0 },                            // 21
	{ "",						0,       0,          0 },                            // 22
	{ "",						0,       0,          0 },                            // 23
};

STD_ROM_PICK(atarisy1)
STD_ROM_FN(atarisy1)

static INT32 Atarisy1BiosInit()
{
	return CommonInit(NULL, 0, digital_joystick);
}

struct BurnDriver BurnDrvAtarisy1 = {
	"atarisy1", NULL, NULL, NULL, "1984",
	"Atari System 1 BIOS\0", "BIOS only", "Atari", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_PRE90S, GBF_BIOS, 0,
	NULL, atarisy1RomInfo, atarisy1RomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	Atarisy1BiosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Marble Madness (set 1)

static struct BurnRomInfo marbleRomDesc[] = {
	{ "136033.623",				0x04000, 0x284ed2e9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136033.624",				0x04000, 0xd541b021, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136033.625",				0x04000, 0x563755c7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136033.626",				0x04000, 0x860feeb3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136033.627",				0x04000, 0xd1dbd439, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136033.628",				0x04000, 0x957d6801, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136033.229",				0x04000, 0xc81d5c14, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136033.630",				0x04000, 0x687a09f7, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136033.107",				0x04000, 0xf3b8745b, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136033.108",				0x04000, 0xe51eecaa, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136033.421",				0x04000, 0x78153dc3, 2 | BRF_PRG | BRF_ESS }, // 10 M6502 Code
	{ "136033.422",				0x04000, 0x2e66300e, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "136033.137",				0x04000, 0x7a45f5c1, 3 | BRF_GRA },           // 12 Sprites & Background Tiles
	{ "136033.138",				0x04000, 0x7e954a88, 3 | BRF_GRA },           // 13
	{ "136033.139",				0x04000, 0x1eb1bb5f, 3 | BRF_GRA },           // 14
	{ "136033.140",				0x04000, 0x8a82467b, 3 | BRF_GRA },           // 15
	{ "136033.141",				0x04000, 0x52448965, 3 | BRF_GRA },           // 16
	{ "136033.142",				0x04000, 0xb4a70e4f, 3 | BRF_GRA },           // 17
	{ "136033.143",				0x04000, 0x7156e449, 3 | BRF_GRA },           // 18
	{ "136033.144",				0x04000, 0x4c3e4c79, 3 | BRF_GRA },           // 19
	{ "136033.145",				0x04000, 0x9062be7f, 3 | BRF_GRA },           // 20
	{ "136033.146",				0x04000, 0x14566dca, 3 | BRF_GRA },           // 21
	{ "136033.149",				0x04000, 0xb6658f06, 3 | BRF_GRA },           // 22
	{ "136033.151",				0x04000, 0x84ee1c80, 3 | BRF_GRA },           // 23
	{ "136033.153",				0x04000, 0xdaa02926, 3 | BRF_GRA },           // 24

	{ "136033.118",				0x00200, 0x2101b0ed, 4 | BRF_GRA },           // 25 Sprite & Background Mapping PROMs
	{ "136033.119",				0x00200, 0x19f6e767, 4 | BRF_GRA },           // 26
};

STDROMPICKEXT(marble, marble, atarisy1)
STD_ROM_FN(marble)

static INT32 MarbleLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x18001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x18000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x28001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x28000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x04000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x14000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x24000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x34000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x44000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x84000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x94000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa4000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 MarbleInit()
{
	has_trackball = 1;
	return CommonInit(MarbleLoadRoms, 103, analog_joystick);
}

struct BurnDriver BurnDrvMarble = {
	"marble", NULL, "atarisy1", NULL, "1984",
	"Marble Madness (set 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, marbleRomInfo, marbleRomName, NULL, NULL, NULL, NULL, MarbleInputInfo, MarbleDIPInfo,
	MarbleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Marble Madness (set 2)

static struct BurnRomInfo marble2RomDesc[] = {
	{ "136033.401",				0x08000, 0xecfc25a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136033.402",				0x08000, 0x7ce9bf53, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136033.403",				0x08000, 0xdafee7a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136033.404",				0x08000, 0xb59ffcf6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136033.107",				0x04000, 0xf3b8745b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136033.108",				0x04000, 0xe51eecaa, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136033.421",				0x04000, 0x78153dc3, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code
	{ "136033.422",				0x04000, 0x2e66300e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136033.137",				0x04000, 0x7a45f5c1, 3 | BRF_GRA },           //  8 Sprites & Background Tiles
	{ "136033.138",				0x04000, 0x7e954a88, 3 | BRF_GRA },           //  9
	{ "136033.139",				0x04000, 0x1eb1bb5f, 3 | BRF_GRA },           // 10
	{ "136033.140",				0x04000, 0x8a82467b, 3 | BRF_GRA },           // 11
	{ "136033.141",				0x04000, 0x52448965, 3 | BRF_GRA },           // 12
	{ "136033.142",				0x04000, 0xb4a70e4f, 3 | BRF_GRA },           // 13
	{ "136033.143",				0x04000, 0x7156e449, 3 | BRF_GRA },           // 14
	{ "136033.144",				0x04000, 0x4c3e4c79, 3 | BRF_GRA },           // 15
	{ "136033.145",				0x04000, 0x9062be7f, 3 | BRF_GRA },           // 16
	{ "136033.146",				0x04000, 0x14566dca, 3 | BRF_GRA },           // 17
	{ "136033.149",				0x04000, 0xb6658f06, 3 | BRF_GRA },           // 18
	{ "136033.151",				0x04000, 0x84ee1c80, 3 | BRF_GRA },           // 19
	{ "136033.153",				0x04000, 0xdaa02926, 3 | BRF_GRA },           // 20

	{ "136033.118",				0x00200, 0x2101b0ed, 4 | BRF_GRA },           // 21 Sprite & Background Mapping PROMs
	{ "136033.119",				0x00200, 0x19f6e767, 4 | BRF_GRA },           // 22
};

STDROMPICKEXT(marble2, marble2, atarisy1)
STD_ROM_FN(marble2)

static INT32 Marble2LoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x04000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x14000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x24000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x34000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x44000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x84000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x94000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa4000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 Marble2Init()
{
	has_trackball = 1;
	return CommonInit(Marble2LoadRoms, 103, analog_joystick);
}

struct BurnDriver BurnDrvMarble2 = {
	"marble2", "marble", "atarisy1", NULL, "1984",
	"Marble Madness (set 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, marble2RomInfo, marble2RomName, NULL, NULL, NULL, NULL, MarbleInputInfo, MarbleDIPInfo,
	Marble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Marble Madness (set 3)

static struct BurnRomInfo marble3RomDesc[] = {
	{ "136033.201",				0x08000, 0x9395804d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136033.202",				0x08000, 0xedd313f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136033.403",				0x08000, 0xdafee7a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136033.204",				0x08000, 0x4d621731, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136033.107",				0x04000, 0xf3b8745b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136033.108",				0x04000, 0xe51eecaa, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136033.121",				0x04000, 0x73fe2b46, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code
	{ "136033.122",				0x04000, 0x03bf65c3, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136033.137",				0x04000, 0x7a45f5c1, 3 | BRF_GRA },           //  8 Sprites & Background Tiles
	{ "136033.138",				0x04000, 0x7e954a88, 3 | BRF_GRA },           //  9
	{ "136033.139",				0x04000, 0x1eb1bb5f, 3 | BRF_GRA },           // 10
	{ "136033.140",				0x04000, 0x8a82467b, 3 | BRF_GRA },           // 11
	{ "136033.141",				0x04000, 0x52448965, 3 | BRF_GRA },           // 12
	{ "136033.142",				0x04000, 0xb4a70e4f, 3 | BRF_GRA },           // 13
	{ "136033.143",				0x04000, 0x7156e449, 3 | BRF_GRA },           // 14
	{ "136033.144",				0x04000, 0x4c3e4c79, 3 | BRF_GRA },           // 15
	{ "136033.145",				0x04000, 0x9062be7f, 3 | BRF_GRA },           // 16
	{ "136033.146",				0x04000, 0x14566dca, 3 | BRF_GRA },           // 17
	{ "136033.149",				0x04000, 0xb6658f06, 3 | BRF_GRA },           // 18
	{ "136033.151",				0x04000, 0x84ee1c80, 3 | BRF_GRA },           // 19
	{ "136033.153",				0x04000, 0xdaa02926, 3 | BRF_GRA },           // 20

	{ "136033.118",				0x00200, 0x2101b0ed, 4 | BRF_GRA },           // 21 Sprite & Background Mapping PROMs
	{ "136033.119",				0x00200, 0x19f6e767, 4 | BRF_GRA },           // 22
};

STDROMPICKEXT(marble3, marble3, atarisy1)
STD_ROM_FN(marble3)

struct BurnDriver BurnDrvMarble3 = {
	"marble3", "marble", "atarisy1", NULL, "1984",
	"Marble Madness (set 3)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, marble3RomInfo, marble3RomName, NULL, NULL, NULL, NULL, MarbleInputInfo, MarbleDIPInfo,
	Marble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Marble Madness (set 4)

static struct BurnRomInfo marble4RomDesc[] = {
	{ "136033.323",				0x04000, 0x4dc2987a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136033.324",				0x04000, 0xe22e6e11, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136033.225",				0x04000, 0x743f6c5c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136033.226",				0x04000, 0xaeb711e3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136033.227",				0x04000, 0xd06d2c22, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136033.228",				0x04000, 0xe69cec16, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136033.229",				0x04000, 0xc81d5c14, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136033.230",				0x04000, 0x526ce8ad, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136033.107",				0x04000, 0xf3b8745b, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136033.108",				0x04000, 0xe51eecaa, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136033.257",				0x04000, 0x2e2e0df8, 2 | BRF_PRG | BRF_ESS }, // 10 M6502 Code
	{ "136033.258",				0x04000, 0x1b9655cd, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "136033.137",				0x04000, 0x7a45f5c1, 3 | BRF_GRA },           // 12 Sprites & Background Tiles
	{ "136033.138",				0x04000, 0x7e954a88, 3 | BRF_GRA },           // 13
	{ "136033.139",				0x04000, 0x1eb1bb5f, 3 | BRF_GRA },           // 14
	{ "136033.140",				0x04000, 0x8a82467b, 3 | BRF_GRA },           // 15
	{ "136033.141",				0x04000, 0x52448965, 3 | BRF_GRA },           // 16
	{ "136033.142",				0x04000, 0xb4a70e4f, 3 | BRF_GRA },           // 17
	{ "136033.143",				0x04000, 0x7156e449, 3 | BRF_GRA },           // 18
	{ "136033.144",				0x04000, 0x4c3e4c79, 3 | BRF_GRA },           // 19
	{ "136033.145",				0x04000, 0x9062be7f, 3 | BRF_GRA },           // 20
	{ "136033.146",				0x04000, 0x14566dca, 3 | BRF_GRA },           // 21
	{ "136033.149",				0x04000, 0xb6658f06, 3 | BRF_GRA },           // 22
	{ "136033.151",				0x04000, 0x84ee1c80, 3 | BRF_GRA },           // 23
	{ "136033.153",				0x04000, 0xdaa02926, 3 | BRF_GRA },           // 24

	{ "136033.118",				0x00200, 0x2101b0ed, 4 | BRF_GRA },           // 25 Sprite & Background Mapping PROMs
	{ "136033.119",				0x00200, 0x19f6e767, 4 | BRF_GRA },           // 26
};

STDROMPICKEXT(marble4, marble4, atarisy1)
STD_ROM_FN(marble4)

struct BurnDriver BurnDrvMarble4 = {
	"marble4", "marble", "atarisy1", NULL, "1984",
	"Marble Madness (set 4)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, marble4RomInfo, marble4RomName, NULL, NULL, NULL, NULL, MarbleInputInfo, MarbleDIPInfo,
	MarbleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Marble Madness (set 5 - LSI Cartridge)

static struct BurnRomInfo marble5RomDesc[] = {
	{ "136033.201",				0x08000, 0x9395804d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136033.202",				0x08000, 0xedd313f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136033.203",				0x08000, 0xdafee7a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136033.204",				0x08000, 0x4d621731, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136033.107",				0x04000, 0xf3b8745b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136033.108",				0x04000, 0xe51eecaa, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136033.121",				0x04000, 0x73fe2b46, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code
	{ "136033.122",				0x04000, 0x03bf65c3, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136033.109",				0x08000, 0x467208f4, 3 | BRF_GRA },           //  8 Sprites & Background Tiles
	{ "136033.110",				0x08000, 0xb883ec76, 3 | BRF_GRA },           //  9
	{ "136033.111",				0x08000, 0xc208bd5e, 3 | BRF_GRA },           // 10
	{ "136033.112",				0x08000, 0x042673d4, 3 | BRF_GRA },           // 11
	{ "136033.113",				0x08000, 0xb390aef3, 3 | BRF_GRA },           // 12
	{ "136033.115",				0x04000, 0xb6658f06, 3 | BRF_GRA },           // 13
	{ "136033.116",				0x04000, 0x84ee1c80, 3 | BRF_GRA },           // 14
	{ "136033.117",				0x04000, 0xdaa02926, 3 | BRF_GRA },           // 15

	{ "136033.118",				0x00200, 0x2101b0ed, 4 | BRF_GRA },           // 16 Sprite & Background Mapping PROMs
	{ "136033.159",				0x00200, 0x19f6e767, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(marble5, marble5, atarisy1)
STD_ROM_FN(marble5)

static INT32 Marble5LoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x84000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x94000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa4000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 Marble5Init()
{
	has_trackball = 1;
	return CommonInit(Marble5LoadRoms, 103, analog_joystick);
}

struct BurnDriver BurnDrvMarble5 = {
	"marble5", "marble", "atarisy1", NULL, "1984",
	"Marble Madness (set 5 - LSI Cartridge)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, marble5RomInfo, marble5RomName, NULL, NULL, NULL, NULL, MarbleInputInfo, MarbleDIPInfo,
	Marble5Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Peter Pack Rat

static struct BurnRomInfo peterpakRomDesc[] = {
	{ "136028.142",				0x04000, 0x4f9fc020, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136028.143",				0x04000, 0x9fb257cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136028.144",				0x04000, 0x50267619, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136028.145",				0x04000, 0x7b6a5004, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136028.146",				0x04000, 0x4183a67a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136028.147",				0x04000, 0x14e2d97b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136028.148",				0x04000, 0x230e8ba9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136028.149",				0x04000, 0x0ff0c13a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136028.101",				0x04000, 0xff712aa2, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136028.102",				0x04000, 0x89ea21a1, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "136028.138",				0x08000, 0x53eaa018, 3 | BRF_GRA },           // 10 Sprites & Background Tiles
	{ "136028.139",				0x08000, 0x354a19cb, 3 | BRF_GRA },           // 11
	{ "136028.140",				0x08000, 0x8d2c4717, 3 | BRF_GRA },           // 12
	{ "136028.141",				0x08000, 0xbf59ea19, 3 | BRF_GRA },           // 13
	{ "136028.150",				0x08000, 0x83362483, 3 | BRF_GRA },           // 14
	{ "136028.151",				0x08000, 0x6e95094e, 3 | BRF_GRA },           // 15
	{ "136028.152",				0x08000, 0x9553f084, 3 | BRF_GRA },           // 16
	{ "136028.153",				0x08000, 0xc2a9b028, 3 | BRF_GRA },           // 17
	{ "136028.105",				0x04000, 0xac9a5a44, 3 | BRF_GRA },           // 18
	{ "136028.108",				0x04000, 0x51941e64, 3 | BRF_GRA },           // 19
	{ "136028.111",				0x04000, 0x246599f3, 3 | BRF_GRA },           // 20
	{ "136028.114",				0x04000, 0x918a5082, 3 | BRF_GRA },           // 21

	{ "136028.136",				0x00200, 0x861cfa36, 4 | BRF_GRA },           // 22 Sprite & Background Mapping PROMs
	{ "136028.137",				0x00200, 0x8507e5ea, 4 | BRF_GRA },           // 23
};

STDROMPICKEXT(peterpak, peterpak, atarisy1)
STD_ROM_FN(peterpak)

static INT32 PeterpakLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x18001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x18000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x104000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x114000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x124000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x134000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 PeterpakInit()
{
	return CommonInit(PeterpakLoadRoms, 107, digital_joystick);
}

struct BurnDriver BurnDrvPeterpak = {
	"peterpak", NULL, "atarisy1", NULL, "1984",
	"Peter Pack Rat\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, peterpakRomInfo, peterpakRomName, NULL, NULL, NULL, NULL, PeterpakInputInfo, PeterpakDIPInfo,
	PeterpakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (set 1)

static struct BurnRomInfo indytempRomDesc[] = {
	{ "136036.432",				0x08000, 0xd888cdf1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136036.431",				0x08000, 0xb7ac7431, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136036.434",				0x08000, 0x802495fd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136036.433",				0x08000, 0x3a914e5c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136036.456",				0x04000, 0xec146b09, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136036.457",				0x04000, 0x6628de01, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136036.358",				0x04000, 0xd9351106, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136036.359",				0x04000, 0xe731caea, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136036.154",				0x04000, 0xcbfc6adb, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136036.155",				0x04000, 0x4c8233ac, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136036.135",				0x08000, 0xffa8749c, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 3 | BRF_GRA },           // 12
	{ "136036.143",				0x08000, 0x7697da26, 3 | BRF_GRA },           // 13
	{ "136036.147",				0x08000, 0x4e9d664c, 3 | BRF_GRA },           // 14
	{ "136036.136",				0x08000, 0xb2b403aa, 3 | BRF_GRA },           // 15
	{ "136036.140",				0x08000, 0xec0c19ca, 3 | BRF_GRA },           // 16
	{ "136036.144",				0x08000, 0x4407df98, 3 | BRF_GRA },           // 17
	{ "136036.148",				0x08000, 0x70dce06d, 3 | BRF_GRA },           // 18
	{ "136036.137",				0x08000, 0x3f352547, 3 | BRF_GRA },           // 19
	{ "136036.141",				0x08000, 0x9cbdffd0, 3 | BRF_GRA },           // 20
	{ "136036.145",				0x08000, 0xe828e64b, 3 | BRF_GRA },           // 21
	{ "136036.149",				0x08000, 0x81503a23, 3 | BRF_GRA },           // 22
	{ "136036.138",				0x08000, 0x48c4d79d, 3 | BRF_GRA },           // 23
	{ "136036.142",				0x08000, 0x7faae75f, 3 | BRF_GRA },           // 24
	{ "136036.146",				0x08000, 0x8ae5a7b5, 3 | BRF_GRA },           // 25
	{ "136036.150",				0x08000, 0xa10c4bd9, 3 | BRF_GRA },           // 26

	{ "136036.152",				0x00200, 0x4f96e57c, 4 | BRF_GRA },           // 27 Sprite & Background Mapping PROMs
	{ "136036.151",				0x00200, 0x7daf351f, 4 | BRF_GRA },           // 28
};

STDROMPICKEXT(indytemp, indytemp, atarisy1)
STD_ROM_FN(indytemp)

static INT32 IndytempLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x100000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x110000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x120000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x130000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x180000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x190000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1b0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 IndytempInit()
{
	return CommonInit(IndytempLoadRoms, 105, digital_joystick);
}

struct BurnDriver BurnDrvIndytemp = {
	"indytemp", NULL, "atarisy1",  NULL, "1985",
	"Indiana Jones and the Temple of Doom (set 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, indytempRomInfo, indytempRomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	IndytempInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (set 2)

static struct BurnRomInfo indytemp2RomDesc[] = {
	{ "136036.470",				0x08000, 0x7fac1dd8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136036.471",				0x08000, 0xe93272fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136036.434",				0x08000, 0x802495fd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136036.433",				0x08000, 0x3a914e5c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136036.456",				0x04000, 0xec146b09, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136036.457",				0x04000, 0x6628de01, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136036.358",				0x04000, 0xd9351106, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136036.359",				0x04000, 0xe731caea, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136036.154",				0x04000, 0xcbfc6adb, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136036.155",				0x04000, 0x4c8233ac, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136036.135",				0x08000, 0xffa8749c, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 3 | BRF_GRA },           // 12
	{ "136036.143",				0x08000, 0x7697da26, 3 | BRF_GRA },           // 13
	{ "136036.147",				0x08000, 0x4e9d664c, 3 | BRF_GRA },           // 14
	{ "136036.136",				0x08000, 0xb2b403aa, 3 | BRF_GRA },           // 15
	{ "136036.140",				0x08000, 0xec0c19ca, 3 | BRF_GRA },           // 16
	{ "136036.144",				0x08000, 0x4407df98, 3 | BRF_GRA },           // 17
	{ "136036.148",				0x08000, 0x70dce06d, 3 | BRF_GRA },           // 18
	{ "136036.137",				0x08000, 0x3f352547, 3 | BRF_GRA },           // 19
	{ "136036.141",				0x08000, 0x9cbdffd0, 3 | BRF_GRA },           // 20
	{ "136036.145",				0x08000, 0xe828e64b, 3 | BRF_GRA },           // 21
	{ "136036.149",				0x08000, 0x81503a23, 3 | BRF_GRA },           // 22
	{ "136036.138",				0x08000, 0x48c4d79d, 3 | BRF_GRA },           // 23
	{ "136036.142",				0x08000, 0x7faae75f, 3 | BRF_GRA },           // 24
	{ "136036.146",				0x08000, 0x8ae5a7b5, 3 | BRF_GRA },           // 25
	{ "136036.150",				0x08000, 0xa10c4bd9, 3 | BRF_GRA },           // 26

	{ "136036.152",				0x00200, 0x4f96e57c, 4 | BRF_GRA },           // 27 Sprite & Background Mapping PROMs
	{ "136036.151",				0x00200, 0x7daf351f, 4 | BRF_GRA },           // 28
};

STDROMPICKEXT(indytemp2, indytemp2, atarisy1)
STD_ROM_FN(indytemp2)

struct BurnDriver BurnDrvIndytemp2 = {
	"indytemp2", "indytemp", "atarisy1", NULL, "1985",
	"Indiana Jones and the Temple of Doom (set 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, indytemp2RomInfo, indytemp2RomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	IndytempInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (set 3)

static struct BurnRomInfo indytemp3RomDesc[] = {
	{ "232.10b",				0x08000, 0x1e80108f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "231.10a",				0x08000, 0x8ae54c0c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "234.12b",				0x08000, 0x86be7e07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "233.12a",				0x08000, 0xbfcea7ae, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "256.15b",				0x04000, 0x3a076fd2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "257.15a",				0x04000, 0x15293606, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "158.16b",				0x04000, 0x10372888, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "159.16a",				0x04000, 0x50f890a8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136036.154",				0x04000, 0xcbfc6adb, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136036.155",				0x04000, 0x4c8233ac, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136036.135",				0x08000, 0xffa8749c, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 3 | BRF_GRA },           // 12
	{ "136036.143",				0x08000, 0x7697da26, 3 | BRF_GRA },           // 13
	{ "136036.147",				0x08000, 0x4e9d664c, 3 | BRF_GRA },           // 14
	{ "136036.136",				0x08000, 0xb2b403aa, 3 | BRF_GRA },           // 15
	{ "136036.140",				0x08000, 0xec0c19ca, 3 | BRF_GRA },           // 16
	{ "136036.144",				0x08000, 0x4407df98, 3 | BRF_GRA },           // 17
	{ "136036.148",				0x08000, 0x70dce06d, 3 | BRF_GRA },           // 18
	{ "136036.137",				0x08000, 0x3f352547, 3 | BRF_GRA },           // 19
	{ "136036.141",				0x08000, 0x9cbdffd0, 3 | BRF_GRA },           // 20
	{ "136036.145",				0x08000, 0xe828e64b, 3 | BRF_GRA },           // 21
	{ "136036.149",				0x08000, 0x81503a23, 3 | BRF_GRA },           // 22
	{ "136036.138",				0x08000, 0x48c4d79d, 3 | BRF_GRA },           // 23
	{ "136036.142",				0x08000, 0x7faae75f, 3 | BRF_GRA },           // 24
	{ "136036.146",				0x08000, 0x8ae5a7b5, 3 | BRF_GRA },           // 25
	{ "136036.150",				0x08000, 0xa10c4bd9, 3 | BRF_GRA },           // 26

	{ "136036.152",				0x00200, 0x4f96e57c, 4 | BRF_GRA },           // 27 Sprite & Background Mapping PROMs
	{ "136036.151",				0x00200, 0x7daf351f, 4 | BRF_GRA },           // 28
};

STDROMPICKEXT(indytemp3, indytemp3, atarisy1)
STD_ROM_FN(indytemp3)

struct BurnDriver BurnDrvIndytemp3 = {
	"indytemp3", "indytemp", "atarisy1", NULL, "1985",
	"Indiana Jones and the Temple of Doom (set 3)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, indytemp3RomInfo, indytemp3RomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	IndytempInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (set 4)

static struct BurnRomInfo indytemp4RomDesc[] = {
	{ "136036.332",				0x08000, 0xa5563773, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136036.331",				0x08000, 0x7d562141, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136036.334",				0x08000, 0xe40828e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136036.333",				0x08000, 0x96e1f1aa, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136036.356",				0x04000, 0x5eba2ac7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136036.357",				0x04000, 0x26e84b5c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136036.358",				0x04000, 0xd9351106, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136036.359",				0x04000, 0xe731caea, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136036.154",				0x04000, 0xcbfc6adb, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136036.155",				0x04000, 0x4c8233ac, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136036.135",				0x08000, 0xffa8749c, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 3 | BRF_GRA },           // 12
	{ "136036.143",				0x08000, 0x7697da26, 3 | BRF_GRA },           // 13
	{ "136036.147",				0x08000, 0x4e9d664c, 3 | BRF_GRA },           // 14
	{ "136036.136",				0x08000, 0xb2b403aa, 3 | BRF_GRA },           // 15
	{ "136036.140",				0x08000, 0xec0c19ca, 3 | BRF_GRA },           // 16
	{ "136036.144",				0x08000, 0x4407df98, 3 | BRF_GRA },           // 17
	{ "136036.148",				0x08000, 0x70dce06d, 3 | BRF_GRA },           // 18
	{ "136036.137",				0x08000, 0x3f352547, 3 | BRF_GRA },           // 19
	{ "136036.141",				0x08000, 0x9cbdffd0, 3 | BRF_GRA },           // 20
	{ "136036.145",				0x08000, 0xe828e64b, 3 | BRF_GRA },           // 21
	{ "136036.149",				0x08000, 0x81503a23, 3 | BRF_GRA },           // 22
	{ "136036.138",				0x08000, 0x48c4d79d, 3 | BRF_GRA },           // 23
	{ "136036.142",				0x08000, 0x7faae75f, 3 | BRF_GRA },           // 24
	{ "136036.146",				0x08000, 0x8ae5a7b5, 3 | BRF_GRA },           // 25
	{ "136036.150",				0x08000, 0xa10c4bd9, 3 | BRF_GRA },           // 26

	{ "136036.152",				0x00200, 0x4f96e57c, 4 | BRF_GRA },           // 27 Sprite & Background Mapping PROMs
	{ "136036.151",				0x00200, 0x7daf351f, 4 | BRF_GRA },           // 28
};

STDROMPICKEXT(indytemp4, indytemp4, atarisy1)
STD_ROM_FN(indytemp4)

struct BurnDriver BurnDrvIndytemp4 = {
	"indytemp4", "indytemp", "atarisy1", NULL, "1985",
	"Indiana Jones and the Temple of Doom (set 4)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, indytemp4RomInfo, indytemp4RomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	IndytempInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (cocktail)

static struct BurnRomInfo indytempcRomDesc[] = {
	{ "136032.116",				0x04000, 0x195c54ad, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136032.117",				0x04000, 0x9af9fe29, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136032.632",				0x08000, 0xd3e1a611, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136032.631",				0x08000, 0x9ac96ba8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136036.534",				0x08000, 0xeae396be, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136036.533",				0x08000, 0x06c66335, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136036.568",				0x08000, 0x2bbc16ed, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136036.569",				0x08000, 0x39270ade, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136036.358",				0x04000, 0xd9351106, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136036.359",				0x04000, 0xe731caea, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, // 10 M6502 Code
	{ "136036.170",				0x08000, 0xf318b321, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "136032.120",				0x04000, 0x90a1950d, 3 | BRF_GRA },           // 12 Alphanumeric Tiles

	{ "136036.135",				0x08000, 0xffa8749c, 4 | BRF_GRA },           // 13 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 4 | BRF_GRA },           // 14
	{ "136036.143",				0x08000, 0x7697da26, 4 | BRF_GRA },           // 15
	{ "136036.147",				0x08000, 0x4e9d664c, 4 | BRF_GRA },           // 16
	{ "136036.136",				0x08000, 0xb2b403aa, 4 | BRF_GRA },           // 17
	{ "136036.140",				0x08000, 0xec0c19ca, 4 | BRF_GRA },           // 18
	{ "136036.144",				0x08000, 0x4407df98, 4 | BRF_GRA },           // 19
	{ "136036.148",				0x08000, 0x70dce06d, 4 | BRF_GRA },           // 20
	{ "136036.137",				0x08000, 0x3f352547, 4 | BRF_GRA },           // 21
	{ "136036.141",				0x08000, 0x9cbdffd0, 4 | BRF_GRA },           // 22
	{ "136036.145",				0x08000, 0xe828e64b, 4 | BRF_GRA },           // 23
	{ "136036.149",				0x08000, 0x81503a23, 4 | BRF_GRA },           // 24
	{ "136036.138",				0x08000, 0x48c4d79d, 4 | BRF_GRA },           // 25
	{ "136036.142",				0x08000, 0x7faae75f, 4 | BRF_GRA },           // 26
	{ "136036.146",				0x08000, 0x8ae5a7b5, 4 | BRF_GRA },           // 27
	{ "136036.150",				0x08000, 0xa10c4bd9, 4 | BRF_GRA },           // 28

	{ "136036.152",				0x00200, 0x4f96e57c, 5 | BRF_GRA },           // 29 Sprite & Background Mapping PROMs
	{ "136036.160",				0x00200, 0x88c65843, 5 | BRF_GRA },           // 30
};

STDROMPICKEXT(indytempc, indytempc, atarisy1)
STD_ROM_FN(indytempc)

static INT32 IndytempcLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x00001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x00000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;

	if (BurnLoadRom(DrvAlphaROM  + 0x00000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x100000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x110000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x120000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x130000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x180000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x190000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1b0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 IndytempcInit()
{
	invert_input0 = 0xff;
	return CommonInit(IndytempcLoadRoms, 105, digital_joystick);
}

struct BurnDriver BurnDrvIndytempc = {
	"indytempc", "indytemp", "atarisy1", NULL, "1985",
	"Indiana Jones and the Temple of Doom (cocktail)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, indytempcRomInfo, indytempcRomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytemcDIPInfo,
	IndytempcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Indiana Jones and the Temple of Doom (German)

static struct BurnRomInfo indytempdRomDesc[] = {
	{ "136036.462",				0x08000, 0x317dc430, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136036.461",				0x08000, 0x8c73f974, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136036.464",				0x08000, 0x3fcb199f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136036.463",				0x08000, 0xd6bda19a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136036.466",				0x04000, 0xfaa7f23a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136036.467",				0x04000, 0xee9fd91a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136036.358",				0x04000, 0xd9351106, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136036.359",				0x04000, 0xe731caea, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136036.153",				0x04000, 0x95294641, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136036.154",				0x04000, 0xcbfc6adb, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136036.155",				0x04000, 0x4c8233ac, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136036.135",				0x08000, 0xffa8749c, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136036.139",				0x08000, 0xb682bfca, 3 | BRF_GRA },           // 12
	{ "136036.143",				0x08000, 0x7697da26, 3 | BRF_GRA },           // 13
	{ "136036.147",				0x08000, 0x4e9d664c, 3 | BRF_GRA },           // 14
	{ "136036.136",				0x08000, 0xb2b403aa, 3 | BRF_GRA },           // 15
	{ "136036.140",				0x08000, 0xec0c19ca, 3 | BRF_GRA },           // 16
	{ "136036.144",				0x08000, 0x4407df98, 3 | BRF_GRA },           // 17
	{ "136036.148",				0x08000, 0x70dce06d, 3 | BRF_GRA },           // 18
	{ "136036.137",				0x08000, 0x3f352547, 3 | BRF_GRA },           // 19
	{ "136036.141",				0x08000, 0x9cbdffd0, 3 | BRF_GRA },           // 20
	{ "136036.145",				0x08000, 0xe828e64b, 3 | BRF_GRA },           // 21
	{ "136036.149",				0x08000, 0x81503a23, 3 | BRF_GRA },           // 22
	{ "136036.138",				0x08000, 0x48c4d79d, 3 | BRF_GRA },           // 23
	{ "136036.142",				0x08000, 0x7faae75f, 3 | BRF_GRA },           // 24
	{ "136036.146",				0x08000, 0x8ae5a7b5, 3 | BRF_GRA },           // 25
	{ "136036.150",				0x08000, 0xa10c4bd9, 3 | BRF_GRA },           // 26

	{ "136036.152",				0x00200, 0x4f96e57c, 4 | BRF_GRA },           // 27 Sprite & Background Mapping PROMs
	{ "136036.151",				0x00200, 0x7daf351f, 4 | BRF_GRA },           // 28
};

STDROMPICKEXT(indytempd, indytempd, atarisy1)
STD_ROM_FN(indytempd)

struct BurnDriver BurnDrvIndytempd = {
	"indytempd", "indytemp", "atarisy1", NULL, "1985",
	"Indiana Jones and the Temple of Doom (German)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, indytempdRomInfo, indytempdRomName, NULL, NULL, NULL, NULL, IndytempInputInfo, IndytempDIPInfo,
	IndytempInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Runner (rev 2)

static struct BurnRomInfo roadrunnRomDesc[] = {
	{ "136040-228.11c",			0x08000, 0xb66c629a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136040-229.11a",			0x08000, 0x5638959f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136040-230.13c",			0x08000, 0xcd7956a3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136040-231.13a",			0x08000, 0x722f2d3b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136040-134.12c",			0x08000, 0x18f431fe, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136040-135.12a",			0x08000, 0xcb06f9ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136040-136.14c",			0x08000, 0x8050bce4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136040-137.14a",			0x08000, 0x3372a5cf, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136040-138.16c",			0x08000, 0xa83155ee, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136040-139.16a",			0x08000, 0x23aead1c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136040-140.17c",			0x04000, 0xd1464c88, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136040-141.17a",			0x04000, 0xf8f2acdf, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136040-143.15e",			0x04000, 0x62b9878e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136040-144.17e",			0x04000, 0x6ef1b804, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136040-101.4b",			0x08000, 0x26d9f29c, 3 | BRF_GRA },           // 14 Sprites & Background Tiles
	{ "136040-107.9b",			0x08000, 0x8aac0ba4, 3 | BRF_GRA },           // 15
	{ "136040-113.4f",			0x08000, 0x48b74c52, 3 | BRF_GRA },           // 16
	{ "136040-119.9f",			0x08000, 0x17a6510c, 3 | BRF_GRA },           // 17
	{ "136040-102.3b",			0x08000, 0xae88f54b, 3 | BRF_GRA },           // 18
	{ "136040-108.8b",			0x08000, 0xa2ac13d4, 3 | BRF_GRA },           // 19
	{ "136040-114.3f",			0x08000, 0xc91c3fcb, 3 | BRF_GRA },           // 20
	{ "136040-120.8f",			0x08000, 0x42d25859, 3 | BRF_GRA },           // 21
	{ "136040-103.2b",			0x08000, 0xf2d7ef55, 3 | BRF_GRA },           // 22
	{ "136040-109.7b",			0x08000, 0x11a843dc, 3 | BRF_GRA },           // 23
	{ "136040-115.2f",			0x08000, 0x8b1fa5bc, 3 | BRF_GRA },           // 24
	{ "136040-121.7f",			0x08000, 0xecf278f2, 3 | BRF_GRA },           // 25
	{ "136040-104.1b",			0x08000, 0x0203d89c, 3 | BRF_GRA },           // 26
	{ "136040-110.6b",			0x08000, 0x64801601, 3 | BRF_GRA },           // 27
	{ "136040-116.1f",			0x08000, 0x52b23a36, 3 | BRF_GRA },           // 28
	{ "136040-122.6f",			0x08000, 0xb1137a9d, 3 | BRF_GRA },           // 29
	{ "136040-105.4d",			0x08000, 0x398a36f8, 3 | BRF_GRA },           // 30
	{ "136040-111.9d",			0x08000, 0xf08b418b, 3 | BRF_GRA },           // 31
	{ "136040-117.2d",			0x08000, 0xc4394834, 3 | BRF_GRA },           // 32
	{ "136040-123.7d",			0x08000, 0xdafd3dbe, 3 | BRF_GRA },           // 33
	{ "136040-106.3d",			0x08000, 0x36a77bc5, 3 | BRF_GRA },           // 34
	{ "136040-112.8d",			0x08000, 0xb6624f3c, 3 | BRF_GRA },           // 35
	{ "136040-118.1d",			0x08000, 0xf489a968, 3 | BRF_GRA },           // 36
	{ "136040-124.6d",			0x08000, 0x524d65f7, 3 | BRF_GRA },           // 37

	{ "136040-126.7a",			0x00200, 0x1713c0cd, 4 | BRF_GRA },           // 38 Sprite & Background Mapping PROMs
	{ "136040-125.5a",			0x00200, 0xa9ca8795, 4 | BRF_GRA },           // 39
};

STDROMPICKEXT(roadrunn, roadrunn, atarisy1)
STD_ROM_FN(roadrunn)

static INT32 RoadrunnLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x100000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x110000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x120000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x130000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x180000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x190000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1b0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x200000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x210000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x220000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x230000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x280000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x290000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x2a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x2b0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 RoadrunnInit()
{
	return CommonInit(RoadrunnLoadRoms, 108, analog_joystick);
}

struct BurnDriver BurnDrvRoadrunn = {
	"roadrunn", NULL, "atarisy1", NULL, "1985",
	"Road Runner (rev 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, roadrunnRomInfo, roadrunnRomName, NULL, NULL, NULL, NULL, RoadrunnInputInfo, RoadrunnDIPInfo,
	RoadrunnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Runner (rev 1+)

static struct BurnRomInfo roadrunn2RomDesc[] = {
	{ "136040-x28.11c",			0x08000, 0xfbd43085, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136040-x29.11a",			0x08000, 0xf8d8819b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136040-x30.13c",			0x08000, 0x6a273375, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136040-x31.13a",			0x08000, 0xeb5c4368, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136040-134.12c",			0x08000, 0x18f431fe, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136040-135.12a",			0x08000, 0xcb06f9ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136040-136.14c",			0x08000, 0x8050bce4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136040-137.14a",			0x08000, 0x3372a5cf, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136040-138.16c",			0x08000, 0xa83155ee, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136040-139.16a",			0x08000, 0x23aead1c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136040-140.17c",			0x04000, 0xd1464c88, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136040-141.17a",			0x04000, 0xf8f2acdf, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136040-143.15e",			0x04000, 0x62b9878e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136040-144.17e",			0x04000, 0x6ef1b804, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136040-101.4b",			0x08000, 0x26d9f29c, 3 | BRF_GRA },           // 14 Sprites & Background Tiles
	{ "136040-107.9b",			0x08000, 0x8aac0ba4, 3 | BRF_GRA },           // 15
	{ "136040-113.4f",			0x08000, 0x48b74c52, 3 | BRF_GRA },           // 16
	{ "136040-119.9f",			0x08000, 0x17a6510c, 3 | BRF_GRA },           // 17
	{ "136040-102.3b",			0x08000, 0xae88f54b, 3 | BRF_GRA },           // 18
	{ "136040-108.8b",			0x08000, 0xa2ac13d4, 3 | BRF_GRA },           // 19
	{ "136040-114.3f",			0x08000, 0xc91c3fcb, 3 | BRF_GRA },           // 20
	{ "136040-120.8f",			0x08000, 0x42d25859, 3 | BRF_GRA },           // 21
	{ "136040-103.2b",			0x08000, 0xf2d7ef55, 3 | BRF_GRA },           // 22
	{ "136040-109.7b",			0x08000, 0x11a843dc, 3 | BRF_GRA },           // 23
	{ "136040-115.2f",			0x08000, 0x8b1fa5bc, 3 | BRF_GRA },           // 24
	{ "136040-121.7f",			0x08000, 0xecf278f2, 3 | BRF_GRA },           // 25
	{ "136040-104.1b",			0x08000, 0x0203d89c, 3 | BRF_GRA },           // 26
	{ "136040-110.6b",			0x08000, 0x64801601, 3 | BRF_GRA },           // 27
	{ "136040-116.1f",			0x08000, 0x52b23a36, 3 | BRF_GRA },           // 28
	{ "136040-122.6f",			0x08000, 0xb1137a9d, 3 | BRF_GRA },           // 29
	{ "136040-105.4d",			0x08000, 0x398a36f8, 3 | BRF_GRA },           // 30
	{ "136040-111.9d",			0x08000, 0xf08b418b, 3 | BRF_GRA },           // 31
	{ "136040-117.2d",			0x08000, 0xc4394834, 3 | BRF_GRA },           // 32
	{ "136040-123.7d",			0x08000, 0xdafd3dbe, 3 | BRF_GRA },           // 33
	{ "136040-106.3d",			0x08000, 0x36a77bc5, 3 | BRF_GRA },           // 34
	{ "136040-112.8d",			0x08000, 0xb6624f3c, 3 | BRF_GRA },           // 35
	{ "136040-118.1d",			0x08000, 0xf489a968, 3 | BRF_GRA },           // 36
	{ "136040-124.6d",			0x08000, 0x524d65f7, 3 | BRF_GRA },           // 37

	{ "136040-126.7a",			0x00200, 0x1713c0cd, 4 | BRF_GRA },           // 38 Sprite & Background Mapping PROMs
	{ "136040-125.5a",			0x00200, 0xa9ca8795, 4 | BRF_GRA },           // 39
};

STDROMPICKEXT(roadrunn2, roadrunn2, atarisy1)
STD_ROM_FN(roadrunn2)

struct BurnDriver BurnDrvRoadrunn2 = {
	"roadrunn2", "roadrunn", "atarisy1", NULL, "1985",
	"Road Runner (rev 1+)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, roadrunn2RomInfo, roadrunn2RomName, NULL, NULL, NULL, NULL, RoadrunnInputInfo, RoadrunnDIPInfo,
	RoadrunnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Runner (rev 1)

static struct BurnRomInfo roadrunn1RomDesc[] = {
	{ "136040-128.11c",			0x08000, 0x5e39d540, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136040-129.11a",			0x08000, 0xd79bfea1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136040-130.13c",			0x08000, 0x66453b37, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136040-131.13a",			0x08000, 0xa8497cdc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136040-134.12c",			0x08000, 0x18f431fe, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136040-135.12a",			0x08000, 0xcb06f9ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136040-136.14c",			0x08000, 0x8050bce4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136040-137.14a",			0x08000, 0x3372a5cf, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136040-138.16c",			0x08000, 0xa83155ee, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136040-139.16a",			0x08000, 0x23aead1c, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136040-140.17c",			0x04000, 0xd1464c88, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136040-141.17a",			0x04000, 0xf8f2acdf, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136040-143.15e",			0x04000, 0x62b9878e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136040-144.17e",			0x04000, 0x6ef1b804, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136040-101.4b",			0x08000, 0x26d9f29c, 3 | BRF_GRA },           // 14 Sprites & Background Tiles
	{ "136040-107.9b",			0x08000, 0x8aac0ba4, 3 | BRF_GRA },           // 15
	{ "136040-113.4f",			0x08000, 0x48b74c52, 3 | BRF_GRA },           // 16
	{ "136040-119.9f",			0x08000, 0x17a6510c, 3 | BRF_GRA },           // 17
	{ "136040-102.3b",			0x08000, 0xae88f54b, 3 | BRF_GRA },           // 18
	{ "136040-108.8b",			0x08000, 0xa2ac13d4, 3 | BRF_GRA },           // 19
	{ "136040-114.3f",			0x08000, 0xc91c3fcb, 3 | BRF_GRA },           // 20
	{ "136040-120.8f",			0x08000, 0x42d25859, 3 | BRF_GRA },           // 21
	{ "136040-103.2b",			0x08000, 0xf2d7ef55, 3 | BRF_GRA },           // 22
	{ "136040-109.7b",			0x08000, 0x11a843dc, 3 | BRF_GRA },           // 23
	{ "136040-115.2f",			0x08000, 0x8b1fa5bc, 3 | BRF_GRA },           // 24
	{ "136040-121.7f",			0x08000, 0xecf278f2, 3 | BRF_GRA },           // 25
	{ "136040-104.1b",			0x08000, 0x0203d89c, 3 | BRF_GRA },           // 26
	{ "136040-110.6b",			0x08000, 0x64801601, 3 | BRF_GRA },           // 27
	{ "136040-116.1f",			0x08000, 0x52b23a36, 3 | BRF_GRA },           // 28
	{ "136040-122.6f",			0x08000, 0xb1137a9d, 3 | BRF_GRA },           // 29
	{ "136040-105.4d",			0x08000, 0x398a36f8, 3 | BRF_GRA },           // 30
	{ "136040-111.9d",			0x08000, 0xf08b418b, 3 | BRF_GRA },           // 31
	{ "136040-117.2d",			0x08000, 0xc4394834, 3 | BRF_GRA },           // 32
	{ "136040-123.7d",			0x08000, 0xdafd3dbe, 3 | BRF_GRA },           // 33
	{ "136040-106.3d",			0x08000, 0x36a77bc5, 3 | BRF_GRA },           // 34
	{ "136040-112.8d",			0x08000, 0xb6624f3c, 3 | BRF_GRA },           // 35
	{ "136040-118.1d",			0x08000, 0xf489a968, 3 | BRF_GRA },           // 36
	{ "136040-124.6d",			0x08000, 0x524d65f7, 3 | BRF_GRA },           // 37

	{ "136040-126.7a",			0x00200, 0x1713c0cd, 4 | BRF_GRA },           // 38 Sprite & Background Mapping PROMs
	{ "136040-125.5a",			0x00200, 0xa9ca8795, 4 | BRF_GRA },           // 39
};

STDROMPICKEXT(roadrunn1, roadrunn1, atarisy1)
STD_ROM_FN(roadrunn1)

struct BurnDriver BurnDrvRoadrunn1 = {
	"roadrunn1", "roadrunn", "atarisy1", NULL, "1985",
	"Road Runner (rev 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, roadrunn1RomInfo, roadrunn1RomName, NULL, NULL, NULL, NULL, RoadrunnInputInfo, RoadrunnDIPInfo,
	RoadrunnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, rev 4)

static struct BurnRomInfo roadblstRomDesc[] = {
	{ "136048-1157.11c",		0x08000, 0x6d9ad91e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1158.11a",		0x08000, 0x7d4cf151, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1159.13c",		0x08000, 0x921c0e34, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1160.13a",		0x08000, 0x8bf22f7d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 34
};

STDROMPICKEXT(roadblst, roadblst, atarisy1)
STD_ROM_FN(roadblst)


static INT32 DrvLoadSplit(INT32 rom, UINT8 *dst, UINT32 offset1, UINT32 size1, UINT32 offset2, UINT32 size2, INT32 flags)
{
	INT32 ret = 1;
	UINT8 *tmp = (UINT8*)BurnMalloc(size1+size2);

	if (BurnLoadRomExt(tmp, rom, 1, flags) == 0)
	{
		memcpy (dst + offset1, tmp, size1);
		memcpy (dst + offset2, tmp + size1, size2);
		ret = 0;
	}

	BurnFree(tmp);

	return ret;
}

static INT32 RoadblstLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	//memset(DrvTileROM, 0xff, 0x0400000);

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x50000, k++, 1, LD_INVERT)) return 1;

	if (DrvLoadSplit(k++, DrvTileROM, 0x080000, 0x8000, 0x100000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x090000, 0x8000, 0x110000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0a0000, 0x8000, 0x120000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0b0000, 0x8000, 0x130000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x180000, 0x8000, 0x200000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x190000, 0x8000, 0x210000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1a0000, 0x8000, 0x220000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1b0000, 0x8000, 0x230000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x300000, 0x8000, 0x280000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x310000, 0x8000, 0x290000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x320000, 0x8000, 0x2a0000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x330000, 0x8000, 0x2b0000, 0x8000, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 RoadblstInit()
{
	is_roadb = 1;

	return CommonInit(RoadblstLoadRoms, 110, analog_pedal);
}

struct BurnDriver BurnDrvRoadblst = {
	"roadblst", NULL, "atarisy1", NULL, "1987",
	"Road Blasters (upright, rev 4)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstRomInfo, roadblstRomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	RoadblstInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, German, rev 3)

static struct BurnRomInfo roadblstgRomDesc[] = {
	{ "136048-2257.11c",		0x08000, 0x6e9de790, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-2258.11a",		0x08000, 0x5160c69e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-2259.13c",		0x08000, 0x62f10976, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-2260.13a",		0x08000, 0x528035ba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 34
};

STDROMPICKEXT(roadblstg, roadblstg, atarisy1)
STD_ROM_FN(roadblstg)

static INT32 RoadblstgInit()
{
	is_roadb = 1;

	return CommonInit(RoadblstLoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstg = {
	"roadblstg", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, German, rev 3)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstgRomInfo, roadblstgRomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	RoadblstgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, German, rev ?)

static struct BurnRomInfo roadblstguRomDesc[] = {
	{ "136048-1257.c11",		0x08000, 0x604a5cc0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1258.a11",		0x08000, 0x3d10929d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1259.c13",		0x08000, 0xb9c807ac, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1260.a13",		0x08000, 0xeaeb1196, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-1163.c12",		0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-1164.a12",		0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-1165.c14",		0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-1166.a14",		0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-1167.c16",		0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-1168.a16",		0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.c17",		0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.a17",		0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.e14",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.e15",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.e17",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.b4",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.b9",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.f4",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.f9",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.h4",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.h9",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1119.b3",			0x08000, 0x2533be39, 3 | BRF_GRA },           // 21
	{ "136048-1120.b8",			0x08000, 0x3de4f4db, 3 | BRF_GRA },           // 22
	{ "136048-1121.f3",			0x08000, 0x4a1b6b9f, 3 | BRF_GRA },           // 23
	{ "136048-1122.f8",			0x08000, 0x680bdb7d, 3 | BRF_GRA },           // 24
	{ "136048-1123.b2",			0x08000, 0xa405d8bf, 3 | BRF_GRA },           // 25
	{ "136048-1124.b7",			0x08000, 0xb9070c2e, 3 | BRF_GRA },           // 26
	{ "136048-1125.f2",			0x08000, 0x5dfac572, 3 | BRF_GRA },           // 27
	{ "136048-1126.f7",			0x08000, 0xa0416c6d, 3 | BRF_GRA },           // 28
	{ "136048-1127.b1",			0x08000, 0x0138b391, 3 | BRF_GRA },           // 29
	{ "136048-1128.b6",			0x08000, 0x5136fb4b, 3 | BRF_GRA },           // 30
	{ "136048-1129.f1",			0x08000, 0x7d75bb12, 3 | BRF_GRA },           // 31
	{ "136048-1130.f6",			0x08000, 0x81bb54d9, 3 | BRF_GRA },           // 32
	{ "136048-1131.d4",			0x08000, 0x72233889, 3 | BRF_GRA },           // 33
	{ "136048-1132.d9",			0x08000, 0x6a82b8a7, 3 | BRF_GRA },           // 34
	{ "136048-1133.d2",			0x08000, 0x845dd347, 3 | BRF_GRA },           // 35
	{ "136048-1134.d7",			0x08000, 0x54e4c9e6, 3 | BRF_GRA },           // 36
	{ "136048-1115.d3",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 37
	{ "136048-1116.d8",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 38
	{ "136048-1117.d1",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 39
	{ "136048-1118.d6",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 40

	{ "136048-1174.a7",			0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 41 Sprite & Background Mapping PROMs
	{ "136048-1173.a5",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 42
};

STDROMPICKEXT(roadblstgu, roadblstgu, atarisy1)
STD_ROM_FN(roadblstgu)

static INT32 RoadblstguLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x50000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x100000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x110000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x120000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x130000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x180000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x190000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1b0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x200000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x210000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x220000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x230000, k++, 1, LD_INVERT)) return 1;

	if (DrvLoadSplit(k++, DrvTileROM, 0x300000, 0x8000, 0x280000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x310000, 0x8000, 0x290000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x320000, 0x8000, 0x2a0000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x330000, 0x8000, 0x2b0000, 0x8000, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 RoadblstguInit()
{
	is_roadb = 1;

	return CommonInit(RoadblstguLoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstgu = {
	"roadblstgu", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, German, rev ?)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstguRomInfo, roadblstguRomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstLSIDIPInfo,
	RoadblstguInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, rev 3)

static struct BurnRomInfo roadblst3RomDesc[] = {
	{ "136048-3157.11c",		0x08000, 0xce88fe34, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-3158.11a",		0x08000, 0x03bf2879, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-3159.13c",		0x08000, 0x4305d74a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-3160.13a",		0x08000, 0x23304687, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 34
};

STDROMPICKEXT(roadblst3, roadblst3, atarisy1)
STD_ROM_FN(roadblst3)

static INT32 Roadblst3Init()
{
	is_roadb = 1;

	return CommonInit(RoadblstLoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblst3 = {
	"roadblst3", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, rev 3)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblst3RomInfo, roadblst3RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	Roadblst3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, German, rev 2)

static struct BurnRomInfo roadblstg2RomDesc[] = {
	{ "136048-1239.11c",		0x10000, 0x3b2bb14b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1240.11a",		0x10000, 0x2a5ab597, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1255.13c",		0x10000, 0x1dcce3e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1256.13a",		0x10000, 0x193eaf68, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 12
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 13
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 14
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 15
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 16
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 17
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 18
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 19
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 20
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 21
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 22
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 23
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 24
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 25
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 26
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 27
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 28

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 29 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 30
};

STDROMPICKEXT(roadblstg2, roadblstg2, atarisy1)
STD_ROM_FN(roadblstg2)

static INT32 Roadblstg2LoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	memcpy (Drv68KROM + 0x50000, Drv68KROM + 0x20000, 0x10000);
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	memcpy (Drv68KROM + 0x60000, Drv68KROM + 0x30000, 0x10000);
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x50000, k++, 1, LD_INVERT)) return 1;

	if (DrvLoadSplit(k++, DrvTileROM, 0x080000, 0x8000, 0x100000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x090000, 0x8000, 0x110000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0a0000, 0x8000, 0x120000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0b0000, 0x8000, 0x130000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x180000, 0x8000, 0x200000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x190000, 0x8000, 0x210000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1a0000, 0x8000, 0x220000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1b0000, 0x8000, 0x230000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x300000, 0x8000, 0x280000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x310000, 0x8000, 0x290000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x320000, 0x8000, 0x2a0000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x330000, 0x8000, 0x2b0000, 0x8000, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 Roadblstg2Init()
{
	is_roadb = 1;

	return CommonInit(Roadblstg2LoadRoms, 110, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstg2 = {
	"roadblstg2", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, German, rev 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstg2RomInfo, roadblstg2RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	Roadblstg2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, rev 2)

static struct BurnRomInfo roadblst2RomDesc[] = {
	{ "136048-1139.11c",		0x10000, 0xb73c1bd5, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1140.11a",		0x10000, 0x6305429b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1155.13c",		0x10000, 0xe95fc7d2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1156.13a",		0x10000, 0x727510f9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 12
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 13
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 14
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 15
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 16
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 17
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 18
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 19
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 20
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 21
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 22
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 23
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 24
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 25
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 26
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 27
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 28

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 29 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 30
};

STDROMPICKEXT(roadblst2, roadblst2, atarisy1)
STD_ROM_FN(roadblst2)

static INT32 Roadblst2LoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	memcpy (Drv68KROM + 0x50000, Drv68KROM + 0x20000, 0x10000);
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	memcpy (Drv68KROM + 0x60000, Drv68KROM + 0x30000, 0x10000);
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x50000, k++, 1, LD_INVERT)) return 1;

	if (DrvLoadSplit(k++, DrvTileROM, 0x080000, 0x8000, 0x100000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x090000, 0x8000, 0x110000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0a0000, 0x8000, 0x120000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0b0000, 0x8000, 0x130000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x180000, 0x8000, 0x200000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x190000, 0x8000, 0x210000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1a0000, 0x8000, 0x220000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1b0000, 0x8000, 0x230000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x300000, 0x8000, 0x280000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x310000, 0x8000, 0x290000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x320000, 0x8000, 0x2a0000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x330000, 0x8000, 0x2b0000, 0x8000, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 Roadblst2Init()
{
	is_roadb = 1;

	return CommonInit(Roadblst2LoadRoms, 110, analog_pedal);
}

struct BurnDriver BurnDrvRoadblst2 = {
	"roadblst2", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, rev 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblst2RomInfo, roadblst2RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	Roadblst2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, German, rev 1)

static struct BurnRomInfo roadblstg1RomDesc[] = {
	{ "136048-1251.11c",		0x10000, 0x7e94d6a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1252.11a",		0x10000, 0xd7a66215, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1253.13c",		0x10000, 0x342bf326, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1254.13a",		0x10000, 0xdb8d7495, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 12
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 13
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 14
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 15
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 16
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 17
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 18
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 19
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 20
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 21
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 22
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 23
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 24
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 25
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 26
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 27
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 28
	
	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 29 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 30
};

STDROMPICKEXT(roadblstg1, roadblstg1, atarisy1)
STD_ROM_FN(roadblstg1)

static INT32 Roadblstg1Init()
{
	is_roadb = 1;

	return CommonInit(Roadblstg2LoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstg1 = {
	"roadblstg1", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, German, rev 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstg1RomInfo, roadblstg1RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	Roadblstg1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (upright, rev 1)

static struct BurnRomInfo roadblst1RomDesc[] = {
	{ "136048-2151.11c",		0x10000, 0xea6b3060, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-2152.11a",		0x10000, 0xf5c1fbe0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-2153.13c",		0x10000, 0x11c41698, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-2154.13a",		0x10000, 0x7b947d64, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 11 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 12
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 13
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 14
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 15
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 16
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 17
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 18
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 19
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 20
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 21
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 22
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 23
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 24
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 25
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 26
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 27
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 28

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 29 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 30
};

STDROMPICKEXT(roadblst1, roadblst1, atarisy1)
STD_ROM_FN(roadblst1)

static INT32 Roadblst1Init()
{
	is_roadb = 1;

	return CommonInit(Roadblst2LoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblst1 = {
	"roadblst1", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (upright, rev 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblst1RomInfo, roadblst1RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	Roadblst1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (cockpit, rev 2)

static struct BurnRomInfo roadblstcRomDesc[] = {
	{ "136048-1179.7p",			0x08000, 0xef448f96, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1180.8p",			0x08000, 0xbdb368d5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1181.7r",			0x08000, 0xd52581da, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1182.8r",			0x08000, 0x847788c4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 34
};

STDROMPICKEXT(roadblstc, roadblstc, atarisy1)
STD_ROM_FN(roadblstc)

static INT32 RoadblstcInit()
{
	is_roadb = 1;

	return CommonInit(RoadblstLoadRoms, 110, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstc = {
	"roadblstc", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (cockpit, rev 2)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstcRomInfo, roadblstcRomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstDIPInfo,
	RoadblstcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (cockpit, German, rev 1)

static struct BurnRomInfo roadblstcgRomDesc[] = {
	{ "136048-1235.7p",			0x08000, 0x58b2998f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-1236.8p",			0x08000, 0x02e23a40, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-1237.7r",			0x08000, 0x5e0a7c5d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-1238.8r",			0x08000, 0x8c8f9523, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.c8",			0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1150.d7",			0x08000, 0xe89e7fc8, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136032-120.p1",			0x04000, 0x90a1950d, 3 | BRF_GRA },           // 14 Alphanumeric Tiles

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 4 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 4 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 4 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 4 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 4 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 4 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 4 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 4 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 4 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 4 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 4 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 4 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 4 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 4 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 4 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 4 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 4 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 4 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 5 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 5 | BRF_GRA },           // 34
	{ "135048-1172.d1",			0x00200, 0xb79d1903, 5 | BRF_OPT },           // 35
	{ "135048-1171.d2",			0x00200, 0x29248a95, 5 | BRF_OPT },           // 36
};

STDROMPICKEXT(roadblstcg, roadblstcg, atarisy1)
STD_ROM_FN(roadblstcg)

static INT32 RoadblstcgLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x50000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x60000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x70000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x80000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;

	if (BurnLoadRom(DrvAlphaROM, k++, 1)) return 1; // load alpha font rom

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x40000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x50000, k++, 1, LD_INVERT)) return 1;

	if (DrvLoadSplit(k++, DrvTileROM, 0x080000, 0x8000, 0x100000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x090000, 0x8000, 0x110000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0a0000, 0x8000, 0x120000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x0b0000, 0x8000, 0x130000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x180000, 0x8000, 0x200000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x190000, 0x8000, 0x210000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1a0000, 0x8000, 0x220000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x1b0000, 0x8000, 0x230000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x300000, 0x8000, 0x280000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x310000, 0x8000, 0x290000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x320000, 0x8000, 0x2a0000, 0x8000, LD_INVERT)) return 1;
	if (DrvLoadSplit(k++, DrvTileROM, 0x330000, 0x8000, 0x2b0000, 0x8000, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 RoadblstcgInit()
{
	is_roadb = 1;

	return CommonInit(RoadblstcgLoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstcg = {
	"roadblstcg", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (cockpit, German, rev 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstcgRomInfo, roadblstcgRomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstLSIDIPInfo,
	RoadblstcgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Road Blasters (cockpit, rev 1)

static struct BurnRomInfo roadblstc1RomDesc[] = {
	{ "136048-2135.7p",			0x08000, 0xc0ef86df, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136048-2136.8p",			0x08000, 0x9637e2f0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136048-2137.7r",			0x08000, 0x5382ab85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136048-2138.8r",			0x08000, 0xc2c75309, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136048-2141.7l",			0x08000, 0x054273b2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136048-2142.8l",			0x08000, 0x49181bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136048-2143.7m",			0x08000, 0xf63dc29a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136048-2144.8m",			0x08000, 0xb1fc5955, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136048-2145.7n",			0x08000, 0xc6d30d6f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136048-2146.8n",			0x08000, 0x16951020, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136048-2147.7k",			0x04000, 0x5c1adf67, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136048-2148.8k",			0x04000, 0xd9ac8966, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136048-1149.14e",		0x04000, 0x2e54f95e, 2 | BRF_PRG | BRF_ESS }, // 12 M6502 Code
	{ "136048-1169.1516e",		0x04000, 0xee318052, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "136048-1170.17e",		0x04000, 0x75dfec33, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "136048-1101.2s",			0x08000, 0xfe342d27, 3 | BRF_GRA },           // 15 Sprites & Background Tiles
	{ "136048-1102.2r",			0x08000, 0x17c7e780, 3 | BRF_GRA },           // 16
	{ "136048-1103.2n",			0x08000, 0x39688e01, 3 | BRF_GRA },           // 17
	{ "136048-1104.2m",			0x08000, 0xc8f9bd8e, 3 | BRF_GRA },           // 18
	{ "136048-1105.2k",			0x08000, 0xc69e439e, 3 | BRF_GRA },           // 19
	{ "136048-1106.2j",			0x08000, 0x4ee55796, 3 | BRF_GRA },           // 20
	{ "136048-1107.3s",			0x10000, 0x02117c58, 3 | BRF_GRA },           // 21
	{ "136048-1108.2p",			0x10000, 0x1e148525, 3 | BRF_GRA },           // 22
	{ "136048-1109.3n",			0x10000, 0x110ce07e, 3 | BRF_GRA },           // 23
	{ "136048-1110.2l",			0x10000, 0xc00aa0f4, 3 | BRF_GRA },           // 24
	{ "136048-1111.4s",			0x10000, 0xc951d014, 3 | BRF_GRA },           // 25
	{ "136048-1112.3r",			0x10000, 0x95c5a006, 3 | BRF_GRA },           // 26
	{ "136048-1113.4n",			0x10000, 0xf61f2370, 3 | BRF_GRA },           // 27
	{ "136048-1114.3m",			0x10000, 0x774a36a8, 3 | BRF_GRA },           // 28
	{ "136048-1115.4r",			0x10000, 0xa47bc79d, 3 | BRF_GRA },           // 29
	{ "136048-1116.3p",			0x10000, 0xb8a5c215, 3 | BRF_GRA },           // 30
	{ "136048-1117.4m",			0x10000, 0x2d1c1f64, 3 | BRF_GRA },           // 31
	{ "136048-1118.3l",			0x10000, 0xbe879b8e, 3 | BRF_GRA },           // 32

	{ "136048-1174.12d",		0x00200, 0xdb4a4d53, 4 | BRF_GRA },           // 33 Sprite & Background Mapping PROMs
	{ "136048-1173.2d",			0x00200, 0xc80574af, 4 | BRF_GRA },           // 34
};

STDROMPICKEXT(roadblstc1, roadblstc1, atarisy1)
STD_ROM_FN(roadblstc1)

static INT32 Roadblstc1Init()
{
	is_roadb = 1;

	return CommonInit(RoadblstLoadRoms, 109, analog_pedal);
}

struct BurnDriver BurnDrvRoadblstc1 = {
	"roadblstc1", "roadblst", "atarisy1", NULL, "1987",
	"Road Blasters (cockpit, rev 1)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, roadblstc1RomInfo, roadblstc1RomName, NULL, NULL, NULL, NULL, RoadblstInputInfo, RoadblstLSIDIPInfo,
	Roadblstc1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Relief Pitcher (System 1, prototype)

static struct BurnRomInfo reliefs1RomDesc[] = {
	{ "rp-pgm-10k-hi-c45.bin",	0x08000, 0x548c0276, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rp-pgm-10k-lo-c44.bin",	0x08000, 0xca37684a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rp-pgm-20k-hi-c58.bin",	0x08000, 0xdd5c53c7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rp-pgm-20k-lo-c57.bin",	0x08000, 0xf064da9f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rp-pgm-30k-hi-c72.bin",	0x08000, 0xda0d7a9c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rp-pgm-30k-lo-c71.bin",	0x08000, 0x247faea5, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "rp-snd-4k-c68.bin",		0x04000, 0xf6ff3d90, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code
	{ "rp-snd-8k-c73.bin",		0x04000, 0x932b3ca0, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "rp-snd-ck-c78.bin",		0x04000, 0x7c5819db, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "rp-p0-1.c14.bin",		0x08000, 0xfe82b82d, 3 | BRF_GRA },           //  9 Sprites & Background Tiles
	{ "rp-p1-1.c36.bin",		0x08000, 0x8ebbbd7b, 3 | BRF_GRA },           // 10
	{ "rp-p2-1.c16.bin",		0x08000, 0xbdffa7ea, 3 | BRF_GRA },           // 11
	{ "rp-p3-1.c38.bin",		0x08000, 0xfb8866bc, 3 | BRF_GRA },           // 12
	{ "rp-p0-2.c10.bin",		0x08000, 0xd2b0d119, 3 | BRF_GRA },           // 13
	{ "rp-p1-2.c32.bin",		0x08000, 0x0aa69c1e, 3 | BRF_GRA },           // 14
	{ "rp-p2-2.c12.bin",		0x08000, 0x27f6000a, 3 | BRF_GRA },           // 15
	{ "rp-p3-2.c34.bin",		0x08000, 0x621f28f5, 3 | BRF_GRA },           // 16
	{ "rp-p0-3.c5.bin",			0x08000, 0x6f05880e, 3 | BRF_GRA },           // 17
	{ "rp-p1-3.c27.bin",		0x08000, 0x9b5a8f6b, 3 | BRF_GRA },           // 18
	{ "rp-p2-3.c7.bin",			0x08000, 0xacc1586d, 3 | BRF_GRA },           // 19
	{ "rp-p3-3.c29.bin",		0x08000, 0xffc1b0f7, 3 | BRF_GRA },           // 20
	{ "rp-p0-4.c1.bin",			0x08000, 0xa933d798, 3 | BRF_GRA },           // 21
	{ "rp-p1-4.c23.bin",		0x08000, 0x6946f9e5, 3 | BRF_GRA },           // 22
	{ "rp-p2-4.c3.bin",			0x08000, 0x34141ef7, 3 | BRF_GRA },           // 23
	{ "rp-p3-4.c25.bin",		0x08000, 0x3f94cd0a, 3 | BRF_GRA },           // 24
	{ "rp-p0-5.c15.bin",		0x08000, 0x5e7e1f00, 3 | BRF_GRA },           // 25
	{ "rp-p1-5.c37.bin",		0x08000, 0x52ed4c87, 3 | BRF_GRA },           // 26
	{ "rp-p2-5.c6.bin",			0x08000, 0x09c7608c, 3 | BRF_GRA },           // 27
	{ "rp-p3-5.c28.bin",		0x08000, 0xb87580ae, 3 | BRF_GRA },           // 28
	{ "rp-p0-6.c11.bin",		0x08000, 0xcb5b2352, 3 | BRF_GRA },           // 29
	{ "rp-p1-6.c33.bin",		0x08000, 0xbd16718f, 3 | BRF_GRA },           // 30
	{ "rp-p2-6.c2.bin",			0x08000, 0x7a4f1469, 3 | BRF_GRA },           // 31
	{ "rp-p3-6.c24.bin",		0x08000, 0xf15426d3, 3 | BRF_GRA },           // 32

	{ "brlf24k_bs.a7",			0x0200, 0x62b8ff72, 4 | BRF_GRA },            // 33 Sprite & Background Mapping PROMs
	{ "brlf24k_ps.a5",			0x0200, 0xeaee166e, 4 | BRF_GRA },            // 34
};

STDROMPICKEXT(reliefs1, reliefs1, atarisy1)
STD_ROM_FN(reliefs1)

static INT32 Reliefs1LoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM    + 0x10001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x10000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x20000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x30000, k++, 2)) return 1;

	if (BurnLoadRom(DrvM6502ROM  + 0x04000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x08000, k++, 1)) return 1;
	if (BurnLoadRom(DrvM6502ROM  + 0x0c000, k++, 1)) return 1;

	memset(DrvTileROM, 0xff, 0x400000);

	if (BurnLoadRomExt(DrvTileROM + 0x00000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x10000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x20000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x30000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x80000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x90000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xa0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0xb0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x100000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x110000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x120000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x130000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x180000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x190000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x1b0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x200000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x210000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x220000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x230000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRomExt(DrvTileROM + 0x280000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x290000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x2a0000, k++, 1, LD_INVERT)) return 1;
	if (BurnLoadRomExt(DrvTileROM + 0x2b0000, k++, 1, LD_INVERT)) return 1;

	if (BurnLoadRom(DrvMapPROM   + 0x00000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMapPROM   + 0x00200, k++, 1)) return 1;

	return 0;
}

static INT32 Reliefs1Init()
{
	return CommonInit(Reliefs1LoadRoms, 0, analog_joystick);
}

struct BurnDriver BurnDrvReliefs1 = {
	"reliefs1", NULL, "atarisy1", NULL, "1986",
	"Relief Pitcher (System 1, prototype)\0", NULL, "Atari Games", "System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, reliefs1RomInfo, reliefs1RomName, NULL, NULL, NULL, NULL, Reliefs1InputInfo, Reliefs1DIPInfo,
	Reliefs1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};
